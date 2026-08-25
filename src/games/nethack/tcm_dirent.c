/* dirent shim implementation: every directory is empty. */
#include <stdlib.h>
#include "dirent.h"

TCM_DIR *
opendir(const char *name)
{
    (void) name;
    TCM_DIR *d = (TCM_DIR *) malloc(sizeof(TCM_DIR));
    if (d)
        d->idx = 0;
    return d;
}

struct tcm_dirent *
readdir(TCM_DIR *dir)
{
    if (!dir || dir->idx > 0)
        return 0;
    dir->idx = 1;
    return 0; /* no entries */
}

int
closedir(TCM_DIR *dir)
{
    free(dir);
    return 0;
}
