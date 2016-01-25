#include "../vidhrdw/centiped.cpp"

/***************************************************************************

Main clock: XTAL = 12.096 MHz
6502 Clock: XTAL/8 = 1.512 MHz (0.756 when accessing playfield RAM)
Horizontal video frequency: HSYNC = XTAL/256/3 = 15.75 kHz
Video frequency: VSYNC = HSYNC/263 ?? = 59.88593 Hz (not sure, could be /262)
VBlank duration: 1/VSYNC * (23/263) = 1460 us


              Centipede Memory map and Dip Switches
              -------------------------------------

Memory map for Centipede directly from the Atari schematics (1981).

 Address  R/W  D7 D6 D5 D4 D3 D2 D1 D0   Function
--------------------------------------------------------------------------------------
0000-03FF       D  D  D  D  D  D  D  D   RAM
--------------------------------------------------------------------------------------
0400-07BF       D  D  D  D  D  D  D  D   Playfield RAM
07C0-07CF       D  D  D  D  D  D  D  D   Motion Object Picture
07D0-07DF       D  D  D  D  D  D  D  D   Motion Object Vert.
07E0-07EF       D  D  D  D  D  D  D  D   Motion Object Horiz.
07F0-07FF             D  D  D  D  D  D   Motion Object Color
--------------------------------------------------------------------------------------
0800       R    D  D  D  D  D  D  D  D   Option Switch 1 (0 = On)
0801       R    D  D  D  D  D  D  D  D   Option Switch 2 (0 = On)
--------------------------------------------------------------------------------------
0C00       R    D           D  D  D  D   Horizontal Mini-Track Ball tm Inputs
           R       D                     VBLANK  (1 = VBlank)
           R          D                  Self-Test  (0 = On)
           R             D               Cocktail Cabinet  (1 = Cocktail)
0C01       R    D  D  D                  R,C,L Coin Switches (0 = On)
           R             D               SLAM  (0 = On)
           R                D            Player 2 Fire Switch (0 = On)
           R                   D         Player 1 Fire Switch (0 = On)
           R                      D      Player 2 Start Switch (0 = On)
           R                         D   Player 1 Start Switch (0 = On)

0C02       R    D           D  D  D  D   Vertical Mini-Track Ball tm Inputs
0C03       R    D  D  D  D               Player 1 Joystick (R,L,Down,Up)
           R                D  D  D  D   Player 2 Joystick   (0 = On)
--------------------------------------------------------------------------------------
1000-100F R/W   D  D  D  D  D  D  D  D   Custom Audio Chip
1404       W                D  D  D  D   Playfield Color RAM
140C       W                D  D  D  D   Motion Object Color RAM
--------------------------------------------------------------------------------------
1600       W    D  D  D  D  D  D  D  D   EA ROM Address & Data Latch
1680       W                D  D  D  D   EA ROM Control Latch
1700       R    D  D  D  D  D  D  D  D   EA ROM Read Data
--------------------------------------------------------------------------------------
1800       W                             IRQ Acknowledge
--------------------------------------------------------------------------------------
1C00       W    D                        Left Coin Counter (1 = On)
1C01       W    D                        Center Coin Counter (1 = On)
1C02       W    D                        Right Coin Counter (1 = On)
1C03       W    D                        Player 1 Start LED (0 = On)
1C04       W    D                        Player 2 Start LED (0 = On)
1C07       W    D                        Track Ball Flip Control (0 = Player 1)
--------------------------------------------------------------------------------------
2000       W                             WATCHDOG
2400       W                             Clear Mini-Track Ball Counters
--------------------------------------------------------------------------------------
2000-3FFF  R                             Program ROM
--------------------------------------------------------------------------------------

-EA ROM is an Erasable Reprogrammable rom to save the top 3 high scores
  and other stuff.


 Dip switches at N9 on the PCB

 8    7    6    5    4    3    2    1    Option
-------------------------------------------------------------------------------------
                              On   On    English $
                              On   Off   German
                              Off  On    French
                              Off  Off   Spanish
-------------------------------------------------------------------------------------
                    On   On              2 lives per game
                    On   Off             3 lives per game $
                    Off  On              4 lives per game
                    Off  Off             5 lives per game
-------------------------------------------------------------------------------------
                                         Bonus life granted at every:
          On   On                        10,000 points
          On   Off                       12.000 points $
          Off  On                        15,000 points
          Off  Off                       20,000 points
-------------------------------------------------------------------------------------
     On                                  Hard game difficulty
     Off                                 Easy game difficulty $
-------------------------------------------------------------------------------------
On                                       1-credit minimum $
Off                                      2-credit minimum
-------------------------------------------------------------------------------------

$ = Manufacturer's suggested settings


 Dip switches at N8 on the PCB

 8    7    6    5    4    3    2    1    Option
-------------------------------------------------------------------------------------
                              On   On    Free play
                              On   Off   1 coin for 2 credits
                              Off  On    1 coin for 1 credit $
                              Off  Off   2 coins for 1 credit
-------------------------------------------------------------------------------------
                    On   On              Right coin mech X 1 $
                    On   Off             Right coin mech X 4
                    Off  On              Right coin mech X 5
                    Off  Off             Right coin mech X 6
-------------------------------------------------------------------------------------
               On                        Left coin mech X 1 $
               Off                       Left coin mech X 2
-------------------------------------------------------------------------------------
On   On   On                             No bonus coins $
On   On   Off                            For every 2 coins inserted, game logic
                                          adds 1 more coin
On   Off  On                             For every 4 coins inserted, game logic
                                          adds 1 more coin
On   Off  Off                            For every 4 coins inserted, game logic
                                          adds 2 more coin
Off  On   On                             For every 5 coins inserted, game logic
                                          adds 1 more coin
Off  On   Off                            For every 3 coins inserted, game logic
                                          adds 1 more coin
-------------------------------------------------------------------------------------
$ = Manufacturer's suggested settings

Changes:
	30 Apr 98 LBO
	* Fixed test mode
	* Changed high score to use earom routines
	* Added support for alternate rom set

Known issues:

* Are coins supposed to take over a second to register?
* Need to confirm CPU and Pokey clocks

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/atari_vg.h"


WRITE_HANDLER( centiped_paletteram_w );
void centiped_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

void centiped_init_machine(void);	/* in vidhrdw */
int centiped_interrupt(void);	/* in vidhrdw */


