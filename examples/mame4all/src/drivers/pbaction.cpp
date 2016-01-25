#include "../vidhrdw/pbaction.cpp"

/***************************************************************************

Pinball Action memory map (preliminary)

driver by Nicola Salmoria


0000-9fff ROM
d000-d3ff Video RAM
d400-d7ff Color RAM
d800-dbff Background Video RAM
dc00-dfff Background Color RAM
e000-e07f Sprites
e400-e5ff Palette RAM

read:
e600      IN0
e601      IN1
e602      IN2
e604      DSW1
e605      DSW2
e606      watchdog reset????

write:
e600      interrupt enable
e604      flip screen
e606      bg scroll? not sure
e800      command for the sound CPU


Notes:
- pbactio2 has a ROM for a third Z80, not emulated, function unknown

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/z80fmly.h"


extern unsigned char *pbaction_videoram2,*pbaction_colorram2;
WRITE_HANDLER( pbaction_videoram2_w );
WRITE_HANDLER( pbaction_colorram2_w );
WRITE_HANDLER( pbaction_flipscreen_w );
WRITE_HANDLER( pbaction_scroll_w );
int pbaction_vh_start(void);
void pbaction_vh_stop(void);

void pbaction_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static WRITE_HANDLER( pbaction_sh_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,0x00);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x9fff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe07f, MRA_RAM },
	{ 0xe400, 0xe5ff, MRA_RAM },
	{ 0xe600, 0xe600, input_port_0_r },	/* IN0 */
	{ 0xe601, 0xe601, input_port_1_r },	/* IN1 */
	{ 0xe602, 0xe602, input_port_2_r },	/* IN2 */
	{ 0xe604, 0xe604, input_port_3_r },	/* DSW1 */
	{ 0xe605, 0xe605, input_port_4_r },	/* DSW2 */
	{ 0xe606, 0xe606, MRA_NOP },	/* ??? */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd3ff, videoram_w, &videoram, &videoram_size },
	{ 0xd400, 0xd7ff, colorram_w, &colorram },
	{ 0xd800, 0xdbff, pbaction_videoram2_w, &pbaction_videoram2 },
	{ 0xdc00, 0xdfff, pbaction_colorram2_w, &pbaction_colorram2 },
	{ 0xe000, 0xe07f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe400, 0xe5ff, paletteram_xxxxBBBBGGGGRRRR_w, &paletteram },
	{ 0xe600, 0xe600, interrupt_enable_w },
	{ 0xe604, 0xe604, pbaction_flipscreen_w },
	{ 0xe606, 0xe606, pbaction_scroll_w },
	{ 0xe800, 0xe800, pbaction_sh_command_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x8000, 0x8000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM },
	{ 0xffff, 0xffff, MWA_NOP },	/* watchdog? */
	{ -1 }	/* end of table */
};


