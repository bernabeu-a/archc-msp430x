#include <iostream>

#include "syscalls.h"

static const elf_wl_functions_t whitelist{
    "leds_init",
    "led_on",
    "leds_on",
    "leds_off"
};

Syscalls::Syscalls():
    functions(read_functions_from_args(0, nullptr, whitelist))
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

void Syscalls::run(uint16_t address)
{
    std::string name = get_name(address);
    if(name == "leds_init")
        leds_init();
    else if(name == "leds_off")
        leds_off();
    else if(name == "leds_on")
        leds_on();
    else if(name == "led_on")
    {
        // TODO: param
        led_on(0);
    }
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

