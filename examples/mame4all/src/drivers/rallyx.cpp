#include "../vidhrdw/rallyx.cpp"

/***************************************************************************

Rally X memory map (preliminary)

driver by Nicola Salmoria


0000-3fff ROM
8000-83ff Radar video RAM + other
8400-87ff video RAM
8800-8bff Radar color RAM + other
8c00-8fff color RAM
9800-9fff RAM

memory mapped ports:

read:
a000      IN0
a080      IN1
a100      DSW1

write:
8014-801f sprites - 6 pairs: code (including flipping) and X position
8814-881f sprites - 6 pairs: Y position and color
8034-880c radar car indicators x position
8834-883c radar car indicators y position
a004-a00c radar car indicators color and x position MSB
a080      watchdog reset
a105      sound voice 1 waveform (nibble)
a111-a113 sound voice 1 frequency (nibble)
a115      sound voice 1 volume (nibble)
a10a      sound voice 2 waveform (nibble)
a116-a118 sound voice 2 frequency (nibble)
a11a      sound voice 2 volume (nibble)
a10f      sound voice 3 waveform (nibble)
a11b-a11d sound voice 3 frequency (nibble)
a11f      sound voice 3 volume (nibble)
a130      virtual screen X scroll position
a140      virtual screen Y scroll position
a170      ? this is written to A LOT of times every frame
a180      explosion sound trigger
a181      interrupt enable
a182      ?
a183      flip screen
a184      1 player start lamp
a185      2 players start lamp
a186      coin lockout
a187	  coin counter

I/O ports:
OUT on port $0 sets the interrupt vector/instruction (the game uses both
IM 2 and IM 0)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



WRITE_HANDLER( pengo_sound_w );
extern unsigned char *pengo_soundregs;

extern unsigned char *rallyx_videoram2,*rallyx_colorram2;
extern unsigned char *rallyx_radarx,*rallyx_radary,*rallyx_radarattr;
extern size_t rallyx_radarram_size;
extern unsigned char *rallyx_scrollx,*rallyx_scrolly;
WRITE_HANDLER( rallyx_videoram2_w );
WRITE_HANDLER( rallyx_colorram2_w );
WRITE_HANDLER( rallyx_spriteram_w );
WRITE_HANDLER( rallyx_flipscreen_w );
void rallyx_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int rallyx_vh_start(void);
void rallyx_vh_stop(void);
void rallyx_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static WRITE_HANDLER( rallyx_coin_lockout_w )
{
	coin_lockout_w(offset, data ^ 1);
}

static WRITE_HANDLER( rallyx_leds_w )
{
	osd_led_w(offset,data);
}

static WRITE_HANDLER( rallyx_play_sound_w )
{
	static int last;


	if (data == 0 && last != 0)
		sample_start(0,0,0);

	last = data;
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9800, 0x9fff, MRA_RAM },
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa080, 0xa080, input_port_1_r },	/* IN1 */
	{ 0xa100, 0xa100, input_port_2_r },	/* DSW1 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, rallyx_videoram2_w, &rallyx_videoram2 },
	{ 0x8800, 0x8bff, colorram_w, &colorram },
	{ 0x8c00, 0x8fff, rallyx_colorram2_w, &rallyx_colorram2 },
	{ 0x9800, 0x9fff, MWA_RAM },
	{ 0xa004, 0xa00f, MWA_RAM, &rallyx_radarattr },
	{ 0xa080, 0xa080, watchdog_reset_w },
	{ 0xa100, 0xa11f, pengo_sound_w, &pengo_soundregs },
	{ 0xa130, 0xa130, MWA_RAM, &rallyx_scrollx },
	{ 0xa140, 0xa140, MWA_RAM, &rallyx_scrolly },
	//{ 0xa170, 0xa170, MWA_NOP },	/* ????? */
	{ 0xa180, 0xa180, rallyx_play_sound_w },
	{ 0xa181, 0xa181, interrupt_enable_w },
	{ 0xa183, 0xa183, rallyx_flipscreen_w },
	{ 0xa184, 0xa185, rallyx_leds_w },
	{ 0xa186, 0xa186, rallyx_coin_lockout_w },
	{ 0xa187, 0xa187, coin_counter_w },
	{ 0x8014, 0x801f, MWA_RAM, &spriteram, &spriteram_size },	/* these are here just to initialize */
	{ 0x8814, 0x881f, MWA_RAM, &spriteram_2 },	/* the pointers. */
	{ 0x8034, 0x803f, MWA_RAM, &rallyx_radarx, &rallyx_radarram_size },	/* ditto */
	{ 0x8834, 0x883f, MWA_RAM, &rallyx_radary },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0, 0, interrupt_vector_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( rallyx )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT |IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW0 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	/* TODO: the bonus score depends on the number of lives */
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "A" )
	PORT_DIPSETTING(    0x04, "B" )
	PORT_DIPSETTING(    0x06, "C" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x38, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "1 Car, Medium" )
	PORT_DIPSETTING(    0x28, "1 Car, Hard" )
	PORT_DIPSETTING(    0x00, "2 Cars, Easy" )
	PORT_DIPSETTING(    0x18, "2 Cars, Medium" )
	PORT_DIPSETTING(    0x30, "2 Cars, Hard" )
	PORT_DIPSETTING(    0x08, "3 Cars, Easy" )
	PORT_DIPSETTING(    0x20, "3 Cars, Medium" )
	PORT_DIPSETTING(    0x38, "3 Cars, Hard" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
INPUT_PORTS_END

INPUT_PORTS_START( nrallyx )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT |IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW0 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	/* TODO: the bonus score depends on the number of lives */
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "A" )
	PORT_DIPSETTING(    0x04, "B" )
	PORT_DIPSETTING(    0x06, "C" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "1 Car, Medium" )
	PORT_DIPSETTING(    0x28, "1 Car, Hard" )
	PORT_DIPSETTING(    0x18, "2 Cars, Medium" )
	PORT_DIPSETTING(    0x30, "2 Cars, Hard" )
	PORT_DIPSETTING(    0x00, "3 Cars, Easy" )
	PORT_DIPSETTING(    0x20, "3 Cars, Medium" )
	PORT_DIPSETTING(    0x38, "3 Cars, Hard" )
	PORT_DIPSETTING(    0x08, "4 Cars, Easy" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },	/* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,	/* bits are packed in groups of four */
			 24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxLayout dotlayout =
{
	4,4,	/* 4*4 characters */
	8,	/* 8 characters */
	2,	/* 2 bits per pixel */
	{ 6, 7 },
	{ 3*8, 2*8, 1*8, 0*8 },
	{ 3*32, 2*32, 1*32, 0*32 },
	16*8	/* every char takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,        0, 64 },
	{ REGION_GFX1, 0, &spritelayout,      0, 64 },
	{ REGION_GFX2, 0, &dotlayout,      64*4,  1 },
	{ -1 } /* end of array */
};



