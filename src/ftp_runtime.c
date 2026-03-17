#include "csapp.h"
#include "ftp_runtime.h"

void ftp_enter_working_directory(const char *program_name, const char *path)
{
    char cwd[MAXLINE];

    if (chdir(path) < 0) {
        fprintf(stderr, "%s: unable to enter working directory '%s': %s\n",
                program_name, path, strerror(errno));
        exit(1);
    }

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s working directory: %s\n", program_name, cwd);
    }
}
