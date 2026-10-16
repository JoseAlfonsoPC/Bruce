#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
#include "i2c_js.h"
#include "hardware_js.h"
#include "core/bus_HAL.h"

static TwoWire *jsI2CBus = nullptr;
static bool jsI2CPendingStop = false;

static void i2c_finish_pending() {
    if (jsI2CBus && jsI2CPendingStop) jsI2CBus->endTransmission(true);
    jsI2CPendingStop = false;
}

static bool i2c_ready(JSContext *ctx) {
    if (jsI2CBus) return true;
    JS_ThrowInternalError(ctx, "call i2c.begin first");
    return false;
}

void js_i2c_end() {
    i2c_finish_pending();
    if (jsI2CBus) releaseI2CBus();
    jsI2CBus = nullptr;
}

JSValue native_i2c_begin(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "i2c.begin(sda,scl,hz) required");
    int sda, scl, hz;
    if (!js_bus_pin(ctx, argv[0], true, sda) || !js_bus_pin(ctx, argv[1], true, scl) ||
        !js_bus_int(ctx, argv[2], 1000, 1000000, hz)) return JS_EXCEPTION;
    if (sda == scl) return JS_ThrowRangeError(ctx, "I2C SDA and SCL must be distinct");
    js_i2c_end();
    jsI2CBus = acquireI2CBus(static_cast<int8_t>(sda), static_cast<int8_t>(scl));
    if (!jsI2CBus) return JS_ThrowInternalError(ctx, "no I2C bus available");
    // Preserve the clock/timeout of the keyboard and other system peripherals.
    if (jsI2CBus != getSysI2CBus()) {
        jsI2CBus->setClock(hz);
        jsI2CBus->setTimeOut(50);
    }
    return JS_NewBool(true);
}

JSValue native_i2c_scan(JSContext *ctx, JSValue *, int, JSValue *) {
    if (!i2c_ready(ctx)) return JS_EXCEPTION;
    i2c_finish_pending();
    std::vector<uint8_t> addresses;
    for (uint8_t address = 1; address < 127; address++) {
        jsI2CBus->beginTransmission(address);
        if (jsI2CBus->endTransmission(true) == 0) addresses.push_back(address);
    }
    JSGCRef reference;
    JSValue *result = JS_PushGCRef(ctx, &reference);
    *result = JS_NewArray(ctx, addresses.size());
    if (!JS_IsException(*result)) {
        for (size_t index = 0; index < addresses.size(); index++) {
            if (JS_IsException(JS_SetPropertyUint32(ctx, *result, index, JS_NewInt32(ctx, addresses[index])))) {
                JS_PopGCRef(ctx, &reference);
                return JS_EXCEPTION;
            }
        }
    }
    return JS_PopGCRef(ctx, &reference);
}

JSValue native_i2c_write(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (!i2c_ready(ctx)) return JS_EXCEPTION;
    if (argc < 2) return JS_ThrowTypeError(ctx, "i2c.write(address,data[,stop]) required");
    int address;
    std::vector<uint8_t> data;
    if (!js_bus_int(ctx, argv[0], 1, 126, address) || !js_bus_bytes(ctx, argv[1], 128, data))
        return JS_EXCEPTION;
    bool stop = argc < 3 || JS_ToBool(ctx, argv[2]);
    i2c_finish_pending();
    jsI2CBus->beginTransmission(address);
    if (!data.empty() && jsI2CBus->write(data.data(), data.size()) != data.size()) {
        jsI2CBus->endTransmission(true);
        return JS_NewInt32(ctx, 1);
    }
    uint8_t error = jsI2CBus->endTransmission(stop);
    jsI2CPendingStop = !stop;
    if (error) i2c_finish_pending();
    return JS_NewInt32(ctx, error);
}

static JSValue i2c_read_bytes(JSContext *ctx, int address, int length) {
    std::vector<uint8_t> data;
    if (length) {
        size_t received = jsI2CBus->requestFrom(static_cast<uint8_t>(address),
                                              static_cast<size_t>(length), true);
        jsI2CPendingStop = false;
        data.reserve(received);
        while (data.size() < received && jsI2CBus->available()) {
            int byte = jsI2CBus->read();
            if (byte < 0) break;
            data.push_back(static_cast<uint8_t>(byte));
        }
    } else i2c_finish_pending();
    return JS_NewUint8ArrayCopy(ctx, data.data(), data.size());
}

JSValue native_i2c_read(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (!i2c_ready(ctx)) return JS_EXCEPTION;
    if (argc < 2) return JS_ThrowTypeError(ctx, "i2c.read(address,length) required");
    int address, length;
    if (!js_bus_int(ctx, argv[0], 1, 126, address) || !js_bus_int(ctx, argv[1], 0, 128, length))
        return JS_EXCEPTION;
    return i2c_read_bytes(ctx, address, length);
}

JSValue native_i2c_write_read(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (!i2c_ready(ctx)) return JS_EXCEPTION;
    if (argc < 3) return JS_ThrowTypeError(ctx, "i2c.writeRead(address,data,length[,waitMs]) required");
    int address, length, wait = 0;
    std::vector<uint8_t> data;
    if (!js_bus_int(ctx, argv[0], 1, 126, address) || !js_bus_bytes(ctx, argv[1], 128, data) ||
        !js_bus_int(ctx, argv[2], 0, 128, length) ||
        (argc > 3 && !js_bus_int(ctx, argv[3], 0, 1000, wait))) return JS_EXCEPTION;
    i2c_finish_pending();
    jsI2CBus->beginTransmission(address);
    if (!data.empty() && jsI2CBus->write(data.data(), data.size()) != data.size()) {
        jsI2CBus->endTransmission(true);
        return JS_NewInt32(ctx, -1);
    }
    uint8_t error = jsI2CBus->endTransmission(length == 0);
    jsI2CPendingStop = length != 0;
    if (error) {
        i2c_finish_pending();
        return JS_NewInt32(ctx, -error);
    }
    if (wait) delay(wait);
    return i2c_read_bytes(ctx, address, length);
}

JSValue native_i2c_end(JSContext *, JSValue *, int, JSValue *) {
    js_i2c_end();
    return JS_UNDEFINED;
}
#endif
