#include "clientFTP.h"
#include "client_requests.h"
#include "csapp.h"
#include "ftp_runtime.h"
#include "ftp_shared.h"

typedef struct {
    typereq_t type;
    char filename[FTP_MAX_FILENAME];
    char login[FTP_MAX_LOGIN];
    char password[FTP_MAX_PASSWORD];
} parsed_command_t;

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

int ftp_client_run(const char *host)
{
    int clientfd;
    char line[MAXLINE];
    ftp_transfer_stats_t stats;
    double kbytes_per_second;

    ftp_enter_working_directory("clientFTP", FTP_CLIENT_DATA_DIR);
    clientfd = Open_clientfd((char *)host, FTP_PORT);
    if (clientfd < 0) {
        fprintf(stderr, "unable to connect to %s:%d\n", host, FTP_PORT);
        return 1;
    }

    printf("Connected to %s.\n", host);
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
            if (ftp_client_get(clientfd, cmd.filename, &stats) < 0) {
                continue;
            }
            kbytes_per_second = (stats.bytes_received / 1024.0) / stats.seconds;
            printf("Transfer successfully complete.\n");
            printf("%" PRIu64 " bytes received in %.3f seconds (%.2f Kbytes/s).\n",
                stats.bytes_received, stats.seconds, kbytes_per_second);
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
    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        return 1;
    }

    ftp_client_run(argv[1]);
}
