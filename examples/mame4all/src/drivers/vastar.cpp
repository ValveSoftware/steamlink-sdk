#include "../vidhrdw/vastar.cpp"

/***************************************************************************

Vastar memory map (preliminary)

driver by Allard Van Der Bas

CPU #1:

0000-7fff ROM
8000-83ff bg #1 attribute RAM
8800-8bff bg #1 video RAM
8c00-8fff bg #1 color RAM
9000-93ff bg #2 attribute RAM
9800-9bff bg #2 video RAM
9c00-9fff bg #2 color RAM
a000-a3ff used only during startup - it's NOT a part of the RAM test
c400-c7ff fg color RAM
c800-cbff fg attribute RAM
cc00-cfff fg video RAM
f000-f7ff RAM (f000-f0ff is shared with CPU #2)

read:
e000      ???

write:
c410-c41f sprites
c430-c43f sprites
c7c0-c7df bg #2 scroll
c7e0-c7ff bg #1 scroll
c810-c81f sprites
c830-c83f sprites
cc10-cc1f sprites
cc30-cc3f sprites
e000      ???

I/O:
read:

write:
02        0 = hold CPU #2?

CPU #2:

0000-1fff ROM
4000-43ff RAM (shared with CPU #1)

read:
8000      IN1
8040      IN0
8080      IN2

write:

I/O:
read:
02        8910 read (port A = DSW0 port B = DSW1)

write:
00        8910 control
01        8910 write

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *vastar_bg1colorram2, *vastar_sprite_priority;
extern unsigned char *vastar_fgvideoram,*vastar_fgcolorram1,*vastar_fgcolorram2;
extern unsigned char *vastar_bg2videoram,*vastar_bg2colorram1,*vastar_bg2colorram2;
extern unsigned char *vastar_bg1scroll,*vastar_bg2scroll;

WRITE_HANDLER( vastar_bg1colorram2_w );
WRITE_HANDLER( vastar_bg2videoram_w );
WRITE_HANDLER( vastar_bg2colorram1_w );
WRITE_HANDLER( vastar_bg2colorram2_w );
void vastar_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int vastar_vh_start(void);
void vastar_vh_stop(void);
void vastar_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static unsigned char *vastar_sharedram;


void vastar_init_machine(void)
{
	/* we must start with the second CPU halted */
	cpu_set_reset_line(1,ASSERT_LINE);
}

WRITE_HANDLER( vastar_hold_cpu2_w )
{
	/* I'm not sure that this works exactly like this */
	if (data & 1)
		cpu_set_reset_line(1,CLEAR_LINE);
	else
		cpu_set_reset_line(1,ASSERT_LINE);
}

READ_HANDLER( vastar_sharedram_r )
{
	return vastar_sharedram[offset];
}

WRITE_HANDLER( vastar_sharedram_w )
{
	vastar_sharedram[offset] = data;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0x8800, 0x8fff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9800, 0x9fff, MRA_RAM },
	{ 0xc400, 0xcfff, MRA_RAM },
	{ 0xe000, 0xe000, MRA_RAM },	/* ?? */
	{ 0xf000, 0xf0ff, vastar_sharedram_r },
	{ 0xf100, 0xf7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x83ff, vastar_bg1colorram2_w, &vastar_bg1colorram2 },
	{ 0x8800, 0x8bff, videoram_w, &videoram, &videoram_size },
	{ 0x8c00, 0x8fff, colorram_w, &colorram },
	{ 0x9000, 0x93ff, vastar_bg2colorram2_w, &vastar_bg2colorram2 },
	{ 0x9800, 0x9bff, vastar_bg2videoram_w, &vastar_bg2videoram },
	{ 0x9c00, 0x9fff, vastar_bg2colorram1_w, &vastar_bg2colorram1 },
	{ 0xc000, 0xc000, MWA_RAM, &vastar_sprite_priority },	/* sprite/BG priority */
	{ 0xc400, 0xc7ff, MWA_RAM, &vastar_fgcolorram1 },
	{ 0xc800, 0xcbff, MWA_RAM, &vastar_fgcolorram2 },
	{ 0xcc00, 0xcfff, MWA_RAM, &vastar_fgvideoram },
	{ 0xe000, 0xe000, MWA_RAM },	/* ?? */
	{ 0xf000, 0xf0ff, vastar_sharedram_w, &vastar_sharedram },
	{ 0xf100, 0xf7ff, MWA_RAM },

	/* in hidden portions of video RAM: */
	{ 0xc400, 0xc43f, MWA_RAM, &spriteram, &spriteram_size },	/* actually c410-c41f and c430-c43f */
	{ 0xc7c0, 0xc7df, MWA_RAM, &vastar_bg2scroll },
	{ 0xc7e0, 0xc7ff, MWA_RAM, &vastar_bg1scroll },
	{ 0xc800, 0xc83f, MWA_RAM, &spriteram_2 },	/* actually c810-c81f and c830-c83f */
	{ 0xcc00, 0xcc3f, MWA_RAM, &spriteram_3 },	/* actually cc10-cc1f and cc30-cc3f */
	{ -1 }	/* end of table */
};


