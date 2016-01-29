#include "../sndhrdw/senjyo.cpp"
#include "../vidhrdw/senjyo.cpp"

/***************************************************************************

Senjyo / Star Force / Baluba-louk

driver by Mirko Buffoni

TODO:
- wrong background colors in baluba, intermissions after round 13


This board was obviously born to run Senjyo. Four scrolling layers, gradient
background, sprite/background priorities, and even a small bitmap for the
radar. Star Force uses only a small subset of the features.

MAIN BOARD:
0000-7fff ROM
8000-8fff RAM
9000-93ff Video RAM
9400-97ff Color RAM
9800-987f Sprites
9c00-9dff Palette RAM
a000-a37f Background Video RAM #3
a800-aaff Background Video RAM #2
b000-b1ff Background Video RAM #1
b800-bbff Radar bitmap

read:
d000      IN0
d001      IN1
d002      IN2
d003      ?
d004      DSW1
d005      DSW2

write:
9e20-9e21 background #1 x position
9e25      background #1 y position
9e28-9e29 background #? x position ??
9e30-9e31 background #2 & #3 x position
9e35      background #2 & #3 y position
d000      flip screen
d002      watchdog reset?
          IN0/IN1 latch ? ( write before read IN0/IN1 )
d004      sound command ( pio-a )

SOUND BOARD
memory read/write
0000-3fff ROM
4000-43ff RAM

write
8000 sound chip channel 1 1st 9f,bf,df,ff
9000 sound chip channel 2 1st 9f,bf,df,ff
a000 sound chip channel 3 1st 9f,bf,df,ff
d000 bit 0-3 single sound volume ( freq = ctc2 )
e000 ? ( initialize only )
f000 ? ( initialize only )

I/O read/write
00   z80pio-A data     ( from sound command )
01   z80pio-A controll ( mode 1 input )
02   z80pio-B data     ( no use )
03   z80pio-B controll ( mode 3 bit i/o )
08   z80ctc-ch1        ( timer mode cysclk/16, bas clock 15.625KHz )
09   z80ctc-ch2        ( cascade from ctc-1  , tempo interrupt 88.778Hz )
0a   z80ctc-ch3        ( timer mode , single sound freq. )
0b   z80ctc-ch4        ( no use )

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/z80fmly.h"



extern unsigned char *senjyo_fgscroll;
extern unsigned char *senjyo_bgstripes;
extern unsigned char *senjyo_scrollx1,*senjyo_scrolly1;
extern unsigned char *senjyo_scrollx2,*senjyo_scrolly2;
extern unsigned char *senjyo_scrollx3,*senjyo_scrolly3;
extern unsigned char *senjyo_fgvideoram,*senjyo_fgcolorram;
extern unsigned char *senjyo_bg1videoram,*senjyo_bg2videoram,*senjyo_bg3videoram;
extern unsigned char *senjyo_radarram;
WRITE_HANDLER( senjyo_fgvideoram_w );
WRITE_HANDLER( senjyo_fgcolorram_w );
WRITE_HANDLER( senjyo_bg1videoram_w );
WRITE_HANDLER( senjyo_bg2videoram_w );
WRITE_HANDLER( senjyo_bg3videoram_w );
WRITE_HANDLER( senjyo_bgstripes_w );
WRITE_HANDLER( senjyo_flipscreen_w );

void init_starforc(void);
void init_starfore(void);
void init_senjyo(void);

int senjyo_vh_start(void);
void senjyo_vh_stop(void);
void senjyo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int senjyo_sh_start(const struct MachineSound *msound);
void senjyo_sh_stop(void);
void senjyo_sh_update(void);

WRITE_HANDLER( senjyo_sh_0_w );
WRITE_HANDLER( senjyo_sh_1_w );
WRITE_HANDLER( senjyo_sh_2_w );

WRITE_HANDLER( starforc_pio_w );
READ_HANDLER( starforc_pio_r );

#if 1
WRITE_HANDLER( senjyo_volume_w );
#endif



static int int_delay_kludge;

void senjyo_init_machine(void)
{
	/* we must avoid generating interrupts for the first few frames otherwise */
	/* Senjyo locks up. There must be an interrupt enable port somewhere, */
	/* or maybe interrupts are genenrated by the CTC. */
	/* Maybe a write to port d002 clears the IRQ line, but I'm not sure. */
	int_delay_kludge = 10;
}

