#include "../vidhrdw/dday.cpp"

/***************************************************************************

D-Day

driver by Zsolt Vasvari


Note: This game doesn't seem to support cocktail mode, which is not too
      suprising for a gun game.

0000-3fff ROM
5000-53ff Foreground RAM 1
5400-57ff Foreground RAM 2
5800-5bff Background RAM (Only the first 28 lines are visible,
						  the last 0x80 bytes probably contain color
						  information)
5c00-5fff Attributes RAM for Foreground 2
          A0-A4 seem to be ignored.
          D0 - X Flip
          D2 - Used by the software to separate area that the short shots
               cannot penetrate
          Others unknown, they don't seem to be used by this game
6000-63ff RAM

read:

6c00  Input Port #1
7000  Dip Sw #1
7400  Dip Sw #2
7800  Timer
7c00  Analog Control

write:

4000 Search light image and flip
6400 AY8910 #1 Control Port
6401 AY8910 #1 Write Port
6800 AY8910 #2 Control Port
6801 AY8910 #2 Write Port
7800 Bit 0 - Coin Counter 1
     Bit 1 - Coin Counter 2
	 Bit 2 - ??? Pulsated when the player is hit
	 Bit 3 - ??? Seems to be unused
	 Bit 4 - Tied to AY8910 RST. Used to turn off sound
	 Bit 5 - ??? Seem to be always on
	 Bit 6 - Search light enable
     Bit 7 - ???


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *dday_videoram2;
extern unsigned char *dday_videoram3;

void dday_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void dday_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( dday_colorram_w );
READ_HANDLER( dday_colorram_r );
WRITE_HANDLER( dday_control_w );
WRITE_HANDLER( dday_searchlight_w );
void dday_decode(void);


// Note: There seems to be no way to reset this timer via hardware.
//       The game uses a difference method to reset it to 99.
//
// Thanks Zwaxy for the timer info.

#define START_TIMER 99
static int timerVal = START_TIMER;

static READ_HANDLER( dday_timer_r )
{
    return ((timerVal / 10) << 4) | (timerVal % 10);
}

// This is not a real interrupt routine. It is just used to decrement the
// counter.
static int dday_interrupt (void)
{
    #define START_TIMER_SMALL 60
    static int timerValSmall = START_TIMER_SMALL;
    /* if the timer hits zero, start over at START_TIMER */
    timerValSmall--;
    if (timerValSmall == 0)
    {
		timerValSmall = START_TIMER_SMALL;
		timerVal--;
		if (timerVal == -1) timerVal = START_TIMER;
    }

    return ignore_interrupt();
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x5000, 0x5bff, MRA_RAM },
	{ 0x5c00, 0x5fff, dday_colorram_r },
	{ 0x6000, 0x63ff, MRA_RAM },
	{ 0x6c00, 0x6c00, input_port_0_r },
	{ 0x7000, 0x7000, input_port_1_r },
	{ 0x7400, 0x7400, input_port_2_r },
	{ 0x7800, 0x7800, dday_timer_r },
	{ 0x7c00, 0x7c00, input_port_3_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x4000, dday_searchlight_w },
	{ 0x5000, 0x53ff, MWA_RAM, &dday_videoram2 },
	{ 0x5400, 0x57ff, MWA_RAM, &dday_videoram3 },
	{ 0x5800, 0x5bff, MWA_RAM, &videoram, &videoram_size },
	{ 0x5c00, 0x5fff, dday_colorram_w, &colorram },
	{ 0x6000, 0x63ff, MWA_RAM },
	{ 0x6400, 0x6400, AY8910_control_port_0_w },
	{ 0x6401, 0x6401, AY8910_write_port_0_w },
	{ 0x6402, 0x6402, AY8910_control_port_0_w },
	{ 0x6403, 0x6403, AY8910_write_port_0_w },
	{ 0x6404, 0x6404, AY8910_control_port_0_w },
	{ 0x6405, 0x6405, AY8910_write_port_0_w },
	{ 0x6406, 0x6406, AY8910_control_port_0_w },
	{ 0x6407, 0x6407, AY8910_write_port_0_w },
	{ 0x6408, 0x6408, AY8910_control_port_0_w },
	{ 0x6409, 0x6409, AY8910_write_port_0_w },
	{ 0x640a, 0x640a, AY8910_control_port_0_w },
	{ 0x640b, 0x640b, AY8910_write_port_0_w },
	{ 0x640c, 0x640c, AY8910_control_port_0_w },
	{ 0x640d, 0x640d, AY8910_write_port_0_w },
	{ 0x6800, 0x6800, AY8910_control_port_1_w },
	{ 0x6801, 0x6801, AY8910_write_port_1_w },
	{ 0x7800, 0x7800, dday_control_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( dday )
	PORT_START      /* IN 0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 ) // Fire Button
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // Doesn't seem to be
                                                  // accessed
	PORT_START      /* DSW 0 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x00, "Extended Play At" )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x04, "15000" )
	PORT_DIPSETTING(    0x08, "20000" )
	PORT_DIPSETTING(    0x0c, "25000" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )  // has to do with lives
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW 1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_8C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_8C ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_8C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_8C ) )

	PORT_START      /* IN1 */
	PORT_ANALOG(0xff, 96, IPT_PADDLE, 20, 10, 0, 191 )
INPUT_PORTS_END

INPUT_PORTS_START( ddayc )
	PORT_START      /* IN 0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 ) // Fire Button
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 ) // Distance Button
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // Doesn't seem to be
                                                  // accessed
	PORT_START      /* DSW 0 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x00, "Extended Play At" )
	PORT_DIPSETTING(    0x00, "4000" )
	PORT_DIPSETTING(    0x04, "6000" )
	PORT_DIPSETTING(    0x08, "8000" )
	PORT_DIPSETTING(    0x0c, "10000" )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Easy" )   // Easy   - No Bombs, No Troop Carriers
	PORT_DIPSETTING(    0x20, "Normal" ) // Normal - No Bombs, Troop Carriers
	PORT_DIPSETTING(    0x10, "Hard" )   // Hard   - Bombs, Troop Carriers
//PORT_DIPSETTING(    0x00, "Hard" ) // Same as 0x10
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) ) // Doesn't seem to be used
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Start with 20000 Pts" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSW 1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_8C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_8C ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_8C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_8C ) )

	PORT_START      /* IN1 */
	PORT_ANALOG(0xff, 96, IPT_PADDLE, 20, 10, 0, 191 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	2,      /* 2 bits per pixel */
	{ 0, 0x0800*8 }, /* the two bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout charlayout_flipx =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	2,      /* 2 bits per pixel */
	{ 0, 0x0800*8 }, /* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,        0, 8 },
	{ REGION_GFX2, 0, &charlayout,       32, 8 },
	{ REGION_GFX3, 0, &charlayout,       64, 8 },
	{ REGION_GFX3, 0, &charlayout_flipx, 64, 8 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,      /* 2 chips */
	1000000,	/* 1.0 MHz ? */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


static struct MachineDriver machine_driver_dday =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			2000000,     /* 2 Mhz ? */
			readmem,writemem,0,0,
			dday_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	512,512,
	dday_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	dday_vh_screenrefresh,

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

ROM_START( dday )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "e8_63co.bin",  0x0000, 0x1000, 0x13d53793 )
	ROM_LOAD( "e7_64co.bin",  0x1000, 0x1000, 0xe1ef2a70 )
	ROM_LOAD( "e6_65co.bin",  0x2000, 0x1000, 0xfe414a83 )
	ROM_LOAD( "e5_66co.bin",  0x3000, 0x1000, 0xfc9f7774 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "k2_71.bin",    0x0000, 0x0800, 0xf85461de )
	ROM_LOAD( "k3_72.bin",    0x0800, 0x0800, 0xfdfe88b6 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "j8_70co.bin",  0x0000, 0x0800, 0x0c60e94c )
	ROM_LOAD( "j9_69co.bin",  0x0800, 0x0800, 0xba341c10 )

	ROM_REGION( 0x1000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "k6_74o.bin",   0x0000, 0x0800, 0x66719aea )
	ROM_LOAD( "k7_75o.bin",   0x0800, 0x0800, 0x5f8772e2 )

	ROM_REGION( 0x2000, REGION_GFX4 )      /* search light */
	ROM_LOAD( "d2_67.bin",    0x0000, 0x1000, 0x2b693e42 )  /* layout */
	ROM_LOAD( "d4_68.bin",    0x1000, 0x0800, 0xf3649264 )  /* mask */
							/*0x1800 -0x1fff will be filled in dynamically */

	ROM_REGION( 0x0800, REGION_GFX5 )      /* layer mask */
	ROM_LOAD( "k4_73.bin",    0x0000, 0x0800, 0xfa6237e4 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dday.m11",     0x0000, 0x0100, 0xaef6bbfc )  /* red component */
	ROM_LOAD( "dday.m8",      0x0100, 0x0100, 0xad3314b9 )  /* green component */
	ROM_LOAD( "dday.m3",      0x0200, 0x0100, 0xe877ab82 )  /* blue component */
ROM_END

ROM_START( ddayc )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "e8_63-c.bin",  0x0000, 0x1000, 0xd4fa3ae3 )
	ROM_LOAD( "e7_64-c.bin",  0x1000, 0x1000, 0x9fb8b1a7 )
	ROM_LOAD( "e6_65-c.bin",  0x2000, 0x1000, 0x4c210686 )
	ROM_LOAD( "e5_66-c.bin",  0x3000, 0x1000, 0xe7e832f9 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "k2_71.bin",    0x0000, 0x0800, 0xf85461de )
	ROM_LOAD( "k3_72.bin",    0x0800, 0x0800, 0xfdfe88b6 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "j8_70-c.bin",  0x0000, 0x0800, 0xa0c6b86b )
	ROM_LOAD( "j9_69-c.bin",  0x0800, 0x0800, 0xd352a3d6 )

	ROM_REGION( 0x1000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "k6_74.bin",    0x0000, 0x0800, 0xd21a3e22 )
	ROM_LOAD( "k7_75.bin",    0x0800, 0x0800, 0xa5e5058c )

	ROM_REGION( 0x2000, REGION_GFX4 )      /* search light */
	ROM_LOAD( "d2_67.bin",    0x0000, 0x1000, 0x2b693e42 )  /* layout */
	ROM_LOAD( "d4_68.bin",    0x1000, 0x0800, 0xf3649264 )  /* mask */
							/*0x1800 -0x1fff will be filled in dynamically */

	ROM_REGION( 0x0800, REGION_GFX5 )      /* layer mask */
	ROM_LOAD( "k4_73.bin",    0x0000, 0x0800, 0xfa6237e4 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "dday.m11",     0x0000, 0x0100, 0xaef6bbfc )  /* red component */
	ROM_LOAD( "dday.m8",      0x0100, 0x0100, 0xad3314b9 )  /* green component */
	ROM_LOAD( "dday.m3",      0x0200, 0x0100, 0xe877ab82 )  /* blue component */
ROM_END


static void init_dday(void)
{
	dday_decode();
}


GAMEX( 1982, dday,  0,    dday, dday,  dday, ROT0, "Olympia", "D-Day", GAME_IMPERFECT_COLORS )
GAMEX( 1982, ddayc, dday, dday, ddayc, dday, ROT0, "Olympia (Centuri license)", "D-Day (Centuri)", GAME_IMPERFECT_COLORS )
