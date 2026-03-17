// Helpers de serialisation et de validation du protocole FTP minimal.
#ifndef FTP_PROTOCOL_H
#define FTP_PROTOCOL_H

#include "ftp_shared.h"

#include <stdint.h>

// Verifie qu'un nom de fichier est local et sans chemin dangereux.
int ftp_is_safe_filename(const char *filename);

// Construit une requete GET complete a envoyer au serveur.
void ftp_build_get_request(request_t *request, const char *filename);

// Lit exactement une requete `request_t` depuis une socket serveur.
int ftp_receive_request(int connfd, request_t *request);

// Envoie une structure `response_t` complete au client.
void ftp_send_response(int connfd, ftp_status_t status, uint32_t type, uint64_t file_size);

// Lit exactement une reponse `response_t` depuis une socket client.
int ftp_receive_response(int clientfd, response_t *response);

// Convertit un code de statut en chaine lisible pour les logs.
const char *ftp_status_to_string(ftp_status_t status);

#endif
