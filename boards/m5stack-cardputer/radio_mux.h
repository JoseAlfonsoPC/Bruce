#pragma once

#if defined(CARDPUTER_ADV_3IN1_MUX)
void adv3in1Nrf24CriticalBegin();
void adv3in1Nrf24CriticalEnd();
bool adv3in1KeyboardTryBegin();
void adv3in1KeyboardEnd();
void adv3in1BeginI2c();
void adv3in1EndI2c();
void adv3in1InitRadioPins();
#endif
