#pragma once
#include <cstddef>
#include <driver/gpio.h>

enum NRF24_MODE {
    NRF_MODE_DISABLED,
    NRF_MODE_SPI,
    NRF_MODE_UART,
    NRF_MODE_BOTH
};
#define CHECK_NRF_SPI(mode) (mode & NRF_MODE_SPI)
#define CHECK_NRF_UART(mode) (mode & NRF_MODE_UART)
#define CHECK_NRF_BOTH(mode) (mode == NRF_MODE_BOTH)

template <typename Pins> bool nrfSpiPinsConfigured(const Pins &pins) {
    const int outputPins[] = {pins.sck, pins.mosi, pins.cs, pins.io0};
    if (!GPIO_IS_VALID_GPIO(pins.miso)) return false;
    for (int pin : outputPins) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || pin == pins.miso) return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            if (outputPins[i] == outputPins[j]) return false;
        }
    }
    return true;
}

template <typename Pins, typename Uart>
NRF24_MODE nrfConfiguredModes(const Pins &pins, const Uart &uart, bool uartReserved, bool spiHardwareOnly) {
    const bool spi = nrfSpiPinsConfigured(pins);
    if (spiHardwareOnly) return spi ? NRF_MODE_SPI : NRF_MODE_DISABLED;
    bool serial = GPIO_IS_VALID_GPIO(uart.rx) && GPIO_IS_VALID_OUTPUT_GPIO(uart.tx) &&
                  uart.rx != uart.tx && !uartReserved;
    const int radioPins[] = {pins.sck, pins.miso, pins.mosi, pins.cs, pins.io0};
    for (int pin : radioPins) {
        if (pin >= 0 && (pin == uart.rx || pin == uart.tx)) serial = false;
    }
    return static_cast<NRF24_MODE>((spi ? NRF_MODE_SPI : 0) | (serial ? NRF_MODE_UART : 0));
}
