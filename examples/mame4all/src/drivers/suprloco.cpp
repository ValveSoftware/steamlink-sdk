#include "../vidhrdw/suprloco.cpp"

/******************************************************************************

Super Locomotive

driver by Zsolt Vasvari

TODO:
- Bit 5 in suprloco_control_w is pulsed when loco turns "super". This is supposed
  to make red parts of sprites blink to purple, it's not clear how this is
  implemented in hardware, there's a hack to support it.

******************************************************************************/

#include "driver.h"
#include "vidhrdw/system1.h"
#include "cpu/z80/z80.h"


extern unsigned char *spriteram;
extern size_t spriteram_size;

extern unsigned char *suprloco_videoram;

void suprloco_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  suprloco_vh_start(void);
void suprloco_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( suprloco_videoram_w );
WRITE_HANDLER( suprloco_scrollram_w );
READ_HANDLER( suprloco_scrollram_r );
WRITE_HANDLER( suprloco_control_w );
READ_HANDLER( suprloco_control_r );


/* in machine/segacrpt.c */
void suprloco_decode(void);


static WRITE_HANDLER( suprloco_soundport_w )
{
	soundlatch_w(0,data);
	cpu_cause_interrupt(1,Z80_NMI_INT);
	/* spin for a while to let the Z80 read the command (fixes hanging sound in Regulus) */
	cpu_spinuntil_time(TIME_IN_USEC(50));
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc1ff, MRA_RAM },
	{ 0xc800, 0xc800, input_port_0_r },
	{ 0xd000, 0xd000, input_port_1_r },
	{ 0xd800, 0xd800, input_port_2_r },
	{ 0xe000, 0xe000, input_port_3_r },
	{ 0xe001, 0xe001, input_port_4_r },
	{ 0xe801, 0xe801, suprloco_control_r },
	{ 0xf000, 0xf6ff, MRA_RAM },
	{ 0xf7e0, 0xf7ff, suprloco_scrollram_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe800, 0xe800, suprloco_soundport_w },
	{ 0xe801, 0xe801, suprloco_control_w },
	{ 0xf000, 0xf6ff, suprloco_videoram_w, &suprloco_videoram },
	{ 0xf7e0, 0xf7ff, suprloco_scrollram_w },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 } /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa000, 0xa003, SN76496_0_w },
	{ 0xc000, 0xc003, SN76496_1_w },
	{ -1 } /* end of table */
};



INPUT_PORTS_START( suprloco )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START  /* DSW2 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x01, "30000" )
	PORT_DIPSETTING(    0x02, "40000" )
	PORT_DIPSETTING(    0x03, "50000" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Initial Entry" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8 by 8 */
	1024,	/* 1024 characters */
	4,		/* 4 bits per pixel */
	{ 0, 1024*8*8, 2*1024*8*8, 3*1024*8*8 },			/* plane */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	/* sprites use colors 256-511 + 512-767 */
	{ REGION_GFX1, 0x6000, &charlayout, 0, 16 },
	{ -1 } /* end of array */
};


static struct SN76496interface sn76496_interface =
{
	2,		/* 2 chips */
	{ 2000000, 4000000 },	/* 8 MHz / 4 ?*/
	{ 100, 100 }
};



static struct MachineDriver machine_driver_suprloco =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 MHz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,
			sound_readmem,sound_writemem,0,0,
			interrupt,4			/* NMIs are caused by the main CPU */
		},
	},
	60, 5000,           /* frames per second, vblank duration */
	1,					/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8,				/* screen_width, screen_height */
	{ 1*8, 31*8-1, 0*8, 28*8-1 },			/* struct rectangle visible_area */
	gfxdecodeinfo,				/* GfxDecodeInfo */
	768, 768,
	suprloco_vh_convert_color_prom,		/* convert color prom routine */

	VIDEO_TYPE_RASTER,
	0,							/* vh_init routine */
	suprloco_vh_start,			/* vh_start routine */
	0,			/* vh_stop routine */
	suprloco_vh_screenrefresh,	/* vh_update routine */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( suprloco )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "ic37.bin",     0x0000, 0x4000, 0x57f514dd )	/* encrypted */
	ROM_LOAD( "ic15.bin",     0x4000, 0x4000, 0x5a1d2fb0 )	/* encrypted */
	ROM_LOAD( "ic28.bin",     0x8000, 0x4000, 0xa597828a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "ic64.bin",     0x0000, 0x2000, 0x0aa57207 )

	ROM_REGION( 0xe000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic63.bin",     0x0000, 0x2000, 0xe571fe81 )
	ROM_LOAD( "ic62.bin",     0x2000, 0x2000, 0x6130f93c )
	ROM_LOAD( "ic61.bin",     0x4000, 0x2000, 0x3b03004e )
							/*0x6000- 0xe000 will be created by init_suprloco */

	ROM_REGION( 0x8000, REGION_GFX2 )	/* 32k for sprites data used at runtime */
	ROM_LOAD( "ic55.bin",     0x0000, 0x4000, 0xee2d3ed3 )
	ROM_LOAD( "ic56.bin",     0x4000, 0x2000, 0xf04a4b50 )
							/*0x6000 empty */

	ROM_REGION( 0x0620, REGION_PROMS )
	ROM_LOAD( "ic100.bin",    0x0100, 0x0080, 0x7b0c8ce5 )  /* color PROM */
	ROM_CONTINUE(             0x0000, 0x0080 )
	ROM_CONTINUE(             0x0180, 0x0080 )
	ROM_CONTINUE(             0x0080, 0x0080 )
	ROM_LOAD( "ic89.bin",     0x0200, 0x0400, 0x1d4b02cb )  /* 3bpp to 4bpp table */
	ROM_LOAD( "ic7.bin",      0x0600, 0x0020, 0x89ba674f )	/* unknown */
ROM_END



void init_suprloco(void)
{
	/* convert graphics to 4bpp from 3bpp */

	int i, j, k, color_source, color_dest;
	unsigned char *source, *dest, *lookup;

	source = memory_region(REGION_GFX1);
	dest   = source + 0x6000;
	lookup = memory_region(REGION_PROMS) + 0x0200;

	for (i = 0; i < 0x80; i++, lookup += 8)
	{
		for (j = 0; j < 0x40; j++, source++, dest++)
		{
			dest[0] = dest[0x2000] = dest[0x4000] = dest[0x6000] = 0;

			for (k = 0; k < 8; k++)
			{
				color_source = (((source[0x0000] >> k) & 0x01) << 2) |
							   (((source[0x2000] >> k) & 0x01) << 1) |
							   (((source[0x4000] >> k) & 0x01) << 0);

				color_dest = lookup[color_source];

				dest[0x0000] |= (((color_dest >> 3) & 0x01) << k);
				dest[0x2000] |= (((color_dest >> 2) & 0x01) << k);
				dest[0x4000] |= (((color_dest >> 1) & 0x01) << k);
				dest[0x6000] |= (((color_dest >> 0) & 0x01) << k);
			}
		}
	}


	/* decrypt program ROMs */
	suprloco_decode();
}



GAME( 1982, suprloco, 0, suprloco, suprloco, suprloco, ROT0, "Sega", "Super Locomotive" )
