#include "darkorbit.h"
#include <string>
#include <iostream>
#include <sstream>

#include "disassembler.h"
#include "memory.h"
#include "offsets.h"


#define TAG_NUMBER(val) (uintptr_t(val) << 3) | 6


// Proxy flash calls to our handlers
uintptr_t hook_proxy(avm::MethodEnv *env, uint32_t argc, uintptr_t *argv)
{
    auto &hook = Darkorbit::get().get_hooks()[env->method_info->id];

    hook.method = env;

    // Restore invokers
    hook.restore();
    
    hook.handler(env, argc, argv);

    // Call original
    uintptr_t r = 0;
    Atom this_object = argv[0];
    if (!(this_object & 7))
    {
        r = env->method_info->method_proc(env, argc, argv);
    }
    else
    {
        r = env->method_info->invoker(env, argc, argv);
    }

    // Save potentially new invokers
    if (env->method_proc != hook.envproc)
    {
        hook.envproc = env->method_proc;
    }

    if (hook.infoproc != env->method_info->method_proc)
    {
        hook.infoproc = env->method_info->method_proc;
    }

    if (hook.invoker != env->method_info->invoker)
    {
        hook.invoker = env->method_info->invoker;
    }

    // Reinstall hook
    env->method_proc = hook_proxy;
    env->method_info->method_proc = hook_proxy;
    env->method_info->invoker = hook_proxy;

    return r;
}

void Darkorbit::hook_flash_function(avm::MethodEnv *method, MyInvoke_t handler)
{
    FlashHook hook;

    auto mit = m_hooks.find(method->method_info->id);
    if (mit != m_hooks.end())
    {
        mit->second.restore();
    }

    hook.envproc = method->method_proc;
    hook.infoproc = method->method_info->method_proc;
    hook.invoker = method->method_info->invoker;
    hook.handler = handler;
    hook.method = method;

    m_hooks[method->method_info->id] = hook;

    method->method_proc = hook_proxy;
    method->method_info->method_proc = hook_proxy;
    method->method_info->invoker = hook_proxy;

}
void Darkorbit::hook_flash_function(avm::MethodInfo *method, MyInvoke_t handler)
{
    FlashHook hook;

    auto mit = m_hooks.find(method->id);
    if (mit != m_hooks.end())
    {
        mit->second.restore();
    }

    hook.envproc = method->method_proc;
    hook.infoproc = method->method_proc;
    hook.invoker = method->invoker;
    hook.handler = handler;
    hook.method_info = method;

    m_hooks[method->id] = hook;

    method->method_proc = hook_proxy;
    method->invoker = hook_proxy;

}

// maybe use a global callback thingy to dispatch jit stuff
void Darkorbit::notify_jit(avm::MethodInfo *method)
{
    if (!m_installed && method->name().find("autoStartEnabled") != std::string::npos)
    {
        hook_flash_function(method, [this] (avm::MethodEnv *env, uint32_t argc, uintptr_t *argv)
        {
            install(avm::remove_kind(argv[0]));
            return 0L;
        });
    }
}

void Darkorbit::notify_freechunk(uintptr_t chunk)
{
    for (auto &[id, hook] : m_hooks)
    {
        if ((reinterpret_cast<uintptr_t>(hook.method) & ~0xfff) == chunk)
        {
            uninstall();
        }
    }
}

std::unordered_map<uint32_t, game::Ship *> Darkorbit::get_ships()
{
    std::unordered_map<uint32_t, game::Ship *> r;

    auto ships = m_screen_manager->get_at<uintptr_t>(0x100, 0x28);
    auto elements = memory::read<uintptr_t>(ships + 0x30);
    auto size = memory::read<uint32_t>(ships + 0x38);

    for (size_t i = 0; i < size; i++)
    {
        auto *ship = avm::remove_kind(memory::read<game::Ship *>(elements + 0x10 + i * 8));
        if (ship && flash_stuff::hasproperty(ship, "pet"))
        {
            r[ship->id] = ship;
        }
    }
    return r;
}

std::future<uintptr_t> Darkorbit::call_sync(const std::function<uintptr_t()> &f)
{
    std::scoped_lock lk { m_call_mut };
    return m_async_calls.emplace_back(f).get_future(); // c++ 17
}

void Darkorbit::handle_async_calls(avm::MethodEnv *env, uint32_t argc, uintptr_t *argv)
{
    std::scoped_lock lk { m_call_mut };
    for (auto &task : m_async_calls)
    {
        task();
    }
    m_async_calls.clear();
}

bool Darkorbit::mouse_click(int x, int y, int button)
{
    flash_stuff::mouse_press(x, y, button);
    flash_stuff::mouse_release(x, y, button);
    return true;
}

