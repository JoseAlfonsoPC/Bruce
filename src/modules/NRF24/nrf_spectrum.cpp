#include "nrf_spectrum.h"
#include "core/display.h"
#include "core/mykeyboard.h"

#if defined(CARDPUTER_ADV_3IN1_MUX)
extern void adv3in1Nrf24CriticalBegin();
extern void adv3in1Nrf24CriticalEnd();
class NrfKeyboardGuard {
public:
    NrfKeyboardGuard() { adv3in1Nrf24CriticalBegin(); }
    ~NrfKeyboardGuard() { adv3in1Nrf24CriticalEnd(); }
};
#endif

#define CHANNELS 80
#define RGB565(r, g, b) ((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
uint8_t channel[CHANNELS];

// scanning channels
#define _BW tftWidth / CHANNELS
String scanChannels(bool web) {
    String result = "{";

    uint8_t rpdValues[CHANNELS] = {0};
    {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        NrfKeyboardGuard guard;
#endif
        digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);

        for (int i = 0; i < CHANNELS; i++) {
            nrfRadio()->setChannel(i);
            nrfRadio()->startListening();
            delayMicroseconds(128);
            nrfRadio()->stopListening();

            int rpd = nrfRadio()->testRPD() ? 1 : 0;
            channel[i] = (channel[i] * 3 + rpd * 125) / 4;
            rpdValues[i] = channel[i];
        }

#if defined(CARDPUTER_ADV_3IN1_MUX)
        digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);
        digitalWrite(bruceConfigPins.NRF24_bus.cs, HIGH);
#else
        digitalWrite(bruceConfigPins.NRF24_bus.io0, HIGH);
#endif
    } // release the shared pins before rendering the sweep

    for (int i = 0; i < CHANNELS; i++) {
        int level = rpdValues[i];
        int x = i * _BW;
        int c = i;

        tft.drawFastVLine(
            x, tftHeight - (10 + level), level, (i % 2 == 0) ? bruceConfig.priColor : TFT_DARKGREY
        ); // for level display

        tft.drawFastVLine(
            x, 0, tftHeight - (9 + level), (i % 8) ? TFT_BLACK : RGB565(25, 25, 25)
        );                                                    /// for clearing
        tft.drawFastVLine(x, 0, level, bruceConfig.secColor); /// for top display
        // show 5 channel gap only
        if (c % 5 == 0 && c != 0) { tft.drawCentreString(String(c).c_str(), x, tftHeight / 2, 1); }

        if (web) {
            if (i > 0) result += ",";
            result += String(level);
        }
    }

    if (web) result += "}";
    return result; // return a string in this format "{1,32,45,32,84,32 .... 12,54,65}" with 80 values to be
                   // used in the WebUI (Future)
}

void nrf_spectrum() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.drawString("2.40Ghz", 0, tftHeight - LH);
    tft.drawCentreString("2.44Ghz", tftWidth / 2, tftHeight - LH, 1);
    tft.drawRightString("2.48Ghz", tftWidth, tftHeight - LH, 1);

    bool started;
    {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        NrfKeyboardGuard guard;
#endif
        started = nrf_start(NRF_MODE_SPI);
    }
    if (started) {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalBegin();
#endif
        nrfRadio()->setAutoAck(false);
        nrfRadio()->disableCRC();       // accept any signal we find
        nrfRadio()->setAddressWidth(2); // a reverse engineering tactic (not typically recommended)
        const uint8_t noiseAddress[][2] = {
            {0x55, 0x55},
            {0xAA, 0xAA},
            {0xA0, 0xAA},
            {0xAB, 0xAA},
            {0xAC, 0xAA},
            {0xAD, 0xAA}
        };
        for (uint8_t i = 0; i < 6; ++i) { nrfRadio()->openReadingPipe(i, noiseAddress[i]); }
        nrfRadio()->setDataRate(RF24_1MBPS);
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalEnd();
#endif

        while (!check(EscPress)) {
            scanChannels();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalBegin();
#endif
        nrfRadio()->stopListening();
        nrfRadio()->powerDown();
#if defined(CARDPUTER_ADV_3IN1_MUX)
        digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);
        digitalWrite(bruceConfigPins.NRF24_bus.cs, HIGH);
        adv3in1Nrf24CriticalEnd();
#endif
        delay(250);
        return;

    } else {
        Serial.println("Fail Starting radio");
        displayError("NRF24 not found");
        delay(500);
        return;
    }
}
