#include "../vidhrdw/dkong.cpp"
#include "../sndhrdw/dkong.cpp"

/***************************************************************************

TODO:
- Radarscope does a check on bit 6 of 7d00 which prevent it from working.
  It's a sound status flag, maybe signaling whan a tune is finished.
  For now, we comment it out.

- radarscp_grid_color_w() is wrong, it probably isn't supposed to change
  the grid color. There are reports of the grid being constantly blue in
  the real game, the flyer confirms this.


Donkey Kong and Donkey Kong Jr. memory map (preliminary) (DKong 3 follows)

0000-3fff ROM (Donkey Kong Jr.and Donkey Kong 3: 0000-5fff)
6000-6fff RAM
6900-6a7f sprites
7000-73ff ?
7400-77ff Video RAM
8000-9fff ROM (DK3 only)



memory mapped ports:

read:
7c00      IN0
7c80      IN1
7d00      IN2 (DK3: DSW2)
7d80      DSW1

*
 * IN0 (bits NOT inverted)
 * bit 7 : ?
 * bit 6 : reset (when player 1 active)
 * bit 5 : ?
 * bit 4 : JUMP player 1
 * bit 3 : DOWN player 1
 * bit 2 : UP player 1
 * bit 1 : LEFT player 1
 * bit 0 : RIGHT player 1
 *
*
 * IN1 (bits NOT inverted)
 * bit 7 : ?
 * bit 6 : reset (when player 2 active)
 * bit 5 : ?
 * bit 4 : JUMP player 2
 * bit 3 : DOWN player 2
 * bit 2 : UP player 2
 * bit 1 : LEFT player 2
 * bit 0 : RIGHT player 2
 *
*
 * IN2 (bits NOT inverted)
 * bit 7 : COIN (IS inverted in Radarscope)
 * bit 6 : ? Radarscope does some wizardry with this bit
 * bit 5 : ?
 * bit 4 : ?
 * bit 3 : START 2
 * bit 2 : START 1
 * bit 1 : ?
 * bit 0 : ? if this is 1, the code jumps to $4000, outside the rom space
 *
*
 * DSW1 (bits NOT inverted)
 * bit 7 : COCKTAIL or UPRIGHT cabinet (1 = UPRIGHT)
 * bit 6 : \ 000 = 1 coin 1 play   001 = 2 coins 1 play  010 = 1 coin 2 plays
 * bit 5 : | 011 = 3 coins 1 play  100 = 1 coin 3 plays  101 = 4 coins 1 play
 * bit 4 : / 110 = 1 coin 4 plays  111 = 5 coins 1 play
 * bit 3 : \bonus at
 * bit 2 : / 00 = 7000  01 = 10000  10 = 15000  11 = 20000
 * bit 1 : \ 00 = 3 lives  01 = 4 lives
 * bit 0 : / 10 = 5 lives  11 = 6 lives
 *

write:
7800-7803 ?
7808      ?
7c00      Background sound/music select:
          00 - nothing
		  01 - Intro tune
		  02 - How High? (intermisson) tune
		  03 - Out of time
		  04 - Hammer
		  05 - Rivet level 2 completed (end tune)
		  06 - Hammer hit
		  07 - Standard level end
		  08 - Background 1	(first screen)
		  09 - ???
		  0A - Background 3	(springs)
		  0B - Background 2 (rivet)
		  0C - Rivet level 1 completed (end tune)
		  0D - Rivet removed
		  0E - Rivet level completed
		  0F - Gorilla roar
7c80      gfx bank select (Donkey Kong Jr. only)
7d00      digital sound trigger - walk
7d01      digital sound trigger - jump
7d02      digital sound trigger - boom (gorilla stomps foot)
7d03      digital sound trigger - coin input/spring
7d04      digital sound trigger	- gorilla fall
7d05      digital sound trigger - barrel jump/prize
7d06      ?
7d07      ?
7d80      digital sound trigger - dead
7d82      flip screen
7d83      ?
7d84      interrupt enable
7d85      0/1 toggle
7d86-7d87 palette bank selector (only bit 0 is significant: 7d86 = bit 0 7d87 = bit 1)


8035 Memory Map:

0000-07ff ROM
0800-0fff Compressed sound sample (Gorilla roar in DKong)

Read ports:
0x20   Read current tune
P2.5   Active low when jumping
T0     Select sound for jump (Normal or Barrell?)
T1     Active low when gorilla is falling

Write ports:
P1     Digital out
P2.7   External decay
P2.6   Select second ROM reading (MOVX instruction will access area 800-fff)
P2.2-0 Select the bank of 256 bytes for second ROM



Donkey Kong 3 memory map (preliminary):

RAM and read ports same as above;

write:
7d00      ?
7d80      ?
7e00      ?
7e80
7e81      char bank selector
7e82      flipscreen
7e83      ?
7e84      interrupt enable
7e85      ?
7e86-7e87 palette bank selector (only bit 0 is significant: 7e86 = bit 0 7e87 = bit 1)


I/O ports

write:
00        ?

Changes:
	Apr 7 98 Howie Cohen
	* Added samples for the climb, jump, land and walking sounds

	Jul 27 99 Chad Hendrickson
	* Added cocktail mode flipscreen

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/i8039/i8039.h"
#include "cpu/s2650/s2650.h"
#include "cpu/m6502/m6502.h"

static int page = 0,mcustatus;
static int p[8] = { 255,255,255,255,255,255,255,255 };
static int t[2] = { 1,1 };


WRITE_HANDLER( radarscp_grid_enable_w );
WRITE_HANDLER( radarscp_grid_color_w );
WRITE_HANDLER( dkong_flipscreen_w );
WRITE_HANDLER( dkongjr_gfxbank_w );
WRITE_HANDLER( dkong3_gfxbank_w );
WRITE_HANDLER( dkong_palettebank_w );
void dkong_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void dkong3_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int dkong_vh_start(void);
void radarscp_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void dkong_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( dkong_sh_w );
WRITE_HANDLER( dkongjr_sh_death_w );
WRITE_HANDLER( dkongjr_sh_drop_w );
WRITE_HANDLER( dkongjr_sh_roar_w );
WRITE_HANDLER( dkongjr_sh_jump_w );
WRITE_HANDLER( dkongjr_sh_walk_w );
WRITE_HANDLER( dkongjr_sh_climb_w );
WRITE_HANDLER( dkongjr_sh_land_w );
WRITE_HANDLER( dkongjr_sh_snapjaw_w );

WRITE_HANDLER( dkong_sh1_w );

#define ACTIVELOW_PORT_BIT(P,A,D)   ((P & (~(1 << A))) | ((D ^ 1) << A))


WRITE_HANDLER( dkong_sh_sound3_w )     { p[2] = ACTIVELOW_PORT_BIT(p[2],5,data); }
WRITE_HANDLER( dkong_sh_sound4_w )    { t[1] = ~data & 1; }
WRITE_HANDLER( dkong_sh_sound5_w )    { t[0] = ~data & 1; }
WRITE_HANDLER( dkong_sh_tuneselect_w ) { soundlatch_w(offset,data ^ 0x0f); }

WRITE_HANDLER( dkongjr_sh_test6_w )      { p[2] = ACTIVELOW_PORT_BIT(p[2],6,data); }
WRITE_HANDLER( dkongjr_sh_test5_w )      { p[2] = ACTIVELOW_PORT_BIT(p[2],5,data); }
WRITE_HANDLER( dkongjr_sh_test4_w )      { p[2] = ACTIVELOW_PORT_BIT(p[2],4,data); }
WRITE_HANDLER( dkongjr_sh_tuneselect_w ) { soundlatch_w(offset,data); }

READ_HANDLER( hunchbks_mirror_r );
WRITE_HANDLER( hunchbks_mirror_w );

static READ_HANDLER( dkong_sh_p1_r )   { return p[1]; }
static READ_HANDLER( dkong_sh_p2_r )   { return p[2]; }
static READ_HANDLER( dkong_sh_t0_r )   { return t[0]; }
static READ_HANDLER( dkong_sh_t1_r )   { return t[1]; }
static READ_HANDLER( dkong_sh_tune_r )
{
	unsigned char *SND = memory_region(REGION_CPU2);
	if (page & 0x40)
	{
		switch (offset)
		{
			case 0x20:  return soundlatch_r(0);
		}
	}
	return (SND[2048+(page & 7)*256+offset]);
}

#include <math.h>

static float envelope,tt;
static int decay;

#define TSTEP 0.001

static WRITE_HANDLER( dkong_sh_p1_w )
{
	envelope=exp(-tt);
	DAC_data_w(0,(int)(data*envelope));
	if (decay) tt+=TSTEP;
	else tt=0;
}

static WRITE_HANDLER( dkong_sh_p2_w )
{
	/*   If P2.Bit7 -> is apparently an external signal decay or other output control
	 *   If P2.Bit6 -> activates the external compressed sample ROM
	 *   If P2.Bit4 -> status code to main cpu
	 *   P2.Bit2-0  -> select the 256 byte bank for external ROM
	 */

	decay = !(data & 0x80);
	page = (data & 0x47);
	mcustatus = ((~data & 0x10) >> 4);
}


