#include "driver.h"
#include "vidhrdw/avgdvg.h"




/*** quantum_interrupt
*
* Purpose: do an interrupt - so many times per raster frame
*
* Returns: 0
*
* History: 11/19/97 PF Created
*
**************************/
int quantum_interrupt(void)
{
	return 1; /* ipl0' == ivector 1 */
}

/*** quantum_switches_r
*
* Purpose: read switches input, which sneaks the VHALT' in
*
* Returns: byte
*
* History: 11/20/97 PF Created
*
**************************/
READ_HANDLER( quantum_switches_r )
{
	return (input_port_0_r(0) |
		(avgdvg_done() ? 1 : 0));
}



WRITE_HANDLER( quantum_led_w )
{
	/* bits 0 and 1 are coin counters */
	coin_counter_w(0,data & 2);
	coin_counter_w(1,data & 1);

	/* bits 4 and 5 are LED controls */
	osd_led_w(0,(data & 0x10) >> 4);
	osd_led_w(1,(data & 0x20) >> 5);

	/* other bits unknown */
}



/*** quantum_snd_read, quantum_snd_write
*
* Purpose: read and write POKEY chips -
*	need to do translation, so we don't directly map it
*
* Returns: register value, for read
*
* History: 11/19/97 PF Created
*
**************************/
WRITE_HANDLER( quantum_snd_w )
{
	if (offset & 0x20) /* A5 selects chip */
		pokey2_w((offset >> 1) % 0x10,data);
	else
		pokey1_w((offset >> 1) % 0x10,data);
}

READ_HANDLER( quantum_snd_r )
{
	if (offset & 0x20)
		return pokey2_r((offset >> 1) % 0x10);
	else
		return pokey1_r((offset >> 1) % 0x10);
}


/*** quantum_trackball
*
* Purpose: read trackball port.  So far, attempting theory:
*	D0-D3 - vert movement delta
*	D4-D7 - horz movement delta
*
*	if wrong, will need to pull out my 74* logic reference
*
* Returns: 8 bit value
*
* History: 11/19/97 PF Created
*
**************************/
READ_HANDLER( quantum_trackball_r )
{
	int x, y;

	x = input_port_4_r (offset);
	y = input_port_3_r (offset);

	return (x << 4) + y;
}


/*** quantum_input_1_r, quantum_input_2_r
*
* Purpose: POKEY input switches read
*
* Returns: in the high bit the appropriate switch value
*
* History: 12/2/97 ASG Created
*
**************************/
READ_HANDLER( quantum_input_1_r )
{
	return (input_port_1_r (0) << (7 - (offset - POT0_C))) & 0x80;
}

READ_HANDLER( quantum_input_2_r )
{
	return (input_port_2_r (0) << (7 - (offset - POT0_C))) & 0x80;
}
