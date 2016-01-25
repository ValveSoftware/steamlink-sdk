#include "../machine/geebee.cpp"
#include "../vidhrdw/geebee.cpp"
#include "../sndhrdw/geebee.cpp"

/****************************************************************************
 *
 * geebee.c
 *
 * system driver
 * juergen buchmueller <pullmoll@t-online.de>, jan 2000
 *
 * memory map (preliminary)
 * 0000-0fff ROM1 / ROM0
 * 1000-1fff ROM2
 * 2000-2fff VRAM
 * 3000-3fff CGROM
 * 4000-4fff RAM
 * 5000-5fff IN
 *			 A1 A0
 *			  0  0	  SW0
 *					  D0 COIN1
 *					  D1 COIN2
 *					  D2 START1
 *					  D3 START2
 *					  D4 BUTTON1
 *					  D5 TEST MODE
 *			  0  1	  SW1
 *					  - not used in Gee Bee
 *					  - digital joystick left/right and button in
 *						Kaitei Tagara Sagashi (two in Cocktail mode)
 *			  1  0	  DSW2
 *					  D0	cabinet: 0= upright  1= table
 *					  D1	balls:	 0= 3		 1= 5
 *					  D2-D3 coinage: 0=1c/1c 1=1c/2c 2=2c/1c 3=free play
 *					  D4-D5 bonus:	 0=none, 1=40k	 2=70k	 3=100k
 *			  1  1	  VOLIN
 *					  D0-D7 vcount where paddle starts (note: rotated 90 deg!)
 *					  - not used(?) in Kaitei Tagara Sagashi
 * 6000-6fff OUT6
 *			 A1 A0
 *			  0  0	  BALL H
 *			  0  1	  BALL V
 *			  1  0	  n/c
 *			  1  1	  SOUND
 *					  D3 D2 D1 D0	   sound
 *					   x  0  0	0  PURE TONE 4V (2000Hz)
 *					   x  0  0	1  PURE TONE 8V (1000Hz)
 *					   x  0  1	0  PURE TONE 16V (500Hz)
 *					   x  0  1	1  PURE TONE 32V (250Hz)
 *					   x  1  0	0  TONE1 (!1V && !16V)
 *					   x  1  0	1  TONE2 (!2V && !32V)
 *					   x  1  1	0  TONE3 (!4V && !64V)
 *					   x  1  1	1  NOISE
 *					   0  x  x	x  DECAY
 *					   1  x  x	x  FULL VOLUME
 * 7000-7fff OUT7
 *			 A2 A1 A0
 *			  0  0	0 LAMP 1
 *			  0  0	1 LAMP 2
 *			  0  1	0 LAMP 3
 *			  0  1	1 COUNTER
 *			  1  0	0 LOCK OUT COIL
 *			  1  0	1 BGW
 *			  1  1	0 BALL ON
 *			  1  1	1 INV
 * 8000-ffff INTA (read FF)
 *
 * TODO:
 * add second controller for cocktail mode and two players?
 *
 ****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* from machine/geebee.c */
extern int geebee_interrupt(void);
extern int kaitei_interrupt(void);
READ_HANDLER( geebee_in_r );
READ_HANDLER( navalone_in_r );
WRITE_HANDLER( geebee_out6_w );
WRITE_HANDLER( geebee_out7_w );

/* from vidhrdw/geebee.c */
extern void geebee_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable, const unsigned char *color_prom);
extern void navalone_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable, const unsigned char *color_prom);

extern int geebee_vh_start(void);
extern int navalone_vh_start(void);
extern int kaitei_vh_start(void);
extern int sos_vh_start(void);
extern void geebee_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

/* from sndhrdw/geebee.c */
WRITE_HANDLER( geebee_sound_w );
extern int geebee_sh_start(const struct MachineSound *msound);
extern void geebee_sh_stop(void);
extern void geebee_sh_update(void);

