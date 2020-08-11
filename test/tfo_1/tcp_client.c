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

#define PORT 53005
#define FILE_NAME "46403o.pdf"

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    struct sockaddr_in their_addr; /* client's address information */
    struct hostent *he;
    int str_len = 50000;
    char str[str_len];

    if (argc != 2)
    {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    if ((he=gethostbyname(argv[1])) == NULL)
    {
        herror("gethostbyname");
        exit(1);
    }


    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(PORT);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    bzero(&(their_addr.sin_zero), 8);

    if (sendto(sockfd, NULL, 0, MSG_FASTOPEN,
               (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("sendto");
        exit(1);
    }

    printf("Successfully connected.\n");

    FILE *fd = fopen(FILE_NAME, "wb");
    if(fd == NULL)
    {
        perror("fopen");
    }

    int n;
    while (1)
    {

        if ( (n = recv(sockfd, str, str_len, 0)) == 0)
        {
            printf("Server has closed connection!\n");
            break;
        }

        fwrite(str, 1, n, fd);

    }

    printf("Finish.\n");

    fclose(fd);
    close(sockfd);
    return 0;
}

