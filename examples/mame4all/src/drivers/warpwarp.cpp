#include "../vidhrdw/warpwarp.cpp"
#include "../sndhrdw/warpwarp.cpp"

/***************************************************************************

Warp Warp memory map (preliminary)

  Memory Map figured out by Chris Hardy (chrish@kcbbs.gen.nz)
  Initial Driver code by Mirko


0000-37FF ROM		Code
4800-4FFF ROM		Graphics rom which must be included in memory space

memory mapped ports:

read:

All Read ports only use bit 0

C000      Coin slot 2
C001      ???
C002      Start P1
C003      Start P2
C004      Fire
C005      Test Mode
C006      ???
C007      Coin Slot 2
C010      Joystick (read like an analog one, but it's digital)
          0->23 = DOWN
          24->63 = UP
          64->111 = LEFT
          112->167 = RIGHT
          168->255 = NEUTRAL
C020-C027 Dipswitch 1->8 in bit 0

write:
c000-c001 bullet x/y pos
C002      Sound
C003      WatchDog reset
C010      Music 1
C020      Music 2
c030-c032 lamps
c034      coin lock out
c035      coin counter
c036      IRQ enable _and_ bullet enable (both on bit 0) (currently ignored)
C037      flip screen (currently ignored)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *warpwarp_bulletsram;
void warpwarp_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void warpwarp_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* from sndhrdw/warpwarp.c */
WRITE_HANDLER( warpwarp_sound_w );
WRITE_HANDLER( warpwarp_music1_w );
WRITE_HANDLER( warpwarp_music2_w );
extern int warpwarp_sh_start(const struct MachineSound *msound);
extern void warpwarp_sh_stop(void);
extern void warpwarp_sh_update(void);

static READ_HANDLER( warpwarp_input_c000_7_r )
{
	return (readinputport(0) >> offset) & 1;
}

/* Read the Dipswitches */
static READ_HANDLER( warpwarp_input_c020_27_r )
{
	return (readinputport(1) >> offset) & 1;
}

static READ_HANDLER( warpwarp_input_controller_r )
{
	int res;

	res = readinputport(2);
	if (res & 1) return 23;
	if (res & 2) return 63;
	if (res & 4) return 111;
	if (res & 8) return 167;
	return 255;
}

static WRITE_HANDLER( warpwarp_leds_w )
{
	osd_led_w(offset,data);
}



static struct MemoryReadAddress bombbee_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x4800, 0x4fff, MRA_ROM },
	{ 0x6000, 0x6007, warpwarp_input_c000_7_r },
	{ 0x6010, 0x6010, input_port_2_r },
	{ 0x6020, 0x6027, warpwarp_input_c020_27_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bombbee_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x4000, 0x43ff, videoram_w, &videoram, &videoram_size },
	{ 0x4400, 0x47ff, colorram_w, &colorram },
	{ 0x4800, 0x4fff, MWA_ROM },
	{ 0x6000, 0x6001, MWA_RAM, &warpwarp_bulletsram },
	{ 0x6002, 0x6002, warpwarp_sound_w },
	{ 0x6003, 0x6003, watchdog_reset_w },
	{ 0x6010, 0x6010, warpwarp_music1_w },
	{ 0x6020, 0x6020, warpwarp_music2_w },
    { 0x6030, 0x6032, warpwarp_leds_w },
	{ 0x6035, 0x6035, coin_counter_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress warpwarp_readmem[] =
{
	{ 0x0000, 0x37ff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x4800, 0x4fff, MRA_ROM },
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0xc000, 0xc007, warpwarp_input_c000_7_r },
	{ 0xc010, 0xc010, warpwarp_input_controller_r },
	{ 0xc020, 0xc027, warpwarp_input_c020_27_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress warpwarp_writemem[] =
{
	{ 0x0000, 0x37ff, MWA_ROM },
	{ 0x4000, 0x43ff, videoram_w, &videoram, &videoram_size },
	{ 0x4400, 0x47ff, colorram_w, &colorram },
	{ 0x4800, 0x4fff, MWA_ROM },
	{ 0x8000, 0x83ff, MWA_RAM },
	{ 0xc000, 0xc001, MWA_RAM, &warpwarp_bulletsram },
	{ 0xc002, 0xc002, warpwarp_sound_w },
    { 0xc003, 0xc003, watchdog_reset_w },
	{ 0xc010, 0xc010, warpwarp_music1_w },
	{ 0xc020, 0xc020, warpwarp_music2_w },
    { 0xc030, 0xc032, warpwarp_leds_w },
	{ 0xc035, 0xc035, coin_counter_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( bombbee )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
//	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPSETTING(    0x20, "60000" )
	PORT_DIPSETTING(    0x40, "70000" )
	PORT_DIPSETTING(    0x60, "80000" )
	PORT_DIPSETTING(    0x80, "100000" )
	PORT_DIPSETTING(    0xa0, "120000" )
	PORT_DIPSETTING(    0xc0, "150000" )
	PORT_DIPSETTING(    0xe0, "None" )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_REVERSE, 30, 10, 0x14, 0xac )
INPUT_PORTS_END

INPUT_PORTS_START( cutieq )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
//	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPSETTING(    0x20, "60000" )
	PORT_DIPSETTING(    0x40, "80000" )
	PORT_DIPSETTING(    0x60, "100000" )
	PORT_DIPSETTING(    0x80, "120000" )
	PORT_DIPSETTING(    0xa0, "150000" )
	PORT_DIPSETTING(    0xc0, "200000" )
	PORT_DIPSETTING(    0xe0, "None" )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_REVERSE, 30, 10, 0x14, 0xac )
INPUT_PORTS_END

INPUT_PORTS_START( warpwarp )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	/* TODO: The bonus setting changes for 5 lives */
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "8000 30000" )
	PORT_DIPSETTING(    0x10, "10000 40000" )
	PORT_DIPSETTING(    0x20, "15000 60000" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/*when level selection is On, press 1 to increase level */
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Selection", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* FAKE - used by input_controller_r to simulate an analog stick */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
INPUT_PORTS_END

/* has High Score Initials dip switch instead of rack test */
INPUT_PORTS_START( warpwarr )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	/* TODO: The bonus setting changes for 5 lives */
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "8000 30000" )
	PORT_DIPSETTING(    0x10, "10000 40000" )
	PORT_DIPSETTING(    0x20, "15000 60000" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "High Score Initials" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )

	PORT_START      /* FAKE - used by input_controller_r to simulate an analog stick */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
INPUT_PORTS_END




static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	1,	/* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	1,	/* 1 bit per pixel */
	{ 0 },
	{  0, 1, 2, 3, 4, 5, 6, 7 ,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_CPU1, 0x4800, &charlayout,   0, 256 },
	{ REGION_CPU1, 0x4800, &spritelayout, 0, 256 },
	{ -1 } /* end of array */
};

