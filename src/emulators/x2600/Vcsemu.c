#include "options.h"
#include "vcsemu.h"
#include "types.h"
#include "vmachine.h"

#include <string.h>

#include "emuapi.h"
#include "x2600_port.h"

/****************************************************************************
* Local macros / typedefs
****************************************************************************/

/****************************************************************************
* Global data
****************************************************************************/

/****************************************************************************
* Imported procedures
****************************************************************************/
extern void mainloop(void);

/****************************************************************************
* Local procedures
****************************************************************************/

/****************************************************************************
* Exported procedures
****************************************************************************/
void vcs_Init(void)
{
  init_machine();	
  /* TCMIPS fix: init_banking() derives theRom from rom_size; without a
     cartridge loaded that underflows the cart buffer. Default to a plain
     4K map until vcs_LoadROM() runs. */
  if (rom_size == 0)
    rom_size = 4096;
  init_hardware();
  tv_on();
}


void vcs_Start(char * filename)
{
  int size = emu_LoadFile(filename, (char *)theCart, 16384); 
  

  if (size > 16384)
    size = 16384;
	
  rom_size = size;
  if (size == 2048)
  {
    memcpy (&theCart[2048], &theCart[0], 2048);
    rom_size = 4096;
  }
  else if (size < 2048)
  {
    theCart[0x0ffc] = 0x00;
    theCart[0x0ffd] = 0xf0;
    rom_size = 4096;
  }
  
  init_hardware();
  init_banking(); 
}


void vcs_Step(void)
{
  //emu_printf("s");
  mainloop();
}


/* TCMIPS addition: load a cartridge from memory instead of a file and
   pick the bankswitch scheme from the image size. */
void vcs_LoadROM(const unsigned char *data, int size)
{
  if (size > 16384)
    size = 16384;

  memcpy (theCart, data, size);
  rom_size = size;
  if (size == 2048)
  {
    memcpy (&theCart[2048], &theCart[0], 2048);
    rom_size = 4096;
  }
  else if (size < 2048)
  {
    theCart[0x0ffc] = 0x00;
    theCart[0x0ffd] = 0xf0;
    rom_size = 4096;
  }

  if (rom_size <= 4096)
    base_opts.bank = 0;          /* no bankswitching */
  else if (rom_size == 8192)
    base_opts.bank = 1;          /* Atari F8 */
  else if (rom_size == 12288)
    base_opts.bank = 4;          /* CBS RAM Plus FA */
  else
    base_opts.bank = 2;          /* Atari F6 */

  init_hardware();
  init_banking();
}






