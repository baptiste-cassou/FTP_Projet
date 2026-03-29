#include "csapp.h"
#include "ftp_protocol.h"
#include "ftp_transfer.h"
#include "server_requests.h"

#include <dirent.h>

typedef struct {
    int authenticated;
} ftp_session_t;

static int ftp_send_status_only(int connfd, const request_t *request, ftp_status_t status)
{
    return ftp_send_response(connfd, status, request->type, 0, 0, 0);
}

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
    if (request->block_size != FTP_BLOCK_SIZE) {
        printf("serverFTP: invalid request block size: %u\n", request->block_size);
        return 0;
    }

    if (request->type == FTP_REQ_GET || request->type == FTP_REQ_PUT || request->type == FTP_REQ_RM) {
        if (!ftp_is_safe_filename(request->filename)) {
            printf("serverFTP: invalid filename in request: '%s'\n", request->filename);
            return 0;
        }
    }

    return 1;
}

static int validate_replication_request(const ftp_replication_request_t *request, ftp_status_t *status)
{
    *status = FTP_STATUS_ERR_BAD_REQUEST;
    if (request->version != FTP_PROTO_VERSION) {
        fprintf(stderr, "serverFTP: invalid replication version: %u\n", request->version);
        return 0;
    }
    if (request->type != FTP_REPL_PUT && request->type != FTP_REPL_RM) {
        fprintf(stderr, "serverFTP: invalid replication type: %u\n", request->type);
        return 0;
    }
    if (!ftp_is_safe_filename(request->filename)) {
        fprintf(stderr, "serverFTP: invalid replication filename: '%s'\n", request->filename);
        return 0;
    }

    *status = FTP_STATUS_OK;
    return 1;
}

static int build_directory_listing(char **payload, uint32_t *payload_size)
{
    DIR *dir;
    struct dirent *entry;
    char *buffer = NULL;
    size_t capacity = FTP_BLOCK_SIZE;
    size_t total = 0;

    *payload = NULL;
    *payload_size = 0;

    dir = opendir(".");
    if (dir == NULL) {
        return -1;
    }

    buffer = Malloc(capacity);
    buffer[0] = '\0';

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len;

        if (name[0] == '.') {
            continue;
        }

        len = strlen(name);
        while (total + len + 2 > capacity) {
            capacity *= 2;
            buffer = Realloc(buffer, capacity);
        }
        memcpy(buffer + total, name, len);
        total += len;
        buffer[total++] = '\n';
    }
    closedir(dir);

    buffer[total] = '\0';
    *payload = buffer;
    *payload_size = (uint32_t)total;
    return 0;
}

static int receive_local_file_payload(int connfd, const char *filename, uint64_t file_size)
{
    int received = ftp_receive_file_payload(connfd, filename, file_size, 0);

    if (received < 0 || (uint64_t)received != file_size) {
        return -1;
    }
    return 0;
}

static int remove_local_file(const char *filename, ftp_status_t *status)
{
    if (unlink(filename) < 0) {
        if (errno == ENOENT) {
            *status = FTP_STATUS_ERR_NOT_FOUND;
        } else {
            *status = FTP_STATUS_ERR_IO;
        }
        return -1;
    }

    *status = FTP_STATUS_OK;
    return 0;
}

static int replicate_to_peer(const ftp_server_context_t *context, const slave_hello_t *peer,
                             ftp_replication_type_t type, const char *filename, uint64_t file_size)
{
    ftp_replication_request_t request;
    ftp_status_t status;
    int controlfd;
    int fd = -1;

    memset(&request, 0, sizeof(request));
    request.version = FTP_PROTO_VERSION;
    request.type = (uint32_t)type;
    request.source_slave_id = (uint32_t)context->slave_id;
    request.file_size = file_size;
    strncpy(request.filename, filename, FTP_MAX_FILENAME - 1);

    controlfd = open_clientfd((char *)peer->host, (int)peer->ctrl_port);
    if (controlfd < 0) {
        fprintf(stderr, "serverFTP slave %d: unable to connect to slave %u control port %u\n",
                context->slave_id, peer->slave_id, peer->ctrl_port);
        return -1;
    }

    if (ftp_send_replication_request(controlfd, &request) < 0) {
        close(controlfd);
        return -1;
    }

    if (type == FTP_REPL_PUT) {
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "serverFTP: unable to open '%s' for replication: %s\n",
                    filename, strerror(errno));
            close(controlfd);
            return -1;
        }
        if (ftp_send_fd_payload(controlfd, fd, file_size) < 0) {
            close(fd);
            close(controlfd);
            return -1;
        }
        close(fd);
    }

    if (ftp_receive_control_reply(controlfd, &status) < 0) {
        close(controlfd);
        return -1;
    }
    close(controlfd);

    if (status != FTP_STATUS_OK) {
        fprintf(stderr, "serverFTP slave %d: slave %u rejected replicated %s '%s' with %s\n",
                context->slave_id,
                peer->slave_id,
                (type == FTP_REPL_PUT) ? "PUT" : "RM",
                filename,
                ftp_status_to_string(status));
        return -1;
    }

    printf("serverFTP slave %d replicated %s '%s' to slave %u\n",
           context->slave_id,
           (type == FTP_REPL_PUT) ? "PUT" : "RM",
           filename,
           peer->slave_id);
    return 0;
}

