#ifndef IPC_H
#define IPC_H

#include <vector>
#include <thread>


union Message;

class Ipc
{
public:
    Ipc() { }

    bool Init();

    const bool Running() const { return m_running; }

    void Run()
    {
        m_running = true;
        m_runner_thread = std::thread(&Ipc::runner, this);
    }

    void Remove();

    ~Ipc();
private:
    void handle_message();
    void runner();

    std::thread m_runner_thread;
    int m_shmid;
    int m_sem;
    Message *m_shared = nullptr;
    bool m_running = false;
};


#endif /* IPC_H */
