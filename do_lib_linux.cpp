#include <dlfcn.h>

#include "utils.h"
#include "flash_hooks.h"
#include "darkorbit.h"
#include "memory.h"

bool is_flash_process =false;

void *dlopen(const char *filename, int flags)
{
	auto *original = reinterpret_cast<decltype(dlopen) *>(dlsym(RTLD_NEXT, "dlopen"));

	auto *r = (*original)(filename, flags);

    // Install flash hooks
    if (filename && std::string(filename).find("libpepflashplayer.so") != std::string::npos)
    {
        is_flash_process = true;
        if (!flash_stuff::install())
        {
            utils::log("[!] Failed to install flash hooks\n");
        }
    }

    return r;
}

int __attribute__((constructor)) lib_ctor ()
{
    utils::log("[+] Loaded!\n");
    return 0;
}

int __attribute__((destructor)) lib_dtor()
{
	utils::log("[+] Unloading!\n");
    Darkorbit::get().uninstall();
    flash_stuff::uninstall();
    return 0;
}
