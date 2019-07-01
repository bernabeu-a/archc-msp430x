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