/*
 * This wrapper routine is necessary because Centipede requires a direction bit
 * to be set or cleared. The direction bit is held until the mouse is moved
 * again.
 *
 * There is a 4-bit counter, and two inputs from the trackball: DIR and CLOCK.
 * CLOCK makes the counter move in the direction of DIR. Since DIR is latched
 * only when a CLOCK arrives, the DIR bit in the input port doesn't change
 * until the trackball actually moves.
 *
 * There is also a CLR input to the counter which could be used by the game to
 * clear the counter, but Centipede doesn't use it (though it would be a good
 * idea to support it anyway).
 *
 * The counter is read 240 times per second. There is no provision whatsoever
 * to prevent the counter from wrapping around between reads.
 */
static READ_HANDLER( centiped_IN0_r )
{
	static int oldpos,sign;
	int newpos;

	newpos = readinputport(6);
	if (newpos != oldpos)
	{
		sign = (newpos - oldpos) & 0x80;
		oldpos = newpos;
	}

	return ((readinputport(0) & 0x70) | (oldpos & 0x0f) | sign );
}

static READ_HANDLER( centiped_IN2_r )
{
	static int oldpos,sign;
	int newpos;

	newpos = readinputport(2);
	if (newpos != oldpos)
	{
		sign = (newpos - oldpos) & 0x80;
		oldpos = newpos;
	}

	return ((oldpos & 0x0f) | sign );
}


static WRITE_HANDLER( centiped_led_w )
{
	osd_led_w(offset,~data >> 7);
}

static READ_HANDLER( centipdb_rand_r )
{
	return rand() % 0xff;
}

