#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server-ip> <port> <your-word>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *my_word = argv[3];

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return 1;
    }

    if (send(sockfd, my_word, strlen(my_word), 0) < 0) {
        perror("send word failed");
        close(sockfd);
        return 1;
    }

    while (1){
        char buffer[BUFFER_SIZE];

        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if(bytes_received <= 0) break;

        buffer[bytes_received] = '\0';
        if(strncmp(buffer,"WAIT|",5)==0)
        {
            char *word = strtok(buffer + 5,"|");

            char *incorrect = strtok(NULL,"|");

            printf("Word: %s\n", word ? word : "");

            printf("Incorrect guesses: %s\n", incorrect ? incorrect : "");

            fflush(stdout);
        }else if(strncmp(buffer,"STATE|",6)==0){
            char *word = strtok(buffer + 6,"|");

            char *incorrect = strtok(NULL,"|");

            printf("Word: %s\n", word ? word : "");

            printf("Incorrect guesses: %s\n", incorrect ? incorrect : "");

            fflush(stdout);

            char guess[32];

            if(!fgets(guess, sizeof(guess), stdin))
            {
                break;
            }

            send(sockfd, guess, strlen(guess), 0);
        }else if(strncmp(buffer, "RESULT|", 7) == 0){
            printf("%s", buffer + 7);
            fflush(stdout);
            break;
        }
    }

    close(sockfd);

    return 0;
}