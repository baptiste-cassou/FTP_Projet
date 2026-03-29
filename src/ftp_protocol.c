#include "csapp.h"
#include "ftp_protocol.h"

enum {
    FTP_REQUEST_WIRE_SIZE = 4u + 4u + 8u + 8u + 4u + FTP_MAX_FILENAME + FTP_MAX_LOGIN + FTP_MAX_PASSWORD,
    FTP_RESPONSE_WIRE_SIZE = 4u + 4u + 8u + 8u + 4u,
    FTP_SLAVE_HELLO_WIRE_SIZE = 4u + 4u + 4u + 4u + FTP_MAX_HOST,
    FTP_CLUSTER_WIRE_SIZE = 4u + 4u + (NB_SLAVES * FTP_SLAVE_HELLO_WIRE_SIZE),
    FTP_REPLICATION_REQUEST_WIRE_SIZE = 4u + 4u + 4u + 4u + 8u + FTP_MAX_FILENAME,
    FTP_CONTROL_REPLY_WIRE_SIZE = 4u + 4u
};

static int ftp_write_exact(int fd, const void *buffer, size_t size, const char *context)
{
    ssize_t n = rio_writen(fd, (void *)buffer, size);

    if (n < 0) {
        fprintf(stderr, "%s: write failed: %s\n", context, strerror(errno));
        return -1;
    }
    if ((size_t)n != size) {
        fprintf(stderr, "%s: short write: %zd/%zu bytes\n", context, n, size);
        return -1;
    }

    return 0;
}

static int ftp_read_message(int fd, void *buffer, size_t size, const char *context, int allow_eof)
{
    ssize_t n = rio_readn(fd, buffer, size);

    if (n < 0) {
        fprintf(stderr, "%s: read failed: %s\n", context, strerror(errno));
        return -1;
    }
    if (n == 0 && allow_eof) {
        return 0;
    }
    if ((size_t)n != size) {
        fprintf(stderr, "%s: incomplete message received: %zd/%zu bytes\n", context, n, size);
        return -1;
    }

    return 1;
}

static uint32_t ftp_bswap32(uint32_t value)
{
    return ((value & 0x000000FFU) << 24)
        | ((value & 0x0000FF00U) << 8)
        | ((value & 0x00FF0000U) >> 8)
        | ((value & 0xFF000000U) >> 24);
}

static uint64_t ftp_bswap64(uint64_t value)
{
    return ((value & 0x00000000000000FFULL) << 56)
        | ((value & 0x000000000000FF00ULL) << 40)
        | ((value & 0x0000000000FF0000ULL) << 24)
        | ((value & 0x00000000FF000000ULL) << 8)
        | ((value & 0x000000FF00000000ULL) >> 8)
        | ((value & 0x0000FF0000000000ULL) >> 24)
        | ((value & 0x00FF000000000000ULL) >> 40)
        | ((value & 0xFF00000000000000ULL) >> 56);
}

static uint64_t ftp_htonll(uint64_t value)
{
    static const uint16_t probe = 0x0102;

    if (*(const unsigned char *)&probe == 0x01) {
        return value;
    }

    return ftp_bswap64(value);
}

static uint64_t ftp_ntohll(uint64_t value)
{
    return ftp_htonll(value);
}

static int ftp_is_valid_request_type(uint32_t type)
{
    return type > FTP_REQ_INVALID && type <= FTP_REQ_AUTH;
}

static int ftp_is_valid_status(uint32_t status)
{
    return status <= FTP_STATUS_RESTART;
}

static int ftp_is_valid_replication_type(uint32_t type)
{
    return type == FTP_REPL_PUT || type == FTP_REPL_RM;
}

static int ftp_u32_looks_host_endian(uint32_t value, uint32_t expected)
{
    return value != expected && ftp_bswap32(value) == expected;
}

static int ftp_u32_range_looks_host_endian(uint32_t value, uint32_t min_valid, uint32_t max_valid)
{
    uint32_t swapped = ftp_bswap32(value);

    return swapped != value && swapped >= min_valid && swapped <= max_valid;
}

