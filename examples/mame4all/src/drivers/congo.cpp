/***************************************************************************

Congo Bongo memory map (preliminary)

0000-1fff ROM 1
2000-3fff ROM 2
4000-5fff ROM 3
6000-7fff ROM 4

8000-87ff RAM 1
8800-8fff RAM 2
a000-a3ff Video RAM
a400-a7ff Color RAM

8400-8fff sprites

read:
c000      IN0
c001      IN1
c002      DSW1
c003      DSW2
c008      IN2

write:
c000-c002 ?
c006      ?
c018      coinAen, used to enable input on coin slot A
c019      coinBen, used to enable input on coin slot B
c01a      serven,  used to enable input on service sw
c01b      counterA,coin counter for slot A
c01c      counterB,coin counter for slot B
c01d      background enable
c01e      flip screen
c01f      interrupt enable

c028-c029 background playfield position
c031      sprite enable ?
c032      sprite start
c033      sprite count


interrupts:
VBlank triggers IRQ, handled with interrupt mode 1
NMI causes a ROM/RAM test.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern struct GfxLayout zaxxon_charlayout1;
extern struct GfxLayout zaxxon_charlayout2;

extern int zaxxon_vid_type;
extern unsigned char *zaxxon_background_position;
extern unsigned char *zaxxon_background_enable;
void zaxxon_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  zaxxon_vh_start(void);
void zaxxon_vh_stop(void);
void zaxxon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



void congo_init_machine(void)
{
	zaxxon_vid_type = 1;
}

WRITE_HANDLER( congo_daio_w )
{
	if (offset == 1)
	{
		if (data & 2) sample_start(0,0,0);
	}
	else if (offset == 2)
	{
		data ^= 0xff;

		if (data & 0x80)
		{
			if (data & 8) sample_start(1,1,0);
			if (data & 4) sample_start(2,2,0);
			if (data & 2) sample_start(3,3,0);
			if (data & 1) sample_start(4,4,0);
		}
	}
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0xa000, 0x83ff, MRA_RAM },
	{ 0xa000, 0xa7ff, MRA_RAM },
	{ 0xc000, 0xc000, input_port_0_r },
	{ 0xc001, 0xc001, input_port_1_r },
	{ 0xc008, 0xc008, input_port_2_r },
	{ 0xc002, 0xc002, input_port_3_r },
	{ 0xc003, 0xc003, input_port_4_r },
	{ 0x0000, 0x7fff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x8000, 0x83ff, MWA_RAM },
	{ 0xa000, 0xa3ff, videoram_w, &videoram, &videoram_size },
	{ 0xa400, 0xa7ff, colorram_w, &colorram },
	{ 0x8400, 0x8fff, MWA_RAM, &spriteram },
	{ 0xc01f, 0xc01f, interrupt_enable_w },
	{ 0xc028, 0xc029, MWA_RAM, &zaxxon_background_position },
	{ 0xc01d, 0xc01d, MWA_RAM, &zaxxon_background_enable },
	{ 0xc038, 0xc038, soundlatch_w },
	{ 0xc030, 0xc033, MWA_NOP }, /* ??? */
	{ 0xc01e, 0xc01e, MWA_NOP }, /* flip unused for now */
	{ 0xc018, 0xc018, MWA_NOP }, /* coinAen */
	{ 0xc019, 0xc019, MWA_NOP }, /* coinBen */
	{ 0xc01a, 0xc01a, MWA_NOP }, /* serven */
	{ 0xc01b, 0xc01c, coin_counter_w }, /* counterA, counterB */
	{ 0x0000, 0x7fff, MWA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sh_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x8000, 0x8003, soundlatch_r },
	{ -1 }
};
static struct MemoryWriteAddress sh_writemem[] =
{
	{ 0x6000, 0x6003, SN76496_0_w },
	{ 0xa000, 0xa003, SN76496_1_w },
	{ 0x4000, 0x47ff, MWA_RAM },
	{ 0x8000, 0x8003, congo_daio_w },
	{ 0x0000, 0x2000, MWA_ROM },
	{ -1 }
};



/***************************************************************************

  Congo Bongo uses NMI to trigger the self test. We use a fake input port
  to tie that event to a keypress.

***************************************************************************/
static int congo_interrupt(void)
{
	if (readinputport(5) & 1)	/* get status of the F2 key */
		return nmi_interrupt();	/* trigger self test */
	else return interrupt();
}

