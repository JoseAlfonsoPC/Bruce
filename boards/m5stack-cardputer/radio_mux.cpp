#include "radio_mux.h"

#if defined(CARDPUTER_ADV_3IN1_MUX)
#include "core/bus_HAL.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr int sharedCeSda = 8;
constexpr int sharedCsScl = 9;
SPIClass *keyboardSpi = nullptr;
bool resumeCe = false;

SemaphoreHandle_t sharedPinsMutex() {
    // A caller can protect a whole receive window and also use the guarded
    // radio accessor inside it. These acquisitions belong to the same task.
    static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    configASSERT(mutex != nullptr);
    return mutex;
}

void restoreRadioPins(bool ceHigh) {
    Wire1.end();
    // Deselect the radio before restoring CE, with shared SPI clocks stopped.
    digitalWrite(sharedCsScl, HIGH);
    pinMode(sharedCsScl, OUTPUT);
    digitalWrite(sharedCeSda, ceHigh ? HIGH : LOW);
    pinMode(sharedCeSda, OUTPUT);
}
} // namespace

void adv3in1Nrf24CriticalBegin() { xSemaphoreTakeRecursive(sharedPinsMutex(), portMAX_DELAY); }
void adv3in1Nrf24CriticalEnd() { xSemaphoreGiveRecursive(sharedPinsMutex()); }
bool adv3in1KeyboardTryBegin() { return xSemaphoreTakeRecursive(sharedPinsMutex(), 0) == pdTRUE; }
void adv3in1KeyboardEnd() { xSemaphoreGiveRecursive(sharedPinsMutex()); }

void adv3in1BeginI2c() {
    // Lock order is shared pins, then SPI. Radio access uses the same order.
    keyboardSpi = acquireSPIBus((gpio_num_t)40, (gpio_num_t)39, (gpio_num_t)14);
    if (keyboardSpi) keyboardSpi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    resumeCe = digitalRead(sharedCeSda) == HIGH;
    Wire1.begin(sharedCeSda, sharedCsScl);
    delay(5);
}

void adv3in1EndI2c() {
    // Receiving and other active radio modes need CE restored to its prior
    // level. Forcing it LOW here silently stopped them after a key press.
    restoreRadioPins(resumeCe);
    if (keyboardSpi) keyboardSpi->endTransaction();
    keyboardSpi = nullptr;
}

void adv3in1InitRadioPins() {
    (void)sharedPinsMutex();
    restoreRadioPins(false);
}
#endif
