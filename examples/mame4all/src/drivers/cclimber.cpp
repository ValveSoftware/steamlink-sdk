#include "../vidhrdw/cclimber.cpp"
#include "../sndhrdw/cclimber.cpp"

/***************************************************************************

Crazy Climber memory map (preliminary)
as described by Lionel Theunissen (lionelth@ozemail.com.au)

Crazy Kong is very similar to Crazy Climber, there is an additional ROM at
5000-5fff and RAM is at 6000-6bff. Dip switches and input connections are
different as well.

Swimmer is similar but also different (e.g. it has two CPUs and two 8910,
graphics are 3bpp instead of 2)

0000h-4fffh ;20k program ROMs. ROM11=0000h
                               ROM10=1000h
                               ROM09=2000h
                               ROM08=3000h
                               ROM07=4000h

8000h-83ffh ;1k scratchpad RAM.
8800h-88ffh ;256 bytes Bigsprite RAM.
9000h-93ffh ;1k screen RAM.
9800h-981fh ;Column smooth scroll position. Corresponds to each char
             column.

9880h-989fh ;Sprite controls. 8 groups of 4 bytes:
  1st byte; code/attribute.
            Bits 0-5: sprite code.
            Bit    6: x invert.
            Bit    7: y invert.
  2nd byte ;color.
            Bits 0-3: colour. (palette scheme 0-15)
            Bit    4: 0=charset1, 1 =charset 2.
  3rd byte ;y position
  4th byte ;x position

98dc        bit 0  big sprite priority over sprites? (1 = less priority)
98ddh ;Bigsprite colour/attribute.
            Bit 0-2: Big sprite colour.
            bit 3  ??
            Bit   4: x invert.
            Bit   5: y invert.
98deh ;Bigsprite y position.
98dfh ;Bigsprite x position.

9c00h-9fffh ;1/2k colour RAM: Bits 0-3: colour. (palette scheme 0-15)
                              Bit    4: 0=charset1, 1=charset2.
                              Bit    5: (not used by CC)
                              Bit    6: x invert.
                              Bit    7: y invert. (not used by CC)

a000h ;RD: Player 1 controls.
            Bit 0: Left up
                1: Left down
                2: Left left
                3: Left right
                4: Right up
                5: Right down
                6: Right left
                7: Right right

a000h ;WR: Non Maskable interrupt.
            Bit 0: 0=NMI disable, 1=NMI enable.

a001h ;WR: Horizontal video direction (Crazy Kong sets it to 1).
            Bit 0: 0=Normal, 1=invert.

a002h ;WR: Vertical video direction (Crazy Kong sets it to 1).
            Bit 0: 0=Normal, 1=invert.

a004h ;WR: Sample trigger.
            Bit 0: 0=Trigger.

a800h ;RD: Player 2 controls (table model only).
            Bit 0: Left up
                1: Left down
                2: Left left
                3: Left right
                4: Right up
                5: Right down
                6: Right left
                7: Right right


a800h ;WR: Sample rate speed.
              Full byte value (0-255).

b000h ;RD: DIP switches.
            Bit 1,0: Number of climbers.
                     00=3, 01=4, 10=5, 11=6.
            Bit   2: Extra climber bonus.
                     0=30000, 1=50000.
            Bit   3: 1=Test Pattern
            Bit 5,4: Coins per credit.
                     00=1, 01=2, 10=3 11=4.
            Bit 7,6: Plays per credit.
                     00=1, 01=2, 10=3, 11=Freeplay.

b000h ;WR: Sample volume.
            Bits 0-5: Volume (0-31).

b800h ;RD: Machine switches.
            Bit 0: Coin 1.
            Bit 1: Coin 2.
            Bit 2: 1 Player start.
            Bit 3: 2 Player start.
            Bit 4: Upright/table select.
                   0=table, 1=upright.


I/O 8  ;AY-3-8910 Control Reg.
I/O 9  ;AY-3-8910 Data Write Reg.
I/O C  ;AY-3-8910 Data Read Reg.
        Port A of the 8910 selects the digital sample to play

Changes:
25 Jan 98 LBO
        * Added support for the real Swimmer bigsprite ROMs, courtesy of Gary Walton.
        * Increased the IRQs for the Swimmer audio CPU to 4 to make it more "jaunty".
          Not sure if this is accurate, but it should be closer.
3 Mar 98 LBO
        * Added alternate version of Swimmer.

TODO:
        * Verify timings of sound/music on Swimmer.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *cclimber_bsvideoram;
extern size_t cclimber_bsvideoram_size;
extern unsigned char *cclimber_bigspriteram;
extern unsigned char *cclimber_column_scroll;
WRITE_HANDLER( cclimber_flipscreen_w );
WRITE_HANDLER( cclimber_colorram_w );
WRITE_HANDLER( cclimber_bigsprite_videoram_w );
void cclimber_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int cclimber_vh_start(void);
void cclimber_vh_stop(void);
void cclimber_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( cclimber_sample_select_w );
WRITE_HANDLER( cclimber_sample_trigger_w );
WRITE_HANDLER( cclimber_sample_rate_w );
WRITE_HANDLER( cclimber_sample_volume_w );
int cclimber_sh_start(const struct MachineSound *msound);
void cclimber_sh_stop(void);



static void cclimber_init_machine (void)
{
	/* Disable interrupts, River Patrol / Silver Land needs this */
	interrupt_enable_w(0, 0);
}



