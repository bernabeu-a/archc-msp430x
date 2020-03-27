#ifndef ELFREADER_H
#define ELFREADER_H

#include <cstdint>
#include <string>
#include <set>
#include <map>

typedef std::set<std::string> elf_wl_functions_t;
typedef std::map<uint16_t, std::string> elf_functions_t;

elf_functions_t read_functions_from_elf(const std::string &elf_filename, const elf_wl_functions_t &whitelist);
elf_functions_t read_functions_from_args(int argc, char **argv, const elf_wl_functions_t &whitelist);

void locate_text_from_elf(const std::string &filename, uint16_t &address_begin, size_t &size);
void locate_text_from_args(int argc, char **argv, uint16_t &address_begin, size_t &size);

#endif

