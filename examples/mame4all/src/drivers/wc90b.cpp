#include "../vidhrdw/wc90b.cpp"

/*
World Cup 90 bootleg driver
---------------------------

Ernesto Corvi
(ernesto@imagina.com)

CPU #1 : Handles background & foreground tiles, controllers, dipswitches.
CPU #2 : Handles sprites and palette
CPU #3 : Audio. The audio chip is a YM2203. I need help with this!.

Memory Layout:

CPU #1
0000-8000 ROM
8000-9000 RAM
a000-a800 Color Ram for background #1 tiles
a800-b000 Video Ram for background #1 tiles
c000-c800 Color Ram for background #2 tiles
c800-c000 Video Ram for background #2 tiles
e000-e800 Color Ram for foreground tiles
e800-f000 Video Ram for foreground tiles
f800-fc00 Common Ram with CPU #2
fd00-fd00 Stick 1, Coin 1 & Start 1 input port
fd02-fd02 Stick 2, Coin 2 & Start 2 input port
fd06-fc06 Dip Switch A
fd08-fc08 Dip Switch B

CPU #2
0000-c000 ROM
c000-d000 RAM
d000-d800 RAM Sprite Ram
e000-e800 RAM Palette Ram
f800-fc00 Common Ram with CPU #1

CPU #3
0000-0xc000 ROM
???????????

Notes:
-----
The bootleg video hardware is quite different from the original machine.
I could not figure out the encoding of the scrolling for the new
video hardware. The memory positions, in case anyone wants to try, are
the following ( CPU #1 memory addresses ):
fd06: scroll bg #1 X coordinate
fd04: scroll bg #1 Y coordinate
fd08: scroll bg #2 X coordinate
fd0a: scroll bg #2 Y coordinate
fd0e: ????

What i used instead, was the local copy kept in RAM. These values
are the ones the original machine uses. This will differ when trying
to use some of this code to write a driver for a similar tecmo bootleg.

Sprites are also very different. Theres a code snippet in the ROM
that converts the original sprites to the new format, wich only allows
16x16 sprites. That snippet also does some ( nasty ) clipping.

Colors are accurate. The graphics ROMs have been modified severely
and encoded in a different way from the original machine. Even if
sometimes it seems colors are not entirely correct, this is only due
to the crappy artwork of the person that did the bootleg.

Dip switches are not complete and they dont seem to differ from
the original machine.

Last but not least, the set of ROMs i have for Euro League seem to have
the sprites corrupted. The game seems to be exactly the same as the
World Cup 90 bootleg.
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


#define TEST_DIPS false /* enable to test unmapped dip switches */

extern unsigned char *wc90b_shared;

extern unsigned char *wc90b_tile_colorram, *wc90b_tile_videoram;
extern unsigned char *wc90b_tile_colorram2, *wc90b_tile_videoram2;
extern unsigned char *wc90b_scroll1xlo, *wc90b_scroll1xhi;
extern unsigned char *wc90b_scroll2xlo, *wc90b_scroll2xhi;
extern unsigned char *wc90b_scroll1ylo, *wc90b_scroll1yhi;
extern unsigned char *wc90b_scroll2ylo, *wc90b_scroll2yhi;

extern size_t wc90b_tile_videoram_size;
extern size_t wc90b_tile_videoram_size2;

int wc90b_vh_start( void );
void wc90b_vh_stop ( void );
READ_HANDLER( wc90b_tile_videoram_r );
WRITE_HANDLER( wc90b_tile_videoram_w );
READ_HANDLER( wc90b_tile_colorram_r );
WRITE_HANDLER( wc90b_tile_colorram_w );
READ_HANDLER( wc90b_tile_videoram2_r );
WRITE_HANDLER( wc90b_tile_videoram2_w );
READ_HANDLER( wc90b_tile_colorram2_r );
WRITE_HANDLER( wc90b_tile_colorram2_w );
READ_HANDLER( wc90b_shared_r );
WRITE_HANDLER( wc90b_shared_w );
void wc90b_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static WRITE_HANDLER( wc90b_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + ((data & 0xf8) << 8);
	cpu_setbank(1,&RAM[bankaddress]);
}

