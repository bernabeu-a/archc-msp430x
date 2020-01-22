#include "energy_manager.h"

const size_t CLOCK_FREQ_MHZ = 24; // MHz

EnergyManager::EnergyManager(EnergyLogger &logger, PowerSupply &supply, const platform_t &platform):
    cycles(0),
    logger(logger),
    supply(supply),
    platform(platform)
{
}

void EnergyManager::add_cycles(size_t amount, current_t current_to_subtract_ua)
{
    cycles += amount;
    energy_t energy_nj = -1e-3 * amount * supply.vcc() * (platform.current_ua() - current_to_subtract_ua) / CLOCK_FREQ_MHZ;
    supply.add_energy(energy_nj);
}

void EnergyManager::transaction(duration_t duration_us, energy_t energy_nj, current_t current_to_subtract_ua)
{
    size_t current_ua = energy_nj * 1e3 / (supply.vcc() * duration_us);
    supply.add_energy(-energy_nj);

    log(); // Log before
    log(current_ua); // Beginning of the transaction
    add_cycles(CLOCK_FREQ_MHZ * duration_us, current_to_subtract_ua); // Forward
    log(current_ua); // End of the transaction
    log(); // Log after
}

void EnergyManager::start_log()
{
    logger.start(cycles, platform.current_ua());
}

void EnergyManager::stop_log()
{
    logger.stop(cycles, platform.current_ua());
}

void EnergyManager::log(current_t temporary_current_ua) 
{
    logger.log(cycles, platform.current_ua() + temporary_current_ua);
}

