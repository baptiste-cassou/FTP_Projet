#include "csapp.h"
#include "utils.h"

//renvoi la taille du fichier sinon -1
int check_size_file(const char * filename) {
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_size;
    return 0;
}