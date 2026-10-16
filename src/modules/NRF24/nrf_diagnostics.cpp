#include "nrf_common.h"
#include "core/display.h"
#include "core/mykeyboard.h"

void nrf_diagnostics() {
    if (!nrf_start(NRF_MODE_SPI)) {
        displayError("NRF24 not found", true);
        return;
    }

    // Receive only, with auto-ack disabled: exercise CE HIGH and the keyboard
    // multiplexer without starting a carrier or sending test packets.
    nrfRadio()->setAutoAck(false);
    nrfRadio()->startListening();
    unsigned int samples = 0, failures = 0;
    drawMainBorderWithTitle("NRF24 Hardware check");
    while (!check(EscPress)) {
        bool connected, ceHigh;
        {
            auto radio = nrfRadio();
            connected = radio->isChipConnected();
            ceHigh = digitalRead(bruceConfigPins.NRF24_bus.io0) == HIGH;
        }
        ++samples;
        if (!connected || !ceHigh) ++failures;
        tft.setCursor(10, 30);
        tft.setTextSize(FP);
        padprintln(String("SPI: ") + (connected ? "OK  " : "FAIL"));
        padprintln(String("RX CE: ") + (ceHigh ? "HIGH" : "LOW "));
        padprintln("Checks: " + String(samples) + "    ");
        padprintln("Failures: " + String(failures) + "    ");
        padprintln("Press keys; Esc to exit");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    {
        auto radio = nrfRadio();
        radio->stopListening();
        radio->powerDown();
    }
    Serial.printf("[NRF24 check] samples=%u failures=%u\n", samples, failures);
}
