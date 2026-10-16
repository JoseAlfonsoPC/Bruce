#include "modules/NRF24/nrf_esb.h"
#include "modules/NRF24/nrf_esb_stats.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

static size_t checks = 0;
static void require(bool condition) { ++checks; assert(condition); }

// Independent wire-format encoder: serialize bits, then divide the message
// polynomial by x^16+x^12+x^5+1. Production uses byte-wise CRC updates.
static std::array<uint8_t, 32> encode(const std::array<uint8_t, 5> &address,
                                    const std::vector<uint8_t> &payload,
                                    uint8_t pid, bool noAck) {
    std::vector<bool> bits;
    auto append = [&](uint32_t value, unsigned count) {
        for (unsigned i = count; i > 0; --i) bits.push_back((value >> (i - 1)) & 1);
    };
    for (auto value : address) append(value, 8);
    append(payload.size(), 6);
    append(pid, 2);
    append(noAck, 1);
    for (auto value : payload) append(value, 8);
    const size_t messageBits = bits.size();
    std::vector<bool> division(bits);
    division.resize(messageBits + 16, false);
    // CRC initial register 0xffff is equivalent to complementing the first
    // sixteen message bits for polynomial division.
    for (size_t bit = 0; bit < 16; ++bit) division[bit] = !division[bit];
    for (size_t bit = 0; bit < messageBits; ++bit) {
        if (!division[bit]) continue;
        for (auto term : {0, 4, 11, 16}) division[bit + term] = !division[bit + term];
    }
    for (size_t bit = messageBits; bit < messageBits + 16; ++bit) bits.push_back(division[bit]);
    std::array<uint8_t, 32> raw{};
    for (size_t bit = 0; bit < bits.size(); ++bit) {
        raw[bit / 8] |= uint8_t(bits[bit]) << (7 - bit % 8);
    }
    return raw;
}

static std::array<uint8_t, 32> earlyAlignment(const std::array<uint8_t, 32> &raw) {
    auto early = raw;
    for (size_t i = 0; i < early.size(); ++i) {
        early[i] = (raw[i] << 1) | (i + 1 < raw.size() ? raw[i + 1] >> 7 : 0);
    }
    return early;
}

int main() {
    NrfEsbFrame decoded{};
    const std::array<uint8_t, 5> address{0x34, 0x72, 0xe1, 0x09, 0x53};
    require(!nrfDecodeEsb(nullptr, 32, decoded));
    std::array<uint8_t, 33> blank{};
    for (size_t length = 0; length < 9; ++length) {
        require(!nrfDecodeEsb(blank.data(), length, decoded));
    }
    require(!nrfDecodeEsb(blank.data(), 33, decoded));
    require(!nrfDecodeEsb(blank.data(), 32, decoded));

    for (uint8_t length = 0; length <= 23; ++length) {
        std::vector<uint8_t> payload(length);
        for (uint8_t i = 0; i < length; ++i) payload[i] = uint8_t(i * 37 + length);
        for (uint8_t pid = 0; pid < 4; ++pid) {
            for (bool noAck : {false, true}) {
                const auto raw = encode(address, payload, pid, noAck);
                for (const auto &capture : {raw, earlyAlignment(raw)}) {
                    // Use an exact-size allocation so ASan catches overreads.
                    const std::vector<uint8_t> packet(capture.begin(), capture.begin() + length + 9);
                    require(nrfDecodeEsb(packet.data(), packet.size(), decoded));
                    require(decoded.size == length);
                    require(memcmp(decoded.address, address.data(), address.size()) == 0);
                    require(length == 0 || memcmp(decoded.payload, payload.data(), length) == 0);
                    require(!nrfDecodeEsb(packet.data(), packet.size() - 1, decoded));
                }
                // Every single-bit error in the address, PCF, payload or CRC
                // must be rejected; trailing capture padding is not protected.
                for (size_t bit = 0; bit < 40 + 9 + 8u * length + 16; ++bit) {
                    auto corrupt = raw;
                    corrupt[bit / 8] ^= uint8_t(1u << (7 - bit % 8));
                    require(!nrfDecodeEsb(corrupt.data(), corrupt.size(), decoded));
                }
            }
        }
    }
    // PCF announces more payload than can fit in a complete raw capture.
    for (uint8_t length = 24; length < 64; ++length) {
        blank[5] = uint8_t(length << 2);
        require(!nrfDecodeEsb(blank.data(), 32, decoded));
    }
    // A full-alignment address may also start with a set bit.
    auto highAddress = address;
    highAddress[0] |= 0x80;
    const auto raw = encode(highAddress, {0, 0xc2, 0, 0, 0, 0, 0, 0, 0, 0x3e}, 2, false);
    require(nrfDecodeEsb(raw.data(), raw.size(), decoded));
    require(memcmp(decoded.address, highAddress.data(), highAddress.size()) == 0);

    // End-to-end scan diagnostics must distinguish noise, empty responses and
    // data without mixing empty responses into the data count.
    NrfEsbScanStats stats;
    require(!stats.consume(nullptr, 32, 3, decoded));
    require(stats.received == 0);
    blank.fill(0);
    require(!stats.consume(blank.data(), 32, 3, decoded));
    blank.fill(0xff);
    require(!stats.consume(blank.data(), 32, 3, decoded));
    require(stats.received == 2 && stats.flat == 2 && stats.valid == 0);
    const auto response = encode(address, {}, 1, false);
    require(stats.consume(response.data(), response.size(), 14, decoded));
    require(stats.received == 3 && stats.valid == 1 && stats.empty == 1 && stats.data == 0);
    require(stats.lastLength == 0 && stats.header0 == 0 && stats.header1 == 0);
    require(stats.consume(raw.data(), raw.size(), 45, decoded));
    require(stats.valid == 2 && stats.empty == 1 && stats.data == 1);
    require(stats.lastLength == 10 && stats.lastChannel == 45);
    require(stats.header0 == 0 && stats.header1 == 0xc2 && stats.sum8 == 0);
    require(stats.consume(response.data(), response.size(), 15, decoded));
    require(stats.lastLength == 10 && stats.lastChannel == 45); // Empty keeps last data metadata.
    const auto shortFrame = encode(address, {0xa5}, 3, true);
    require(stats.consume(shortFrame.data(), shortFrame.size(), 71, decoded));
    require(stats.lastLength == 1 && stats.lastChannel == 71);
    require(stats.header0 == 0xa5 && stats.header1 == 0 && stats.sum8 == 0xa5);
    require(stats.valid == stats.empty + stats.data && stats.valid <= stats.received);
    auto corrupt = raw;
    corrupt[9] ^= 0x20;
    require(!stats.consume(corrupt.data(), corrupt.size(), 8, decoded));
    require(stats.lastLength == 1 && stats.lastChannel == 71 && stats.valid == 4);
    stats = {};
    require(stats.received == 0 && stats.valid == 0 && stats.empty == 0 && stats.data == 0 && stats.flat == 0);
    require(stats.lastLength == 0 && stats.lastChannel == 0 && stats.header0 == 0 && stats.header1 == 0 && stats.sum8 == 0);
    std::cout << checks << " passive ESB decoder checks passed\n";
}
