// Helpers de serialisation et de validation du protocole FTP minimal.
#ifndef FTP_PROTOCOL_H
#define FTP_PROTOCOL_H

#include "ftp_shared.h"

#include <stdint.h>

// Verifie qu'un nom de fichier est local et sans chemin dangereux.
int ftp_is_safe_filename(const char *filename);

// Initialise une requete vide pour un type donne.
void ftp_init_request(request_t *request, typereq_t type);

// Construit une requete GET complete a envoyer au serveur.
void ftp_build_get_request(request_t *request, const char *filename, uint64_t offset);

// Construit une requete PUT complete a envoyer au serveur.
void ftp_build_put_request(request_t *request, const char *filename, uint64_t file_size);

// Construit une requete simple basee sur un nom de fichier (RM).
void ftp_build_named_request(request_t *request, typereq_t type, const char *filename);

// Construit une requete AUTH complete.
void ftp_build_auth_request(request_t *request, const char *login, const char *password);

// Envoie une requete `request_t` complete au serveur.
int ftp_send_request(int connfd, const request_t *request);

// Lit exactement une requete `request_t` depuis une socket serveur.
int ftp_receive_request(int connfd, request_t *request);

// Envoie une structure `response_t` complete au client.
int ftp_send_response(int connfd, ftp_status_t status, uint32_t type, uint64_t file_size, uint64_t offset, uint32_t payload_size);

// Lit exactement une reponse `response_t` depuis une socket client.
int ftp_receive_response(int clientfd, response_t *response);

// Helpers pour le protocole de controle maitre/esclaves.
int ftp_send_slave_hello(int connfd, const slave_hello_t *hello);
int ftp_receive_slave_hello(int connfd, slave_hello_t *hello);
int ftp_send_cluster_map(int connfd, const slave_cluster_t *cluster);
int ftp_receive_cluster_map(int connfd, slave_cluster_t *cluster);
int ftp_send_replication_request(int connfd, const ftp_replication_request_t *request);
int ftp_receive_replication_request(int connfd, ftp_replication_request_t *request);
int ftp_send_control_reply(int connfd, ftp_status_t status);
int ftp_receive_control_reply(int connfd, ftp_status_t *status);

// Convertit un code de statut en chaine lisible pour les logs.
const char *ftp_status_to_string(ftp_status_t status);

#endif
