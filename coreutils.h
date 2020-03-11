#ifndef COREUTILS_H
#define COREUTILS_H

#include <cstddef>
#include <functional>

#include "msp430x_arch.H"

typedef std::function<void(size_t)> interrupt_handler_t;

#endif

