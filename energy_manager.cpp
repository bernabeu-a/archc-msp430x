#include "energy_manager.h"

const size_t CLOCK_FREQ = 24; // MHz

EnergyManager::EnergyManager(EnergyLogger &logger, PowerSupply &supply, const platform_t &platform):
    cycles(0),
    logger(logger),
    supply(supply),
    platform(platform)
{
}

void EnergyManager::add_cycles(size_t amount)
{
    cycles += amount;
    supply.add_energy(-1e-6 * amount * supply.vcc() * platform.current() / CLOCK_FREQ);
}

void EnergyManager::transaction(size_t duration, size_t energy)
{
    size_t current = energy*1e6 / (supply.vcc() * duration);
    supply.add_energy(-1. * energy);

    log(); // Log before
    log(current); // Beginning of the transaction
    add_cycles(CLOCK_FREQ * duration); // Forward
    log(current); // End of the transaction
    log(); // Log after
}

void EnergyManager::start_log()
{
    logger.start(cycles, platform.current());
}

void EnergyManager::stop_log()
{
    logger.stop(cycles, platform.current());
}

void EnergyManager::log(size_t temporary_current) 
{
    logger.log(cycles, platform.current() + temporary_current);
}

