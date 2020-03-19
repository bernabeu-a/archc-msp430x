#ifndef ENERGY_MANAGER_H
#define ENERGY_MANAGER_H

#include <cstddef>

#include "energy_logger.h"
#include "power_supply.h"
#include "sytare-syscalls/peripherals.h"

class EnergyManager
{
    public:
        EnergyManager(EnergyLogger &logger, PowerSupply &supply, const platform_t &platform);

        void add_cycles(size_t amount, current_t current_to_subtract_ua);
        void transaction(duration_t duration, energy_t energy_nj, current_t current_to_subtract_ua);

        void start_log();
        void stop_log();
        void log(current_t temporary_current_ua = 0);
        
        void force_reboot();

    private:
        size_t cycles;

        EnergyLogger &logger;
        PowerSupply &supply;
        const platform_t &platform;
};

#endif