bool Darkorbit::key_click(uint32_t key)
{
    auto *kbmapper = m_event_manager->get_at<avm::ScriptObject *>(0x68);
    kbmapper->call(3, static_cast<Atom>(key));
    return true;
}

bool Darkorbit::lock_entity(uint32_t id)
{
    utils::log("[*] Trying to lock entity {}\n", id);
    auto facade = m_screen_manager->get_at<avm::ScriptObject *>(0x100, 0x70, 0x28);

    game::Ship *player = reinterpret_cast<game::Ship *>(avm::remove_kind(m_event_manager->call(7)));
    game::Ship *target_ship = get_ships()[id];

    if (target_ship && player)
    {
        std::array<uintptr_t, 8> array_args {
            TAG_NUMBER(target_ship->id),
            TAG_NUMBER(target_ship->position().x),
            TAG_NUMBER(target_ship->position().y),
            TAG_NUMBER(player->position().x),
            TAG_NUMBER(player->position().y),
            TAG_NUMBER(0),
            TAG_NUMBER(0),
            TAG_NUMBER(100 + rand() % 400), // radius
        };

        // Should call send_notification() but we leave it as is since it's not used anymore
        auto *arg_array = (avm::Array *)flash_stuff::newarray(facade->vtable->methods[0], array_args.size(), array_args.data());
        avm::String *notification = flash_stuff::newstring(facade->core(), "MapAssetNotificationTRY_TO_SELECT_MAPASSET");

        facade->call(8, notification, (uintptr_t)arg_array | 1);

        utils::log("[*] Locking {}", target_ship->name());
        return true;
    }
    return false;
}

bool Darkorbit::refine_ore(uint32_t ore, uint32_t amount)
{
    auto *refinement = m_gui_manager->get_at<avm::ScriptObject *>(0x78);

    if (refinement)
    {
        if (!m_refine_multiname)
        {
            auto disass = Disassembler::Disassemble(refinement->vtable->methods[20]->method_info);
            for (uint32_t &xref : disass.GetXrefs())
            {
                auto *mn = m_const_pool->get_multiname(xref);
                if (mn && mn->ns->get_uri().find(".com.module") != std::string::npos)
                {
                    m_refine_multiname = xref;
                    utils::log("[+] Found multiname: {}::{} ffs\n", mn->ns->get_uri(), mn->get_name());
                    break;
                }
            }
        }

        if (m_refine_multiname)
        {
            auto *obj = flash_stuff::finddef(m_main->vtable->einit, m_const_pool->get_multiname(m_refine_multiname));

            if (auto *closure = obj->get_at<avm::ClassClosure *>(0x20))
            {
                auto *instance = reinterpret_cast<avm::ScriptObject *>(closure->call(5));

                auto *ore_info = instance->get_at<avm::ScriptObject *>(0x20);
                auto *ore_type = ore_info->get_at<avm::ScriptObject *>(0x20);

                ore_info->write_at<double>(0x28, amount);
                ore_type->write_at<int>(0x20, ore);

                auto *net = m_main->get_at<avm::ScriptObject *>(0x230);

                net->call(18, instance);
            }
        }
    }

    return true;
}

bool Darkorbit::use_item(const std::string &name, uint8_t type, uint8_t bar)
{
    avm::String *name_str = create_string(name);

    avm::ScriptObject *instance = m_action_closure->construct();

    auto *net = m_main->get_at<avm::ScriptObject *>(0x230);


    if (!m_item_prop_mn)
    {
        auto traits = instance->vtable->traits->parse_traits();
        for (const auto &slot : traits.get_slots())
        {
            avm::Multiname *type_mn = m_const_pool->get_multiname(slot.type_id);
            if (type_mn && type_mn->get_name() == "String")
            {
                m_item_prop_mn = slot.name_index;
                break;
            }
        }
    }

    if (!m_item_prop_mn)
    {
        instance->set_at(name_str, 0x28);
    }
    else
    {
        auto *mn = m_const_pool->get_multiname(m_item_prop_mn);
        // Use setproperty to make sure gc knows we're moving this struct here
        flash_stuff::setproperty(instance, mn, reinterpret_cast<Atom>(name_str) | 2);
    }
    instance->set_at(type, 0x20);
    instance->set_at(bar, 0x24);

    net->call(19, instance);

    return false;
}

bool Darkorbit::send_notification(const std::string &name, std::vector<Atom> args)
{
    utils::log("[*] Send notification {}\n", name);

    auto facade = m_screen_manager->get_at<avm::ScriptObject *>(0x100, 0x70, 0x28);

    // no need to cache these, ref count is not increased
    auto *arg_array =
        reinterpret_cast<avm::Array *>(flash_stuff::newarray(facade->vtable->methods[0], args.size(), args.data()));
    avm::String *notification = create_string("MapAssetNotificationTRY_TO_SELECT_MAPASSET");

    facade->call(8, notification, (uintptr_t)arg_array | 1);

    return true;
}

