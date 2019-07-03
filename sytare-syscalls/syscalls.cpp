#include <iostream>

#include "syscalls.h"

#define REG_FIRST_PARAM 12

static const elf_wl_functions_t whitelist{
    // Leds
    "leds_init",
    "led_on",
    "leds_on",
    "leds_off",

    // Energy
    "energy_init",
    "energy_reduce_consumption",
    "energy_start_measurements",
    "energy_stop_measurements"
};

Syscalls::Syscalls(platform_t &platform):
    functions(read_functions_from_args(0, nullptr, whitelist)),
    platform(platform)
{
}

void Syscalls::print() const
{
    for(const auto &it : functions)
        std::cout << std::hex << it.first << " -> " << it.second << std::endl;
}

bool Syscalls::is_syscall(uint16_t address) const
{
    return functions.find(address) != functions.cend();
}

std::string Syscalls::get_name(uint16_t address) const
{
    return functions.find(address)->second;
}

void Syscalls::run(
    uint16_t address,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB
)
{
    std::string name = get_name(address);
    if(name == "leds_init")
        leds_init();
    else if(name == "leds_off")
        leds_off();
    else if(name == "leds_on")
        leds_on();
    else if(name == "led_on")
        led_on(RB[REG_FIRST_PARAM]);
    else if(name == "energy_init")
        energy_init();
    else if(name == "energy_reduce_consumption")
        energy_reduce_consumption();
    else if(name == "energy_start_measurements")
        energy_start_measurements();
    else if(name == "energy_stop_measurements")
        energy_stop_measurements();
    else
    {
        std::cerr << "Oops: Unsupported syscall \"" << name << "\"" << std::endl;
    }
}

void Syscalls::leds_init()
{
    platform.leds.init();
}

void Syscalls::leds_off()
{
    platform.leds.off();
}

void Syscalls::leds_on()
{
    platform.leds.on();
}

void Syscalls::led_on(uint8_t n)
{
    platform.leds.on(n);
}

void Syscalls::energy_init()
{
    platform.energy.init();
}

void Syscalls::energy_reduce_consumption()
{
}

void Syscalls::energy_start_measurements()
{
    platform.energy.start();
}

void Syscalls::energy_stop_measurements()
{
    platform.energy.stop();
}

