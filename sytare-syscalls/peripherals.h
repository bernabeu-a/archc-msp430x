#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <cstdint>

class Peripheral
{
    public:
        Peripheral();

        // syscalls
        void init();

        virtual size_t current() const = 0;

    protected:
        bool check_initialized() const;

        bool initialized;
};

class Leds: public Peripheral
{
    public:
        Leds();

        virtual size_t current() const;
        
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

    size_t current() const
    {
        return leds.current();
    }
};

#endif

