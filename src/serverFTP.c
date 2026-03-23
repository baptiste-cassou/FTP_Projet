#include "csapp.h"
#include "ftp_runtime.h"
#include "ftp_shared.h"
#include "serverFTP.h"
#include "server_requests.h"

#include <errno.h>

static volatile sig_atomic_t parent_stop = 0;
static volatile sig_atomic_t child_stop = 0;
static int g_listenfd = -1;
static int g_connfd = -1;
static pid_t child_pids[NB_PROC];

static void safe_close_listenfd(void)
{
    int fd = g_listenfd;
    if (fd >= 0) {
        g_listenfd = -1;
        close(fd);
    }
}

static void parent_sigint_handler(int sig)
{
    int i;
    (void)sig;
    parent_stop = 1;
    safe_close_listenfd();
    for (i = 0; i < NB_PROC; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGINT);
        }
    }
}

static void child_sigint_handler(int sig)
{
    (void)sig;
    child_stop = 1;
    safe_close_listenfd();
    if (g_connfd >= 0) {
        close(g_connfd);
        g_connfd = -1;
    }
    _exit(0);
}

static void install_handler(void (*handler)(int))
{
    Signal(SIGINT, handler); //passage sur la fonction de csapp
}

static void worker_loop(int listenfd)
{
    struct sockaddr_in clientaddr;
    socklen_t clientlen;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[FTP_MAX_FILENAME];

    g_listenfd = listenfd;
    install_handler(child_sigint_handler);

    while (!child_stop) {
        int connfd;
        clientlen = (socklen_t)sizeof(clientaddr);
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd < 0) {
            if (errno == EINTR || errno == EBADF) {
                continue;
            }
            continue;
        }

        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, FTP_MAX_FILENAME, 0, 0, 0);
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);
        printf("serverFTP worker %d connected to %s (%s)\n", getpid(), client_hostname, client_ip_string);

        g_connfd = connfd;
        ftp_handle_client(connfd);
        if (g_connfd >= 0) {
            Close(g_connfd);    
            g_connfd = -1;
        }
    }
}

int ftp_server_run(int port)
{
    int i;

    ftp_enter_working_directory("serverFTP", FTP_SERVER_DATA_DIR);
    g_listenfd = Open_listenfd(port);
    printf("serverFTP listening on port %d with %d workers\n", port, NB_PROC);

    for (i = 0; i < NB_PROC; i++) {
        pid_t pid = Fork();
        if (pid == 0) {
            worker_loop(g_listenfd);
            exit(0);
        }
        child_pids[i] = pid;
    }

    install_handler(parent_sigint_handler);

    while (!parent_stop) {
        pause();
    }

    for (i = 0; i < NB_PROC; i++) {
        if (child_pids[i] > 0) {
            waitpid(child_pids[i], NULL, 0);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int slave_id;
    int port;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slave_id>\n", argv[0]);
        return 1;
    }


    slave_id = atoi(argv[1]);
    if (slave_id < 1 || slave_id > NB_SLAVES) {
        fprintf(stderr, "Invalid slave_id. Must be between 1 and %d.\n", NB_SLAVES);
        return 1;
    }

    if (FTP_SLAVE_CLIENT_BASE_PORT + slave_id == 2121) {
        fprintf(stderr, "Error: Slave client port cannot be 2121 (conflicts with master server port).\n");
        return 1;
    }

    port = FTP_SLAVE_CLIENT_PORT(slave_id);
    return ftp_server_run(port);
}
