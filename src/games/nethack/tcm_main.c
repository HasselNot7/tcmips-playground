/* TCMIPS entry point for NetHack 5.0.0
 * Modeled on sys/unix/unixmain.c, stripped for bare-metal. */
#include "hack.h"
#include "dlb.h"

#include "tcm_port.h"

#ifndef TCM_HOST
#include <dev/console.h>
#endif

__attribute__((used)) const char tcm_build_id[] = "tcm-nethack-v1.0";

static char tcm_argv0[] = "nethack";
static char *tcm_argv[] = { tcm_argv0, 0 };

int
main(int argc UNUSED, char **argv UNUSED)
{
    NHFILE *nhfp;
    boolean resuming = FALSE;

    tcm_ascii_console_init(); /* device driver / host simulation */
    tcm_vterm_init();

#ifndef TCM_HOST
    tcm_embed_init(); /* blobs must live in RAM on the device */
#endif

    early_init(1, tcm_argv);
    (void) tcm_build_id;

    gh.hname = tcm_argv[0];
    svh.hackpid = getpid();

    choose_windows(DEFAULT_WINDOW_SYS);

    initoptions();

    u.uhp = 1; /* prevent RIP on early quits */

    /* fixed player identity: skip whoami/getpwuid entirely */
    Strcpy(svp.plname, "Hero");

    argc = 1;
    argv = tcm_argv;
    init_nhwindows(&argc, argv); /* now we can set up window system */

    set_playmode(); /* sets plname to "wizard" in wizard mode */

    gp.plnamelen = 0; /* not an exact username */
    plnamesuffix();   /* strip role,race,&c suffix */

    dlb_init();
    dlb_init();
    vision_init();
    init_sound_disp_gamewindows();

            
    if (*svp.plname) {
        getlock();
        }

    if (*svp.plname && (nhfp = restore_saved_game()) != 0) {
        pline("Restoring save file...");
        mark_synch();
        if (dorecover(nhfp))
            resuming = TRUE;
        else
            (void) delete_savefile();
    }

    if (!resuming) {
        player_selection();
        newgame();
    }

    moveloop(TRUE);

    exit_nhwindows((char *) 0);
    return 0;
}
