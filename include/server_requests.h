// Requetes traitees par le serveur FTP
#ifndef SERVER_REQUESTS_H
#define SERVER_REQUESTS_H

#include "ftp_shared.h"

typedef struct {
    int slave_id;
    slave_cluster_t cluster;
} ftp_server_context_t;

// Traite une connexion client complete
void ftp_handle_client(int connfd, const ftp_server_context_t *context);

// Traite une connexion de replication recue sur le port de controle d'un esclave.
int ftp_handle_replication_connection(int connfd);

#endif