static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, interrupt_enable_w },
	{ 0x02, 0x02, vastar_hold_cpu2_w },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress cpu2_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x40ff, vastar_sharedram_r },
	{ 0x8000, 0x8000, input_port_1_r },
	{ 0x8040, 0x8040, input_port_0_r },
	{ 0x8080, 0x8080, input_port_2_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress cpu2_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x40ff, vastar_sharedram_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort cpu2_readport[] =
{
	{ 0x02, 0x02, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort cpu2_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( vastar )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Show Author Credits" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Slow Motion", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes are packed in one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout spritelayoutdw =
{
	16,32,	/* 16*32 sprites */
	128,	/* 128 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			96*8, 97*8, 98*8, 99*8, 100*8, 101*8, 102*8, 103*8 },
	128*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	8,8,	/* 8*8 tiles */
	512,	/* 512 tiles */
	2,	/* 4 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 64 },
	{ REGION_GFX2, 0, &spritelayout,   0, 64 },
	{ REGION_GFX2, 0, &spritelayoutdw, 0, 64 },
	{ REGION_GFX3, 0, &tilelayout,     0, 64 },
	{ REGION_GFX4, 0, &tilelayout,     0, 64 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz??????? */
	{ 50 },
	{ input_port_3_r },
	{ input_port_4_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_vastar =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ???? */
			readmem,writemem,0,writeport,
			nmi_interrupt,1
		},
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ???? */
			cpu2_readmem,cpu2_writemem,cpu2_readport,cpu2_writeport,
			interrupt,4	/* ??? */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,	/* 10 CPU slices per frame - seems enough to ensure proper */
			/* synchronization of the CPUs */
	vastar_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	vastar_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	vastar_vh_start,
	vastar_vh_stop,
	vastar_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
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

ROM_START( vastar )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "e_f4.rom",     0x0000, 0x1000, 0x45fa5075 )
	ROM_LOAD( "e_k4.rom",     0x1000, 0x1000, 0x84531982 )
	ROM_LOAD( "e_h4.rom",     0x2000, 0x1000, 0x94a4f778 )
	ROM_LOAD( "e_l4.rom",     0x3000, 0x1000, 0x40e4d57b )
	ROM_LOAD( "e_j4.rom",     0x4000, 0x1000, 0xbd607651 )
	ROM_LOAD( "e_n4.rom",     0x5000, 0x1000, 0x7a3779a4 )
	ROM_LOAD( "e_n7.rom",     0x6000, 0x1000, 0x31b6be39 )
	ROM_LOAD( "e_n5.rom",     0x7000, 0x1000, 0xf63f0e78 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "e_f2.rom",     0x0000, 0x1000, 0x713478d8 )
	ROM_LOAD( "e_j2.rom",     0x1000, 0x1000, 0xe4535442 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_c9.rom",     0x0000, 0x2000, 0x34f067b6 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_f7.rom",     0x0000, 0x2000, 0xedbf3b13 )
	ROM_LOAD( "c_f9.rom",     0x2000, 0x2000, 0x8f309e22 )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_n4.rom",     0x0000, 0x2000, 0xb5f9c866 )

	ROM_REGION( 0x2000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_s4.rom",     0x0000, 0x2000, 0xc9fbbfc9 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "tbp24s10.6p",  0x0000, 0x0100, 0xa712d73a )	/* red component */
	ROM_LOAD( "tbp24s10.6s",  0x0100, 0x0100, 0x0a7d48ec )	/* green component */
	ROM_LOAD( "tbp24s10.6m",  0x0200, 0x0100, 0x4c3db907 )	/* blue component */
	ROM_LOAD( "tbp24s10.8n",  0x0300, 0x0100, 0xb5297a3b )	/* ???? */
ROM_END

ROM_START( vastar2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "3.4f",         0x0000, 0x1000, 0x6741ff9c )
	ROM_LOAD( "6.4k",         0x1000, 0x1000, 0x5027619b )
	ROM_LOAD( "4.4h",         0x2000, 0x1000, 0xfdaa44e6 )
	ROM_LOAD( "7.4l",         0x3000, 0x1000, 0x29bef91c )
	ROM_LOAD( "5.4j",         0x4000, 0x1000, 0xc17c2458 )
	ROM_LOAD( "8.4n",         0x5000, 0x1000, 0x8ca25c37 )
	ROM_LOAD( "10.6n",        0x6000, 0x1000, 0x80df74ba )
	ROM_LOAD( "9.5n",         0x7000, 0x1000, 0x239ec84e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "e_f2.rom",     0x0000, 0x1000, 0x713478d8 )
	ROM_LOAD( "e_j2.rom",     0x1000, 0x1000, 0xe4535442 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_c9.rom",     0x0000, 0x2000, 0x34f067b6 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_f7.rom",     0x0000, 0x2000, 0xedbf3b13 )
	ROM_LOAD( "c_f9.rom",     0x2000, 0x2000, 0x8f309e22 )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_n4.rom",     0x0000, 0x2000, 0xb5f9c866 )

	ROM_REGION( 0x2000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c_s4.rom",     0x0000, 0x2000, 0xc9fbbfc9 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "tbp24s10.6p",  0x0000, 0x0100, 0xa712d73a )	/* red component */
	ROM_LOAD( "tbp24s10.6s",  0x0100, 0x0100, 0x0a7d48ec )	/* green component */
	ROM_LOAD( "tbp24s10.6m",  0x0200, 0x0100, 0x4c3db907 )	/* blue component */
	ROM_LOAD( "tbp24s10.8n",  0x0300, 0x0100, 0xb5297a3b )	/* ???? */
ROM_END



GAMEX( 1983, vastar,  0,      vastar, vastar, 0, ROT90, "Sesame Japan", "Vastar (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1983, vastar2, vastar, vastar, vastar, 0, ROT90, "Sesame Japan", "Vastar (set 2)", GAME_NO_COCKTAIL )
