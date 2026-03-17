// Helpers de lecture et d'ecriture de payloads de fichiers.
#ifndef FTP_TRANSFER_H
#define FTP_TRANSFER_H

#include "ftp_shared.h"

// Charge un fichier complet en memoire cote serveur.
int ftp_load_file(const char *filename, void **buffer, uint64_t *file_size, ftp_status_t *status);

// Lit `file_size` octets depuis la socket et les ecrit dans `filename`.
int ftp_receive_file_payload(int connfd, const char *filename, uint64_t file_size);

#endif
