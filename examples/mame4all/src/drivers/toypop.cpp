#include "../machine/toypop.cpp"
#include "../vidhrdw/toypop.cpp"

/****************************************

TOYPOP
1986 Namco

driver by Edgardo E. Contini Salvan (pag2806@iperbole.bologna.it)

TOYPOP uses a 6809 main CPU,
another 6809 for the sound and
a 68000 to create the background image.

Libble Rabble should run on the same board

****************************************/

#include "driver.h"
#include "sound/namco.h"
#include "vidhrdw/generic.h"

// machine\toypop.c
void toypop_init_machine(void);
READ_HANDLER( toypop_cycle_r );
int toypop_interrupt(void);
WRITE_HANDLER( toypop_interrupt_enable_w );
WRITE_HANDLER( toypop_interrupt_disable_w );
extern unsigned char *toypop_sharedram_1, *toypop_sharedram_2, *toypop_customio, *toypop_speedup;
READ_HANDLER( toypop_sharedram_1_r );
WRITE_HANDLER( toypop_sharedram_1_w );
READ_HANDLER( toypop_sharedram_2_r );
WRITE_HANDLER( toypop_sharedram_2_w );
WRITE_HANDLER( toypop_cpu_reset_w );
READ_HANDLER( toypop_customio_r );

// vidhrdw\toypop.c
extern unsigned char *bg_image;
int toypop_vh_start(void);
void toypop_vh_stop(void);
READ_HANDLER( toypop_background_r );
WRITE_HANDLER( toypop_background_w );
WRITE_HANDLER( toypop_flipscreen_w );
void toypop_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void toypop_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