/* Note that River Patrol reads/writes to a000-a4f0. This is a bug in the code.
   The instruction at 0x0593 should say LD DE,$8000 */

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6bff, MRA_RAM },	/* Crazy Kong only */
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0x8800, 0x8bff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },	/* video RAM */
	{ 0x9800, 0x9bff, MRA_RAM },	/* column scroll registers */
	{ 0x9c00, 0x9fff, MRA_RAM },	/* color RAM */
	{ 0xa000, 0xa000, input_port_0_r },     /* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },     /* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },     /* DSW */
	{ 0xb800, 0xb800, input_port_3_r },     /* IN2 */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x6bff, MWA_RAM },    /* Crazy Kong only */
	{ 0x8000, 0x83ff, MWA_RAM },
	{ 0x8800, 0x88ff, cclimber_bigsprite_videoram_w, &cclimber_bsvideoram, &cclimber_bsvideoram_size },
	{ 0x8900, 0x8bff, MWA_RAM },  /* not used, but initialized */
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0x9400, 0x97ff, videoram_w }, /* mirror address, used by Crazy Climber to draw windows */
	/* 9800-9bff and 9c00-9fff share the same RAM, interleaved */
	/* (9800-981f for scroll, 9c20-9c3f for color RAM, and so on) */
	{ 0x9800, 0x981f, MWA_RAM, &cclimber_column_scroll },
	{ 0x9880, 0x989f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x98dc, 0x98df, MWA_RAM, &cclimber_bigspriteram },
	{ 0x9800, 0x9bff, MWA_RAM },  /* not used, but initialized */
	{ 0x9c00, 0x9fff, cclimber_colorram_w, &colorram },
	{ 0xa000, 0xa000, interrupt_enable_w },
	{ 0xa001, 0xa002, cclimber_flipscreen_w },
	{ 0xa004, 0xa004, cclimber_sample_trigger_w },
	{ 0xa800, 0xa800, cclimber_sample_rate_w },
	{ 0xb000, 0xb000, cclimber_sample_volume_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x0c, 0x0c, AY8910_read_port_0_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x08, 0x08, AY8910_control_port_0_w },
	{ 0x09, 0x09, AY8910_write_port_0_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( cclimber )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP     | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* DSW */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x04, "50000" )
	PORT_BITX(    0x08, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Rack Test", KEYCODE_F1, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x0020, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Free_Play ) )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

/* several differences with cclimber: note that IN2 bits are ACTIVE_LOW, while in */
/* cclimber they are ACTIVE_HIGH. */
INPUT_PORTS_START( ckong )
	PORT_START      /* IN0 */
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )

	PORT_START      /* DSW */
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
	PORT_DIPSETTING(    0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( rpatrolb )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x3e, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x3e, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )

	PORT_START      /* DSW */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPSETTING(    0x0c, "6" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0020, 0x00, "Unknown 1" )  /* Probably unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Unknown 2" )  /* Probably unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Memory Test" )
	PORT_DIPSETTING(    0x00, "Retry on Error" )
	PORT_DIPSETTING(    0x80, "Stop on Error" )

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	512,    /* 512 characters (256 in Crazy Climber) */
	2,      /* 2 bits per pixel */
	{ 0, 512*8*8 }, /* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};
static struct GfxLayout bscharlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	2,      /* 2 bits per pixel */
	{ 0, 256*8*8 }, /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	128,    /* 128 sprites (64 in Crazy Climber) */
	2,      /* 2 bits per pixel */
	{ 0, 128*16*16 },       /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,       /* pretty straightforward layout */
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,      0, 16 }, /* char set #1 */
	{ REGION_GFX1, 0x2000, &charlayout,      0, 16 }, /* char set #2 */
	{ REGION_GFX2, 0x0000, &bscharlayout, 16*4,  8 }, /* big sprite char set */
	{ REGION_GFX1, 0x0000, &spritelayout,    0, 16 }, /* sprite set #1 */
	{ REGION_GFX1, 0x2000, &spritelayout,    0, 16 }, /* sprite set #2 */
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,      /* 1 chip */
	1536000,	/* 1.536 MHz */
	{ 50 },
	{ 0 },
	{ 0 },
	{ cclimber_sample_select_w },
	{ 0 }
};

static struct CustomSound_interface custom_interface =
{
	cclimber_sh_start,
	cclimber_sh_stop,
	0
};



static struct MachineDriver machine_driver_cclimber =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 MHz */
			readmem,writemem,readport,writeport,
			nmi_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	cclimber_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	96,16*4+8*4,
	cclimber_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	cclimber_vh_start,
	cclimber_vh_stop,
	cclimber_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
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

