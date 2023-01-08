#ifndef MEMORY_H
#define MEMORY_H
#include <string>
#include <cstdint>
#include <vector>

namespace memory
{
    struct MemPage
	{
		MemPage(uintptr_t s,uintptr_t e, 
				char r, char w, char x, char c,
				uintptr_t offset, uintptr_t size,
				const std::string &name) :
			start(s), end(e),
			read(r), write(w), exec(x), cow(c),
			offset(offset), size(size),
			name(name)
		{
		}

		uintptr_t start, end;
		char read, write, exec, cow;
		uintptr_t offset;
		uintptr_t size;
		std::string name;
	};


	int unprotect(uint64_t address);

	uintptr_t query_memory(uint8_t *query, const char *mask, const std::string &area = "");

	inline uintptr_t query_memory(uint8_t *query, unsigned int len)
    {
		std::string mask(len, 'x');
        return query_memory(query, mask.c_str());
    }

	uintptr_t find_pattern(const std::string &query, const std::string &segment);

	std::vector<MemPage> get_pages(const std::string &name = "");

	template<typename T>
	constexpr T read(uintptr_t addr)
	{
		return *reinterpret_cast<T *>(addr);
	}

	template <typename T, typename ... Offsets >
	constexpr T read(uintptr_t address, uintptr_t ofs, Offsets ... offsets)
	{
		return read<T>(*reinterpret_cast<uintptr_t *>(address) + ofs, offsets...);
	}

	template <typename T>
	constexpr void write(uintptr_t address, T value)
	{
		*reinterpret_cast<T*>(address) = value;
	}

	template <typename T, typename ... Offsets >
	constexpr T write(uintptr_t address, T value, uintptr_t ofs, Offsets ... offsets)
	{
		return write<T>(*reinterpret_cast<uintptr_t *>(address) + ofs, value, offsets...);
	}

};

#endif // MEMORY_H
