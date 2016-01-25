#include "../vidhrdw/tagteam.cpp"

/***************************************************************************

Tag Team Wrestling hardware description:

This hardware is very similar to the BurgerTime/Lock N Chase family of games
but there are just enough differences to make it a pain to share the
codebase. It looks like this hardware is a bridge between the BurgerTime
family and the later Technos games, like Mat Mania and Mysterious Stones.

The video hardware supports 3 sprite banks instead of 1
The sound hardware appears nearly identical to Mat Mania

TODO:
        * fix hi-score (reset) bug

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"

void tagteam_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

READ_HANDLER( tagteam_mirrorvideoram_r );
WRITE_HANDLER( tagteam_mirrorvideoram_w );
READ_HANDLER( tagteam_mirrorcolorram_r );
WRITE_HANDLER( tagteam_mirrorcolorram_w );
WRITE_HANDLER( tagteam_video_control_w );
WRITE_HANDLER( tagteam_control_w );

int  tagteam_vh_start (void);
void tagteam_vh_stop (void);
void tagteam_vh_screenrefresh (struct osd_bitmap *bitmap, int full_refresh);

static WRITE_HANDLER( sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,M6502_INT_IRQ);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x2000, 0x2000, input_port_1_r },     /* IN1 */
	{ 0x2001, 0x2001, input_port_0_r },     /* IN0 */
	{ 0x2002, 0x2002, input_port_2_r },     /* DSW2 */
	{ 0x2003, 0x2003, input_port_3_r },     /* DSW1 */
	{ 0x4000, 0x43ff, tagteam_mirrorvideoram_r },
	{ 0x4400, 0x47ff, tagteam_mirrorcolorram_r },
	{ 0x4800, 0x4fff, MRA_RAM },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
//	{ 0x2000, 0x2000, tagteam_unk_w },
	{ 0x2001, 0x2001, tagteam_control_w },
	{ 0x2002, 0x2002, sound_command_w },
//	{ 0x2003, 0x2003, MWA_NOP }, /* Appears to increment when you're out of the ring */
	{ 0x4000, 0x43ff, tagteam_mirrorvideoram_w },
	{ 0x4400, 0x47ff, tagteam_mirrorcolorram_w },
	{ 0x4800, 0x4bff, videoram_w, &videoram, &videoram_size },
	{ 0x4c00, 0x4fff, colorram_w, &colorram },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x2007, 0x2007, soundlatch_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x2000, 0x2000, AY8910_write_port_0_w },
	{ 0x2001, 0x2001, AY8910_control_port_0_w },
	{ 0x2002, 0x2002, AY8910_write_port_1_w },
	{ 0x2003, 0x2003, AY8910_control_port_1_w },
	{ 0x2004, 0x2004, DAC_0_data_w },
	{ 0x2005, 0x2005, interrupt_enable_w },
	{ -1 }  /* end of table */
};



static int tagteam_interrupt(void)
{
	static int coin;
	int port;

	port = readinputport(0) & 0xc0;

	if (port != 0xc0)    /* Coin */
	{
		if (coin == 0)
		{
			coin = 1;
			return nmi_interrupt();
		}
	}
	else coin = 0;

        return ignore_interrupt();
}

INPUT_PORTS_START( tagteam )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_START2 )

	PORT_START      /* DSW1 - 7 not used?, 8 = VBLANK! */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, "A 2C/1C B 1C/1C" )
	PORT_DIPSETTING(    0x08, "A 2C/1C B 1C/2C" )
	PORT_DIPSETTING(    0x04, "A 2C/1C B 1C/3C" )
	PORT_DIPSETTING(    0x03, "A 1C/1C B 2C/1C" )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0b, "A 1C/1C B 1C/2C" )
	PORT_DIPSETTING(    0x07, "A 1C/1C B 1C/3C" )
	PORT_DIPSETTING(    0x02, "A 1C/2C B 2C/1C" )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0e, "A 1C/2C B 1C/1C" )
	PORT_DIPSETTING(    0x06, "A 1C/2C B 1C/3C" )
	PORT_DIPSETTING(    0x01, "A 1C/3C B 2C/1C" )
	PORT_DIPSETTING(    0x0d, "A 1C/3C B 1C/1C" )
	PORT_DIPSETTING(    0x09, "A 1C/3C B 1C/2C" )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x20, 0x00, "Control Panel" )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 - 3,4,5,6,7,8 = not used? */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
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
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	3072,   /* 3072 characters */
	3,      /* 3 bits per pixel */
	{ 2*3072*8*8, 3072*8*8, 0 },    /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};


static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	768,    /* 768 sprites */
	3,      /* 3 bits per pixel */
	{ 2*768*16*16, 768*16*16, 0 },  /* the bitplanes are separated */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo tagteam_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 4 }, /* chars */
	{ REGION_GFX1, 0, &spritelayout, 0, 4 }, /* sprites */
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,      /* 2 chips */
	1500000,        /* 1.5 MHz ?? */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct DACinterface dac_interface =
{
	1,
	{ 255 }
};

