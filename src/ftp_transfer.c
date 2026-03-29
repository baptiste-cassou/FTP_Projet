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

// Legacy helper kept from the single-buffer version of the project.
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

int ftp_send_buffer_payload(int connfd, const void *buffer, uint32_t payload_size)
{
    ssize_t n;

    if (payload_size == 0) {
        return 0;
    }

    n = rio_writen(connfd, (void *)buffer, payload_size);
    if (n < 0 || (uint32_t)n != payload_size) {
        fprintf(stderr, "ftp payload: failed to send buffer payload: %s\n",
                (n < 0) ? strerror(errno) : "short write");
        return -1;
    }

    return 0;
}

int ftp_send_fd_payload(int connfd, int fd, uint64_t file_size)
{
    char buffer[FTP_BLOCK_SIZE];
    uint64_t remaining = file_size;

    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buffer) ? (size_t)remaining : sizeof(buffer);
        ssize_t n = read(fd, buffer, chunk);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }

        if (rio_writen(connfd, buffer, (size_t)n) != n) {
            return -1;
        }
        remaining -= (uint64_t)n;
    }

    return 0;
}

int ftp_receive_buffer_payload(int connfd, uint32_t payload_size, char **buffer)
{
    char *payload;
    ssize_t n;

    if (buffer == NULL) {
        return -1;
    }

    payload = Malloc((size_t)payload_size + 1);
    if (payload_size == 0) {
        payload[0] = '\0';
        *buffer = payload;
        return 0;
    }

    n = rio_readn(connfd, payload, payload_size);
    if (n < 0 || (uint32_t)n != payload_size) {
        Free(payload);
        return -1;
    }

    payload[payload_size] = '\0';
    *buffer = payload;
    return 0;
}

int ftp_receive_file_payload(int connfd, const char *filename, uint64_t file_size, off_t offset)
{
    int fd;
    uint64_t remaining;
    int received = 0;
    if (offset > file_size) offset = 0; // check de sécurité si le fichier à été modif entre temps coté serveur alors on écrase de 0 le fichier
    fd = open(filename, O_WRONLY | O_CREAT | ((offset == 0) ? O_TRUNC : 0), DEF_MODE);
    remaining = file_size - offset;

    char buffer[FTP_BLOCK_SIZE];

    if (fd < 0) {
        fprintf(stderr, "clientFTP: unable to open '%s' for writing: %s\n", filename, strerror(errno));
        return -1;
    }

    lseek(fd, offset, SEEK_SET); //on décalle de l'offset nécéssaire (si il n'y a pas d'offset alors pas de seek car offset == 0)
    
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buffer) ? (size_t)remaining : sizeof(buffer);
        ssize_t n = rio_readn(connfd, buffer, chunk);

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
        received += n;
        remaining -= (uint64_t)chunk;
    }

    close(fd);
    return received;
}
