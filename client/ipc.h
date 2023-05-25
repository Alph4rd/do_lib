#ifndef IPC_H
#define IPC_H

#include <vector>
#include <thread>


union Message;

namespace ipc
{
    class Server
    {
    public:
        Server() { }

        bool Init();

        void Run()
        {
            running = true;
            runner_thread = std::thread(&Server::runner, this);
        }

        void Remove();
        ~Server();

    private:
        void handle_message();
        void runner();

        std::thread runner_thread;
        int shmid, sem;
        Message *shared = nullptr;
        bool running = false;
    };

    class Client
    {
    public:
        Client() { }

        bool Connect(int ipc_id);

        template<typename T>
        void SendMessage(const T &data)
        {
            memcpy(shared, &data, sizeof(T));
        }

        void Remove();
        ~Client()
        {
            Remove();
        }

    private:
        void handle_message();

        int shmid, sem;
        void *shared = (void *)-1;
        bool running = false;

    };

}

#endif /* IPC_H */
