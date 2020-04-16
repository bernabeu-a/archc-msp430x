#include <vector>
#include <string>
#include <sstream>

std::vector<std::string> str_split(const std::string &s, char c)
{
    std::vector<std::string> ret;
    size_t offset = 0;
    size_t to;
    while((to = s.find(c, offset)) != std::string::npos)
    {
        std::string tmp = s.substr(offset, to-offset);
        ret.push_back(tmp);
        offset = to+1;
    }
    ret.push_back(s.substr(offset));
    return ret;
}

float str2float(const std::string &s)
{
    float tmp;
    std::istringstream stream(s);
    stream >> tmp;
    return tmp;
}

size_t str2uint(const std::string &s)
{
    size_t tmp;
    std::istringstream stream(s);
    stream >> tmp;
    return tmp;
}

size_t str2hex_uint(const std::string &s)
{
    size_t tmp;
    std::istringstream stream(s);
    stream >> std::hex >> tmp;
    return tmp;
}

