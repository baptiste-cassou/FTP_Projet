// Requetes emises par le client FTP.
#ifndef CLIENT_REQUESTS_H
#define CLIENT_REQUESTS_H

#include <stdint.h>

typedef struct {
    uint64_t bytes_transferred;
    double seconds;
} ftp_transfer_stats_t;

// Envoie une requete GET et enregistre la reponse recue dans `data_client/`.
int ftp_client_get(int clientfd, const char *filename, ftp_transfer_stats_t *stats);

// Envoie une requete PUT et televerse un fichier local vers le serveur.
int ftp_client_put(int clientfd, const char *filename, ftp_transfer_stats_t *stats);

// Envoie une requete LS et retourne la liste recue dans `listing`.
int ftp_client_ls(int clientfd, char **listing);

// Envoie une requete RM pour supprimer un fichier cote serveur.
int ftp_client_rm(int clientfd, const char *filename);

// Envoie une requete AUTH pour authentifier la session courante.
int ftp_client_auth(int clientfd, const char *login, const char *password);

// Envoie une requete BYE pour terminer la session proprement
int ftp_client_bye(int clientfd);

#endif