static READ_HANDLER( dkong_in2_r )
{
	return input_port_2_r(offset) | (mcustatus << 6);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },	/* DK: 0000-3fff */
	{ 0x6000, 0x6fff, MRA_RAM },	/* including sprites RAM */
	{ 0x7400, 0x77ff, MRA_RAM },	/* video RAM */
	{ 0x7c00, 0x7c00, input_port_0_r },	/* IN0 */
	{ 0x7c80, 0x7c80, input_port_1_r },	/* IN1 */
	{ 0x7d00, 0x7d00, dkong_in2_r },	/* IN2/DSW2 */
	{ 0x7d80, 0x7d80, input_port_3_r },	/* DSW1 */
	{ 0x8000, 0x9fff, MRA_ROM },	/* DK3 and bootleg DKjr only */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress dkong3_readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },	/* DK: 0000-3fff */
	{ 0x6000, 0x6fff, MRA_RAM },	/* including sprites RAM */
	{ 0x7400, 0x77ff, MRA_RAM },	/* video RAM */
	{ 0x7c00, 0x7c00, input_port_0_r },	/* IN0 */
	{ 0x7c80, 0x7c80, input_port_1_r },	/* IN1 */
	{ 0x7d00, 0x7d00, input_port_2_r },	/* IN2/DSW2 */
	{ 0x7d80, 0x7d80, input_port_3_r },	/* DSW1 */
	{ 0x8000, 0x9fff, MRA_ROM },	/* DK3 and bootleg DKjr only */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress radarscp_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x7000, 0x73ff, MWA_RAM },    /* ???? */
	{ 0x7400, 0x77ff, videoram_w, &videoram, &videoram_size },
	{ 0x7800, 0x7803, MWA_RAM },	/* ???? */
	{ 0x7808, 0x7808, MWA_RAM },	/* ???? */
	{ 0x7c00, 0x7c00, dkong_sh_tuneselect_w },
	{ 0x7c80, 0x7c80, radarscp_grid_color_w },
	{ 0x7d00, 0x7d02, dkong_sh1_w },	/* walk/jump/boom sample trigger */
	{ 0x7d03, 0x7d03, dkong_sh_sound3_w },
	{ 0x7d04, 0x7d04, dkong_sh_sound4_w },
	{ 0x7d05, 0x7d05, dkong_sh_sound5_w },
	{ 0x7d80, 0x7d80, dkong_sh_w },
	{ 0x7d81, 0x7d81, radarscp_grid_enable_w },
	{ 0x7d82, 0x7d82, dkong_flipscreen_w },
	{ 0x7d83, 0x7d83, MWA_RAM },
	{ 0x7d84, 0x7d84, interrupt_enable_w },
	{ 0x7d85, 0x7d85, MWA_RAM },
	{ 0x7d86, 0x7d87, dkong_palettebank_w },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dkong_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x7000, 0x73ff, MWA_RAM },    /* ???? */
	{ 0x7400, 0x77ff, videoram_w, &videoram, &videoram_size },
	{ 0x7800, 0x7803, MWA_RAM },	/* ???? */
	{ 0x7808, 0x7808, MWA_RAM },	/* ???? */
	{ 0x7c00, 0x7c00, dkong_sh_tuneselect_w },
//	{ 0x7c80, 0x7c80,  },
	{ 0x7d00, 0x7d02, dkong_sh1_w },	/* walk/jump/boom sample trigger */
	{ 0x7d03, 0x7d03, dkong_sh_sound3_w },
	{ 0x7d04, 0x7d04, dkong_sh_sound4_w },
	{ 0x7d05, 0x7d05, dkong_sh_sound5_w },
	{ 0x7d80, 0x7d80, dkong_sh_w },
	{ 0x7d81, 0x7d81, MWA_RAM },	/* ???? */
	{ 0x7d82, 0x7d82, dkong_flipscreen_w },
	{ 0x7d83, 0x7d83, MWA_RAM },
	{ 0x7d84, 0x7d84, interrupt_enable_w },
	{ 0x7d85, 0x7d85, MWA_RAM },
	{ 0x7d86, 0x7d87, dkong_palettebank_w },
	{ -1 }	/* end of table */
};

READ_HANDLER( herbiedk_iack_r )
{
	s2650_set_sense(1);
    return 0;
}

