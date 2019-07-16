#include "energy_manager.h"

EnergyManager::EnergyManager(EnergyLogger &logger, const platform_t &platform):
    cycles(0),
    logger(logger),
    platform(platform)
{
}

void EnergyManager::add_cycles(size_t amount)
{
    cycles += amount;
}

void EnergyManager::transaction(size_t duration, size_t energy)
{
    const float VCC = 3.3; // V
    const size_t CLOCK_FREQ = 24; // MHz

    std::cout << "transaction:" << std::endl
              << ' ' << std::dec << duration << " us" << std::endl
              << ' ' << energy << " uJ" << std::endl;

    log(); // Log before
    log(energy*1e6 / (VCC * duration)); // Beginning of the transaction
    add_cycles(CLOCK_FREQ * duration); // Forward
    log(energy*1e6 / (VCC * duration)); // End of the transaction
    log(); // Log after
}

void EnergyManager::start_log()
{
    logger.start(cycles, platform.current());
}

void EnergyManager::stop_log()
{
    logger.stop();
}

void EnergyManager::log(size_t temporary_current) 
{
    logger.log(cycles, platform.current() + temporary_current);
}