static WRITE_HANDLER( centipdb_AY8910_w )
{
	AY8910_control_port_0_w(0, offset);
	AY8910_write_port_0_w(0, data);
}

static READ_HANDLER( centipdb_AY8910_r )
{
	AY8910_control_port_0_w(0, offset);
	return AY8910_read_port_0_r(0);
}

static struct MemoryReadAddress centiped_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0400, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0800, input_port_4_r },	/* DSW1 */
	{ 0x0801, 0x0801, input_port_5_r },	/* DSW2 */
	{ 0x0c00, 0x0c00, centiped_IN0_r },	/* IN0 */
	{ 0x0c01, 0x0c01, input_port_1_r },	/* IN1 */
	{ 0x0c02, 0x0c02, centiped_IN2_r },	/* IN2 */	/* JB 971220 */
	{ 0x0c03, 0x0c03, input_port_3_r },	/* IN3 */
	{ 0x1000, 0x100f, pokey1_r },
	{ 0x1700, 0x173f, atari_vg_earom_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },	/* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

/* Same as the regular one, except it uses an AY8910 and an external RNG */
static struct MemoryReadAddress centipdb_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0400, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0800, input_port_4_r },	/* DSW1 */
	{ 0x0801, 0x0801, input_port_5_r },	/* DSW2 */
	{ 0x0c00, 0x0c00, centiped_IN0_r },	/* IN0 */
	{ 0x0c01, 0x0c01, input_port_1_r },	/* IN1 */
	{ 0x0c02, 0x0c02, centiped_IN2_r },	/* IN2 */	/* JB 971220 */
	{ 0x0c03, 0x0c03, input_port_3_r },	/* IN3 */
	{ 0x1000, 0x100f, centipdb_AY8910_r },
	{ 0x1700, 0x173f, atari_vg_earom_r },
	{ 0x1780, 0x1780, centipdb_rand_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },	/* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress centipb2_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0400, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0800, input_port_4_r },	/* DSW1 */
	{ 0x0801, 0x0801, input_port_5_r },	/* DSW2 */
	{ 0x0c00, 0x0c00, centiped_IN0_r },	/* IN0 */
	{ 0x0c01, 0x0c01, input_port_1_r },	/* IN1 */
	{ 0x0c02, 0x0c02, centiped_IN2_r },	/* IN2 */	/* JB 971220 */
	{ 0x0c03, 0x0c03, input_port_3_r },	/* IN3 */
	{ 0x1001, 0x1001, AY8910_read_port_0_r },
	{ 0x1700, 0x173f, atari_vg_earom_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0x6000, 0x67ff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },	/* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress centiped_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07bf, videoram_w, &videoram, &videoram_size },
	{ 0x07c0, 0x07ff, MWA_RAM, &spriteram },
	{ 0x1000, 0x100f, pokey1_w },
	{ 0x1400, 0x140f, centiped_paletteram_w, &paletteram },
	{ 0x1600, 0x163f, atari_vg_earom_w },
	{ 0x1680, 0x1680, atari_vg_earom_ctrl_w },
	{ 0x1800, 0x1800, MWA_NOP },	/* IRQ acknowldege */
	{ 0x1c00, 0x1c02, coin_counter_w },
	{ 0x1c03, 0x1c04, centiped_led_w },
	{ 0x1c07, 0x1c07, flip_screen_w },
	{ 0x2000, 0x2000, watchdog_reset_w },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ -1 }	/* end of table */
};