int Darkorbit::check_method_signature(avm::ScriptObject *obj, int methodIdx, bool methodName, const std::string &signature)
{
    if (obj) {
        avm::MethodEnv *method = obj->vtable->methods[methodIdx];
        if (method && method->method_info) {
            std::string flashSignature = get_method_signature(method->method_info, methodName);

            utils::log("Signature: {} == {}\n", signature, flashSignature);
            return !flashSignature.empty() && flashSignature == signature;
        }
    }

    return -1;
}

std::string Darkorbit::get_method_signature(avm::MethodInfo *mi, bool method_name)
{
    std::stringstream ss;

    avm::MethodSignature *ms = flash_stuff::get_method_signature(mi);
    if (ms) {
        ss << get_builtin_type(ms->_returnTraits);

        if (method_name) {
            std::string mn = mi->name();
            if (mn.empty()) return "";

            ss << "(";
            auto index = mn.find('/');
            if (index != std::string::npos) {
                ss << mn.substr(index + 1);
            } else ss << mn;
            ss << ")";
        }

        ss << "(";
        for (int i = 0; i <= ms->param_count; i++) {
            auto bt = get_builtin_type(ms->paramTraits(i));

            ss << bt;
            if (i > ms->param_count - ms->optional_count) {
                ss << "?";
            }
        }

        ss << ")" << ms->param_count << ms->optional_count << ms->rest_offset << ms->max_stack
        << ms->local_count << ms->max_scope << ms->frame_size << ms->isNative << ms->allowExtraArgs;
    }

    return ss.str();
}

bool Darkorbit::install(uintptr_t main_app_address)
{
    mouse_click(0, 0, 1); // Mouse click to initialize the click param

    m_main            = memory::read<avm::ScriptObject *>(main_app_address + 0x540);
    m_screen_manager  = m_main->get_at<avm::ScriptObject *>(0x1f8);
    m_gui_manager     = m_main->get_at<avm::ScriptObject *>(0x200);
    m_event_manager   = m_screen_manager->get_at<avm::ScriptObject *>(0xc8);

    utils::log("[+] Main {x}\n", m_main);
    utils::log("[+] Screen {x}\n", m_screen_manager);
    utils::log("[+] Event {x}\n", m_event_manager);

    auto vtable          = m_main->get_at<uintptr_t>(0x10);
    auto vtable_init     = memory::read<uintptr_t>(vtable + 0x10);
    auto vtable_scope    = memory::read<uintptr_t>(vtable_init + 0x18);
    abc_env              = memory::read<avm::AbcEnv *>(vtable_scope + 0x10);
    m_const_pool         = abc_env->pool;


    avm::Multiname *proxy_mn = abc_env->pool->find_multiname("ItemsControlMenuProxy");

    avm::ScriptObject *menu_proxy_obj = flash_stuff::finddef(m_main->vtable->einit, proxy_mn); //  abc_env->finddef("ItemsControlMenuProxy$")

    if (!menu_proxy_obj)
    {
        utils::log("[!] Failed to find menu proxy global\n");
        return false;
    }
    else if (auto *menu_proxy = menu_proxy_obj->get_at<avm::ClassClosure *>(0x20))
    {
        avm::ScriptObject *proxy_object = menu_proxy->construct();
        avm::MethodEnv *send_action     = proxy_object->vtable->get_methods().at(36);
        uint32_t packet_mn              = send_action->method_info->get_params()[0];
        avm::Multiname *mn              = m_const_pool->get_multiname(packet_mn);
        avm::ScriptObject *global       = flash_stuff::finddef(send_action, mn);

        m_action_closure = global->get_at<avm::ClassClosure *>(0x20);
    }
    else
    {
        utils::log("[!] Failed to find ItemsControlMenuProxy closure!!\n");
        return false;
    }

    if (auto timer_method = m_screen_manager->vtable->methods[34]->method_info)
    {
        using namespace std::placeholders;
        utils::log("[+] Found gui timer method at {x}\n", reinterpret_cast<uintptr_t>(timer_method));
        hook_flash_function(timer_method, std::bind(&Darkorbit::handle_async_calls, this, _1, _2, _3));
    }
    else
    {
        utils::log("[!] Failed to find GuiManager timer!!\n");
        return false;
    }

    if (!m_ipc.Running() && m_ipc.Init())
    {
        m_ipc.Run();
    }
    else
    {
        // ....
    }

    return (m_installed = true);
}

bool Darkorbit::uninstall()
{
    utils::log("[-] Uninstalling...\n");

    for (auto &[id, hook] : m_hooks)
    {
        hook.restore();
    }
    m_hooks.clear();

    m_refine_multiname = 0;
    m_item_prop_mn = 0;


    m_ipc.Remove();
    m_installed = false;
    return true;
}
