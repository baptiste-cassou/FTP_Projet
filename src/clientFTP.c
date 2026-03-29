#include "clientFTP.h"
#include "client_requests.h"
#include "csapp.h"
#include "ftp_protocol.h"
#include "ftp_runtime.h"
#include "ftp_shared.h"

typedef struct {
    typereq_t type;
    char filename[FTP_MAX_FILENAME];
    char login[FTP_MAX_LOGIN];
    char password[FTP_MAX_PASSWORD];
} parsed_command_t;

static int receive_slave_redirection(int masterfd, slave_hello_t *hello)
{
    if (ftp_receive_slave_hello(masterfd, hello) < 0) {
        fprintf(stderr, "clientFTP: impossible de lire la requête de redirection\n");
        return -1;
    }

    if (hello->version != FTP_PROTO_VERSION) {
        fprintf(stderr, "clientFTP: mauvaise version %u\n", hello->version);
        return -1;
    }
    if (hello->client_port <= 3000 || hello->client_port >= 4000 ) {
        fprintf(stderr, "clientFTP: port invalid (%d)\n", hello->client_port);
        return -1;
    }

    return 0;
}

static int ftp_handle_redirection(const char *host, int port, char *r_host, int *r_port){
    slave_hello_t redirect;
    int clientfd = open_clientfd((char *)host, port);
    if (clientfd < 0) {
        fprintf(stderr, "impossible de se connecter au maitre %s:%d\n", host, port);
        return -1;
    }
    if (receive_slave_redirection(clientfd, &redirect) < 0) {
        Close(clientfd);
        return -1; //en cas d'erreur avec le slave on retente une nouvelle connexion
    }
    Close(clientfd);

    strncpy(r_host, redirect.host, FTP_MAX_HOST - 1);

    r_host[FTP_MAX_HOST - 1] = '\0'; 
    *r_port = (int)redirect.client_port;
    printf("Redirection vers le slave %u (%s:%d)\n", redirect.slave_id,r_host,*r_port);
    return 0;
}

static int parse_command(const char *line, parsed_command_t *cmd)
{
    char command[16];
    char extra[16];
    int fields;

    memset(cmd, 0, sizeof(*cmd));
    cmd->type = FTP_REQ_INVALID;

    fields = sscanf(line, " %15s", command);
    if (fields < 1) {
        return -1;
    }

    // Parsing de toutes les commandes
    if (strcmp(command, "get") == 0) {
        fields = sscanf(line, " %15s %255s %15s", command, cmd->filename, extra);
        if (fields != 2) return -1;
        cmd->type = FTP_REQ_GET;

    } else if (strcmp(command, "put") == 0) {
        fields = sscanf(line, " %15s %255s %15s", command, cmd->filename, extra);
        if (fields != 2) return -1;
        cmd->type = FTP_REQ_PUT;

    } else if (strcmp(command, "ls") == 0) {
        fields = sscanf(line, " %15s %15s", command, extra);
        if (fields != 1) return -1;
        cmd->type = FTP_REQ_LS;

    } else if (strcmp(command, "rm") == 0) {
        fields = sscanf(line, " %15s %255s %15s", command, cmd->filename, extra);
        if (fields != 2) return -1;
        cmd->type = FTP_REQ_RM;

    } else if (strcmp(command, "bye") == 0) {
        fields = sscanf(line, " %15s %15s", command, extra);
        if (fields != 1) return -1;
        cmd->type = FTP_REQ_BYE;

    } else if (strcmp(command, "auth") == 0) {
        fields = sscanf(line, " %15s %31s %63s %15s", command, cmd->login, cmd->password, extra);
        if (fields != 3) return -1;
        cmd->type = FTP_REQ_AUTH;
    } else {
        return -1;
    }

    return 0;
}

static int ftp_client_connect(const char *host, int port, int *clientfd) {
    char target_host[FTP_MAX_HOST]; //hostname après première connexion
    int target_port = port; //port après première connexion

    if (port == FTP_MASTER_PORT) {
        if(ftp_handle_redirection(host, port, target_host, &target_port) < 0) {
            fprintf(stderr, "redirection impossible\n");
            return -1;
        }
        *clientfd = open_clientfd(target_host, target_port);
    } else {
        *clientfd = open_clientfd((char *)host, target_port); // on peut tjrs se connecter directement à un serveur si port != 2121
    }

    if (*clientfd < 0) {
        fprintf(stderr, "unable to connect to %s:%d\n",
                (port == FTP_MASTER_PORT) ? target_host : host,
                target_port);
        return -1;
    }
    
    printf("Connected to %s:%d.\n",
           (port == FTP_MASTER_PORT) ? target_host : host,
           target_port);
    return 0;
}