/* Same as the regular one, except it uses an AY8910 */
static struct MemoryWriteAddress centipdb_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07bf, videoram_w, &videoram, &videoram_size },
	{ 0x07c0, 0x07ff, MWA_RAM, &spriteram },
	{ 0x1000, 0x100f, centipdb_AY8910_w },
	{ 0x1400, 0x140f, centiped_paletteram_w, &paletteram },
	{ 0x1600, 0x163f, atari_vg_earom_w },
	{ 0x1680, 0x1680, atari_vg_earom_ctrl_w },
	{ 0x1800, 0x1800, MWA_NOP },	/* IRQ acknowldege */
	{ 0x1c00, 0x1c02, coin_counter_w },
	{ 0x1c03, 0x1c04, centiped_led_w },
	{ 0x1c07, 0x1c07, flip_screen_w },
	{ 0x2000, 0x2000, watchdog_reset_w },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress centipb2_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07bf, videoram_w, &videoram, &videoram_size },
	{ 0x07c0, 0x07ff, MWA_RAM, &spriteram },
	{ 0x1000, 0x1000, AY8910_write_port_0_w },
	{ 0x1001, 0x1001, AY8910_control_port_0_w },
	{ 0x1400, 0x140f, centiped_paletteram_w, &paletteram },
	{ 0x1600, 0x163f, atari_vg_earom_w },
	{ 0x1680, 0x1680, atari_vg_earom_ctrl_w },
	{ 0x1800, 0x1800, MWA_NOP },	/* IRQ acknowldege */
	{ 0x1c00, 0x1c02, coin_counter_w },
	{ 0x1c03, 0x1c04, centiped_led_w },
	{ 0x1c07, 0x1c07, flip_screen_w },
	{ 0x2000, 0x2000, watchdog_reset_w },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ 0x6000, 0x67ff, MWA_ROM },
	{ -1 }	/* end of table */
};


/* The input ports are identical for the real one and the bootleg one, except
   that one of the languages is Italian in the bootleg one instead of Spanish */

#define PORTS(GAMENAME, FOURTH_LANGUAGE)										\
																				\
INPUT_PORTS_START( GAMENAME )										\
	PORT_START	/* IN0 */														\
	/* The lower 4 bits and bit 7 are for trackball x input. */					\
	/* They are handled by fake input port 6 and a custom routine. */			\
	PORT_BIT ( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )								\
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Cabinet ) )								\
	PORT_DIPSETTING (   0x00, DEF_STR( Upright ) )								\
	PORT_DIPSETTING (   0x10, DEF_STR( Cocktail ) )								\
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )											\
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )								\
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )								\
																				\
	PORT_START	/* IN1 */														\
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_START1 )								\
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_START2 )								\
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )								\
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )					\
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_TILT )									\
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )									\
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )									\
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )									\
																				\
	PORT_START	/* IN2 */														\
	PORT_ANALOGX( 0xff, 0x00, IPT_TRACKBALL_Y, 50, 10, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE )  \
	/* The lower 4 bits are the input, and bit 7 is the direction. */			\
	/* The state of bit 7 does not change if the trackball is not moved.*/		\
																				\
	PORT_START	/* IN3 */														\
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )	\
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )		\
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )	\
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )	\
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )				\
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )				\
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )				\
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )				\
																				\
	PORT_START	/* IN4 */														\
	PORT_DIPNAME(0x03, 0x00, "Language" )										\
	PORT_DIPSETTING (   0x00, "English" )										\
	PORT_DIPSETTING (   0x01, "German" )										\
	PORT_DIPSETTING (   0x02, "French" )										\
	PORT_DIPSETTING (   0x03, FOURTH_LANGUAGE )									\
	PORT_DIPNAME(0x0c, 0x04, DEF_STR( Lives ) )									\
	PORT_DIPSETTING (   0x00, "2" )												\
	PORT_DIPSETTING (   0x04, "3" )												\
	PORT_DIPSETTING (   0x08, "4" )												\
	PORT_DIPSETTING (   0x0c, "5" )												\
	PORT_DIPNAME(0x30, 0x10, DEF_STR( Bonus_Life ) )							\
	PORT_DIPSETTING (   0x00, "10000" )											\
	PORT_DIPSETTING (   0x10, "12000" )											\
	PORT_DIPSETTING (   0x20, "15000" )											\
	PORT_DIPSETTING (   0x30, "20000" )											\
	PORT_DIPNAME(0x40, 0x40, DEF_STR( Difficulty ) )							\
	PORT_DIPSETTING (   0x40, "Easy" )											\
	PORT_DIPSETTING (   0x00, "Hard" )											\
	PORT_DIPNAME(0x80, 0x00, "Credit Minimum" )									\
	PORT_DIPSETTING (   0x00, "1" )												\
	PORT_DIPSETTING (   0x80, "2" )												\
																				\
	PORT_START	/* IN5 */														\
	PORT_DIPNAME(0x03, 0x02, DEF_STR( Coinage ) )								\
	PORT_DIPSETTING (   0x03, DEF_STR( 2C_1C ) )								\
	PORT_DIPSETTING (   0x02, DEF_STR( 1C_1C ) )								\
	PORT_DIPSETTING (   0x01, DEF_STR( 1C_2C ) )								\
	PORT_DIPSETTING (   0x00, DEF_STR( Free_Play ) )							\
	PORT_DIPNAME(0x0c, 0x00, "Right Coin" )										\
	PORT_DIPSETTING (   0x00, "*1" )											\
	PORT_DIPSETTING (   0x04, "*4" )											\
	PORT_DIPSETTING (   0x08, "*5" )											\
	PORT_DIPSETTING (   0x0c, "*6" )											\
	PORT_DIPNAME(0x10, 0x00, "Left Coin" )										\
	PORT_DIPSETTING (   0x00, "*1" )											\
	PORT_DIPSETTING (   0x10, "*2" )											\
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coins" )									\
	PORT_DIPSETTING (   0x00, "None" )											\
	PORT_DIPSETTING (   0x20, "3 credits/2 coins" )								\
	PORT_DIPSETTING (   0x40, "5 credits/4 coins" )								\
	PORT_DIPSETTING (   0x60, "6 credits/4 coins" )								\
	PORT_DIPSETTING (   0x80, "6 credits/5 coins" )								\
	PORT_DIPSETTING (   0xa0, "4 credits/3 coins" )								\
																				\
	PORT_START	/* IN6, fake trackball input port. */							\
	PORT_ANALOGX( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE, 50, 10, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE )	\
INPUT_PORTS_END

