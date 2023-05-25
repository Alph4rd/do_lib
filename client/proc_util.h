#ifndef PROC_UTIL_H
#define PROC_UTIL_H

#include <cstdint>
#include <string>
#include <vector>

namespace ProcUtil
{
    struct MemPage
    {
        MemPage(
                uintptr_t s, uintptr_t e,
                char r, char w, char x, char c,
                uint32_t ofs, uintptr_t size, const std::string name) :
            start(s), end(e),
            read(r), write(w), exec(x), cow(c),
            offset(ofs), size(size),
            name(name)
        {
        }

        uintptr_t start, end;
        char read, write, exec, cow;
        uint32_t offset;
        uintptr_t size;
        std::string name;
    };

    bool IsChildOf(pid_t child_pid, pid_t test_parent);

    std::vector<int> FindProcsByName(const std::string &n);

    bool ProcessExists(pid_t pid);

    size_t ReadMemoryBytes(pid_t pid, uintptr_t address, void *dest, uint64_t size);
    size_t WriteMemoryBytes(pid_t pid, uintptr_t address, void *dest, uint64_t size);

    pid_t GetParent(pid_t pid);

    uintptr_t FindPattern(pid_t pid, const std::string &query, const std::string &segment);

    int QueryMemory(pid_t pid, uint8_t *query, const char *mask, uintptr_t *out, uint32_t amount);

    std::vector<MemPage> GetPages(pid_t pid, const std::string &name = "");

    uint64_t GetMemoryUsage(pid_t pid);

    class Process
    {
    public:
        Process(pid_t pid) : pid(pid) { }

        template <typename T>
        T Read(uintptr_t address, int *result = nullptr)
        {
            T r;
            int ok = ReadMemoryBytes(pid, address, &r, sizeof(T));

            if (result) *result = ok;

            return r;
        }

        template <typename T>
        void Write(uintptr_t address, T value, int *result = nullptr)
        {
            T r;
            int ok = WriteMemoryBytes(pid, address, &value, sizeof(T));
            if (result) *result = ok;
        }

        pid_t pid = 0;
    };
};

#endif /* UNIX_TOOLS_H */
