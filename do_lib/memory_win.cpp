#include "memory.h"
#include <sstream>

#include <Windows.h>
#include <Psapi.h>
#include <memoryapi.h>


static uintptr_t module_base = 0;
static uintptr_t module_size = 0;

void memory::set_module_info(uintptr_t b, uintptr_t s)
{
    module_base = b;
    module_size = s;
}

int memory::make_writable(uint64_t address)
{
    return 0;
}

uintptr_t pattern_match(uint8_t *query, uintptr_t query_size, const char *mask, uint8_t *data, uintptr_t data_size)
{
    for (uintptr_t i = 0; i < data_size - query_size; i++)
    {
        bool found = true;
        for (int j = 0; j < query_size; j++)
        {
            if (data[i + j] != query[j] && mask[j] != '?')
            {
                found = false;
                break;
            }
        }
        if (found)
            return reinterpret_cast<uintptr_t>(data) + i;
    }

    return 0ULL;
}

uintptr_t memory::query_memory(uint8_t *query, const char *mask, const std::string &area)
{
    uintptr_t begin = 0;
    uintptr_t size = -1;

    MEMORY_BASIC_INFORMATION mbi { 0 };
    auto pattern_size = static_cast<uintptr_t>(strlen(mask));

    if (!area.empty())
    {
        HMODULE module = GetModuleHandleA(area.c_str());
        MODULEINFO modinfo { 0 };

        if (module == 0)
        {
            printf("[!] Module %s not found\n", area.c_str());
            return 0;
        }

        GetModuleInformation(GetCurrentProcess(), module, &modinfo, sizeof(MODULEINFO));

        begin = reinterpret_cast<uintptr_t>(modinfo.lpBaseOfDll);
        size = modinfo.SizeOfImage;
    }


    MEMORY_BASIC_INFORMATION this_mbi;
    VirtualQuery(reinterpret_cast<LPCVOID>(&query_memory), &this_mbi, sizeof(this_mbi));

    for (uintptr_t address = begin;  address < begin+size; address += mbi.RegionSize)
    {
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)))
            continue;
        if (pattern_size > mbi.RegionSize || (mbi.Protect & PAGE_NOACCESS) || mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) || (mbi.State & PAGE_READONLY))
            continue;

        auto r = pattern_match(query, pattern_size, mask, reinterpret_cast<uint8_t *>(address), mbi.RegionSize);
        if (r && r != reinterpret_cast<uintptr_t>(query))
            return r;
    }

    return 0ULL;
}

uintptr_t memory::find_pattern(const std::string &query, const std::string &segment)
{
    std::stringstream ss(query);
    std::string data{ };
    std::string mask{ };
    std::vector<uint8_t> bytes;

    while (std::getline(ss, data, ' ')) {
        if (data.find('?') != std::string::npos) {
            mask += "?";
            bytes.push_back(0);
        } else {
            bytes.push_back(static_cast<uint8_t>(std::stoi(data, nullptr, 16)));
            mask += "x";
        }
    }

    return query_memory(&bytes.at(0), mask.c_str(), segment);
}

std::vector<memory::mempage> memory::getpages(const std::string &name)
{
    return {};
}