PORTS(centiped, "Spanish")
PORTS(centipdb, "Italian")


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 256*8*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	8,16,	/* 16*8 sprites */
	128,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 128*16*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   4, 4 },	/* 4 color codes to support midframe */
												/* palette changes in test mode */
	{ REGION_GFX1, 0, &spritelayout, 0, 1 },
	{ -1 } /* end of array */
};



static struct POKEYinterface pokey_interface =
{
	1,	/* 1 chip */
	12096000/8,	/* 1.512 MHz */
	{ 100 },
	/* The 8 pot handlers */
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	/* The allpot handler */
	{ 0 },
};

static struct AY8910interface centipdb_ay8910_interface =
{
	1,	/* 1 chips */
	12096000/8,	/* 1.512 MHz */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct AY8910interface centipb2_ay8910_interface =
{
	1,	/* 1 chips */
	12096000/8,	/* 1.512 MHz */
	{ 50 },
	{ centipdb_rand_r },
	{ 0 },
	{ 0 },
	{ 0 }
};


#define DRIVER(GAMENAME, SOUND_TYPE, SOUND_INTERFACE)							\
																				\
static struct MachineDriver machine_driver_##GAMENAME =							\
{																				\
	/* basic machine hardware */												\
	{																			\
		{																		\
			CPU_M6502,															\
			12096000/8,	/* 1.512 Mhz (slows down to 0.75MHz while accessing playfield RAM) */	\
			GAMENAME##_readmem,GAMENAME##_writemem,0,0,							\
			centiped_interrupt,4												\
		}																		\
	},																			\
	60, 1460,	/* frames per second, vblank duration */						\
	1,	/* single CPU, no need for interleaving */								\
	centiped_init_machine,														\
																				\
	/* video hardware */														\
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 30*8-1 },									\
	gfxdecodeinfo,																\
	4+4*4, 4+4*4,																\
	0,																			\
																				\
	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY|VIDEO_MODIFIES_PALETTE,				\
	0,																			\
	generic_vh_start,															\
	generic_vh_stop,															\
	centiped_vh_screenrefresh,													\
																				\
	/* sound hardware */														\
	0,0,0,0,																	\
	{																			\
		{																		\
			SOUND_TYPE,															\
			SOUND_INTERFACE,													\
		}																		\
	},																			\
																				\
	atari_vg_earom_handler														\
};


