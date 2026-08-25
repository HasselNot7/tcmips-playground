/* Minimal dirent shim for TCMIPS: directories always appear empty.
 * (The RAM filesystem stores save files flat; save scanning is a no-op.) */
#ifndef TCM_DIRENT_H
#define TCM_DIRENT_H

#define dirent tcm_dirent
#define DIR TCM_DIR

typedef struct {
    int idx;
} TCM_DIR;

struct tcm_dirent {
    char d_name[256];
};

#ifdef __cplusplus
extern "C" {
#endif

TCM_DIR *opendir(const char *name);
struct tcm_dirent *readdir(TCM_DIR *dir);
int closedir(TCM_DIR *dir);

#ifdef __cplusplus
}
#endif

#endif /* TCM_DIRENT_H */
