#include "hardware_js.h"
#include "i2c_js.h"
#include <cassert>
#include <cmath>
#include <iostream>

using Native = JSValue (*)(JSContext *, JSValue *, int, JSValue *);
static JSContext ctx;
static int checks = 0;
static JSValue num(double n) { auto v = JS_NewInt32(&ctx, 0); v.number = n; return v; }
static JSValue bytes(std::initializer_list<uint8_t> data) { return JS_NewUint8ArrayCopy(&ctx, data.begin(), data.size()); }
static JSValue call(Native fn, std::initializer_list<JSValue> values = {}) {
    std::vector<JSValue> args(values); return fn(&ctx, nullptr, args.size(), args.data());
}
static void require(bool condition) { ++checks; assert(condition); }
static void error(Native fn, std::initializer_list<JSValue> args = {}) { require(JS_IsException(call(fn, args))); }

int main() {
    error(native_i2c_scan);
    error(native_i2c_read, {num(32), num(1)});
    error(native_i2c_write, {num(32), bytes({1})});
    error(native_spi_transfer, {bytes({1})});
    error(native_uart_read);
    error(native_uart_write, {bytes({1})});
    error(native_uart_available);
    error(native_i2c_begin, {num(2), num(2), num(100000)});
    error(native_i2c_begin, {num(25), num(1), num(100000)});
    error(native_i2c_begin, {num(46), num(1), num(100000)});
    error(native_i2c_begin, {num(2), num(1), num(NAN)});
    error(native_i2c_begin, {num(2), num(1), num(1.5)});
    failI2c = true;
    error(native_i2c_begin, {num(2), num(1), num(100000)});
    failI2c = false;
    require(call(native_i2c_begin, {num(2), num(1), num(400000)}).number == 1);
    require(externalWire.clock == 400000);
    externalWire.rx = {0x00, 0x80, 0xff};
    auto read = call(native_i2c_read, {num(32), num(8)});
    require(*read.bytes == std::vector<uint8_t>({0, 128, 255}));
    require(call(native_i2c_read, {num(32), num(0)}).bytes->empty());
    error(native_i2c_read, {num(127), num(1)});
    error(native_i2c_read, {num(32), num(-1)});
    error(native_i2c_read, {num(32), num(129)});
    require(call(native_i2c_write, {num(32), bytes({1})}).number == 0);
    externalWire.error = 2;
    require(call(native_i2c_write_read, {num(32), bytes({1}), num(2)}).number == -2);
    externalWire.error = 0;
    externalWire.rx = {9};
    require(call(native_i2c_write_read, {num(32), bytes({1}), num(2)}).bytes->at(0) == 9);
    externalWire.writeLimit = 0;
    require(call(native_i2c_write_read, {num(32), bytes({1}), num(1)}).number == -1);
    externalWire.writeLimit = 128;
#ifdef CARDPUTER_ADV_3IN1_MUX
    error(native_i2c_begin, {num(8), num(9), num(1000000)});
    error(native_uart_begin, {num(115200), num(8), num(9)});
    error(native_spi_begin, {num(40), num(39), num(14), num(9)});
    require(call(native_i2c_scan).array->size() == 126);
#else
    call(native_i2c_begin, {num(8), num(9), num(1000000)});
    require(systemWire.clock == 100000);
    require(call(native_i2c_scan).array->size() == 126);
#endif
    call(native_i2c_begin, {num(2), num(1), num(100000)});
    call(native_i2c_write, {num(32), bytes({1}), JS_NewBool(false)});
    int stops = externalWire.stops;
    call(native_i2c_end);
    require(externalWire.stops == stops + 1);
    error(native_i2c_scan);
    error(native_spi_begin, {num(40), num(39), num(14), num(14)});
    error(native_spi_begin, {num(40), num(39), num(14), num(15), num(0)});
    error(native_spi_begin, {num(40), num(39), num(14), num(15), num(1000000), num(4)});
    failSpi = true;
    error(native_spi_begin, {num(40), num(39), num(14), num(15)});
    failSpi = false;
    require(call(native_spi_begin, {num(40), num(39), num(14), num(15)}).number == 1);
    require(pinLevels[15] == HIGH);
    auto transfer = call(native_spi_transfer, {bytes({0, 128, 255})});
    require(*transfer.bytes == std::vector<uint8_t>({255, 127, 0}));
    require(fakeSpi.transactions == 0 && pinLevels[15] == HIGH);
    require(sharedPinLocks == 0);
    int transfers = fakeSpi.transfers;
    require(call(native_spi_transfer, {bytes({})}).bytes->empty());
    require(fakeSpi.transfers == transfers);
    JSValue huge = JS_NewUint8ArrayCopy(&ctx, nullptr, 0);
    huge.bytes->resize(4097);
    error(native_spi_transfer, {huge});
    call(native_spi_end);
    error(native_spi_transfer, {bytes({1})});
    error(native_uart_begin, {num(115200), num(1), num(1)});
    error(native_uart_begin, {num(1), num(1), num(2)});
    USBserial.output = &Serial1;
    error(native_uart_begin, {num(115200), num(1), num(2)});
    USBserial.output = nullptr;
    require(call(native_uart_begin, {num(115200), num(1), num(2)}).number == 1);
    Serial1.rx = {0, 255};
    require(call(native_uart_available).number == 2);
    require(*call(native_uart_read, {num(4), num(3)}).bytes == std::vector<uint8_t>({0, 255}));
    require(fakeMillis == 3);
    require(call(native_uart_write, {bytes({0, 255})}).number == 2);
    require(Serial1.tx == std::vector<uint8_t>({0, 255}));
    error(native_uart_read, {num(4097)});
    error(native_uart_read, {num(1), num(5001)});
    error(native_uart_write, {huge});
    call(native_i2c_begin, {num(2), num(1), num(100000)});
    call(native_spi_begin, {num(40), num(39), num(14), num(15)});
    js_hardware_cleanup();
    require(!Serial1.ready && pinLevels[15] == HIGH);
    error(native_spi_transfer, {bytes({1})});
    error(native_uart_available);
    error(native_i2c_scan);
    js_hardware_cleanup();
    std::cout << checks << " native bus checks passed\n";
}
