#include "../vidhrdw/ttmahjng.cpp"

/***************************************************************************


 ***************************************************************************/

#include "driver.h"

extern unsigned char *ttmahjng_sharedram;
extern unsigned char *ttmahjng_videoram1;
extern unsigned char *ttmahjng_videoram2;
extern size_t ttmahjng_videoram_size;

void ttmahjng_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  ttmahjng_vh_start(void);
void ttmahjng_vh_stop(void);
WRITE_HANDLER( ttmahjng_out0_w );
WRITE_HANDLER( ttmahjng_out1_w );
WRITE_HANDLER( ttmahjng_videoram1_w );
WRITE_HANDLER( ttmahjng_videoram2_w );
READ_HANDLER( ttmahjng_videoram1_r );
READ_HANDLER( ttmahjng_videoram2_r );
WRITE_HANDLER( ttmahjng_sharedram_w );
READ_HANDLER( ttmahjng_sharedram_r );
void ttmahjng_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static int psel;
WRITE_HANDLER( input_port_matrix_w )
{
	psel = data;
}

READ_HANDLER( input_port_matrix_r )
{
	int	cdata;

	cdata = 0;
	switch (psel)
	{
		case	1:
			cdata = readinputport(2);
			break;
		case	2:
			cdata = readinputport(3);
			break;
		case	4:
			cdata = readinputport(4);
			break;
		case	8:
			cdata = readinputport(5);
			break;
		default:
			break;
	}
	return cdata;
}


static struct MemoryReadAddress cpu1_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x43ff, ttmahjng_sharedram_r },
	{ 0x4800, 0x4800, input_port_0_r },
	{ 0x5000, 0x5000, input_port_1_r },
	{ 0x5800, 0x5800, input_port_matrix_r },
	{ 0x7838, 0x7838, MRA_NOP },
	{ 0x7859, 0x7859, MRA_NOP },
	{ 0x8000, 0xbfff, ttmahjng_videoram1_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cpu1_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x43ff, ttmahjng_sharedram_w, &ttmahjng_sharedram },
	{ 0x4800, 0x4800, ttmahjng_out0_w },
	{ 0x5000, 0x5000, ttmahjng_out1_w },
	{ 0x5800, 0x5800, input_port_matrix_w },
	{ 0x5f3e, 0x5f3e, MWA_NOP },
	{ 0x6800, 0x6800, AY8910_write_port_0_w },
	{ 0x6900, 0x6900, AY8910_control_port_0_w },
	{ 0x8000, 0xbfff, ttmahjng_videoram1_w, &ttmahjng_videoram1, &ttmahjng_videoram_size },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress cpu2_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x43ff, ttmahjng_sharedram_r },
	{ 0x8000, 0xbfff, ttmahjng_videoram2_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cpu2_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x43ff, ttmahjng_sharedram_w },
	{ 0x8000, 0xbfff, ttmahjng_videoram2_w, &ttmahjng_videoram2 },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( ttmahjng )
	PORT_START      /* IN0 */
	PORT_DIPNAME( 0x01, 0x00, "Unknown 01" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x01, "01" )
	PORT_DIPNAME( 0x02, 0x00, "Unknown 02" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x02, "02" )
	PORT_DIPNAME( 0x04, 0x00, "Unknown 04" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x04, "04" )
	PORT_DIPNAME( 0x08, 0x00, "Unknown 08" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x08, "08" )
	PORT_DIPNAME( 0x10, 0x00, "Unknown 10" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x10, "10" )
	PORT_DIPNAME( 0x20, 0x00, "Unknown 20" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x20, "20" )
	PORT_DIPNAME( 0x40, 0x00, "Unknown 40" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x40, "40" )
	PORT_DIPNAME( 0x80, 0x00, "Unknown 80" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x80, "80" )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 A", KEYCODE_A, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 I", KEYCODE_I, IP_JOY_NONE )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 M", KEYCODE_M, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Kan", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )		// START2?
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 B", KEYCODE_B, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 F", KEYCODE_F, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 J", KEYCODE_J, IP_JOY_NONE )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 N", KEYCODE_N, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Reach", KEYCODE_LSHIFT, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )		// START1?
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 C", KEYCODE_C, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 G", KEYCODE_G, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 K", KEYCODE_K, IP_JOY_NONE )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Ron", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Chi", KEYCODE_Z, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN5 */
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 D", KEYCODE_D, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 H", KEYCODE_H, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 L", KEYCODE_L, IP_JOY_NONE )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Pon", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


static struct AY8910interface ay8910_interface =
{
	1,			/* 1 chip */
	10000000/8, 		/* 10Mhz / 8 = 1.25Mhz */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


static struct MachineDriver machine_driver_ttmahjng =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80 | CPU_16BIT_PORT,
			2500000,	/* 10Mhz / 4 = 2.5Mhz */
			cpu1_readmem, cpu1_writemem, 0, 0,
			interrupt, 1
		},
		{
			CPU_Z80,
			2500000,	/* 10Mhz / 4 = 2.5Mhz */
			cpu2_readmem, cpu2_writemem, 0, 0,
			ignore_interrupt, 0
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0, 256-1 },
	0,
	8, 0,
	ttmahjng_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	0,
	ttmahjng_vh_start,
	ttmahjng_vh_stop,
	ttmahjng_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ttmahjng )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ju04", 0x0000, 0x1000, 0xfe7c693a )
	ROM_LOAD( "ju05", 0x1000, 0x1000, 0x985723d3 )
	ROM_LOAD( "ju06", 0x2000, 0x1000, 0x2cd69bc8 )
	ROM_LOAD( "ju07", 0x3000, 0x1000, 0x30e8ec63 )

	ROM_REGION( 0x0200, REGION_PROMS ) /* color proms */
	/* The upper 128 bytes are 0's, used by the hardware to blank the display */
	ROM_LOAD( "ju03", 0x0000, 0x0100, 0x27d47624 )
	ROM_LOAD( "ju09", 0x0100, 0x0100, 0x27d47624 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "ju01", 0x0000, 0x0800, 0x0f05ca3c )
	ROM_LOAD( "ju02", 0x0800, 0x0800, 0xc1ffeceb )
	ROM_LOAD( "ju08", 0x1000, 0x0800, 0x2dcc76b5 )
ROM_END


GAME( 1981, ttmahjng, 0, ttmahjng, ttmahjng, 0, ROT0, "Taito", "T.T. Mahjong" )
