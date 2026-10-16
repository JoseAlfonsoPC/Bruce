#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin) >= 0 && (pin) < 49 && (pin) != 25 && (pin) != 46)
#define pdMS_TO_TICKS(value) (value)
constexpr int SMOOTH_FONT = 0;
constexpr uint8_t SI4710_ADDR1 = 0x63;
constexpr int SI4713_PROP_TX_RDS_PS_MISC = 0x2c03;
inline bool AnyKeyPress = true, SelPress = true, EscPress = false, returnToMenu = false;
inline int keyChecks = 0, acquisitions = 0, releases = 0, driverBegins = 0, driverOps = 0;
inline bool busOwned = false, noBus = false, driverReady = true;
inline std::string lastError;
inline int errorScreens = 0;
inline bool check(bool &key) {
    if (++keyChecks > 100) throw std::runtime_error("Unexpected wait after detection failure");
    const bool value = key;
    key = false;
    return value;
}
inline void delay(unsigned long) {}
inline void vTaskDelay(int) {}
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
inline int constrain(int value, int low, int high) { return std::max(low, std::min(high, value)); }
struct ConfigFake { int bgColor = 0, priColor = 1; };
struct ConfigPinsFake { struct Pins { int sda = 2, scl = 1; } i2c_bus; };
inline ConfigFake bruceConfig;
inline ConfigPinsFake bruceConfigPins;
inline int tftWidth = 240, tftHeight = 135;
struct Output {
    template <typename... Args> void print(Args...) {}
    template <typename... Args> void println(Args...) {}
    template <typename... Args> void fillScreen(Args...) {}
    template <typename... Args> void setCursor(Args...) {}
    template <typename... Args> void drawCentreString(Args...) {}
    template <typename... Args> void setTextSize(Args...) {}
    template <typename... Args> void fillRect(Args...) {}
    template <typename... Args> void drawLine(Args...) {}
} inline tft, Serial;
struct Option { std::string label; std::function<void()> operation; };
inline std::vector<Option> options;
inline void addOptionToMainMenu() { returnToMenu = false; }
inline void loopOptions(std::vector<Option> &) { returnToMenu = true; }
inline void displayTextLine(const char *, bool = false) {}
inline void displayError(const char *message, bool wait) {
    if (busOwned) throw std::runtime_error("Error screen holds I2C bus");
    if (!wait || AnyKeyPress || SelPress) throw std::runtime_error("Error can disappear on entry key");
    lastError = message;
    ++errorScreens;
}
struct TwoWire {
    int error = 2, probes = 0, timeout = 0, address = 0;
    void setTimeOut(int value) { timeout = value; }
    void beginTransmission(int value) { address = value; ++probes; }
    int endTransmission() { return error; }
} inline externalWire;
inline TwoWire *acquireI2CBus(int8_t sda, int8_t scl) {
    ++acquisitions;
    if (sda != bruceConfigPins.i2c_bus.sda || scl != bruceConfigPins.i2c_bus.scl)
        throw std::runtime_error("Configured I2C pins ignored");
    busOwned = !noBus;
    return noBus ? nullptr : &externalWire;
}
inline void releaseI2CBus() { ++releases; busOwned = false; }
struct Adafruit_Si4713 {
    uint8_t currNoiseLevel = 0;
    bool begin(uint8_t address, TwoWire *wire) {
        ++driverBegins;
        if (!busOwned || wire != &externalWire || address != SI4710_ADDR1 || wire->error)
            throw std::runtime_error("Driver entered without a configured, responding bus");
        return driverReady;
    }
    template <typename... Args> void readTuneMeasure(Args...) { ++driverOps; }
    void readTuneStatus() { ++driverOps; }
    void reset() { ++driverOps; }
    template <typename... Args> void setTXpower(Args...) { ++driverOps; }
    template <typename... Args> void tuneFM(Args...) { ++driverOps; }
    void beginRDS() { ++driverOps; }
    template <typename... Args> void setRDSstation(Args...) { ++driverOps; }
    template <typename... Args> void setRDSbuffer(Args...) { ++driverOps; }
    template <typename... Args> void setProperty(Args...) { ++driverOps; }
};
