/* TCMIPS platform glue for NetHack 5.0.0 */
#ifndef TCM_PORT_H
#define TCM_PORT_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

void tcm_embed_init(void);
int tcm_getch(void);
long tcm_rand(void);
void tcm_napms(int ms);
void tcm_vterm_init(void);
void tcm_vterm_flush(void);
void tcm_vterm_clear(void);
void tcm_vterm_raw(const char *s);
void tcm_vterm_putc(int c);
void tcm_vterm_cmov(int x, int y);
void tcm_vterm_setattr(int fg, int bg, int bold, int rev);
void tcm_vterm_cl_end(void);
void tcm_vterm_cl_eos(void);
int tcm_vterm_cur_x(void);
int tcm_vterm_cur_y(void);
extern int tcm_vterm_CO;
extern int tcm_vterm_LI;

#ifdef __cplusplus
}
#endif

/*
 * symbol-interposition avoidance: libc.bc holds strong definitions of
 * the POSIX file calls; on the device linker our definitions lost the
 * race, so all file-call sites are renamed to our own tcm_fs_* names.
 */
#if !defined(TCM_HOST) || defined(TCM_FS_HOST)
#define open(...)    tcm_fs_open(__VA_ARGS__)
#define read(...)    tcm_fs_read(__VA_ARGS__)
#define write(...)   tcm_fs_write(__VA_ARGS__)
#define close(fd)    tcm_fs_close(fd)
#define lseek(...)   tcm_fs_lseek(__VA_ARGS__)
#define unlink(p)    tcm_fs_unlink(p)
#define link(...)    tcm_fs_link(__VA_ARGS__)
#define creat(...)   tcm_fs_creat(__VA_ARGS__)
#define open64(...)  tcm_fs_open64(__VA_ARGS__)
#define creat64(...) tcm_fs_creat64(__VA_ARGS__)
#define openat(...)  tcm_fs_openat(__VA_ARGS__)
#define openat64(...) tcm_fs_openat64(__VA_ARGS__)
#define chmod(p, m)  tcm_fs_chmod(p, m)
#define stat(p, st)  tcm_fs_stat(p, st)
#define fstat(fd, st) tcm_fs_fstat(fd, st)
#define isatty(fd)   tcm_fs_isatty(fd)
#define access(...)  tcm_fs_access(__VA_ARGS__)
#define rename(p, q) tcm_fs_rename(p, q)
#define fcntl(...)   tcm_fs_fcntl(__VA_ARGS__)
#define getcwd(...)  tcm_fs_getcwd(__VA_ARGS__)

/* declarations for the renamed implementations */
int tcm_fs_open(const char *, int, ...);
int tcm_fs_close(int);
ssize_t tcm_fs_read(int, void *, size_t);
ssize_t tcm_fs_write(int, const void *, size_t);
off_t tcm_fs_lseek(int, off_t, int);
int tcm_fs_unlink(const char *);
int tcm_fs_link(const char *, const char *);
int tcm_fs_creat(const char *, mode_t);
int tcm_fs_open64(const char *, int, ...);
int tcm_fs_creat64(const char *, mode_t);
int tcm_fs_openat(int, const char *, int, ...);
int tcm_fs_openat64(int, const char *, int, ...);
int tcm_fs_chmod(const char *, mode_t);
int tcm_fs_stat(const char *, struct stat *);
int tcm_fs_fstat(int, struct stat *);
int tcm_fs_isatty(int);
int tcm_fs_access(const char *, int);
int tcm_fs_rename(const char *, const char *);
int tcm_fs_fcntl(int, int, ...);
char *tcm_fs_getcwd(char *, size_t);
extern unsigned long tcm_boot_ms;
extern unsigned long tcm_wall_ms(void);
#endif /* !TCM_HOST */

#endif
