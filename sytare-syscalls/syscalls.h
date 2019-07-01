#ifndef SYTARE_SYSCALLS_H
#define SYTARE_SYSCALLS_H

#include "utils/elfreader.h"

class Syscalls
{
    public:
        Syscalls();

        void print() const;

    private:
        elf_functions_t functions;
};

#endif

