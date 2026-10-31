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

#ifndef _CORE_PERSISTENCE_HPP
#define _CORE_PERSISTENCE_HPP

#include <Print.h>
#include <config.pb.h>

/*
 * Config persistence for the STM32 (Frame1) HAL.
 *
 * Same interface and on-media layout as HAL/pico's Persistence - an 8-byte
 * header {config_size, config_crc} followed by the nanopb-encoded Config -
 * but backed by a reserved region of internal flash instead of a LittleFS
 * file, since STM32duino has no filesystem on internal flash. See the .cpp
 * for the flash layout.
 */
class Persistence {
  public:
    typedef struct _ConfigHeader {
        size_t config_size = 0;
        uint32_t config_crc = 0;
    } ConfigHeader;

    Persistence();
    ~Persistence();

    bool SaveConfig(Config &config);
    bool LoadConfig(Config &config);
    bool CheckSavedConfig();
    size_t LoadConfigRaw(Print &out, bool validate = true);

    static constexpr size_t config_offset = sizeof(ConfigHeader);
};

extern Persistence persistence;

#endif
