#include "csapp.h"
#include "ftp_protocol.h"

int ftp_is_safe_filename(const char *filename)
{
    size_t len = strnlen(filename, FTP_MAX_FILENAME);

    if (len == 0 || len == FTP_MAX_FILENAME) {
        return 0;
    }
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return 0;
    }
    if (strchr(filename, '/') != NULL) {
        return 0;
    }

    return 1;
}

void ftp_build_get_request(request_t *request, const char *filename)
{
    memset(request, 0, sizeof(*request));
    request->version = FTP_PROTO_VERSION;
    request->type = FTP_REQ_GET;
    request->block_size = FTP_BLOCK_SIZE;
    strncpy(request->filename, filename, FTP_MAX_FILENAME - 1);
}

int ftp_receive_request(int connfd, request_t *request)
{
    ssize_t n = Rio_readn(connfd, request, sizeof(*request));

    if (n < 0) {
        fprintf(stderr, "serverFTP: error reading request: %s\n", strerror(errno));
        return -1;
    }
    if (n == 0) {
        printf("serverFTP: client closed connection before sending a request\n");
        return 0;
    }
    if ((size_t)n != sizeof(*request)) {
        fprintf(stderr, "serverFTP: incomplete request received: %zd bytes\n", n);
        return -1;
    }

    printf("serverFTP: received request type %u\n", request->type);
    return 1;
}

void ftp_send_response(int connfd, ftp_status_t status, uint32_t type, uint64_t file_size)
{
    response_t response;

    memset(&response, 0, sizeof(response));
    response.status = (uint32_t)status;
    response.type = type;
    response.file_size = file_size;
    Rio_writen(connfd, &response, sizeof(response));
}

int ftp_receive_response(int clientfd, response_t *response)
{
    ssize_t n = Rio_readn(clientfd, response, sizeof(*response));

    if (n < 0) {
        fprintf(stderr, "clientFTP: error while reading server response: %s\n", strerror(errno));
        return -1;
    }
    if ((size_t)n != sizeof(*response)) {
        fprintf(stderr, "clientFTP: incomplete response received: %zd bytes\n", n);
        return -1;
    }

    return 0;
}

const char *ftp_status_to_string(ftp_status_t status)
{
    switch (status) {
    case FTP_STATUS_OK:
        return "OK";
    case FTP_STATUS_ERR_BAD_REQUEST:
        return "BAD_REQUEST";
    case FTP_STATUS_ERR_NOT_FOUND:
        return "NOT_FOUND";
    case FTP_STATUS_ERR_IO:
        return "IO_ERROR";
    case FTP_STATUS_ERR_AUTH_REQUIRED:
        return "AUTH_REQUIRED";
    case FTP_STATUS_ERR_AUTH_FAILED:
        return "AUTH_FAILED";
    case FTP_STATUS_ERR_UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "UNKNOWN";
    }
}