ROM_START( cclimber )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "cc11",         0x0000, 0x1000, 0x217ec4ff )
	ROM_LOAD( "cc10",         0x1000, 0x1000, 0xb3c26cef )
	ROM_LOAD( "cc09",         0x2000, 0x1000, 0x6db0879c )
	ROM_LOAD( "cc08",         0x3000, 0x1000, 0xf48c5fe3 )
	ROM_LOAD( "cc07",         0x4000, 0x1000, 0x3e873baf )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc06",         0x0000, 0x0800, 0x481b64cc )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc04",         0x1000, 0x0800, 0x332347cb )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc05",         0x2000, 0x0800, 0x2c33b760 )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc03",         0x3000, 0x0800, 0x4e4b3658 )
	/* empty hole - Crazy Kong has an additional ROM here */

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc02",         0x0000, 0x0800, 0x14f3ecc9 )
	ROM_LOAD( "cc01",         0x0800, 0x0800, 0x21c0f9fb )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cclimber.pr1", 0x0000, 0x0020, 0x751c3325 )
	ROM_LOAD( "cclimber.pr2", 0x0020, 0x0020, 0xab1940fa )
	ROM_LOAD( "cclimber.pr3", 0x0040, 0x0020, 0x71317756 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13",         0x0000, 0x1000, 0xe0042f75 )
	ROM_LOAD( "cc12",         0x1000, 0x1000, 0x5da13aaa )
ROM_END

static void init_cclimber(void)
{
/*
	translation mask is layed out like this:

	  0 1 2 3 4 5 6 7 8 9 a b c d e f
	0 <------A------> <------A------>
	1 <------B------> <------B------>
	2 <------A------> <------A------>
	3 <------B------> <------B------>
	4 <------C------> <------C------>
	5 <------D------> <------D------>
	6 <------C------> <------C------>
	7 <------D------> <------D------>
	8 <------E------> <------E------>
	9 <------F------> <------F------>
	a <------E------> <------E------>
	b <------F------> <------F------>
	c <------G------> <------G------>
	d <------H------> <------H------>
	e <------G------> <------G------>
	f <------H------> <------H------>

	Where <------A------> etc. are groups of 8 unrelated values.

	therefore in the following table we only keep track of <--A-->, <--B--> etc.
*/
	static const unsigned char xortable[2][64] =
	{
		/* -1 marks spots which are unused and therefore unknown */
		{
			0x44,0x15,0x45,0x11,0x50,0x15,0x15,0x41,
			0x01,0x50,0x15,0x41,0x11,0x45,0x45,0x11,
			0x11,0x41,0x01,0x55,0x04,0x10,0x51,0x05,
			0x15,0x55,0x51,0x05,0x55,0x40,0x01,0x55,
			0x54,0x50,0x51,0x05,0x11,0x40,0x14,  -1,
			0x54,0x10,0x40,0x51,0x05,0x54,0x14,  -1,
			0x44,0x14,0x01,0x40,0x14,  -1,0x41,0x50,
			0x50,0x41,0x41,0x45,0x14,  -1,0x10,0x01
		},
		{
			0x44,0x11,0x04,0x50,0x11,0x50,0x41,0x05,
			0x10,0x50,0x54,0x01,0x54,0x44,  -1,0x40,
			0x54,0x04,0x51,0x15,0x55,0x15,0x14,0x05,
			0x51,0x05,0x55,  -1,0x50,0x50,0x40,0x54,
			  -1,0x55,  -1,  -1,0x10,0x55,0x50,0x04,
			0x41,0x10,0x05,0x51,  -1,0x55,0x51,0x54,
			0x01,0x51,0x11,0x45,0x44,0x10,0x14,0x40,
			0x55,0x15,0x41,0x15,0x45,0x10,0x44,0x41
		}
	};
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0x0000;A < 0x10000;A++)
	{
		int i,j;
		unsigned char src;


		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 012467 of the source data */
		j = (src & 0x07) + ((src & 0x10) >> 1) + ((src & 0xc0) >> 2);

		/* decode the opcodes */
		rom[A + diff] = src ^ xortable[i][j];
	}
}

