#include "../vidhrdw/yiear.cpp"

/***************************************************************************

	Yie Ar Kung-Fu memory map (preliminary)
	enrique.sanchez@cs.us.es

CPU:    Motorola 6809

Normal 6809 IRQs must be generated each video frame (60 fps).
The 6809 NMI is used for sound timing.


0000	  	R	VLM5030 status ???
4000	 	 W  control port
					bit 0 - flip screen
					bit 1 - NMI enable
					bit 2 - IRQ enable
					bit 3 - coin counter A
					bit 4 - coin counter B
4800	 	 W	sound latch write
4900	 	 W  copy sound latch to SN76496
4a00	 	 W  VLM5030 write
4b00	 	 W  VLM5030 start
4c00		R   DSW #0
4d00		R   DSW #1
4e00		R   IN #0
4e01		R   IN #1
4e02		R   IN #2
4e03		R   DSW #2
4f00	 	 W  watchdog
5000-502f	 W  sprite RAM 1 (18 sprites)
					byte 0 - bit 0 - sprite code MSB
							 bit 6 - flip X
							 bit 7 - flip Y
					byte 1 - Y position
5030-53ff	RW  RAM
5400-542f    W  sprite RAM 2
					byte 0 - X position
					byte 1 - sprite code LSB
5430-57ff	RW  RAM
5800-5fff	RW  video RAM
					byte 0 - bit 4 - character code MSB
							 bit 6 - flip Y
							 bit 7 - flip X
					byte 1 - character code LSB
8000-ffff	R   ROM


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"


void yiear_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void yiear_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( yiear_videoram_w );
WRITE_HANDLER( yiear_control_w );
int  yiear_nmi_interrupt(void);

/* in sndhrdw/trackfld.c */
WRITE_HANDLER( konami_SN76496_latch_w );
WRITE_HANDLER( konami_SN76496_0_w );



static READ_HANDLER( yiear_speech_r )
{
	return rand();
	/* maybe bit 0 is VLM5030 busy pin??? */
	if (VLM5030_BSY()) return 1;
	else return 0;
}

static WRITE_HANDLER( yiear_speech_st_w )
{
	/* no idea if this is correct... */
	VLM5030_ST( 1 );
	VLM5030_ST( 0 );
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0000, yiear_speech_r },
	{ 0x4c00, 0x4c00, input_port_3_r },
	{ 0x4d00, 0x4d00, input_port_4_r },
	{ 0x4e00, 0x4e00, input_port_0_r },
	{ 0x4e01, 0x4e01, input_port_1_r },
	{ 0x4e02, 0x4e02, input_port_2_r },
	{ 0x4e03, 0x4e03, input_port_5_r },
	{ 0x5000, 0x5fff, MRA_RAM },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x4000, 0x4000, yiear_control_w },
	{ 0x4800, 0x4800, konami_SN76496_latch_w },
	{ 0x4900, 0x4900, konami_SN76496_0_w },
	{ 0x4a00, 0x4a00, VLM5030_data_w },
	{ 0x4b00, 0x4b00, yiear_speech_st_w },
	{ 0x4f00, 0x4f00, watchdog_reset_w },
	{ 0x5000, 0x502f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x5030, 0x53ff, MWA_RAM },
	{ 0x5400, 0x542f, MWA_RAM, &spriteram_2 },
	{ 0x5430, 0x57ff, MWA_RAM },
	{ 0x5800, 0x5fff, videoram_w, &videoram, &videoram_size },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};



