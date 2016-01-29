#include "../machine/arkanoid.cpp"
#include "../vidhrdw/arkanoid.cpp"

/***************************************************************************

	Arkanoid driver (Preliminary)


	Japanese version support cocktail mode (DSW #7), the others don't.

	Here are the versions we have:

	arkanoid	World version, probably an earlier revision
	arknoidu	USA version, probably a later revision; There has been code
			    inserted, NOT patched, so I don't think it's a bootleg
				The 68705 code for this one was not available; I made it up from
				the World version changing the level data pointer table.
    arkatour    Tournament version
				The 68705 code for this one was not available; I made it up from
				the World version changing the level data pointer table.
	arknoidj	Japanese version with level selector.
				The 68705 code for this one was not available; I made it up from
				the World version changing the level data pointer table.
	arkbl2		Bootleg of the early Japanese version.
				The only difference is that the warning text has been replaced
				by "WAIT"
				ROM	E2.6F should be identical to the real Japanese one.
				(It only differs in the country byte from A75_11.ROM)
				This version works fine with the real MCU ROM
	arkatayt	Another bootleg of the early Japanese one, more heavily modified
	arkblock	Another bootleg of the early Japanese one, more heavily modified
	arkbloc2	Another bootleg
	arkbl3   	Another bootleg of the early Japanese one, more heavily modified
	arkangc		Game Corporation bootleg with level selector

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



void arkanoid_init_machine(void);

WRITE_HANDLER( arkanoid_d008_w );
void arkanoid_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void arkanoid_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

READ_HANDLER( arkanoid_Z80_mcu_r );
WRITE_HANDLER( arkanoid_Z80_mcu_w );

READ_HANDLER( arkanoid_68705_portA_r );
WRITE_HANDLER( arkanoid_68705_portA_w );
WRITE_HANDLER( arkanoid_68705_ddrA_w );

READ_HANDLER( arkanoid_68705_portC_r );
WRITE_HANDLER( arkanoid_68705_portC_w );
WRITE_HANDLER( arkanoid_68705_ddrC_w );

READ_HANDLER( arkanoid_68705_input_0_r );
READ_HANDLER( arkanoid_input_2_r );


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, MRA_RAM },
	{ 0xd001, 0xd001, AY8910_read_port_0_r },
	{ 0xd00c, 0xd00c, arkanoid_68705_input_0_r },  /* mainly an input port, with 2 bits from the 68705 */
	{ 0xd010, 0xd010, input_port_1_r },
	{ 0xd018, 0xd018, arkanoid_Z80_mcu_r },  /* input from the 68705 */
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd000, AY8910_control_port_0_w },
	{ 0xd001, 0xd001, AY8910_write_port_0_w },
	{ 0xd008, 0xd008, arkanoid_d008_w },	/* gfx bank, flip screen etc. */
	{ 0xd010, 0xd010, watchdog_reset_w },
	{ 0xd018, 0xd018, arkanoid_Z80_mcu_w }, /* output to the 68705 */
	{ 0xe000, 0xe7ff, videoram_w, &videoram, &videoram_size },
	{ 0xe800, 0xe83f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe840, 0xefff, MWA_RAM },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress boot_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, MRA_RAM },
	{ 0xd001, 0xd001, AY8910_read_port_0_r },
	{ 0xd00c, 0xd00c, input_port_0_r },
	{ 0xd010, 0xd010, input_port_1_r },
	{ 0xd018, 0xd018, arkanoid_input_2_r },
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress boot_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd000, AY8910_control_port_0_w },
	{ 0xd001, 0xd001, AY8910_write_port_0_w },
	{ 0xd008, 0xd008, arkanoid_d008_w },	/* gfx bank, flip screen etc. */
	{ 0xd010, 0xd010, watchdog_reset_w },
	{ 0xd018, 0xd018, MWA_NOP },
	{ 0xe000, 0xe7ff, videoram_w, &videoram, &videoram_size },
	{ 0xe800, 0xe83f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe840, 0xefff, MWA_RAM },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress mcu_readmem[] =
{
	{ 0x0000, 0x0000, arkanoid_68705_portA_r },
	{ 0x0001, 0x0001, arkanoid_input_2_r },
	{ 0x0002, 0x0002, arkanoid_68705_portC_r },
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress mcu_writemem[] =
{
	{ 0x0000, 0x0000, arkanoid_68705_portA_w },
	{ 0x0002, 0x0002, arkanoid_68705_portC_w },
	{ 0x0004, 0x0004, arkanoid_68705_ddrA_w },
	{ 0x0006, 0x0006, arkanoid_68705_ddrC_w },
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( arkanoid )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SPECIAL )	/* input from the 68705, some bootlegs need it to be 1 */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* input from the 68705 */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 - spinner Player 1 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 30, 15, 0, 0)

	PORT_START      /* IN3 - spinner Player 2  */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_COCKTAIL, 30, 15, 0, 0)

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "20K 60K and every 60K" )
	PORT_DIPSETTING(    0x00, "20000 only" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
INPUT_PORTS_END

/* These are the input ports of the real Japanese ROM set                        */
/* 'Block' uses the these ones as well.	The Tayto bootleg is different			 */
/*  in coinage and # of lives.                    								 */

INPUT_PORTS_START( arknoidj )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SPECIAL )	/* input from the 68705, some bootlegs need it to be 1 */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* input from the 68705 */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 - spinner (multiplexed for player 1 and 2) */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 30, 15, 0, 0)

	PORT_START      /* IN3 - spinner Player 2  */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_COCKTAIL, 30, 15, 0, 0)

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "20K 60K and every 60K" )
	PORT_DIPSETTING(    0x00, "20000 only" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )
