#include <iostream>

#include "commands.h"

Commands::Commands(EnergyManager &manager):
    manager(manager)
{
}

bool Commands::run(uint16_t address)
{
    if(!Commands::is_command(address))
        return false;

    switch(address)
    {
        case 0: // WRAPPER_ENTRY_BEGIN
            std::cout << "BEGIN(WRAPPER_ENTRY)" << std::endl;
            manager.start_log();
            break;
        case 1: // WRAPPER_ENTRY_END
            std::cout << "END(WRAPPER_ENTRY)" << std::endl;
            manager.stop_log();
            break;
        case 2: // WRAPPER_EXIT_BEGIN
            std::cout << "BEGIN(WRAPPER_EXIT)" << std::endl;
            manager.start_log();
            break;
        case 3: // WRAPPER_EXIT_END
            std::cout << "END(WRAPPER_EXIT)" << std::endl;
            manager.stop_log();
            break;
        case 4: // ORACLE_SAYS_SUFFICIENT
            std::cout << "ORACLE_SAYS_SUFFICIENT" << std::endl;
            break;
        case 5: // ORACLE_SAYS_INSUFFICIENT
            std::cout << "ORACLE_SAYS_INSUFFICIENT" << std::endl;
            break;

        default:
            return false;
    }

    return true;
}

bool Commands::is_command(uint16_t address)
{
    return address < 6;
}

