#ifndef COREUTILS_H
#define COREUTILS_H

#include <cstddef>
#include <functional>

#include "msp430x_arch.H"

typedef std::function<void(
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>&,
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>&,
    size_t)>
interrupt_handler_t;

#endif

