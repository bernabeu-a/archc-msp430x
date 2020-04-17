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
    double energy_nj = -1e-3 * amount * supply.vcc() * (platform.current_ua() - current_to_subtract_ua) / CLOCK_FREQ_MHZ;
    supply.add_energy(energy_nj);

    tmp_energy_nj += -energy_nj;
}

void EnergyManager::transaction(duration_t duration_us, energy_t energy_nj, current_t current_to_subtract_ua)
{
    size_t current_ua;
    if(duration_us > 0)
        current_ua = energy_nj * 1e3 / (supply.vcc() * duration_us);
    else if(energy_nj == 0)
        current_ua = 0;
    else
        std::cerr << "Oops EnergyManager::transaction: duration_us == 0 && energy_nj != 0" << std::endl;
    supply.add_energy(-energy_nj);

    log(); // Log before
    log(current_ua); // Beginning of the transaction
    add_cycles(CLOCK_FREQ_MHZ * duration_us, current_to_subtract_ua); // Forward
    log(current_ua); // End of the transaction
    log(); // Log after

    tmp_energy_nj += energy_nj;
}

void EnergyManager::start_log()
{
    logger.start(cycles, platform.current_ua());
    tmp_energy_nj = 0;
}

void EnergyManager::stop_log()
{
    logger.stop(cycles, platform.current_ua());
    size_t e_nj = tmp_energy_nj;
    std::cout << "energy_nj = " << std::dec << e_nj << std::endl;
}

void EnergyManager::log(current_t temporary_current_ua) 
{
    logger.log(cycles, platform.current_ua() + temporary_current_ua);
}

void EnergyManager::force_reboot()
{
    supply.force_reboot();
}