static struct namco_interface namco_interface =
{
	3072000/32,	/* sample rate */
	3,			/* number of voices */
	100,		/* playback volume */
	REGION_SOUND1	/* memory region */
};

static const char *rallyx_sample_names[] =
{
	"*rallyx",
	"bang.wav",
	0	/* end of array */
};

static struct Samplesinterface samples_interface =
{
	1,	/* 1 channel */
	80,	/* volume */
	rallyx_sample_names
};



static struct MachineDriver machine_driver_rallyx =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ? */
			readmem,writemem,0,writeport,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	32,64*4+4,
	rallyx_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	rallyx_vh_start,
	rallyx_vh_stop,
	rallyx_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rallyx )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1b",           0x0000, 0x1000, 0x5882700d )
	ROM_LOAD( "rallyxn.1e",   0x1000, 0x1000, 0xed1eba2b )
	ROM_LOAD( "rallyxn.1h",   0x2000, 0x1000, 0x4f98dd1c )
	ROM_LOAD( "rallyxn.1k",   0x3000, 0x1000, 0x9aacccf0 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "8e",           0x0000, 0x1000, 0x277c1de5 )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "im5623.8m",    0x0000, 0x0100, 0x3c16f62c )	/* dots */

	ROM_REGION( 0x0120, REGION_PROMS )
	ROM_LOAD( "m3-7603.11n",  0x0000, 0x0020, 0xc7865434 )
	ROM_LOAD( "im5623.8p",    0x0020, 0x0100, 0x834d4fda )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound proms */
	ROM_LOAD( "im5623.3p",    0x0000, 0x0100, 0x4bad7017 )
	ROM_LOAD( "im5623.2m",    0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( rallyxm )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1b",           0x0000, 0x1000, 0x5882700d )
	ROM_LOAD( "1e",           0x1000, 0x1000, 0x786585ec )
	ROM_LOAD( "1h",           0x2000, 0x1000, 0x110d7dcd )
	ROM_LOAD( "1k",           0x3000, 0x1000, 0x473ab447 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "8e",           0x0000, 0x1000, 0x277c1de5 )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "im5623.8m",    0x0000, 0x0100, 0x3c16f62c )	/* dots */

	ROM_REGION( 0x0120, REGION_PROMS )
	ROM_LOAD( "m3-7603.11n",  0x0000, 0x0020, 0xc7865434 )
	ROM_LOAD( "im5623.8p",    0x0020, 0x0100, 0x834d4fda )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound proms */
	ROM_LOAD( "im5623.3p",    0x0000, 0x0100, 0x4bad7017 )
	ROM_LOAD( "im5623.2m",    0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( nrallyx )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "nrallyx.1b",   0x0000, 0x1000, 0x9404c8d6 )
	ROM_LOAD( "nrallyx.1e",   0x1000, 0x1000, 0xac01bf3f )
	ROM_LOAD( "nrallyx.1h",   0x2000, 0x1000, 0xaeba29b5 )
	ROM_LOAD( "nrallyx.1k",   0x3000, 0x1000, 0x78f17da7 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nrallyx.8e",   0x0000, 0x1000, 0xca7a174a )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "im5623.8m",    0x0000, 0x0100, BADCRC( 0x3c16f62c ) )	/* dots */

	ROM_REGION( 0x0120, REGION_PROMS )
	ROM_LOAD( "nrallyx.pr1",  0x0000, 0x0020, 0xa0a49017 )
	ROM_LOAD( "nrallyx.pr2",  0x0020, 0x0100, 0xb2b7ca15 )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound proms */
	ROM_LOAD( "nrallyx.spr",  0x0000, 0x0100, 0xb75c4e87 )
	ROM_LOAD( "im5623.2m",    0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END



GAME( 1980, rallyx,  0,      rallyx, rallyx,  0, ROT0, "Namco", "Rally X" )
GAME( 1980, rallyxm, rallyx, rallyx, rallyx,  0, ROT0, "[Namco] (Midway license)", "Rally X (Midway)" )
GAME( 1981, nrallyx, 0,      rallyx, nrallyx, 0, ROT0, "Namco", "New Rally X" )
