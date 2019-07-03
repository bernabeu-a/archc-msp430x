#ifndef SYTARE_SYSCALLS_H
#define SYTARE_SYSCALLS_H

#include <cstdint>

#include "utils/elfreader.h"
#include "peripherals.h"
#include "msp430x_isa.H"

class Syscalls
{
    public:
        Syscalls(platform_t &platform);

        void print() const;
        bool is_syscall(uint16_t address) const;

        // precondition: is_syscall(address) == true
        std::string get_name(uint16_t address) const;

        // precondition: is_syscall(address) == true
        void run(
            uint16_t address,
            ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB
        );

    private:
        // Leds
        void leds_init();
        void leds_off();
        void leds_on();
        void led_on(uint8_t n);

        elf_functions_t functions;
        platform_t &platform;
};

#endif

