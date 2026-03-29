#include "csapp.h"
#include "ftp_protocol.h"
#include "ftp_shared.h"

#include <errno.h>

typedef struct {
    int ctrl_fd;
    int connected;
    slave_hello_t hello;
} registered_slave_t;

static volatile sig_atomic_t g_stop = 0;
static int g_listenfd = -1;
static registered_slave_t g_slaves[NB_SLAVES];
static int g_next_slave_index = 0;

static void safe_close_fd(int *fd)
{
    int current = *fd;
    if (current >= 0) {
        close(current);
        *fd = -1;
    }
}

static void master_sigint_handler(int sig)
{
    int i;

    (void)sig;
    g_stop = 1;
    safe_close_fd(&g_listenfd);
    for (i = 0; i < NB_SLAVES; i++) {
        safe_close_fd(&g_slaves[i].ctrl_fd);
    }
}

static void install_handler(void)
{
    Signal(SIGINT, master_sigint_handler);
}

static int send_cluster_map_to_slave(registered_slave_t *slave)
{
    slave_cluster_t cluster;
    int i;

    memset(&cluster, 0, sizeof(cluster));
    cluster.version = FTP_PROTO_VERSION;
    cluster.slave_count = NB_SLAVES;
    for (i = 0; i < NB_SLAVES; i++) {
        cluster.slaves[i] = g_slaves[i].hello;
    }

    if (ftp_send_cluster_map(slave->ctrl_fd, &cluster) < 0) {
        fprintf(stderr, "masterFTP: unable to send cluster map to slave %u\n",
                slave->hello.slave_id);
        return -1;
    }
    return 0;
}

static int register_one_slave(int slave_id, registered_slave_t *slave)
{
    int ctrl_fd;
    slave_hello_t hello;

    memset(slave, 0, sizeof(*slave));
    slave->ctrl_fd = -1;
    ctrl_fd = open_clientfd("127.0.0.1", FTP_SLAVE_CTRL_PORT(slave_id));
    if (ctrl_fd < 0) {
        fprintf(stderr, "masterFTP: unable to connect to slave %d on control port %d\n",
                slave_id, FTP_SLAVE_CTRL_PORT(slave_id));
        return -1;
    }

    if (ftp_receive_slave_hello(ctrl_fd, &hello) < 0) {
        close(ctrl_fd);
        return -1;
    }
    if (hello.version != FTP_PROTO_VERSION) {
        fprintf(stderr, "masterFTP: slave %d announced invalid version %u\n",
                slave_id, hello.version);
        close(ctrl_fd);
        return -1;
    }
    if ((int)hello.slave_id != slave_id) {
        fprintf(stderr, "masterFTP: expected slave id %d, received %u\n",
                slave_id, hello.slave_id);
        close(ctrl_fd);
        return -1;
    }
    if ((int)hello.ctrl_port != FTP_SLAVE_CTRL_PORT(slave_id)) {
        fprintf(stderr, "masterFTP: slave %d announced unexpected control port %u\n",
                slave_id, hello.ctrl_port);
        close(ctrl_fd);
        return -1;
    }

    slave->ctrl_fd = ctrl_fd;
    slave->connected = 1;
    slave->hello = hello;

    printf("masterFTP: slave %d registered (%s, client=%u, control=%u)\n",
           slave_id, slave->hello.host, slave->hello.client_port, slave->hello.ctrl_port);
    return 0;
}

static int register_all_slaves(void)
{
    int i;

    for (i = 1; i <= NB_SLAVES; i++) {
        if (register_one_slave(i, &g_slaves[i - 1]) < 0) {
            return -1;
        }
    }
    return 0;
}

static int send_cluster_map_to_all_slaves(void)
{
    int i;

    for (i = 0; i < NB_SLAVES; i++) {
        if (!g_slaves[i].connected || g_slaves[i].ctrl_fd < 0) {
            return -1;
        }
        if (send_cluster_map_to_slave(&g_slaves[i]) < 0) {
            return -1;
        }
        safe_close_fd(&g_slaves[i].ctrl_fd);
    }

    return 0;
}

static registered_slave_t *choose_next_slave(void)
{
    int attempts;
    for (attempts = 0; attempts < NB_SLAVES; attempts++) { 
        int idx = (g_next_slave_index + attempts) % NB_SLAVES;
        if (g_slaves[idx].connected) { //prend le prochain slave
            g_next_slave_index = (idx + 1) % NB_SLAVES;
            return &g_slaves[idx];
        }
    }

    return NULL;
}

static void serve_client_placeholders(void)
{
    struct sockaddr_in clientaddr;
    socklen_t clientlen = (socklen_t)sizeof(clientaddr);
    char client_ip_string[INET_ADDRSTRLEN];

    while (!g_stop) {
        int connfd = accept(g_listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd < 0) {
            if (errno == EINTR || errno == EBADF) {
                continue;
            }
            fprintf(stderr, "masterFTP: accept failed: %s\n", strerror(errno));
            continue;
        }

        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);
        
        registered_slave_t *slave = choose_next_slave();

        if (slave == NULL) {
            fprintf(stderr, "masterFTP: aucun slave n'est prêt à accueillir la connexion %s\n", client_ip_string);
            Close(connfd);
            clientlen = (socklen_t)sizeof(clientaddr);
            continue;
        }

        if (ftp_send_slave_hello(connfd, &slave->hello) < 0) {
            Close(connfd);
            clientlen = (socklen_t)sizeof(clientaddr);
            continue;
        }
        printf("masterFTP: redirected client %s to slave %u (%s:%u)\n",
            client_ip_string,
            slave->hello.slave_id,
            slave->hello.host,
            slave->hello.client_port);
        Close(connfd); //fermeture de la connection entre le master et le client
        clientlen = (socklen_t)sizeof(clientaddr);
    }
}

static int ftp_master_run(void)
{
    install_handler();

    if (register_all_slaves() < 0) {
        master_sigint_handler(SIGINT);
        return 1;
    }
    if (send_cluster_map_to_all_slaves() < 0) {
        master_sigint_handler(SIGINT);
        return 1;
    }
    g_listenfd = Open_listenfd(FTP_MASTER_PORT);
    printf("masterFTP: listening on port %d with %d registered slaves\n",
           FTP_MASTER_PORT, NB_SLAVES);

    serve_client_placeholders();
    return 0;
}

int main(void)
{
    return ftp_master_run();
}