INPUT_PORTS_START( yiear )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "30000 80000" )
	PORT_DIPSETTING(    0x00, "40000 90000" )
	PORT_DIPNAME( 0x10, 0x10, "Unknown DSW1 4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Difficulty?" )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x40, 0x40, "Unknown DSW1 6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Number of Controllers" )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, "Unknown DSW2 4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Unknown DSW2 5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown DSW2 6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown DSW2 7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown DSW2 8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
	/* 0x00 gives invalid */
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8x8 characters */
	512,	/* 512 characters */
	4,		/* 4 bits per pixel */
	{ 4, 0, 512*16*8+4, 512*16*8+0 },	/* plane offsets */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* each character takes 16 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16x16 sprites */
	512,	/* 512 sprites */
	4,		/* 4 bits per pixel */
	{ 4, 0, 512*64*8+4, 512*64*8+0 },	/* plane offsets */
	{ 0*8*8+0, 0*8*8+1, 0*8*8+2, 0*8*8+3, 1*8*8+0, 1*8*8+1, 1*8*8+2, 1*8*8+3,
	  2*8*8+0, 2*8*8+1, 2*8*8+2, 2*8*8+3, 3*8*8+0, 3*8*8+1, 3*8*8+2, 3*8*8+3 },
	{  0*8,  1*8,  2*8,  3*8,  4*8,  5*8,  6*8,  7*8,
	  32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8    /* each sprite takes 64 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   16, 1 },
	{ REGION_GFX2, 0, &spritelayout,  0, 1 },
	{ -1 } /* end of array */
};



struct SN76496interface sn76496_interface =
{
	1,			/* 1 chip */
	{ 1500000 },	/*  1.5 MHz ? (hand tuned) */
	{ 100 }
};

struct VLM5030interface vlm5030_interface =
{
	3580000,    /* master clock  */
	100,        /* volume        */
	REGION_SOUND1,	/* memory region  */
	0,          /* memory size of speech rom */
	0			/* VCU            */
};



static struct MachineDriver machine_driver_yiear =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1250000,	/* 1.25 Mhz */
			readmem, writemem, 0, 0,
			interrupt,1,	/* vblank */
			yiear_nmi_interrupt,500	/* music tempo (correct frequency unknown) */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32, 32,
	yiear_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	yiear_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		},
		{
			SOUND_VLM5030,
			&vlm5030_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( yiear )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "i08.10d",      0x08000, 0x4000, 0xe2d7458b )
	ROM_LOAD( "i07.8d",       0x0c000, 0x4000, 0x7db7442e )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g16_1.bin",    0x00000, 0x2000, 0xb68fd91d )
	ROM_LOAD( "g15_2.bin",    0x02000, 0x2000, 0xd9b167c6 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g04_5.bin",    0x00000, 0x4000, 0x45109b29 )
	ROM_LOAD( "g03_6.bin",    0x04000, 0x4000, 0x1d650790 )
	ROM_LOAD( "g06_3.bin",    0x08000, 0x4000, 0xe6aa945b )
	ROM_LOAD( "g05_4.bin",    0x0c000, 0x4000, 0xcc187c22 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "yiear.clr",    0x00000, 0x0020, 0xc283d71f )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* 8k for the VLM5030 data */
	ROM_LOAD( "a12_9.bin",    0x00000, 0x2000, 0xf75a1539 )
ROM_END

ROM_START( yiear2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "d12_8.bin",    0x08000, 0x4000, 0x49ecd9dd )
	ROM_LOAD( "d14_7.bin",    0x0c000, 0x4000, 0xbc2e1208 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g16_1.bin",    0x00000, 0x2000, 0xb68fd91d )
	ROM_LOAD( "g15_2.bin",    0x02000, 0x2000, 0xd9b167c6 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g04_5.bin",    0x00000, 0x4000, 0x45109b29 )
	ROM_LOAD( "g03_6.bin",    0x04000, 0x4000, 0x1d650790 )
	ROM_LOAD( "g06_3.bin",    0x08000, 0x4000, 0xe6aa945b )
	ROM_LOAD( "g05_4.bin",    0x0c000, 0x4000, 0xcc187c22 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "yiear.clr",    0x00000, 0x0020, 0xc283d71f )

	ROM_REGION( 0x2000, REGION_SOUND1 )	/* 8k for the VLM5030 data */
	ROM_LOAD( "a12_9.bin",    0x00000, 0x2000, 0xf75a1539 )
ROM_END



GAME( 1985, yiear,  0,     yiear, yiear, 0, ROT0, "Konami", "Yie Ar Kung-Fu (set 1)" )
GAME( 1985, yiear2, yiear, yiear, yiear, 0, ROT0, "Konami", "Yie Ar Kung-Fu (set 2)" )
