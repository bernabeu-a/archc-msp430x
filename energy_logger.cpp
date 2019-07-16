#include "energy_logger.h"

EnergyLogger::EnergyLogger(std::ostream &out):
    out(out),
    former_current_point({.measuring = false})
{
}

void EnergyLogger::start(size_t cycles, size_t current)
{
    if(former_current_point.measuring)
        std::cerr << "Oops EnergyLogger::start in wrong state" << std::endl;
    else
        log(cycles, current, true);
}

void EnergyLogger::stop()
{
    if(!former_current_point.measuring)
        std::cerr << "Oops EnergyLogger::stop in wrong state" << std::endl;
    former_current_point.measuring = false;
}

void EnergyLogger::log(size_t cycles, size_t current)
{
    if(former_current_point.measuring)
        log(cycles, current, false);
}

void EnergyLogger::log(size_t cycles, size_t current, bool first)
{
    if(first)
        std::cout << "> " << std::dec << cycles << ", " << current << std::endl;
    else if(current != former_current_point.current)
    {
        std::cout << "> " << std::dec << former_current_point.cycles << ", " << former_current_point.current << std::endl
                  << "> " << std::dec << cycles << ", " << current << std::endl;
    }

    former_current_point.measuring = true;
    former_current_point.cycles = cycles;
    former_current_point.current = current;
}

