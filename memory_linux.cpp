#include "memory.h"
#include <cstring>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sstream>
#include <iostream>

#include "utils.h"


int memory:: unprotect(uint64_t address)
{
	long pagesize = sysconf(_SC_PAGESIZE);
	void *m_address = (void *)((long)address & ~(pagesize - 1));
	return mprotect(m_address, pagesize, PROT_WRITE | PROT_READ | PROT_EXEC);
}

std::vector<memory::MemPage> memory::get_pages(const std::string &name)
{
	std::vector<MemPage> pages;
	if (std::ifstream maps_f { "/proc/self/maps" })
	{
		std::string line;
		while (std::getline(maps_f, line))
		{
			std::stringstream ss(line);

			uintptr_t start, end, offset, dev_major, dev_minor, inode;
			char skip, r, w, x, c;
			std::string path_name;

			ss >> std::hex >> start >> skip >> end >>
				r >> w >> x >> c >>
				offset >> dev_major >>
				skip >> dev_minor >> 
				inode >> path_name;

			if (!name.empty() && path_name.find(name) == std::string::npos)
			{
				continue;
			}

			pages.emplace_back(start, end, r, w, x, c, offset, 0, path_name);
		}
	}
	return pages;
}

uintptr_t memory::query_memory(uint8_t *query, const char *mask, const std::string &area)
{
	uintptr_t query_size = strlen(mask);
	uintptr_t size = 0;

	for (auto &region : get_pages(area)) 
	{
		size = region.end - region.start;

		if (query_size > size || (uintptr_t(query) > region.start && uintptr_t(query) < region.end))
		{
			continue;
		}

		for (uintptr_t i = region.start; i < region.end-query_size; i++)
		{
			bool found = true;
			for (uintptr_t j = 0; j < query_size; j++)
			{
				if (*reinterpret_cast<uint8_t *>(i + j) != query[j] && mask[j] != '?')
				{
					found = false;
					break;
				}
			}

			if (found)
			{
				return i;
			}
		}
	}

	return 0ULL;
}

uintptr_t memory::find_pattern(const std::string &query, const std::string &segment)
{
	std::stringstream ss(query);
	std::string data{ };
	std::string mask{ };
	std::vector<uint8_t> bytes;

	while (std::getline(ss, data, ' ')) 
	{
		if (data.find('?') != std::string::npos) 
		{
			mask += "?";
			bytes.push_back(0);
		}
		else 
		{
			bytes.push_back(static_cast<uint8_t>(std::stoi(data, nullptr, 16)));
			mask += "x";
		}
	}
	return query_memory(&bytes.at(0), mask.c_str(), segment);
}