ROM_START( cclimbrj )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "cc11j.bin",    0x0000, 0x1000, 0x89783959 )
	ROM_LOAD( "cc10j.bin",    0x1000, 0x1000, 0x14eda506 )
	ROM_LOAD( "cc09j.bin",    0x2000, 0x1000, 0x26489069 )
	ROM_LOAD( "cc08j.bin",    0x3000, 0x1000, 0xb33c96f8 )
	ROM_LOAD( "cc07j.bin",    0x4000, 0x1000, 0xfbc9626c )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc06",         0x0000, 0x0800, 0x481b64cc )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc04",         0x1000, 0x0800, 0x332347cb )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc05",         0x2000, 0x0800, 0x2c33b760 )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc03",         0x3000, 0x0800, 0x4e4b3658 )
	/* empty hole - Crazy Kong has an additional ROM here */

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc02",         0x0000, 0x0800, 0x14f3ecc9 )
	ROM_LOAD( "cc01",         0x0800, 0x0800, 0x21c0f9fb )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cclimber.pr1", 0x0000, 0x0020, 0x751c3325 )
	ROM_LOAD( "cclimber.pr2", 0x0020, 0x0020, 0xab1940fa )
	ROM_LOAD( "cclimber.pr3", 0x0040, 0x0020, 0x71317756 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

ROM_START( ccboot )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "m11.bin",      0x0000, 0x1000, 0x5efbe180 )
	ROM_LOAD( "m10.bin",      0x1000, 0x1000, 0xbe2748c7 )
	ROM_LOAD( "cc09j.bin",    0x2000, 0x1000, 0x26489069 )
	ROM_LOAD( "m08.bin",      0x3000, 0x1000, 0xe3c542d6 )
	ROM_LOAD( "cc07j.bin",    0x4000, 0x1000, 0xfbc9626c )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc06",         0x0000, 0x0800, 0x481b64cc )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "m04.bin",      0x1000, 0x0800, 0x6fb80538 )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "m05.bin",      0x2000, 0x0800, 0x056af36b )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "m03.bin",      0x3000, 0x0800, 0x67127253 )
	/* empty hole - Crazy Kong has an additional ROM here */

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "m02.bin",      0x0000, 0x0800, 0x7f4877de )
	ROM_LOAD( "m01.bin",      0x0800, 0x0800, 0x49fab908 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cclimber.pr1", 0x0000, 0x0020, 0x751c3325 )
	ROM_LOAD( "cclimber.pr2", 0x0020, 0x0020, 0xab1940fa )
	ROM_LOAD( "cclimber.pr3", 0x0040, 0x0020, 0x71317756 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

ROM_START( ccboot2 )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "11.4k",        0x0000, 0x1000, 0xb2b17e24 )
	ROM_LOAD( "10.4j",        0x1000, 0x1000, 0x8382bc0f )
	ROM_LOAD( "cc09j.bin",    0x2000, 0x1000, 0x26489069 )
	ROM_LOAD( "m08.bin",      0x3000, 0x1000, 0xe3c542d6 )
	ROM_LOAD( "cc07j.bin",    0x4000, 0x1000, 0xfbc9626c )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc06",         0x0000, 0x0800, 0x481b64cc )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc04",         0x1000, 0x0800, 0x332347cb )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc05",         0x2000, 0x0800, 0x2c33b760 )
	/* empty hole - Crazy Kong has an additional ROM here */
	ROM_LOAD( "cc03",         0x3000, 0x0800, 0x4e4b3658 )
	/* empty hole - Crazy Kong has an additional ROM here */

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc02",         0x0000, 0x0800, 0x14f3ecc9 )
	ROM_LOAD( "cc01",         0x0800, 0x0800, 0x21c0f9fb )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cclimber.pr1", 0x0000, 0x0020, 0x751c3325 )
	ROM_LOAD( "cclimber.pr2", 0x0020, 0x0020, 0xab1940fa )
	ROM_LOAD( "cclimber.pr3", 0x0040, 0x0020, 0x71317756 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

static void init_cclimbrj(void)
{
/*
	translation mask is layed out like this:

	  0 1 2 3 4 5 6 7 8 9 a b c d e f
	0 <------A------> <------A------>
	1 <------B------> <------B------>
	2 <------A------> <------A------>
	3 <------B------> <------B------>
	4 <------C------> <------C------>
	5 <------D------> <------D------>
	6 <------C------> <------C------>
	7 <------D------> <------D------>
	8 <------E------> <------E------>
	9 <------F------> <------F------>
	a <------E------> <------E------>
	b <------F------> <------F------>
	c <------G------> <------G------>
	d <------H------> <------H------>
	e <------G------> <------G------>
	f <------H------> <------H------>

	Where <------A------> etc. are groups of 8 unrelated values.

	therefore in the following table we only keep track of <--A-->, <--B--> etc.
*/
	static const unsigned char xortable[2][64] =
	{
		{
			0x41,0x55,0x44,0x10,0x55,0x11,0x04,0x55,
			0x15,0x01,0x51,0x45,0x15,0x40,0x10,0x01,
			0x04,0x50,0x55,0x01,0x44,0x15,0x15,0x10,
			0x45,0x11,0x55,0x41,0x50,0x10,0x55,0x10,
			0x14,0x40,0x05,0x54,0x05,0x41,0x04,0x55,
			0x14,0x41,0x01,0x51,0x45,0x50,0x40,0x01,
			0x51,0x01,0x05,0x10,0x10,0x50,0x54,0x41,
			0x40,0x51,0x14,0x50,0x01,0x50,0x15,0x40
		},
		{
			0x50,0x10,0x10,0x51,0x44,0x50,0x50,0x50,
			0x41,0x05,0x11,0x55,0x51,0x11,0x54,0x11,
			0x14,0x54,0x54,0x50,0x54,0x40,0x44,0x04,
			0x14,0x50,0x15,0x44,0x54,0x14,0x05,0x50,
			0x01,0x04,0x55,0x51,0x45,0x40,0x11,0x15,
			0x44,0x41,0x11,0x15,0x41,0x05,0x55,0x51,
			0x51,0x54,0x05,0x01,0x15,0x51,0x41,0x45,
			0x14,0x11,0x41,0x45,0x50,0x55,0x05,0x01
		}
	};
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0x0000;A < 0x10000;A++)
	{
		int i,j;
		unsigned char src;


		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 012467 of the source data */
		j = (src & 0x07) + ((src & 0x10) >> 1) + ((src & 0xc0) >> 2);

		/* decode the opcodes */
		rom[A + diff] = src ^ xortable[i][j];
	}
}

