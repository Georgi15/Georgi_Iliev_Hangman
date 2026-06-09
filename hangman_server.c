#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "game.h"
#include "game.c"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2

struct Client {
    int fd;
    int has_sent_word;
    secret_word_t opponent_word;
    int finished;
    int incorrect_count;
};

static void build_word_display(secret_word_t *word, char *out)
{
    size_t pos = 0;

    for(size_t i = 0; i < word->word_length; i++)
    {
        char c;

        if(secret_word_letter_at(word, i, &c)
            == SECRET_WORD_LETTER_REVEALED)
        {
            out[pos++] = c;
        }
        else
        {
            out[pos++] = '_';
        }
    }

    out[pos] = '\0';
}

static void build_incorrect_guesses_display(secret_word_t *word, char *out)
{
    size_t pos = 0;

    for (char c = 'a'; c <= 'z'; c++)
    {
        if ((word->incorrect_guesses.bitfield & (1u << (c - 'a'))) != 0)
        {
            if(pos > 1){
                out[pos++] = ',';
                out[pos++] = ' ';
                out[pos++] = c;
            }else{
                out[pos++] = ' ';
                out[pos++] = c;
            }
        }
    }

    out[pos] = '\0';
}

static void send_state(struct Client *client)
{
    char word[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    char incorrect_guesses[BUFFER_SIZE];

    build_word_display(&client->opponent_word, word);
    build_incorrect_guesses_display(&client->opponent_word, incorrect_guesses);

    snprintf(msg, sizeof(msg), "STATE|%s|", word);
    snprintf(msg + strlen(msg), sizeof(msg) - strlen(msg), "%s", incorrect_guesses);

    send(client->fd, msg, strlen(msg), 0);
}

static void send_result(struct Client *client, struct Client *opponent, int resultType) 
{
    char myIncorrect[BUFFER_SIZE];
    char opponentIncorrect[BUFFER_SIZE];
    char message[BUFFER_SIZE];

    build_incorrect_guesses_display(&client->opponent_word, myIncorrect);

    build_incorrect_guesses_display(&opponent->opponent_word, opponentIncorrect);

    const char *resultText;

    switch(resultType)
    {
        case 1:
            resultText = "YOU WIN! :)";
            break;

        case 2:
            resultText = "You Lose! :(";
            break;

        default:
            resultText = "Tie :/";
            break;
    }

    snprintf( message, sizeof(message), "RESULT|%s\nYour incorrect guesses:%s\nOpponent's incorrect guesses:%s\n", resultText, myIncorrect,opponentIncorrect);

    send(client->fd, message, strlen(message),0);
}

static void send_wait_signal(struct Client *client)
{
    char word[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    char incorrect_guesses[BUFFER_SIZE];

    build_word_display(&client->opponent_word, word);
    build_incorrect_guesses_display(&client->opponent_word, incorrect_guesses);

    snprintf(msg, sizeof(msg), "WAIT|%s|", word);
    snprintf(msg + strlen(msg), sizeof(msg) - strlen(msg), "%s", incorrect_guesses);

    send(client->fd, msg, strlen(msg), 0);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    printf("Listening on %d...\n", port);

    struct Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].has_sent_word = 0;
        clients[i].finished = 0;
        clients[i].incorrect_count = 0;
        clients[i].opponent_word.word = NULL;
        clients[i].opponent_word.revealed_bitfield = NULL;
        clients[i].opponent_word.word_length = 0;
        clients[i].opponent_word.revealed_bitfield_length = 0;
        clients[i].opponent_word.incorrect_guesses = letter_set_empty();
        clients[i].opponent_word.all_guesses = letter_set_empty();
    }

    int num_clients = 0;
    int game_started = 0;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);

        int max_fd = -1;

        if (num_clients < MAX_CLIENTS) {
            FD_SET(server_fd, &readfds);
            max_fd = server_fd;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select failed");
            break;
        }

        if (num_clients < MAX_CLIENTS && FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

            if (client_fd < 0) {
                perror("Accept failed");
                continue;
            }

            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    slot = i;
                    break;
                }
            }

            if (slot != -1) {
                clients[slot].fd = client_fd;
                clients[slot].has_sent_word = 0;
                num_clients++;
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &readfds)) {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_received = recv(clients[i].fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = '\0';
                    if (!clients[i].has_sent_word) {
                        int opponent = (i == 0) ? 1 : 0;
                        if(!secret_word_init_from_c_string(&clients[opponent].opponent_word, buffer))
                        {
                            send(clients[i].fd, "ERROR\n", 6, 0);
                            close(clients[i].fd);
                            clients[i].fd = -1;
                            continue;
                        }
                        clients[i].has_sent_word = 1;

                        int both_ready = 1;
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if ((clients[j].fd != -1 && !clients[j].has_sent_word) || num_clients != MAX_CLIENTS) {
                                both_ready = 0;
                                break;
                            }
                        }

                        if (both_ready) {
                            game_started = 1;
                            send_state(&clients[0]);
                            send_state(&clients[1]);
                        }

                    } else if (game_started) {
                        char guess = buffer[0];
                        secret_word_guess(&clients[i].opponent_word,guess);
                        if(secret_word_is_solved(&clients[i].opponent_word))
                        {
                            clients[i].finished = 1;
                            send_wait_signal(&clients[i]);
                        }
                        else{
                            send_state(&clients[i]);
                        }
                        if(clients[0].finished && clients[1].finished)
                        {
                            size_t p0 =
                                secret_word_incorrect_guess_count(
                                    &clients[0].opponent_word);

                            size_t p1 =
                                secret_word_incorrect_guess_count(
                                    &clients[1].opponent_word);

                            if(p0 < p1)
                            {
                                send_result(&clients[0], &clients[1], 1);
                                send_result(&clients[1], &clients[0], 2);
                            }
                            else if(p1 < p0)
                            {
                                send_result(&clients[1], &clients[0], 1);
                                send_result(&clients[0], &clients[1], 2);
                            }
                            else
                            {
                                send_result(&clients[0], &clients[1], 3);
                                send_result(&clients[1], &clients[0], 3);
                            }
                            return 0;
                        }
                    }
                } else if (bytes_received == 0) {
                    close(clients[i].fd);
                    secret_word_free(&clients[i].opponent_word);
                    clients[i].opponent_word.word = NULL;
                    clients[i].opponent_word.revealed_bitfield = NULL;
                    clients[i].opponent_word.word_length = 0;
                    clients[i].opponent_word.revealed_bitfield_length = 0;
                    clients[i].fd = -1;
                    clients[i].has_sent_word = 0;
                    num_clients--;
                    game_started = 0;
                } else {
                    perror("recv failed");
                    close(clients[i].fd);
                    secret_word_free(&clients[i].opponent_word);
                    clients[i].opponent_word.word = NULL;
                    clients[i].opponent_word.revealed_bitfield = NULL;
                    clients[i].opponent_word.word_length = 0;
                    clients[i].opponent_word.revealed_bitfield_length = 0;
                    clients[i].fd = -1;
                    clients[i].has_sent_word = 0;
                    num_clients--;
                    game_started = 0;
                }
            }
        }
    }

    close(server_fd);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            close(clients[i].fd);
            secret_word_free(&clients[i].opponent_word);
        }
    }

    return 0;
}