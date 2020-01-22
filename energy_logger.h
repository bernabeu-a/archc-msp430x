#ifndef ENERGY_LOGGER_H
#define ENERGY_LOGGER_H

#include <iostream>
#include <cstddef>

#include "sytare-syscalls/peripherals.h"

class EnergyLogger
{
    public:
        EnergyLogger(std::ostream &out);

        void start(size_t cycles, current_t current_ua);
        void stop(size_t cycles, current_t current_ua);

        void log(size_t cycles, current_t current_ua);

    private:
        void log(size_t cycles, current_t current_ua, bool first);

        std::ostream &out;
        bool enabled;

        struct
        {
            size_t cycles;
            current_t current_ua;
        } former_current_point;
};

#endif

