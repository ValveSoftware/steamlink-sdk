#include "../vidhrdw/tigeroad.cpp"

/***************************************************************************

Tiger Road (C) 1987 Romstar/Capcom USA

Please contact Phil Stroffolino (phil@maya.com) if there are any questions
regarding this driver.

F1 Dream protection workaround by Eric Hustvedt

Memory Overview:
	0xfe0800    sprites
	0xfec000    text
	0xfe4000    input ports,dip switches (read); sound out, video reg (write)
	0xfe4002	protection (F1 Dream only)
	0xfe8000    scroll registers
	0xff8200    palette
	0xffC000    working RAM
	0xffEC70    high scores (not saved)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


WRITE_HANDLER( tigeroad_videoctrl_w );
WRITE_HANDLER( tigeroad_scroll_w );
void tigeroad_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static unsigned char *ram;

/*
 F1 Dream protection code written by Eric Hustvedt (hustvedt@ma.ultranet.com).

 The genuine F1 Dream game uses an 8751 microcontroller as a protection measure.
 Since the microcontroller's ROM is unavailable all interactions with it are handled
 via blackbox algorithm.

 Some notes:
 - The 8751 is triggered via location 0xfe4002, in place of the soundlatch normally
 	present. The main cpu writes 0 to the location when it wants the 8751 to perform some work.
 - The 8751 has memory which shadows locations 0xffffe0-0xffffff of the main cpu's address space.
 - The word at 0xffffe0 contains an 'opcode' which is written just before the write to 0xfe4002.
 - Some of the writes to the soundlatch may not be handled. 0x27fc is the main sound routine, the
 	other locations are less frequently used.
*/

static int f1dream_613ea_lookup[16] = {
0x0052, 0x0031, 0x00a7, 0x0043, 0x0007, 0x008a, 0x00b1, 0x0066, 0x009f, 0x00cc, 0x0009, 0x004d, 0x0033, 0x0028, 0x00d0, 0x0025};

static int f1dream_613eb_lookup[256] = {
0x0001, 0x00b5, 0x00b6, 0x00b6, 0x00b6, 0x00b6, 0x00b6, 0x00b6, 0x00b7, 0x0001, 0x00b8, 0x002f, 0x002f, 0x002f, 0x002f, 0x00b9,
0x00aa, 0x0031, 0x00ab, 0x00ab, 0x00ab, 0x00ac, 0x00ad, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x0091,
0x009c, 0x009d, 0x009e, 0x009f, 0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x009b, 0x0091,
0x00bc, 0x0092, 0x000b, 0x0009, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0073, 0x0001, 0x0098, 0x0099, 0x009a, 0x009b, 0x0091,
0x00bc, 0x007b, 0x000b, 0x0008, 0x0087, 0x0088, 0x0089, 0x008a, 0x007f, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f, 0x0090, 0x0091,
0x00bd, 0x007b, 0x000b, 0x0007, 0x007c, 0x007d, 0x007e, 0x0001, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086,
0x00bc, 0x0070, 0x000b, 0x0006, 0x0071, 0x0072, 0x0073, 0x0001, 0x0074, 0x000d, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a,
0x00bc, 0x00ba, 0x000a, 0x0005, 0x0065, 0x0066, 0x0067, 0x0068, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
0x00bc, 0x0059, 0x0001, 0x0004, 0x005a, 0x005b, 0x0001, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064,
0x0014, 0x004d, 0x0001, 0x0003, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0001, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058,
0x0014, 0x0043, 0x0001, 0x0002, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x00bb, 0x004a, 0x004b, 0x004c, 0x0001, 0x0001,
0x0014, 0x002b, 0x0001, 0x0038, 0x0039, 0x003a, 0x003b, 0x0031, 0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0001,
0x0014, 0x002d, 0x0001, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0001, 0x0014, 0x0037, 0x0001,
0x0014, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x0001, 0x0001, 0x0001, 0x002a, 0x002b, 0x002c,
0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001e, 0x001e, 0x001e, 0x001f, 0x0020,
0x000c, 0x000d, 0x000e, 0x0001, 0x000f, 0x0010, 0x0011, 0x0012, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x0013 };

