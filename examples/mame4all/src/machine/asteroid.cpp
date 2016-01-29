/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "machine/atari_vg.h"
#include "vidhrdw/avgdvg.h"

int asteroid_interrupt (void)
{
	/* Turn off interrupts if self-test is enabled */
	if (readinputport(0) & 0x80)
		return ignore_interrupt();
	else
		return nmi_interrupt();
}

int llander_interrupt (void)
{
	/* Turn off interrupts if self-test is enabled */
	if (readinputport(0) & 0x02)
		return nmi_interrupt();
	else
		return ignore_interrupt();
}

READ_HANDLER( asteroid_IN0_r )
{

	int res;
	int bitmask;

	res=readinputport(0);

	bitmask = (1 << offset);

	if (cpu_gettotalcycles() & 0x100)
		res |= 0x02;
	if (!avgdvg_done())
		res |= 0x04;

	if (res & bitmask)
		res = 0x80;
	else
		res = ~0x80;

	return res;
}

READ_HANDLER( asteroib_IN0_r )
{
	int res;

	res=readinputport(0);

//	if (cpu_gettotalcycles() & 0x100)
//		res |= 0x02;
	if (!avgdvg_done())
		res |= 0x80;

	return res;
}

/*
 * These 7 memory locations are used to read the player's controls.
 * Typically, only the high bit is used. This is handled by one input port.
 */

READ_HANDLER( asteroid_IN1_r )
{
	int res;
	int bitmask;

	res=readinputport(1);
	bitmask = (1 << offset);

	if (res & bitmask)
		res = 0x80;
	else
	 	res = ~0x80;
	return (res);
}

READ_HANDLER( asteroid_DSW1_r )
{
	int res;
	int res1;

	res1 = readinputport(2);

	res = 0xfc | ((res1 >> (2 * (3 - (offset & 0x3)))) & 0x3);
	return res;
}


WRITE_HANDLER( asteroid_bank_switch_w )
{
	static int asteroid_bank = 0;
	int asteroid_newbank;
	unsigned char *RAM = memory_region(REGION_CPU1);


	asteroid_newbank = (data >> 2) & 1;
	if (asteroid_bank != asteroid_newbank) {
		/* Perform bankswitching on page 2 and page 3 */
		int temp;
		int i;

		asteroid_bank = asteroid_newbank;
		for (i = 0; i < 0x100; i++) {
			temp = RAM[0x200 + i];
			RAM[0x200 + i] = RAM[0x300 + i];
			RAM[0x300 + i] = temp;
		}
	}
	osd_led_w (0, ~(data >> 1));
	osd_led_w (1, ~data);
}

WRITE_HANDLER( astdelux_bank_switch_w )
{
	static int astdelux_bank = 0;
	int astdelux_newbank;
	unsigned char *RAM = memory_region(REGION_CPU1);


	astdelux_newbank = (data >> 7) & 1;
	if (astdelux_bank != astdelux_newbank) {
		/* Perform bankswitching on page 2 and page 3 */
		int temp;
		int i;

		astdelux_bank = astdelux_newbank;
		for (i = 0; i < 0x100; i++) {
			temp = RAM[0x200 + i];
			RAM[0x200 + i] = RAM[0x300 + i];
			RAM[0x300 + i] = temp;
		}
	}
}

WRITE_HANDLER( astdelux_led_w )
{
	osd_led_w (offset, ~data);
}

void asteroid_init_machine(void)
{
	asteroid_bank_switch_w (0,0);
}

/*
 * This is Lunar Lander's Inputport 0.
 */
READ_HANDLER( llander_IN0_r )
{
	int res;

	res = readinputport(0);

	if (avgdvg_done())
		res |= 0x01;
	if (cpu_gettotalcycles() & 0x100)
		res |= 0x40;

	return res;
}
