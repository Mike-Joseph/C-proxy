/*
  Copyright (c) 2019 The Mode Group
  
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  
      https://www.apache.org/licenses/LICENSE-2.0
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#define TFO_ENABLED 1
#define LOG_INFO 1
#define BACKLOG 128
#define BUFFSIZE 100000
#define TFO_BUFFSIZE 5000

struct sockaddr_in backendReAddr;
char downConnReIp[INET_ADDRSTRLEN];
int downConnRePort;

typedef struct
{
    int senderFd;
    int receiverFd;
} ioCopySlaveArg;

typedef struct
{
    int upConnFd;
    char upConnReIp[INET_ADDRSTRLEN];
    int upConnRePort;
} ioCopyMasterArg;

void exitWithError(const char* errInfo)
{
    perror(errInfo);
    exit(1);
}

void* ioCopySlave(void* args)
{
    ioCopySlaveArg* actual_args = (ioCopySlaveArg*) args;
    int senderFd = actual_args->senderFd;
    int receiverFd = actual_args->receiverFd;

    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return NULL;
    }

    int size = 0, total_sent = 0, last_sent = 0;

    while (1)
    {
        size = splice(senderFd, NULL, pipefd[1], NULL, BUFFSIZE, 0);

        if (size == 0)
        {
            /* relay shutdown info to the other side */
            shutdown(receiverFd, SHUT_WR);
            goto close;
        } else if (size == -1) {
            perror("splice");
            goto close;
        }

        total_sent = 0;

        while (total_sent < size)
        {
            last_sent = splice(pipefd[0], NULL, receiverFd, NULL, BUFFSIZE, 0);

            if(last_sent == -1)
            {
                perror("splice");
                goto close;
            }

            total_sent += last_sent;
        }

    }

close:
    close(pipefd[0]);
    close(pipefd[1]);
    return NULL;
}

