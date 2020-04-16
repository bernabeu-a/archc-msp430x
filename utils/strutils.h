#ifndef STRUTILS_H
#define STRUTILS_H

#include <vector>
#include <string>

std::vector<std::string> str_split(const std::string &s, char c);
float str2float(const std::string &s);
size_t str2uint(const std::string &s);
size_t str2hex_uint(const std::string &s);

#endif

