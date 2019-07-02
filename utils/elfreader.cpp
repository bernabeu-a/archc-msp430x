#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <cstdint>
#include <cstdio>

#include "elfreader.h"

static void read_elf_entry(const std::string &s, uint16_t &address, std::string &name)
{
    std::string tmp;
    std::istringstream iss(s);
    iss >> tmp >> name;
    address = std::stol(tmp, nullptr, 16);
}

elf_functions_t read_functions_from_elf(const std::string &elf_filename, const elf_wl_functions_t &whitelist)
{
    elf_functions_t ret;
    std::ostringstream command;
    command << "readelf -Ws \"" << elf_filename << "\" | awk '$4==\"FUNC\" {print $2, $8;}'";

    FILE *fp = popen(command.str().c_str(), "r");
    if(fp != NULL)
    {
        char *line = NULL;
        size_t linesize = 0;
        while(getline(&line, &linesize, fp) >= 0)
        {
            uint16_t address;
            std::string name;
            read_elf_entry(line, address, name);

            if(whitelist.find(name) != whitelist.cend()) // Name is whitelisted
                ret.insert(std::make_pair(address, name));

            free(line);
            linesize = 0;
            line = NULL;
        }
        pclose(fp);
    }
    else
        std::cerr << "Cannot run command `" << command.str() << "`" << std::endl;
    return ret;
}

elf_functions_t read_functions_from_args(int , char **, const elf_wl_functions_t &whitelist)
{
    extern char *appfilename;
    return read_functions_from_elf(appfilename, whitelist);
}