/* almost the same as Zaxxon; UP and DOWN are inverted, and the joystick is 4 way. */
INPUT_PORTS_START( congo )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	/* the coin inputs must stay active for exactly one frame, otherwise */
	/* the game will keep inserting coins. */
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	/* Coin Aux doesn't need IMPULSE to pass the test, but it still needs it */
	/* to avoid the freeze. */
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_SERVICE1, 1 )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x03, "10000" )
	PORT_DIPSETTING(    0x01, "20000" )
	PORT_DIPSETTING(    0x02, "30000" )
	PORT_DIPSETTING(    0x00, "40000" )
	PORT_DIPNAME( 0x0c, 0x0c, "Difficulty???" )
	PORT_DIPSETTING(    0x0c, "Easy?" )
	PORT_DIPSETTING(    0x04, "Medium?" )
	PORT_DIPSETTING(    0x08, "Hard?" )
	PORT_DIPSETTING(    0x00, "Hardest?" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, "2C/1C 5C/3C 6C/4C" )
	PORT_DIPSETTING(    0x0a, "2C/1C 3C/2C 4C/3C" )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, "1C/1C 5C/6C" )
	PORT_DIPSETTING(    0x0c, "1C/1C 4C/5C" )
	PORT_DIPSETTING(    0x04, "1C/1C 2C/3C" )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, "1C/2C 5C/11C" )
	PORT_DIPSETTING(    0x00, "1C/2C 4C/9C" )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xf0, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x60, "2C/1C 5C/3C 6C/4C" )
	PORT_DIPSETTING(    0xa0, "2C/1C 3C/2C 4C/3C" )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, "1C/1C 5C/6C" )
	PORT_DIPSETTING(    0xc0, "1C/1C 4C/5C" )
	PORT_DIPSETTING(    0x40, "1C/1C 2C/3C" )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, "1C/2C 5C/11C" )
	PORT_DIPSETTING(    0x00, "1C/2C 4C/9C" )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_6C ) )

	PORT_START	/* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
INPUT_PORTS_END

/* Same as Congo Bongo, except the Demo Sounds dip, that here turns the
   sound off in the whole game. */
INPUT_PORTS_START( tiptop )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused (the self test doesn't mention it) */
	/* the coin inputs must stay active for exactly one frame, otherwise */
	/* the game will keep inserting coins. */
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	/* Coin Aux doesn't need IMPULSE to pass the test, but it still needs it */
	/* to avoid the freeze. */
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_SERVICE1, 1 )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x03, "10000" )
	PORT_DIPSETTING(    0x01, "20000" )
	PORT_DIPSETTING(    0x02, "30000" )
	PORT_DIPSETTING(    0x00, "40000" )
	PORT_DIPNAME( 0x0c, 0x0c, "Difficulty???" )
	PORT_DIPSETTING(    0x0c, "Easy?" )
	PORT_DIPSETTING(    0x04, "Medium?" )
	PORT_DIPSETTING(    0x08, "Hard?" )
	PORT_DIPSETTING(    0x00, "Hardest?" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x40, 0x40, "Sound" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, "2C/1C 5C/3C 6C/4C" )
	PORT_DIPSETTING(    0x0a, "2C/1C 3C/2C 4C/3C" )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, "1C/1C 5C/6C" )
	PORT_DIPSETTING(    0x0c, "1C/1C 4C/5C" )
	PORT_DIPSETTING(    0x04, "1C/1C 2C/3C" )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, "1C/2C 5C/11C" )
	PORT_DIPSETTING(    0x00, "1C/2C 4C/9C" )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xf0, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x60, "2C/1C 5C/3C 6C/4C" )
	PORT_DIPSETTING(    0xa0, "2C/1C 3C/2C 4C/3C" )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, "1C/1C 5C/6C" )
	PORT_DIPSETTING(    0xc0, "1C/1C 4C/5C" )
	PORT_DIPSETTING(    0x40, "1C/1C 2C/3C" )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, "1C/2C 5C/11C" )
	PORT_DIPSETTING(    0x00, "1C/2C 4C/9C" )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_6C ) )

	PORT_START	/* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
INPUT_PORTS_END


static struct GfxLayout spritelayout =
{
	32,32,	/* 32*32 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 2*128*128*8, 128*128*8, 0 },    /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 24*8+4, 24*8+5, 24*8+6, 24*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			96*8, 97*8, 98*8, 99*8, 100*8, 101*8, 102*8, 103*8 },
	128*8	/* every sprite takes 128 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &zaxxon_charlayout1,  0, 32 },	/* characters */
	{ REGION_GFX2, 0, &zaxxon_charlayout2,  0, 32 },	/* background tiles */
	{ REGION_GFX3, 0, &spritelayout,        0, 32 },	/* sprites */
	{ -1 } /* end of array */
};



static struct SN76496interface sn76496_interface =
{
	2,	/* 2 chips */
	{ 4000000, 4000000 },	/* 4 Mhz??? */
	{ 100, 100 }
};

/* Samples for Congo Bongo, these are needed for now    */
/* as I haven't gotten around to calculate a formula for the */
/* simple oscillators producing the drums VL*/
/* As for now, thanks to Tim L. for providing samples */

static const char *congo_sample_names[] =
{
	"*congo",
	"gorilla.wav",
	"bass.wav",
	"congaa.wav",
	"congab.wav",
	"rim.wav",
	0
};

