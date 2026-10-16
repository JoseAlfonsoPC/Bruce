#pragma once
#include "nrf_esb.h"

// Keep diagnostic metadata only; no addresses or HID report bodies are stored.
struct NrfEsbScanStats {
    uint32_t received = 0, valid = 0, empty = 0, data = 0, flat = 0;
    uint8_t lastLength = 0, lastChannel = 0, header0 = 0, header1 = 0, sum8 = 0;

    bool consume(const uint8_t *raw, size_t size, uint8_t channel, NrfEsbFrame &frame) {
        if (!raw || size < 9 || size > 32) return false;
        ++received;
        bool identical = true;
        for (size_t i = 1; i < size; ++i) {
            if (raw[i] != raw[0]) {
                identical = false;
                break;
            }
        }
        if (identical) ++flat;
        if (!nrfDecodeEsb(raw, size, frame)) return false;
        ++valid;
        if (frame.size == 0) {
            ++empty;
        } else {
            ++data;
            lastLength = frame.size;
            lastChannel = channel;
            header0 = frame.payload[0];
            header1 = frame.size > 1 ? frame.payload[1] : 0;
            sum8 = 0;
            for (uint8_t i = 0; i < frame.size; ++i) sum8 += frame.payload[i];
        }
        return true;
    }
};
