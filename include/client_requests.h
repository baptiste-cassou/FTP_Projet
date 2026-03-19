// Requetes emises par le client FTP.
#ifndef CLIENT_REQUESTS_H
#define CLIENT_REQUESTS_H

#include <stdint.h>

typedef struct {
    uint64_t bytes_received;
    double seconds;
} ftp_transfer_stats_t;

// Envoie une requete GET et enregistre la reponse recue dans `data_client/`.
int ftp_client_get(int clientfd, const char *filename, ftp_transfer_stats_t *stats);

// Envoie une requete BYE pour terminer la session proprement
int ftp_client_bye(int clientfd);

#endif
