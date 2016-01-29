/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"


static int ctrld;
static int h_pos, v_pos;

extern int missile_flipscreen;

int  missile_video_r (int address);
void missile_video_w (int address, int data);
void missile_video_mult_w (int address, int data);

WRITE_HANDLER( missile_palette_w );
void missile_flip_screen (void);


/********************************************************************************************/
READ_HANDLER( missile_IN0_r )
{
	if (ctrld)	/* trackball */
	{
		if (!missile_flipscreen)
	  	    return ((readinputport(5) << 4) & 0xf0) | (readinputport(4) & 0x0f);
		else
	  	    return ((readinputport(7) << 4) & 0xf0) | (readinputport(6) & 0x0f);
	}
	else	/* buttons */
		return (readinputport(0));
}


/********************************************************************************************/
void missile_init_machine(void)
{
	h_pos = v_pos = 0;
}


/********************************************************************************************/
WRITE_HANDLER( missile_w )
{
	int pc, opcode;
	int address = offset + 0x640;

	pc = cpu_getpreviouspc();
	opcode = cpu_readop(pc);

	/* 3 different ways to write to video ram - the third is caught by the core memory handler */
	if (opcode == 0x81)
	{
		/* 	STA ($00,X) */
		missile_video_w (address, data);
		return;
	}
	if (address <= 0x3fff)
	{
		missile_video_mult_w (address, data);
		return;
	}

	/* $4c00 - watchdog */
	if (address == 0x4c00)
	{
		watchdog_reset_w (address, data);
		return;
	}

	/* $4800 - various IO */
	if (address == 0x4800)
	{
		if (missile_flipscreen != (!(data & 0x40)))
			missile_flip_screen ();
		missile_flipscreen = !(data & 0x40);
		coin_counter_w (0, data & 0x20);
		coin_counter_w (1, data & 0x10);
		coin_counter_w (2, data & 0x08);
		osd_led_w (0, ~data >> 1);
		osd_led_w (1, ~data >> 2);
		ctrld = data & 1;
		return;
	}

	/* $4d00 - IRQ acknowledge */
	if (address == 0x4d00)
	{
		return;
	}

	/* $4000 - $400f - Pokey */
	if (address >= 0x4000 && address <= 0x400f)
	{
		pokey1_w (address, data);
		return;
	}

	/* $4b00 - $4b07 - color RAM */
	if (address >= 0x4b00 && address <= 0x4b07)
	{
		int r,g,b;


		r = 0xff * ((~data >> 3) & 1);
		g = 0xff * ((~data >> 2) & 1);
		b = 0xff * ((~data >> 1) & 1);

		palette_change_color(address - 0x4b00,r,g,b);

		return;
	}

	//logerror("possible unmapped write, offset: %04x, data: %02x\n", address, data);
}


/********************************************************************************************/

unsigned char *missile_video2ram;

READ_HANDLER( missile_r )
{
	int pc, opcode;
	int address = offset + 0x1900;

	pc = cpu_getpreviouspc();
	opcode = cpu_readop(pc);

	if (opcode == 0xa1)
	{
		/* 	LDA ($00,X)  */
		return (missile_video_r(address));
	}

	if (address >= 0x5000)
		return missile_video2ram[address - 0x5000];

	if (address == 0x4800)
		return (missile_IN0_r(0));
	if (address == 0x4900)
		return (readinputport (1));
	if (address == 0x4a00)
		return (readinputport (2));

	if ((address >= 0x4000) && (address <= 0x400f))
		return (pokey1_r (address & 0x0f));

	//logerror("possible unmapped read, offset: %04x\n", address);
	return 0;
}