static struct MemoryReadAddress toypop_readmem_I_6809[] =
{
	{ 0x2800, 0x2fff, MRA_RAM },								/* shared RAM with the 68000 CPU */
	{ 0x6000, 0x602f, toypop_customio_r },						/* custom I/O chip interface */
	{ 0x6840, 0x6bff, MRA_RAM },								/* shared RAM with the sound CPU */
	{ 0x8000, 0xffff, MRA_ROM },								/* ROM code */
	{ 0x0000, 0x7fff, MRA_RAM },								/* RAM everywhere else */

	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress toypop_writemem_I_6809[] =
{
	{ 0x0000, 0x03ff, videoram_w, &videoram, &videoram_size },	/* video RAM */
	{ 0x0400, 0x07ff, colorram_w, &colorram },					/* color RAM */
	{ 0x0800, 0x0f7f, MWA_RAM },								/* general RAM, area 1 */
	{ 0x0f80, 0x0fff, MWA_RAM, &spriteram, &spriteram_size },	/* sprite RAM, area 1 */
	{ 0x1000, 0x177f, MWA_RAM },								/* general RAM, area 2 */
	{ 0x1780, 0x17ff, MWA_RAM, &spriteram_2 },					/* sprite RAM, area 2 */
	{ 0x1800, 0x1f7f, MWA_RAM },								/* general RAM, area 3 */
	{ 0x1f80, 0x1fff, MWA_RAM, &spriteram_3 },					/* sprite RAM, area 3 */
	{ 0x2800, 0x2fff, MWA_RAM, &toypop_sharedram_2 },			/* shared RAM with the 68000 CPU */
	{ 0x6000, 0x602f, MWA_RAM, &toypop_customio },				/* custom I/O chip interface */
	{ 0x6840, 0x6bff, MWA_RAM, &toypop_sharedram_1 },			/* shared RAM with the sound CPU */
	{ 0x7000, 0x7000, MWA_RAM },								/* watchdog timer ??? */
	// any of these four addresses could be the sound CPU reset
	// (at the start the program writes on all four)
//	{ 0x8000, 0x8000, MWA_NOP },								/* ??? */
//	{ 0x8800, 0x8800, MWA_NOP },								/* ??? */
//	{ 0x9000, 0x9000, MWA_NOP },								/* ??? */
	{ 0x9800, 0x9800, toypop_cpu_reset_w },						/* sound CPU reset ??? */
	{ 0xa000, 0xa001, MWA_NOP },								/* background image palette ??? */
	{ 0x8000, 0xffff, MWA_ROM },								/* ROM code */

	{ -1 }	/* end of table */
};

static struct MemoryReadAddress toypop_readmem_II_6809[] =
{
	{ 0x0040, 0x03ff, toypop_sharedram_1_r },	/* shared RAM with the main CPU */
	{ 0xe000, 0xffff, MRA_ROM },				/* ROM code */

	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress toypop_writemem_II_6809[] =
{
	{ 0x0000, 0x003f, mappy_sound_w, &namco_soundregs },	/* sound control registers */
	{ 0x0040, 0x03ff, toypop_sharedram_1_w },				/* shared RAM with the main CPU */
	{ 0x4000, 0x4000, MWA_RAM },							/* interrupt enable ??? */
	{ 0x6000, 0x6000, MWA_RAM },							/* watchdog ??? */
	{ 0xe000, 0xffff, MWA_ROM },							/* ROM code */

	{ -1 }	/* end of table */
};

static struct MemoryReadAddress toypop_readmem_68k[] =
{
	{ 0x000000, 0x007fff, MRA_ROM },				/* ROM code */
	{ 0x080000, 0x080001, toypop_cycle_r },			/* speed hack */
	{ 0x080000, 0x0bffff, MRA_BANK1 },				/* RAM */
	{ 0x100000, 0x100fff, toypop_sharedram_2_r },	/* shared RAM with the main CPU */
	{ 0x190000, 0x1901ff, MRA_BANK2 },				/* RAM */
	{ 0x190200, 0x19fdff, toypop_background_r },	/* RAM containing the background image */
	{ 0x19FE00, 0x1dffff, MRA_BANK3 },				/* RAM */

	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress toypop_writemem_68k[] =
{
	{ 0x000000, 0x007fff, MWA_ROM },						/* ROM code */
	{ 0x080000, 0x0bffff, MWA_BANK1, &toypop_speedup },		/* RAM */
	{ 0x100000, 0x100fff, toypop_sharedram_2_w },			/* shared RAM with the main CPU */
	{ 0x18fffc, 0x18ffff, toypop_flipscreen_w },			/* flip mode */
	{ 0x190000, 0x1901ff, MWA_BANK2 },						/* RAM */
	{ 0x190200, 0x19fdff, toypop_background_w, &bg_image },	/* RAM containing the background image */
	{ 0x19FE00, 0x1dffff, MWA_BANK3 },						/* RAM */
	{ 0x300000, 0x300001, toypop_interrupt_enable_w },		/* interrupt enable */
	{ 0x380000, 0x380001, toypop_interrupt_disable_w },		/* interrupt disable */

	{ -1 }	/* end of table */
};

//////////////////////////////////////////////////////////////////////////////////

INPUT_PORTS_START( toypop )
	// FAKE
	/* The player inputs and the dipswitches are not memory mapped,
		they are handled by an I/O chip (I guess). */
	/* These fake input ports are read by toypop_customio_r() */
	PORT_START	// IN0
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_COIN1)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_COIN2)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT(0x0c, IP_ACTIVE_HIGH, IPT_UNKNOWN)