INPUT_PORTS_END

/* Is the same as arkanoij, but the Coinage and Bonus_Life dips are different */
INPUT_PORTS_START( arkatayt )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SPECIAL )	/* input from the 68705, some bootlegs need it to be 1 */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* input from the 68705 */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 - spinner (multiplexed for player 1 and 2) */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 30, 15, 0, 0)

	PORT_START      /* IN3 - spinner Player 2  */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_COCKTAIL, 30, 15, 0, 0)

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "60K 100K and every 60K" )
	PORT_DIPSETTING(    0x00, "60000 only" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	3,	/* 3 bits per pixel */
	{ 2*4096*8*8, 4096*8*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,  0, 64 },
	/* sprites use the same characters above, but are 16x8 */
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chips */
	1500000,	/* 1.5 MHz ???? */
	{ 33 },
	{ 0 },
	{ input_port_4_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_arkanoid =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,	/* 6 Mhz ?? */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_M68705,
			500000,	/* .5 Mhz (don't know really how fast, but it doesn't need to even be this fast) */
			mcu_readmem,mcu_writemem,0,0,
			ignore_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	100, /* 100 CPU slices per second to synchronize between the MCU and the main CPU */
	arkanoid_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	512, 512,
	arkanoid_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	arkanoid_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_bootleg =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,	/* 6 Mhz ?? */
			boot_readmem,boot_writemem,0,0,
			interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	512, 512,
	arkanoid_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	arkanoid_vh_screenrefresh,

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

ROM_START( arkanoid )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a75_01-1.rom", 0x0000, 0x8000, 0x5bcda3b0 )
	ROM_LOAD( "a75_11.rom",   0x8000, 0x8000, 0xeafd7191 )

	ROM_REGION( 0x0800, REGION_CPU2 )	/* 8k for the microcontroller */
	ROM_LOAD( "arkanoid.uc",  0x0000, 0x0800, 0x515d77b6 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arknoidu )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a75-19.bin",   0x0000, 0x8000, 0xd3ad37d7 )
	ROM_LOAD( "a75-18.bin",   0x8000, 0x8000, 0xcdc08301 )

	ROM_REGION( 0x0800, REGION_CPU2 )	/* 8k for the microcontroller */
	ROM_LOAD( "arknoidu.uc",  0x0000, 0x0800, BADCRC( 0xde518e47 ) )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkatour )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "t_ark1.bin",   0x0000, 0x8000, 0xe3b8faf5 )
	ROM_LOAD( "t_ark2.bin",   0x8000, 0x8000, 0x326aca4d )

	ROM_REGION( 0x0800, REGION_CPU2 )	/* 8k for the microcontroller */
	ROM_LOAD( "arkatour.uc",  0x0000, 0x0800, BADCRC( 0xd3249559 ) )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "t_ark3.bin",   0x00000, 0x8000, 0x5ddea3cf )
	ROM_LOAD( "t_ark4.bin",   0x08000, 0x8000, 0x5fcf2e85 )
	ROM_LOAD( "t_ark5.bin",   0x10000, 0x8000, 0x7b76b192 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arknoidj )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a75-21.rom",   0x0000, 0x8000, 0xbf0455fc )
	ROM_LOAD( "a75-22.rom",   0x8000, 0x8000, 0x3a2688d3 )

	ROM_REGION( 0x0800, REGION_CPU2 )	/* 8k for the microcontroller */
	ROM_LOAD( "arknoidj.uc",  0x0000, 0x0800, BADCRC( 0x0a4abef6 ) )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkbl2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "e1.6d",        0x0000, 0x8000, 0xdd4f2b72 )
	ROM_LOAD( "e2.6f",        0x8000, 0x8000, 0xbbc33ceb )

	ROM_REGION( 0x0800, REGION_CPU2 )	/* 8k for the microcontroller */
	ROM_LOAD( "68705p3.6i",   0x0000, 0x0800, 0x389a8cfb )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkbl3 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "arkanunk.1",   0x0000, 0x8000, 0xb0f73900 )
	ROM_LOAD( "arkanunk.2",   0x8000, 0x8000, 0x9827f297 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkatayt )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "arkanoid.1",   0x0000, 0x8000, 0x6e0a2b6f )
	ROM_LOAD( "arkanoid.2",   0x8000, 0x8000, 0x5a97dd56 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkblock )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "block01.bin",  0x0000, 0x8000, 0x5be667e1 )
	ROM_LOAD( "block02.bin",  0x8000, 0x8000, 0x4f883ef1 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkbloc2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ark-6.bin",    0x0000, 0x8000, 0x0be015de )
	ROM_LOAD( "arkgc.2",      0x8000, 0x8000, 0x9f0d4754 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END

ROM_START( arkangc )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "arkgc.1",      0x0000, 0x8000, 0xc54232e6 )
	ROM_LOAD( "arkgc.2",      0x8000, 0x8000, 0x9f0d4754 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a75_03.rom",   0x00000, 0x8000, 0x038b74ba )
	ROM_LOAD( "a75_04.rom",   0x08000, 0x8000, 0x71fae199 )
	ROM_LOAD( "a75_05.rom",   0x10000, 0x8000, 0xc76374e2 )

	ROM_REGION( 0x0600, REGION_PROMS )
	ROM_LOAD( "07.bpr",       0x0000, 0x0200, 0x0af8b289 )	/* red component */
	ROM_LOAD( "08.bpr",       0x0200, 0x0200, 0xabb002fb )	/* green component */
	ROM_LOAD( "09.bpr",       0x0400, 0x0200, 0xa7c6c277 )	/* blue component */
ROM_END



GAME( 1986, arkanoid, 0,        arkanoid, arkanoid, 0, ROT90, "Taito Corporation Japan", "Arkanoid (World)" )
GAME( 1986, arknoidu, arkanoid, arkanoid, arkanoid, 0, ROT90, "Taito America Corporation (Romstar license)", "Arkanoid (US)" )
GAME( 1986, arknoidj, arkanoid, arkanoid, arknoidj, 0, ROT90, "Taito Corporation", "Arkanoid (Japan)" )
GAMEX(1986, arkbl2,   arkanoid, arkanoid, arknoidj, 0, ROT90, "bootleg", "Arkanoid (Japanese bootleg Set 2)", GAME_NOT_WORKING )
GAMEX(1986, arkbl3,   arkanoid, bootleg,  arknoidj, 0, ROT90, "bootleg", "Arkanoid (Japanese bootleg Set 3)", GAME_NOT_WORKING )
GAME( 1986, arkatayt, arkanoid, bootleg,  arkatayt, 0, ROT90, "bootleg", "Arkanoid (Tayto bootleg, Japanese)" )
GAMEX(1986, arkblock, arkanoid, bootleg,  arknoidj, 0, ROT90, "bootleg", "Block (bootleg, Japanese)", GAME_NOT_WORKING )
GAME( 1986, arkbloc2, arkanoid, bootleg,  arknoidj, 0, ROT90, "bootleg", "Block (Game Corporation bootleg)" )
GAME( 1986, arkangc,  arkanoid, bootleg,  arknoidj, 0, ROT90, "bootleg", "Arkanoid (Game Corporation bootleg)" )
GAME( 1987, arkatour, arkanoid, arkanoid, arkanoid, 0, ROT90, "Taito America Corporation (Romstar license)", "Tournament Arkanoid (US)" )
