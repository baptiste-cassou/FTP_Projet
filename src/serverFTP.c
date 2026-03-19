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
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        unix_error("sigaction error");
    }
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

int ftp_server_run(void)
{
    int i;

    ftp_enter_working_directory("serverFTP", FTP_SERVER_DATA_DIR);
    g_listenfd = Open_listenfd(FTP_PORT);
    printf("serverFTP listening on port %d with %d workers\n", FTP_PORT, NB_PROC);

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

int main(void)
{
    return ftp_server_run();
}
