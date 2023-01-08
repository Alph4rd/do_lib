#ifndef FLASH_STUFF_H
#define FLASH_STUFF_H
#include <string>

#include "avm.h"

class flash_stuff
{
public:

	static bool install();
	static void uninstall();

    static bool					hasproperty(avm::ScriptObject *obj, const std::string &prop_name);
    static uintptr_t 			getproperty(Atom obj, avm::Multiname *mm, avm::VTable *vtable);
    static void		 			setproperty(avm::ScriptObject *obj, avm::Multiname *mm, Atom value);
    static uintptr_t 			gettraitsbinding(avm::Traits *traits);
    static uintptr_t 			newarray(avm::MethodEnv *, uint32_t, void *);
    static avm::String			*newstring(avm::AvmCore *, const std::string &s);
    static avm::ScriptObject	*finddef(avm::MethodEnv *, avm::Multiname *);
};

#endif // FLASH_STUFF_H
