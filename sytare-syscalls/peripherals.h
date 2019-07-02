#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <cstdint>

class Peripheral
{
    public:
        Peripheral();

        // syscalls
        void init();

    protected:
        bool check_initialized() const;

        bool initialized;
};

class Leds: public Peripheral
{
    public:
        Leds();
        
        // syscalls
        void on(uint8_t n);
        void on();
        void off();

    private:
        uint8_t value;
};

struct platform_t
{
    Leds leds;
};

#endif

