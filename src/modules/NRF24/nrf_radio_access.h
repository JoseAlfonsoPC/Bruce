#pragma once

#if defined(CARDPUTER_ADV_3IN1_MUX)
void adv3in1Nrf24CriticalBegin();
void adv3in1Nrf24CriticalEnd();
#endif

// The temporary owns the shared pins until the end of the complete expression,
// including the entire RF24 method (CE transitions and its SPI transactions).
// Other boards have no additional lock or allocation.
template <typename Radio> class Nrf24Access {
public:
    explicit Nrf24Access(Radio &radio) : radio_(&radio) {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalBegin();
#endif
    }
    ~Nrf24Access() {
#if defined(CARDPUTER_ADV_3IN1_MUX)
        adv3in1Nrf24CriticalEnd();
#endif
    }
    Nrf24Access(const Nrf24Access &) = delete;
    Nrf24Access &operator=(const Nrf24Access &) = delete;
    Radio *operator->() const { return radio_; }

private:
    Radio *radio_;
};