static struct MemoryReadAddress hunchbkd_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x2000, 0x2fff, MRA_ROM },
	{ 0x4000, 0x4fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_ROM },
	{ 0x1400, 0x1400, input_port_0_r },		/* IN0 */
	{ 0x1480, 0x1480, input_port_1_r },		/* IN1 */
	{ 0x1500, 0x1500, input_port_2_r },		/* IN2/DSW2 */
    { 0x1507, 0x1507, herbiedk_iack_r },  	/* Clear Int */
	{ 0x1580, 0x1580, input_port_3_r },		/* DSW1 */
	{ 0x1600, 0x1bff, MRA_RAM },			/* video RAM */
	{ 0x1c00, 0x1fff, MRA_RAM },
    { 0x3000, 0x3fff, hunchbks_mirror_r },
    { 0x5000, 0x5fff, hunchbks_mirror_r },
    { 0x7000, 0x7fff, hunchbks_mirror_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress hunchbkd_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x2000, 0x2fff, MWA_ROM },
	{ 0x4000, 0x4fff, MWA_ROM },
	{ 0x6000, 0x6fff, MWA_ROM },
	{ 0x1400, 0x1400, dkong_sh_tuneselect_w },
	{ 0x1480, 0x1480, dkongjr_gfxbank_w },
	{ 0x1580, 0x1580, dkong_sh_w },
	{ 0x1582, 0x1582, dkong_flipscreen_w },
	{ 0x1584, 0x1584, MWA_RAM },			/* Possibly still interupt enable */
	{ 0x1585, 0x1585, MWA_RAM },			/* written a lot - every int */
	{ 0x1586, 0x1587, dkong_palettebank_w },
	{ 0x1600, 0x17ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x1800, 0x1bff, videoram_w, &videoram, &videoram_size },
	{ 0x1C00, 0x1fff, MWA_RAM },
    { 0x3000, 0x3fff, hunchbks_mirror_w },
    { 0x5000, 0x5fff, hunchbks_mirror_w },
    { 0x7000, 0x7fff, hunchbks_mirror_w },
    { -1 }  /* end of table */
};

int hunchloopback;

WRITE_HANDLER( hunchbkd_data_w )
{
	hunchloopback=data;
}

READ_HANDLER( hunchbkd_port0_r )
{
	//logerror("port 0 : pc = %4x\n",s2650_get_pc());

	switch (s2650_get_pc())
	{
		case 0x00e9:  return 0xff;
		case 0x0114:  return 0xfb;
	}

    return 0;
}

READ_HANDLER( hunchbkd_port1_r )
{
	return hunchloopback;
}

READ_HANDLER( herbiedk_port1_r )
{
	switch (s2650_get_pc())
	{
        case 0x002b:
		case 0x09dc:  return 0x0;
	}

    return 1;
}