static int f1dream_17b74_lookup[128] = {
0x0003, 0x0040, 0x0005, 0x0080, 0x0003, 0x0080, 0x0005, 0x00a0, 0x0003, 0x0040, 0x0005, 0x00c0, 0x0003, 0x0080, 0x0005, 0x00e0,
0x0003, 0x0040, 0x0006, 0x0000, 0x0003, 0x0080, 0x0006, 0x0020, 0x0003, 0x0040, 0x0006, 0x0040, 0x0003, 0x0080, 0x0006, 0x0060,
0x0000, 0x00a0, 0x0009, 0x00e0, 0x0000, 0x00e0, 0x000a, 0x0000, 0x0000, 0x00a0, 0x000a, 0x0020, 0x0000, 0x00e0, 0x000a, 0x0040,
0x0000, 0x00a0, 0x000a, 0x0060, 0x0000, 0x00e0, 0x000a, 0x0080, 0x0000, 0x00a0, 0x000a, 0x00a0, 0x0000, 0x00e0, 0x000a, 0x00c0,
0x0003, 0x0040, 0x0005, 0x0080, 0x0003, 0x0080, 0x0005, 0x00a0, 0x0003, 0x0040, 0x0005, 0x00c0, 0x0003, 0x0080, 0x0005, 0x00e0,
0x0003, 0x0040, 0x0006, 0x0000, 0x0003, 0x0080, 0x0006, 0x0020, 0x0003, 0x0040, 0x0006, 0x0040, 0x0003, 0x0080, 0x0006, 0x0060,
0x0000, 0x00a0, 0x0009, 0x00e0, 0x0000, 0x00e0, 0x000a, 0x0000, 0x0000, 0x00a0, 0x000a, 0x0020, 0x0000, 0x00e0, 0x000a, 0x0040,
0x0000, 0x00a0, 0x000a, 0x0060, 0x0000, 0x00e0, 0x000a, 0x0080, 0x0000, 0x00a0, 0x000a, 0x00a0, 0x0000, 0x00e0, 0x000a, 0x00c0 };

static int f1dream_2450_lookup[32] = {
0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0, 0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0,
0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0, 0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0 };

static void f1dream_protection_w(void)
{
	int indx;
	int value = 255;
	int prevpc = cpu_getpreviouspc();

	if (prevpc == 0x244c)
	{
		/* Called once, when a race is started.*/
		indx = READ_WORD(&ram[0x3ff0]);
		WRITE_WORD(&ram[0x3fe6],f1dream_2450_lookup[indx]);
		WRITE_WORD(&ram[0x3fe8],f1dream_2450_lookup[++indx]);
		WRITE_WORD(&ram[0x3fea],f1dream_2450_lookup[++indx]);
		WRITE_WORD(&ram[0x3fec],f1dream_2450_lookup[++indx]);
	}
	else if (prevpc == 0x613a)
	{
		/* Called for every sprite on-screen.*/
		if ((READ_WORD(&ram[0x3ff6])) < 15)
		{
			indx = f1dream_613ea_lookup[READ_WORD(&ram[0x3ff6])] - (READ_WORD(&ram[0x3ff4]));
			if (indx > 255)
			{
				indx <<= 4;
				indx += READ_WORD(&ram[0x3ff6]) & 0x00ff;
				value = f1dream_613eb_lookup[indx];
			}
		}

		WRITE_WORD(&ram[0x3ff2],value);
	}
	else if (prevpc == 0x17b70)
	{
		/* Called only before a real race, not a time trial.*/
		if ((READ_WORD(&ram[0x3ff0])) >= 0x04) indx = 128;
		else if ((READ_WORD(&ram[0x3ff0])) > 0x02) indx = 96;
		else if ((READ_WORD(&ram[0x3ff0])) == 0x02) indx = 64;
		else if ((READ_WORD(&ram[0x3ff0])) == 0x01) indx = 32;
		else indx = 0;

		indx += READ_WORD(&ram[0x3fee]);
		if (indx < 128)
		{
			WRITE_WORD(&ram[0x3fe6],f1dream_17b74_lookup[indx]);
			WRITE_WORD(&ram[0x3fe8],f1dream_17b74_lookup[++indx]);
			WRITE_WORD(&ram[0x3fea],f1dream_17b74_lookup[++indx]);
			WRITE_WORD(&ram[0x3fec],f1dream_17b74_lookup[++indx]);
		}
		else
		{
			WRITE_WORD(&ram[0x3fe6],0x00ff);
			WRITE_WORD(&ram[0x3fe8],0x00ff);
			WRITE_WORD(&ram[0x3fea],0x00ff);
			WRITE_WORD(&ram[0x3fec],0x00ff);
		}
	}
	else if ((prevpc == 0x27f8) || (prevpc == 0x511a) || (prevpc == 0x5142) || (prevpc == 0x516a))
	{
		/* The main CPU stuffs the byte for the soundlatch into 0xfffffd.*/
		soundlatch_w(2,READ_WORD(&ram[0x3ffc]));
	}
}