	PORT_START	// IN1
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY | IPF_PLAYER2)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_PLAYER2)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_PLAYER2)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_BUTTON1)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_START1)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_START2)

	PORT_START	// DSW0
	PORT_DIPNAME(0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(   0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(   0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(   0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(   0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME(0x04, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(   0x04, DEF_STR( On ) )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_SERVICE | IPF_TOGGLE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(0x08, DEF_STR( On ) )
	PORT_DIPNAME(0x10, 0x00, "Freeze" )
	PORT_DIPSETTING(   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(   0x10, DEF_STR( On ) )
	PORT_DIPNAME(0x20, 0x00, "Level Select" )
	PORT_DIPSETTING(   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(   0x20, DEF_STR( On ) )
	PORT_DIPNAME(0x40, 0x00, "2p play" )
	PORT_DIPSETTING(   0x00, "2 Credits" )
	PORT_DIPSETTING(   0x40, "1 Credit" )
	PORT_DIPNAME(0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(   0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(   0x00, DEF_STR( On ) )

	PORT_START	// DSW1
	PORT_DIPNAME(0x01, 0x00, "Entering" )	// ???
	PORT_DIPSETTING(   0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(   0x00, DEF_STR( On ) )
	PORT_DIPNAME(0x06, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(   0x02, "Easy" )
	PORT_DIPSETTING(   0x00, "Normal" )
	PORT_DIPSETTING(   0x04, "Hard" )
	PORT_DIPSETTING(   0x06, "Very hard" )
	PORT_DIPNAME(0x08, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(   0x00, "Every 15000 points" )
	PORT_DIPSETTING(   0x08, "Every 20000 points" )
	PORT_DIPNAME(0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(   0x10, "1" )
	PORT_DIPSETTING(   0x20, "2" )
	PORT_DIPSETTING(   0x00, "3" )
	PORT_DIPSETTING(   0x30, "5" )
	PORT_DIPNAME(0xc0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(   0xc0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(   0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(   0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(   0x40, DEF_STR( 1C_2C ) )
INPUT_PORTS_END

///////////////////////////////////////////////////////////////////////////////////

static struct GfxLayout toypop_charlayout =
{
	8,8,             /* 8*8 characters */
	512,             /* 512 characters */
	2,             /* 2 bits per pixel */
	{ 0, 4 },      /* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },   /* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },   /* v offset */
	16*8           /* every char takes 16 bytes */
};

static struct GfxLayout toypop_spritelayout =
{
	16,16,       /* 16*16 sprites */
	256,            /* 256 sprites */
	2,                 /* 2 bits per pixel */
	{ 0, 4 },   /* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
	24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	32 * 8, 33 * 8, 34 * 8, 35 * 8, 36 * 8, 37 * 8, 38 * 8, 39 * 8 },
	64*8    /* every sprite takes 64 bytes */
};

static struct GfxDecodeInfo toypop_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &toypop_charlayout,      0, 128 },
	{ REGION_GFX2, 0, &toypop_spritelayout, 64*4, 256 },
	{ -1 } /* end of array */
};

/////////////////////////////////////////////////////////////////////////////////////

static struct namco_interface namco_interface =
{
	23920,	/* sample rate (approximate value) */
	8,		/* number of voices */
	100,	/* playback volume */
	REGION_SOUND1	/* memory region */
};

static struct MachineDriver machine_driver_toypop =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1600000,	/* 1.6 Mhz (?) */
			toypop_readmem_I_6809,toypop_writemem_I_6809,0,0,
			interrupt,1
		},
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			1600000,	/* 1.6 Mhz (?) */
			toypop_readmem_II_6809,toypop_writemem_II_6809,0,0,
			interrupt,1
		},
		{
			CPU_M68000,
			8000000,	/* 8 Mhz (?) */
			toypop_readmem_68k,toypop_writemem_68k,0,0,
			toypop_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	100,    /* 100 CPU slices per frame - an high value to ensure proper */
			/* synchronization of the CPUs */
	toypop_init_machine,

	/* video hardware */
	18*16, 14*16, { 0*16, 18*16-1, 0*16, 14*16-1 },
	toypop_gfxdecodeinfo,
	256,256+256,
	toypop_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	toypop_vh_start,
	toypop_vh_stop,
	toypop_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		}
	}
};



ROM_START( liblrabl )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for the first CPU */
	ROM_LOAD( "5b.rom",   0x8000, 0x4000, 0xda7a93c2 )
	ROM_LOAD( "5c.rom",   0xc000, 0x4000, 0x6cae25dc )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "2c.rom",   0xe000, 0x2000, 0x7c09e50a )

	ROM_REGION( 0x8000, REGION_CPU3 )		/* 32k for the third CPU */
	ROM_LOAD_EVEN( "8c.rom",   0x0000, 0x4000, 0xa00cd959 )
	ROM_LOAD_ODD( "10c.rom",   0x0000, 0x4000, 0x09ce209b )

	/* temporary space for graphics (disposed after conversion) */
	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5p.rom",   0x0000, 0x2000, 0x3b4937f0 )	/* characters */

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9t.rom",   0x0000, 0x4000, 0xa88e24ca )	/* sprites */

	ROM_REGION( 0x0600, REGION_PROMS )	/* color proms */
	ROM_LOAD( "lr1-3.1r", 0x0000, 0x0100, 0xf3ec0d07 )	// palette: red component
	ROM_LOAD( "lr1-2.1s", 0x0100, 0x0100, 0x2ae4f702 )	// palette: green component
	ROM_LOAD( "lr1-1.1t", 0x0200, 0x0100, 0x7601f208 )	// palette: blue component
	ROM_LOAD( "lr1-5.5l", 0x0300, 0x0100, 0x940f5397 )	/* characters */
	ROM_LOAD( "lr1-6.2p", 0x0400, 0x0200, 0xa6b7f850 )	/* sprites */

	ROM_REGION( 0x0100, REGION_SOUND1 )	/* sound prom */
	ROM_LOAD( "lr1-4.3d", 0x0000, 0x0100, 0x16a9166a )
ROM_END

ROM_START( toypop )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for the first CPU */
	ROM_LOAD( "tp1-2.5b", 0x8000, 0x4000, 0x87469620 )
	ROM_LOAD( "tp1-1.5c", 0xc000, 0x4000, 0xdee2fd6e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "tp1-3.2c", 0xe000, 0x2000, 0x5f3bf6e2 )

	ROM_REGION( 0x8000, REGION_CPU3 )		/* 32k for the third CPU */
	ROM_LOAD_EVEN( "tp1-4.8c", 0x0000, 0x4000, 0x76997db3 )
	ROM_LOAD_ODD( "tp1-5.10c", 0x0000, 0x4000, 0x37de8786 )

	/* temporary space for graphics (disposed after conversion) */
	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tp1-7.5p", 0x0000, 0x2000, 0x95076f9e )	/* characters */

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tp1-6.9t", 0x0000, 0x4000, 0x481ffeaf )	/* sprites */

	ROM_REGION( 0x0600, REGION_PROMS )	/* color proms */
	ROM_LOAD( "tp1-3.1r", 0x0000, 0x0100, 0xcfce2fa5 )	// palette: red component
	ROM_LOAD( "tp1-2.1s", 0x0100, 0x0100, 0xaeaf039d )	// palette: green component
	ROM_LOAD( "tp1-1.1t", 0x0200, 0x0100, 0x08e7cde3 )	// palette: blue component
	ROM_LOAD( "tp1-4.5l", 0x0300, 0x0100, 0x74138973 )	/* characters */
	ROM_LOAD( "tp1-5.2p", 0x0400, 0x0200, 0x4d77fa5a )	/* sprites */

	ROM_REGION( 0x0100, REGION_SOUND1 )	/* sound prom */
	ROM_LOAD( "lr1-4.3d", 0x0000, 0x0100, 0x16a9166a )
ROM_END


GAMEX(1983, liblrabl, 0, toypop, toypop, 0, ROT0, "Namco", "Libble Rabble", GAME_NOT_WORKING )
GAME( 1986, toypop,   0, toypop, toypop, 0, ROT0, "Namco", "Toypop" )
