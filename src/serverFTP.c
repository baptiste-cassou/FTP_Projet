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
static int g_ctrl_listenfd = -1;
static int g_ctrl_connfd = -1;
static pid_t child_pids[NB_PROC];

static void safe_close_fd(int *fdp)
{
    int current = *fdp;
    if (current >= 0) {
        close(current);
        *fdp = -1;
    }
}

static void safe_close_listenfd(void)
{
    safe_close_fd(&g_listenfd);
}

static int register_slave_to_master(int slave_id, int client_port)
{
    struct sockaddr_in masteraddr;
    socklen_t masterlen = (socklen_t)sizeof(masteraddr);
    char master_ip_string[INET_ADDRSTRLEN];
    slave_hello_t hello;

    printf("serverFTP slave %d waiting for master on control port %d\n",
           slave_id, FTP_SLAVE_CTRL_PORT(slave_id));

    while (!parent_stop) {
        g_ctrl_connfd = accept(g_ctrl_listenfd, (SA *)&masteraddr, &masterlen);
        if (g_ctrl_connfd < 0) {
            if (errno == EINTR || errno == EBADF) {
                continue;
            }
            fprintf(stderr, "serverFTP: unable to accept master control connection: %s\n",
                    strerror(errno));
            return -1;
        }
        break;
    }

    if (g_ctrl_connfd < 0) {
        return -1;
    }

    memset(&hello, 0, sizeof(hello));
    hello.version = FTP_PROTO_VERSION;
    hello.slave_id = (uint32_t)slave_id;
    hello.client_port = (uint32_t)client_port;
    hello.ctrl_port = (uint32_t)FTP_SLAVE_CTRL_PORT(slave_id);
    if (gethostname(hello.host, sizeof(hello.host) - 1) < 0) {
        strncpy(hello.host, "127.0.0.1", sizeof(hello.host) - 1);
    }

    Inet_ntop(AF_INET, &masteraddr.sin_addr, master_ip_string, INET_ADDRSTRLEN);
    Rio_writen(g_ctrl_connfd, &hello, sizeof(hello));
    printf("serverFTP slave %d registered to master %s with client port %d\n",
           slave_id, master_ip_string, client_port);

    return 0;
}

static void parent_sigint_handler(int sig)
{
    int i;
    (void)sig;
    parent_stop = 1;
    safe_close_listenfd();
    safe_close_fd(&g_ctrl_listenfd);
    safe_close_fd(&g_ctrl_connfd);
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
    safe_close_fd(&g_ctrl_listenfd);
    safe_close_fd(&g_ctrl_connfd);
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

int ftp_server_run(int slave_id)
{
    int i;
    int client_port = FTP_SLAVE_CLIENT_PORT(slave_id);

    ftp_enter_working_directory("serverFTP", FTP_SERVER_DATA_DIR);
    g_listenfd = Open_listenfd(client_port);
    g_ctrl_listenfd = Open_listenfd(FTP_SLAVE_CTRL_PORT(slave_id));
    install_handler(parent_sigint_handler);

    if (register_slave_to_master(slave_id, client_port) < 0) {
        safe_close_fd(&g_ctrl_connfd);
        safe_close_fd(&g_ctrl_listenfd);
        safe_close_listenfd();
        return 1;
    }

    printf("serverFTP slave %d listening on client port %d with %d workers\n",
           slave_id, client_port, NB_PROC);

    for (i = 0; i < NB_PROC; i++) {
        pid_t pid = Fork();
        if (pid == 0) {
            safe_close_fd(&g_ctrl_listenfd);
            safe_close_fd(&g_ctrl_connfd);
            worker_loop(g_listenfd);
            exit(0);
        }
        child_pids[i] = pid;
    }

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

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slave_id>\n", argv[0]);
        return 1;
    }


    slave_id = atoi(argv[1]);
    if (slave_id < 1 || slave_id > NB_SLAVES) {
        fprintf(stderr, "Invalid slave_id. Must be between 1 and %d.\n", NB_SLAVES);
        return 1;
    }

    if (FTP_SLAVE_CLIENT_PORT(slave_id) == FTP_MASTER_PORT) {
        fprintf(stderr, "Error: Slave client port cannot be 2121 (conflicts with master server port).\n");
        return 1;
    }

    return ftp_server_run(slave_id);
}