static int ftp_request_type_looks_host_endian(uint32_t type)
{
    return ftp_u32_range_looks_host_endian(type, FTP_REQ_GET, FTP_REQ_AUTH);
}

static int ftp_status_looks_host_endian(uint32_t status)
{
    return ftp_u32_range_looks_host_endian(status, FTP_STATUS_OK, FTP_STATUS_RESTART);
}

static int ftp_replication_type_looks_host_endian(uint32_t type)
{
    return ftp_u32_range_looks_host_endian(type, FTP_REPL_PUT, FTP_REPL_RM);
}

static void ftp_log_endianness_hint(const char *context, const char *field)
{
    fprintf(stderr, "%s: invalid %s, probable host-endian message (expected network byte order)\n",
            context, field);
}

static int ftp_validate_request_message(const request_t *request, const char *context)
{
    if (request->version != FTP_PROTO_VERSION) {
        if (ftp_u32_looks_host_endian(request->version, FTP_PROTO_VERSION)) {
            ftp_log_endianness_hint(context, "request version");
        } else {
            fprintf(stderr, "%s: invalid request version %u\n", context, request->version);
        }
        return -1;
    }
    if (!ftp_is_valid_request_type(request->type)) {
        if (ftp_request_type_looks_host_endian(request->type)) {
            ftp_log_endianness_hint(context, "request type");
        } else {
            fprintf(stderr, "%s: invalid request type %u\n", context, request->type);
        }
        return -1;
    }
    if (request->block_size != FTP_BLOCK_SIZE) {
        if (ftp_u32_looks_host_endian(request->block_size, FTP_BLOCK_SIZE)) {
            ftp_log_endianness_hint(context, "request block size");
        } else {
            fprintf(stderr, "%s: invalid request block size %u\n", context, request->block_size);
        }
        return -1;
    }

    return 0;
}

static int ftp_validate_response_message(const response_t *response, const char *context)
{
    if (!ftp_is_valid_status(response->status)) {
        if (ftp_status_looks_host_endian(response->status)) {
            ftp_log_endianness_hint(context, "response status");
        } else {
            fprintf(stderr, "%s: invalid response status %u\n", context, response->status);
        }
        return -1;
    }
    if (!ftp_is_valid_request_type(response->type)) {
        if (ftp_request_type_looks_host_endian(response->type)) {
            ftp_log_endianness_hint(context, "response type");
        } else {
            fprintf(stderr, "%s: invalid response type %u\n", context, response->type);
        }
        return -1;
    }
    if (response->offset > response->file_size) {
        fprintf(stderr, "%s: invalid response offset %" PRIu64 " for file size %" PRIu64 "\n",
                context, response->offset, response->file_size);
        return -1;
    }
    if ((uint64_t)response->payload_size > response->file_size) {
        fprintf(stderr, "%s: invalid response payload size %u for file size %" PRIu64 "\n",
                context, response->payload_size, response->file_size);
        return -1;
    }

    return 0;
}

static int ftp_validate_slave_hello_message(const slave_hello_t *hello, const char *context)
{
    if (hello->version != FTP_PROTO_VERSION) {
        if (ftp_u32_looks_host_endian(hello->version, FTP_PROTO_VERSION)) {
            ftp_log_endianness_hint(context, "slave hello version");
        } else {
            fprintf(stderr, "%s: invalid slave hello version %u\n", context, hello->version);
        }
        return -1;
    }
    if (hello->slave_id == 0 || hello->slave_id > NB_SLAVES) {
        if (ftp_u32_range_looks_host_endian(hello->slave_id, 1, NB_SLAVES)) {
            ftp_log_endianness_hint(context, "slave id");
        } else {
            fprintf(stderr, "%s: invalid slave id %u\n", context, hello->slave_id);
        }
        return -1;
    }
    if (hello->client_port == 0 || hello->ctrl_port == 0) {
        fprintf(stderr, "%s: invalid slave ports client=%u control=%u\n",
                context, hello->client_port, hello->ctrl_port);
        return -1;
    }
    if (hello->host[0] == '\0') {
        fprintf(stderr, "%s: empty slave host\n", context);
        return -1;
    }

    return 0;
}

