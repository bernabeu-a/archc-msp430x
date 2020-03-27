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
    std::string fullname;
    std::istringstream iss(s);
    iss >> tmp >> fullname;
    address = std::stol(tmp, nullptr, 16);

    // Remove all suffixes (starting with '.' character)
    size_t index = fullname.find('.');
    if(index == std::string::npos)
        name = fullname;
    else
        name = fullname.substr(0, index);
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

void locate_text_from_elf(const std::string &elf_filename, uint16_t &address_begin, size_t &size)
{
    std::ostringstream command;
    command << "readelf -S \"" << elf_filename << "\" | awk '$3==\".text\" {print $5, $7}'";

    FILE *fp = popen(command.str().c_str(), "r");
    if(fp != NULL)
    {
        char *line = NULL;
        size_t linesize = 0;
        if(getline(&line, &linesize, fp) >= 0)
        {
            std::string s_address_begin;
            std::string s_size;

            std::istringstream iss(line);
            iss >> s_address_begin >> s_size;
            address_begin = std::stol(s_address_begin, nullptr, 16);
            size = std::stol(s_size, nullptr, 16);

            free(line);
        }
        pclose(fp);
    }
    else
        std::cerr << "Cannot run command `" << command.str() << "`" << std::endl;
}

void locate_text_from_args(int, char **, uint16_t &address_begin, size_t &size)
{
    extern char *appfilename;
    return locate_text_from_elf(appfilename, address_begin, size);
}
