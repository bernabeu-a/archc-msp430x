#ifndef ENERGY_LOGGER_H
#define ENERGY_LOGGER_H

#include <iostream>
#include <cstddef>

class EnergyLogger
{
    public:
        EnergyLogger(std::ostream &out);

        void start(size_t cycles, size_t current);
        void stop();

        void log(size_t cycles, size_t current);

    private:
        void log(size_t cycles, size_t current, bool first);

        std::ostream &out;
        bool enabled;

        struct
        {
            size_t cycles;
            size_t current;
        } former_current_point;
};

#endif

