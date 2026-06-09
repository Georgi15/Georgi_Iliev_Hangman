CC = gcc

SERVER_TARGET = hangman-server
CLIENT_TARGET = hangman-client

SERVER_SRC = hangman_server.c
CLIENT_SRC = hangman_client.c

DEPS = game.h game.c

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_SRC) $(DEPS)
	$(CC) $(SERVER_SRC) -o $(SERVER_TARGET) 

$(CLIENT_TARGET): $(CLIENT_SRC)
	$(CC) $(CLIENT_SRC) -o $(CLIENT_TARGET) 

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)