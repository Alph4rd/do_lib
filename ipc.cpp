#include "ipc.h"

#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "darkorbit.h"
#include "flash_stuff.h"
#include "memory.h"
#include "utils.h"

#define MEM_SIZE 1024

using namespace std::chrono_literals;

union semun
{ 
    int                 val;
    struct semid_ds *   buf;
    unsigned short *    array;
#if defined(__linux__)
    struct seminfo *    __buf;
#endif
};

enum class MessageType
{
    CALL,
    RESULT,
    SEND_NOTIFICATION,
    REFINE,
    UPGRADE,
    USE_ITEM,
    KEY_CLICK,
    MOUSE_CLICK,
    NONE

};

struct RefineMessage
{
    MessageType type = MessageType::REFINE;
    uintptr_t refine_util;
    int ore, amount;
};

struct SendNotificationMessage
{
    MessageType type = MessageType::SEND_NOTIFICATION;
    char name[64];
    uint32_t argc;
    uintptr_t argv[64];
};

struct FunctionResultMessage
{
    MessageType type = MessageType::RESULT;
    bool error = false;
    uintptr_t value;
};

struct CallFunctionMessage
{
    MessageType type = MessageType::CALL;;
    avm::ScriptObject *object;
    uint32_t index;
    int argc;
    uintptr_t argv[64];
};

struct UseItemMessage
{
    MessageType type = MessageType::USE_ITEM;
    char name[64];
    uint8_t action_type;
    bool action_bar;

    // ItemsControlMenuConstants.ACTION_SELECTION == 1
    // ItemsControlMenuConstants.ACTION_TOOGLE == 0
    // ItemsControlMenuConstants.ACTION_ONE_SHOT == 1
    // barId = _loc2_.barId == CATEGORY_BAR ? 0 : 1;

};

struct KeyClickMessage
{
    MessageType type = MessageType::KEY_CLICK;
    uint32_t key;
};

struct MouseClickMessage
{
    MessageType type = MessageType::MOUSE_CLICK;
    uint32_t button;
    int32_t x;
    int32_t y;
};


union Message
{
    Message() { };
    MessageType type = MessageType::NONE;;
    CallFunctionMessage call;
    FunctionResultMessage result;
    SendNotificationMessage notify;
    RefineMessage refine;
    UseItemMessage item;
    KeyClickMessage key;
    MouseClickMessage click;
};

static_assert(sizeof(Message) < MEM_SIZE, "Message is larger than the allocated shared memory");