static WRITE_HANDLER( f1dream_control_w )
{
	//logerror("protection write, PC: %04x  FFE1 Value:%01x\n",cpu_get_pc(), ram[0x3fe1]);
	f1dream_protection_w();
}

static WRITE_HANDLER( tigeroad_soundcmd_w )
{
	if ((data & 0xff000000) == 0)
		soundlatch_w(offset,(data >> 8) & 0xff);
}

static WRITE_HANDLER( msm5205_w )
{
	MSM5205_reset_w(offset,(data>>7)&1);
	MSM5205_data_w(offset,data);
	MSM5205_vclk_w(offset,1);
	MSM5205_vclk_w(offset,0);
}

int tigeroad_interrupt(void)
{
	return 2;
}


/***************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xfe0800, 0xfe0cff, MRA_BANK1 },
	{ 0xfe0d00, 0xfe1807, MRA_BANK2 },
	{ 0xfe4000, 0xfe4001, input_port_0_r },
	{ 0xfe4002, 0xfe4003, input_port_1_r },
	{ 0xfe4004, 0xfe4005, input_port_2_r },
	{ 0xfec000, 0xfec7ff, MRA_BANK3 },
	{ 0xff8200, 0xff867f, paletteram_word_r },
	{ 0xffc000, 0xffffff, MRA_BANK4 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xfe0800, 0xfe0cff, MWA_BANK1, &spriteram, &spriteram_size },
	{ 0xfe0d00, 0xfe1807, MWA_BANK2 },  /* still part of OBJ RAM */
	{ 0xfe4000, 0xfe4001, tigeroad_videoctrl_w },	/* char bank, coin counters, + ? */
	/*{ 0xfe4002, 0xfe4003, tigeroad_soundcmd_w }, added by init_tigeroad() */
	{ 0xfec000, 0xfec7ff, MWA_BANK3, &videoram, &videoram_size },
	{ 0xfe8000, 0xfe8003, tigeroad_scroll_w },
	{ 0xfe800e, 0xfe800f, MWA_NOP },    /* fe800e = watchdog or IRQ acknowledge */
	{ 0xff8200, 0xff867f, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },
	{ 0xffc000, 0xffffff, MWA_BANK4, &ram },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8000, YM2203_status_port_0_r },
	{ 0xa000, 0xa000, YM2203_status_port_1_r },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8000, YM2203_control_port_0_w },
	{ 0x8001, 0x8001, YM2203_write_port_0_w },
	{ 0xa000, 0xa000, YM2203_control_port_1_w },
	{ 0xa001, 0xa001, YM2203_write_port_1_w },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x7f, 0x7f, soundlatch2_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sample_readmem[] =
{
	{ 0x0000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

/* yes, no RAM */
static struct MemoryWriteAddress sample_writemem[] =
{
	{ 0x0000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct IOReadPort sample_readport[] =
{
	{ 0x00, 0x00, soundlatch2_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort sample_writeport[] =
{
	{ 0x01, 0x01, msm5205_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( tigeroad )
	PORT_START  /* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* dipswitch */
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0200, "4" )
	PORT_DIPSETTING(      0x0100, "5" )
	PORT_DIPSETTING(      0x0000, "7" )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ))
	PORT_DIPSETTING(      0x0400, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x1800, 0x1800, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x1800, "20000 70000 70000" )
	PORT_DIPSETTING(      0x1000, "20000 80000 80000" )
	PORT_DIPSETTING(      0x0800, "30000 80000 80000" )
	PORT_DIPSETTING(      0x0000, "30000 90000 90000" )
	PORT_DIPNAME( 0x6000, 0x6000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x2000, "Very Easy" )
	PORT_DIPSETTING(      0x4000, "Easy" )
	PORT_DIPSETTING(      0x6000, "Normal" )
	PORT_DIPSETTING(      0x0000, "Difficult" )
	PORT_DIPNAME( 0x8000, 0x8000, "Allow Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( toramich )
	PORT_START  /* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* dipswitch */
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0200, "4" )
	PORT_DIPSETTING(      0x0100, "5" )
	PORT_DIPSETTING(      0x0000, "7" )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ))
	PORT_DIPSETTING(      0x0400, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0800, "20000 70000 70000" )
	PORT_DIPSETTING(      0x0000, "20000 80000 80000" )
	PORT_DIPNAME( 0x1000, 0x1000, "Allow Level Select" )
	PORT_DIPSETTING(      0x1000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x6000, 0x6000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x4000, "Easy" )
	PORT_DIPSETTING(      0x6000, "Normal" )
	PORT_DIPSETTING(      0x2000, "Difficult" )
	PORT_DIPSETTING(      0x0000, "Very Difficult" )
	PORT_DIPNAME( 0x8000, 0x8000, "Allow Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( f1dream )
	PORT_START  /* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* dipswitch */
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0200, "4" )
	PORT_DIPSETTING(      0x0100, "5" )
	PORT_DIPSETTING(      0x0000, "7" )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ))
	PORT_DIPSETTING(      0x0400, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x1800, 0x1800, "F1 Up Point" )
	PORT_DIPSETTING(      0x1800, "12" )
	PORT_DIPSETTING(      0x1000, "16" )
	PORT_DIPSETTING(      0x0800, "18" )
	PORT_DIPSETTING(      0x0000, "20" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x2000, "Normal" )
	PORT_DIPSETTING(      0x0000, "Difficult" )
	PORT_DIPNAME( 0x4000, 0x0000, "Version" )
	PORT_DIPSETTING(      0x0000, "International" )
	PORT_DIPSETTING(      0x4000, "Japan" )
	PORT_DIPNAME( 0x8000, 0x8000, "Allow Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Yes ) )
INPUT_PORTS_END



static struct GfxLayout text_layout =
{
	8,8,    /* character size */
	2048,   /* number of characters */
	2,      /* bits per pixel */
	{ 4, 0 }, /* plane offsets */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 }, /* x offsets */
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 }, /* y offsets */
	16*8
};

