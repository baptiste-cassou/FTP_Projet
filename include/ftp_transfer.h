// Helpers de lecture et d'ecriture de payloads de fichiers.
#ifndef FTP_TRANSFER_H
#define FTP_TRANSFER_H

#include "ftp_shared.h"

// Helper legacy de l'etape 1 conserve pour reference; le code courant transfere en flux.
int ftp_load_file(const char *filename, void **buffer, uint64_t *file_size, ftp_status_t *status);

// Envoie un payload deja en memoire.
int ftp_send_buffer_payload(int connfd, const void *buffer, uint32_t payload_size);

// Envoie `file_size` octets a partir d'un descripteur deja ouvert.
int ftp_send_fd_payload(int connfd, int fd, uint64_t file_size);

// Lit `payload_size` octets et les retourne dans un buffer termine par '\0'.
int ftp_receive_buffer_payload(int connfd, uint32_t payload_size, char **buffer);

// Lit `file_size` octets depuis la socket et les ecrit dans `filename`.
int ftp_receive_file_payload(int connfd, const char *filename, uint64_t file_size, off_t offset);

#endif
