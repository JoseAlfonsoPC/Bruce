#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Passive ESB decoding, extracted from nrf_mousejack.cpp. The 32-byte
// promiscuous capture includes the address, 9-bit PCF and 16-bit CRC.
// Reference: BastilleResearch/nrf-research-firmware, src/radio.c.
struct NrfEsbFrame {
    uint8_t address[5];
    uint8_t payload[23];
    uint8_t size;
};

inline uint16_t nrfEsbCrcUpdate(uint16_t crc, uint8_t byte, uint8_t bits) {
    crc ^= static_cast<uint16_t>(byte) << 8;
    while (bits--) {
        crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

inline bool nrfDecodeEsb(const uint8_t *raw, size_t size, NrfEsbFrame &frame) {
    if (!raw || size < 9 || size > 32) return false;
    uint8_t buf[32];
    for (uint8_t offset = 0; offset < 2; ++offset) {
        memcpy(buf, raw, size);
        if (offset) {
            for (size_t x = size - 1; x > 0; --x) { buf[x] = (buf[x - 1] << 7) | (buf[x] >> 1); }
            buf[0] >>= 1;
        }
        const uint8_t length = buf[5] >> 2;
        if (length > size - 9) continue;

        const uint16_t given = static_cast<uint16_t>(buf[6 + length] << 9) |
                               static_cast<uint16_t>(buf[7 + length] << 1) |
                               (buf[8 + length] >> 7);
        uint16_t crc = 0xffff;
        for (size_t x = 0; x < 6u + length; ++x) { crc = nrfEsbCrcUpdate(crc, buf[x], 8); }
        crc = nrfEsbCrcUpdate(crc, buf[6 + length] & 0x80, 1);
        if (crc != given) continue;

        memcpy(frame.address, buf, sizeof(frame.address));
        frame.size = length;
        for (uint8_t x = 0; x < length; ++x) {
            frame.payload[x] = (buf[6 + x] << 1) | (buf[7 + x] >> 7);
        }
        return true;
    }
    return false;
}
