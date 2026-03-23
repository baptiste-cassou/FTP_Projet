// Definitions communes du protocole FTP minimal.
#ifndef FTP_SHARED_H
#define FTP_SHARED_H

#include <stdint.h>
 
// Gerer les esclaves
#define NB_SLAVES 2
#define FTP_SLAVE_CLIENT_BASE_PORT 3001 // Port de base = le client 3001 va controler 4001 (slave 1)
#define FTP_SLAVE_CTRL_BASE_PORT 4001   // le client 3002 va controler 4002 (slave 2) etc)

// Helpers pour les ports des esclaves
#define FTP_SLAVE_CLIENT_PORT(id) (FTP_SLAVE_CLIENT_BASE_PORT + (id) - 1)
#define FTP_SLAVE_CTRL_PORT(id)   (FTP_SLAVE_CTRL_BASE_PORT + (id) - 1)

// Parametres globaux du serveur et du protocole.
#define FTP_MASTER_PORT 2121           // A modifier pour inclure les nouveaux ports et le serverMASTER etc
#define FTP_PROTO_VERSION 1
#define FTP_BLOCK_SIZE 4096
#define NB_PROC 1
#define FTP_SERVER_DATA_DIR "data_server"
#define FTP_CLIENT_DATA_DIR "data_client"

// Tailles maximales des champs texte transportes sur le reseau.
#define FTP_MAX_FILENAME 256
#define FTP_MAX_LOGIN 32
#define FTP_MAX_PASSWORD 64

// Types de requetes supportes par le protocole.
typedef enum {
    FTP_REQ_INVALID = 0,
    FTP_REQ_GET = 1,
    FTP_REQ_PUT = 2,
    FTP_REQ_LS = 3,
    FTP_REQ_RM = 4,
    FTP_REQ_BYE = 5,
    FTP_REQ_AUTH = 6
} typereq_t;

// Codes de retour renvoyes par le serveur au client.
typedef enum {
    FTP_STATUS_OK = 0,
    FTP_STATUS_ERR_BAD_REQUEST = 1,
    FTP_STATUS_ERR_NOT_FOUND = 2,
    FTP_STATUS_ERR_IO = 3,
    FTP_STATUS_ERR_AUTH_REQUIRED = 4,
    FTP_STATUS_ERR_AUTH_FAILED = 5,
    FTP_STATUS_ERR_UNSUPPORTED = 6,
    FTP_STATUS_RESTART = 7
} ftp_status_t;

// Requete binaire envoyee par le client vers le serveur.
typedef struct {
    uint32_t version;
    uint32_t type;      // typereq_t
    uint64_t offset;    // reprise de transfert
    uint32_t block_size;
    char filename[FTP_MAX_FILENAME];
    char login[FTP_MAX_LOGIN];
    char password[FTP_MAX_PASSWORD];
} request_t;

// Reponse binaire renvoyee par le serveur au client.
typedef struct {
    uint32_t status;    // ftp_status_t
    uint32_t type;      // typereq_t reponse associee
    uint64_t file_size; // taille totale du fichier concerne
    uint64_t offset;    // offset accepte par le serveur
    uint32_t payload_size;
} response_t;



#endif
