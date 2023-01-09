#ifndef UTILS_H
#define UTILS_H
#include <cmath>

#include <iostream>
#include <fstream>
#include <string>

#define LOG_F(msg, ...) utils::log("[%s] " msg, __func__, __VA_ARGS__);

#define LOG_FILE "/tmp/do_output.txt"

#define DEBUG 1

namespace utils
{
    static inline void log(std::ofstream &of, const char *data)
    {
        of << data;
    }

    static inline void log(const char *data)
    {
#ifdef DEBUG
        std::ofstream fhandle { LOG_FILE, std::ios::app};
        log(fhandle, data);
#endif
    }

    template <typename T, typename ... Args>
    static void log(std::ofstream &of, const char *s, T value, Args ... args)
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
                    {
                        continue;
                    }
                    else if (*s == '}' && *(s-1) != '\\')
                    {
                        if (key == 'x')
                            of << std::hex;
                        else
                            of << std::dec;
                        of << value;
                        log(of, s+1, args...);
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
    static inline void log(const char *s, T value, Args ... args)
    {
#ifdef DEBUG
        std::ofstream fhandle { LOG_FILE, std::ios::app};
        log(fhandle, s, value, args...);
#endif
    }

    template <typename T, typename ... Args>
    static inline void log(const std::string &s, T value, Args ... args)
    {
        log(s.c_str(), value, args...);
    }


class vec2
{
public:
    vec2() : x(0), y(0) { }
    vec2(int n) : x(n), y(n) { }
    vec2(int x, int y) : x(x), y(y) { }
    vec2(float n) : x(n), y(n) { }
    vec2(float x, float y) : x(x), y(y) { }
    vec2(double x, double y) : x(x), y(y) { }
    vec2(const vec2 &p) : x(p.x), y(p.y) { }

    float distance(float x, float y) const
    {
        return sqrt(pow(this->x - x, 2) + pow(this->y - y, 2));
    }

    float distance(int x, int y) const
    {
        return distance(static_cast<float>(x), static_cast<float>(y));
    }

    float distance(const vec2 &other) const
    {
        return distance(other.x, other.y);
    }

    vec2 MapTo(float mx, float my) const
    {
        return vec2(x * mx, y * my);
    }

    vec2 operator+=(const vec2 &rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    vec2 operator+=(const float &rhs)
    {
        x += rhs;
        y += rhs;
        return *this;
    }
    vec2 operator+=(const int &rhs) { return *this += static_cast<float>(rhs); }

    vec2 operator-=(const vec2 &rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    vec2 operator-=(const float &rhs)
    {
        x -= rhs;
        y -= rhs;
        return *this;
    }
    vec2 operator-=(const int &rhs) { return *this -= static_cast<float>(rhs); }

    vec2 operator*=(const vec2 &rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }
    vec2 operator*=(const float &rhs)
    {
        x *= rhs;
        y *= rhs;
        return *this;
    }
    vec2 operator*=(const int &rhs) { return *this *= static_cast<float>(rhs); }

    vec2 operator/=(const vec2 &rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }
    vec2 operator/=(const float &rhs)
    {
        x /= rhs;
        y /= rhs;
        return *this;
    }
    vec2 operator/=(const int &rhs) { return *this /= static_cast<float>(rhs); }
    
    // from cpprefeerence
    // friends defined inside class body are inline and are hidden from non-ADL lookup
    // passing lhs by value helps optimize chained a+b+c
    // otherwise, both parameters may be const references
    friend vec2 operator+(vec2 lhs, const vec2 &rhs)
    {
        lhs += rhs;
        return lhs;
    }

    friend vec2 operator+(vec2 lhs, const float &rhs)
    {
        lhs += rhs;
        return lhs;
    }

    friend vec2 operator+(vec2 lhs, const int &rhs) { return lhs + static_cast<float>(rhs); }

    friend vec2 operator-(vec2 lhs, const vec2 &rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    friend vec2 operator-(vec2 lhs, const float &rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    friend vec2 operator-(vec2 lhs, const int &rhs) { return lhs - static_cast<float>(rhs); }

    friend vec2 operator*(vec2 lhs, const vec2 &rhs)
    {
        lhs *= rhs;
        return lhs;
    }

    friend vec2 operator*(vec2 lhs, const float &rhs)
    {
        lhs *= rhs;
        return lhs;
    }

    friend vec2 operator*(vec2 lhs, const int &rhs) { return lhs * static_cast<float>(rhs); }

    friend vec2 operator/(vec2 lhs, const vec2 &rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    friend vec2 operator/(vec2 lhs, const float &rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    friend vec2 operator/(vec2 lhs, const int &rhs) { return lhs / static_cast<float>(rhs); }

    bool operator==(const vec2 &other) const
    {
        return(x == other.x && y == other.y);
    }

    bool operator!=(const vec2 &other)
    {
        return !(*this == other);
    }

    vec2 &operator=(const vec2 &other)
    {
        if (this != &other) {
            x = other.x;
            y = other.y;
        }
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const utils::vec2& vec)
    {
        os << std::fixed << "vec2(" << vec.x << ", " << vec.y << ") ";
        return os;
    }


    float x, y;
};

};

#endif /* UTILS_H */
