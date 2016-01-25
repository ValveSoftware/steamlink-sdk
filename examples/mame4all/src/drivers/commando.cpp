#include "../vidhrdw/commando.cpp"

/***************************************************************************

Commando memory map (preliminary)

driver by Nicola Salmoria


MAIN CPU
0000-bfff ROM
d000-d3ff Video RAM
d400-d7ff Color RAM
d800-dbff background video RAM
dc00-dfff background color RAM
e000-ffff RAM
fe00-ff7f Sprites

read:
c000      IN0
c001      IN1
c002      IN2
c003      DSW1
c004      DSW2

write:
c808-c809 background scroll x position
c80a-c80b background scroll y position

SOUND CPU
0000-3fff ROM
4000-47ff RAM

write:
8000      YM2203 #1 control
8001      YM2203 #1 write
8002      YM2203 #2 control
8003      YM2203 #2 write

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *commando_fgvideoram;
extern unsigned char *commando_bgvideoram;

WRITE_HANDLER( commando_fgvideoram_w );
WRITE_HANDLER( commando_bgvideoram_w );
WRITE_HANDLER( commando_scrollx_w );
WRITE_HANDLER( commando_scrolly_w );
WRITE_HANDLER( commando_c804_w );
int commando_vh_start(void);
void commando_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void commando_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void commando_eof_callback(void);



static int commando_interrupt(void)
{
	return 0x00d7;	/* RST 10h - VBLANK */
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc000, input_port_0_r },
	{ 0xc001, 0xc001, input_port_1_r },
	{ 0xc002, 0xc002, input_port_2_r },
	{ 0xc003, 0xc003, input_port_3_r },
	{ 0xc004, 0xc004, input_port_4_r },
	{ 0xd000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc800, 0xc800, soundlatch_w },
	{ 0xc804, 0xc804, commando_c804_w },
	{ 0xc808, 0xc809, commando_scrollx_w },
	{ 0xc80a, 0xc80b, commando_scrolly_w },
	{ 0xd000, 0xd7ff, commando_fgvideoram_w, &commando_fgvideoram },
	{ 0xd800, 0xdfff, commando_bgvideoram_w, &commando_bgvideoram },
	{ 0xe000, 0xfdff, MWA_RAM },
	{ 0xfe00, 0xff7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xff80, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM },
	{ 0x8000, 0x8000, YM2203_control_port_0_w },
	{ 0x8001, 0x8001, YM2203_write_port_0_w },
	{ 0x8002, 0x8002, YM2203_control_port_1_w },
	{ 0x8003, 0x8003, YM2203_write_port_1_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( commando )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, "Starting Stage" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "40000 50000" )
	PORT_DIPSETTING(    0x07, "10000 500000" )
	PORT_DIPSETTING(    0x03, "10000 600000" )
	PORT_DIPSETTING(    0x05, "20000 600000" )
	PORT_DIPSETTING(    0x01, "20000 700000" )
	PORT_DIPSETTING(    0x06, "30000 700000" )
	PORT_DIPSETTING(    0x02, "30000 800000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright One Player" )
	PORT_DIPSETTING(    0x40, "Upright Two Players" )
/*	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) ) */
	PORT_DIPSETTING(    0xc0, DEF_STR( Cocktail ) )
INPUT_PORTS_END

/* service mode replaces demo sounds, different bonus */
INPUT_PORTS_START( commandu )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, "Starting Stage" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x07, "10000 50000" )
	PORT_DIPSETTING(    0x03, "10000 60000" )
	PORT_DIPSETTING(    0x05, "20000 60000" )
	PORT_DIPSETTING(    0x01, "20000 70000" )
	PORT_DIPSETTING(    0x06, "30000 70000" )
	PORT_DIPSETTING(    0x02, "30000 80000" )
	PORT_DIPSETTING(    0x04, "40000 100000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_SERVICE( 0x08, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright One Player" )
	PORT_DIPSETTING(    0x40, "Upright Two Players" )