static int ftp_validate_cluster_message(const slave_cluster_t *cluster, const char *context)
{
    uint32_t i;

    if (cluster->version != FTP_PROTO_VERSION) {
        if (ftp_u32_looks_host_endian(cluster->version, FTP_PROTO_VERSION)) {
            ftp_log_endianness_hint(context, "cluster version");
        } else {
            fprintf(stderr, "%s: invalid cluster version %u\n", context, cluster->version);
        }
        return -1;
    }
    if (cluster->slave_count != NB_SLAVES) {
        if (ftp_u32_looks_host_endian(cluster->slave_count, NB_SLAVES)) {
            ftp_log_endianness_hint(context, "cluster slave count");
        } else {
            fprintf(stderr, "%s: invalid cluster slave count %u\n", context, cluster->slave_count);
        }
        return -1;
    }
    for (i = 0; i < NB_SLAVES; i++) {
        if (ftp_validate_slave_hello_message(&cluster->slaves[i], context) < 0) {
            return -1;
        }
    }

    return 0;
}

static int ftp_validate_replication_request_message(const ftp_replication_request_t *request, const char *context)
{
    if (request->version != FTP_PROTO_VERSION) {
        if (ftp_u32_looks_host_endian(request->version, FTP_PROTO_VERSION)) {
            ftp_log_endianness_hint(context, "replication version");
        } else {
            fprintf(stderr, "%s: invalid replication version %u\n", context, request->version);
        }
        return -1;
    }
    if (!ftp_is_valid_replication_type(request->type)) {
        if (ftp_replication_type_looks_host_endian(request->type)) {
            ftp_log_endianness_hint(context, "replication type");
        } else {
            fprintf(stderr, "%s: invalid replication type %u\n", context, request->type);
        }
        return -1;
    }
    if (request->source_slave_id == 0 || request->source_slave_id > NB_SLAVES) {
        if (ftp_u32_range_looks_host_endian(request->source_slave_id, 1, NB_SLAVES)) {
            ftp_log_endianness_hint(context, "replication source slave id");
        } else {
            fprintf(stderr, "%s: invalid replication source slave id %u\n",
                    context, request->source_slave_id);
        }
        return -1;
    }

    return 0;
}

static int ftp_validate_control_reply_message(uint32_t version, ftp_status_t status, const char *context)
{
    if (version != FTP_PROTO_VERSION) {
        if (ftp_u32_looks_host_endian(version, FTP_PROTO_VERSION)) {
            ftp_log_endianness_hint(context, "control reply version");
        } else {
            fprintf(stderr, "%s: invalid control reply version %u\n", context, version);
        }
        return -1;
    }
    if (!ftp_is_valid_status((uint32_t)status)) {
        if (ftp_status_looks_host_endian((uint32_t)status)) {
            ftp_log_endianness_hint(context, "control reply status");
        } else {
            fprintf(stderr, "%s: invalid control reply status %u\n", context, (uint32_t)status);
        }
        return -1;
    }

    return 0;
}

static void ftp_encode_u32(unsigned char **cursor, uint32_t value)
{
    uint32_t network = htonl(value);

    memcpy(*cursor, &network, sizeof(network));
    *cursor += sizeof(network);
}

static void ftp_encode_u64(unsigned char **cursor, uint64_t value)
{
    uint64_t network = ftp_htonll(value);

    memcpy(*cursor, &network, sizeof(network));
    *cursor += sizeof(network);
}

static void ftp_encode_bytes(unsigned char **cursor, const void *bytes, size_t size)
{
    memcpy(*cursor, bytes, size);
    *cursor += size;
}

static uint32_t ftp_decode_u32(const unsigned char **cursor)
{
    uint32_t network;

    memcpy(&network, *cursor, sizeof(network));
    *cursor += sizeof(network);
    return ntohl(network);
}

static uint64_t ftp_decode_u64(const unsigned char **cursor)
{
    uint64_t network;

    memcpy(&network, *cursor, sizeof(network));
    *cursor += sizeof(network);
    return ftp_ntohll(network);
}

static void ftp_decode_bytes(const unsigned char **cursor, void *bytes, size_t size)
{
    memcpy(bytes, *cursor, size);
    *cursor += size;
}