DRIVER(centiped, SOUND_POKEY, &pokey_interface)

/* In the bootlegs the Pokey has been replaced by an AY8910 */
DRIVER(centipdb, SOUND_AY8910, &centipdb_ay8910_interface)
DRIVER(centipb2, SOUND_AY8910, &centipb2_ay8910_interface)


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( centiped )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "centiped.307", 0x2000, 0x0800, 0x5ab0d9de )
	ROM_LOAD( "centiped.308", 0x2800, 0x0800, 0x4c07fd3e )
	ROM_LOAD( "centiped.309", 0x3000, 0x0800, 0xff69b424 )
	ROM_LOAD( "centiped.310", 0x3800, 0x0800, 0x44e40fa4 )
	ROM_RELOAD(               0xf800, 0x0800 )	/* for the reset and interrupt vectors */

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "centiped.211", 0x0000, 0x0800, 0x880acfb9 )
	ROM_LOAD( "centiped.212", 0x0800, 0x0800, 0xb1397029 )
ROM_END

ROM_START( centipd2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "centiped.207", 0x2000, 0x0800, 0xb2909e2f )
	ROM_LOAD( "centiped.208", 0x2800, 0x0800, 0x110e04ff )
	ROM_LOAD( "centiped.209", 0x3000, 0x0800, 0xcc2edb26 )
	ROM_LOAD( "centiped.210", 0x3800, 0x0800, 0x93999153 )
	ROM_RELOAD(               0xf800, 0x0800 )	/* for the reset and interrupt vectors */

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "centiped.211", 0x0000, 0x0800, 0x880acfb9 )
	ROM_LOAD( "centiped.212", 0x0800, 0x0800, 0xb1397029 )
ROM_END

ROM_START( centipdb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "olympia.c28",  0x2000, 0x0800, 0x8a744e57 )
	ROM_LOAD( "olympia.c29",  0x2800, 0x0800, 0xbb897b10 )
	ROM_LOAD( "olympia.c30",  0x3000, 0x0800, 0x2297c2ac )
	ROM_LOAD( "olympia.c31",  0x3800, 0x0800, 0xcc529d6b )
	ROM_RELOAD(               0xf800, 0x0800 )	/* for the reset and interrupt vectors */

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "olympia.c32",  0x0000, 0x0800, 0xd91b9724 )
	ROM_LOAD( "olympia.c33",  0x0800, 0x0800, 0x1a6acd02 )
ROM_END

ROM_START( centipb2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "d1",  		  0x2000, 0x0800, 0xb17b8e0b )
	ROM_LOAD( "e1",  		  0x2800, 0x0800, 0x7684398e )
	ROM_LOAD( "h1",  		  0x3000, 0x0800, 0x74580fe4 )
	ROM_LOAD( "j1",  		  0x3800, 0x0800, 0x84600161 )
	ROM_RELOAD(               0xf800, 0x0800 )	/* for the reset and interrupt vectors */
	ROM_LOAD( "k1",  		  0x6000, 0x0800, 0xf1aa329b )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "centiped.211", 0x0000, 0x0800, 0x880acfb9 )
	ROM_LOAD( "centiped.212", 0x0800, 0x0800, 0xb1397029 )
ROM_END


GAME( 1980, centiped, 0,        centiped, centiped, 0, ROT270, "Atari", "Centipede (revision 3)" )
GAME( 1980, centipd2, centiped, centiped, centiped, 0, ROT270, "Atari", "Centipede (revision 2)" )
GAME( 1980, centipdb, centiped, centipdb, centipdb, 0, ROT270, "bootleg", "Centipede (bootleg set 1)" )
GAME( 1980, centipb2, centiped, centipb2, centiped, 0, ROT270, "bootleg", "Centipede (bootleg set 2)" )