/*	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) ) */
	PORT_DIPSETTING(    0xc0, DEF_STR( Cocktail ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	2,
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8
};
static struct GfxLayout tilelayout =
{
	16,16,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(0,3), RGN_FRAC(1,3), RGN_FRAC(2,3) },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8
};
static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+4, RGN_FRAC(1,2)+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   192, 16 },	/* colors 192-255 */
	{ REGION_GFX2, 0, &tilelayout,     0, 16 },	/* colors   0-127 */
	{ REGION_GFX3, 0, &spritelayout, 128,  4 },	/* colors 128-191 */
	{ -1 } /* end of array */
};



static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	1500000,	/* 1.5 MHz ? */
	{ YM2203_VOL(15,15), YM2203_VOL(15,15) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_commando =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 MHz (?) */
			readmem,writemem,0,0,
			commando_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 3 MHz (?) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,16*4+4*16+16*8,
	commando_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM,
	commando_eof_callback,
	commando_vh_start,
	0,
	commando_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( commando )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "m09_cm04.bin", 0x0000, 0x8000, 0x8438b694 )
	ROM_LOAD( "m08_cm03.bin", 0x8000, 0x4000, 0x35486542 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "f09_cm02.bin", 0x0000, 0x4000, 0xf9cc4a74 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "d05_vt01.bin", 0x00000, 0x4000, 0x505726e0 )	/* characters */

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a05_vt11.bin", 0x00000, 0x4000, 0x7b2e1b48 )	/* tiles */
	ROM_LOAD( "a06_vt12.bin", 0x04000, 0x4000, 0x81b417d3 )
	ROM_LOAD( "a07_vt13.bin", 0x08000, 0x4000, 0x5612dbd2 )
	ROM_LOAD( "a08_vt14.bin", 0x0c000, 0x4000, 0x2b2dee36 )
	ROM_LOAD( "a09_vt15.bin", 0x10000, 0x4000, 0xde70babf )
	ROM_LOAD( "a10_vt16.bin", 0x14000, 0x4000, 0x14178237 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "e07_vt05.bin", 0x00000, 0x4000, 0x79f16e3d )	/* sprites */
	ROM_LOAD( "e08_vt06.bin", 0x04000, 0x4000, 0x26fee521 )
	ROM_LOAD( "e09_vt07.bin", 0x08000, 0x4000, 0xca88bdfd )
	ROM_LOAD( "h07_vt08.bin", 0x0c000, 0x4000, 0x2019c883 )
	ROM_LOAD( "h08_vt09.bin", 0x10000, 0x4000, 0x98703982 )
	ROM_LOAD( "h09_vt10.bin", 0x14000, 0x4000, 0xf069d2f8 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "01d_vtb1.bin", 0x0000, 0x0100, 0x3aba15a1 )	/* red */
	ROM_LOAD( "02d_vtb2.bin", 0x0100, 0x0100, 0x88865754 )	/* green */
	ROM_LOAD( "03d_vtb3.bin", 0x0200, 0x0100, 0x4c14c3f6 )	/* blue */
	ROM_LOAD( "01h_vtb4.bin", 0x0300, 0x0100, 0xb388c246 )	/* palette selector (not used) */
	ROM_LOAD( "06l_vtb5.bin", 0x0400, 0x0100, 0x712ac508 )	/* interrupt timing (not used) */
	ROM_LOAD( "06e_vtb6.bin", 0x0500, 0x0100, 0x0eaf5158 )	/* video timing (not used) */
ROM_END

