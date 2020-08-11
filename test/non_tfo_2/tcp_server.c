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

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/wait.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<time.h>

#define BACKLOG 10
#define PORT 53005
#define FILE_NAME "46403o.pdf"


int main()
{
    int sockfd, new_fd;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr; /* client's address info */
    int sin_size;
    char dst[INET_ADDRSTRLEN];
    int str_len = 5000;
    char str[str_len];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
    bzero(&(my_addr.sin_zero), 8);

    if (bind(sockfd,(struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    while(1)
    {
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(AF_INET, &(their_addr.sin_addr), dst, INET_ADDRSTRLEN);

        printf("Server: got connection from %s\n", dst);

        FILE *fd = fopen(FILE_NAME, "wb");
        if(fd == NULL)
        {
            perror("fopen");
        }

        int n;
        while (1)
        {

            if ((n = recv(new_fd, str, str_len, 0)) == 0)
            {
                printf("Client has closed connection!\n");
                break;
            }

            fwrite(str, 1, n, fd);

        }

        printf("Server side closes connection.\n");

        fclose(fd);
        close(new_fd);
    }

    close(sockfd);
    return 0;
}

