#include "flash_stuff.h"
#include <subhook/subhook.h>

#include "memory.h"
#include "utils.h"
#include "darkorbit.h"

namespace flash_offsets
{
    std::ptrdiff_t verifyjit            = 0x2fb430;
    std::ptrdiff_t free_chunk           = 0xb8a100;
    std::ptrdiff_t getproperty          = 0x326c30;
    std::ptrdiff_t setproperty          = 0x326a40;
    std::ptrdiff_t get_traits_binding   = 0x32b4c0;
    std::ptrdiff_t newarray             = 0x2def80;
    std::ptrdiff_t newstring            = 0x322f00;
    std::ptrdiff_t finddef              = 0x30fe50;
};



typedef uintptr_t (*getproperty_t)(uintptr_t, avm::Multiname *, avm::VTable *);
getproperty_t getproperty_f = nullptr;

typedef void (*setproperty_t)(avm::Toplevel *, Atom, avm::Multiname *, Atom , avm::VTable *);
setproperty_t  setproperty_f = nullptr;

typedef uintptr_t (*get_traits_binding_t)(avm::Traits *);
get_traits_binding_t get_traits_binding_f = nullptr;

typedef uintptr_t (*newarray_t)(avm::MethodEnv *, uint32_t, void *);
newarray_t newarray_f = nullptr;

typedef void (*verifyMethod_t)(avm::MethodInfo* m, avm::Toplevel *toplevel, avm::AbcEnv* abc_env);
verifyMethod_t verify_method = nullptr;

typedef avm::String *(*newstring_t)(avm::AvmCore *core, const char *, int32_t , int32_t , bool, bool);
newstring_t newstring_f = nullptr;

typedef avm::ScriptObject *(*finddef_t)(avm::MethodEnv *, avm::Multiname *);
finddef_t finddef_f = nullptr;



subhook::Hook *free_chunk_hook = nullptr;
subhook::Hook *verify_jit_hook = nullptr;

void verify_jit(uintptr_t _this, avm::MethodInfo *method, uintptr_t ms, uintptr_t toplevel, avm::AbcEnv *abc_env, uintptr_t osr)
{
    subhook::ScopedHookRemove hk(verify_jit_hook);

    reinterpret_cast<decltype(verify_jit) *>(verify_jit_hook->GetSrc())(_this, method, ms, toplevel, abc_env, osr);

    Darkorbit::get().notify_jit(method);
}

void free_chunk(uintptr_t _this, uintptr_t chunk)
{
    subhook::ScopedHookRemove hk(free_chunk_hook);
    Darkorbit::get().notify_freechunk(chunk);
    reinterpret_cast<decltype(free_chunk) *>(free_chunk_hook->GetSrc())(_this, chunk);
}


bool flash_stuff::hasproperty(avm::ScriptObject *obj, const std::string &prop_name)
{
    return obj->vtable->traits->parse_traits().has_trait(prop_name);
}

uintptr_t flash_stuff::getproperty(uintptr_t obj, avm::Multiname *mm, avm::VTable *vtable)
{
    return getproperty_f(obj, mm, vtable);
}

void flash_stuff::setproperty(avm::ScriptObject *obj, avm::Multiname *mn, Atom value)
{
    return setproperty_f(obj->vtable->toplevel, reinterpret_cast<Atom>(obj), mn, value, obj->vtable);
}

uintptr_t flash_stuff::gettraitsbinding(avm::Traits *traits)
{
    return get_traits_binding_f(traits);
}

uintptr_t flash_stuff::newarray(avm::MethodEnv *env, uint32_t argc, void *argv)
{
    return newarray_f(env, argc, argv);
}

avm::String *flash_stuff::newstring(avm::AvmCore *core, const std::string &s)
{
    return newstring_f(core, s.data(), -1, 0, 0, 0);
}

avm::ScriptObject *flash_stuff::finddef(avm::MethodEnv *env, avm::Multiname *mn)
{
    return finddef_f(env, mn);
}

bool flash_stuff::install()
{
    uintptr_t base = 0;
    try
    {
        base = memory::get_pages("libpepflashplayer").at(0).start;
    }
    catch (...)
    {
        utils::log("[!] Failed to find flash lib");
        return false;
    }

    verify_jit_hook = new subhook::Hook(
                reinterpret_cast<void *>(base + flash_offsets::verifyjit),
                reinterpret_cast<void *>(verify_jit),
                subhook::HookFlags::HookFlag64BitOffset);

    verify_jit_hook->Install();

    free_chunk_hook = new subhook::Hook(
                reinterpret_cast<void *>(base + flash_offsets::free_chunk),
                reinterpret_cast<void *>(free_chunk),
                subhook::HookFlags::HookFlag64BitOffset);

    free_chunk_hook->Install();

    getproperty_f           = reinterpret_cast<getproperty_t>(base + flash_offsets::getproperty);
    setproperty_f           = reinterpret_cast<setproperty_t>(base + flash_offsets::setproperty);
    get_traits_binding_f    = reinterpret_cast<get_traits_binding_t>(base + flash_offsets::get_traits_binding);
    newarray_f              = reinterpret_cast<newarray_t>(base + flash_offsets::newarray);
    newstring_f             = reinterpret_cast<newstring_t>(base + flash_offsets::newstring);
    finddef_f               = reinterpret_cast<finddef_t>(base + flash_offsets::finddef);

    return true;
}

void flash_stuff::uninstall()
{
    if (verify_jit_hook && verify_jit_hook->IsInstalled())
    {
        verify_jit_hook->Remove();
    }

    if (free_chunk_hook && free_chunk_hook->IsInstalled())
    {
        free_chunk_hook->Remove();
    }
}