ROM_START( commandu )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "u4-f",         0x0000, 0x8000, 0xa6118935 )
	ROM_LOAD( "u3-f",         0x8000, 0x4000, 0x24f49684 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "f09_cm02.bin", 0x0000, 0x4000, 0xf9cc4a74 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "d05_vt01.bin", 0x00000, 0x4000, 0x505726e0 )	/* characters */

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a05_vt11.bin", 0x00000, 0x4000, 0x7b2e1b48 )	/* tiles */
	ROM_LOAD( "a06_vt12.bin", 0x04000, 0x4000, 0x81b417d3 )
	ROM_LOAD( "a07_vt13.bin", 0x08000, 0x4000, 0x5612dbd2 )
	ROM_LOAD( "a08_vt14.bin", 0x0c000, 0x4000, 0x2b2dee36 )
	ROM_LOAD( "a09_vt15.bin", 0x10000, 0x4000, 0xde70babf )
	ROM_LOAD( "a10_vt16.bin", 0x14000, 0x4000, 0x14178237 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "e07_vt05.bin", 0x00000, 0x4000, 0x79f16e3d )	/* sprites */
	ROM_LOAD( "e08_vt06.bin", 0x04000, 0x4000, 0x26fee521 )
	ROM_LOAD( "e09_vt07.bin", 0x08000, 0x4000, 0xca88bdfd )
	ROM_LOAD( "h07_vt08.bin", 0x0c000, 0x4000, 0x2019c883 )
	ROM_LOAD( "h08_vt09.bin", 0x10000, 0x4000, 0x98703982 )
	ROM_LOAD( "h09_vt10.bin", 0x14000, 0x4000, 0xf069d2f8 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "01d_vtb1.bin", 0x0000, 0x0100, 0x3aba15a1 )	/* red */
	ROM_LOAD( "02d_vtb2.bin", 0x0100, 0x0100, 0x88865754 )	/* green */
	ROM_LOAD( "03d_vtb3.bin", 0x0200, 0x0100, 0x4c14c3f6 )	/* blue */
	ROM_LOAD( "01h_vtb4.bin", 0x0300, 0x0100, 0xb388c246 )	/* palette selector (not used) */
	ROM_LOAD( "06l_vtb5.bin", 0x0400, 0x0100, 0x712ac508 )	/* interrupt timing (not used) */
	ROM_LOAD( "06e_vtb6.bin", 0x0500, 0x0100, 0x0eaf5158 )	/* video timing (not used) */
ROM_END

ROM_START( commandj )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "09m_so04.bin", 0x0000, 0x8000, 0xd3f2bfb3 )
	ROM_LOAD( "08m_so03.bin", 0x8000, 0x4000, 0xed01f472 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "09f_so02.bin", 0x0000, 0x4000, 0xca20aca5 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "d05_vt01.bin", 0x00000, 0x4000, 0x505726e0 )	/* characters */

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a05_vt11.bin", 0x00000, 0x4000, 0x7b2e1b48 )	/* tiles */
	ROM_LOAD( "a06_vt12.bin", 0x04000, 0x4000, 0x81b417d3 )
	ROM_LOAD( "a07_vt13.bin", 0x08000, 0x4000, 0x5612dbd2 )
	ROM_LOAD( "a08_vt14.bin", 0x0c000, 0x4000, 0x2b2dee36 )
	ROM_LOAD( "a09_vt15.bin", 0x10000, 0x4000, 0xde70babf )
	ROM_LOAD( "a10_vt16.bin", 0x14000, 0x4000, 0x14178237 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "e07_vt05.bin", 0x00000, 0x4000, 0x79f16e3d )	/* sprites */
	ROM_LOAD( "e08_vt06.bin", 0x04000, 0x4000, 0x26fee521 )
	ROM_LOAD( "e09_vt07.bin", 0x08000, 0x4000, 0xca88bdfd )
	ROM_LOAD( "h07_vt08.bin", 0x0c000, 0x4000, 0x2019c883 )
	ROM_LOAD( "h08_vt09.bin", 0x10000, 0x4000, 0x98703982 )
	ROM_LOAD( "h09_vt10.bin", 0x14000, 0x4000, 0xf069d2f8 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "01d_vtb1.bin", 0x0000, 0x0100, 0x3aba15a1 )	/* red */
	ROM_LOAD( "02d_vtb2.bin", 0x0100, 0x0100, 0x88865754 )	/* green */
	ROM_LOAD( "03d_vtb3.bin", 0x0200, 0x0100, 0x4c14c3f6 )	/* blue */
	ROM_LOAD( "01h_vtb4.bin", 0x0300, 0x0100, 0xb388c246 )	/* palette selector (not used) */
	ROM_LOAD( "06l_vtb5.bin", 0x0400, 0x0100, 0x712ac508 )	/* interrupt timing (not used) */
	ROM_LOAD( "06e_vtb6.bin", 0x0500, 0x0100, 0x0eaf5158 )	/* video timing (not used) */
