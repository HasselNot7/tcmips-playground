/* TCMIPS platform glue for NetHack 5.0.0 */
#ifndef TCM_PORT_H
#define TCM_PORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int tcm_getch(void);
long tcm_rand(void);
void tcm_napms(int ms);
void tcm_vterm_init(void);
void tcm_vterm_flush(void);
void tcm_vterm_clear(void);
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

#endif
