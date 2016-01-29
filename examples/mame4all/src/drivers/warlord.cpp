#include "../vidhrdw/warlord.cpp"

/***************************************************************************
Warlords Driver by Lee Taylor and John Clegg


              Warlords Memory map and Dip Switches
              ------------------------------------

 Address  R/W  D7 D6 D5 D4 D3 D2 D1 D0   Function
--------------------------------------------------------------------------------------
0000-03FF       D  D  D  D  D  D  D  D   RAM
--------------------------------------------------------------------------------------
0400-07BF       D  D  D  D  D  D  D  D   Screen RAM (8x8 TILES, 32x32 SCREEN)
07C0-07CF       D  D  D  D  D  D  D  D   Motion Object Picture
07D0-07DF       D  D  D  D  D  D  D  D   Motion Object Vert.
07E0-07EF       D  D  D  D  D  D  D  D   Motion Object Horiz.
--------------------------------------------------------------------------------------
0800       R    D  D  D  D  D  D  D  D   Option Switch 1 (0 = On) (DSW 1)
0801       R    D  D  D  D  D  D  D  D   Option Switch 2 (0 = On) (DSW 2)
--------------------------------------------------------------------------------------
0C00       R    D                        Cocktail Cabinet  (0 = Cocktail)
           R       D                     VBLANK  (1 = VBlank)
           R          D                  SELF TEST
		   R             D               DIAG STEP (Unused)
0C01       R    D  D  D                  R,C,L Coin Switches (0 = On)
           R             D               Slam (0 = On)
           R                D            Player 4 Start Switch (0 = On)
           R                   D         Player 3 Start Switch (0 = On)
           R                      D      Player 2 Start Switch (0 = On)
           R                         D   Player 1 Start Switch (0 = On)
--------------------------------------------------------------------------------------
1000-100F  W   D  D  D  D  D  D  D  D    Pokey
--------------------------------------------------------------------------------------
1800       W                             IRQ Acknowledge
--------------------------------------------------------------------------------------
1C00-1C02  W    D  D  D  D  D  D  D  D   Coin Counters
--------------------------------------------------------------------------------------
1C03-1C06  W    D  D  D  D  D  D  D  D   LEDs
--------------------------------------------------------------------------------------
4000       W                             Watchdog
--------------------------------------------------------------------------------------
5000-7FFF  R                             Program ROM
--------------------------------------------------------------------------------------

Game Option Settings - J2 (DSW1)
=========================

8   7   6   5   4   3   2   1       Option
------------------------------------------
                        On  On      English
                        On  Off     French
                        Off On      Spanish
                        Off Off     German
                    On              Music at end of each game
                    Off             Music at end of game for new highscore
        On  On                      1 or 2 player game costs 1 credit
        On  Off                     1 player game=1 credit, 2 player=2 credits
        Off Off                     1 or 2 player game costs 2 credits
        Off On                      Not used
-------------------------------------------


Game Price Settings - M2 (DSW2)
========================

8   7   6   5   4   3   2   1       Option
------------------------------------------
                        On  On      Free play
                        On  Off     1 coin for 2 credits
                        Off On      1 coin for 1 credit
                        Off Off     2 coins for 1 credit
                On  On              Right coin mech x 1
                On  Off             Right coin mech x 4
                Off On              Right coin mech x 5
                Off Off             Right coin mech x 6
            On                      Left coin mech x 1
            Off                     Left coin mech x 2
On  On  On                          No bonus coins
On  On  Off                         For every 2 coins, add 1 coin
On  Off On                          For every 4 coins, add 1 coin
On  Off Off                         For every 4 coins, add 2 coins
Off On  On                          For every 5 coins, add 1 coin
------------------------------------------


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

void warlord_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void warlord_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static WRITE_HANDLER( warlord_led_w )
{
	osd_led_w(offset,~data >> 7);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0800, input_port_2_r },	/* DSW1 */
	{ 0x0801, 0x0801, input_port_3_r },	/* DSW2 */
	{ 0x0c00, 0x0c00, input_port_0_r },	/* IN0 */
	{ 0x0c01, 0x0c01, input_port_1_r },	/* IN1 */
	{ 0x1000, 0x100f, pokey1_r },		/* Read the 4 paddle values & the random # gen */
	{ 0x5000, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },		/* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07bf, videoram_w, &videoram, &videoram_size },
	{ 0x07c0, 0x07ff, MWA_RAM, &spriteram },
	{ 0x1000, 0x100f, pokey1_w },
	{ 0x1800, 0x1800, MWA_NOP },        /* IRQ Acknowledge */
	{ 0x1c00, 0x1c02, coin_counter_w },
	{ 0x1c03, 0x1c06, warlord_led_w },	/* 4 start lights */
	{ 0x4000, 0x4000, watchdog_reset_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( warlord )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME(0x10, 0x00, "Diag Step" )  /* Not referenced */
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x10, DEF_STR( On ) )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_DIPNAME(0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING (   0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Cocktail ) )

	PORT_START	/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START	/* IN2 */
	PORT_DIPNAME(0x03, 0x00, "Language" )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x01, "French" )
	PORT_DIPSETTING (   0x02, "Spanish" )
	PORT_DIPSETTING (   0x03, "German" )
	PORT_DIPNAME(0x04, 0x00, "Music" )
	PORT_DIPSETTING (   0x00, "End of game" )
	PORT_DIPSETTING (   0x04, "High score only" )
	PORT_BIT ( 0xc8, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME(0x30, 0x00, "Credits" )
	PORT_DIPSETTING (   0x00, "1p/2p = 1 credit" )
	PORT_DIPSETTING (   0x10, "1p = 1, 2p = 2" )
	PORT_DIPSETTING (   0x20, "1p/2p = 2 credits" )

	PORT_START	/* IN3 */
	PORT_DIPNAME(0x03, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, "Right Coin" )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x04, "*4" )
	PORT_DIPSETTING (   0x08, "*5" )
	PORT_DIPSETTING (   0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, "Left Coin" )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x10, "*2" )
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING (   0x00, "None" )
	PORT_DIPSETTING (   0x20, "3 credits/2 coins" )
	PORT_DIPSETTING (   0x40, "5 credits/4 coins" )
	PORT_DIPSETTING (   0x60, "6 credits/4 coins" )
	PORT_DIPSETTING (   0x80, "6 credits/5 coins" )

    /* IN4-7 fake to control player paddles */
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_PLAYER1, 50, 10, 0x1d, 0xcb )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_PLAYER2, 50, 10, 0x1d, 0xcb )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_PLAYER3, 50, 10, 0x1d, 0xcb )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_PLAYER4, 50, 10, 0x1d, 0xcb )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	64,	    /* 64  characters */
	2,	    /* 2 bits per pixel */
	{ 128*8*8, 0 },	/* bitplane separation */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,   0,   8 },
	{ REGION_GFX1, 0x0200, &charlayout,   8*4, 8 },
	{ -1 } /* end of array */
};