int senjyo_interrupt(void)
{
	if (int_delay_kludge == 0) return interrupt();
	else int_delay_kludge--;
	return ignore_interrupt();
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x97ff, MRA_RAM },
	{ 0x9800, 0x987f, MRA_RAM },
	{ 0x9c00, 0x9d8f, MRA_RAM },
	{ 0x9e00, 0x9e3f, MRA_RAM },
	{ 0xa000, 0xa37f, MRA_RAM },
	{ 0xa800, 0xaaff, MRA_RAM },
	{ 0xb000, 0xb1ff, MRA_RAM },
	{ 0xb800, 0xbbff, MRA_RAM },
	{ 0xd000, 0xd000, input_port_0_r },	/* player 1 input */
	{ 0xd001, 0xd001, input_port_1_r },	/* player 2 input */
	{ 0xd002, 0xd002, input_port_2_r },	/* coin */
	{ 0xd004, 0xd004, input_port_3_r },	/* DSW1 */
	{ 0xd005, 0xd005, input_port_4_r },	/* DSW2 */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },
	{ 0x9000, 0x93ff, senjyo_fgvideoram_w, &senjyo_fgvideoram },
	{ 0x9400, 0x97ff, senjyo_fgcolorram_w, &senjyo_fgcolorram },
	{ 0x9800, 0x987f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9c00, 0x9d8f, paletteram_IIBBGGRR_w, &paletteram },
	{ 0x9e00, 0x9e1f, MWA_RAM, &senjyo_fgscroll },
	{ 0x9e20, 0x9e21, MWA_RAM, &senjyo_scrolly3 },
/*	{ 0x9e22, 0x9e23, height of the layer (Senjyo only, fixed at 0x380) */
	{ 0x9e25, 0x9e25, MWA_RAM, &senjyo_scrollx3 },
	{ 0x9e27, 0x9e27, senjyo_bgstripes_w, &senjyo_bgstripes },	/* controls width of background stripes */
	{ 0x9e28, 0x9e29, MWA_RAM, &senjyo_scrolly2 },
/*	{ 0x9e2a, 0x9e2b, height of the layer (Senjyo only, fixed at 0x200) */
	{ 0x9e2d, 0x9e2d, MWA_RAM, &senjyo_scrollx2 },
	{ 0x9e30, 0x9e31, MWA_RAM, &senjyo_scrolly1 },
/*	{ 0x9e32, 0x9e33, height of the layer (Senjyo only, fixed at 0x100) */
	{ 0x9e35, 0x9e35, MWA_RAM, &senjyo_scrollx1 },
/*	{ 0x9e38, 0x9e38, probably radar y position (Senjyo only, fixed at 0x61) */
/*	{ 0x9e3d, 0x9e3d, probably radar x position (Senjyo only, 0x00/0xc0 depending on screen flip) */
{ 0x9e00, 0x9e3f, MWA_RAM },
	{ 0xa000, 0xa37f, senjyo_bg3videoram_w, &senjyo_bg3videoram },
	{ 0xa800, 0xaaff, senjyo_bg2videoram_w, &senjyo_bg2videoram },
	{ 0xb000, 0xb1ff, senjyo_bg1videoram_w, &senjyo_bg1videoram },
	{ 0xb800, 0xbbff, MWA_RAM, &senjyo_radarram },
	{ 0xd000, 0xd000, senjyo_flipscreen_w },
	{ 0xd004, 0xd004, z80pioA_0_p_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ 0x8000, 0x8000, SN76496_0_w },
	{ 0x9000, 0x9000, SN76496_1_w },
	{ 0xa000, 0xa000, SN76496_2_w },
	{ 0xd000, 0xd000, senjyo_volume_w },
#if 0
	{ 0xe000, 0xe000, unknown },
	{ 0xf000, 0xf000, unknown },