static struct MachineDriver machine_driver_tagteam =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz ?? */
			readmem,writemem,0,0,
			tagteam_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			975000,  /* 975 kHz ?? */
			sound_readmem,sound_writemem,0,0,
			nmi_interrupt,16   /* IRQs are triggered by the main CPU */
		}
	},
	57, 3072,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },
	tagteam_gfxdecodeinfo,
	32, 32,
	tagteam_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	tagteam_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};



ROM_START( bigprowr )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "bf00-1.20",    0x08000, 0x2000, 0x8aba32c9 )
	ROM_LOAD( "bf01.33",      0x0a000, 0x2000, 0x0a41f3ae )
	ROM_LOAD( "bf02.34",      0x0c000, 0x2000, 0xa28b0a0e )
	ROM_LOAD( "bf03.46",      0x0e000, 0x2000, 0xd4cf7ec7 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for audio code */
	ROM_LOAD( "bf4.8",        0x04000, 0x2000, 0x0558e1d8 )
	ROM_LOAD( "bf5.7",        0x06000, 0x2000, 0xc1073f24 )
	ROM_LOAD( "bf6.6",        0x08000, 0x2000, 0x208cd081 )
	ROM_LOAD( "bf7.3",        0x0a000, 0x2000, 0x34a033dc )
	ROM_LOAD( "bf8.2",        0x0c000, 0x2000, 0xeafe8056 )
	ROM_LOAD( "bf9.1",        0x0e000, 0x2000, 0xd589ce1b )

	ROM_REGION( 0x12000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bf10.89",      0x00000, 0x2000, 0xb1868746 )
	ROM_LOAD( "bf11.94",      0x02000, 0x2000, 0xc3fe99c1 )
	ROM_LOAD( "bf12.103",     0x04000, 0x2000, 0xc8717a46 )
	ROM_LOAD( "bf13.91",      0x06000, 0x2000, 0x23ee34d3 )
	ROM_LOAD( "bf14.95",      0x08000, 0x2000, 0xa6721142 )
	ROM_LOAD( "bf15.105",     0x0a000, 0x2000, 0x60ae1078 )
	ROM_LOAD( "bf16.93",      0x0c000, 0x2000, 0xd33dc245 )
	ROM_LOAD( "bf17.96",      0x0e000, 0x2000, 0xccf42380 )
	ROM_LOAD( "bf18.107",     0x10000, 0x2000, 0xfd6f006d )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "fko.8",        0x0000, 0x0020, 0xb6ee1483 )
	ROM_LOAD( "fjo.25",       0x0020, 0x0020, 0x24da2b63 ) /* What is this prom for? */
ROM_END

ROM_START( tagteam )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "prowbf0.bin",  0x08000, 0x2000, 0x6ec3afae )
	ROM_LOAD( "prowbf1.bin",  0x0a000, 0x2000, 0xb8fdd176 )
	ROM_LOAD( "prowbf2.bin",  0x0c000, 0x2000, 0x3d33a923 )
	ROM_LOAD( "prowbf3.bin",  0x0e000, 0x2000, 0x518475d2 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for audio code */
	ROM_LOAD( "bf4.8",        0x04000, 0x2000, 0x0558e1d8 )
	ROM_LOAD( "bf5.7",        0x06000, 0x2000, 0xc1073f24 )
	ROM_LOAD( "bf6.6",        0x08000, 0x2000, 0x208cd081 )
	ROM_LOAD( "bf7.3",        0x0a000, 0x2000, 0x34a033dc )
	ROM_LOAD( "bf8.2",        0x0c000, 0x2000, 0xeafe8056 )
	ROM_LOAD( "bf9.1",        0x0e000, 0x2000, 0xd589ce1b )

	ROM_REGION( 0x12000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "prowbf10.bin", 0x00000, 0x2000, 0x48165902 )
	ROM_LOAD( "bf11.94",      0x02000, 0x2000, 0xc3fe99c1 )
	ROM_LOAD( "prowbf12.bin", 0x04000, 0x2000, 0x69de1ea2 )
	ROM_LOAD( "prowbf13.bin", 0x06000, 0x2000, 0xecfa581d )
	ROM_LOAD( "bf14.95",      0x08000, 0x2000, 0xa6721142 )
	ROM_LOAD( "prowbf15.bin", 0x0a000, 0x2000, 0xd0de7e03 )
	ROM_LOAD( "prowbf16.bin", 0x0c000, 0x2000, 0x75ee5705 )
	ROM_LOAD( "bf17.96",      0x0e000, 0x2000, 0xccf42380 )
	ROM_LOAD( "prowbf18.bin", 0x10000, 0x2000, 0xe73a4bba )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "fko.8",        0x0000, 0x0020, 0xb6ee1483 )
	ROM_LOAD( "fjo.25",       0x0020, 0x0020, 0x24da2b63 ) /* What is this prom for? */
ROM_END



GAME( 1983, bigprowr, 0,        tagteam, tagteam, 0, ROT270, "Technos", "The Big Pro Wrestling!" )
GAME( 1983, tagteam,  bigprowr, tagteam, tagteam, 0, ROT270, "Technos (Data East license)", "Tag Team Wrestling" )
