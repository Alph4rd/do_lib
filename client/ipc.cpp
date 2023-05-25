#include "ipc.h"

#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "utils.h"

#define MEM_SIZE 1024

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
    LOCK_ENTITY,
    NONE
};


union Message
{
    MessageType type = MessageType::NONE;;
};


bool ipc::Server::Init(int pid)
{
    if ((shmid = shmget(pid, MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        utils::format("[ipc::Server::init] Failed to get shared memory: {}\n", strerror(errno));
        return false;
    }

    if ((sem = semget(pid, 2, IPC_CREAT | 0666)) < 0)
    {
        utils::format("[ipc::Server::init] Failed to create shared semaphore: {}\n", strerror(errno));
        return false;
    }

    if ((shared = reinterpret_cast<Message *>(shmat(shmid, NULL, 0))) == (Message *)-1)
    {
        utils::format("[ipc::Server::init] Failed to attach shared memory to our process: {}\n", strerror(errno));
        return false;
    }

    if (semctl(sem, 0, SETVAL, 1) == -1 || semctl(sem, 1, SETVAL, 0) == -1)
    {
        utils::format("[ipc::Server::init] semctl failed: {}\n", strerror(errno));
        return false;
    }

    return true;
}

void ipc::Server::Remove()
{
    union semun dummy;
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        utils::format("[ipc::Server::Remove] shmctl failed: {}\n", strerror(errno));
    if (semctl(sem, 0, IPC_RMID, dummy) == -1)
        utils::format("[ipc::Server::Remove] semctl failed: {}\n", strerror(errno));
}

void ipc::Server::handle_message()
{
    if (!shared) return;

    switch (shared->type)
    {

    }
}

void ipc::Server::runner()
{
    struct sembuf sop[2];

    while (running)
    {
        sop[0] = { .sem_num=0, .sem_op=0, .sem_flg=0 };
        sop[1] = { .sem_num=1, .sem_op=1, .sem_flg=0 };
        if (semop(sem, sop, 2) == -1)
        {
            if (errno == EINTR)
                continue;
            utils::format("[ipc::Server::runner] semop1 failed {}\n", strerror(errno));
            running = false;
            break;
        }

        handle_message();

        sop[0] = { .sem_num=0, .sem_op=1, .sem_flg=0 };
        sop[1] = { .sem_num=1, .sem_op=-1, .sem_flg=0 };
        if (semop(sem, sop, 2) == -1)
        {
            utils::format("[ipc::Server::runner] semop2 failed {}\n", strerror(errno));
            running = false;
            break;
        }
    }
}


bool ipc::Client::Connect(int ipc_id)
{
    if ((shmid = shmget(ipc_id, MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("Failed to get shared memory");
        return false;
    }

    if ((shared = shmat(shmid, NULL, 0)) == (void *)-1)
    {
        perror("Failed to attach shared memory to our process");
        return false;
    }

    if ((sem  = semget(ipc_id, 2, IPC_CREAT | 0600)) < 0)
    {
        perror("Failed to create shared semaphore");
        return false;
    }

    if (semctl(sem, 0, SETVAL, 1) == -1 || semctl(sem, 1, SETVAL, 0) == -1)
    {
        perror("Failed to set browser ipc sempahore value");
        return false;
    }

    return true;
}

void ipc::Client::Remove()
{

    if (shared != (void*)-1) shmdt(shared);
    if (sem >= 0) semctl(sem, 0, IPC_RMID, 1);

    shared = (void *)-1;
    sem = -1;
    shmid = -1;
}
