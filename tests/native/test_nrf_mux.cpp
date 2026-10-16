#include "fakes.h"
#include "modules/NRF24/nrf_radio_access.h"
#include "radio_mux.h"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <thread>

static int checks = 0;
static void require(bool condition) { ++checks; assert(condition); }

struct FakeRadio {
    int calls = 0;
    int value() { ++calls; return 42; }
#ifdef CARDPUTER_ADV_3IN1_MUX
    bool excludesKeyboard() {
        return std::async(std::launch::async, [] {
            bool acquired = adv3in1KeyboardTryBegin();
            if (acquired) adv3in1KeyboardEnd();
            return !acquired;
        }).get();
    }
    void transaction() {
        fakeSpi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
        assert(!Wire1.begun);
        digitalWrite(9, LOW);
        assert(digitalRead(8) == HIGH);
        std::this_thread::yield();
        digitalWrite(9, HIGH);
        fakeSpi.endTransaction();
    }
#endif
};

static void returnsEarly(FakeRadio &radio) {
    Nrf24Access<FakeRadio> access(radio);
    (void)access->value();
    return;
}

int main() {
    FakeRadio radio;
    require(Nrf24Access<FakeRadio>(radio)->value() == 42);
    returnsEarly(radio);
    require(radio.calls == 2);

#ifdef CARDPUTER_ADV_3IN1_MUX
    adv3in1InitRadioPins();
    require(digitalRead(8) == LOW && digitalRead(9) == HIGH);
    require(!Wire1.begun);

    // The guard spans the complete driver call and permits nested scopes.
    require(Nrf24Access<FakeRadio>(radio)->excludesKeyboard());
    {
        Nrf24Access<FakeRadio> outer(radio);
        require(Nrf24Access<FakeRadio>(radio)->value() == 42);
        require(outer->excludesKeyboard());
    }
    require(adv3in1KeyboardTryBegin());
    adv3in1KeyboardEnd();

    // Both active and idle CE states must survive keyboard/codec I2C traffic.
    for (int level : {LOW, HIGH}) {
        digitalWrite(8, level);
        require(adv3in1KeyboardTryBegin());
        adv3in1BeginI2c();
        require(Wire1.begun && fakeSpi.transactions == 1);
        digitalWrite(8, !level);
        digitalWrite(9, LOW);
        pinWrites.clear();
        adv3in1EndI2c();
        require(digitalRead(8) == level);
        require(digitalRead(9) == HIGH);
        require(!Wire1.begun && fakeSpi.transactions == 0);
        require(pinWrites.size() == 2);
        require(pinWrites[0] == std::make_pair(9, HIGH));
        require(pinWrites[1] == std::make_pair(8, level));
        adv3in1KeyboardEnd();
    }

    // No hardware controller: restore CE and release the mutex anyway.
    failSpi = true;
    digitalWrite(8, HIGH);
    adv3in1Nrf24CriticalBegin();
    adv3in1BeginI2c();
    adv3in1EndI2c();
    adv3in1Nrf24CriticalEnd();
    require(digitalRead(8) == HIGH && fakeSpi.transactions == 0);
    failSpi = false;

    // Exercise the actual mux against competing keyboard/radio threads.
    std::atomic<int> processedKeys{0};
    digitalWrite(8, HIGH);
    std::thread keyboard([&] {
        while (processedKeys < 200) {
            if (adv3in1KeyboardTryBegin()) {
                adv3in1BeginI2c();
                digitalWrite(8, LOW); // simulated SDA transitions
                digitalWrite(9, LOW);
                adv3in1EndI2c();
                ++processedKeys;
                adv3in1KeyboardEnd();
            }
            std::this_thread::yield();
        }
    });
    for (int index = 0; index < 1000; index++) Nrf24Access<FakeRadio>(radio)->transaction();
    keyboard.join();
    require(processedKeys == 200);
    require(digitalRead(8) == HIGH && digitalRead(9) == HIGH);
    require(!Wire1.begun && fakeSpi.transactions == 0);

    // Clean shutdown is retained through subsequent keyboard input.
    {
        Nrf24Access<FakeRadio> access(radio);
        digitalWrite(8, LOW);
    }
    require(adv3in1KeyboardTryBegin());
    adv3in1BeginI2c();
    adv3in1EndI2c();
    adv3in1KeyboardEnd();
    require(digitalRead(8) == LOW);
#endif
    std::cout << checks << " NRF24 mux checks passed\n";
}
