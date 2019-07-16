#ifndef ENERGY_MANAGER_H
#define ENERGY_MANAGER_H

#include <cstddef>

#include "energy_logger.h"
#include "sytare-syscalls/peripherals.h"

class EnergyManager
{
    public:
        EnergyManager(EnergyLogger &logger, const platform_t &platform);

        void add_cycles(size_t amount);
        void transaction(size_t tcycles, size_t tenergy);

        void start_log();
        void stop_log();
        void log() const;

    private:
        size_t cycles;

        EnergyLogger &logger;
        const platform_t &platform;
};

#endif

