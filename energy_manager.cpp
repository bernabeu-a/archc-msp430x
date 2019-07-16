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

void EnergyManager::transaction(size_t tcycles, size_t tenergy)
{
    const float VCC = 3.3;
    const float CLOCK_FREQ = 24e6;

    float inv_dt = CLOCK_FREQ / tcycles;
    float additional_current = tenergy * inv_dt / VCC;

    // TODO: inject current value

    add_cycles(tcycles);
}

void EnergyManager::start_log()
{
    logger.start(cycles, platform.current());
}

void EnergyManager::stop_log()
{
    logger.stop();
}

void EnergyManager::log() const
{
    logger.log(cycles, platform.current());
}

