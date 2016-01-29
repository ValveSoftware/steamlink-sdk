/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"



int arkanoid_paddle_select;

static int z80write,fromz80,m68705write,toz80;

static unsigned char portA_in,portA_out,ddrA;
static unsigned char portC_out,ddrC;

FILE *thelog;

void arkanoid_init_machine(void)
{
	portA_in = portA_out = z80write = m68705write = 0;
}

READ_HANDLER( arkanoid_Z80_mcu_r )
{
	/* return the last value the 68705 wrote, and mark that we've read it */
	m68705write = 0;
	return toz80;
}

WRITE_HANDLER( arkanoid_Z80_mcu_w )
{
	/* a write from the Z80 has occurred, mark it and remember the value */
	z80write = 1;
	fromz80 = data;

	/* give up a little bit of time to let the 68705 detect the write */
	cpu_spinuntil_trigger(700);
}

READ_HANDLER( arkanoid_68705_portA_r )
{
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

WRITE_HANDLER( arkanoid_68705_portA_w )
{
	portA_out = data;
}

WRITE_HANDLER( arkanoid_68705_ddrA_w )
{
	ddrA = data;
}


READ_HANDLER( arkanoid_68705_portC_r )
{
	int res=0;

	/* bit 0 is high on a write strobe; clear it once we've detected it */
	if (z80write) res |= 0x01;

	/* bit 1 is high if the previous write has been read */
	if (!m68705write) res |= 0x02;

	return (portC_out & ddrC) | (res & ~ddrC);
}

WRITE_HANDLER( arkanoid_68705_portC_w )
{
	if ((ddrC & 0x04) && (~data & 0x04) && (portC_out & 0x04))
	{
		/* mark that the command has been seen */
		cpu_trigger(700);

		/* return the last value the Z80 wrote */
		z80write = 0;
		portA_in = fromz80;
	}
	if ((ddrC & 0x08) && (~data & 0x08) && (portC_out & 0x08))
	{
		/* a write from the 68705 to the Z80; remember its value */
		m68705write = 1;
		toz80 = portA_out;
	}

	portC_out = data;
}

WRITE_HANDLER( arkanoid_68705_ddrC_w )
{
	ddrC = data;
}



READ_HANDLER( arkanoid_68705_input_0_r )
{
	int res = input_port_0_r(offset) & 0x3f;

	/* bit 0x40 comes from the sticky bit */
	if (!z80write) res |= 0x40;

	/* bit 0x80 comes from a write latch */
	if (!m68705write) res |= 0x80;

	return res;
}

READ_HANDLER( arkanoid_input_2_r )
{
	if (arkanoid_paddle_select)
	{
		return input_port_3_r(offset);
	}
	else
	{
		return input_port_2_r(offset);
	}
}