ROM_START( ckong )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "d05-07.bin",   0x0000, 0x1000, 0xb27df032 )
	ROM_LOAD( "f05-08.bin",   0x1000, 0x1000, 0x5dc1aaba )
	ROM_LOAD( "h05-09.bin",   0x2000, 0x1000, 0xc9054c94 )
	ROM_LOAD( "k05-10.bin",   0x3000, 0x1000, 0x069c4797 )
	ROM_LOAD( "l05-11.bin",   0x4000, 0x1000, 0xae159192 )
	ROM_LOAD( "n05-12.bin",   0x5000, 0x1000, 0x966bc9ab )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n11-06.bin",   0x0000, 0x1000, 0x2dcedd12 )
	ROM_LOAD( "k11-04.bin",   0x1000, 0x1000, 0x3375b3bd )
	ROM_LOAD( "l11-05.bin",   0x2000, 0x1000, 0xfa7cbd91 )
	ROM_LOAD( "h11-03.bin",   0x3000, 0x1000, 0x5655cc11 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c11-02.bin",   0x0000, 0x0800, 0xd1352c31 )
	ROM_LOAD( "a11-01.bin",   0x0800, 0x0800, 0xa7a2fdbd )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "prom.v6",      0x0000, 0x0020, 0xb3fc1505 )
	ROM_LOAD( "prom.u6",      0x0020, 0x0020, 0x26aada9e )
	ROM_LOAD( "prom.t6",      0x0040, 0x0020, 0x676b3166 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

ROM_START( ckonga )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "d05-07.bin",   0x0000, 0x1000, 0xb27df032 )
	ROM_LOAD( "f05-08.bin",   0x1000, 0x1000, 0x5dc1aaba )
	ROM_LOAD( "h05-09.bin",   0x2000, 0x1000, 0xc9054c94 )
	ROM_LOAD( "10.dat",       0x3000, 0x1000, 0xc3beb501 )
	ROM_LOAD( "l05-11.bin",   0x4000, 0x1000, 0xae159192 )
	ROM_LOAD( "n05-12.bin",   0x5000, 0x1000, 0x966bc9ab )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n11-06.bin",   0x0000, 0x1000, 0x2dcedd12 )
	ROM_LOAD( "k11-04.bin",   0x1000, 0x1000, 0x3375b3bd )
	ROM_LOAD( "l11-05.bin",   0x2000, 0x1000, 0xfa7cbd91 )
	ROM_LOAD( "h11-03.bin",   0x3000, 0x1000, 0x5655cc11 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c11-02.bin",   0x0000, 0x0800, 0xd1352c31 )
	ROM_LOAD( "a11-01.bin",   0x0800, 0x0800, 0xa7a2fdbd )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "prom.v6",      0x0000, 0x0020, 0xb3fc1505 )
	ROM_LOAD( "prom.u6",      0x0020, 0x0020, 0x26aada9e )
	ROM_LOAD( "prom.t6",      0x0040, 0x0020, 0x676b3166 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

ROM_START( ckongjeu )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "d05-07.bin",   0x0000, 0x1000, 0xb27df032 )
	ROM_LOAD( "f05-08.bin",   0x1000, 0x1000, 0x5dc1aaba )
	ROM_LOAD( "h05-09.bin",   0x2000, 0x1000, 0xc9054c94 )
	ROM_LOAD( "ckjeu10.dat",  0x3000, 0x1000, 0x7e6eeec4 )
	ROM_LOAD( "l05-11.bin",   0x4000, 0x1000, 0xae159192 )
	ROM_LOAD( "ckjeu12.dat",  0x5000, 0x1000, 0x0532f270 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n11-06.bin",   0x0000, 0x1000, 0x2dcedd12 )
	ROM_LOAD( "k11-04.bin",   0x1000, 0x1000, 0x3375b3bd )
	ROM_LOAD( "l11-05.bin",   0x2000, 0x1000, 0xfa7cbd91 )
	ROM_LOAD( "h11-03.bin",   0x3000, 0x1000, 0x5655cc11 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c11-02.bin",   0x0000, 0x0800, 0xd1352c31 )
	ROM_LOAD( "a11-01.bin",   0x0800, 0x0800, 0xa7a2fdbd )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "prom.v6",      0x0000, 0x0020, 0xb3fc1505 )
	ROM_LOAD( "prom.u6",      0x0020, 0x0020, 0x26aada9e )
	ROM_LOAD( "prom.t6",      0x0040, 0x0020, 0x676b3166 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

ROM_START( ckongo )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "o55a-1",       0x0000, 0x1000, 0x8bfb4623 )
	ROM_LOAD( "o55a-2",       0x1000, 0x1000, 0x9ae8089b )
	ROM_LOAD( "o55a-3",       0x2000, 0x1000, 0xe82b33c8 )
	ROM_LOAD( "o55a-4",       0x3000, 0x1000, 0xf038f941 )
	ROM_LOAD( "o55a-5",       0x4000, 0x1000, 0x5182db06 )
	/* no ROM at 5000 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	/* same as ckong but with halves switched */
	ROM_LOAD( "o50b-1",       0x0000, 0x0800, 0xcae9e2bf )
	ROM_CONTINUE(             0x2000, 0x0800 )
	ROM_LOAD( "o50b-2",       0x0800, 0x0800, 0xfba82114 )
	ROM_CONTINUE(             0x2800, 0x0800 )
	ROM_LOAD( "o50b-3",       0x1000, 0x0800, 0x1714764b )
	ROM_CONTINUE(             0x3000, 0x0800 )
	ROM_LOAD( "o50b-4",       0x1800, 0x0800, 0xb7008b57 )
	ROM_CONTINUE(             0x3800, 0x0800 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c11-02.bin",   0x0000, 0x0800, 0xd1352c31 )
	ROM_LOAD( "a11-01.bin",   0x0800, 0x0800, 0xa7a2fdbd )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "prom.v6",      0x0000, 0x0020, 0xb3fc1505 )
	ROM_LOAD( "prom.u6",      0x0020, 0x0020, 0x26aada9e )
	ROM_LOAD( "prom.t6",      0x0040, 0x0020, 0x676b3166 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "cc12j.bin",    0x1000, 0x1000, 0x9003ffbd )
ROM_END

ROM_START( ckongalc )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ck7.bin",      0x0000, 0x1000, 0x2171cac3 )
	ROM_LOAD( "ck8.bin",      0x1000, 0x1000, 0x88b83ff7 )
	ROM_LOAD( "ck9.bin",      0x2000, 0x1000, 0xcff2af47 )
	ROM_LOAD( "ck10.bin",     0x3000, 0x1000, 0x520fa4de )
	ROM_LOAD( "ck11.bin",     0x4000, 0x1000, 0x327dcadf )
	/* no ROM at 5000 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ck6.bin",      0x0000, 0x1000, 0xa8916dc8 )
	ROM_LOAD( "ck4.bin",      0x1000, 0x1000, 0xb62a0367 )
	ROM_LOAD( "ck5.bin",      0x2000, 0x1000, 0xcd3b5dde )
	ROM_LOAD( "ck3.bin",      0x3000, 0x1000, 0x61122c5e )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ck2.bin",      0x0000, 0x0800, 0xf67c80f1 )
	ROM_LOAD( "ck1.bin",      0x0800, 0x0800, 0x80eb517d )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cclimber.pr1", 0x0000, 0x0020, 0x751c3325 )
	ROM_LOAD( "cclimber.pr2", 0x0020, 0x0020, 0xab1940fa )
	ROM_LOAD( "ck6t.bin",     0x0040, 0x0020, 0xb4e827a5 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "ck12.bin",     0x1000, 0x1000, 0x2eb23b60 )
ROM_END

ROM_START( monkeyd )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ck7.bin",      0x0000, 0x1000, 0x2171cac3 )
	ROM_LOAD( "ck8.bin",      0x1000, 0x1000, 0x88b83ff7 )
	ROM_LOAD( "ck9.bin",      0x2000, 0x1000, 0xcff2af47 )
	ROM_LOAD( "ck10.bin",     0x3000, 0x1000, 0x520fa4de )
	ROM_LOAD( "md5l.bin",     0x4000, 0x1000, 0xd1db1bb0 )
	/* no ROM at 5000 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ck6.bin",      0x0000, 0x1000, 0xa8916dc8 )
	ROM_LOAD( "ck4.bin",      0x1000, 0x1000, 0xb62a0367 )
	ROM_LOAD( "ck5.bin",      0x2000, 0x1000, 0xcd3b5dde )
	ROM_LOAD( "ck3.bin",      0x3000, 0x1000, 0x61122c5e )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ck2.bin",      0x0000, 0x0800, 0xf67c80f1 )
	ROM_LOAD( "ck1.bin",      0x0800, 0x0800, 0x80eb517d )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cclimber.pr1", 0x0000, 0x0020, 0x00000000 )
	ROM_LOAD( "cclimber.pr2", 0x0020, 0x0020, 0x00000000 )
	ROM_LOAD( "ck6t.bin",     0x0040, 0x0020, 0x00000000 )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc13j.bin",    0x0000, 0x1000, 0x5f0bcdfb )
	ROM_LOAD( "ck12.bin",     0x1000, 0x1000, 0x2eb23b60 )
ROM_END

ROM_START( rpatrolb )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "rp1.4l",       0x0000, 0x1000, 0xbfd7ae7a )
	ROM_LOAD( "rp2.4j",       0x1000, 0x1000, 0x03f53340 )
	ROM_LOAD( "rp3.4f",       0x2000, 0x1000, 0x8fa300df )
	ROM_LOAD( "rp4.4e",       0x3000, 0x1000, 0x74a8f1f4 )
	ROM_LOAD( "rp5.4c",       0x4000, 0x1000, 0xd7ef6c87 )
	/* no ROM at 5000 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rp6.6n",       0x0000, 0x0800, 0x19f18e9e )
	/* 0800-0fff empty */
	ROM_LOAD( "rp8.6k",       0x1000, 0x0800, 0x008738c7 )
	/* 1800-1fff empty */
	ROM_LOAD( "rp7.6l",       0x2000, 0x0800, 0x07f2070d )
	/* 2800-2fff empty */
	ROM_LOAD( "rp9.6h",       0x3000, 0x0800, 0xea5aafca )
	/* 3800-3fff empty */

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rp11.6c",      0x0000, 0x0800, 0x065651a5 )
	ROM_LOAD( "rp10.6a",      0x0800, 0x0800, 0x59747c31 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "bprom1.9n",    0x0000, 0x0020, 0xf9a2383b )
	ROM_LOAD( "bprom2.9p",    0x0020, 0x0020, 0x1743bd26 )
	ROM_LOAD( "bprom3.9c",    0x0040, 0x0020, 0xee03bc96 )

	/* no samples */
ROM_END

ROM_START( silvland )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "7.2r",         0x0000, 0x1000, 0x57e6be62 )
	ROM_LOAD( "8.1n",         0x1000, 0x1000, 0xbbb2b287 )
	ROM_LOAD( "rp3.4f",       0x2000, 0x1000, 0x8fa300df )
	ROM_LOAD( "10.2n",        0x3000, 0x1000, 0x5536a65d )
	ROM_LOAD( "11.1r",        0x4000, 0x1000, 0x6f23f66f )
	ROM_LOAD( "12.2k",        0x5000, 0x1000, 0x26f1537c )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "6.6n",         0x0000, 0x0800, 0xaffb804f )
	/* 0800-0fff empty */
	ROM_LOAD( "4.6k",         0x1000, 0x0800, 0xe487579d )
	/* 1800-1fff empty */
	ROM_LOAD( "5.6l",         0x2000, 0x0800, 0xad4642e5 )
	/* 2800-2fff empty */
	ROM_LOAD( "3.6h",         0x3000, 0x0800, 0x59125a1a )
	/* 3800-3fff empty */

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2.6c",         0x0000, 0x0800, 0xc8d32b8e )
	ROM_LOAD( "1.6a",         0x0800, 0x0800, 0xee333daf )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "mb7051.1v",    0x0000, 0x0020, 0x1d2343b1 )
	ROM_LOAD( "mb7051.1u",    0x0020, 0x0020, 0xc174753c )
	ROM_LOAD( "mb7051.1t",    0x0040, 0x0020, 0x04a1be01 )

	/* no samples */