static void ftp_encode_slave_hello(unsigned char **cursor, const slave_hello_t *hello)
{
    ftp_encode_u32(cursor, hello->version);
    ftp_encode_u32(cursor, hello->slave_id);
    ftp_encode_u32(cursor, hello->client_port);
    ftp_encode_u32(cursor, hello->ctrl_port);
    ftp_encode_bytes(cursor, hello->host, FTP_MAX_HOST);
}

static void ftp_decode_slave_hello(const unsigned char **cursor, slave_hello_t *hello)
{
    memset(hello, 0, sizeof(*hello));
    hello->version = ftp_decode_u32(cursor);
    hello->slave_id = ftp_decode_u32(cursor);
    hello->client_port = ftp_decode_u32(cursor);
    hello->ctrl_port = ftp_decode_u32(cursor);
    ftp_decode_bytes(cursor, hello->host, FTP_MAX_HOST);
    hello->host[FTP_MAX_HOST - 1] = '\0';
}

static void ftp_encode_request(unsigned char *buffer, const request_t *request)
{
    unsigned char *cursor = buffer;

    ftp_encode_u32(&cursor, request->version);
    ftp_encode_u32(&cursor, request->type);
    ftp_encode_u64(&cursor, request->offset);
    ftp_encode_u64(&cursor, request->file_size);
    ftp_encode_u32(&cursor, request->block_size);
    ftp_encode_bytes(&cursor, request->filename, FTP_MAX_FILENAME);
    ftp_encode_bytes(&cursor, request->login, FTP_MAX_LOGIN);
    ftp_encode_bytes(&cursor, request->password, FTP_MAX_PASSWORD);
}

static void ftp_decode_request(const unsigned char *buffer, request_t *request)
{
    const unsigned char *cursor = buffer;

    memset(request, 0, sizeof(*request));
    request->version = ftp_decode_u32(&cursor);
    request->type = ftp_decode_u32(&cursor);
    request->offset = ftp_decode_u64(&cursor);
    request->file_size = ftp_decode_u64(&cursor);
    request->block_size = ftp_decode_u32(&cursor);
    ftp_decode_bytes(&cursor, request->filename, FTP_MAX_FILENAME);
    ftp_decode_bytes(&cursor, request->login, FTP_MAX_LOGIN);
    ftp_decode_bytes(&cursor, request->password, FTP_MAX_PASSWORD);
    request->filename[FTP_MAX_FILENAME - 1] = '\0';
    request->login[FTP_MAX_LOGIN - 1] = '\0';
    request->password[FTP_MAX_PASSWORD - 1] = '\0';
}

static void ftp_encode_response(unsigned char *buffer, const response_t *response)
{
    unsigned char *cursor = buffer;

    ftp_encode_u32(&cursor, response->status);
    ftp_encode_u32(&cursor, response->type);
    ftp_encode_u64(&cursor, response->file_size);
    ftp_encode_u64(&cursor, response->offset);
    ftp_encode_u32(&cursor, response->payload_size);
}

static void ftp_decode_response(const unsigned char *buffer, response_t *response)
{
    const unsigned char *cursor = buffer;

    memset(response, 0, sizeof(*response));
    response->status = ftp_decode_u32(&cursor);
    response->type = ftp_decode_u32(&cursor);
    response->file_size = ftp_decode_u64(&cursor);
    response->offset = ftp_decode_u64(&cursor);
    response->payload_size = ftp_decode_u32(&cursor);
}

static void ftp_encode_cluster(unsigned char *buffer, const slave_cluster_t *cluster)
{
    unsigned char *cursor = buffer;
    uint32_t i;

    ftp_encode_u32(&cursor, cluster->version);
    ftp_encode_u32(&cursor, cluster->slave_count);
    for (i = 0; i < NB_SLAVES; i++) {
        ftp_encode_slave_hello(&cursor, &cluster->slaves[i]);
    }
}

static void ftp_decode_cluster(const unsigned char *buffer, slave_cluster_t *cluster)
{
    const unsigned char *cursor = buffer;
    uint32_t i;

    memset(cluster, 0, sizeof(*cluster));
    cluster->version = ftp_decode_u32(&cursor);
    cluster->slave_count = ftp_decode_u32(&cursor);
    for (i = 0; i < NB_SLAVES; i++) {
        ftp_decode_slave_hello(&cursor, &cluster->slaves[i]);
    }
}

