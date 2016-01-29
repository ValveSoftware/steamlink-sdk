/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"

int ddrible_int_enable_0, ddrible_int_enable_1;

void ddrible_init_machine( void )
{
	ddrible_int_enable_0 = ddrible_int_enable_1 = 0;
}

WRITE_HANDLER( int_0_w )
{
	if (data & 0x02)
		ddrible_int_enable_0 = 1;
	else
		ddrible_int_enable_0 = 0;
}

WRITE_HANDLER( int_1_w )
{
	if (data & 0x02)
		ddrible_int_enable_1 = 1;
	else
		ddrible_int_enable_1 = 0;
}
