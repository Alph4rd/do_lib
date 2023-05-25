#include "sock_ipc.h"

#include <stdexcept>

#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

SockIpc::SockIpc()
{
    if ((m_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        throw std::runtime_error { "Failed to create ipc socket" };
    }
}

SockIpc::~SockIpc() 
{
    if (m_sock != -1)
    {
        close(m_sock);
    }
}

bool SockIpc::Connect(const std::string &path)
{
    sockaddr_un m_remote;
    m_remote.sun_family = AF_UNIX;
    strncpy(m_remote.sun_path, path.c_str(), sizeof(m_remote.sun_path));


    if (connect(m_sock, reinterpret_cast<sockaddr *>(&m_remote), sizeof(m_remote)) < 0)
    {
        return false;
    }
    m_connected = true;
    return true;
}

void SockIpc::Send(const std::string &msg)
{
    write(m_sock, msg.data(), msg.size());
}