static int replicate_mutation(const ftp_server_context_t *context, ftp_replication_type_t type,
                              const char *filename, uint64_t file_size)
{
    uint32_t i;
    int failures = 0;

    if (context == NULL || context->cluster.slave_count == 0) {
        return 0;
    }

    for (i = 0; i < context->cluster.slave_count && i < NB_SLAVES; i++) {
        const slave_hello_t *peer = &context->cluster.slaves[i];

        if (peer->slave_id == 0 || (int)peer->slave_id == context->slave_id) {
            continue;
        }
        if (replicate_to_peer(context, peer, type, filename, file_size) < 0) {
            failures++;
        }
    }

    return (failures == 0) ? 0 : -1;
}

static int handle_auth_request(int connfd, const request_t *request, ftp_session_t *session)
{
    if (strncmp(request->login, FTP_AUTH_LOGIN, FTP_MAX_LOGIN) == 0
        && strncmp(request->password, FTP_AUTH_PASSWORD, FTP_MAX_PASSWORD) == 0) {
        session->authenticated = 1;
        return ftp_send_status_only(connfd, request, FTP_STATUS_OK);
    }

    session->authenticated = 0;
    return ftp_send_status_only(connfd, request, FTP_STATUS_ERR_AUTH_FAILED);
}

static int handle_ls_request(int connfd, const request_t *request)
{
    char *payload = NULL;
    uint32_t payload_size = 0;
    int rc;

    if (build_directory_listing(&payload, &payload_size) < 0) {
        return ftp_send_status_only(connfd, request, FTP_STATUS_ERR_IO);
    }

    rc = ftp_send_response(connfd, FTP_STATUS_OK, request->type, payload_size, 0, payload_size);
    if (rc == 0 && payload_size > 0) {
        rc = ftp_send_buffer_payload(connfd, payload, payload_size);
    }
    Free(payload);
    return rc;
}

static int handle_rm_request(int connfd, const request_t *request, const ftp_session_t *session,
                             const ftp_server_context_t *context)
{
    ftp_status_t status;

    if (!session->authenticated) {
        return ftp_send_status_only(connfd, request, FTP_STATUS_ERR_AUTH_REQUIRED);
    }

    if (remove_local_file(request->filename, &status) < 0) {
        return ftp_send_status_only(connfd, request, status);
    }

    if (replicate_mutation(context, FTP_REPL_RM, request->filename, 0) < 0) {
        fprintf(stderr, "serverFTP slave %d: replication failed for RM '%s'\n",
                (context != NULL) ? context->slave_id : -1, request->filename);
    }

    return ftp_send_status_only(connfd, request, FTP_STATUS_OK);
}

static int handle_put_request(int connfd, const request_t *request, const ftp_session_t *session,
                              const ftp_server_context_t *context)
{
    if (!session->authenticated) {
        return ftp_send_status_only(connfd, request, FTP_STATUS_ERR_AUTH_REQUIRED);
    }

    if (ftp_send_status_only(connfd, request, FTP_STATUS_OK) < 0) {
        return -1;
    }
    if (receive_local_file_payload(connfd, request->filename, request->file_size) < 0) {
        return -1;
    }

    if (replicate_mutation(context, FTP_REPL_PUT, request->filename, request->file_size) < 0) {
        fprintf(stderr, "serverFTP slave %d: replication failed for PUT '%s'\n",
                (context != NULL) ? context->slave_id : -1, request->filename);
    }

    return ftp_send_status_only(connfd, request, FTP_STATUS_OK);
}

