#include "csapp.h"
#include "ftp_protocol.h"
#include "ftp_transfer.h"
#include "server_requests.h"

static int validate_request(const request_t *request, ftp_status_t *status)
{
    *status = FTP_STATUS_ERR_BAD_REQUEST;
    if (request->version != FTP_PROTO_VERSION) {
        printf("serverFTP: invalid request version: %u\n", request->version);
        return 0;
    }

    if (request->type <= FTP_REQ_INVALID || request->type > FTP_REQ_AUTH) {
        printf("serverFTP: invalid request type: %u\n", request->type);
        return 0;
    }

    if (request->type == FTP_REQ_GET || request->type == FTP_REQ_PUT || request->type == FTP_REQ_RM) {
        if (!ftp_is_safe_filename(request->filename)) {
            printf("serverFTP: invalid filename in request: '%s'\n", request->filename);
            return 0;
        }
    }

    printf("serverFTP: request is valid\n");
    return 1;
}

/*
static void handle_get_request(int connfd, const request_t *request)
{
    void *buffer = NULL;
    uint64_t file_size = 0;
    ftp_status_t status = FTP_STATUS_ERR_IO;
    if (ftp_load_file(request->filename, &buffer, &file_size, &status) < 0) {
        ftp_send_response(connfd, status, request->type, 0);
        return;
    }

    ftp_send_response(connfd, FTP_STATUS_OK, request->type, file_size);
    if (file_size > 0) {
        Rio_writen(connfd, buffer, (size_t)file_size);
    }

    Free(buffer);
}
*/

static void handle_get_request(int connfd, const request_t *request)
{
    struct stat st;
    int fd;
    char buffer[FTP_BLOCK_SIZE];
    ftp_status_t status = FTP_STATUS_ERR_IO;

    if (stat(request->filename, &st) < 0) {
        if (errno == ENOENT) {
            status = FTP_STATUS_ERR_NOT_FOUND;
        }
        ftp_send_response(connfd, status, request->type, 0);
        return;
    }

    fd = Open(request->filename, O_RDONLY, 0);
    if (fd < 0) {
        if (errno == ENOENT) {
            status = FTP_STATUS_ERR_NOT_FOUND;
        }
        ftp_send_response(connfd, status, request->type, 0);
        return;
    }

    ftp_send_response(connfd, FTP_STATUS_OK, request->type, (uint64_t)st.st_size);

    while(1){
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {   
            break;
        }
        printf("Sending packets of size %zd bytes\n", n);
        Rio_writen(connfd, buffer, (size_t)n);
    }

    Close(fd);
}




void ftp_handle_client(int connfd)
{
    request_t request;
    ftp_status_t status = FTP_STATUS_ERR_BAD_REQUEST;
    int receive_status;

    while (1) { // While pour fair e une session 
        memset(&request, 0, sizeof(request));
        status = FTP_STATUS_ERR_BAD_REQUEST;

        receive_status = ftp_receive_request(connfd, &request);
        if (receive_status <= 0) {
            return;
        }

        request.filename[FTP_MAX_FILENAME - 1] = '\0';

        if (!validate_request(&request, &status)) {
            ftp_send_response(connfd, status, request.type, 0);
            continue;
        }
        
        switch (request.type) {
        case FTP_REQ_GET:
            handle_get_request(connfd, &request);
            break;
        case FTP_REQ_BYE:
            ftp_send_response(connfd, FTP_STATUS_OK, request.type, 0);
            return;
        default:
            ftp_send_response(connfd, FTP_STATUS_ERR_UNSUPPORTED, request.type, 0);
            break;
        }
    }
}
