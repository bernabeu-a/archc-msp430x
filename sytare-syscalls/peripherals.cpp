#include <iostream>

#include "peripherals.h"

Peripheral::Peripheral():
    initialized(false)
{
}

void Peripheral::init()
{
    initialized = true;
}

bool Peripheral::check_initialized() const
{
    if(!initialized)
    {
        std::cerr << "Oops: peripheral not initialized" << std::endl;
    }
    return initialized;
}

/******** Cpu  ********/

Cpu::Cpu():
    state(ACTIVE)
{
    init(); // Cpu is always initialized
}

current_t Cpu::current_ua() const
{
    switch(state)
    {
        case LPM0:
            return 278;

        case LPM1:
            return 233;

        case LPM2:
            return 66;

        case LPM3:
        case LPM4:
            return 8;

        default:
            return 1224;
    }
}

// syscalls
void Cpu::active()
{
    state = ACTIVE;
}

void Cpu::lpm0()
{
    state = LPM0;
}

void Cpu::lpm1()
{
    state = LPM1;
}

void Cpu::lpm2()
{
    state = LPM2;
}

void Cpu::lpm3()
{
    state = LPM3;
}

void Cpu::lpm4()
{
    state = LPM4;
}

/******** Leds ********/

Leds::Leds():
    value(0)
{
}

current_t Leds::current_ua() const
{
    size_t n = 0;
    for(size_t i = 0; i < 8; ++i)
        n += ((value >> i) & 1);
    return n * 1230;
}

void Leds::on(uint8_t n)
{
    if(check_initialized())
    {
        value |= (1 << (n-1));
        std::cout << "New led value: " << std::dec << (int) value << std::endl;
    }
}

void Leds::on()
{
    if(check_initialized())
        value = 0xff;
}

void Leds::off(uint8_t n)
{
    if(check_initialized())
    {
        value &= ~(1 << (n-1));
        std::cout << "New led value: " << std::dec << (int) value << std::endl;
    }
}

void Leds::off()
{
    if(check_initialized())
        value = 0x00;
}

/******** Port ********/

Port::Port()
{
}

current_t Port::current_ua() const
{
    return 0;
}

/******** Spi ********/

Spi::Spi()
{
}

current_t Spi::current_ua() const
{
    return 0;
}

/******** CC2500 ********/
CC2500::CC2500():
    state(SLEEP)
{
}

void CC2500::init()
{
    Peripheral::init();
    state = IDLE;
}

current_t CC2500::current_ua() const
{
    //const size_t sleep_current = 215;
    const size_t sleep_current = 0;
    switch(state)
    {
        case IDLE:
            return sleep_current + 1603;

        case RX:
            return sleep_current + 16709;

        default:
            return sleep_current;
    }
}

bool CC2500::is_idle() const
{
    return state == IDLE;
}

bool CC2500::is_sleep() const
{
    return state == SLEEP;
}

bool CC2500::is_rx() const
{
    return state == RX;
}

void CC2500::idle()
{
    if(check_initialized())
        state = IDLE;
}

void CC2500::sleep()
{
    if(check_initialized())
        state = SLEEP;
}

void CC2500::wakeup()
{
    if(check_initialized())
    {
        if(state != SLEEP)
            std::cerr << "Oops CC2500::wakeup called in wrong state" << std::endl;
        else
            state = IDLE;
    }
}

void CC2500::send_packet(const uint8_t *buf, size_t size)
{
    if(check_initialized())
    {
        if(state != IDLE)
            std::cerr << "Oops CC2500::send_packet called in wrong state" << std::endl;

    }
}

/******** Temperature ********/

Temperature::Temperature()
{
}

current_t Temperature::current_ua() const
{
    return 0;
}

uint16_t Temperature::sample() const
{
    if(!check_initialized())
        std::cerr << "Oops Temperature::sample, driver not initialized" << std::endl;
    return 25;
}

/******** Accelerometer ********/

Accelerometer::Accelerometer():
    state(OFF)
{
}

current_t Accelerometer::current_ua() const
{
    return state == OFF ? 0 : 385; // 384.6 uA
}

void Accelerometer::on()
{
    if(check_initialized())
        state = ON;
}

void Accelerometer::off()
{
    if(check_initialized())
        state = OFF;
}

void Accelerometer::measure(acquisition_t &data) const
{
    data.x = 1;
    data.y = 2;
    data.z = 3;
}

/******** Energy ********/

Energy::Energy():
    measuring(false)
{
}

current_t Energy::current_ua() const
{
    return 0;
}

bool Energy::is_measuring() const
{
    return measuring;
}

void Energy::start()
{
    if(check_initialized())
    {
        if(measuring)
            std::cerr << "Oops Energy::start called in wrong state" << std::endl;
        else
        {
            measuring = true;
            std::cout << "Energy start" << std::endl;
        }
    }
}

void Energy::stop()
{
    if(check_initialized())
    {
        if(!measuring)
            std::cerr << "Oops Energy::stop called in wrong state" << std::endl;
        else
        {
            measuring = false;
            std::cout << "Energy stop" << std::endl;
        }
    }
}