static void ftp_encode_replication_request(unsigned char *buffer, const ftp_replication_request_t *request)
{
    unsigned char *cursor = buffer;

    ftp_encode_u32(&cursor, request->version);
    ftp_encode_u32(&cursor, request->type);
    ftp_encode_u32(&cursor, request->source_slave_id);
    ftp_encode_u32(&cursor, request->reserved);
    ftp_encode_u64(&cursor, request->file_size);
    ftp_encode_bytes(&cursor, request->filename, FTP_MAX_FILENAME);
}

static void ftp_decode_replication_request(const unsigned char *buffer, ftp_replication_request_t *request)
{
    const unsigned char *cursor = buffer;

    memset(request, 0, sizeof(*request));
    request->version = ftp_decode_u32(&cursor);
    request->type = ftp_decode_u32(&cursor);
    request->source_slave_id = ftp_decode_u32(&cursor);
    request->reserved = ftp_decode_u32(&cursor);
    request->file_size = ftp_decode_u64(&cursor);
    ftp_decode_bytes(&cursor, request->filename, FTP_MAX_FILENAME);
    request->filename[FTP_MAX_FILENAME - 1] = '\0';
}

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

void ftp_init_request(request_t *request, typereq_t type)
{
    memset(request, 0, sizeof(*request));
    request->version = FTP_PROTO_VERSION;
    request->type = (uint32_t)type;
    request->block_size = FTP_BLOCK_SIZE;
}

void ftp_build_get_request(request_t *request, const char *filename, uint64_t offset)
{
    ftp_init_request(request, FTP_REQ_GET);
    request->offset = offset;
    strncpy(request->filename, filename, FTP_MAX_FILENAME - 1);
}

void ftp_build_put_request(request_t *request, const char *filename, uint64_t file_size)
{
    ftp_init_request(request, FTP_REQ_PUT);
    request->file_size = file_size;
    strncpy(request->filename, filename, FTP_MAX_FILENAME - 1);
}

void ftp_build_named_request(request_t *request, typereq_t type, const char *filename)
{
    ftp_init_request(request, type);
    strncpy(request->filename, filename, FTP_MAX_FILENAME - 1);
}

void ftp_build_auth_request(request_t *request, const char *login, const char *password)
{
    ftp_init_request(request, FTP_REQ_AUTH);
    strncpy(request->login, login, FTP_MAX_LOGIN - 1);
    strncpy(request->password, password, FTP_MAX_PASSWORD - 1);
}

int ftp_send_request(int connfd, const request_t *request)
{
    unsigned char buffer[FTP_REQUEST_WIRE_SIZE];

    ftp_encode_request(buffer, request);
    return ftp_write_exact(connfd, buffer, sizeof(buffer), "ftp protocol request");
}

int ftp_receive_request(int connfd, request_t *request)
{
    unsigned char buffer[FTP_REQUEST_WIRE_SIZE];
    int status = ftp_read_message(connfd, buffer, sizeof(buffer), "serverFTP request", 1);

    if (status <= 0) {
        if (status == 0) {
            printf("serverFTP: client closed connection before sending a request\n");
        }
        return status;
    }

    ftp_decode_request(buffer, request);
    ftp_validate_request_message(request, "serverFTP request");
    printf("serverFTP: received request type %u\n", request->type);
    return 1;
}

int ftp_send_response(int connfd, ftp_status_t status, uint32_t type, uint64_t file_size, uint64_t offset, uint32_t payload_size)
{
    unsigned char buffer[FTP_RESPONSE_WIRE_SIZE];
    response_t response;

    memset(&response, 0, sizeof(response));
    response.status = (uint32_t)status;
    response.type = type;
    response.file_size = file_size;
    response.offset = offset;
    response.payload_size = payload_size;
    ftp_encode_response(buffer, &response);
    return ftp_write_exact(connfd, buffer, sizeof(buffer), "ftp protocol response");
}

