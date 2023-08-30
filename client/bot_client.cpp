#include "bot_client.h"
#include <cstring>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstring>

#include "utils.h"
#include "proc_util.h"
#include "sock_ipc.h"

#include <signal.h>
#include <sys/uio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>


#define MEM_SIZE 1024


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
    CHECK_SIGNATURE,

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
    uintptr_t object;
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

struct GetSignatureMessage
{
    MessageType type = MessageType::CHECK_SIGNATURE;;
    uintptr_t object;
    uint32_t index;
    bool method_name;
    char signature[0x100];

    int32_t result;
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
    GetSignatureMessage sig;
};


BotClient::BotClient() :
    m_browser_ipc(new SockIpc())
{
}

BotClient::~BotClient()
{
    if (m_browser_pid > 0)
    {
        kill(m_browser_pid, SIGKILL);
    }
}

void sigchld_handler(int signal)
{
    int status = 0;
    waitpid(0, &status, WNOHANG);
}

void BotClient::LaunchBrowser()
{
    int pid = fork();

    switch (pid)
    {
        case -1: // https://rachelbythebay.com/w/2014/08/19/fork/
        {
            perror("Fork failed.");
            break;
        }
        case 0:
        {
            const char *fpath = "lib/backpage-linux-x86_64.AppImage";

            std::vector<const char *> envp
            {
                "LD_PRELOAD=lib/libdo_lib.so",
            };
            for (int i = 0; environ[i]; i++)
            {
                envp.push_back(environ[i]);
            }
            envp.push_back(nullptr);

            std::string url = m_url;
            std::string sid = m_sid;

            while (*(url.end()-1) == '/')
            {
                url.resize(url.size()-1);
            }

            if (sid.find("dosid=") == 0)
            {
                sid.replace(0, 6, "");
            }

            execle(fpath, fpath, "--sid", sid.c_str(), "--url", url.c_str(), "--launch", NULL, envp.data());
            break;
        }
        default:
        {
            signal(SIGCHLD, sigchld_handler);

            m_browser_pid = pid;
            break;
        }
    }
}

void BotClient::SendBrowserCommand(const std::string &&message, int sync)
{
    if (m_browser_pid > 0 && !ProcUtil::ProcessExists(m_browser_pid))
    {
        fprintf(stderr, "[SendBrowserCommand] Browser process not found, restarting it\n");
        LaunchBrowser();
        m_flash_pid = -1;
        return;
    }

    if (!m_browser_ipc->Connected())
    {
        if (m_browser_pid < 0)
        {
            return;
        }

        std::string ipc_path = utils::format("/tmp/darkbot_ipc_{}", m_browser_pid);

        //printf("[SendBrowserCommand] Connecting to %s\n", ipc_path.c_str());

        if (!m_browser_ipc->Connect(ipc_path))
        {
            printf("[SendBrowserCommand] Failed to connect to browser %d\n", m_browser_pid);
            return;
        }
    }

    //printf("[SendBrowserCommand] Sending message %s\n", message.c_str());

    m_browser_ipc->Send(message);
    return;
}

bool BotClient::find_flash_process()
{
    auto procs = ProcUtil::FindProcsByName("no-sandbox");
    for (int proc_pid : procs)
    {
        if (ProcUtil::IsChildOf(proc_pid, m_browser_pid) && ProcUtil::GetPages(proc_pid, "libpepflashplayer").size() > 0)
        {
            m_flash_pid = proc_pid;
            return true;
        }
    }
    return false;
}

void BotClient::reset()
{
    // Reset
    if (m_shared_mem_flash) shmdt(m_shared_mem_flash);
    if (m_flash_sem >= 0) semctl(m_flash_sem, 0, IPC_RMID, 1);


    m_shared_mem_flash = nullptr;
    m_flash_pid = -1;
    m_flash_sem = -1;
    m_flash_shmid = -1;
    m_flash_pid = -1;
}

// Not a great name since it has side-effects like refreshgin or restarting the browser
bool BotClient::IsValid()
{
    if (m_browser_pid > 0 && !ProcUtil::ProcessExists(m_browser_pid))
    {
        fprintf(stderr, "[IsValid] Browser process not found, restarting it\n");
        LaunchBrowser();
        return false;
    }


    if (m_flash_pid == -1)
    {
        return find_flash_process();
    }

    if (!ProcUtil::ProcessExists(m_flash_pid))
    {
        fprintf(stderr, "[IsValid] Flash process not found, trying to refresh %d, %d\n", m_flash_pid, m_browser_pid);
        SendBrowserCommand("refresh", 1);
        reset();
        return false;
    }
    return true;
}