ROM_END

ROM_START( spaceinv )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "u4",           0x0000, 0x8000, 0x834ba0de )
	ROM_LOAD( "u3",           0x8000, 0x4000, 0x07e4ee3a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "u2",           0x0000, 0x4000, 0xcbf8c40e )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "u1",           0x00000, 0x4000, 0xf477e13a )	/* characters */

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a05_vt11.bin", 0x00000, 0x4000, 0x7b2e1b48 )	/* tiles */
	ROM_LOAD( "a06_vt12.bin", 0x04000, 0x4000, 0x81b417d3 )
	ROM_LOAD( "a07_vt13.bin", 0x08000, 0x4000, 0x5612dbd2 )
	ROM_LOAD( "a08_vt14.bin", 0x0c000, 0x4000, 0x2b2dee36 )
	ROM_LOAD( "a09_vt15.bin", 0x10000, 0x4000, 0xde70babf )
	ROM_LOAD( "a10_vt16.bin", 0x14000, 0x4000, 0x14178237 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "u5",           0x00000, 0x4000, 0x2a97c933 )	/* sprites */
	ROM_LOAD( "e08_vt06.bin", 0x04000, 0x4000, 0x26fee521 )
	ROM_LOAD( "e09_vt07.bin", 0x08000, 0x4000, 0xca88bdfd )
	ROM_LOAD( "u8",           0x0c000, 0x4000, 0xd6b4aa2e )
	ROM_LOAD( "h08_vt09.bin", 0x10000, 0x4000, 0x98703982 )
	ROM_LOAD( "h09_vt10.bin", 0x14000, 0x4000, 0xf069d2f8 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "01d_vtb1.bin", 0x0000, 0x0100, 0x3aba15a1 )	/* red */
	ROM_LOAD( "02d_vtb2.bin", 0x0100, 0x0100, 0x88865754 )	/* green */
	ROM_LOAD( "03d_vtb3.bin", 0x0200, 0x0100, 0x4c14c3f6 )	/* blue */
	ROM_LOAD( "01h_vtb4.bin", 0x0300, 0x0100, 0xb388c246 )	/* palette selector (not used) */
	ROM_LOAD( "06l_vtb5.bin", 0x0400, 0x0100, 0x712ac508 )	/* interrupt timing (not used) */
	ROM_LOAD( "06e_vtb6.bin", 0x0500, 0x0100, 0x0eaf5158 )	/* video timing (not used) */
ROM_END



static void init_commando(void)
{
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	memory_set_opcode_base(0,rom+diff);

	/* the first opcode is not encrypted */
	rom[0+diff] = rom[0];
	for (A = 1;A < 0xc000;A++)
	{
		int src;

		src = rom[A];
		rom[A+diff] = src ^ (src & 0xee) ^ ((src & 0xe0) >> 4) ^ ((src & 0x0e) << 4);
	}
}

static void init_spaceinv(void)
{
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	memory_set_opcode_base(0,rom+diff);

	/* the first opcode *is* encrypted */
	for (A = 0;A < 0xc000;A++)
	{
		int src;

		src = rom[A];
		rom[A+diff] = src ^ (src & 0xee) ^ ((src & 0xe0) >> 4) ^ ((src & 0x0e) << 4);
	}
}



GAME( 1985, commando, 0,        commando, commando, commando, ROT90, "Capcom", "Commando (World)" )
GAME( 1985, commandu, commando, commando, commandu, commando, ROT90, "Capcom (Data East USA license)", "Commando (US)" )
GAME( 1985, commandj, commando, commando, commando, commando, ROT90, "Capcom", "Senjo no Ookami" )
GAME( 1985, spaceinv, commando, commando, commando, spaceinv, ROT90, "bootleg", "Space Invasion" )
