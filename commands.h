#ifndef COMMANDS_H
#define COMMANDS_H

#include <cstdint>

#include "energy_manager.h"

class Commands
{
    public:
        Commands(EnergyManager &manager);

        bool run(uint16_t address);

        static bool is_command(uint16_t address);

    private:
        EnergyManager &manager;
};

#endif