ROM_END





/***************************************************************************

  Swimmer driver

***************************************************************************/

WRITE_HANDLER( swimmer_bgcolor_w );
WRITE_HANDLER( swimmer_palettebank_w );
void swimmer_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void swimmer_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( swimmer_sidepanel_enable_w );



WRITE_HANDLER( swimmer_sh_soundlatch_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,0xff);
}



static struct MemoryReadAddress swimmer_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9400, 0x97ff, videoram_r }, /* mirror address (used by Swimmer) */
	{ 0x9c00, 0x9fff, MRA_RAM },
	{ 0xa000, 0xa000, input_port_0_r },
	{ 0xa800, 0xa800, input_port_1_r },
	{ 0xb000, 0xb000, input_port_2_r },
	{ 0xb800, 0xb800, input_port_3_r },
	{ 0xb880, 0xb880, input_port_4_r },
	{ 0xc000, 0xc7ff, MRA_RAM },    /* ??? used by Guzzler */
	{ 0xe000, 0xffff, MRA_ROM },    /* Guzzler only */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress swimmer_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x88ff, cclimber_bigsprite_videoram_w, &cclimber_bsvideoram, &cclimber_bsvideoram_size },
	{ 0x8900, 0x89ff, cclimber_bigsprite_videoram_w },      /* mirror for the above (Guzzler writes to both) */
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0x9400, 0x97ff, videoram_w }, /* mirror address (used by Guzzler) */
	{ 0x9800, 0x981f, MWA_RAM, &cclimber_column_scroll },
	{ 0x9880, 0x989f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x98fc, 0x98ff, MWA_RAM, &cclimber_bigspriteram },
	{ 0x9c00, 0x9fff, cclimber_colorram_w, &colorram },
	{ 0xa000, 0xa000, interrupt_enable_w },
	{ 0xa001, 0xa002, cclimber_flipscreen_w },
	{ 0xa003, 0xa003, swimmer_sidepanel_enable_w },
	{ 0xa004, 0xa004, swimmer_palettebank_w },
	{ 0xa800, 0xa800, swimmer_sh_soundlatch_w },
	{ 0xb800, 0xb800, swimmer_bgcolor_w },  /* river color in Swimmer */
	{ 0xc000, 0xc7ff, MWA_RAM },    /* ??? used by Guzzler */
	{ 0xe000, 0xffff, MWA_ROM },    /* Guzzler only */
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x4000, 0x4001, MRA_RAM },    /* ??? */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x4000, 0x4000, MWA_RAM },    /* ??? */
	{ -1 }  /* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_write_port_0_w },
	{ 0x01, 0x01, AY8910_control_port_0_w },
	{ 0x80, 0x80, AY8910_write_port_1_w },
	{ 0x81, 0x81, AY8910_control_port_1_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( swimmer )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x04, "20000" )
	PORT_DIPSETTING(    0x08, "30000" )
	PORT_DIPSETTING(    0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_6C ) )

	PORT_START      /* IN3/DSW2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "???" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0xc0, "Hard" )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( guzzler )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )   /* probably unused */

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "64", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "20000 50000" )
	PORT_DIPSETTING(    0x00, "30000 100000" )
	PORT_DIPSETTING(    0x08, "30000" )
	PORT_DIPSETTING(    0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_6C ) )

	PORT_START      /* DSW1 */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )     /* probably unused */
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0020, 0x00, "High Score Names" )
	PORT_DIPSETTING(    0x0020, "3 Letters" )
	PORT_DIPSETTING(    0x00, "10 Letters" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0xc0, "Hardest" )

	PORT_START      /* coin */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 2)
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 2)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )     /* probably unused */
INPUT_PORTS_END