#endif
	{ -1 }  /* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x00, 0x03, z80pio_0_r },
	{ 0x08, 0x0b, z80ctc_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x03, z80pio_0_w },
	{ 0x08, 0x0b, z80ctc_0_w },
	{ -1 }	/* end of table */
};




INPUT_PORTS_START( senjyo )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
/* coin input for both must be active between 2 and 9 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "100000" )
	PORT_DIPSETTING(    0x02, "None" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Planes" )
	PORT_DIPSETTING(    0x00, "Normal" )
	PORT_DIPSETTING(    0x80, "Always come at you" )
INPUT_PORTS_END

INPUT_PORTS_START( starforc )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
/* coin input for both must be active between 2 and 9 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 200000 500000" )
	PORT_DIPSETTING(    0x01, "100000 300000 800000" )
	PORT_DIPSETTING(    0x02, "50000 200000" )
	PORT_DIPSETTING(    0x03, "100000 300000" )
	PORT_DIPSETTING(    0x04, "50000" )
	PORT_DIPSETTING(    0x05, "100000" )
	PORT_DIPSETTING(    0x06, "200000" )
	PORT_DIPSETTING(    0x07, "None" )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easiest" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x18, "Difficult" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x28, "Hardest" )
	/* 0x30 and x038 are unused */
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( baluba )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
/* coin input for both must be active between 2 and 9 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000 100000 200000" )
	PORT_DIPSETTING(    0x01, "50000 200000 500000" )
	PORT_DIPSETTING(    0x02, "30000 100000" )
	PORT_DIPSETTING(    0x03, "50000 200000" )
	PORT_DIPSETTING(    0x04, "30000" )
	PORT_DIPSETTING(    0x05, "100000" )
	PORT_DIPSETTING(    0x06, "200000" )
	PORT_DIPSETTING(    0x07, "None" )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x18, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x28, "5" )
	PORT_DIPSETTING(    0x30, "6" )
	PORT_DIPSETTING(    0x38, "7" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	3,	/* 3 bits per pixel */
	{ 0, 512*8*8, 2*512*8*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout tilelayout_256 =
{
	16,16,	/* 16*16 characters */
	256,	/* 256 characters */
	3,	/* 3 bits per pixel */
	{ 0, 256*16*16, 2*256*16*16 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every character takes 32 consecutive bytes */
};
static struct GfxLayout tilelayout_128 =
{
	16,16,	/* 16*16 characters */
	128,	/* 128 characters */
	3,	/* 3 bits per pixel */
	{ 0, 128*16*16, 2*128*16*16 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every character takes 32 consecutive bytes */
};
static struct GfxLayout spritelayout1 =
{
	16,16,	/* 16*16 sprites */
	512,	/* 512 sprites */
	3,	/* 3 bits per pixel */
	{ 0, 512*16*16, 2*512*16*16 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};
static struct GfxLayout spritelayout2 =
{
	32,32,	/* 32*32 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 0, 128*32*32, 2*128*32*32 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
			40*8+0, 40*8+1, 40*8+2, 40*8+3, 40*8+4, 40*8+5, 40*8+6, 40*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			80*8, 81*8, 82*8, 83*8, 84*8, 85*8, 86*8, 87*8 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,       0, 8 },	/*   0- 63 characters */
	{ REGION_GFX2, 0, &tilelayout_256,  64, 8 },	/*  64-127 background #1 */
	{ REGION_GFX3, 0, &tilelayout_256, 128, 8 },	/* 128-191 background #2 */
	{ REGION_GFX4, 0, &tilelayout_128, 192, 8 },	/* 192-255 background #3 */
	{ REGION_GFX5, 0, &spritelayout1,  320, 8 },	/* 320-383 normal sprites */
	{ REGION_GFX5, 0, &spritelayout2,  320, 8 },	/* 320-383 large sprites */
													/* 384-399 is background */
	{ -1 } /* end of array */
};



static Z80_DaisyChain daisy_chain[] =
{
	{ z80pio_reset , z80pio_interrupt, z80pio_reti , 0 }, /* device 0 = PIO_0 , low  priority */
	{ z80ctc_reset , z80ctc_interrupt, z80ctc_reti , 0 }, /* device 1 = CTC_0 , high priority */
	{ 0,0,0,-1} 	   /* end mark */
};



static struct SN76496interface sn76496_interface =
{
	3,	/* 3 chips */
	{ 2000000, 2000000, 2000000 },	/* 2 Mhz? */
	{ 50, 50, 50 }
};

static struct CustomSound_interface custom_interface =
{
	senjyo_sh_start,
	senjyo_sh_stop,
	senjyo_sh_update
};



static struct MachineDriver machine_driver_senjyo =
{
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz? */
			readmem,writemem,0,0,
			senjyo_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2000000,	/* 2 Mhz? */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			0,0, /* interrupts are made by z80 daisy chain system */
			0,0,daisy_chain
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	senjyo_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	402, 402,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	senjyo_vh_start,
	senjyo_vh_stop,
	senjyo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		},
		{
			SOUND_CUSTOM,
			&custom_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( senjyo )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "08m_05t.bin", 0x0000, 0x2000, 0xb1f3544d )
	ROM_LOAD( "08k_04t.bin", 0x2000, 0x2000, 0xe34468a8 )
	ROM_LOAD( "08j_03t.bin", 0x4000, 0x2000, 0xc33aedee )
	ROM_LOAD( "08f_02t.bin", 0x6000, 0x2000, 0x0ef4db9e )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "02h_01t.bin", 0x0000, 0x2000, 0xc1c24455 )

	ROM_REGION( 0x03000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "08h_08b.bin", 0x00000, 0x1000, 0x0c875994 )	/* fg */
	ROM_LOAD( "08f_07b.bin", 0x01000, 0x1000, 0x497bea8e )
	ROM_LOAD( "08d_06b.bin", 0x02000, 0x1000, 0x4ef69b00 )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "05n_16m.bin", 0x00000, 0x1000, 0x0d3e00fb )	/* bg1 */
	ROM_LOAD( "05k_15m.bin", 0x02000, 0x1000, 0x93442213 )
	ROM_CONTINUE(			 0x04000, 0x1000             )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "07n_18m.bin", 0x00000, 0x1000, 0xd50fced3 )	/* bg2 */
	ROM_LOAD( "07k_17m.bin", 0x02000, 0x1000, 0x10c3a5f0 )
	ROM_CONTINUE(			 0x04000, 0x1000             )

	ROM_REGION( 0x03000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "09n_20m.bin", 0x00000, 0x1000, 0x54cb8126 )	/* bg3 */
	ROM_LOAD( "09k_19m.bin", 0x01000, 0x2000, 0x373e047c )

	ROM_REGION( 0x0c000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "08p_13b.bin", 0x00000, 0x2000, 0x40127efd )	/* sprites */
	ROM_LOAD( "08s_14b.bin", 0x02000, 0x2000, 0x42648ffa )
	ROM_LOAD( "08m_11b.bin", 0x04000, 0x2000, 0xccc4680b )
	ROM_LOAD( "08n_12b.bin", 0x06000, 0x2000, 0x742fafed )
	ROM_LOAD( "08j_09b.bin", 0x08000, 0x2000, 0x1ee63b5c )
	ROM_LOAD( "08k_10b.bin", 0x0a000, 0x2000, 0xa9f41ec9 )

	ROM_REGION( 0x0020, REGION_PROMS )	/* PROMs */
    ROM_LOAD( "07b.bin",    0x0000, 0x0020, 0x68db8300 )	/* unknown - timing? */
ROM_END

ROM_START( starforc )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "starforc.3",   0x0000, 0x4000, 0x8ba27691 )
	ROM_LOAD( "starforc.2",   0x4000, 0x4000, 0x0fc4d2d6 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "starforc.1",   0x0000, 0x2000, 0x2735bb22 )

	ROM_REGION( 0x03000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.7",   0x00000, 0x1000, 0xf4803339 )	/* fg */
	ROM_LOAD( "starforc.8",   0x01000, 0x1000, 0x96979684 )
	ROM_LOAD( "starforc.9",   0x02000, 0x1000, 0xeead1d5c )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.15",  0x00000, 0x2000, 0xc3bda12f )	/* bg1 */
	ROM_LOAD( "starforc.14",  0x02000, 0x2000, 0x9e9384fe )
	ROM_LOAD( "starforc.13",  0x04000, 0x2000, 0x84603285 )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.12",  0x00000, 0x2000, 0xfdd9e38b )	/* bg2 */
	ROM_LOAD( "starforc.11",  0x02000, 0x2000, 0x668aea14 )
	ROM_LOAD( "starforc.10",  0x04000, 0x2000, 0xc62a19c1 )

	ROM_REGION( 0x03000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.18",  0x00000, 0x1000, 0x6455c3ad )	/* bg3 */
	ROM_LOAD( "starforc.17",  0x01000, 0x1000, 0x68c60d0f )
	ROM_LOAD( "starforc.16",  0x02000, 0x1000, 0xce20b469 )

	ROM_REGION( 0x0c000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.6",   0x00000, 0x4000, 0x5468a21d )	/* sprites */
	ROM_LOAD( "starforc.5",   0x04000, 0x4000, 0xf71717f8 )
	ROM_LOAD( "starforc.4",   0x08000, 0x4000, 0xdd9d68a4 )

	ROM_REGION( 0x0020, REGION_PROMS )	/* PROMs */
    ROM_LOAD( "07b.bin",    0x0000, 0x0020, 0x68db8300 )	/* unknown - timing? */
ROM_END

ROM_START( starfore )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "starfore.005", 0x0000, 0x2000, 0x825f7ebe )
	ROM_LOAD( "starfore.004", 0x2000, 0x2000, 0xfbcecb65 )
	ROM_LOAD( "starfore.003", 0x4000, 0x2000, 0x9f8013b9 )
	ROM_LOAD( "starfore.002", 0x6000, 0x2000, 0xf8111eba )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "starfore.000", 0x0000, 0x2000, 0xa277c268 )

	ROM_REGION( 0x03000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.7",   0x00000, 0x1000, 0xf4803339 )	/* fg */
	ROM_LOAD( "starforc.8",   0x01000, 0x1000, 0x96979684 )
	ROM_LOAD( "starforc.9",   0x02000, 0x1000, 0xeead1d5c )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.15",  0x00000, 0x2000, 0xc3bda12f )	/* bg1 */
	ROM_LOAD( "starforc.14",  0x02000, 0x2000, 0x9e9384fe )
	ROM_LOAD( "starforc.13",  0x04000, 0x2000, 0x84603285 )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.12",  0x00000, 0x2000, 0xfdd9e38b )	/* bg2 */
	ROM_LOAD( "starforc.11",  0x02000, 0x2000, 0x668aea14 )
	ROM_LOAD( "starforc.10",  0x04000, 0x2000, 0xc62a19c1 )

	ROM_REGION( 0x03000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.18",  0x00000, 0x1000, 0x6455c3ad )	/* bg3 */
	ROM_LOAD( "starforc.17",  0x01000, 0x1000, 0x68c60d0f )
	ROM_LOAD( "starforc.16",  0x02000, 0x1000, 0xce20b469 )

	ROM_REGION( 0x0c000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.6",   0x00000, 0x4000, 0x5468a21d )	/* sprites */
	ROM_LOAD( "starforc.5",   0x04000, 0x4000, 0xf71717f8 )
	ROM_LOAD( "starforc.4",   0x08000, 0x4000, 0xdd9d68a4 )

	ROM_REGION( 0x0020, REGION_PROMS )	/* PROMs */
    ROM_LOAD( "07b.bin",    0x0000, 0x0020, 0x68db8300 )	/* unknown - timing? */
ROM_END

ROM_START( megaforc )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "mf3.bin",      0x0000, 0x4000, 0xd3ea82ec )
	ROM_LOAD( "mf2.bin",      0x4000, 0x4000, 0xaa320718 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "starforc.1",   0x0000, 0x2000, 0x2735bb22 )

	ROM_REGION( 0x03000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mf7.bin",      0x00000, 0x1000, 0x43ef8d20 )	/* fg */
	ROM_LOAD( "mf8.bin",      0x01000, 0x1000, 0xc36fb746 )
	ROM_LOAD( "mf9.bin",      0x02000, 0x1000, 0x62e7c9ec )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.15",  0x00000, 0x2000, 0xc3bda12f )	/* bg1 */
	ROM_LOAD( "starforc.14",  0x02000, 0x2000, 0x9e9384fe )
	ROM_LOAD( "starforc.13",  0x04000, 0x2000, 0x84603285 )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.12",  0x00000, 0x2000, 0xfdd9e38b )	/* bg2 */
	ROM_LOAD( "starforc.11",  0x02000, 0x2000, 0x668aea14 )
	ROM_LOAD( "starforc.10",  0x04000, 0x2000, 0xc62a19c1 )

	ROM_REGION( 0x03000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.18",  0x00000, 0x1000, 0x6455c3ad )	/* bg3 */
	ROM_LOAD( "starforc.17",  0x01000, 0x1000, 0x68c60d0f )
	ROM_LOAD( "starforc.16",  0x02000, 0x1000, 0xce20b469 )

	ROM_REGION( 0x0c000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "starforc.6",   0x00000, 0x4000, 0x5468a21d )	/* sprites */
	ROM_LOAD( "starforc.5",   0x04000, 0x4000, 0xf71717f8 )
	ROM_LOAD( "starforc.4",   0x08000, 0x4000, 0xdd9d68a4 )

	ROM_REGION( 0x0020, REGION_PROMS )	/* PROMs */
    ROM_LOAD( "07b.bin",    0x0000, 0x0020, 0x68db8300 )	/* unknown - timing? */
ROM_END

ROM_START( baluba )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "0",   		  0x0000, 0x4000, 0x0e2ebe32 )
	ROM_LOAD( "1",   		  0x4000, 0x4000, 0xcde97076 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "2",   		  0x0000, 0x2000, 0x441fbc64 )

	ROM_REGION( 0x03000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15",   		  0x00000, 0x1000, 0x3dda0d84 )	/* fg */
	ROM_LOAD( "16",   		  0x01000, 0x1000, 0x3ebc79d8 )
	ROM_LOAD( "17",   		  0x02000, 0x1000, 0xc4430deb )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9",  		  0x00000, 0x2000, 0x90f88c43 )	/* bg1 */
	ROM_LOAD( "10",  		  0x02000, 0x2000, 0xab117070 )
	ROM_LOAD( "11",  		  0x04000, 0x2000, 0xe13b44b0 )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "12", 		  0x00000, 0x2000, 0xa6541c8d )	/* bg2 */
	ROM_LOAD( "13", 		  0x02000, 0x2000, 0xafccdd18 )
	ROM_LOAD( "14", 		  0x04000, 0x2000, 0x69542e65 )

	ROM_REGION( 0x03000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "8",  		  0x00000, 0x1000, 0x31e97ef9 )	/* bg3 */
	ROM_LOAD( "7",  		  0x01000, 0x1000, 0x5915c5e2 )
	ROM_LOAD( "6",  		  0x02000, 0x1000, 0xad6881da )

	ROM_REGION( 0x0c000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5",  		  0x00000, 0x4000, 0x3b6b6e96 )	/* sprites */
	ROM_LOAD( "4",  		  0x04000, 0x4000, 0xdd954124 )
	ROM_LOAD( "3",  		  0x08000, 0x4000, 0x7ac24983 )

	ROM_REGION( 0x0020, REGION_PROMS )	/* PROMs */
    ROM_LOAD( "07b.bin",    0x0000, 0x0020, 0x68db8300 )	/* unknown - timing? */
ROM_END



GAME( 1983, senjyo,   0,        senjyo, senjyo,   senjyo,   ROT90, "Tehkan", "Senjyo" )
GAME( 1984, starforc, 0,        senjyo, starforc, starforc, ROT90, "Tehkan", "Star Force" )
GAME( 1984, starfore, starforc, senjyo, starforc, starfore, ROT90, "Tehkan", "Star Force (encrypted)" )
GAME( 1985, megaforc, starforc, senjyo, starforc, starforc, ROT90, "Tehkan (Video Ware license)", "Mega Force" )
GAMEX(1986, baluba,   0,        senjyo, baluba,   starforc, ROT90, "Able Corp, Ltd.", "Baluba-louk no Densetsu", GAME_IMPERFECT_COLORS )
