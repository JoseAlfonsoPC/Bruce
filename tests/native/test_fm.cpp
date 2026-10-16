#include "fm_test_support.h"
#include "modules/fm/fm.h"
#include <cassert>
#include <iostream>

extern bool is_running, auto_scan;
extern uint16_t fm_scan();
static int checks = 0;
static void verify(bool value, int line) {
    ++checks;
    if (!value) std::cerr << "FM check failed on line " << line << '\n';
    assert(value);
}
#define require(value) verify((value), __LINE__)
static void reset() {
    require(!busOwned);
    acquisitions = releases = driverBegins = driverOps = errorScreens = keyChecks = 0;
    externalWire = {};
    bruceConfigPins.i2c_bus = {2, 1};
    AnyKeyPress = SelPress = true;
    EscPress = returnToMenu = is_running = auto_scan = noBus = false;
    driverReady = true;
    lastError.clear();
}

int main() {
    const std::function<void()> entryPoints[] = {
        [] { require(!fm_begin()); },
        [] { require(!fm_setup(false, true)); },
        [] { fm_live_run(false); },
        [] { fm_live_run(true); },
        [] { fm_spectrum(); },
        [] { fm_ta_run(); },
        [] { fm_stop(); },
        [] { require(fm_scan() == 0); },
    };
    for (int error : {2, 4, 5}) {
        for (const auto &entry : entryPoints) {
            reset();
            externalWire.error = error;
            entry();
            require(driverBegins == 0 && driverOps == 0);
            require(acquisitions == 1 && releases == 1 && !busOwned);
            require(externalWire.address == SI4710_ADDR1 && externalWire.timeout == 50);
            require(!is_running && errorScreens == 1 && lastError == "Si4713 not found");
            require(keyChecks == 2); // No broadcast/scan idle loop follows an error.
        }
    }
    for (const auto &pins : std::vector<std::pair<int, int>>{{-1, 1}, {2, 2}, {46, 1}, {2, 49}}) {
        reset();
        bruceConfigPins.i2c_bus = {pins.first, pins.second};
        require(!fm_begin());
        require(acquisitions == 0 && driverBegins == 0 && !busOwned);
    }
#if defined(CARDPUTER_ADV_3IN1_MUX)
    for (const auto &pins : std::vector<std::pair<int, int>>{{8, 1}, {2, 8}, {9, 1}, {2, 9}}) {
        reset();
        bruceConfigPins.i2c_bus = {pins.first, pins.second};
        require(!fm_begin());
        require(acquisitions == 0 && lastError == "FM: use Grove I2C");
    }
#endif
    reset();
    noBus = true;
    require(!fm_begin());
    require(driverBegins == 0 && releases == 0 && lastError == "FM I2C unavailable");

    reset();
    externalWire.error = 0;
    driverReady = false;
    require(!fm_setup(false, true));
    require(driverBegins == 1 && driverOps == 0 && !is_running);
    require(releases == 1 && lastError == "Si4713 init failed");

    reset();
    is_running = true; // A disconnected module must also invalidate stale state.
    fm_stop();
    require(!is_running && driverOps == 0 && releases == 1);

    reset();
    externalWire.error = 0;
    fm_live_run(false); // Cancel the frequency menu without starting RF.
    require(driverBegins == 1 && driverOps == 0);
    require(!busOwned && releases == 1 && !is_running);

    reset();
    externalWire.error = 0;
    fm_stop(); // Stopping an idle device must not call an uninitialized driver.
    require(driverBegins == 0 && driverOps == 0 && releases == 1);
    std::cout << checks << " FM presence and failure checks passed\n";
}