static struct IOWritePort hunchbkd_writeport[] =
{
	{ 0x101, 0x101, hunchbkd_data_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort hunchbkd_readport[] =
{
	{ 0x00, 0x00, hunchbkd_port0_r },
	{ 0x01, 0x01, hunchbkd_port1_r },
	{ -1 }	/* end of table */
};

static struct IOReadPort herbiedk_readport[] =
{
	{ 0x01, 0x01, herbiedk_port1_r },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ -1 }	/* end of table */
};
static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ -1 }	/* end of table */
};
static struct IOReadPort readport_sound[] =
{
	{ 0x00,     0xff,     dkong_sh_tune_r },
	{ I8039_p1, I8039_p1, dkong_sh_p1_r },
	{ I8039_p2, I8039_p2, dkong_sh_p2_r },
	{ I8039_t0, I8039_t0, dkong_sh_t0_r },
	{ I8039_t1, I8039_t1, dkong_sh_t1_r },
	{ -1 }	/* end of table */
};
static struct IOWritePort writeport_sound[] =
{
	{ I8039_p1, I8039_p1, dkong_sh_p1_w },
	{ I8039_p2, I8039_p2, dkong_sh_p2_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport_hunchbkd_sound[] =
{
	{ I8039_bus,I8039_bus,soundlatch_r },
	{ I8039_p1, I8039_p1, dkong_sh_p1_r },
	{ I8039_p2, I8039_p2, dkong_sh_p2_r },
	{ I8039_t0, I8039_t0, dkong_sh_t0_r },
	{ I8039_t1, I8039_t1, dkong_sh_t1_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dkongjr_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x7400, 0x77ff, videoram_w, &videoram, &videoram_size },
	{ 0x7800, 0x7803, MWA_RAM },	/* ???? */
	{ 0x7808, 0x7808, MWA_RAM },	/* ???? */
	{ 0x7c00, 0x7c00, dkongjr_sh_tuneselect_w },
	{ 0x7c80, 0x7c80, dkongjr_gfxbank_w },
	{ 0x7c81, 0x7c81, dkongjr_sh_test6_w },
	{ 0x7d00, 0x7d00, dkongjr_sh_climb_w }, /* HC - climb sound */
	{ 0x7d01, 0x7d01, dkongjr_sh_jump_w }, /* HC - jump */
	{ 0x7d02, 0x7d02, dkongjr_sh_land_w }, /* HC - climb sound */
	{ 0x7d03, 0x7d03, dkongjr_sh_roar_w },
	{ 0x7d04, 0x7d04, dkong_sh_sound4_w },
	{ 0x7d05, 0x7d05, dkong_sh_sound5_w },
	{ 0x7d06, 0x7d06, dkongjr_sh_snapjaw_w },
	{ 0x7d07, 0x7d07, dkongjr_sh_walk_w },	/* controls pitch of the walk/climb? */
	{ 0x7d80, 0x7d80, dkongjr_sh_death_w },
	{ 0x7d81, 0x7d81, dkongjr_sh_drop_w },   /* active when Junior is falling */{ 0x7d84, 0x7d84, interrupt_enable_w },
	{ 0x7d82, 0x7d82, dkong_flipscreen_w },
	{ 0x7d86, 0x7d87, dkong_palettebank_w },
	{ 0x8000, 0x9fff, MWA_ROM },	/* bootleg DKjr only */
	{ -1 }	/* end of table */
};







WRITE_HANDLER( dkong3_2a03_reset_w )
{
	if (data & 1)
	{
		cpu_set_reset_line(1,CLEAR_LINE);
		cpu_set_reset_line(2,CLEAR_LINE);
	}
	else
	{
		cpu_set_reset_line(1,ASSERT_LINE);
		cpu_set_reset_line(2,ASSERT_LINE);
	}
}

static struct MemoryWriteAddress dkong3_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x7400, 0x77ff, videoram_w, &videoram, &videoram_size },
	{ 0x7c00, 0x7c00, soundlatch_w },
	{ 0x7c80, 0x7c80, soundlatch2_w },
	{ 0x7d00, 0x7d00, soundlatch3_w },
	{ 0x7d80, 0x7d80, dkong3_2a03_reset_w },
	{ 0x7e81, 0x7e81, dkong3_gfxbank_w },
	{ 0x7e82, 0x7e82, dkong_flipscreen_w },
	{ 0x7e84, 0x7e84, interrupt_enable_w },
	{ 0x7e85, 0x7e85, MWA_NOP },	/* ??? */
	{ 0x7e86, 0x7e87, dkong_palettebank_w },
	{ 0x8000, 0x9fff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct IOWritePort dkong3_writeport[] =
{
	{ 0x00, 0x00, IOWP_NOP },	/* ??? */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress dkong3_sound1_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM },
	{ 0x4016, 0x4016, soundlatch_r },
	{ 0x4017, 0x4017, soundlatch2_r },
	{ 0x4000, 0x4017, NESPSG_0_r },
	{ 0xe000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dkong3_sound1_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x4000, 0x4017, NESPSG_0_w },
	{ 0xe000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress dkong3_sound2_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM },
	{ 0x4016, 0x4016, soundlatch3_r },
	{ 0x4000, 0x4017, NESPSG_1_r },
	{ 0xe000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dkong3_sound2_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x4000, 0x4017, NESPSG_1_w },
	{ 0xe000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( dkong )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
//	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Service_Mode ) )
//	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
//	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* status from sound cpu */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "7000" )
	PORT_DIPSETTING(    0x04, "10000" )
	PORT_DIPSETTING(    0x08, "15000" )
	PORT_DIPSETTING(    0x0c, "20000" )
	PORT_DIPNAME( 0x70, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END


INPUT_PORTS_START( dkong3 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN3 )


	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_BITX(0x40, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x04, "40000" )
	PORT_DIPSETTING(    0x08, "50000" )
	PORT_DIPSETTING(    0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Additional Bonus" )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x10, "40000" )
	PORT_DIPSETTING(    0x20, "50000" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END


INPUT_PORTS_START( hunchbdk )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* status from sound cpu */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x04, "20000" )
	PORT_DIPSETTING(    0x08, "40000" )
	PORT_DIPSETTING(    0x0c, "80000" )
	PORT_DIPNAME( 0x70, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END

INPUT_PORTS_START( herbiedk )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* status from sound cpu */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x70, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END


static struct GfxLayout dkong_charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 256*8*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout dkongjr_charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 512*8*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout dkong_spritelayout =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	2,	/* 2 bits per pixel */
	{ 128*16*16, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,	/* the two halves of the sprite are separated */
			64*16*16+0, 64*16*16+1, 64*16*16+2, 64*16*16+3, 64*16*16+4, 64*16*16+5, 64*16*16+6, 64*16*16+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};
static struct GfxLayout dkong3_spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	2,	/* 2 bits per pixel */
	{ 256*16*16, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,	/* the two halves of the sprite are separated */
			128*16*16+0, 128*16*16+1, 128*16*16+2, 128*16*16+3, 128*16*16+4, 128*16*16+5, 128*16*16+6, 128*16*16+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo dkong_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &dkong_charlayout,   0, 64 },
	{ REGION_GFX2, 0x0000, &dkong_spritelayout, 0, 64 },
	{ -1 } /* end of array */
};
static struct GfxDecodeInfo dkongjr_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &dkongjr_charlayout, 0, 64 },
	{ REGION_GFX2, 0x0000, &dkong_spritelayout, 0, 64 },
	{ -1 } /* end of array */
};
static struct GfxDecodeInfo dkong3_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &dkongjr_charlayout,   0, 64 },
	{ REGION_GFX2, 0x0000, &dkong3_spritelayout,  0, 64 },
	{ -1 } /* end of array */
};


static struct DACinterface dkong_dac_interface =
{
	1,
	{ 55 }
};

static const char *dkong_sample_names[] =
{
	"*dkong",
	"effect00.wav",
	"effect01.wav",
	"effect02.wav",
	0	/* end of array */
};

static const char *dkongjr_sample_names[] =
{
	"*dkongjr",
	"jump.wav",
	"land.wav",
	"roar.wav",
	"climb.wav",   /* HC */
	"death.wav",  /* HC */
	"drop.wav",  /* HC */
	"walk.wav", /* HC */
	"snapjaw.wav",  /* HC */
	0	/* end of array */
};

static struct Samplesinterface dkong_samples_interface =
{
	8,	/* 8 channels */
	25,	/* volume */
	dkong_sample_names
};

static struct Samplesinterface dkongjr_samples_interface =
{
	8,	/* 8 channels */
	25,	/* volume */
	dkongjr_sample_names
};

static struct MachineDriver machine_driver_radarscp =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz (?) */
			readmem,radarscp_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_I8035 | CPU_AUDIO_CPU,
			6000000/15,	/* 6Mhz crystal */
			readmem_sound,writemem_sound,readport_sound,writeport_sound,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	dkong_gfxdecodeinfo,
	256+2, 64*4,	/* two extra colors for stars and radar grid */
	dkong_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY|VIDEO_MODIFIES_PALETTE,
	0,
	dkong_vh_start,
	generic_vh_stop,
	radarscp_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dkong_dac_interface
		},
		{
			SOUND_SAMPLES,
			&dkong_samples_interface
		}
	}
};

static struct MachineDriver machine_driver_dkong =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz (?) */
			readmem,dkong_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_I8035 | CPU_AUDIO_CPU,
			6000000/15,	/* 6Mhz crystal */
			readmem_sound,writemem_sound,readport_sound,writeport_sound,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	dkong_gfxdecodeinfo,
	256, 64*4,
	dkong_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	dkong_vh_start,
	generic_vh_stop,
	dkong_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dkong_dac_interface
		},
		{
			SOUND_SAMPLES,
			&dkong_samples_interface
		}
	}
};

static int hunchbkd_interrupt(void)
{
	return 0x03;	/* hunchbkd S2650 interrupt vector */
}

static struct MachineDriver machine_driver_hunchbkd =
{
	/* basic machine hardware */
	{
		{
			CPU_S2650,
			3072000,
			hunchbkd_readmem,hunchbkd_writemem,hunchbkd_readport,hunchbkd_writeport,
			hunchbkd_interrupt,1
		},
        {
			CPU_I8035 | CPU_AUDIO_CPU,
			6000000/15,	/* 6Mhz crystal */
			readmem_sound,writemem_sound,readport_hunchbkd_sound,writeport_sound,
			ignore_interrupt,1
		}
    },
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	dkong_gfxdecodeinfo,
	256, 64*4,
	dkong_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	dkong_vh_start,
	generic_vh_stop,
	dkong_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dkong_dac_interface
		}
	}
};

