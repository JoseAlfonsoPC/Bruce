#ifndef __NRF_COMMON_H
#define __NRF_COMMON_H
#include <RF24.h>
#include <globals.h>
#include "nrf_radio_access.h"
#include "nrf_transport.h"

// Define the Macros case it hasn't been declared
#ifndef NRF24_CE_PIN
#define NRF24_CE_PIN -1
#endif
#ifndef NRF24_SS_PIN
#define NRF24_SS_PIN -1
#endif

Nrf24Access<RF24> nrfRadio();
extern HardwareSerial NRFSerial; // Uses UART2 for External NRF's

NRF24_MODE nrf_setMode();
NRF24_MODE nrf_selectMode(NRF24_MODE current, bool spiOnly = false);

bool nrf_start(NRF24_MODE mode);
bool nrf_is_ready();
void nrf_diagnostics();

void nrf_info();
#endif