static WRITE_HANDLER( wc90b_bankswitch1_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU2);


	bankaddress = 0x10000 + ((data & 0xf8) << 8);
	cpu_setbank(2,&RAM[bankaddress]);
}

static WRITE_HANDLER( wc90b_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(2,/*Z80_NMI_INT*/-1000);
}

static struct MemoryReadAddress wc90b_readmem1[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_RAM }, /* Main RAM */
	{ 0xa000, 0xa7ff, wc90b_tile_colorram_r }, /* bg 1 color ram */
	{ 0xa800, 0xafff, wc90b_tile_videoram_r }, /* bg 1 tile ram */
	{ 0xc000, 0xc7ff, wc90b_tile_colorram2_r }, /* bg 2 color ram */
	{ 0xc800, 0xcfff, wc90b_tile_videoram2_r }, /* bg 2 tile ram */
	{ 0xe000, 0xe7ff, colorram_r }, /* fg color ram */
	{ 0xe800, 0xefff, videoram_r }, /* fg tile ram */
	{ 0xf000, 0xf7ff, MRA_BANK1 },
	{ 0xf800, 0xfbff, wc90b_shared_r },
	{ 0xfd00, 0xfd00, input_port_0_r }, /* Stick 1, Coin 1 & Start 1 */
	{ 0xfd02, 0xfd02, input_port_1_r }, /* Stick 2, Coin 2 & Start 2 */
	{ 0xfd06, 0xfd06, input_port_2_r }, /* DIP Switch A */
	{ 0xfd08, 0xfd08, input_port_3_r }, /* DIP Switch B */
	{ 0xfd00, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress wc90b_readmem2[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc1ff, MRA_RAM },
	{ 0xc200, 0xe1ff, MRA_RAM },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ 0xf000, 0xf7ff, MRA_BANK2 },
	{ 0xf800, 0xfbff, wc90b_shared_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress wc90b_writemem1[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8075, MWA_RAM },
	{ 0x8076, 0x8076, MWA_RAM, &wc90b_scroll1xlo },
	{ 0x8077, 0x8077, MWA_RAM, &wc90b_scroll1xhi },
	{ 0x8078, 0x8078, MWA_RAM, &wc90b_scroll1ylo },
	{ 0x8079, 0x8079, MWA_RAM, &wc90b_scroll1yhi },
	{ 0x807a, 0x807a, MWA_RAM, &wc90b_scroll2xlo },
	{ 0x807b, 0x807b, MWA_RAM, &wc90b_scroll2xhi },
	{ 0x807c, 0x807c, MWA_RAM, &wc90b_scroll2ylo },
	{ 0x807d, 0x807d, MWA_RAM, &wc90b_scroll2yhi },
	{ 0x807e, 0x9fff, MWA_RAM },
	{ 0xa000, 0xa7ff, wc90b_tile_colorram_w, &wc90b_tile_colorram },
	{ 0xa800, 0xafff, wc90b_tile_videoram_w, &wc90b_tile_videoram, &wc90b_tile_videoram_size },
	{ 0xc000, 0xc7ff, wc90b_tile_colorram2_w, &wc90b_tile_colorram2 },
	{ 0xc800, 0xcfff, wc90b_tile_videoram2_w, &wc90b_tile_videoram2, &wc90b_tile_videoram_size2 },
	{ 0xe000, 0xe7ff, colorram_w, &colorram },
	{ 0xe800, 0xefff, videoram_w, &videoram, &videoram_size },
	{ 0xf000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xfbff, wc90b_shared_w, &wc90b_shared },
	{ 0xfc00, 0xfc00, wc90b_bankswitch_w },
	{ 0xfd00, 0xfd00, wc90b_sound_command_w },
	/*  */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress wc90b_writemem2[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd7ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe000, 0xe7ff, paletteram_xxxxBBBBGGGGRRRR_swap_w, &paletteram },
	{ 0xf000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xfbff, wc90b_shared_w },
	{ 0xfc00, 0xfc00, wc90b_bankswitch1_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xe800, 0xe800, YM2203_status_port_0_r },
	{ 0xe801, 0xe801, YM2203_read_port_0_r },
	{ 0xec00, 0xec00, YM2203_status_port_1_r },
	{ 0xec01, 0xec01, YM2203_read_port_1_r },
	{ 0xf800, 0xf800, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xe800, 0xe800, YM2203_control_port_0_w },
	{ 0xe801, 0xe801, YM2203_write_port_0_w },
	{ 0xec00, 0xec00, YM2203_control_port_1_w },
	{ 0xec01, 0xec01, YM2203_write_port_1_w },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( wc90b )
	PORT_START	/* IN0 bit 0-5 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 bit 0-5 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* DSWA */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, "10 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Easy" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, "Continue Game countdown speed" )
	PORT_DIPSETTING(    0x40, "Normal (1 sec per number)" )
	PORT_DIPSETTING(    0x00, "Faster (56/60 per number)" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSWB */
	PORT_DIPNAME( 0x03, 0x03, "1 Player Game Time" )
	PORT_DIPSETTING(    0x01, "1:00" )
	PORT_DIPSETTING(    0x02, "1:30" )
	PORT_DIPSETTING(    0x03, "2:00" )
	PORT_DIPSETTING(    0x00, "2:30" )
	PORT_DIPNAME( 0x1c, 0x1c, "2 Player Game Time" )
	PORT_DIPSETTING(    0x0c, "1:00" )
	PORT_DIPSETTING(    0x14, "1:30" )
	PORT_DIPSETTING(    0x04, "2:00" )
	PORT_DIPSETTING(    0x18, "2:30" )
	PORT_DIPSETTING(    0x1c, "3:00" )
	PORT_DIPSETTING(    0x08, "3:30" )
	PORT_DIPSETTING(    0x10, "4:00" )
	PORT_DIPSETTING(    0x00, "5:00" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x80, "Japanese" )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	2048,	/* 2048 characters */
	4,	/* 4 bits per pixel */
	{ 0, 0x4000*8, 0x8000*8, 0xc000*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	16,16,	/* 16*16 characters */
	256,	/* 256 characters */
	4,	/* 4 bits per pixel */
	{ 0*0x20000*8, 1*0x20000*8, 2*0x20000*8, 3*0x20000*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		(0x1000*8)+0, (0x1000*8)+1, (0x1000*8)+2, (0x1000*8)+3, (0x1000*8)+4, (0x1000*8)+5, (0x1000*8)+6, (0x1000*8)+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		0x800*8, 0x800*8+1*8, 0x800*8+2*8, 0x800*8+3*8, 0x800*8+4*8, 0x800*8+5*8, 0x800*8+6*8, 0x800*8+7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 32*32 characters */
	4096,	/* 1024 characters */
	4,	/* 4 bits per pixel */
	{ 3*0x20000*8, 2*0x20000*8, 1*0x20000*8, 0*0x20000*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		(16*8)+0, (16*8)+1, (16*8)+2, (16*8)+3, (16*8)+4, (16*8)+5, (16*8)+6, (16*8)+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 8*8+1*8, 8*8+2*8, 8*8+3*8, 8*8+4*8, 8*8+5*8, 8*8+6*8, 8*8+7*8 },
	32*8	/* every char takes 128 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout,      	1*16*16, 16*16 },
	{ REGION_GFX2, 0x00000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x02000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x04000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x06000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x08000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x0a000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x0c000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x0e000, &tilelayout,			2*16*16, 16*16 },
	{ REGION_GFX2, 0x10000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x12000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x14000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x16000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x18000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x1a000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x1c000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX2, 0x1e000, &tilelayout,			3*16*16, 16*16 },
	{ REGION_GFX3, 0x00000, &spritelayout,		0*16*16, 16*16 }, // sprites
	{ -1 } /* end of array */
};



/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_nmi_line(2,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	2000000,	/* 2 MHz ????? */
	{ YM2203_VOL(25,25), YM2203_VOL(25,25) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};

static struct MachineDriver machine_driver_wc90b =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,	/* 6.0 Mhz ??? */
			wc90b_readmem1, wc90b_writemem1,0,0,
			interrupt,1
		},
		{
			CPU_Z80,
			6000000,	/* 6.0 Mhz ??? */
			wc90b_readmem2, wc90b_writemem2,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* 4 MHz ???? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* NMIs are triggered by the YM2203 */
								/* IRQs are triggered by the main CPU */
		}

	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	4*16*16, 4*16*16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	wc90b_vh_start,
	wc90b_vh_stop,
	wc90b_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

ROM_START( wc90b )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* 128k for code */
	ROM_LOAD( "a02.bin",      0x00000, 0x10000, 0x192a03dd )	/* c000-ffff is not used */
	ROM_LOAD( "a03.bin",      0x10000, 0x10000, 0xf54ff17a )	/* banked at f000-f7ff */

	ROM_REGION( 0x20000, REGION_CPU2 )	/* 96k for code */  /* Second CPU */
	ROM_LOAD( "a04.bin",      0x00000, 0x10000, 0x3d535e2f )	/* c000-ffff is not used */
	ROM_LOAD( "a05.bin",      0x10000, 0x10000, 0x9e421c4b )	/* banked at f000-f7ff */

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 192k for the audio CPU */
	ROM_LOAD( "a01.bin",      0x00000, 0x10000, 0x3d317622 )

	ROM_REGION( 0x010000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a06.bin",      0x000000, 0x04000, 0x3b5387b7 )
	ROM_LOAD( "a08.bin",      0x004000, 0x04000, 0xc622a5a3 )
	ROM_LOAD( "a10.bin",      0x008000, 0x04000, 0x0923d9f6 )
	ROM_LOAD( "a20.bin",      0x00c000, 0x04000, 0xb8dec83e )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a07.bin",      0x000000, 0x20000, 0x38c31817 )
	ROM_LOAD( "a09.bin",      0x020000, 0x20000, 0x32e39e29 )
	ROM_LOAD( "a11.bin",      0x040000, 0x20000, 0x5ccec796 )
	ROM_LOAD( "a21.bin",      0x060000, 0x20000, 0x0c54a091 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "146_a12.bin",  0x000000, 0x10000, 0xd5a60096 )
	ROM_LOAD( "147_a13.bin",  0x010000, 0x10000, 0x36bbf467 )
	ROM_LOAD( "148_a14.bin",  0x020000, 0x10000, 0x26371c18 )
	ROM_LOAD( "149_a15.bin",  0x030000, 0x10000, 0x75aa9b86 )
	ROM_LOAD( "150_a16.bin",  0x040000, 0x10000, 0x0da825f9 )
	ROM_LOAD( "151_a17.bin",  0x050000, 0x10000, 0x228429d8 )
	ROM_LOAD( "152_a18.bin",  0x060000, 0x10000, 0x516b6c09 )
	ROM_LOAD( "153_a19.bin",  0x070000, 0x10000, 0xf36390a9 )
ROM_END


void init_wc90b(void)
{
	int i;

	/* sprite graphics are inverted */
	for (i = 0; i < memory_region_length(REGION_GFX3); i++)
		memory_region(REGION_GFX3)[i] ^= 0xff;
}


GAMEX( 1989, wc90b, wc90, wc90b, wc90b, wc90b, ROT0, "bootleg", "Euro League", GAME_NO_COCKTAIL )