static struct GfxLayout tile_layout =
{
	32,32,  /* tile size */
	2048,   /* number of tiles */
	4,      /* bits per pixel */

	{ 2048*256*8+4, 2048*256*8+0, 4, 0 },

	{ /* x offsets */
		0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
		64*8+0, 64*8+1, 64*8+2, 64*8+3, 64*8+8+0, 64*8+8+1, 64*8+8+2, 64*8+8+3,
		2*64*8+0, 2*64*8+1, 2*64*8+2, 2*64*8+3, 2*64*8+8+0, 2*64*8+8+1, 2*64*8+8+2, 2*64*8+8+3,
		3*64*8+0, 3*64*8+1, 3*64*8+2, 3*64*8+3, 3*64*8+8+0, 3*64*8+8+1, 3*64*8+8+2, 3*64*8+8+3,
	},

	{ /* y offsets */
		0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16,
		24*16, 25*16, 26*16, 27*16, 28*16, 29*16, 30*16, 31*16
	},

	256*8
};

static struct GfxLayout sprite_layout =
{
	16,16,  /* tile size */
	4096,   /* number of tiles */
	4,      /* bits per pixel */
	{ 3*4096*32*8, 2*4096*32*8, 1*4096*32*8, 0*4096*32*8 }, /* plane offsets */
	{ /* x offsets */
		0, 1, 2, 3, 4, 5, 6, 7,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7
	},
	{ /* y offsets */
		0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8
	},
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &text_layout,   512, 16 },   /* colors 512-575 */
	{ REGION_GFX2, 0, &tile_layout,     0, 16 },   /* colors   0-255 */
	{ REGION_GFX3, 0, &sprite_layout, 256, 16 },   /* colors 256-511 */
	{ -1 } /* end of array */
};