bool Ipc::Init()
{
    pid_t pid = getpid();

    if ((m_shmid = shmget(pid, MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        utils::log("[Ipc::init] Failed to get shared memory: {}\n", strerror(errno));
        return false;
    }

    if ((m_sem = semget(pid, 2, IPC_CREAT | 0666)) < 0)
    {
        utils::log("[Ipc::init] Failed to create shared semaphore: {}\n", strerror(errno));
        return false;
    }

    if ((m_shared = reinterpret_cast<Message *>(shmat(m_shmid, NULL, 0))) == (Message *)-1)
    {
        utils::log("[Ipc::init] Failed to attach shared memory to our process: {}\n", strerror(errno));
        return false;
    }

    if (semctl(m_sem, 0, SETVAL, 1) == -1 || semctl(m_sem, 1, SETVAL, 0) == -1)
    {
        utils::log("[Ipc::init] semctl failed: {}\n", strerror(errno));
        return false;
    }

    return true;
}

void Ipc::Remove()
{
    union semun dummy;
    if (shmctl(m_shmid, IPC_RMID, NULL) == -1)
    {
        utils::log("[Ipc::Remove] shmctl failed: {}\n", strerror(errno));
    }
    if (semctl(m_sem, 0, IPC_RMID, dummy) == -1)
    {
        utils::log("[Ipc::Remove] semctl failed: {}\n", strerror(errno));
    }

    utils::log("[Ipc::Remove] waiting for runner thread to stop\n");

    m_running = false;
    if (m_runner_thread.joinable())
    {
        m_runner_thread.join();
    }
}

void Ipc::handle_message()
{
    if (!m_shared)
    {
        return;
    }


    auto *result = reinterpret_cast<FunctionResultMessage *>(m_shared);


    switch (m_shared->type)
    {
        case MessageType::CALL:
        {
            auto *call = reinterpret_cast<CallFunctionMessage *>(m_shared);

            if (!call->object)
            {
                utils::log("[Ipc::handle_message] null object\n");
                result->error = true;
                result->type = MessageType::RESULT;
                break;
            }

            if ((call->argc * sizeof(uintptr_t)) > sizeof(CallFunctionMessage::argv))
            {
                utils::log("[Ipc::handle_message] argc too big {x}\n", static_cast<int>(m_shared->type));
                result->error = true;
                result->type = MessageType::RESULT;
                break;
            }

            auto res = Darkorbit::get().call_sync([call] {
                return call->object->call_method(call->index, call->argc, call->argv);
            });

            if (res.wait_for(5000ms) != std::future_status::ready)
            {
                result->error = true;
                result->type = MessageType::RESULT;
            }
            else
            {
                auto value = res.get();
                result->type = MessageType::RESULT;
                result->error = false;
                result->value = value;
            }
            break;
        }
        case MessageType::SEND_NOTIFICATION:
        {
            auto *msg = reinterpret_cast<SendNotificationMessage *>(m_shared);
            std::string name = msg->name;

            if (static_cast<size_t>(msg->argc) > sizeof(msg->argv) / sizeof(msg->argv[0]))
            {
                utils::log("[Ipc::handle_message] argc too big {x}\n", static_cast<int>(m_shared->type));
                result->error = true;
                result->type = MessageType::RESULT;
                break;
            }

            std::vector<uintptr_t> args(&msg->argv[0], &msg->argv[msg->argc]);

            auto res = Darkorbit::get().call_sync([name, args] ()
            {
                return Darkorbit::get().send_notification(name, args);
            });

            if (res.wait_for(5000ms) != std::future_status::ready)
            {
                result->error = true;
                result->type = MessageType::RESULT;
            }
            break;
        }
        case MessageType::USE_ITEM:
        {
            auto *msg = reinterpret_cast<UseItemMessage *>(m_shared);
            std::string name = msg->name;

            auto res = Darkorbit::get().call_sync([name] ()
            {
                return Darkorbit::get().use_item(name, 0, 1);
            });

            if (res.wait_for(5000ms) != std::future_status::ready)
            {
                result->error = true;
                result->type = MessageType::RESULT;
            }
            break;
        }
        case MessageType::REFINE:
        {
            auto *msg = reinterpret_cast<RefineMessage *>(m_shared);
            auto res = Darkorbit::get().call_sync([ore=msg->ore, amount=msg->amount]
            {
                return Darkorbit::get().refine_ore(ore, amount);
            });

            if (res.wait_for(5000ms) != std::future_status::ready)
            {
                result->error = true;
                result->type = MessageType::RESULT;
            }

            break;
        }
        case MessageType::KEY_CLICK:
        {
            auto *msg = reinterpret_cast<KeyClickMessage *>(m_shared);
            auto res = Darkorbit::get().call_sync([key=msg->key]
            {
                return Darkorbit::get().key_click(key);
            });

            if (res.wait_for(5000ms) != std::future_status::ready)
            {
                result->error = true;
                result->type = MessageType::RESULT;
            }

            break;
        }
        case MessageType::MOUSE_CLICK:
        {
            auto *msg = reinterpret_cast<MouseClickMessage *>(m_shared);
            auto res = Darkorbit::get().call_sync([msg]
            {
                return Darkorbit::get().mouse_click(msg->x, msg->y, msg->button);
            });

            if (res.wait_for(5000ms) != std::future_status::ready)
            {
                result->error = true;
                result->type = MessageType::RESULT;
            }

            break;
        }


        default:
            utils::log("[Ipc::handle_message] Unknown ipc message type {x}\n", static_cast<int>(m_shared->type));
            break;
    }
}

void Ipc::runner()
{
    struct sembuf sop[2];

    while (m_running)
    {
        sop[0] = { .sem_num=0, .sem_op=0, .sem_flg=0 };
        sop[1] = { .sem_num=1, .sem_op=1, .sem_flg=0 };
        if (semop(m_sem, sop, 2) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            utils::log("[Ipc::runner] semop1 failed {}\n", strerror(errno));
            m_running = false;
            break;
        }

        handle_message();

        sop[0] = { .sem_num=0, .sem_op= 1, .sem_flg=0 };
        sop[1] = { .sem_num=1, .sem_op=-1, .sem_flg=0 };
        if (semop(m_sem, sop, 2) == -1)
        {
            utils::log("[Ipc::runner] semop2 failed {}\n", strerror(errno));
            m_running = false;
            break;
        }
    }
    utils::log("[Ipc::runner] Stopped\n");
}

Ipc::~Ipc()
{
    Remove();
}
