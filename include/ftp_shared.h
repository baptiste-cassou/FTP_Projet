#ifndef FTP_SHARED_H
#define FTP_SHARED_H
#include <stdint.h>

// Constantes
#define FTP_PORT 2121
#define FTP_PROTO_VERSION 1
#define FTP_BLOCK_SIZE 4096
#define NB_PROC 4


#define FTP_MAX_FILENAME 256
#define FTP_MAX_LOGIN 32
#define FTP_MAX_PASSWORD 64


typedef enum {
    FTP_REQ_INVALID = 0,
    FTP_REQ_GET = 1,
    FTP_REQ_PUT = 2,
    FTP_REQ_LS = 3,
    FTP_REQ_RM = 4,
    FTP_REQ_BYE = 5,
    FTP_REQ_AUTH = 6
} typereq_t;

typedef enum {
    FTP_STATUS_OK = 0,
    FTP_STATUS_ERR_BAD_REQUEST = 1,
    FTP_STATUS_ERR_NOT_FOUND = 2,
    FTP_STATUS_ERR_IO = 3,
    FTP_STATUS_ERR_AUTH_REQUIRED = 4,
    FTP_STATUS_ERR_AUTH_FAILED = 5,
    FTP_STATUS_ERR_UNSUPPORTED = 6
} ftp_status_t;

typedef struct {
    uint32_t version;
    uint32_t type;      // typereq_t 
    uint64_t offset;    // reprise de transfert
    uint32_t block_size;
    char filename[FTP_MAX_FILENAME];
    char login[FTP_MAX_LOGIN];
    char password[FTP_MAX_PASSWORD];
} request_t;

typedef struct {
    uint32_t status;    // ftp_status_t
    uint32_t type;      // typereq_t reponse associee
    uint64_t file_size; // taille totale du fichier concerne
    uint64_t offset;    // offset accepte par le serveur
    uint32_t payload_size;
} response_t;

#endif
