#ifndef UTILS_H
#define UTILS_H
#include <cmath>

#include <iostream>
#include <sstream>
#include <string>

#define LOG_F(msg, ...) utils::format("[%s] " msg, __func__, __VA_ARGS__);

namespace utils
{
    static inline void format(std::stringstream &of, const char *data)
    {
        of << data;
    }

    static inline std::string format(const char *data)
    {
        std::stringstream ss;
        format(ss, data);
        return ss.str();
    }

    template <typename T, typename ... Args>
    static void format(std::stringstream &of, const char *s, T value, Args ... args)
    {
        const char *start = s;
        for (; *s != 0; s++)
        {
            if (*s == '{' && (s == start || *(s-1) != '\\'))
            {
                char key = '\x00';
                for (s++; *s != 0; s++)
                {
                    if (*s == ' ')
                        continue;
                    else if (*s == '}' && *(s-1) != '\\')
                    {
                        if (key == 'x')
                            of << std::hex;
                        else
                            of << std::dec;
                        of << value;
                        format(of, s+1, args...);
                        return;
                    }
                    else if(key)
                    {
                        key = '\x00';
                        break;
                    }
                    else
                        key = *s;
                }
            }
            of << *s;
        }
    }

    template <typename T, typename ... Args>
    static inline std::string format(const char *s, T value, Args ... args)
    {
        std::stringstream ss;
        format(ss, s, value, args...);
        return ss.str();
    }

    template <typename T, typename ... Args>
    static inline std::string format(const std::string &s, T value, Args ... args)
    {
        return format(s.c_str(), value, args...);
    }
};

#endif /* UTILS_H */
