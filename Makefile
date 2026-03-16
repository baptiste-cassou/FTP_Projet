CC ?= gcc
CFLAGS ?= -Wall -g
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LIBS ?= -lpthread

SRC_DIR := src
BIN_DIR := bin

SERVER_SRC := $(SRC_DIR)/serverFTP.c
CLIENT_SRC := $(SRC_DIR)/clientFTP.c
COMMON_SRC := $(SRC_DIR)/csapp.c

SERVER_OBJ := $(BIN_DIR)/serverFTP.o
CLIENT_OBJ := $(BIN_DIR)/clientFTP.o
COMMON_OBJ := $(BIN_DIR)/csapp.o

SERVER_BIN := $(BIN_DIR)/serverFTP
CLIENT_BIN := $(BIN_DIR)/clientFTP

HOST ?= 127.0.0.1

.PHONY: all clean run-server run-client

all: $(SERVER_BIN) $(CLIENT_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN) $(HOST)

clean:
	rm -f $(BIN_DIR)/*.o $(SERVER_BIN) $(CLIENT_BIN)
