#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <cstdint>

#include "../coreutils.h"
#include "../power_supply.h"
#include "../utils/strutils.h"

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

class MPU: public Peripheral
{
    public:
        MPU(
            uint32_t address_begin,
            uint32_t address_end,
            size_t segment_count,
            ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM,
            interrupt_handler_t interrupt_handler,
            size_t interrupt_id);

        virtual current_t current_ua() const;

        uint16_t read(uint32_t address) const;
        uint8_t read_byte(uint32_t address) const;
        
        void write(uint32_t address, uint16_t word);
        void write_byte(uint32_t address, uint8_t byte);

        // Syscalls
        void block(size_t blockid);
        void unblock(size_t blockid);

    private:
        bool is_in_sram(uint32_t address, size_t &blockid);
        void fault(uint32_t address);

        ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM;
        uint32_t address_begin;
        uint32_t address_end;
        size_t block_size;
        std::vector<bool> segments; // false => unlocked, true => locked
        interrupt_handler_t interrupt_handler;
        size_t interrupt_id;

        uint16_t text_begin;
        uint16_t text_end;
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
    MPU &mpu;

    const PowerSupply &supply;

    platform_t(MPU &mpu, const PowerSupply &supply):
        mpu(mpu),
        supply(supply)
    {
    }

    current_t current_ua() const
    {
        return cpu.current_ua() + leds.current_ua() + port.current_ua() + spi.current_ua() + cc2500.current_ua() + temperature.current_ua() + accelerometer.current_ua() + mpu.current_ua();
    }
};

#endif