static struct GfxLayout swimmer_charlayout =
{
	8,8,    /* 8*8 characters */
	512,    /* 512 characters */
	3,      /* 3 bits per pixel */
	{ 0, 512*8*8, 512*2*8*8 },      /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	     /* characters are upside down */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout swimmer_spritelayout =
{
	16,16,  /* 16*16 sprites */
	128,    /* 128 sprites */
	3,	      /* 3 bits per pixel */
	{ 0, 128*16*16, 128*2*16*16 },  /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,       /* pretty straightforward layout */
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo swimmer_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &swimmer_charlayout,      0, 64 }, /* characters */
	{ REGION_GFX1, 0, &swimmer_spritelayout,    0, 32 }, /* sprite set #1 */
	{ REGION_GFX2, 0, &swimmer_charlayout,   64*8, 4 },  /* big sprite set */
	{ -1 } /* end of array */
};



static struct AY8910interface swimmer_ay8910_interface =
{
	2,      /* 2 chips */
	4000000/2,	/* 2 MHz */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_swimmer =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 MHz */
			swimmer_readmem,swimmer_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000/2,	/* 2 MHz */
			sound_readmem,sound_writemem,0,sound_writeport,
			0,0,
			nmi_interrupt,4000000/16384 /* IRQs are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	swimmer_gfxdecodeinfo,
	256+32+2,64*8+4*8,
	swimmer_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,
	0,
	cclimber_vh_start,
	cclimber_vh_stop,
	swimmer_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&swimmer_ay8910_interface
		}
	}
};



ROM_START( swimmer )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "sw1",          0x0000, 0x1000, 0xf12481e7 )
	ROM_LOAD( "sw2",          0x1000, 0x1000, 0xa0b6fdd2 )
	ROM_LOAD( "sw3",          0x2000, 0x1000, 0xec93d7de )
	ROM_LOAD( "sw4",          0x3000, 0x1000, 0x0107927d )
	ROM_LOAD( "sw5",          0x4000, 0x1000, 0xebd8a92c )
	ROM_LOAD( "sw6",          0x5000, 0x1000, 0xf8539821 )
	ROM_LOAD( "sw7",          0x6000, 0x1000, 0x37efb64e )
	ROM_LOAD( "sw8",          0x7000, 0x1000, 0x33d6001e )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "sw12",         0x0000, 0x1000, 0x2eee9bcb )

	ROM_REGION( 0x3000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sw15",         0x0000, 0x1000, 0x4f3608cb )  /* chars */
	ROM_LOAD( "sw14",         0x1000, 0x1000, 0x7181c8b4 )
	ROM_LOAD( "sw13",         0x2000, 0x1000, 0x2eb1af5c )

