/*************************************************************************

  Food Fight machine hardware

*************************************************************************/

#include <stdlib.h>
#include "driver.h"




/*
 *		Statics
 */

static int whichport = 0;


/*
 *		Interrupt handlers.
 */

void foodf_delayed_interrupt (int param)
{
	cpu_cause_interrupt (0, 2);
}

int foodf_interrupt (void)
{
	/* INT 2 once per frame in addition to... */
	if (cpu_getiloops () == 0)
		timer_set (TIME_IN_USEC (100), 0, foodf_delayed_interrupt);

	/* INT 1 on the 32V signal */
	return 1;
}


/*
 *		NVRAM read/write.
 *      also used by Quantum
 */

static unsigned char nvram[128];

READ_HANDLER( foodf_nvram_r )
{
	return ((nvram[(offset / 4) ^ 0x03] >> 2*(offset % 4))) & 0x0f;
}


WRITE_HANDLER( foodf_nvram_w )
{
	nvram[(offset / 4) ^ 0x03] &= ~(0x0f << 2*(offset % 4));
	nvram[(offset / 4) ^ 0x03] |= (data & 0x0f) << 2*(offset % 4);
}

void foodf_nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,128);
	else
	{
		if (file)
			osd_fread(file,nvram,128);
		else
			memset(nvram,0xff,128);
	}
}


/*
 *		Analog controller read dispatch.
 */

READ_HANDLER( foodf_analog_r )
{
	switch (offset)
	{
		case 0:
		case 2:
		case 4:
		case 6:
			return readinputport (whichport);
	}
	return 0;
}


/*
 *		Digital controller read dispatch.
 */

READ_HANDLER( foodf_digital_r )
{
	switch (offset)
	{
		case 0:
			return input_port_4_r (offset);
	}
	return 0;
}


/*
 *		Analog write dispatch.
 */

WRITE_HANDLER( foodf_analog_w )
{
	whichport = 3 - ((offset/2) & 3);
}


/*
 *		Digital write dispatch.
 */

WRITE_HANDLER( foodf_digital_w )
{
}
