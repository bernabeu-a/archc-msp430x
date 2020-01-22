#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <cstdint>

typedef long int energy_t;
typedef size_t current_t;
typedef size_t duration_t;

class Peripheral
{
    public:
        Peripheral();

        // syscalls
        virtual void init();

        virtual current_t current_ua() const = 0;

    protected:
        bool check_initialized() const;

        bool initialized;
};

class Cpu: public Peripheral
{
    public:
        Cpu();

        virtual current_t current_ua() const;

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

        virtual current_t current_ua() const;
        
        // syscalls
        void on(uint8_t n);
        void on();
        void off(uint8_t n);
        void off();

    private:
        uint8_t value;
};

class Port: public Peripheral
{
    public:
        Port();

        virtual current_t current_ua() const;
};

class Spi: public Peripheral
{
    public:
        Spi();
        // TODO: overload init() to make changes in Port device

        virtual current_t current_ua() const;
};

class CC2500: public Peripheral
{
    public:
        CC2500();

        virtual current_t current_ua() const;

        bool is_idle() const;
        bool is_sleep() const;
        bool is_rx() const;

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

class Temperature : public Peripheral
{
    public:
        Temperature();

        virtual current_t current_ua() const;

        // syscalls
        uint16_t sample() const;
};

class Accelerometer : public Peripheral
{
    public:
        Accelerometer();

        virtual current_t current_ua() const;

        // structures
        struct acquisition_t
        {
            uint16_t x, y, z;
        };

        // syscalls
        void on();
        void off();
        void measure(acquisition_t &data) const;

    private:
        enum {
            OFF,
            ON
        } state;
};

class Energy: public Peripheral
{
    public:
        Energy();

        virtual current_t current_ua() const;
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
    Temperature temperature;
    Accelerometer accelerometer;
    Energy energy;

    current_t current_ua() const
    {
        return cpu.current_ua() + leds.current_ua() + port.current_ua() + spi.current_ua() + cc2500.current_ua() + temperature.current_ua() + accelerometer.current_ua();
    }
};

#endif

