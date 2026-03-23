CC ?= gcc
CFLAGS ?= -Wall -g
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LIBS ?= -lpthread

SRC_DIR := src
BIN_DIR := bin

SERVER_SRC := $(SRC_DIR)/serverFTP.c
MASTER_SRC := $(SRC_DIR)/server_master.c
CLIENT_SRC := $(SRC_DIR)/clientFTP.c
COMMON_SRC := $(SRC_DIR)/csapp.c
RUNTIME_SRC := $(SRC_DIR)/ftp_runtime.c
PROTOCOL_SRC := $(SRC_DIR)/ftp_protocol.c
TRANSFER_SRC := $(SRC_DIR)/ftp_transfer.c
SERVER_REQ_SRC := $(SRC_DIR)/server_requests.c
CLIENT_REQ_SRC := $(SRC_DIR)/client_requests.c
UTILS_SRC := $(SRC_DIR)/utils.c

SERVER_OBJ := $(BIN_DIR)/serverFTP.o
CLIENT_OBJ := $(BIN_DIR)/clientFTP.o
COMMON_OBJ := $(BIN_DIR)/csapp.o
RUNTIME_OBJ := $(BIN_DIR)/ftp_runtime.o
PROTOCOL_OBJ := $(BIN_DIR)/ftp_protocol.o
TRANSFER_OBJ := $(BIN_DIR)/ftp_transfer.o
MASTER_OBJ := $(BIN_DIR)/server_master.o
SERVER_REQ_OBJ := $(BIN_DIR)/server_requests.o
CLIENT_REQ_OBJ := $(BIN_DIR)/client_requests.o
UTILS_OBJ := $(BIN_DIR)/utils.o

COMMON_FTP_OBJS := $(COMMON_OBJ) $(RUNTIME_OBJ) $(PROTOCOL_OBJ) $(TRANSFER_OBJ) $(UTILS_OBJ)
MASTER_COMMON_OBJS := $(COMMON_OBJ) $(RUNTIME_OBJ) $(PROTOCOL_OBJ) $(UTILS_OBJ)
SERVER_OBJS := $(SERVER_OBJ) $(COMMON_FTP_OBJS) $(SERVER_REQ_OBJ)
MASTER_OBJS := $(MASTER_OBJ) $(MASTER_COMMON_OBJS)
CLIENT_OBJS := $(CLIENT_OBJ) $(COMMON_FTP_OBJS) $(CLIENT_REQ_OBJ)

SERVER_BIN := $(BIN_DIR)/serverFTP
MASTER_BIN := $(BIN_DIR)/masterFTP
CLIENT_BIN := $(BIN_DIR)/clientFTP

HOST ?= 127.0.0.1

.PHONY: all clean run-server run-master run-client

all: $(SERVER_BIN) $(MASTER_BIN) $(CLIENT_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SERVER_BIN): $(SERVER_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(MASTER_BIN): $(MASTER_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(CLIENT_BIN): $(CLIENT_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-master: $(MASTER_BIN)
	./$(MASTER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN) $(HOST)

run-all: $(SERVER_BIN) $(CLIENT_BIN)
	-./$(SERVER_BIN) > logs/server.log 2>&1 &
	sleep 1
	-./$(CLIENT_BIN) $(HOST)
	pkill -9 serverFTP

clean:
	rm -f $(BIN_DIR)/*.o $(SERVER_BIN) $(MASTER_BIN) $(CLIENT_BIN) *.log *.toc *.aux *.out