int herbiedk_interrupt(void)
{
	s2650_set_sense(0);
	return ignore_interrupt();
}

static struct MachineDriver machine_driver_herbiedk =
{
	/* basic machine hardware */
	{
		{
			CPU_S2650,
			3072000,
			hunchbkd_readmem,hunchbkd_writemem,herbiedk_readport,hunchbkd_writeport,
			herbiedk_interrupt,1
		},
        {
			CPU_I8035 | CPU_AUDIO_CPU,
			6000000/15,	/* 6Mhz crystal */
			readmem_sound,writemem_sound,readport_hunchbkd_sound,writeport_sound,
			ignore_interrupt,1
		}
    },
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	dkong_gfxdecodeinfo,
	256, 64*4,
	dkong_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	dkong_vh_start,
	generic_vh_stop,
	dkong_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dkong_dac_interface
		}
	}
};

static struct MachineDriver machine_driver_dkongjr =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz (?) */
			readmem,dkongjr_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_I8035 | CPU_AUDIO_CPU,
			6000000/15,	/* 6Mhz crystal */
			readmem_sound,writemem_sound,readport_sound,writeport_sound,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	dkongjr_gfxdecodeinfo,
	256, 64*4,
	dkong_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	dkong_vh_start,
	generic_vh_stop,
	dkong_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dkong_dac_interface
		},
		{
			SOUND_SAMPLES,
			&dkongjr_samples_interface
		}
	}
};



static struct NESinterface nes_interface =
{
	2,
	{ REGION_CPU2, REGION_CPU3 },
	{ 50, 50 },
};


static struct MachineDriver machine_driver_dkong3 =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			8000000/2,	/* 4 Mhz */
			dkong3_readmem,dkong3_writemem,0,dkong3_writeport,
			nmi_interrupt,1
		},
		{
			CPU_N2A03 | CPU_AUDIO_CPU,
			N2A03_DEFAULTCLOCK,
			dkong3_sound1_readmem,dkong3_sound1_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_N2A03 | CPU_AUDIO_CPU,
			N2A03_DEFAULTCLOCK,
			dkong3_sound2_readmem,dkong3_sound2_writemem,0,0,
			nmi_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	dkong3_gfxdecodeinfo,
	256, 64*4,
	dkong3_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	dkong_vh_start,
	generic_vh_stop,
	dkong_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NES,
			&nes_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( radarscp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "trs2c5fc",     0x0000, 0x1000, 0x40949e0d )
	ROM_LOAD( "trs2c5gc",     0x1000, 0x1000, 0xafa8c49f )
	ROM_LOAD( "trs2c5hc",     0x2000, 0x1000, 0x51b8263d )
	ROM_LOAD( "trs2c5kc",     0x3000, 0x1000, 0x1f0101f7 )
	/* space for diagnostic ROM */

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "trs2s3i",      0x0000, 0x0800, 0x78034f14 )
	/* socket 3J is empty */

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "trs2v3gc",     0x0000, 0x0800, 0xf095330e )
	ROM_LOAD( "trs2v3hc",     0x0800, 0x0800, 0x15a316f0 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "trs2v3dc",     0x0000, 0x0800, 0xe0bb0db9 )
	ROM_LOAD( "trs2v3cc",     0x0800, 0x0800, 0x6c4e7dad )
	ROM_LOAD( "trs2v3bc",     0x1000, 0x0800, 0x6fdd63f1 )
	ROM_LOAD( "trs2v3ac",     0x1800, 0x0800, 0xbbf62755 )

	ROM_REGION( 0x0800, REGION_GFX3 )	/* radar/star timing table */
	ROM_LOAD( "trs2v3ec",     0x0000, 0x0800, 0x0eca8d6b )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "rs2-x.xxx",    0x0000, 0x0100, 0x54609d61 ) /* palette low 4 bits (inverted) */
	ROM_LOAD( "rs2-c.xxx",    0x0100, 0x0100, 0x79a7d831 ) /* palette high 4 bits (inverted) */
	ROM_LOAD( "rs2-v.1hc",    0x0200, 0x0100, 0x1b828315 ) /* character color codes on a per-column basis */
ROM_END