/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_interface =
{
	2,          /* 2 chips */
	3579545,    /* 3.579 MHz ? */
	{ YM2203_VOL(25,25), YM2203_VOL(25,25) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};

static struct MSM5205interface msm5205_interface =
{
	1,		/* 1 chip */
	384000,	/* 384KHz ? */
	{ 0 },	/* interrupt function */
	{ MSM5205_SEX_4B },	/* 4KHz playback ?  */
	{ 100 }
};


static struct MachineDriver machine_driver_tigeroad =
{
	{
		{
			CPU_M68000,
			6000000, /* ? Main clock is 24MHz */
			readmem,writemem,0,0,
			tigeroad_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,    /* 4 Mhz ??? */
			sound_readmem,sound_writemem,0,sound_writeport,
			ignore_interrupt,0  /* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2203 */
		}
	},
	60, 2500,   /* frames per second, vblank duration */
				/* vblank duration hand tuned to get proper sprite/background alignment */
	1,  /* CPU slices per frame */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	576, 576,
	0, /* convert color prom routine */

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	0,
	0,
	tigeroad_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

/* same as above but with additional Z80 for samples playback */
static struct MachineDriver machine_driver_toramich =
{
	{
		{
			CPU_M68000,
			6000000, /* ? Main clock is 24MHz */
			readmem,writemem,0,0,
			tigeroad_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,    /* 4 Mhz ??? */
			sound_readmem,sound_writemem,0,sound_writeport,
			ignore_interrupt,0  /* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2203 */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? */
			sample_readmem,sample_writemem,sample_readport,sample_writeport,
			0,0,
			interrupt,4000	/* ? */
		}
	},
	60, 2500,   /* frames per second, vblank duration */
				/* vblank duration hand tuned to get proper sprite/background alignment */
	1,  /* CPU slices per frame */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	576, 576,
	0, /* convert color prom routine */

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	0,
	0,
	tigeroad_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( tigeroad )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "tru02.bin",    0x00000, 0x20000, 0x8d283a95 )
	ROM_LOAD_ODD ( "tru04.bin",    0x00000, 0x20000, 0x72e2ef20 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU */
	ROM_LOAD( "tru05.bin",    0x0000, 0x8000, 0xf9a7c9bf )

	/* no samples player in the English version */

	ROM_REGION( 0x008000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tr01.bin",     0x00000, 0x08000, 0x74a9f08c ) /* 8x8 text */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tr-01a.bin",   0x00000, 0x20000, 0xa8aa2e59 ) /* tiles */
	ROM_LOAD( "tr-04a.bin",   0x20000, 0x20000, 0x8863a63c )
	ROM_LOAD( "tr-02a.bin",   0x40000, 0x20000, 0x1a2c5f89 )
	ROM_LOAD( "tr05.bin",     0x60000, 0x20000, 0x5bf453b3 )
	ROM_LOAD( "tr-03a.bin",   0x80000, 0x20000, 0x1e0537ea )
	ROM_LOAD( "tr-06a.bin",   0xa0000, 0x20000, 0xb636c23a )
	ROM_LOAD( "tr-07a.bin",   0xc0000, 0x20000, 0x5f907d4d )
	ROM_LOAD( "tr08.bin",     0xe0000, 0x20000, 0xadee35e2 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tr-09a.bin",   0x00000, 0x20000, 0x3d98ad1e ) /* sprites */
	ROM_LOAD( "tr-10a.bin",   0x20000, 0x20000, 0x8f6f03d7 )
	ROM_LOAD( "tr-11a.bin",   0x40000, 0x20000, 0xcd9152e5 )
	ROM_LOAD( "tr-12a.bin",   0x60000, 0x20000, 0x7d8a99d0 )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "tr13.bin",     0x0000, 0x8000, 0xa79be1eb )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "trprom.bin",   0x0000, 0x0100, 0xec80ae36 )	/* unknown (palette bank select?) */
ROM_END

ROM_START( toramich )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "tr_02.bin",    0x00000, 0x20000, 0xb54723b1 )
	ROM_LOAD_ODD ( "tr_04.bin",    0x00000, 0x20000, 0xab432479 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU */
	ROM_LOAD( "tr_05.bin",    0x0000, 0x8000, 0x3ebe6e62 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* samples player */
	ROM_LOAD( "tr_03.bin",    0x0000, 0x10000, 0xea1807ef )

	ROM_REGION( 0x008000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tr01.bin",     0x00000, 0x08000, 0x74a9f08c ) /* 8x8 text */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tr-01a.bin",   0x00000, 0x20000, 0xa8aa2e59 ) /* tiles */
	ROM_LOAD( "tr-04a.bin",   0x20000, 0x20000, 0x8863a63c )
	ROM_LOAD( "tr-02a.bin",   0x40000, 0x20000, 0x1a2c5f89 )
	ROM_LOAD( "tr05.bin",     0x60000, 0x20000, 0x5bf453b3 )
	ROM_LOAD( "tr-03a.bin",   0x80000, 0x20000, 0x1e0537ea )
	ROM_LOAD( "tr-06a.bin",   0xa0000, 0x20000, 0xb636c23a )
	ROM_LOAD( "tr-07a.bin",   0xc0000, 0x20000, 0x5f907d4d )
	ROM_LOAD( "tr08.bin",     0xe0000, 0x20000, 0xadee35e2 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tr-09a.bin",   0x00000, 0x20000, 0x3d98ad1e ) /* sprites */
	ROM_LOAD( "tr-10a.bin",   0x20000, 0x20000, 0x8f6f03d7 )
	ROM_LOAD( "tr-11a.bin",   0x40000, 0x20000, 0xcd9152e5 )
	ROM_LOAD( "tr-12a.bin",   0x60000, 0x20000, 0x7d8a99d0 )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "tr13.bin",     0x0000, 0x8000, 0xa79be1eb )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "trprom.bin",   0x0000, 0x0100, 0xec80ae36 )	/* unknown (palette bank select?) */
ROM_END

ROM_START( f1dream )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "06j_02.bin",   0x00000, 0x20000, 0x3c2ec697 )
	ROM_LOAD_ODD ( "06k_03.bin",   0x00000, 0x20000, 0x85ebad91 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU */
	ROM_LOAD( "12k_04.bin",   0x0000, 0x8000, 0x4b9a7524 )

	ROM_REGION( 0x008000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "10d_01.bin",   0x00000, 0x08000, 0x361caf00 ) /* 8x8 text */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "03f_12.bin",   0x00000, 0x10000, 0xbc13e43c ) /* tiles */
	ROM_LOAD( "01f_10.bin",   0x10000, 0x10000, 0xf7617ad9 )
	ROM_LOAD( "03h_14.bin",   0x20000, 0x10000, 0xe33cd438 )
	/* 30000-7ffff empty */
	ROM_LOAD( "02f_11.bin",   0x80000, 0x10000, 0x4aa49cd7 )
	ROM_LOAD( "17f_09.bin",   0x90000, 0x10000, 0xca622155 )
	ROM_LOAD( "02h_13.bin",   0xa0000, 0x10000, 0x2a63961e )
	/* b0000-fffff empty */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "03b_06.bin",   0x00000, 0x10000, 0x5e54e391 ) /* sprites */
	/* 10000-1ffff empty */
	ROM_LOAD( "02b_05.bin",   0x20000, 0x10000, 0xcdd119fd )
	/* 30000-3ffff empty */
	ROM_LOAD( "03d_08.bin",   0x40000, 0x10000, 0x811f2e22 )
	/* 50000-5ffff empty */
	ROM_LOAD( "02d_07.bin",   0x60000, 0x10000, 0xaa9a1233 )
	/* 70000-7ffff empty */

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "07l_15.bin",   0x0000, 0x8000, 0x978758b7 )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "09e_tr.bin",   0x0000, 0x0100, 0xec80ae36 )	/* unknown (palette bank select?) */
ROM_END