/*******************************************************
 *
 * memory regions
 *
 *******************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },	/* GeeBee uses only the first 4K */
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x3000, 0x37ff, MRA_ROM },	/* GeeBee uses only the first 1K */
	{ 0x4000, 0x40ff, MRA_RAM },
	{ 0x5000, 0x5fff, geebee_in_r },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress readmem_navalone[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x3000, 0x37ff, MRA_ROM },
	{ 0x4000, 0x40ff, MRA_RAM },
	{ 0x5000, 0x5fff, navalone_in_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x23ff, videoram_w, &videoram, &videoram_size },
	{ 0x2400, 0x27ff, videoram_w }, /* mirror used in kaitei */
	{ 0x3000, 0x37ff, MWA_ROM },
    { 0x4000, 0x40ff, MWA_RAM },
	{ 0x6000, 0x6fff, geebee_out6_w },
	{ 0x7000, 0x7fff, geebee_out7_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x50, 0x5f, geebee_in_r },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport_navalone[] =
{
	{ 0x50, 0x5f, navalone_in_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x60, 0x6f, geebee_out6_w },
	{ 0x70, 0x7f, geebee_out7_w },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( geebee )
	PORT_START		/* IN0 SW0 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW, IPT_COIN1   )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW, IPT_COIN2   )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

	PORT_START      /* IN1 SW1 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

	PORT_START      /* IN2 DSW2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( Free_Play ) )
	/* Bonus Life moved to two inputs to allow changing 3/5 lives mode separately */
	PORT_BIT	( 0x30, 0x00, IPT_UNUSED )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

	PORT_START		/* IN3 VOLIN */
	PORT_ANALOG( 0xff, 0x58, IPT_PADDLE | IPF_REVERSE, 30, 15, 0x10, 0xa0 )

	PORT_START		/* IN4 FAKE for 3 lives */
	PORT_BIT	( 0x0f, 0x00, IPT_UNUSED )
	PORT_DIPNAME( 0x30, 0x00, "Bonus Life (3 lives)" )
	PORT_DIPSETTING(    0x10, "40k 80k" )
	PORT_DIPSETTING(    0x20, "70k 140k" )
	PORT_DIPSETTING(    0x30, "100k 200k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

	PORT_START		/* IN5 FAKE for 5 lives */
	PORT_BIT	( 0x0f, 0x00, IPT_UNUSED )
	PORT_DIPNAME( 0x30, 0x00, "Bonus Life (5 lives)" )
	PORT_DIPSETTING(    0x10, "60k 120k" )
	PORT_DIPSETTING(    0x20, "100k 200k" )
	PORT_DIPSETTING(    0x30, "150k 300k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

INPUT_PORTS_END

INPUT_PORTS_START( navalone )
	PORT_START		/* IN0 SW0 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW, IPT_COIN1   )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT	( 0x20, IP_ACTIVE_LOW, IPT_COIN2   )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

	PORT_START      /* IN1 SW1 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT	( 0xc0, 0x00, IPT_UNUSED )

	PORT_START      /* IN2 DSW2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x02, "3" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
    PORT_DIPNAME( 0x38, 0x10, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x30, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x10, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Free_Play ) )
    PORT_BIT    ( 0xc0, 0x00, IPT_UNUSED )

	PORT_START		/* IN3 VOLIN */
	PORT_BIT	( 0xff, 0x58, IPT_UNUSED )

	PORT_START		/* IN4 two-way digital joystick */
	PORT_BIT	( 0x01, 0x00, IPT_JOYSTICK_LEFT )
	PORT_BIT	( 0x02, 0x00, IPT_JOYSTICK_RIGHT )

INPUT_PORTS_END

INPUT_PORTS_START( kaitei )
	PORT_START		/* IN0 SW0 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW,	IPT_COIN1 )
	PORT_BIT	( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW,	IPT_START1 )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW,	IPT_START2 )
	PORT_BIT	( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
    PORT_BIT    ( 0x20, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT	( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT	( 0x80, IP_ACTIVE_LOW,	IPT_UNKNOWN )

	PORT_START      /* IN1 SW1 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW,	IPT_JOYSTICK_RIGHT )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW,	IPT_JOYSTICK_LEFT )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW,	IPT_BUTTON1 )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW,	IPT_JOYSTICK_RIGHT | IPF_COCKTAIL )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW,	IPT_JOYSTICK_LEFT | IPF_COCKTAIL )
	PORT_BIT	( 0x20, IP_ACTIVE_LOW,	IPT_BUTTON1 | IPF_COCKTAIL )
    PORT_BIT    ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT	( 0x80, IP_ACTIVE_LOW,	IPT_UNKNOWN )

	PORT_START      /* IN2 DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Cocktail) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x04, "5" )
    PORT_DIPSETTING(    0x00, "7" )
    PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
    PORT_BIT    ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT	( 0x80, IP_ACTIVE_LOW,	IPT_UNKNOWN )

	PORT_START		/* IN3 VOLIN */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW,	IPT_UNKNOWN )
    PORT_BIT    ( 0x08, IP_ACTIVE_LOW,  IPT_UNKNOWN )
    PORT_BIT    ( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
    PORT_BIT    ( 0x20, IP_ACTIVE_LOW,  IPT_UNKNOWN )
    PORT_BIT    ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT	( 0x80, IP_ACTIVE_LOW,	IPT_UNKNOWN )

INPUT_PORTS_END

INPUT_PORTS_START( kaitein )
	PORT_START		/* IN0 SW0 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW,	IPT_COIN1 )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW,	IPT_COIN2 )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW,	IPT_START1 )
	PORT_BIT	( 0x08, IP_ACTIVE_LOW,	IPT_START2 )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW,	IPT_BUTTON1 )
	PORT_BIT	( 0x20, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x40, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x80, IP_ACTIVE_LOW,	IPT_UNKNOWN )

	PORT_START      /* IN1 SW1 */
	PORT_BIT	( 0x01, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x02, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x04, IP_ACTIVE_LOW,	IPT_UNKNOWN )
    PORT_BIT    ( 0x08, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT	( 0x10, IP_ACTIVE_LOW,	IPT_UNKNOWN )
    PORT_BIT    ( 0x20, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT	( 0x40, IP_ACTIVE_LOW,	IPT_UNKNOWN )
    PORT_BIT    ( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

    PORT_START      /* IN2 DSW2 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x01, "3" )
	PORT_DIPSETTING(	0x02, "4" )
	PORT_DIPSETTING(	0x03, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
    PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x30, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x10, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_BIT	( 0x40, IP_ACTIVE_LOW,	IPT_UNKNOWN )
	PORT_BIT	( 0x80, IP_ACTIVE_LOW,	IPT_UNKNOWN )

	PORT_START		/* IN3 VOLIN */
	PORT_BIT	( 0xff, 0x58, IPT_UNUSED )

	PORT_START		/* IN4 two-way digital joystick */
	PORT_BIT	( 0x01, 0x00, IPT_JOYSTICK_LEFT )
	PORT_BIT	( 0x02, 0x00, IPT_JOYSTICK_RIGHT )

INPUT_PORTS_END

INPUT_PORTS_START( sos )
    PORT_START      /* IN0 SW0 */
    PORT_BIT    ( 0x01, IP_ACTIVE_LOW, IPT_COIN1   )
    PORT_BIT    ( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT    ( 0x04, IP_ACTIVE_LOW, IPT_START1  )
    PORT_BIT    ( 0x08, IP_ACTIVE_LOW, IPT_START2  )
    PORT_BIT    ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
    PORT_BIT    ( 0x20, IP_ACTIVE_LOW, IPT_COIN2   )
    PORT_BIT    ( 0xc0, 0x00, IPT_UNUSED )

    PORT_START      /* IN1 SW1 */
    PORT_BIT    ( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT    ( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT    ( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT    ( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT    ( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT    ( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT    ( 0xc0, 0x00, IPT_UNUSED )

    PORT_START      /* IN2 DSW2 */
    PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
    PORT_DIPSETTING(    0x01, DEF_STR( Cocktail) )
    PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x00, "2" )
    PORT_DIPSETTING(    0x02, "3" )
    PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x38, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x30, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
    PORT_BIT    ( 0xc0, 0x00, IPT_UNUSED )

    PORT_START      /* IN3 VOLIN */
	PORT_ANALOG( 0xff, 0x58, IPT_PADDLE | IPF_REVERSE, 30, 15, 0x10, 0xa0 )

INPUT_PORTS_END

static struct GfxLayout charlayout_1k =
{
	8, 8,							   /* 8x8 pixels */
	128,							   /* 128 codes */
	1,								   /* 1 bit per pixel */
	{0},							   /* no bitplanes */
	/* x offsets */
	{0,1,2,3,4,5,6,7},
	/* y offsets */
    {0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8},
	8 * 8							   /* eight bytes per code */
};

static struct GfxDecodeInfo gfxdecodeinfo_1k[] =
{
	{ REGION_CPU1, 0x3000, &charlayout_1k, 0, 4 },
	{-1}							   /* end of array */
};

static struct GfxLayout charlayout_2k =
{
    8, 8,                              /* 8x8 pixels */
	256,							   /* 256 codes */
    1,                                 /* 1 bit per pixel */
    {0},                               /* no bitplanes */
    /* x offsets */
    {0,1,2,3,4,5,6,7},
    /* y offsets */
    {0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8},
    8 * 8                              /* eight bytes per code */
};

static struct GfxDecodeInfo gfxdecodeinfo_2k[] =
{
	{ REGION_CPU1, 0x3000, &charlayout_2k, 0, 4 },
	{-1}							   /* end of array */
};

static struct CustomSound_interface custom_interface =
{
	geebee_sh_start,
	geebee_sh_stop,
	geebee_sh_update
};

static struct MachineDriver machine_driver_geebee =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			18432000/9, 		/* 18.432 Mhz / 9 */
			readmem,writemem,readport,writeport,
			geebee_interrupt,1	/* one interrupt per frame */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	34*8, 32*8, { 0*8, 34*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo_1k,  /* gfxdecodeinfo */
	3+32768, 4*2,		/* extra colors for the overlay */
    geebee_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	geebee_vh_start,
	generic_vh_stop,
	geebee_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_CUSTOM,
			&custom_interface
		}
    }
};


static struct MachineDriver machine_driver_navalone =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			18432000/9, 		/* 18.432 Mhz / 9 */
			readmem_navalone,writemem,readport_navalone,writeport,
			geebee_interrupt,1	/* one interrupt per frame */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	34*8, 32*8, { 0*8, 34*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo_2k,  /* gfxdecodeinfo */
	3+32768, 4*2,		/* extra colors for the overlay */
    navalone_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	navalone_vh_start,
	generic_vh_stop,
	geebee_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_CUSTOM,
			&custom_interface
		}
    }
};



static struct MachineDriver machine_driver_kaitei =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			18432000/9, 		/* 18.432 Mhz / 9 */
			readmem_navalone,writemem,readport_navalone,writeport,
			kaitei_interrupt,1	/* one interrupt per frame */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	34*8, 32*8, { 0*8, 34*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo_2k,  /* gfxdecodeinfo */
	3+32768, 4*2,		/* extra colors for the overlay */
	navalone_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	kaitei_vh_start,
	generic_vh_stop,
	geebee_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_CUSTOM,
			&custom_interface
		}
    }
};

static struct MachineDriver machine_driver_sos =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			18432000/9, 		/* 18.432 Mhz / 9 */
			readmem_navalone,writemem,readport_navalone,writeport,
			geebee_interrupt,1	/* one interrupt per frame */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	34*8, 32*8, { 0*8, 34*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo_2k,  /* gfxdecodeinfo */
	3+32768, 4*2,		/* extra colors for the overlay */
	navalone_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sos_vh_start,
	generic_vh_stop,
	geebee_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_CUSTOM,
			&custom_interface
		}
    }
};



ROM_START( geebee )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "geebee.1k",      0x0000, 0x1000, 0x8a5577e0 )
	ROM_LOAD( "geebee.3a",      0x3000, 0x0400, 0xf257b21b )
ROM_END

ROM_START( geebeeg )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "geebee.1k",      0x0000, 0x1000, 0x8a5577e0 )
	ROM_LOAD( "geebeeg.3a",     0x3000, 0x0400, 0xa45932ba )
ROM_END

ROM_START( navalone )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "navalone.p1",    0x0000, 0x0800, 0x5a32016b )
	ROM_LOAD( "navalone.p2",    0x0800, 0x0800, 0xb1c86fe3 )
	ROM_LOAD( "navalone.chr",   0x3000, 0x0800, 0xb26c6170 )
ROM_END

ROM_START( kaitei )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "kaitei_7.1k",    0x0000, 0x0800, 0x32f70d48 )
	ROM_RELOAD( 				0x0800, 0x0800 )
    ROM_LOAD( "kaitei_1.1m",    0x1000, 0x0400, 0x9a7ab3b9 )
	ROM_LOAD( "kaitei_2.1p",    0x1400, 0x0400, 0x5eeb0fff )
	ROM_LOAD( "kaitei_3.1s",    0x1800, 0x0400, 0x5dff4df7 )
	ROM_LOAD( "kaitei_4.1t",    0x1c00, 0x0400, 0xe5f303d6 )
	ROM_LOAD( "kaitei_5.bin",   0x3000, 0x0400, 0x60fdb795 )
	ROM_LOAD( "kaitei_6.bin",   0x3400, 0x0400, 0x21399ace )
