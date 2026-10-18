/*
 * This file is part of HayBox
 * Copyright (C) 2024 Jonathan Haylett
 *
 * HayBox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/Persistence.hpp"

#include "stdlib.hpp"

#include <Arduino.h> // STM32duino: pulls in the STM32 HAL (flash)
#include <CRC32.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string.h>

/*
 * Flash-backed config persistence for the Frame1 (STM32F303VE).
 *
 * The pico keeps config.bin on a LittleFS partition. STM32duino has no
 * filesystem on internal flash, so this stores the identical layout - an
 * 8-byte header {config_size, config_crc} followed by the nanopb-encoded
 * Config - directly in a region reserved at the top of flash:
 *
 *     0x0807C000 .. 0x0807FFFF   16 KB = 8 x 2 KB pages
 *
 * 16 KB covers the worst-case encoded Config given the max_count bounds in
 * config.options (roughly 14 KB); a real config is far smaller. The region:
 *   - is excluded from the linker by boards/frame1.json, which shrinks
 *     upload.maximum_size by the same 16 KB, so code can never land here;
 *   - sits above anything dfu-util writes when flashing firmware.bin at
 *     0x08020000, so a saved config survives firmware updates;
 *   - is far above the write-protected DFU bootloader (0x08000000-0x0801FFFF).
 *
 * Saving streams the encoded protobuf straight into flash one 32-bit word at a
 * time (no RAM copy), computing the CRC on the way, and programs the header
 * LAST. A power loss mid-save therefore leaves the header erased (all 0xFF),
 * which fails validation and falls back to defaults on the next boot. Loading
 * reads the memory-mapped flash in place.
 *
 * Flash program/erase stalls the core on this single-bank part (it executes
 * from the same flash), so a save blocks for a few hundred ms. That matches
 * the pico's behaviour and only happens on a Configurator save or the very
 * first boot.
 */

namespace {

constexpr uint32_t kRegionAddr = 0x0807C000UL;
constexpr uint32_t kRegionSize = 16UL * 1024UL;
constexpr uint32_t kDataAddr = kRegionAddr + Persistence::config_offset;
constexpr uint32_t kMaxDataSize = kRegionSize - Persistence::config_offset;

static_assert(kRegionSize % FLASH_PAGE_SIZE == 0, "config region must be whole flash pages");
static_assert(Persistence::config_offset % 4 == 0, "config header must be word aligned");

const Persistence::ConfigHeader *stored_header() {
    return reinterpret_cast<const Persistence::ConfigHeader *>(kRegionAddr);
}

const uint8_t *stored_data() {
    return reinterpret_cast<const uint8_t *>(kDataAddr);
}

/* Validate the stored header and CRC. Erased flash reads 0xFFFFFFFF, which
 * fails the size bound. */
bool stored_config_valid(size_t *size_out) {
    const Persistence::ConfigHeader *header = stored_header();
    if (header->config_size == 0 || header->config_size > kMaxDataSize) {
        return false;
    }
    if (CRC32::calculate(stored_data(), header->config_size) != header->config_crc) {
        return false;
    }
    if (size_out != nullptr) {
        *size_out = header->config_size;
    }
    return true;
}

/* Streams bytes into flash a word at a time, CRC-ing as it goes. Unwritten
 * lanes of a partial final word are left at 0xFF (the erased state). */
struct FlashWriter {
    uint32_t addr;
    uint32_t word = 0xFFFFFFFFUL;
    uint8_t lane = 0;
    size_t written = 0;
    bool ok = true;
    CRC32 crc;

    explicit FlashWriter(uint32_t start) : addr(start) {}

    void put(uint8_t byte) {
        crc.update(byte);
        word &= ~(0xFFUL << (8 * lane));
        word |= (uint32_t)byte << (8 * lane);
        written++;
        if (++lane == 4) {
            flush();
        }
    }

    void flush() {
        if (lane == 0) {
            return;
        }
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, word) != HAL_OK) {
            ok = false;
        }
        addr += 4;
        word = 0xFFFFFFFFUL;
        lane = 0;
    }
};

bool pb_flash_write(pb_ostream_t *stream, const pb_byte_t *buf, size_t count) {
    FlashWriter *writer = static_cast<FlashWriter *>(stream->state);
    if (writer->written + count > kMaxDataSize) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        writer->put(buf[i]);
    }
    return writer->ok;
}

bool erase_region() {
    FLASH_EraseInitTypeDef erase = {};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = kRegionAddr;
    erase.NbPages = kRegionSize / FLASH_PAGE_SIZE;
    uint32_t page_error = 0;
    return HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK;
}

} // namespace

Persistence::Persistence() {}

Persistence::~Persistence() {}

bool Persistence::SaveConfig(Config &config) {
    // Make sure config encodes correctly and fits the region.
    size_t encoded_size;
    if (!pb_get_encoded_size(&encoded_size, Config_fields, &config)) {
        return false;
    }
    if (encoded_size == 0 || encoded_size > kMaxDataSize) {
        return false;
    }

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);

    bool ok = erase_region();

    // Encode Protobuf data directly into the flash body (after the header).
    FlashWriter writer(kDataAddr);
    if (ok) {
        pb_ostream_t ostream = {};
        ostream.callback = &pb_flash_write;
        ostream.state = &writer;
        ostream.max_size = kMaxDataSize;
        ok = pb_encode(&ostream, Config_fields, &config);
        writer.flush();
        ok = ok && writer.ok && ostream.bytes_written == encoded_size;
    }

    // Header last, so an interrupted save is detected as invalid.
    if (ok) {
        ConfigHeader header;
        header.config_size = encoded_size;
        header.config_crc = writer.crc.finalize();
        uint32_t words[2];
        memcpy(words, &header, sizeof(words));
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, kRegionAddr, words[0]) == HAL_OK &&
             HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, kRegionAddr + 4, words[1]) == HAL_OK;
    }

    HAL_FLASH_Lock();

    return ok && CheckSavedConfig();
}

bool Persistence::LoadConfig(Config &config) {
    size_t size;
    if (!stored_config_valid(&size)) {
        return false;
    }

    // Reset config defaults first, so config is completely replaced rather than merged with
    // defaults.
    config = Config_init_default;

    // Decode Protobuf data straight out of memory-mapped flash.
    pb_istream_t istream = pb_istream_from_buffer(stored_data(), size);
    return pb_decode(&istream, Config_fields, &config);
}

bool Persistence::CheckSavedConfig() {
    return stored_config_valid(nullptr);
}

size_t Persistence::LoadConfigRaw(Print &out, bool validate) {
    size_t size = stored_header()->config_size;
    if (validate) {
        if (!stored_config_valid(&size)) {
            return 0;
        }
    } else if (size == 0 || size > kMaxDataSize) {
        return 0;
    }

    // Write raw Protobuf encoded data to output stream.
    return out.write(stored_data(), size);
}

Persistence persistence;