static struct IOWritePort sound_writeport[] =
{
	{ 0x10, 0x10, AY8910_control_port_0_w },
	{ 0x11, 0x11, AY8910_write_port_0_w },
	{ 0x20, 0x20, AY8910_control_port_1_w },
	{ 0x21, 0x21, AY8910_write_port_1_w },
	{ 0x30, 0x30, AY8910_control_port_2_w },
	{ 0x31, 0x31, AY8910_write_port_2_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( pbaction )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_A ) )
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
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x01, "70K 200K 1000K" )
	PORT_DIPSETTING(    0x00, "70K 200K" )
	PORT_DIPSETTING(    0x04, "100K 300K 1000K" )
	PORT_DIPSETTING(    0x03, "100K 300K" )
	PORT_DIPSETTING(    0x02, "100K" )
	PORT_DIPSETTING(    0x06, "200K 1000K" )
	PORT_DIPSETTING(    0x05, "200K" )
	PORT_DIPSETTING(    0x07, "None" )
	PORT_DIPNAME( 0x08, 0x00, "Extra" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPNAME( 0x30, 0x00, "Difficulty (Flippers)" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x10, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x30, "Hardest" )
	PORT_DIPNAME( 0xc0, 0x00, "Difficulty (Outlanes)" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END



static struct GfxLayout charlayout1 =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	3,	/* 3 bits per pixel */
	{ 0, 1024*8*8, 2*1024*8*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout charlayout2 =
{
	8,8,	/* 8*8 characters */
	2048,	/* 2048 characters */
	4,	/* 4 bits per pixel */
	{ 0, 2048*8*8, 2*2048*8*8, 3*2048*8*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout1 =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 0, 256*16*16, 2*256*16*16 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,	/* pretty straightforward layout */
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};
static struct GfxLayout spritelayout2 =
{
	32,32,	/* 32*32 sprites */
	32,	/* 32 sprites */
	3,	/* 3 bits per pixel */
	{ 0*64*32*32, 1*64*32*32, 2*64*32*32 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,	/* pretty straightforward layout */
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
	{ REGION_GFX1, 0x00000, &charlayout1,    0, 16 },	/*   0-127 characters */
	{ REGION_GFX2, 0x00000, &charlayout2,  128,  8 },	/* 128-255 background */
	{ REGION_GFX3, 0x00000, &spritelayout1,  0, 16 },	/*   0-127 normal sprites */
	{ REGION_GFX3, 0x01000, &spritelayout2,  0, 16 },	/*   0-127 large sprites */
	{ -1 } /* end of array */
};


static struct AY8910interface ay8910_interface =
{
	3,	/* 3 chips */
	1500000,	/* 1.5 MHz?????? */
	{ 25, 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


int pbaction_interrupt(void)
{
	return  0x02;	/* the CPU is in Interrupt Mode 2 */
}


static struct MachineDriver machine_driver_pbaction =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz? */
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3072000,	/* 3.072 Mhz (?????) */
			sound_readmem,sound_writemem,0,sound_writeport,
			pbaction_interrupt,2	/* ??? */
									/* IRQs are caused by the main CPU */
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	pbaction_vh_start,
	pbaction_vh_stop,
	pbaction_vh_screenrefresh,

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

ROM_START( pbaction )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "b-p7.bin",     0x0000, 0x4000, 0x8d6dcaae )
	ROM_LOAD( "b-n7.bin",     0x4000, 0x4000, 0xd54d5402 )
	ROM_LOAD( "b-l7.bin",     0x8000, 0x2000, 0xe7412d68 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound board */
	ROM_LOAD( "a-e3.bin",     0x0000,  0x2000, 0x0e53a91f )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a-s6.bin",     0x00000, 0x2000, 0x9a74a8e1 )
	ROM_LOAD( "a-s7.bin",     0x02000, 0x2000, 0x5ca6ad3c )
	ROM_LOAD( "a-s8.bin",     0x04000, 0x2000, 0x9f00b757 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a-j5.bin",     0x00000, 0x4000, 0x21efe866 )
	ROM_LOAD( "a-j6.bin",     0x04000, 0x4000, 0x7f984c80 )
	ROM_LOAD( "a-j7.bin",     0x08000, 0x4000, 0xdf69e51b )
	ROM_LOAD( "a-j8.bin",     0x0c000, 0x4000, 0x0094cb8b )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-c7.bin",     0x00000, 0x2000, 0xd1795ef5 )
	ROM_LOAD( "b-d7.bin",     0x02000, 0x2000, 0xf28df203 )
	ROM_LOAD( "b-f7.bin",     0x04000, 0x2000, 0xaf6e9817 )
ROM_END


ROM_START( pbactio2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "pba16.bin",     0x0000, 0x4000, 0x4a239ebd )
	ROM_LOAD( "pba15.bin",     0x4000, 0x4000, 0x3afef03a )
	ROM_LOAD( "pba14.bin",     0x8000, 0x2000, 0xc0a98c8a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound board */
	ROM_LOAD( "pba1.bin",     0x0000,  0x2000, 0x8b69b933 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for a third Z80 (not emulated) */
	ROM_LOAD( "pba17.bin",    0x0000,  0x4000, 0x2734ae60 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a-s6.bin",     0x00000, 0x2000, 0x9a74a8e1 )
	ROM_LOAD( "a-s7.bin",     0x02000, 0x2000, 0x5ca6ad3c )
	ROM_LOAD( "a-s8.bin",     0x04000, 0x2000, 0x9f00b757 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a-j5.bin",     0x00000, 0x4000, 0x21efe866 )
	ROM_LOAD( "a-j6.bin",     0x04000, 0x4000, 0x7f984c80 )
	ROM_LOAD( "a-j7.bin",     0x08000, 0x4000, 0xdf69e51b )
	ROM_LOAD( "a-j8.bin",     0x0c000, 0x4000, 0x0094cb8b )

	ROM_REGION( 0x06000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-c7.bin",     0x00000, 0x2000, 0xd1795ef5 )
	ROM_LOAD( "b-d7.bin",     0x02000, 0x2000, 0xf28df203 )
	ROM_LOAD( "b-f7.bin",     0x04000, 0x2000, 0xaf6e9817 )
ROM_END



GAME( 1985, pbaction, 0,        pbaction, pbaction, 0, ROT90, "Tehkan", "Pinball Action (set 1)" )
GAME( 1985, pbactio2, pbaction, pbaction, pbaction, 0, ROT90, "Tehkan", "Pinball Action (set 2)" )