ROM_START( dkong )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dk.5e",        0x0000, 0x1000, 0xba70b88b )
	ROM_LOAD( "dk.5c",        0x1000, 0x1000, 0x5ec461ec )
	ROM_LOAD( "dk.5b",        0x2000, 0x1000, 0x1c97d324 )
	ROM_LOAD( "dk.5a",        0x3000, 0x1000, 0xb9005ac0 )
	/* space for diagnostic ROM */

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dk.3h",        0x0000, 0x0800, 0x45a4ed06 )
	ROM_LOAD( "dk.3f",        0x0800, 0x0800, 0x4743fe92 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk.3n",        0x0000, 0x0800, 0x12c8c95d )
	ROM_LOAD( "dk.3p",        0x0800, 0x0800, 0x15e9c5e9 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk.7c",        0x0000, 0x0800, 0x59f8054d )
	ROM_LOAD( "dk.7d",        0x0800, 0x0800, 0x672e4714 )
	ROM_LOAD( "dk.7e",        0x1000, 0x0800, 0xfeaa59ee )
	ROM_LOAD( "dk.7f",        0x1800, 0x0800, 0x20f2ef7e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkong.2k",     0x0000, 0x0100, 0x1e82d375 ) /* palette low 4 bits (inverted) */
	ROM_LOAD( "dkong.2j",     0x0100, 0x0100, 0x2ab01dc8 ) /* palette high 4 bits (inverted) */
	ROM_LOAD( "dkong.5f",     0x0200, 0x0100, 0x44988665 ) /* character color codes on a per-column basis */
ROM_END

ROM_START( dkongjp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "5f.cpu",       0x0000, 0x1000, 0x424f2b11 )
	ROM_LOAD( "5g.cpu",       0x1000, 0x1000, 0xd326599b )
	ROM_LOAD( "5h.cpu",       0x2000, 0x1000, 0xff31ac89 )
	ROM_LOAD( "5k.cpu",       0x3000, 0x1000, 0x394d6007 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dk.3h",        0x0000, 0x0800, 0x45a4ed06 )
	ROM_LOAD( "dk.3f",        0x0800, 0x0800, 0x4743fe92 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk.3n",        0x0000, 0x0800, 0x12c8c95d )
	ROM_LOAD( "5k.vid",       0x0800, 0x0800, 0x3684f914 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk.7c",        0x0000, 0x0800, 0x59f8054d )
	ROM_LOAD( "dk.7d",        0x0800, 0x0800, 0x672e4714 )
	ROM_LOAD( "dk.7e",        0x1000, 0x0800, 0xfeaa59ee )
	ROM_LOAD( "dk.7f",        0x1800, 0x0800, 0x20f2ef7e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkong.2k",     0x0000, 0x0100, 0x1e82d375 ) /* palette low 4 bits (inverted) */
	ROM_LOAD( "dkong.2j",     0x0100, 0x0100, 0x2ab01dc8 ) /* palette high 4 bits (inverted) */
	ROM_LOAD( "dkong.5f",     0x0200, 0x0100, 0x44988665 ) /* character color codes on a per-column basis */
ROM_END

ROM_START( dkongjpo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "5f.cpu",       0x0000, 0x1000, 0x424f2b11 )
	ROM_LOAD( "5g.cpu",       0x1000, 0x1000, 0xd326599b )
	ROM_LOAD( "5h.bin",       0x2000, 0x1000, 0x1d28895d )
	ROM_LOAD( "5k.bin",       0x3000, 0x1000, 0x7961599c )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dk.3h",        0x0000, 0x0800, 0x45a4ed06 )
	ROM_LOAD( "dk.3f",        0x0800, 0x0800, 0x4743fe92 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk.3n",        0x0000, 0x0800, 0x12c8c95d )
	ROM_LOAD( "5k.vid",       0x0800, 0x0800, 0x3684f914 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk.7c",        0x0000, 0x0800, 0x59f8054d )
	ROM_LOAD( "dk.7d",        0x0800, 0x0800, 0x672e4714 )
	ROM_LOAD( "dk.7e",        0x1000, 0x0800, 0xfeaa59ee )
	ROM_LOAD( "dk.7f",        0x1800, 0x0800, 0x20f2ef7e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkong.2k",     0x0000, 0x0100, 0x1e82d375 ) /* palette low 4 bits (inverted) */
	ROM_LOAD( "dkong.2j",     0x0100, 0x0100, 0x2ab01dc8 ) /* palette high 4 bits (inverted) */
	ROM_LOAD( "dkong.5f",     0x0200, 0x0100, 0x44988665 ) /* character color codes on a per-column basis */
ROM_END

ROM_START( dkongjr )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dkj.5b",       0x0000, 0x1000, 0xdea28158 )
	ROM_CONTINUE(             0x3000, 0x1000 )
	ROM_LOAD( "dkj.5c",       0x2000, 0x0800, 0x6fb5faf6 )
	ROM_CONTINUE(             0x4800, 0x0800 )
	ROM_CONTINUE(             0x1000, 0x0800 )
	ROM_CONTINUE(             0x5800, 0x0800 )
	ROM_LOAD( "dkj.5e",       0x4000, 0x0800, 0xd042b6a8 )
	ROM_CONTINUE(             0x2800, 0x0800 )
	ROM_CONTINUE(             0x5000, 0x0800 )
	ROM_CONTINUE(             0x1800, 0x0800 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dkj.3h",       0x0000, 0x1000, 0x715da5f8 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.3n",       0x0000, 0x1000, 0x8d51aca9 )
	ROM_LOAD( "dkj.3p",       0x1000, 0x1000, 0x4ef64ba5 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.7c",       0x0000, 0x0800, 0xdc7f4164 )
	ROM_LOAD( "dkj.7d",       0x0800, 0x0800, 0x0ce7dcf6 )
	ROM_LOAD( "dkj.7e",       0x1000, 0x0800, 0x24d1ff17 )
	ROM_LOAD( "dkj.7f",       0x1800, 0x0800, 0x0f8c083f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkjrprom.2e",  0x0000, 0x0100, 0x463dc7ad )	/* palette low 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2f",  0x0100, 0x0100, 0x47ba0042 )	/* palette high 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2n",  0x0200, 0x0100, 0xdbf185bf )	/* character color codes on a per-column basis */
ROM_END

ROM_START( dkngjrjp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dkjr1",        0x0000, 0x1000, 0xec7e097f )
	ROM_CONTINUE(             0x3000, 0x1000 )
	ROM_LOAD( "dkjr2",        0x2000, 0x0800, 0xc0a18f0d )
	ROM_CONTINUE(             0x4800, 0x0800 )
	ROM_CONTINUE(             0x1000, 0x0800 )
	ROM_CONTINUE(             0x5800, 0x0800 )
	ROM_LOAD( "dkjr3",        0x4000, 0x0800, 0xa81dd00c )
	ROM_CONTINUE(             0x2800, 0x0800 )
	ROM_CONTINUE(             0x5000, 0x0800 )
	ROM_CONTINUE(             0x1800, 0x0800 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dkj.3h",       0x0000, 0x1000, 0x715da5f8 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkjr9",        0x0000, 0x1000, 0xa95c4c63 )
	ROM_LOAD( "dkjr10",       0x1000, 0x1000, 0xadc11322 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.7c",       0x0000, 0x0800, 0xdc7f4164 )
	ROM_LOAD( "dkj.7d",       0x0800, 0x0800, 0x0ce7dcf6 )
	ROM_LOAD( "dkj.7e",       0x1000, 0x0800, 0x24d1ff17 )
	ROM_LOAD( "dkj.7f",       0x1800, 0x0800, 0x0f8c083f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkjrprom.2e",  0x0000, 0x0100, 0x463dc7ad )	/* palette low 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2f",  0x0100, 0x0100, 0x47ba0042 )	/* palette high 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2n",  0x0200, 0x0100, 0xdbf185bf )	/* character color codes on a per-column basis */
ROM_END

ROM_START( dkjrjp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dkjp.5b",      0x0000, 0x1000, 0x7b48870b )
	ROM_CONTINUE(             0x3000, 0x1000 )
	ROM_LOAD( "dkjp.5c",      0x2000, 0x0800, 0x12391665 )
	ROM_CONTINUE(             0x4800, 0x0800 )
	ROM_CONTINUE(             0x1000, 0x0800 )
	ROM_CONTINUE(             0x5800, 0x0800 )
	ROM_LOAD( "dkjp.5e",      0x4000, 0x0800, 0x6c9f9103 )
	ROM_CONTINUE(             0x2800, 0x0800 )
	ROM_CONTINUE(             0x5000, 0x0800 )
	ROM_CONTINUE(             0x1800, 0x0800 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dkj.3h",       0x0000, 0x1000, 0x715da5f8 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.3n",       0x0000, 0x1000, 0x8d51aca9 )
	ROM_LOAD( "dkj.3p",       0x1000, 0x1000, 0x4ef64ba5 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.7c",       0x0000, 0x0800, 0xdc7f4164 )
	ROM_LOAD( "dkj.7d",       0x0800, 0x0800, 0x0ce7dcf6 )
	ROM_LOAD( "dkj.7e",       0x1000, 0x0800, 0x24d1ff17 )
	ROM_LOAD( "dkj.7f",       0x1800, 0x0800, 0x0f8c083f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkjrprom.2e",  0x0000, 0x0100, 0x463dc7ad )	/* palette low 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2f",  0x0100, 0x0100, 0x47ba0042 )	/* palette high 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2n",  0x0200, 0x0100, 0xdbf185bf )	/* character color codes on a per-column basis */
ROM_END

ROM_START( dkjrbl )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "djr1-c.5b",    0x0000, 0x1000, 0xffe9e1a5 )
	ROM_CONTINUE(             0x3000, 0x1000 )
	ROM_LOAD( "djr1-c.5c",    0x2000, 0x0800, 0x982e30e8 )
	ROM_CONTINUE(             0x4800, 0x0800 )
	ROM_CONTINUE(             0x1000, 0x0800 )
	ROM_CONTINUE(             0x5800, 0x0800 )
	ROM_LOAD( "djr1-c.5e",    0x4000, 0x0800, 0x24c3d325 )
	ROM_CONTINUE(             0x2800, 0x0800 )
	ROM_CONTINUE(             0x5000, 0x0800 )
	ROM_CONTINUE(             0x1800, 0x0800 )
	ROM_LOAD( "djr1-c.5a",    0x8000, 0x1000, 0xbb5f5180 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "dkj.3h",       0x0000, 0x1000, 0x715da5f8 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.3n",       0x0000, 0x1000, 0x8d51aca9 )
	ROM_LOAD( "dkj.3p",       0x1000, 0x1000, 0x4ef64ba5 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dkj.7c",       0x0000, 0x0800, 0xdc7f4164 )
	ROM_LOAD( "dkj.7d",       0x0800, 0x0800, 0x0ce7dcf6 )
	ROM_LOAD( "dkj.7e",       0x1000, 0x0800, 0x24d1ff17 )
	ROM_LOAD( "dkj.7f",       0x1800, 0x0800, 0x0f8c083f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkjrprom.2e",  0x0000, 0x0100, 0x463dc7ad )	/* palette low 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2f",  0x0100, 0x0100, 0x47ba0042 )	/* palette high 4 bits (inverted) */
	ROM_LOAD( "dkjrprom.2n",  0x0200, 0x0100, 0xdbf185bf )	/* character color codes on a per-column basis */
ROM_END

ROM_START( dkong3 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dk3c.7b",      0x0000, 0x2000, 0x38d5f38e )
	ROM_LOAD( "dk3c.7c",      0x2000, 0x2000, 0xc9134379 )
	ROM_LOAD( "dk3c.7d",      0x4000, 0x2000, 0xd22e2921 )
	ROM_LOAD( "dk3c.7e",      0x8000, 0x2000, 0x615f14b7 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound #1 */
	ROM_LOAD( "dk3c.5l",      0xe000, 0x2000, 0x7ff88885 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* sound #2 */
	ROM_LOAD( "dk3c.6h",      0xe000, 0x2000, 0x36d7200c )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk3v.3n",      0x0000, 0x1000, 0x415a99c7 )
	ROM_LOAD( "dk3v.3p",      0x1000, 0x1000, 0x25744ea0 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk3v.7c",      0x0000, 0x1000, 0x8ffa1737 )
	ROM_LOAD( "dk3v.7d",      0x1000, 0x1000, 0x9ac84686 )
	ROM_LOAD( "dk3v.7e",      0x2000, 0x1000, 0x0c0af3fb )
	ROM_LOAD( "dk3v.7f",      0x3000, 0x1000, 0x55c58662 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkc1-c.1d",    0x0000, 0x0200, 0xdf54befc ) /* palette red & green component */
	ROM_LOAD( "dkc1-c.1c",    0x0100, 0x0200, 0x66a77f40 ) /* palette blue component */
	ROM_LOAD( "dkc1-v.2n",    0x0200, 0x0100, 0x50e33434 )	/* character color codes on a per-column basis */
ROM_END

ROM_START( dkong3j )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dk3c.7b",      0x0000, 0x2000, 0x38d5f38e )
	ROM_LOAD( "dk3c.7c",      0x2000, 0x2000, 0xc9134379 )
	ROM_LOAD( "dk3c.7d",      0x4000, 0x2000, 0xd22e2921 )
	ROM_LOAD( "dk3cj.7e",     0x8000, 0x2000, 0x25b5be23 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound #1 */
	ROM_LOAD( "dk3c.5l",      0xe000, 0x2000, 0x7ff88885 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* sound #2 */
	ROM_LOAD( "dk3c.6h",      0xe000, 0x2000, 0x36d7200c )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk3v.3n",      0x0000, 0x1000, 0x415a99c7 )
	ROM_LOAD( "dk3v.3p",      0x1000, 0x1000, 0x25744ea0 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dk3v.7c",      0x0000, 0x1000, 0x8ffa1737 )
	ROM_LOAD( "dk3v.7d",      0x1000, 0x1000, 0x9ac84686 )
	ROM_LOAD( "dk3v.7e",      0x2000, 0x1000, 0x0c0af3fb )
	ROM_LOAD( "dk3v.7f",      0x3000, 0x1000, 0x55c58662 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dkc1-c.1d",    0x0000, 0x0200, 0xdf54befc ) /* palette red & green component */
	ROM_LOAD( "dkc1-c.1c",    0x0100, 0x0200, 0x66a77f40 ) /* palette blue component */
	ROM_LOAD( "dkc1-v.2n",    0x0200, 0x0100, 0x50e33434 )	/* character color codes on a per-column basis */
ROM_END

ROM_START( hunchbkd )
	ROM_REGION( 0x8000, REGION_CPU1 )	/* 32k for code */
	ROM_LOAD( "hb.5e",        0x0000, 0x1000, 0x4c3ac070 )
	ROM_LOAD( "hbsc-1.5c",    0x2000, 0x1000, 0x9b0e6234 )
	ROM_LOAD( "hb.5b",        0x4000, 0x1000, 0x4cde80f3 )
	ROM_LOAD( "hb.5a",        0x6000, 0x1000, 0xd60ef5b2 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "hb.3h",        0x0000, 0x0800, 0xa3c240d4 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hb.3n",        0x0000, 0x0800, 0x443ed5ac )
	ROM_LOAD( "hb.3p",        0x0800, 0x0800, 0x073e7b0c )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hb.7c",        0x0000, 0x0800, 0x3ba71686 )
	ROM_LOAD( "hb.7d",        0x0800, 0x0800, 0x5786948d )
	ROM_LOAD( "hb.7e",        0x1000, 0x0800, 0xf845e8ca )
	ROM_LOAD( "hb.7f",        0x1800, 0x0800, 0x52d20fea )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "hbprom.2e",    0x0000, 0x0100, 0x37aab98f )	/* palette low 4 bits (inverted) */
	ROM_LOAD( "hbprom.2f",    0x0100, 0x0100, 0x845b8dcc )	/* palette high 4 bits (inverted) */
	ROM_LOAD( "hbprom.2n",    0x0200, 0x0100, 0xdff9070a )	/* character color codes on a per-column basis */
ROM_END

ROM_START( herbiedk )
	ROM_REGION( 0x8000, REGION_CPU1 )	/* 32k for code */
	ROM_LOAD( "5f.cpu",        0x0000, 0x1000, 0xc7ab3ac6 )
	ROM_LOAD( "5g.cpu",        0x2000, 0x1000, 0xd1031aa6 )
	ROM_LOAD( "5h.cpu",        0x4000, 0x1000, 0xc0daf551 )
	ROM_LOAD( "5k.cpu",        0x6000, 0x1000, 0x67442242 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "3i.snd",        0x0000, 0x0800, 0x20e30406 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5h.vid",        0x0000, 0x0800, 0xea2a2547 )
	ROM_LOAD( "5k.vid",        0x0800, 0x0800, 0xa8d421c9 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "7c.clk",        0x0000, 0x0800, 0xaf646166 )
	ROM_LOAD( "7d.clk",        0x0800, 0x0800, 0xd8e15832 )
	ROM_LOAD( "7e.clk",        0x1000, 0x0800, 0x2f7e65fa )
	ROM_LOAD( "7f.clk",        0x1800, 0x0800, 0xad32d5ae )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "74s287.2k",     0x0000, 0x0100, 0x7dc0a381 ) /* palette high 4 bits (inverted) */
	ROM_LOAD( "74s287.2j",     0x0100, 0x0100, 0x0a440c00 ) /* palette low 4 bits (inverted) */
	ROM_LOAD( "74s287.vid",    0x0200, 0x0100, 0x5a3446cc ) /* character color codes on a per-column basis */
ROM_END

ROM_START( herocast )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	/* the loading addresses are most likely wrong */
	/* the ROMs are probably not contiguous. */
	/* For example there's a table which suddenly stops at */
	/* 1dff and resumes at 3e00 */
	ROM_LOAD( "red-dot.rgt",  0x0000, 0x2000, 0x9c4af229 )	/* encrypted */
	ROM_LOAD( "wht-dot.lft",  0x2000, 0x2000, 0xc10f9235 )	/* encrypted */
	/* space for diagnostic ROM */
	ROM_LOAD( "2532.3f",      0x4000, 0x1000, 0x553b89bb )	/* ??? contains unencrypted */
													/* code mapped at 3000 */

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "silver.3h",    0x0000, 0x0800, 0x67863ce9 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "pnk.3n",       0x0000, 0x0800, 0x574dfd7a )
	ROM_LOAD( "blk.3p",       0x0800, 0x0800, 0x16f7c040 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gold.7c",      0x0000, 0x0800, 0x5f5282ed )
	ROM_LOAD( "orange.7d",    0x0800, 0x0800, 0x075d99f5 )
	ROM_LOAD( "yellow.7e",    0x1000, 0x0800, 0xf6272e96 )
	ROM_LOAD( "violet.7f",    0x1800, 0x0800, 0xca020685 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "82s126.2e",    0x0000, 0x0100, 0x463dc7ad )	/* palette low 4 bits (inverted) */
	ROM_LOAD( "82s126.2f",    0x0100, 0x0100, 0x47ba0042 )	/* palette high 4 bits (inverted) */
	ROM_LOAD( "82s126.2n",    0x0200, 0x0100, 0x37aece4b )	/* character color codes on a per-column basis */
ROM_END



static void init_herocast(void)
{
	int A;
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* swap data lines D3 and D4, this fixes the text but nothing more. */
	for (A = 0;A < 0x4000;A++)
	{
		int v;

		v = RAM[A];
		RAM[A] = (v & 0xe7) | ((v & 0x10) >> 1) | ((v & 0x08) << 1);
	}
}



static void init_radarscp(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* TODO: Radarscope does a check on bit 6 of 7d00 which prevent it from working. */
	/* It's a sound status flag, maybe signaling when a tune is finished. */
	/* For now, we comment it out. */
	RAM[0x1e9c] = 0xc3;
	RAM[0x1e9d] = 0xbd;
}



GAMEX(1980, radarscp, 0,       radarscp, dkong,    radarscp, ROT90, "Nintendo", "Radar Scope", GAME_IMPERFECT_SOUND )
GAME( 1981, dkong,    0,       dkong,    dkong,    0,        ROT90, "Nintendo of America", "Donkey Kong (US)" )
GAME( 1981, dkongjp,  dkong,   dkong,    dkong,    0,        ROT90, "Nintendo", "Donkey Kong (Japan set 1)" )
GAME( 1981, dkongjpo, dkong,   dkong,    dkong,    0,        ROT90, "Nintendo", "Donkey Kong (Japan set 2)" )
GAME( 1982, dkongjr,  0,       dkongjr,  dkong,    0,        ROT90, "Nintendo of America", "Donkey Kong Junior (US)" )
GAME( 1982, dkngjrjp, dkongjr, dkongjr,  dkong,    0,        ROT90, "bootleg?", "Donkey Kong Jr. (Original Japanese)" )
GAME( 1982, dkjrjp,   dkongjr, dkongjr,  dkong,    0,        ROT90, "Nintendo", "Donkey Kong Junior (Japan)" )
GAME( 1982, dkjrbl,   dkongjr, dkongjr,  dkong,    0,        ROT90, "Nintendo of America", "Donkey Kong Junior (bootleg?)" )
GAME( 1983, dkong3,   0,       dkong3,   dkong3,   0,        ROT90, "Nintendo of America", "Donkey Kong 3 (US)" )
GAME( 1983, dkong3j,  dkong3,  dkong3,   dkong3,   0,        ROT90, "Nintendo", "Donkey Kong 3 (Japan)" )

GAMEX(1983, hunchbkd, 0,       hunchbkd, hunchbdk, 0,        ROT90, "Century", "Hunchback (Donkey Kong conversion)", GAME_WRONG_COLORS )
GAMEX(1984, herbiedk, 0,       herbiedk, herbiedk, 0,        ROT90, "CVS", "Herbie at the Olympics (DK conversion)", GAME_WRONG_COLORS )	/*"Seatongrove UK Ltd"*/
GAMEX(1984, herocast, 0,       dkong,    dkong,    herocast, ROT90, "Seatongrove (Crown license)", "herocast", GAME_NOT_WORKING )
