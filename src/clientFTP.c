#include "clientFTP.h"
#include "client_requests.h"
#include "csapp.h"
#include "ftp_runtime.h"
#include "ftp_shared.h"

static int parse_get_command(const char *line, char *filename)
{
    char command[16];
    char extra[16];
    int fields;

    filename[0] = '\0';
    fields = sscanf(line, " %15s %255s %15s", command, filename, extra);
    if (fields != 2) {
        return -1;
    }
    if (strcmp(command, "get") != 0) {
        return -1;
    }

    return 0;
}

int ftp_client_run(const char *host)
{
    int clientfd;
    char line[MAXLINE];
    char filename[FTP_MAX_FILENAME];
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
        
        if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
            fprintf(stderr, "bye\n");
            Close(clientfd);
            break; 
        }

        if (strcmp(line, "\n") == 0) continue;
        
        if (parse_get_command(line, filename) < 0) {
            fprintf(stderr, "clientFTP: expected 'get <filename>'\n");
            continue;
        }
        if (ftp_client_get(clientfd, filename, &stats) < 0) {
            continue;
        }

        kbytes_per_second = (stats.bytes_received / 1024.0) / stats.seconds;
        printf("Transfer successfully complete.\n");
        printf("%" PRIu64 " bytes received in %.3f seconds (%.2f Kbytes/s).\n",
            stats.bytes_received, stats.seconds, kbytes_per_second);
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