int ftp_receive_response(int clientfd, response_t *response)
{
    unsigned char buffer[FTP_RESPONSE_WIRE_SIZE];

    if (ftp_read_message(clientfd, buffer, sizeof(buffer), "clientFTP response", 0) <= 0) {
        return -1;
    }

    ftp_decode_response(buffer, response);
    if (ftp_validate_response_message(response, "clientFTP response") < 0) {
        return -1;
    }
    return 0;
}

int ftp_send_slave_hello(int connfd, const slave_hello_t *hello)
{
    unsigned char buffer[FTP_SLAVE_HELLO_WIRE_SIZE];
    unsigned char *cursor = buffer;

    ftp_encode_slave_hello(&cursor, hello);
    return ftp_write_exact(connfd, buffer, sizeof(buffer), "ftp slave hello");
}

int ftp_receive_slave_hello(int connfd, slave_hello_t *hello)
{
    unsigned char buffer[FTP_SLAVE_HELLO_WIRE_SIZE];
    const unsigned char *cursor = buffer;

    if (ftp_read_message(connfd, buffer, sizeof(buffer), "ftp control slave hello", 0) <= 0) {
        return -1;
    }

    ftp_decode_slave_hello(&cursor, hello);
    if (ftp_validate_slave_hello_message(hello, "ftp control slave hello") < 0) {
        return -1;
    }
    return 0;
}

int ftp_send_cluster_map(int connfd, const slave_cluster_t *cluster)
{
    unsigned char buffer[FTP_CLUSTER_WIRE_SIZE];

    ftp_encode_cluster(buffer, cluster);
    return ftp_write_exact(connfd, buffer, sizeof(buffer), "ftp cluster map");
}

int ftp_receive_cluster_map(int connfd, slave_cluster_t *cluster)
{
    unsigned char buffer[FTP_CLUSTER_WIRE_SIZE];

    if (ftp_read_message(connfd, buffer, sizeof(buffer), "ftp control cluster map", 0) <= 0) {
        return -1;
    }

    ftp_decode_cluster(buffer, cluster);
    if (ftp_validate_cluster_message(cluster, "ftp control cluster map") < 0) {
        return -1;
    }

    return 0;
}

int ftp_send_replication_request(int connfd, const ftp_replication_request_t *request)
{
    unsigned char buffer[FTP_REPLICATION_REQUEST_WIRE_SIZE];

    ftp_encode_replication_request(buffer, request);
    return ftp_write_exact(connfd, buffer, sizeof(buffer), "ftp replication request");
}

int ftp_receive_replication_request(int connfd, ftp_replication_request_t *request)
{
    unsigned char buffer[FTP_REPLICATION_REQUEST_WIRE_SIZE];

    if (ftp_read_message(connfd, buffer, sizeof(buffer), "ftp control replication request", 0) <= 0) {
        return -1;
    }

    ftp_decode_replication_request(buffer, request);
    if (ftp_validate_replication_request_message(request, "ftp control replication request") < 0) {
        return -1;
    }
    return 0;
}

int ftp_send_control_reply(int connfd, ftp_status_t status)
{
    unsigned char buffer[FTP_CONTROL_REPLY_WIRE_SIZE];
    unsigned char *cursor = buffer;

    ftp_encode_u32(&cursor, FTP_PROTO_VERSION);
    ftp_encode_u32(&cursor, (uint32_t)status);
    return ftp_write_exact(connfd, buffer, sizeof(buffer), "ftp control reply");
}

int ftp_receive_control_reply(int connfd, ftp_status_t *status)
{
    unsigned char buffer[FTP_CONTROL_REPLY_WIRE_SIZE];
    const unsigned char *cursor = buffer;
    uint32_t version;

    if (ftp_read_message(connfd, buffer, sizeof(buffer), "ftp control reply", 0) <= 0) {
        return -1;
    }

    version = ftp_decode_u32(&cursor);
    *status = (ftp_status_t)ftp_decode_u32(&cursor);
    if (ftp_validate_control_reply_message(version, *status, "ftp control reply") < 0) {
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
    case FTP_STATUS_RESTART:
        return "RESTART";
    default:
        return "UNKNOWN";
    }
}
