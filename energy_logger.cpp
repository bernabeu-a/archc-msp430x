#include "energy_logger.h"

EnergyLogger::EnergyLogger(std::ostream &out):
    out(out),
    enabled(false)
{
}

void EnergyLogger::start(size_t cycles, current_t current_ua)
{
    if(enabled)
        std::cerr << "Oops EnergyLogger::start in wrong state" << std::endl;
    else
        log(cycles, current_ua, true);
}

void EnergyLogger::stop(size_t cycles, current_t current_ua)
{
    if(!enabled)
        std::cerr << "Oops EnergyLogger::stop in wrong state" << std::endl;
    log(cycles, current_ua, true);
    enabled = false;
}

void EnergyLogger::log(size_t cycles, current_t current_ua)
{
    if(enabled)
        log(cycles, current_ua, false);
}

void EnergyLogger::log(size_t cycles, current_t current_ua, bool first)
{
    if(first)
        std::cout << "> " << std::dec << cycles << ", " << current_ua << std::endl;
    else if(current_ua != former_current_point.current_ua)
    {
        std::cout << "> " << std::dec << former_current_point.cycles << ", " << former_current_point.current_ua << std::endl
                  << "> " << std::dec << cycles << ", " << current_ua << std::endl;
    }

    enabled = true;
    former_current_point.cycles = cycles;
    former_current_point.current_ua = current_ua;
}

