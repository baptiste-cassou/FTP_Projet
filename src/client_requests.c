#include "csapp.h"
#include "client_requests.h"
#include "ftp_protocol.h"
#include "ftp_transfer.h"

int ftp_client_get(int clientfd, const char *filename, ftp_transfer_stats_t *stats)
{
    request_t request;
    response_t response;
    struct timeval start;
    struct timeval end;

    if (stats == NULL) {
        fprintf(stderr, "clientFTP: missing transfer stats buffer\n");
        return -1;
    }

    memset(stats, 0, sizeof(*stats));
    if (!ftp_is_safe_filename(filename)) {
        fprintf(stderr, "clientFTP: invalid filename '%s'\n", filename);
        return -1;
    }

    ftp_build_get_request(&request, filename);
    gettimeofday(&start, NULL);
    Rio_writen(clientfd, &request, sizeof(request));

    if (ftp_receive_response(clientfd, &response) < 0) {
        return -1;
    }
    if (response.type != request.type) {
        fprintf(stderr, "clientFTP: unexpected response type %u\n", response.type);
        return -1;
    }
    if (response.status != FTP_STATUS_OK) {
        fprintf(stderr, "clientFTP: server returned %s for '%s'\n",
                ftp_status_to_string((ftp_status_t)response.status), filename);
        return -1;
    }
    if (ftp_receive_file_payload(clientfd, filename, response.file_size) < 0) {
        return -1;
    }

    gettimeofday(&end, NULL);
    stats->bytes_received = response.file_size;
    stats->seconds = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1000000.0;
    if (stats->seconds <= 0.0) {
        stats->seconds = 0.000001;
    }

    return 0;
}