static struct CustomSound_interface custom_interface =
{
	warpwarp_sh_start,
	warpwarp_sh_stop,
	warpwarp_sh_update
};


#define MACHINE(NAME) 								\
static struct MachineDriver machine_driver_##NAME = \
{ 			 										\
	{ 												\
		{ 											\
			CPU_8080, 								\
			2048000,	/* 3 Mhz? */ 				\
			NAME##_readmem,NAME##_writemem,0,0, 	\
			interrupt,1 							\
		} 											\
	}, 												\
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */	\
	1,	/* single CPU, no need for interleaving */ 	\
	0, 												\
 													\
	/* video hardware */ 							\
  	34*8, 32*8, { 0*8, 34*8-1, 2*8, 30*8-1 }, 		\
	gfxdecodeinfo, 									\
	256, 2*256, 									\
	warpwarp_vh_convert_color_prom, 				\
 													\
	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY, 		\
	0, 												\
	generic_vh_start, 								\
	generic_vh_stop, 								\
	warpwarp_vh_screenrefresh, 						\
 													\
	/* sound hardware */ 							\
	0,0,0,0,										\
	{												\
		{											\
			SOUND_CUSTOM,							\
			&custom_interface						\
		}											\
	}												\
};


MACHINE( bombbee )
MACHINE( warpwarp )


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( bombbee )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "bombbee.1k",   0x0000, 0x2000, 0x9f8cd7af )
	ROM_LOAD( "bombbee.4c",   0x4800, 0x0800, 0x5f37d569 )
ROM_END

ROM_START( cutieq )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cutieq.1k",    0x0000, 0x2000, 0x6486cdca )
	ROM_LOAD( "cutieq.4c",    0x4800, 0x0800, 0x0e1618c9 )
ROM_END

ROM_START( warpwarp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "g-n9601n.2r",  0x0000, 0x1000, 0xf5262f38 )
	ROM_LOAD( "g-09602n.2m",  0x1000, 0x1000, 0xde8355dd )
	ROM_LOAD( "g-09603n.1p",  0x2000, 0x1000, 0xbdd1dec5 )
	ROM_LOAD( "g-09613n.1t",  0x3000, 0x0800, 0xaf3d77ef )
	ROM_LOAD( "g-9611n.4c",   0x4800, 0x0800, 0x380994c8 )
ROM_END

ROM_START( warpwarr )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "g-09601.2r",   0x0000, 0x1000, 0x916ffa35 )
	ROM_LOAD( "g-09602.2m",   0x1000, 0x1000, 0x398bb87b )
	ROM_LOAD( "g-09603.1p",   0x2000, 0x1000, 0x6b962fc4 )
	ROM_LOAD( "g-09613.1t",   0x3000, 0x0800, 0x60a67e76 )
	ROM_LOAD( "g-9611.4c",    0x4800, 0x0800, 0x00e6a326 )
ROM_END

ROM_START( warpwar2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "g-09601.2r",   0x0000, 0x1000, 0x916ffa35 )
	ROM_LOAD( "g-09602.2m",   0x1000, 0x1000, 0x398bb87b )
	ROM_LOAD( "g-09603.1p",   0x2000, 0x1000, 0x6b962fc4 )
	ROM_LOAD( "g-09612.1t",   0x3000, 0x0800, 0xb91e9e79 )
	ROM_LOAD( "g-9611.4c",    0x4800, 0x0800, 0x00e6a326 )
ROM_END



GAMEX( 1979, bombbee,  0,        bombbee,  bombbee,  0, ROT90, "Namco", "Bomb Bee", GAME_NO_COCKTAIL )
GAMEX( 1979, cutieq,   0,        bombbee,  cutieq,   0, ROT90, "Namco", "Cutie Q", GAME_NO_COCKTAIL )
GAMEX( 1981, warpwarp, 0,        warpwarp, warpwarp, 0, ROT90, "Namco", "Warp & Warp", GAME_NO_COCKTAIL )
GAMEX( 1981, warpwarr, warpwarp, warpwarp, warpwarr, 0, ROT90, "[Namco] (Rock-ola license)", "Warp Warp (Rock-ola set 1)", GAME_NO_COCKTAIL )
GAMEX( 1981, warpwar2, warpwarp, warpwarp, warpwarr, 0, ROT90, "[Namco] (Rock-ola license)", "Warp Warp (Rock-ola set 2)", GAME_NO_COCKTAIL )
