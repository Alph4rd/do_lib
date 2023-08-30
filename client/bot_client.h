#ifndef BOT_CLIENT_H
#define BOT_CLIENT_H
#include <memory>
#include "proc_util.h"

class SockIpc;
union Message;

class BotClient
{
public:
    BotClient();
    ~BotClient();

    void SetCredentials(const std::string &sid, const std::string &url)
    {
        m_sid = sid;
        m_url = url;
    }

    void LaunchBrowser();

    void SetPid(int pid) { m_browser_pid = pid; }
    inline int Pid() const { return m_browser_pid; }
    inline int FlashPid() const { return m_flash_pid; }

    bool IsValid();

    void SendBrowserCommand(const std::string &&s, int sync);

    void SendFlashCommand(Message *message, Message *response = nullptr);

    bool RefineOre(uintptr_t refine_util, uint32_t ore, uint32_t amount);
    bool SendNotification(uintptr_t screen_manager, const std::string &name, const std::vector<uintptr_t> &args);
    bool UseItem(const std::string &name, uint8_t type, uint8_t bar);
    uintptr_t CallMethod(uintptr_t obj, uint32_t index, const std::vector<uintptr_t> &args);
    bool ClickKey(uint32_t key);
    bool MouseClick(int32_t x, int32_t y, uint32_t button);
    int CheckMethodSignature(uintptr_t object, uint32_t index, bool check_name, const std::string &sig);

    template <typename T>
    T Read(uintptr_t address, int *result = nullptr)
    {
        T r;
        int ok = ProcUtil::ReadMemoryBytes(m_flash_pid, address, &r, sizeof(T));
        if (result)
        {
            *result = ok;
        }
        if (ok < 0)
        {
            return 0;
        }
        return r;
    }

    template <typename T>
    void Write(uintptr_t address, T value, int *result = nullptr)
    {
        T r;
        int ok = ProcUtil::WriteMemoryBytes(m_flash_pid, address, &value, sizeof(T));
        if (result)
        {
            *result = ok;
        }
    }

    std::vector<uintptr_t> QueryMemory(uint8_t *query, size_t size, size_t amount)
    {
        if (m_flash_pid < 0 && !find_flash_process())
        {
            return { };
        }
        std::vector<uintptr_t> result (amount);
        std::string mask(size, 'x');
        size_t f = ProcUtil::QueryMemory(m_flash_pid, query, mask.c_str(), &result[0], result.size());
        result.resize(f);
        return result;
    }

    std::vector<uintptr_t> QueryMemory(std::vector<uint8_t> &query, size_t amount)
    {
        if (m_flash_pid < 0 && !find_flash_process())
        {
            return { };
        }
        std::vector<uintptr_t> result(amount);
        std::string mask(query.size(), 'x');
        size_t f = ProcUtil::QueryMemory(m_flash_pid, &query[0], mask.c_str(), &result[0], result.size());
        result.resize(f);
        return result;
    }



private:
    std::unique_ptr<SockIpc> m_browser_ipc;
    char *m_shared_mem = nullptr;
    Message *m_shared_mem_flash = nullptr;

    std::string m_sid;
    std::string m_url;

    int m_flash_sem = -1;
    int m_flash_shmid = -1;

    int m_browser_pid = -1, m_flash_pid = -1;

    bool find_flash_process();
    void reset();
};


#endif /* BOT_CLIENT_H */