static struct POKEYinterface pokey_interface =
{
	1,	/* 1 chip */
	12096000/8,	/* 1.512 MHz */
	{ 100 },
	/* The 8 pot handlers */
	{ input_port_4_r },
	{ input_port_5_r },
	{ input_port_6_r },
	{ input_port_7_r },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	/* The allpot handler */
	{ 0 },
};



static struct MachineDriver machine_driver_warlord =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			12096000/16, /* 756 kHz */
			readmem,writemem,0,0,
			interrupt,4
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	128, 8*4+8*4,
	warlord_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY|VIDEO_MODIFIES_PALETTE,
	0,
	generic_vh_start,
	generic_vh_stop,
	warlord_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( warlord )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "037154.1m",    0x5000, 0x0800, 0x18006c87 )
	ROM_LOAD( "037153.1k",    0x5800, 0x0800, 0x67758f4c )
	ROM_LOAD( "037158.1j",    0x6000, 0x0800, 0x1f043a86 )
	ROM_LOAD( "037157.1h",    0x6800, 0x0800, 0x1a639100 )
	ROM_LOAD( "037156.1e",    0x7000, 0x0800, 0x534f34b4 )
	ROM_LOAD( "037155.1d",    0x7800, 0x0800, 0x23b94210 )
	ROM_RELOAD(            0xf800, 0x0800 )	/* for the reset and interrupt vectors */

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "037159.6e",    0x0000, 0x0800, 0xff979a08 )

	ROM_REGION( 0x0100, REGION_PROMS )
	/* Only the first 0x80 bytes are used by the hardware. A7 is grounded. */
	/* Bytes 0x00-0x3f are used fore the color cocktail version. */
	/* Bytes 0x40-0x7f are for the upright version of the cabinet with a */
	/* mirror and painted background. */
	ROM_LOAD( "warlord.clr",  0x0000, 0x0100, 0xa2c5c277 )
ROM_END



GAME( 1980, warlord, 0, warlord, warlord, 0, ROT0, "Atari", "Warlords" )
