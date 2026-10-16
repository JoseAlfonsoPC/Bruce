#include "fakes.h"
#include "modules/NRF24/nrf_transport.h"
#include <cassert>
#include <iostream>

struct Pins { int sck = 40, miso = 39, mosi = 14, cs = 9, io0 = 8; };
struct Uart { int rx = 2, tx = 1; };
static int checks = 0;
static void require(bool value) { ++checks; assert(value); }

int main() {
    Pins pins;
    Uart uart;
    require(nrfConfiguredModes(pins, uart, false, false) == NRF_MODE_BOTH);
    require(nrfConfiguredModes(pins, uart, true, false) == NRF_MODE_SPI);
    require(nrfConfiguredModes(pins, uart, false, true) == NRF_MODE_SPI);

    pins.io0 = -1; // CE is required even though SPIPins::checkConflict ignores it.
    require(!nrfSpiPinsConfigured(pins));
    require(nrfConfiguredModes(pins, uart, false, false) == NRF_MODE_UART);
    require(nrfConfiguredModes(pins, uart, false, true) == NRF_MODE_DISABLED);
    require(nrfConfiguredModes(pins, uart, true, false) == NRF_MODE_DISABLED);
    pins = Pins{};
    pins.io0 = pins.cs;
    require(!nrfSpiPinsConfigured(pins));
    pins = Pins{};
    pins.io0 = pins.miso;
    require(!nrfSpiPinsConfigured(pins));
    pins = Pins{};
    pins.miso = 46; // Input-only GPIO may be used for MISO, never for CE.
    require(nrfSpiPinsConfigured(pins));
    pins = Pins{};
    pins.io0 = 46;
    require(!nrfSpiPinsConfigured(pins));
    pins = Pins{};
    pins.sck = 25; // Nonexistent GPIO in the simulated SoC.
    require(!nrfSpiPinsConfigured(pins));
    pins = Pins{};
    uart.rx = pins.io0;
    require(nrfConfiguredModes(pins, uart, false, false) == NRF_MODE_SPI);
    uart = Uart{};
    uart.tx = uart.rx;
    require(nrfConfiguredModes(pins, uart, false, false) == NRF_MODE_SPI);
    uart.tx = 46;
    require(nrfConfiguredModes(pins, uart, false, false) == NRF_MODE_SPI);
    uart = Uart{};
    uart.rx = -1;
    require(nrfConfiguredModes(pins, uart, false, false) == NRF_MODE_SPI);
    std::cout << checks << " NRF24 transport checks passed\n";
}