	ROM_REGION( 0x3000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sw23",         0x0000, 0x0800, 0x9ca67e24 )  /* bigsprite data */
	ROM_RELOAD(               0x0800, 0x0800 )	/* Guzzler has larger ROMs */
	ROM_LOAD( "sw22",         0x1000, 0x0800, 0x02c10992 )
	ROM_RELOAD(               0x1800, 0x0800 )	/* Guzzler has larger ROMs */
	ROM_LOAD( "sw21",         0x2000, 0x0800, 0x7f4993c1 )
	ROM_RELOAD(               0x2800, 0x0800 )	/* Guzzler has larger ROMs */

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "8220.clr",     0x0000, 0x100, 0x72c487ed )
	ROM_LOAD( "8212.clr",     0x0100, 0x100, 0x39037799 )
	ROM_LOAD( "8221.clr",     0x0200, 0x020, 0x3b2deb3a )
ROM_END

ROM_START( swimmera )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "swa1",         0x0000, 0x1000, 0x42c2b6c5 )
	ROM_LOAD( "swa2",         0x1000, 0x1000, 0x49bac195 )
	ROM_LOAD( "swa3",         0x2000, 0x1000, 0xa6d8cb01 )
	ROM_LOAD( "swa4",         0x3000, 0x1000, 0x7be75182 )
	ROM_LOAD( "swa5",         0x4000, 0x1000, 0x78f79573 )
	ROM_LOAD( "swa6",         0x5000, 0x1000, 0xfda9b311 )
	ROM_LOAD( "swa7",         0x6000, 0x1000, 0x7090e5ee )
	ROM_LOAD( "swa8",         0x7000, 0x1000, 0xab86efa9 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "sw12",         0x0000, 0x1000, 0x2eee9bcb )

	ROM_REGION( 0x3000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sw15",         0x0000, 0x1000, 0x4f3608cb )  /* chars */
	ROM_LOAD( "sw14",         0x1000, 0x1000, 0x7181c8b4 )
	ROM_LOAD( "sw13",         0x2000, 0x1000, 0x2eb1af5c )

	ROM_REGION( 0x3000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sw23",         0x0000, 0x0800, 0x9ca67e24 )  /* bigsprite data */
	ROM_RELOAD(               0x0800, 0x0800 )	/* Guzzler has larger ROMs */
	ROM_LOAD( "sw22",         0x1000, 0x0800, 0x02c10992 )
	ROM_RELOAD(               0x1800, 0x0800 )	/* Guzzler has larger ROMs */
	ROM_LOAD( "sw21",         0x2000, 0x0800, 0x7f4993c1 )
	ROM_RELOAD(               0x2800, 0x0800 )	/* Guzzler has larger ROMs */

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "8220.clr",     0x0000, 0x100, 0x72c487ed )
	ROM_LOAD( "8212.clr",     0x0100, 0x100, 0x39037799 )
	ROM_LOAD( "8221.clr",     0x0200, 0x020, 0x3b2deb3a )
ROM_END

ROM_START( guzzler )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "guzz-01.bin",  0x0000, 0x2000, 0x58aaa1e9 )
	ROM_LOAD( "guzz-02.bin",  0x2000, 0x2000, 0xf80ceb17 )
	ROM_LOAD( "guzz-03.bin",  0x4000, 0x2000, 0xe63c65a2 )
	ROM_LOAD( "guzz-04.bin",  0x6000, 0x2000, 0x45be42f5 )
	ROM_LOAD( "guzz-16.bin",  0xe000, 0x2000, 0x61ee00b7 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound board */
	ROM_LOAD( "guzz-12.bin",  0x0000, 0x1000, 0xf3754d9e )

	ROM_REGION( 0x3000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "guzz-13.bin",  0x0000, 0x1000, 0xafc464e2 )   /* chars */
	ROM_LOAD( "guzz-14.bin",  0x1000, 0x1000, 0xacbdfe1f )
	ROM_LOAD( "guzz-15.bin",  0x2000, 0x1000, 0x66978c05 )

	ROM_REGION( 0x3000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "guzz-11.bin",  0x0000, 0x1000, 0xec2e9d86 )   /* big sprite */
	ROM_LOAD( "guzz-10.bin",  0x1000, 0x1000, 0xbd3f0bf7 )
	ROM_LOAD( "guzz-09.bin",  0x2000, 0x1000, 0x18927579 )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "guzzler.003",  0x0000, 0x100, 0xf86930c1 )
	ROM_LOAD( "guzzler.002",  0x0100, 0x100, 0xb566ea9e )
	ROM_LOAD( "guzzler.001",  0x0200, 0x020, 0x69089495 )
ROM_END



GAME( 1980, cclimber, 0,        cclimber, cclimber, cclimber, ROT0,   "Nichibutsu", "Crazy Climber (US)" )
GAME( 1980, cclimbrj, cclimber, cclimber, cclimber, cclimbrj, ROT0,   "Nichibutsu", "Crazy Climber (Japan)" )
GAME( 1980, ccboot,   cclimber, cclimber, cclimber, cclimbrj, ROT0,   "bootleg", "Crazy Climber (bootleg set 1)" )
GAME( 1980, ccboot2,  cclimber, cclimber, cclimber, cclimbrj, ROT0,   "bootleg", "Crazy Climber (bootleg set 2)" )
GAME( 1981, ckong,    0,        cclimber, ckong,    0,        ROT270, "Falcon", "Crazy Kong (set 1)" )
GAME( 1981, ckonga,   ckong,    cclimber, ckong,    0,        ROT270, "Falcon", "Crazy Kong (set 2)" )
GAME( 1981, ckongjeu, ckong,    cclimber, ckong,    0,        ROT270, "bootleg", "Crazy Kong (Jeutel bootleg)" )
GAME( 1981, ckongo,   ckong,    cclimber, ckong,    0,        ROT270, "bootleg", "Crazy Kong (Orca bootleg)" )
GAME( 1981, ckongalc, ckong,    cclimber, ckong,    0,        ROT270, "bootleg", "Crazy Kong (Alca bootleg)" )
GAME( 1981, monkeyd,  ckong,    cclimber, ckong,    0,        ROT270, "bootleg", "Monkey Donkey" )
GAME( ????, rpatrolb, 0,        cclimber, rpatrolb, 0,        ROT0,   "bootleg", "River Patrol (bootleg)" )
GAME( ????, silvland, rpatrolb, cclimber, rpatrolb, 0,        ROT0,   "Falcon", "Silver Land" )

GAME( 1982, swimmer,  0,       swimmer, swimmer, 0, ROT0,  "Tehkan", "Swimmer (set 1)" )
GAME( 1982, swimmera, swimmer, swimmer, swimmer, 0, ROT0,  "Tehkan", "Swimmer (set 2)" )
GAME( 1983, guzzler,  0,       swimmer, guzzler, 0, ROT90, "Tehkan", "Guzzler" )
