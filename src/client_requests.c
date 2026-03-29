#include "csapp.h"
#include "client_requests.h"
#include "ftp_protocol.h"
#include "ftp_transfer.h"
#include "utils.h"

static void ftp_reset_stats(ftp_transfer_stats_t *stats)
{
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
}

static void ftp_finalize_stats(ftp_transfer_stats_t *stats, const struct timeval *start, const struct timeval *end, uint64_t bytes)
{
    if (stats == NULL) {
        return;
    }

    stats->bytes_transferred = bytes;
    stats->seconds = (double)(end->tv_sec - start->tv_sec)
        + (double)(end->tv_usec - start->tv_usec) / 1000000.0;
    if (stats->seconds <= 0.0) {
        stats->seconds = 0.000001;
    }
}

static int ftp_receive_checked_response(int clientfd, const request_t *request, response_t *response)
{
    if (ftp_receive_response(clientfd, response) < 0) {
        return -2;
    }
    if (response->type != request->type) {
        fprintf(stderr, "clientFTP: unexpected response type %u\n", response->type);
        return -2;
    }
    return 0;
}

static int ftp_expect_ok_status(const response_t *response, const char *context)
{
    if (response->status != FTP_STATUS_OK) {
        fprintf(stderr, "clientFTP: server returned %s for %s\n",
                ftp_status_to_string((ftp_status_t)response->status), context);
        return -1;
    }
    return 0;
}

static int ftp_open_local_upload(const char *filename, uint64_t *file_size)
{
    struct stat st;
    int fd;

    if (!ftp_is_safe_filename(filename)) {
        fprintf(stderr, "clientFTP: invalid filename '%s'\n", filename);
        return -1;
    }
    if (stat(filename, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        fprintf(stderr, "clientFTP: unable to stat '%s': %s\n", filename, strerror(errno));
        return -1;
    }

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "clientFTP: unable to open '%s': %s\n", filename, strerror(errno));
        return -1;
    }

    *file_size = (uint64_t)st.st_size;
    return fd;
}

int ftp_client_get(int clientfd, const char *filename, ftp_transfer_stats_t *stats)
{
    request_t request;
    response_t response;
    struct timeval start;
    struct timeval end;
    uint64_t offset;
    int received;

    if (stats == NULL) {
        fprintf(stderr, "clientFTP: missing transfer stats buffer\n");
        return -1;
    }

    ftp_reset_stats(stats);
    if (!ftp_is_safe_filename(filename)) {
        fprintf(stderr, "clientFTP: invalid filename '%s'\n", filename);
        return -1;
    }

    offset = check_size_file(filename);
    ftp_build_get_request(&request, filename, offset);
    gettimeofday(&start, NULL);
    if (ftp_send_request(clientfd, &request) < 0) {
        return -2;
    }

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        return -2;
    }
    if (response.status != FTP_STATUS_OK && response.status != FTP_STATUS_RESTART) {
        fprintf(stderr, "clientFTP: server returned %s for '%s'\n",
                ftp_status_to_string((ftp_status_t)response.status), filename);
        return -1;
    }

    received = ftp_receive_file_payload(clientfd, filename, response.file_size, response.offset);
    if (received < 0) {
        return -2;
    }

    gettimeofday(&end, NULL);
    ftp_finalize_stats(stats, &start, &end, (uint64_t)received);
    return 0;
}

int ftp_client_put(int clientfd, const char *filename, ftp_transfer_stats_t *stats)
{
    request_t request;
    response_t response;
    struct timeval start;
    struct timeval end;
    uint64_t file_size = 0;
    int fd;

    if (stats == NULL) {
        fprintf(stderr, "clientFTP: missing transfer stats buffer\n");
        return -1;
    }

    ftp_reset_stats(stats);
    fd = ftp_open_local_upload(filename, &file_size);
    if (fd < 0) {
        return -1;
    }

    ftp_build_put_request(&request, filename, file_size);
    gettimeofday(&start, NULL);
    if (ftp_send_request(clientfd, &request) < 0) {
        Close(fd);
        return -2;
    }

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        Close(fd);
        return -2;
    }
    if (ftp_expect_ok_status(&response, "PUT preflight") < 0) {
        Close(fd);
        return -1;
    }

    if (ftp_send_fd_payload(clientfd, fd, file_size) < 0) {
        Close(fd);
        return -2;
    }
    Close(fd);

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        return -2;
    }
    if (ftp_expect_ok_status(&response, filename) < 0) {
        return -1;
    }

    gettimeofday(&end, NULL);
    ftp_finalize_stats(stats, &start, &end, file_size);
    return 0;
}

int ftp_client_ls(int clientfd, char **listing)
{
    request_t request;
    response_t response;

    if (listing == NULL) {
        return -1;
    }

    *listing = NULL;
    ftp_init_request(&request, FTP_REQ_LS);
    if (ftp_send_request(clientfd, &request) < 0) {
        return -2;
    }

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        return -2;
    }
    if (ftp_expect_ok_status(&response, "LS") < 0) {
        return -1;
    }

    if (ftp_receive_buffer_payload(clientfd, response.payload_size, listing) < 0) {
        return -2;
    }

    return 0;
}

int ftp_client_rm(int clientfd, const char *filename)
{
    request_t request;
    response_t response;

    if (!ftp_is_safe_filename(filename)) {
        fprintf(stderr, "clientFTP: invalid filename '%s'\n", filename);
        return -1;
    }

    ftp_build_named_request(&request, FTP_REQ_RM, filename);
    if (ftp_send_request(clientfd, &request) < 0) {
        return -2;
    }

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        return -2;
    }
    return ftp_expect_ok_status(&response, filename);
}

int ftp_client_auth(int clientfd, const char *login, const char *password)
{
    request_t request;
    response_t response;

    ftp_build_auth_request(&request, login, password);
    if (ftp_send_request(clientfd, &request) < 0) {
        return -2;
    }

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        return -2;
    }
    return ftp_expect_ok_status(&response, "AUTH");
}

int ftp_client_bye(int clientfd)
{
    request_t request;
    response_t response;

    ftp_init_request(&request, FTP_REQ_BYE);
    if (ftp_send_request(clientfd, &request) < 0) {
        return -1;
    }

    if (ftp_receive_checked_response(clientfd, &request, &response) < 0) {
        return -1;
    }
    if (response.status != FTP_STATUS_OK) {
        fprintf(stderr, "clientFTP: server returned %s for BYE\n",
                ftp_status_to_string((ftp_status_t)response.status));
        return -1;
    }

    return 0;
}
