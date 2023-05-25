#ifndef SOCK_IPC_H
#define SOCK_IPC_H
#include <string>



class SockIpc
{
public:
    SockIpc();
    ~SockIpc();

    bool Connected() const { return m_connected; }

    bool Connect(const std::string &path);

    void Send(const std::string &msg);

    bool m_connected = false;
    int m_sock = -1;
};

#endif // SOCK_IPC_H
