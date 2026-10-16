#pragma once
#define __HELPERS_JS_H__
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct JSContext { int errors = 0; };
struct JSCStringBuf {};
struct JSValue {
    enum Kind { Undefined, Number, Bool, String, Bytes, Array, Exception } kind = Undefined;
    double number = 0;
    std::shared_ptr<std::string> text;
    std::shared_ptr<std::vector<uint8_t>> bytes;
    std::shared_ptr<std::vector<JSValue>> array;
};
inline const JSValue JS_UNDEFINED{};
inline const JSValue JS_EXCEPTION{JSValue::Exception};
inline JSValue JS_NewInt32(JSContext *, int value) { JSValue v; v.kind = JSValue::Number; v.number = value; return v; }
inline JSValue JS_NewBool(bool value) { JSValue v; v.kind = JSValue::Bool; v.number = value; return v; }
inline bool JS_IsNumber(JSContext *, JSValue v) { return v.kind == JSValue::Number; }
inline bool JS_IsString(JSContext *, JSValue v) { return v.kind == JSValue::String; }
inline bool JS_IsTypedArray(JSContext *, JSValue v) { return v.kind == JSValue::Bytes; }
inline bool JS_IsException(JSValue v) { return v.kind == JSValue::Exception; }
inline void JS_ToNumber(JSContext *, double *out, JSValue v) { *out = v.number; }
inline bool JS_ToBool(JSContext *, JSValue v) { return v.number != 0; }
inline JSValue JS_ThrowTypeError(JSContext *ctx, const char *) { ++ctx->errors; return JS_EXCEPTION; }
inline JSValue JS_ThrowRangeError(JSContext *ctx, const char *) { ++ctx->errors; return JS_EXCEPTION; }
inline JSValue JS_ThrowInternalError(JSContext *ctx, const char *) { ++ctx->errors; return JS_EXCEPTION; }
inline const char *JS_ToCStringLen(JSContext *, size_t *len, JSValue v, JSCStringBuf *) {
    *len = v.text->size(); return v.text->data();
}
inline const char *JS_GetTypedArrayBuffer(JSContext *, size_t *len, JSValue v) {
    *len = v.bytes->size(); return reinterpret_cast<const char *>(v.bytes->data());
}
inline JSValue JS_NewUint8ArrayCopy(JSContext *, const uint8_t *bytes, size_t length) {
    JSValue v; v.kind = JSValue::Bytes; v.bytes = std::make_shared<std::vector<uint8_t>>();
    if (length) v.bytes->assign(bytes, bytes + length);
    return v;
}
inline JSValue JS_NewArray(JSContext *, size_t size) {
    JSValue v; v.kind = JSValue::Array; v.array = std::make_shared<std::vector<JSValue>>(size); return v;
}
inline JSValue JS_SetPropertyUint32(JSContext *, JSValue v, size_t index, JSValue item) { v.array->at(index) = item; return JS_UNDEFINED; }
struct JSGCRef { JSValue value; };
inline JSValue *JS_PushGCRef(JSContext *, JSGCRef *ref) { return &ref->value; }
inline JSValue JS_PopGCRef(JSContext *, JSGCRef *ref) { return ref->value; }

using gpio_num_t = int;
constexpr int SOC_GPIO_PIN_COUNT = 49;
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) < 49 && (pin) != 25)
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) (GPIO_IS_VALID_GPIO(pin) && (pin) != 46)
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, MSBFIRST = 1;
constexpr int SPI_MODE0 = 0, SPI_MODE1 = 1, SPI_MODE2 = 2, SPI_MODE3 = 3;
constexpr int SERIAL_8N1 = 0;
inline uint32_t fakeMillis = 0;
inline std::map<int, int> pinLevels;
inline std::vector<std::pair<int, int>> pinWrites;
inline uint32_t millis() { return fakeMillis; }
inline void delay(unsigned long value) { fakeMillis += value; }
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int level) { pinLevels[pin] = level; pinWrites.emplace_back(pin, level); }
inline int digitalRead(int pin) { return pinLevels[pin]; }

struct SPISettings { SPISettings(uint32_t, int, uint8_t) {} };
struct SPIClass {
    std::mutex mutex;
    int transactions = 0, transfers = 0;
    void beginTransaction(SPISettings) { mutex.lock(); ++transactions; }
    uint8_t transfer(uint8_t byte) { ++transfers; return byte ^ 0xff; }
    void endTransaction() { --transactions; mutex.unlock(); }
};
struct TwoWire {
    bool begun = false;
    unsigned int clock = 100000;
    int writes = 0, requests = 0, stops = 0, error = 0, address = 0;
    size_t writeLimit = 128;
    std::deque<uint8_t> rx;
    bool begin(int, int) { begun = true; return true; }
    bool end() { begun = false; return true; }
    void setClock(uint32_t value) { clock = value; }
    void setTimeOut(int) {}
    void beginTransmission(int value) { address = value; }
    int endTransmission(bool stop) { stops += stop; return error; }
    size_t write(const uint8_t *, size_t length) { ++writes; return std::min(length, writeLimit); }
    size_t requestFrom(uint8_t, size_t length, bool) { ++requests; return std::min(length, rx.size()); }
    int available() { return rx.size(); }
    int read() { if (rx.empty()) return -1; int value = rx.front(); rx.pop_front(); return value; }
};
struct HardwareSerial {
    bool ready = false;
    int starts = 0, ends = 0;
    std::deque<uint8_t> rx;
    std::vector<uint8_t> tx;
    void begin(int, int, int, int) { ready = true; ++starts; }
    void end() { ready = false; ++ends; }
    explicit operator bool() const { return ready; }
    int available() { return rx.size(); }
    int read() { if (rx.empty()) return -1; int v = rx.front(); rx.pop_front(); return v; }
    size_t write(const uint8_t *data, size_t size) { tx.assign(data, data + size); return size; }
};
inline HardwareSerial Serial1;
struct USBSerialFake {
    HardwareSerial *output = nullptr;
    HardwareSerial *getSerialOutput() { return output; }
};
inline USBSerialFake USBserial;
inline int sharedPinLocks = 0;
#ifndef TEST_REAL_ADV_MUX
inline void adv3in1Nrf24CriticalBegin() { ++sharedPinLocks; }
inline void adv3in1Nrf24CriticalEnd() { --sharedPinLocks; }
#endif
inline TwoWire externalWire, systemWire, Wire1;
inline SPIClass fakeSpi;
inline bool failI2c = false, failSpi = false;
inline int i2cReleases = 0;
inline TwoWire *acquireI2CBus(int8_t sda, int8_t) { return failI2c ? nullptr : sda == 8 ? &systemWire : &externalWire; }
inline TwoWire *getSysI2CBus() { return &systemWire; }
inline void releaseI2CBus() { ++i2cReleases; }
inline SPIClass *acquireSPIBus(gpio_num_t, gpio_num_t, gpio_num_t) { return failSpi ? nullptr : &fakeSpi; }
