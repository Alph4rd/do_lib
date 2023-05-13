#ifndef OFFSETS_H
#define OFFSETS_H
#include <cstdint>

namespace offsets
{
    static const std::ptrdiff_t verifyjit            = 0x2fb430;
    static const std::ptrdiff_t free_chunk           = 0xb8a100;
    static const std::ptrdiff_t getproperty          = 0x326c30;
    static const std::ptrdiff_t setproperty          = 0x326a40;
    static const std::ptrdiff_t get_traits_binding   = 0x32b4c0;
    static const std::ptrdiff_t newarray             = 0x2def80;
    static const std::ptrdiff_t newstring            = 0x322f00;
    static const std::ptrdiff_t finddef              = 0x30fe50;
    static const std::ptrdiff_t input_thing_vt       = 0x131b710;
    static const std::ptrdiff_t mouse_release        = 0x72fe90;
    static const std::ptrdiff_t mouse_press          = 0x7306e0;
}

#endif // OFFSETS_H