static int ftp_client_reconnect(const char *host, int port, int *clientfd,
                                parsed_command_t cmd, ftp_transfer_stats_t *stats)
{
    int attempt;
    int result;

    if (*clientfd >= 0) {
        Close(*clientfd);
        *clientfd = -1;
    }

    for (attempt = 0; attempt < FTP_MAX_TRY_RECONNECTION; attempt++) {
        if (ftp_client_connect(host, port, clientfd) < 0) {
            if (attempt + 1 < FTP_MAX_TRY_RECONNECTION) {
                sleep(FTP_TIME_BETWEEN_TRY);
            }
            continue;
        }

        result = ftp_client_get(*clientfd, cmd.filename, stats);
        if (result == 0) {
            return 0;
        }

        if (result == -1) {
            return -1;
        }

        Close(*clientfd);
        *clientfd = -1;
        if (attempt + 1 < FTP_MAX_TRY_RECONNECTION) {
            sleep(FTP_TIME_BETWEEN_TRY);
        }
    }

    return -2;
}


int ftp_client_run(const char *host, int port)
{
    int clientfd = -1;
    char line[MAXLINE];
    ftp_transfer_stats_t stats;
    char *listing = NULL;
    double kbytes_per_second;

    ftp_enter_working_directory("clientFTP", FTP_CLIENT_DATA_DIR);
    if (ftp_client_connect(host, port, &clientfd) < 0) {
        return 1;
    }
    
    while (1) {
        printf("FTP >>> ");
        fflush(stdout);
        if (Fgets(line, sizeof(line), stdin) == NULL) {
            Close(clientfd);
            return 0;
        }
        if (strcmp(line, "\n") == 0) continue;

        parsed_command_t cmd;
        if (parse_command(line, &cmd) < 0) {
            fprintf(stderr, "clientFTP: commande invalide. Commandes disponibles : get <f>, put <f>, ls, rm <f>, auth <login> <pass>, bye\n");
            continue;
        }

        if (cmd.type == FTP_REQ_GET) {
            switch (ftp_client_get(clientfd, cmd.filename, &stats)) {
                case 0: //cas normal
                    break;
                case -1: //cas d'erreur normal
                    continue;
                case -2: //cas d'erreur serveur, tentative de reconnexion
                    fprintf(stderr, "Erreur serveur, tentative de reconnexion %s:%d\n", host, port);
                    switch (ftp_client_reconnect(host, port, &clientfd, cmd, &stats)) {
                        case 0:
                            break;
                        case -1:
                            continue;
                        case -2:
                            fprintf(stderr, "clientFTP: le client a tenté de se reconnecter %d fois mais a échoué\n",
                                    FTP_MAX_TRY_RECONNECTION);
                            return 1;
                    }
                    break;
            }
            kbytes_per_second = (stats.bytes_transferred / 1024.0) / stats.seconds;
            printf("Transfer successfully complete.\n");
            printf("%" PRIu64 " bytes received in %.3f seconds (%.2f Kbytes/s).\n",
                stats.bytes_transferred, stats.seconds, kbytes_per_second);
        } else if (cmd.type == FTP_REQ_PUT) {
            if (ftp_client_put(clientfd, cmd.filename, &stats) < 0) {
                continue;
            }
            kbytes_per_second = (stats.bytes_transferred / 1024.0) / stats.seconds;
            printf("Upload successfully complete.\n");
            printf("%" PRIu64 " bytes sent in %.3f seconds (%.2f Kbytes/s).\n",
                stats.bytes_transferred, stats.seconds, kbytes_per_second);
        } else if (cmd.type == FTP_REQ_LS) {
            if (ftp_client_ls(clientfd, &listing) < 0) {
                continue;
            }
            if (listing[0] == '\0') {
                printf("(empty directory)\n");
            } else {
                printf("%s", listing);
                if (listing[strlen(listing) - 1] != '\n') {
                    printf("\n");
                }
            }
            Free(listing);
            listing = NULL;
        } else if (cmd.type == FTP_REQ_RM) {
            if (ftp_client_rm(clientfd, cmd.filename) < 0) {
                continue;
            }
            printf("File '%s' removed.\n", cmd.filename);
        } else if (cmd.type == FTP_REQ_AUTH) {
            if (ftp_client_auth(clientfd, cmd.login, cmd.password) < 0) {
                continue;
            }
            printf("Authentication successful.\n");
        } else if (cmd.type == FTP_REQ_BYE) {
            ftp_client_bye(clientfd);
            printf("Bye!\n");
            break;
        } else {
            fprintf(stderr, "clientFTP: commande '%s' non encore implémentée côté client\n", line);
        }
    }
    Close(clientfd);
    return 0;
}

int main(int argc, char **argv)
{
    int port = FTP_MASTER_PORT;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: %s <host> [port]\n", argv[0]);
        return 1;
    }
    if (argc == 3) {
        port = atoi(argv[2]);
        if (port <= 0) {
            fprintf(stderr, "clientFTP: invalid port '%s'\n", argv[2]);
            return 1;
        }
    }

    return ftp_client_run(argv[1], port);
}
