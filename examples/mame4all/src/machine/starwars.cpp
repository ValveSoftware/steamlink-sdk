/***************************************************************************
machine\starwars.c

STARWARS MACHINE FILE

This file is Copyright 1997, Steve Baines.
Modified by Frank Palazzolo for sound support

Release 2.0 (6 August 1997)

See drivers\starwars.c for notes

***************************************************************************/

#include "driver.h"
#include "cpuintrf.h"
#include "swmathbx.h"
#include "vidhrdw/avgdvg.h"


/* control select values for ADC_R */
#define kPitch	0
#define kYaw	1
#define kThrust	2

static unsigned char control_num = kPitch;

#if 0
/********************************************************/
READ_HANDLER( input_bank_0_r )
{
	int x;
	x=input_port_0_r(0); /* Read memory mapped port 1 */
	x=x&0xdf; /* Clear out bit 5 (SPARE 1) */
#if(MACHDEBUG==1)
	printf("(%x)input_bank_0_r   (returning %xh)\n", cpu_get_pc(), x);
#endif
	return x;
}
#endif

/********************************************************/
READ_HANDLER( starwars_input_bank_1_r )
{
	int x;
	x=input_port_1_r(0); /* Read memory mapped port 2 */

#if 0
	x=x&0x34; /* Clear out bit 3 (SPARE 2), and 0 and 1 (UNUSED) */
	/* MATH_RUN (bit 7) set to 0 */
	x=x|(0x40);  /* Set bit 6 to 1 (VGHALT) */
#endif

	/* Kludge to enable Starwars Mathbox Self-test                  */
	/* The mathbox looks like it's running, from this address... :) */
	if (cpu_get_pc() == 0xf978)
		x|=0x80;

	/* Kludge to enable Empire Mathbox Self-test                  */
	/* The mathbox looks like it's running, from this address... :) */
	if (cpu_get_pc() == 0xf655)
		x|=0x80;

	if (avgdvg_done())
		x|=0x40;
	else
		x&=~0x40;

	return x;
}
/*********************************************************/
/********************************************************/
READ_HANDLER( starwars_control_r )
{

	if (control_num == kPitch)
		return readinputport (4);
	else if (control_num == kYaw)
		return readinputport (5);
	/* default to unused thrust */
	else return 0;
}

WRITE_HANDLER( starwars_control_w )
{
	control_num = offset;
}