ROM_END

ROM_START( kaitein )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "kaitein.p1",     0x0000, 0x0800, 0xd88e10ae )
	ROM_LOAD( "kaitein.p2",     0x0800, 0x0800, 0xaa9b5763 )
	ROM_LOAD( "kaitein.chr",    0x3000, 0x0800, 0x3125af4d )
ROM_END

ROM_START( sos )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "sos.p1",         0x0000, 0x0800, 0xf70bdafb )
	ROM_LOAD( "sos.p2",         0x0800, 0x0800, 0x58e9c480 )
	ROM_LOAD( "sos.chr",        0x3000, 0x0800, 0x66f983e4 )
ROM_END


GAME ( 1978, geebee,   0,        geebee,   geebee,   0, ROT90, "Namco", "Gee Bee" )
GAME ( 1978, geebeeg,  geebee,	 geebee,   geebee,   0, ROT90, "[Namco] (Gremlin license)", "Gee Bee (Gremlin)" )
GAMEX( 1980, navalone, 0,        navalone, navalone, 0, ROT90, "Namco", "Navalone", GAME_IMPERFECT_SOUND )
GAME ( 1980, kaitei,   0,        kaitei,   kaitei,   0, ROT90, "K.K. Tokki", "Kaitei Takara Sagashi" )
GAME ( 1980, kaitein,  kaitei,	 kaitei,   kaitein,  0, ROT90, "Namco", "Kaitei Takara Sagashi (Namco)" )
GAMEX( 1980, sos,	   0,        sos,      sos,      0, ROT90, "Namco", "SOS", GAME_IMPERFECT_SOUND )

