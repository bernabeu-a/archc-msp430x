#ifndef SYTARE_SYSCALLS_H
#define SYTARE_SYSCALLS_H

#include "utils/elfreader.h"

class Syscalls
{
    public:
        Syscalls();

        void print() const;
        bool is_syscall(uint16_t address) const;

        // precondition: is_syscall(address) == true
        std::string get_name(uint16_t address) const;

    private:
        elf_functions_t functions;
};

#endif

