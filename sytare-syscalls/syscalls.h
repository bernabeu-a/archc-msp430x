#ifndef SYTARE_SYSCALLS_H
#define SYTARE_SYSCALLS_H

#include <cstdint>

#include "utils/elfreader.h"
#include "peripherals.h"
#include "energy_manager.h"
#include "msp430x_isa.H"

struct syscall_consumption_t
{
    energy_t energy_nj;
    current_t max_current_ua;
    duration_t duration_us;
};

class Syscalls
{
    public:
        Syscalls(platform_t &platform, EnergyManager &emanager);

        void print() const;
        bool is_syscall(uint16_t address) const;

        // precondition: is_syscall(address) == true
        std::string get_name(uint16_t address) const;

        // precondition: is_syscall(address) == true
        void run(
            uint16_t address,
            ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
            ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB
        );

    private:
        // Leds
        void leds_init();
        void leds_off();
        void leds_on();
        void led_on(uint8_t n);
        void led_off(uint8_t n);

        // Port
        void port_init();

        // Spi
        void spi_init();

        // cc2500
        void cc2500_init();
        void cc2500_configure();
        void cc2500_idle();
        void cc2500_sleep();
        void cc2500_wakeup();
        void cc2500_send_packet(const uint8_t *buf, size_t size);

        // Temperature
        void temperature_init();
        uint16_t temperature_sample();

        // Accelerometer
        void accelerometer_init();
        void accelerometer_on();
        void accelerometer_off();
        void accelerometer_measure(Accelerometer::acquisition_t &data);
        

        // DMA
        void dma_memset(
            ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
            uint16_t dst, uint8_t val, uint16_t len
        );

        void dma_memcpy(
            ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
            uint16_t dst, uint16_t src, uint16_t len
        );

        // Energy
        void energy_init();
        void energy_reduce_consumption();
        void energy_start_measurements();
        void energy_stop_measurements();

        elf_functions_t functions;
        platform_t &platform;
        EnergyManager &emanager;
};

#endif

