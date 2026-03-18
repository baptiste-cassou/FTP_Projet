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

    if (request->type != FTP_REQ_GET) {
        if (request->type > FTP_REQ_INVALID && request->type <= FTP_REQ_AUTH) {
            *status = FTP_STATUS_ERR_UNSUPPORTED;
        }
        printf("serverFTP: invalid request type: %u\n", request->type);
        return 0;
    }

    if (!ftp_is_safe_filename(request->filename)) {
        printf("serverFTP: invalid filename in request: '%s'\n", request->filename);
        return 0;
    }

    printf("serverFTP: request is valid\n");
    return 1;
}


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

void ftp_handle_client(int connfd)
{
    request_t request;
    ftp_status_t status = FTP_STATUS_ERR_BAD_REQUEST;
    memset(&request, 0, sizeof(request));
    if (ftp_receive_request(connfd, &request) <= 0) {
        return;
    }

    request.filename[FTP_MAX_FILENAME - 1] = '\0';
    if (!validate_request(&request, &status)) {
        ftp_send_response(connfd, status, request.type, 0);
        return;
    }

    switch (request.type) {
    case FTP_REQ_GET:
        handle_get_request(connfd, &request);
        return;
    default:
        ftp_send_response(connfd, FTP_STATUS_ERR_UNSUPPORTED, request.type, 0);
        return;
    }
}