void BotClient::SendFlashCommand(Message *message, Message *response)
{
    if (!IsValid())
    {
        return;
    }

    {
        if ((m_flash_shmid = shmget(m_flash_pid, MEM_SIZE, IPC_CREAT | 0666)) < 0)
        {
            fprintf(stderr, "[SendFlashCommand] Failed to get shared memory\n");
            return;
        }
    }

    if (!m_shared_mem_flash || m_shared_mem_flash == (void *)-1)
    {
        if ((m_shared_mem_flash = reinterpret_cast<Message *>(shmat(m_flash_shmid, NULL, 0))) == (void *)-1)
        {
            fprintf(stderr, "[SendFlashCommand] Failed to attach shared memory to our process\n");
            return;
        }
    }

    if (m_flash_sem < 0)
    {
        if ((m_flash_sem = semget(m_flash_pid , 2, IPC_CREAT | 0600)) < 0)
        {
            m_flash_pid = -1;
            fprintf(stderr, "[SendFlashCommand] Failed to create semaphore");
            return;
        }
    }


    *m_shared_mem_flash = *message;

    static timespec timeout { .tv_sec = 1, .tv_nsec = 0 };
    sembuf sop[2] { { 0, -1, 0 }, { 1, 0, 0 } };

    // Notify
    sop[0] = { 0, -1, 0 };
    if (semtimedop(m_flash_sem, &sop[0], 1, &timeout) == -1)
    {
        if (errno == EAGAIN)
        {
            fprintf(stderr, "[SendFlashCommand] Failed to send command to flash, timeout\n");
            return;
        }
        perror("[SendFlashCommand] semop failed");
        return;
    }

    // Wait
    sop[1] = { 1, 0, 0 };
    if (semtimedop(m_flash_sem, &sop[1], 1, &timeout) == -1)
    {
        if (errno == EAGAIN)
        {
            fprintf(stderr, "[SendFlashCommand] Failed to send command to flash, timeout\n");
            return;
        }
        perror("[SendFlashCommand] semop failed");
        return;
    }

    if (response)
    {
        memcpy(response, m_shared_mem_flash, sizeof(Message));
    }
}

bool BotClient::SendNotification(uintptr_t screen_manager, const std::string &name, const std::vector<uintptr_t> &args)
{
    Message message;
    message.type = MessageType::SEND_NOTIFICATION;
    message.notify.argc = args.size();
    std::memcpy(message.notify.argv, args.data(), sizeof(message.notify.argv));
    std::strncpy(message.notify.name, name.c_str(), sizeof(message.notify.name));
    SendFlashCommand(&message);
    return true;
}

bool BotClient::RefineOre(uintptr_t refine_util, uint32_t ore, uint32_t amount)
{
    Message message;
    message.type = MessageType::REFINE;
    message.refine.refine_util = refine_util;
    message.refine.ore = ore;
    message.refine.amount = amount;

    SendFlashCommand(&message);
    return true;
}

bool BotClient::UseItem(const std::string &name, uint8_t type, uint8_t bar)
{
    Message message;
    message.type = MessageType::USE_ITEM;
    message.item.action_type = type;
    message.item.action_bar = bar;
    std::strncpy(message.item.name, name.c_str(), sizeof(message.item.name));
    SendFlashCommand(&message);
    return true;
}

uintptr_t BotClient::CallMethod(uintptr_t obj, uint32_t index, const std::vector<uintptr_t> &args)
{
    Message message;
    message.type = MessageType::CALL;

    message.call.object = obj;
    message.call.index = index;
    message.call.argc = args.size();
    memcpy(message.call.argv, args.data(), args.size() * sizeof(uintptr_t));

    Message response;

    SendFlashCommand(&message, &response);

    return response.result.value;
}

bool BotClient::ClickKey(uint32_t key)
{
    Message message;
    message.type = MessageType::KEY_CLICK;
    message.key.key = key;
    SendFlashCommand(&message);
    return true;
}

bool BotClient::MouseClick(int32_t x, int32_t y, uint32_t button)
{
    Message message;
    message.type = MessageType::MOUSE_CLICK;
    message.click.x = x;
    message.click.y = y;
    message.click.button = button;
    SendFlashCommand(&message);
    return true;
}

int BotClient::CheckMethodSignature(uintptr_t object, uint32_t index, bool check_name, const std::string &sig)
{
    Message message;
    message.type = MessageType::CHECK_SIGNATURE;
    message.sig.object = object;
    message.sig.index = index;
    message.sig.method_name = check_name;

    strncpy(message.sig.signature, sig.c_str(), sizeof(message.sig.signature));

    Message response;
    SendFlashCommand(&message, &response);

    return response.sig.result;
}
