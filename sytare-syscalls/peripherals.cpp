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

/******** Leds ********/

Leds::Leds():
    value(0)
{
}

void Leds::on(uint8_t n)
{
    if(check_initialized())
        value |= (1 << n);
}

void Leds::on()
{
    if(check_initialized())
        value = 0xff;
}

void Leds::off()
{
    if(check_initialized())
        value = 0x00;
}