void* ioCopyMaster(void* args)
{
    ioCopyMasterArg* actual_args = (ioCopyMasterArg*) args;
    int upConnFd = actual_args->upConnFd;
    int upConnRePort = actual_args->upConnRePort;
    char upConnReIp[INET_ADDRSTRLEN];
    strcpy(upConnReIp, actual_args->upConnReIp);
    free(args);

    int downConnFd;

    /* create client socket */
    if ((downConnFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        close(upConnFd);
        return NULL;
    }

# ifdef TFO_ENABLED
    /* set upConnFd to be non-blocking */
    int flags = fcntl(upConnFd, F_GETFL, 0);

    if (flags == -1 || fcntl(upConnFd, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        perror("fcntl");
        goto conn_close;
    }

    char buff[TFO_BUFFSIZE];

    int size = 0, total_sent = 0, last_sent = 0;

    size = recv(upConnFd, buff, TFO_BUFFSIZE, 0);

    if (size > 0 || (size < 0 && errno == EWOULDBLOCK) )
    {
        if(size < 0)
        {
            size = 0;
        }

# ifdef LOG_INFO
        printf("TFO data from %s:%d - %d bytes\n", upConnReIp, upConnRePort, size);
        fflush(stdout);
# endif

        if ((last_sent = sendto(downConnFd, buff, size, MSG_FASTOPEN,
                                (struct sockaddr*)&backendReAddr, sizeof(struct sockaddr))) == -1)
        {
            perror("connect");
            goto conn_close;
        }

        total_sent += last_sent;

        while(total_sent < size)
        {
            if((last_sent = send(downConnFd, &buff[total_sent], size - total_sent, 0)) == -1)
            {
                perror("send");
                goto conn_close;
            }

            total_sent += last_sent;
        }
    }
    else if(size == 0)
    {
        goto conn_close;
    }
    else
    {
        perror("recv");
        goto conn_close;
    }

    /* set upConnFd back to blocking */
    flags = fcntl(upConnFd, F_GETFL, 0);
    
    if (flags == -1 || fcntl(upConnFd, F_SETFL, (flags & ~O_NONBLOCK)) != 0)
    {
        perror("fcntl");
        goto conn_close;
    }
# else
    if (connect(downConnFd, (struct sockaddr*)&backendReAddr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect");
        goto conn_close;
    }
# endif

#ifdef LOG_INFO
    printf("Connect %s:%d and %s:%d\n", upConnReIp, upConnRePort, downConnReIp, downConnRePort);
    fflush(stdout);
#endif

    /* launch two new threads to do the job; no need to malloc because arguments are immutable */
    ioCopySlaveArg args1, args2;

    args1.senderFd = upConnFd;
    args1.receiverFd = downConnFd;
    args2.senderFd = downConnFd;
    args2.receiverFd = upConnFd;

    pthread_t t1, t2;
    pthread_create(&t1, NULL, &ioCopySlave, (void*) &args1);
    pthread_create(&t2, NULL, &ioCopySlave, (void*) &args2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

conn_close:
    close(upConnFd);
    close(downConnFd);
#ifdef LOG_INFO
    printf("Connection between %s:%d and %s:%d is closed\n",
           upConnReIp, upConnRePort, downConnReIp, downConnRePort);
    fflush(stdout);
#endif
    return NULL;
}

int main(int argc, char* argv[])
{
    int sin_size = sizeof(struct sockaddr_in);

    /* listen socket */
    int listenFd;
    struct sockaddr_in listenMyAddr;
    char listenMyReIp[INET_ADDRSTRLEN];
    int listenMyRePort;

    if (argc != 5)
    {
        fprintf(stderr, "usage: ./tcp_proxy listenIP listenPort backendIP backendPort\n");
        exit(1);
    }

    /* read listen socket's address and port */
    bzero(&listenMyAddr, sizeof(listenMyAddr));
    listenMyAddr.sin_family = AF_INET;
    listenMyAddr.sin_addr.s_addr = inet_addr(argv[1]);
    listenMyAddr.sin_port = htons(atoi(argv[2]));

    inet_ntop(AF_INET, &(listenMyAddr.sin_addr), listenMyReIp, INET_ADDRSTRLEN);
    listenMyRePort = (int) ntohs(listenMyAddr.sin_port);
#ifdef LOG_INFO
    printf("Listening address %s:%d\n", listenMyReIp, listenMyRePort);
    fflush(stdout);
#endif

    /* read backend sockets address and port */
    bzero(&backendReAddr, sizeof(backendReAddr));
    backendReAddr.sin_family = AF_INET;
    backendReAddr.sin_addr.s_addr = inet_addr(argv[3]);
    backendReAddr.sin_port = htons(atoi(argv[4]));

    inet_ntop(AF_INET, &(backendReAddr.sin_addr), downConnReIp, INET_ADDRSTRLEN);
    downConnRePort = (int) ntohs(backendReAddr.sin_port);
#ifdef LOG_INFO
    printf("Backend address %s:%d\n", downConnReIp, downConnRePort);
    fflush(stdout);
#endif

    /* create a listening server */
    if ((listenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) exitWithError("socket");

    if (bind(listenFd, (struct sockaddr*)&listenMyAddr, sizeof(struct sockaddr)) == -1)
        exitWithError("bind");

# ifdef TFO_ENABLED
    int qlen = 5;
    setsockopt(listenFd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
# endif

    if (listen(listenFd, BACKLOG) == -1) exitWithError("listen");

#ifdef LOG_INFO
    printf("Proxy is listening on %s:%d\n", listenMyReIp, listenMyRePort);
    fflush(stdout);
#endif

    while(1)
    {
        /* Need to malloc, because the thread will be detached */
        ioCopyMasterArg* args = (ioCopyMasterArg*) malloc(sizeof(ioCopyMasterArg));
        if (args == NULL) exitWithError("malloc");

        struct sockaddr_in upConnReAddr;

        args->upConnFd = accept(
            listenFd, (struct sockaddr*) &upConnReAddr, (socklen_t*) &sin_size);

        if (args->upConnFd == -1) exitWithError("accept");

        inet_ntop(AF_INET, &(upConnReAddr.sin_addr), args->upConnReIp, INET_ADDRSTRLEN);
        args->upConnRePort = (int) ntohs(upConnReAddr.sin_port);

#ifdef LOG_INFO
        printf("Accept %s:%d\n", args->upConnReIp, args->upConnRePort);
        fflush(stdout);
#endif

        pthread_t t;
        pthread_create(&t, NULL, &ioCopyMaster, (void*) args);

        if (pthread_detach(t) != 0) exitWithError("pthread_detach");

    }

    close(listenFd);

#ifdef LOG_INFO
    printf("Proxy is closed\n");
    fflush(stdout);
#endif

    return 0;
}
