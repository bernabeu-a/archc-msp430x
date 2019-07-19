#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <cstdint>

class Peripheral
{
    public:
        Peripheral();

        // syscalls
        virtual void init();

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

class Port: public Peripheral
{
    public:
        Port();

        virtual size_t current() const;
};

class Spi: public Peripheral
{
    public:
        Spi();
        // TODO: overload init() to make changes in Port device

        virtual size_t current() const;
};

class CC2500: public Peripheral
{
    public:
        CC2500();

        virtual size_t current() const;

        // syscalls
        void init();
        void idle();
        void sleep();
        void wakeup();
        void send_packet(const uint8_t *buf, size_t size);

    private:
        enum {
            IDLE,
            SLEEP,
            RX
        } state;
};

class Energy: public Peripheral
{
    public:
        Energy();

        virtual size_t current() const;
        bool is_measuring() const;
        
        // syscalls
        void start();
        void stop();

    private:
        bool measuring;
};

struct platform_t
{
    Cpu cpu;
    Leds leds;
    Port port;
    Spi spi;
    CC2500 cc2500;
    Energy energy;

    size_t current() const
    {
        return cpu.current() + leds.current() + port.current() + spi.current() + cc2500.current();
    }
};

#endif

