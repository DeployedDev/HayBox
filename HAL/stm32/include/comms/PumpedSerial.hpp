#ifndef _COMMS_PUMPEDSERIAL_HPP
#define _COMMS_PUMPEDSERIAL_HPP

#include <Arduino.h>

/*
 * Serial (TinyUSB CDC) wrapper that keeps servicing the USB stack while a
 * reader is blocked waiting for data.
 *
 * On the pico, TinyUSB runs from a background task, so a blocking read on
 * Serial - e.g. Stream::readBytes(), which pb_arduino uses to decode an
 * incoming config - keeps receiving: the task moves each completed USB OUT
 * transfer into the CDC FIFO while the reader spins. STM32duino has no such
 * task. On this port received bytes only reach the FIFO when tud_task() runs,
 * so a blocking read would drain the ~256-byte FIFO and then starve until the
 * read timed out, which made the Configurator's SET_CONFIG (the one command
 * that receives a large payload) fail to decode.
 *
 * This wrapper calls yield() - our port's tud_task() plus CDC TX flush -
 * whenever the reader polls and finds the base stream empty, so the FIFO keeps
 * filling for as long as the host is sending. The Configurator is handed this
 * instead of Serial directly; the backend itself stays identical to the pico's.
 */
class PumpedSerial : public Stream {
  public:
    explicit PumpedSerial(Stream &base) : _base(base) {}

    int available() override {
        if (!_base.available()) {
            yield();
        }
        return _base.available();
    }

    int read() override {
        if (!_base.available()) {
            yield();
        }
        return _base.read();
    }

    int peek() override {
        if (!_base.available()) {
            yield();
        }
        return _base.peek();
    }

    size_t write(uint8_t b) override {
        return _base.write(b);
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        return _base.write(buffer, size);
    }

    int availableForWrite() override {
        return _base.availableForWrite();
    }

    void flush() override {
        _base.flush();
    }

  private:
    Stream &_base;
};

#endif
