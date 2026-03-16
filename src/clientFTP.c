#include "csapp.h"
#include "ftp_shared.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host;
    char buf[MAXLINE];
    rio_t rio;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        return 1;
    }
    host = argv[1];

    clientfd = Open_clientfd(host, FTP_PORT);
    if (clientfd < 0) {
        fprintf(stderr, "unable to connect to %s:%d\n", host, FTP_PORT);
        return 1;
    }

    printf("Connected to %s.\n", host);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));
        if (Rio_readlineb(&rio, buf, MAXLINE) <= 0) {
            break;
        }
        Fputs(buf, stdout);
    }

    Close(clientfd);
    return 0;
}