ROM_START( f1dreamb )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "f1d_04.bin",   0x00000, 0x10000, 0x903febad )
	ROM_LOAD_ODD( "f1d_05.bin",   0x00000, 0x10000, 0x666fa2a7 )
	ROM_LOAD_EVEN( "f1d_02.bin",   0x20000, 0x10000, 0x98973c4c )
	ROM_LOAD_ODD( "f1d_03.bin",   0x20000, 0x10000, 0x3d21c78a )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU */
	ROM_LOAD( "12k_04.bin",   0x0000, 0x8000, 0x4b9a7524 )

	ROM_REGION( 0x008000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "10d_01.bin",   0x00000, 0x08000, 0x361caf00 ) /* 8x8 text */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "03f_12.bin",   0x00000, 0x10000, 0xbc13e43c ) /* tiles */
	ROM_LOAD( "01f_10.bin",   0x10000, 0x10000, 0xf7617ad9 )
	ROM_LOAD( "03h_14.bin",   0x20000, 0x10000, 0xe33cd438 )
	/* 30000-7ffff empty */
	ROM_LOAD( "02f_11.bin",   0x80000, 0x10000, 0x4aa49cd7 )
	ROM_LOAD( "17f_09.bin",   0x90000, 0x10000, 0xca622155 )
	ROM_LOAD( "02h_13.bin",   0xa0000, 0x10000, 0x2a63961e )
	/* b0000-fffff empty */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "03b_06.bin",   0x00000, 0x10000, 0x5e54e391 ) /* sprites */
	/* 10000-1ffff empty */
	ROM_LOAD( "02b_05.bin",   0x20000, 0x10000, 0xcdd119fd )
	/* 30000-3ffff empty */
	ROM_LOAD( "03d_08.bin",   0x40000, 0x10000, 0x811f2e22 )
	/* 50000-5ffff empty */
	ROM_LOAD( "02d_07.bin",   0x60000, 0x10000, 0xaa9a1233 )
	/* 70000-7ffff empty */

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "07l_15.bin",   0x0000, 0x8000, 0x978758b7 )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "09e_tr.bin",   0x0000, 0x0100, 0xec80ae36 )	/* unknown (palette bank select?) */
ROM_END



void init_tigeroad(void)
{
	install_mem_write_handler(0, 0xfe4002, 0xfe4003, tigeroad_soundcmd_w);
}

void init_f1dream(void)
{
	install_mem_write_handler(0, 0xfe4002, 0xfe4003, f1dream_control_w);
}



GAMEX( 1987, tigeroad, 0,        tigeroad, tigeroad, tigeroad, ROT0, "Capcom (Romstar license)", "Tiger Road (US)", GAME_NO_COCKTAIL )
GAMEX( 1987, toramich, tigeroad, toramich, toramich, tigeroad, ROT0, "Capcom", "Tora eno Michi (Japan)", GAME_NO_COCKTAIL )

/* F1 Dream has an Intel 8751 microcontroller for protection */
GAMEX( 1988, f1dream,  0,        tigeroad, f1dream,  f1dream,  ROT0, "Capcom (Romstar license)", "F-1 Dream", GAME_NO_COCKTAIL )
GAMEX( 1988, f1dreamb, f1dream,  tigeroad, f1dream,  tigeroad, ROT0, "bootleg", "F-1 Dream (bootleg)", GAME_NO_COCKTAIL )
