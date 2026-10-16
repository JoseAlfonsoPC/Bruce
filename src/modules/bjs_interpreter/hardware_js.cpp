#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
#include "hardware_js.h"
#include "core/bus_HAL.h"
#include <HardwareSerial.h>
#include <cmath>
#include <driver/gpio.h>

static SPIClass *scriptSpi = nullptr;
static int scriptCs = -1;
static uint32_t scriptSpiHz = 1000000;
static uint8_t scriptSpiMode = SPI_MODE0;
static bool scriptUartReady = false;

#if defined(CARDPUTER_ADV_3IN1_MUX)
extern void adv3in1Nrf24CriticalBegin();
extern void adv3in1Nrf24CriticalEnd();
#endif

bool js_bus_int(JSContext *ctx, JSValue value, int minimum, int maximum, int &result) {
    double number = 0;
    if (!JS_IsNumber(ctx, value)) {
        JS_ThrowTypeError(ctx, "bus argument must be a number");
        return false;
    }
    JS_ToNumber(ctx, &number, value);
    if (!std::isfinite(number) || std::floor(number) != number || number < minimum || number > maximum) {
        JS_ThrowRangeError(ctx, "bus argument is out of range");
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

bool js_bus_pin(JSContext *ctx, JSValue value, bool output, int &result) {
    if (!js_bus_int(ctx, value, 0, SOC_GPIO_PIN_COUNT - 1, result)) return false;
#if defined(CARDPUTER_ADV_3IN1_MUX)
    if (result == 8 || result == 9) {
        JS_ThrowRangeError(ctx, "GPIO8/9 are reserved for the keyboard/NRF24 multiplexer");
        return false;
    }
#endif
    if (!(output ? GPIO_IS_VALID_OUTPUT_GPIO(result) : GPIO_IS_VALID_GPIO(result))) {
        JS_ThrowRangeError(ctx, "invalid GPIO for this bus signal");
        return false;
    }
    return true;
}

bool js_bus_bytes(JSContext *ctx, JSValue value, size_t maximum, std::vector<uint8_t> &result) {
    size_t length = 0;
    JSCStringBuf text;
    const char *data = nullptr;
    if (JS_IsString(ctx, value)) data = JS_ToCStringLen(ctx, &length, value, &text);
    else if (JS_IsTypedArray(ctx, value)) data = JS_GetTypedArrayBuffer(ctx, &length, value);
    else {
        JS_ThrowTypeError(ctx, "bus data must be a string or typed array");
        return false;
    }
    if (length > maximum) {
        JS_ThrowRangeError(ctx, "bus data is too large");
        return false;
    }
    if (!data && length) {
        JS_ThrowTypeError(ctx, "invalid bus data buffer");
        return false;
    }
    if (length) result.assign(reinterpret_cast<const uint8_t *>(data),
                              reinterpret_cast<const uint8_t *>(data) + length);
    return true;
}

void js_spi_end() {
    if (scriptCs >= 0) digitalWrite(scriptCs, HIGH);
    scriptSpi = nullptr;
    scriptCs = -1;
    // The HAL owns SPI; stopping it would also stop SD/display peripherals.
}

void js_uart_end() {
    if (scriptUartReady) Serial1.end();
    scriptUartReady = false;
}

void js_hardware_cleanup() {
    js_i2c_end();
    js_spi_end();
    js_uart_end();
}

JSValue native_spi_begin(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (argc < 4) return JS_ThrowTypeError(ctx, "spi.begin(sck,miso,mosi,cs[,hz,mode]) required");
    int sck, miso, mosi, cs, hz = 1000000, mode = 0;
    if (!js_bus_pin(ctx, argv[0], true, sck) || !js_bus_pin(ctx, argv[1], false, miso) ||
        !js_bus_pin(ctx, argv[2], true, mosi) || !js_bus_pin(ctx, argv[3], true, cs) ||
        (argc > 4 && !js_bus_int(ctx, argv[4], 1000, 40000000, hz)) ||
        (argc > 5 && !js_bus_int(ctx, argv[5], 0, 3, mode))) return JS_EXCEPTION;
    if (sck == miso || sck == mosi || sck == cs || miso == mosi || miso == cs || mosi == cs)
        return JS_ThrowRangeError(ctx, "SPI pins must be distinct");
    js_spi_end();
    scriptSpi = acquireSPIBus(static_cast<gpio_num_t>(sck), static_cast<gpio_num_t>(miso),
                              static_cast<gpio_num_t>(mosi));
    if (!scriptSpi) return JS_ThrowInternalError(ctx, "no hardware SPI controller available");
    scriptCs = cs;
    scriptSpiHz = hz;
    const uint8_t modes[] = {SPI_MODE0, SPI_MODE1, SPI_MODE2, SPI_MODE3};
    scriptSpiMode = modes[mode];
    pinMode(scriptCs, OUTPUT);
    digitalWrite(scriptCs, HIGH);
    return JS_NewBool(true);
}

JSValue native_spi_transfer(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (!scriptSpi) return JS_ThrowInternalError(ctx, "call spi.begin first");
    if (argc < 1) return JS_ThrowTypeError(ctx, "spi.transfer(data) required");
    std::vector<uint8_t> data;
    if (!js_bus_bytes(ctx, argv[0], 4096, data)) return JS_EXCEPTION;
    if (!data.empty()) {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalBegin();
#endif
        scriptSpi->beginTransaction(SPISettings(scriptSpiHz, MSBFIRST, scriptSpiMode));
        digitalWrite(scriptCs, LOW);
        for (auto &byte : data) byte = scriptSpi->transfer(byte);
        digitalWrite(scriptCs, HIGH);
        scriptSpi->endTransaction();
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalEnd();
#endif
    }
    return JS_NewUint8ArrayCopy(ctx, data.data(), data.size());
}

JSValue native_spi_end(JSContext *, JSValue *, int, JSValue *) {
    js_spi_end();
    return JS_UNDEFINED;
}

JSValue native_uart_begin(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "uart.begin(baud,rx,tx) required");
    if (USBserial.getSerialOutput() == &Serial1)
        return JS_ThrowInternalError(ctx, "UART1 is currently used by the serial console");
    int baud, rx, tx;
    if (!js_bus_int(ctx, argv[0], 300, 5000000, baud) || !js_bus_pin(ctx, argv[1], false, rx) ||
        !js_bus_pin(ctx, argv[2], true, tx)) return JS_EXCEPTION;
    if (rx == tx) return JS_ThrowRangeError(ctx, "UART RX and TX must be distinct");
    js_uart_end();
    Serial1.begin(baud, SERIAL_8N1, rx, tx);
    scriptUartReady = static_cast<bool>(Serial1);
    if (!scriptUartReady) return JS_ThrowInternalError(ctx, "UART initialization failed");
    return JS_NewBool(true);
}

JSValue native_uart_available(JSContext *ctx, JSValue *, int, JSValue *) {
    if (!scriptUartReady) return JS_ThrowInternalError(ctx, "call uart.begin first");
    return JS_NewInt32(ctx, Serial1.available());
}

JSValue native_uart_read(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (!scriptUartReady) return JS_ThrowInternalError(ctx, "call uart.begin first");
    int length = 1, timeout = 0;
    if ((argc > 0 && !js_bus_int(ctx, argv[0], 0, 4096, length)) ||
        (argc > 1 && !js_bus_int(ctx, argv[1], 0, 5000, timeout))) return JS_EXCEPTION;
    std::vector<uint8_t> data;
    data.reserve(length);
    const uint32_t start = millis();
    while (data.size() < static_cast<size_t>(length)) {
        if (Serial1.available()) {
            int byte = Serial1.read();
            if (byte >= 0) data.push_back(static_cast<uint8_t>(byte));
        } else {
            if (millis() - start >= static_cast<uint32_t>(timeout)) break;
            delay(1);
        }
    }
    return JS_NewUint8ArrayCopy(ctx, data.data(), data.size());
}

JSValue native_uart_write(JSContext *ctx, JSValue *, int argc, JSValue *argv) {
    if (!scriptUartReady) return JS_ThrowInternalError(ctx, "call uart.begin first");
    if (argc < 1) return JS_ThrowTypeError(ctx, "uart.write(data) required");
    std::vector<uint8_t> data;
    if (!js_bus_bytes(ctx, argv[0], 4096, data)) return JS_EXCEPTION;
    return JS_NewInt32(ctx, data.empty() ? 0 : Serial1.write(data.data(), data.size()));
}

JSValue native_uart_end(JSContext *, JSValue *, int, JSValue *) {
    js_uart_end();
    return JS_UNDEFINED;
}
#endif
