#pragma once
#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
#include "helpers_js.h"
#include <vector>

bool js_bus_int(JSContext *ctx, JSValue value, int minimum, int maximum, int &result);
bool js_bus_pin(JSContext *ctx, JSValue value, bool output, int &result);
bool js_bus_bytes(JSContext *ctx, JSValue value, size_t maximum, std::vector<uint8_t> &result);
void js_spi_end();
void js_uart_end();
void js_i2c_end();
void js_hardware_cleanup();

extern "C" {
JSValue native_spi_begin(JSContext *, JSValue *, int, JSValue *);
JSValue native_spi_transfer(JSContext *, JSValue *, int, JSValue *);
JSValue native_spi_end(JSContext *, JSValue *, int, JSValue *);
JSValue native_uart_begin(JSContext *, JSValue *, int, JSValue *);
JSValue native_uart_available(JSContext *, JSValue *, int, JSValue *);
JSValue native_uart_read(JSContext *, JSValue *, int, JSValue *);
JSValue native_uart_write(JSContext *, JSValue *, int, JSValue *);
JSValue native_uart_end(JSContext *, JSValue *, int, JSValue *);
}
#endif
