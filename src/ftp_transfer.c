#include "csapp.h"
#include "ftp_transfer.h"

static int write_full(int fd, const void *buffer, size_t count)
{
    const char *cursor = buffer;
    size_t written = 0;

    while (written < count) {
        ssize_t n = write(fd, cursor + written, count - written);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        written += (size_t)n;
    }

    return 0;
}
/* marche pas encore
int ftp_get_stats(const char *filename, uint64_t *file_size, ftp_status_t *status) {
    struct stat st;
    int fd = -1;
    char *data = NULL;
    uint64_t total = 0;

    *file_size = 0;
    *status = FTP_STATUS_ERR_IO;

    if (stat(filename, &st) < 0) {
        if (errno == ENOENT) {
            *status = FTP_STATUS_ERR_NOT_FOUND;
        }
        return -1;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
        return -1;
    }
    if ((uint64_t)st.st_size > (uint64_t)SIZE_MAX) {
        return -1;
    }

    *file_size = (uint64_t)st.st_size;
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            *status = FTP_STATUS_ERR_NOT_FOUND;
        }
        return -1;
    }
    if (*file_size == 0) {
        close(fd);
        return 0;
    }
    close(fd);
}

int ftp_load_partial(const char *filename, void **buffer, size_t *size) {
    int data;
    int fd = open(filename, O_RDONLY);
    uint64_t total = 0;


    data = Malloc(size);
    int readed = read(fd, data, *size);
    while (total < *size) {
        size_t remaining = (size_t)(*size - total);
        ssize_t n = read(fd, data + (size_t)total, remaining);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            Free(data);
            *size = 0;
            return -1;
        }
        if (n == 0) {
            close(fd);
            Free(data);
            *size = 0;
            return -1;
        }

        total += (uint64_t)n;
    }

    close(fd);
    *buffer = data;
    return 0;
}
*/
int ftp_load_file(const char *filename, void **buffer, uint64_t *file_size, ftp_status_t *status)
{
    struct stat st;
    int fd = -1;
    char *data = NULL;
    uint64_t total = 0;

    *buffer = NULL;
    *file_size = 0;
    *status = FTP_STATUS_ERR_IO;

    if (stat(filename, &st) < 0) {
        if (errno == ENOENT) {
            *status = FTP_STATUS_ERR_NOT_FOUND;
        }
        return -1;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
        return -1;
    }
    if ((uint64_t)st.st_size > (uint64_t)SIZE_MAX) {
        return -1;
    }

    *file_size = (uint64_t)st.st_size;
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            *status = FTP_STATUS_ERR_NOT_FOUND;
        }
        return -1;
    }

    if (*file_size == 0) {
        close(fd);
        return 0;
    }

    data = Malloc((size_t)*file_size);
    while (total < *file_size) {
        size_t remaining = (size_t)(*file_size - total);
        ssize_t n = read(fd, data + (size_t)total, remaining);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            Free(data);
            *file_size = 0;
            return -1;
        }
        if (n == 0) {
            close(fd);
            Free(data);
            *file_size = 0;
            return -1;
        }

        total += (uint64_t)n;
    }

    close(fd);
    *buffer = data;
    return 0;
}

int ftp_receive_file_payload(int connfd, const char *filename, uint64_t file_size)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, DEF_MODE);
    uint64_t remaining = file_size;
    char buffer[FTP_BLOCK_SIZE];

    if (fd < 0) {
        fprintf(stderr, "clientFTP: unable to open '%s' for writing: %s\n", filename, strerror(errno));
        return -1;
    }

    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buffer) ? (size_t)remaining : sizeof(buffer);
        ssize_t n = Rio_readn(connfd, buffer, chunk);

        if (n < 0) {
            fprintf(stderr, "clientFTP: error while receiving file data: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        if ((size_t)n != chunk) {
            fprintf(stderr, "clientFTP: incomplete file payload received: %zd bytes\n", n);
            close(fd);
            return -1;
        }
        if (write_full(fd, buffer, chunk) < 0) {
            fprintf(stderr, "clientFTP: error while writing '%s': %s\n", filename, strerror(errno));
            close(fd);
            return -1;
        }

        remaining -= (uint64_t)chunk;
    }

    close(fd);
    return 0;
}