static int handle_get_request(int connfd, const request_t *request)
{
    struct stat st;
    int fd;
    uint64_t accepted_offset = 0;
    uint64_t remaining_size;
    int rc;

    if (stat(request->filename, &st) < 0) {
        return ftp_send_status_only(connfd, request,
                                    (errno == ENOENT) ? FTP_STATUS_ERR_NOT_FOUND : FTP_STATUS_ERR_IO);
    }

    fd = open(request->filename, O_RDONLY);
    if (fd < 0) {
        return ftp_send_status_only(connfd, request,
                                    (errno == ENOENT) ? FTP_STATUS_ERR_NOT_FOUND : FTP_STATUS_ERR_IO);
    }

    if (request->offset > 0 && request->offset <= (uint64_t)st.st_size) {
        accepted_offset = request->offset;
        if (lseek(fd, (off_t)accepted_offset, SEEK_SET) < 0) {
            rc = ftp_send_response(connfd, FTP_STATUS_ERR_IO, request->type, (uint64_t)st.st_size, 0, 0);
            Close(fd);
            return rc;
        }
        remaining_size = (uint64_t)st.st_size - accepted_offset;
        rc = ftp_send_response(connfd, FTP_STATUS_RESTART, request->type,
                               (uint64_t)st.st_size, accepted_offset, (uint32_t)remaining_size);
    } else {
        remaining_size = (uint64_t)st.st_size;
        rc = ftp_send_response(connfd, FTP_STATUS_OK, request->type,
                               (uint64_t)st.st_size, 0, (uint32_t)remaining_size);
    }
    if (rc < 0) {
        Close(fd);
        return -1;
    }

    rc = ftp_send_fd_payload(connfd, fd, remaining_size);
    if (rc < 0) {
        fprintf(stderr, "serverFTP: failed while sending '%s'\n", request->filename);
    }

    Close(fd);
    return rc;
}

static int handle_replicated_rm(const ftp_replication_request_t *request)
{
    ftp_status_t status;

    if (unlink(request->filename) < 0 && errno != ENOENT) {
        status = FTP_STATUS_ERR_IO;
    } else {
        status = FTP_STATUS_OK;
        printf("serverFTP: applied replicated RM '%s' from slave %u\n",
               request->filename, request->source_slave_id);
    }

    return status;
}

static int handle_replicated_put(int connfd, const ftp_replication_request_t *request)
{
    if (receive_local_file_payload(connfd, request->filename, request->file_size) < 0) {
        return FTP_STATUS_ERR_IO;
    }

    printf("serverFTP: applied replicated PUT '%s' from slave %u\n",
           request->filename, request->source_slave_id);
    return FTP_STATUS_OK;
}

int ftp_handle_replication_connection(int connfd)
{
    ftp_replication_request_t request;
    ftp_status_t status;

    if (ftp_receive_replication_request(connfd, &request) < 0) {
        return -1;
    }
    if (!validate_replication_request(&request, &status)) {
        ftp_send_control_reply(connfd, status);
        return -1;
    }

    switch ((ftp_replication_type_t)request.type) {
    case FTP_REPL_PUT:
        status = (ftp_status_t)handle_replicated_put(connfd, &request);
        break;
    case FTP_REPL_RM:
        status = (ftp_status_t)handle_replicated_rm(&request);
        break;
    default:
        status = FTP_STATUS_ERR_BAD_REQUEST;
        break;
    }

    if (ftp_send_control_reply(connfd, status) < 0) {
        return -1;
    }
    return (status == FTP_STATUS_OK) ? 0 : -1;
}

void ftp_handle_client(int connfd, const ftp_server_context_t *context)
{
    request_t request;
    ftp_status_t status = FTP_STATUS_ERR_BAD_REQUEST;
    ftp_session_t session;
    int receive_status;

    memset(&session, 0, sizeof(session));

    while (1) {
        memset(&request, 0, sizeof(request));
        status = FTP_STATUS_ERR_BAD_REQUEST;

        receive_status = ftp_receive_request(connfd, &request);
        if (receive_status <= 0) {
            return;
        }

        request.filename[FTP_MAX_FILENAME - 1] = '\0';
        request.login[FTP_MAX_LOGIN - 1] = '\0';
        request.password[FTP_MAX_PASSWORD - 1] = '\0';

        if (!validate_request(&request, &status)) {
            if (ftp_send_status_only(connfd, &request, status) < 0) {
                return;
            }
            continue;
        }

        switch (request.type) {
        case FTP_REQ_GET:
            if (handle_get_request(connfd, &request) < 0) {
                return;
            }
            break;
        case FTP_REQ_PUT:
            if (handle_put_request(connfd, &request, &session, context) < 0) {
                return;
            }
            break;
        case FTP_REQ_LS:
            if (handle_ls_request(connfd, &request) < 0) {
                return;
            }
            break;
        case FTP_REQ_RM:
            if (handle_rm_request(connfd, &request, &session, context) < 0) {
                return;
            }
            break;
        case FTP_REQ_AUTH:
            if (handle_auth_request(connfd, &request, &session) < 0) {
                return;
            }
            break;
        case FTP_REQ_BYE:
            ftp_send_status_only(connfd, &request, FTP_STATUS_OK);
            return;
        default:
            if (ftp_send_status_only(connfd, &request, FTP_STATUS_ERR_UNSUPPORTED) < 0) {
                return;
            }
            break;
        }
    }
}