static struct Samplesinterface samples_interface =
{
	5,	/* 5 channels */
	25,	/* volume */
	congo_sample_names
};



static struct MachineDriver machine_driver_congo =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ?? */
			readmem,writemem,0,0,
			congo_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2000000,
			sh_readmem, sh_writemem, 0,0,
			interrupt, 4
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	congo_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1,2*8, 30*8-1 },
	gfxdecodeinfo,
	256,32*8,
	zaxxon_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	zaxxon_vh_start,
	zaxxon_vh_stop,
	zaxxon_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
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

ROM_START( congo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "congo1.bin",   0x0000, 0x2000, 0x09355b5b )
	ROM_LOAD( "congo2.bin",   0x2000, 0x2000, 0x1c5e30ae )
	ROM_LOAD( "congo3.bin",   0x4000, 0x2000, 0x5ee1132c )
	ROM_LOAD( "congo4.bin",   0x6000, 0x2000, 0x5332b9bf )

	ROM_REGION( 0x10000, REGION_CPU2 ) /*64K for the sound cpu*/
	ROM_LOAD( "congo17.bin",  0x0000, 0x2000, 0x5024e673 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "congo5.bin",   0x00000, 0x1000, 0x7bf6ba2b )	/* characters */
	/* 1000-1800 empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "congo8.bin",   0x00000, 0x2000, 0xdb99a619 )	/* background tiles */
	ROM_LOAD( "congo9.bin",   0x02000, 0x2000, 0x93e2309e )
	ROM_LOAD( "congo10.bin",  0x04000, 0x2000, 0xf27a9407 )

	ROM_REGION( 0xc000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "congo12.bin",  0x00000, 0x2000, 0x15e3377a )	/* sprites */
	ROM_LOAD( "congo13.bin",  0x02000, 0x2000, 0x1d1321c8 )
	ROM_LOAD( "congo11.bin",  0x04000, 0x2000, 0x73e2709f )
	ROM_LOAD( "congo14.bin",  0x06000, 0x2000, 0xbf9169fe )
	ROM_LOAD( "congo16.bin",  0x08000, 0x2000, 0xcb6d5775 )
	ROM_LOAD( "congo15.bin",  0x0a000, 0x2000, 0x7b15a7a4 )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "congo6.bin",   0x0000, 0x2000, 0xd637f02b )
	/* 2000-3fff empty space to match Zaxxon */
	ROM_LOAD( "congo7.bin",   0x4000, 0x2000, 0x80927943 )
	/* 6000-7fff empty space to match Zaxxon */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "congo.u68",    0x0000, 0x100, 0xb788d8ae ) /* palette */
ROM_END

ROM_START( tiptop )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "tiptop1.bin",  0x0000, 0x2000, 0xe19dc77b )
	ROM_LOAD( "tiptop2.bin",  0x2000, 0x2000, 0x3fcd3b6e )
	ROM_LOAD( "tiptop3.bin",  0x4000, 0x2000, 0x1c94250b )
	ROM_LOAD( "tiptop4.bin",  0x6000, 0x2000, 0x577b501b )

	ROM_REGION( 0x10000, REGION_CPU2 ) /*64K for the sound cpu*/
	ROM_LOAD( "congo17.bin",  0x0000, 0x2000, 0x5024e673 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "congo5.bin",   0x00000, 0x1000, 0x7bf6ba2b )	/* characters */
	/* 1000-1800 empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "congo8.bin",   0x00000, 0x2000, 0xdb99a619 )	/* background tiles */
	ROM_LOAD( "congo9.bin",   0x02000, 0x2000, 0x93e2309e )
	ROM_LOAD( "congo10.bin",  0x04000, 0x2000, 0xf27a9407 )

	ROM_REGION( 0xc000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "congo12.bin",  0x00000, 0x2000, 0x15e3377a )	/* sprites */
	ROM_LOAD( "congo13.bin",  0x02000, 0x2000, 0x1d1321c8 )
	ROM_LOAD( "congo11.bin",  0x04000, 0x2000, 0x73e2709f )
	ROM_LOAD( "congo14.bin",  0x06000, 0x2000, 0xbf9169fe )
	ROM_LOAD( "congo16.bin",  0x08000, 0x2000, 0xcb6d5775 )
	ROM_LOAD( "congo15.bin",  0x0a000, 0x2000, 0x7b15a7a4 )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "congo6.bin",   0x0000, 0x2000, 0xd637f02b )
	/* 2000-3fff empty space to match Zaxxon */
	ROM_LOAD( "congo7.bin",   0x4000, 0x2000, 0x80927943 )
	/* 6000-7fff empty space to match Zaxxon */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "congo.u68",    0x0000, 0x100, 0xb788d8ae ) /* palette */
ROM_END



GAME( 1983, congo,  0,     congo, congo,  0, ROT90, "Sega", "Congo Bongo" )
GAME( 1983, tiptop, congo, congo, tiptop, 0, ROT90, "Sega", "Tip Top" )
