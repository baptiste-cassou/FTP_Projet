#include "csapp.h"
#include "utils.h"

uint64_t check_size_file(const char *filename)
{
    struct stat st;

    if (stat(filename, &st) == 0 && st.st_size >= 0) {
        return (uint64_t)st.st_size;
    }

    return 0;
}
