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

class Cpu: public Peripheral
{
    public:
        Cpu();

        virtual size_t current() const;

        // syscalls
        void active();
        void lpm0();
        void lpm1();
        void lpm2();
        void lpm3();
        void lpm4();

    private:
        enum {
            ACTIVE,
            LPM0,
            LPM1,
            LPM2,
            LPM3,
            LPM4
        } state;
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
    Cpu cpu;
    Leds leds;

    size_t current() const
    {
        return cpu.current() + leds.current();
    }
};

#endif

