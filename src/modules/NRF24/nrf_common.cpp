#include "nrf_common.h"
#include "../../core/bus_HAL.h"
#include "../../core/mykeyboard.h"

static RF24 NRFradio(bruceConfigPins.NRF24_bus.io0, bruceConfigPins.NRF24_bus.cs);
HardwareSerial NRFSerial = HardwareSerial(2); // Uses UART2 for External NRF's
SPIClass *NRFSPI;
static bool radioInitialized = false;

Nrf24Access<RF24> nrfRadio() { return Nrf24Access<RF24>(NRFradio); }
bool nrf_is_ready() { return radioInitialized; }

namespace {
NRF24_MODE configuredModes() {
#if defined(CARDPUTER_ADV_3IN1_MUX)
    // The expansion contains an SPI radio, without a UART-to-NRF bridge.
    constexpr bool spiHardwareOnly = true;
#else
    constexpr bool spiHardwareOnly = false;
#endif
    return nrfConfiguredModes(
        bruceConfigPins.NRF24_bus,
        bruceConfigPins.uart_bus,
        USBserial.getSerialOutput() == &Serial1,
        spiHardwareOnly
    );
}
} // namespace

void nrf_info() {
    const NRF24_MODE modes = configuredModes();
    bool detected = false;
    if (CHECK_NRF_SPI(modes) && nrf_start(NRF_MODE_SPI)) {
        auto radio = nrfRadio();
        detected = radio->isChipConnected();
        radio->powerDown();
    }
    drawMainBorderWithTitle("NRF24 Information");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setCursor(10, 30);
#if defined(CARDPUTER_ADV_3IN1_MUX)
    padprintln("Transport: SPI (ADV 3-in-1)");
#else
    padprintln(String("SPI pins: ") + (CHECK_NRF_SPI(modes) ? "configured" : "not configured"));
    padprintln(String("UART bridge: ") + (CHECK_NRF_UART(modes) ? "available" : "unavailable"));
#endif
    padprintln(String("Radio: ") + (detected ? "NRF24 detected" : "not detected on SPI"));
    const auto &pins = bruceConfigPins.NRF24_bus;
    padprintln("SCK " + String(pins.sck) + " MISO " + String(pins.miso) + " MOSI " + String(pins.mosi));
    padprintln("CS " + String(pins.cs) + " CE " + String(pins.io0));
    padprintln("MouseJack: supported ESB only");
    padprintln("No WiFi/Bluetooth discovery");
    padprintln("Esc: back");
    check(AnyKeyPress); // Discard the event that opened this screen.
    check(SelPress);
    while (!check(EscPress)) { vTaskDelay(1); }
}

bool nrf_start(NRF24_MODE mode) {
    bool result = false;
    if (mode <= NRF_MODE_DISABLED || mode > NRF_MODE_BOTH || (mode & configuredModes()) != mode) {
        Serial.println("Requested NRF24 transport unavailable");
        return false;
    }

    if (CHECK_NRF_UART(mode)) {
        if (USBserial.getSerialOutput() == &Serial1) {
            displayError("(E) UART already in use", true);
            return false;
        }
        NRFSerial.begin(115200, SERIAL_8N1, bruceConfigPins.uart_bus.rx, bruceConfigPins.uart_bus.tx);
        Serial.println("NRF24 on Serial Started");
        result = true;
    };

    if (!CHECK_NRF_SPI(mode)) return result;

    // Also protect CE/CS setup, before calling into the driver.
    auto radio = nrfRadio();
    radioInitialized = false;

#if defined(CARDPUTER_ADV_3IN1_MUX)
    // CC1101, NRF24 and LoRa share SCK/MISO/MOSI on the 3-in-1 board.
    // Keep every inactive device deselected before talking to NRF24.
    if (bruceConfigPins.CC1101_bus.cs != GPIO_NUM_NC) {
        pinMode(bruceConfigPins.CC1101_bus.cs, OUTPUT);
        digitalWrite(bruceConfigPins.CC1101_bus.cs, HIGH);
    }

#if !defined(LITE_VERSION)
    if (bruceConfigPins.LoRa_bus.cs != GPIO_NUM_NC) {
        pinMode(bruceConfigPins.LoRa_bus.cs, OUTPUT);
        digitalWrite(bruceConfigPins.LoRa_bus.cs, HIGH);
    }
#endif
#endif

    // Always re-assert CE LOW and CS HIGH before begin() — these pins
    // may have been left in an indeterminate state by the previous session,
    // especially after stopConstCarrier() which can leave CE HIGH internally.
    pinMode(bruceConfigPins.NRF24_bus.cs, OUTPUT);
    digitalWrite(bruceConfigPins.NRF24_bus.cs, HIGH);
    pinMode(bruceConfigPins.NRF24_bus.io0, OUTPUT);
    digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);
    delay(5); // Let pins settle before SPI traffic

    NRFSPI =
        acquireSPIBus(bruceConfigPins.NRF24_bus.sck, bruceConfigPins.NRF24_bus.miso, bruceConfigPins.NRF24_bus.mosi);
    if (!NRFSPI) {
        Serial.println("No hardware SPI bus available for NRF24, cannot initialize NRF24");
        return false;
    }
    delay(10);

    if (radio->begin(
            NRFSPI,
            rf24_gpio_pin_t(bruceConfigPins.NRF24_bus.io0),
            rf24_gpio_pin_t(bruceConfigPins.NRF24_bus.cs)
        )) {
        result = true;
        radioInitialized = true;
    } else {
        return false;
    }
    return result;
}

NRF24_MODE nrf_setMode() {
    // Automatic selection is also used by background APIs: never open a menu here.
    const NRF24_MODE modes = configuredModes();
    if (CHECK_NRF_SPI(modes)) return NRF_MODE_SPI;
    if (CHECK_NRF_UART(modes)) return NRF_MODE_UART;
    displayError("NRF24 pins not configured", true);
    return NRF_MODE_DISABLED;
}

NRF24_MODE nrf_selectMode(NRF24_MODE current, bool spiOnly) {
    const NRF24_MODE modes = configuredModes();
    NRF24_MODE selected = NRF_MODE_DISABLED;
    std::vector<Option> modeOptions;
    int initialIndex = 0;
    if (CHECK_NRF_SPI(modes)) {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        modeOptions.push_back({"SPI (ADV 3-in-1)", [&]() { selected = NRF_MODE_SPI; }});
#else
        modeOptions.push_back({"SPI", [&]() { selected = NRF_MODE_SPI; }});
#endif
    }
    if (!spiOnly && CHECK_NRF_UART(modes)) {
        if (current == NRF_MODE_UART) initialIndex = modeOptions.size();
        modeOptions.push_back({"UART bridge", [&]() { selected = NRF_MODE_UART; }});
        if (CHECK_NRF_SPI(modes)) {
            if (current == NRF_MODE_BOTH) initialIndex = modeOptions.size();
            modeOptions.push_back({"SPI + UART bridge", [&]() { selected = NRF_MODE_BOTH; }});
        }
    }
    if (modeOptions.empty()) {
        displayError(spiOnly ? "MouseJack needs SPI pins" : "NRF24 pins not configured", true);
        return NRF_MODE_DISABLED;
    }
    modeOptions.push_back({"Back", []() {}});
    loopOptions(modeOptions, MENU_TYPE_SUBMENU, "NRF transport", initialIndex);
    return selected;
}
