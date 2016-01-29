#include "../sndhrdw/bzone.cpp"

/***************************************************************************

Battlezone memory map (preliminary)

0000-04ff RAM
0800      IN0
0a00      IN1
0c00      IN2

1200      Vector generator start (write)
1400
1600      Vector generator reset (write)

1800      Mathbox Status register
1810      Mathbox value (lo-byte)
1818      Mathbox value (hi-byte)
1820-182f POKEY I/O
1828      Control inputs
1860-187f Mathbox RAM

2000-2fff Vector generator RAM
3000-37ff Mathbox ROM
5000-7fff ROM

Battlezone settings:

0 = OFF  1 = ON  X = Don't Care  $ = Atari suggests

** IMPORTANT - BITS are INVERTED in the game itself **

TOP 8 SWITCH DIP
87654321
--------
XXXXXX11   Free Play
XXXXXX10   1 coin for 2 plays
XXXXXX01   1 coin for 1 play
XXXXXX00   2 coins for 1 play
XXXX11XX   Right coin mech x 1
XXXX10XX   Right coin mech x 4
XXXX01XX   Right coin mech x 5
XXXX00XX   Right coin mech x 6
XXX1XXXX   Center (or Left) coin mech x 1
XXX0XXXX   Center (or Left) coin mech x 2
111XXXXX   No bonus coin
110XXXXX   For every 2 coins inserted, game logic adds 1 more
101XXXXX   For every 4 coins inserted, game logic adds 1 more
100XXXXX   For every 4 coins inserted, game logic adds 2 more
011XXXXX   For every 5 coins inserted, game logic adds 1 more

BOTTOM 8 SWITCH DIP
87654321
--------
XXXXXX11   Game starts with 2 tanks
XXXXXX10   Game starts with 3 tanks  $
XXXXXX01   Game starts with 4 tanks
XXXXXX00   Game starts with 5 tanks
XXXX11XX   Missile appears after 5,000 points
XXXX10XX   Missile appears after 10,000 points  $
XXXX01XX   Missile appears after 20,000 points
XXXX00XX   Missile appears after 30,000 points
XX11XXXX   No bonus tank
XX10XXXX   Bonus taks at 15,000 and 100,000 points  $
XX01XXXX   Bonus taks at 20,000 and 100,000 points
XX00XXXX   Bonus taks at 50,000 and 100,000 points
11XXXXXX   English language
10XXXXXX   French language
01XXXXXX   German language
00XXXXXX   Spanish language

4 SWITCH DIP

XX11   All coin mechanisms register on one coin counter
XX01   Left and center coin mechanisms on one coin counter, right on second
XX10   Center and right coin mechanisms on one coin counter, left on second
XX00   Each coin mechanism has it's own counter


++++++++++++++++++++++++++



Red Baron memory map (preliminary)

0000-04ff RAM
0800      COIN_IN
0a00      IN1
0c00      IN2

1200      Vector generator start (write)
1400
1600      Vector generator reset (write)

1800      Mathbox Status register
1802      Button inputs
1804      Mathbox value (lo-byte)
1806      Mathbox value (hi-byte)
1808      Red Baron Sound (bit 1 selects joystick pot to read also)
1810-181f POKEY I/O
1818      Joystick inputs
1860-187f Mathbox RAM

2000-2fff Vector generator RAM
3000-37ff Mathbox ROM
5000-7fff ROM

RED BARON DIP SWITCH SETTINGS
Donated by Dana Colbert


$=Default
"K" = 1,000

Switch at position P10
                                  8    7    6    5    4    3    2    1
                                _________________________________________
English                        $|    |    |    |    |    |    |Off |Off |
Spanish                         |    |    |    |    |    |    |Off | On |
French                          |    |    |    |    |    |    | On |Off |
German                          |    |    |    |    |    |    | On | On |
                                |    |    |    |    |    |    |    |    |
 Bonus airplane granted at:     |    |    |    |    |    |    |    |    |
Bonus at 2K, 10K and 30K        |    |    |    |    |Off |Off |    |    |
Bonus at 4K, 15K and 40K       $|    |    |    |    |Off | On |    |    |
Bonus at 6K, 20K and 50K        |    |    |    |    | On |Off |    |    |
No bonus airplanes              |    |    |    |    | On | On |    |    |
                                |    |    |    |    |    |    |    |    |
2 aiplanes per game             |    |    |Off |Off |    |    |    |    |
3 airplanes per game           $|    |    |Off | On |    |    |    |    |
4 airplanes per game            |    |    | On |Off |    |    |    |    |
5 airplanes per game            |    |    | On | On |    |    |    |    |
                                |    |    |    |    |    |    |    |    |
1-play minimum                 $|    |Off |    |    |    |    |    |    |
2-play minimum                  |    | On |    |    |    |    |    |    |
                                |    |    |    |    |    |    |    |    |
Self-adj. game difficulty: on  $|Off |    |    |    |    |    |    |    |
Self-adj. game difficulty: off  | On |    |    |    |    |    |    |    |
                                -----------------------------------------

  If self-adjusting game difficulty feature is
turned on, the program strives to maintain the
following average game lengths (in seconds):

                                        Airplanes per game:
     Bonus airplane granted at:          2   3     4     5
2,000, 10,000 and 30,000 points         90  105$  120   135
4,000, 15,000 and 40,000 points         75   90   105   120
6,000, 20,000 and 50,000 points         60   75    90   105
             No bonus airplanes         45   60    75    90



Switch at position M10
                                  8    7    6    5    4    3    2    1
                                _________________________________________
    50  PER PLAY                |    |    |    |    |    |    |    |    |
 Straight 25  Door:             |    |    |    |    |    |    |    |    |
No Bonus Coins                  |Off |Off |Off |Off |Off |Off | On | On |
Bonus $1= 3 plays               |Off | On | On |Off |Off |Off | On | On |
Bonus $1= 3 plays, 75 = 2 plays |Off |Off | On |Off |Off |Off | On | On |
                                |    |    |    |    |    |    |    |    |
 25 /$1 Door or 25 /25 /$1 Door |    |    |    |    |    |    |    |    |
No Bonus Coins                  |Off |Off |Off |Off |Off | On | On | On |
Bonus $1= 3 plays               |Off | On | On |Off |Off | On | On | On |
Bonus $1= 3 plays, 75 = 2 plays |Off |Off | On |Off |Off | On | On | On |
                                |    |    |    |    |    |    |    |    |
    25  PER PLAY                |    |    |    |    |    |    |    |    |
 Straight 25  Door:             |    |    |    |    |    |    |    |    |
No Bonus Coins                  |Off |Off |Off |Off |Off |Off | On |Off |
Bonus 50 = 3 plays              |Off |Off | On |Off |Off |Off | On |Off |
Bonus $1= 5 plays               |Off | On |Off |Off |Off |Off | On |Off |
                                |    |    |    |    |    |    |    |    |
 25 /$1 Door or 25 /25 /$1 Door |    |    |    |    |    |    |    |    |
No Bonus Coins                  |Off |Off |Off |Off |Off | On | On |Off |
Bonus 50 = 3 plays              |Off |Off | On |Off |Off | On | On |Off |
Bonus $1= 5 plays               |Off | On |Off |Off |Off | On | On |Off |
                                -----------------------------------------

Switch at position L11
                                                      1    2    3    4
                                                    _____________________
All 3 mechs same denomination                       | On | On |    |    |
Left and Center same, right different denomination  | On |Off |    |    |
Right and Center same, left differnnt denomination  |Off | On |    |    |
All different denominations                         |Off |Off |    |    |
                                                    ---------------------

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"
#include "machine/mathbox.h"
#include "machine/atari_vg.h"

#define IN0_3KHZ (1<<7)
#define IN0_VG_HALT (1<<6)

WRITE_HANDLER( redbaron_sounds_w );
WRITE_HANDLER( bzone_sounds_w );
int bzone_sh_start(const struct MachineSound *msound);
void bzone_sh_stop(void);
void bzone_sh_update(void);
int redbaron_sh_start(const struct MachineSound *msound);
void redbaron_sh_stop(void);
void redbaron_sh_update(void);

READ_HANDLER( bzone_IN0_r )
{
	int res;

	res = readinputport(0);

	if (cpu_gettotalcycles() & 0x100)
		res |= IN0_3KHZ;
	else
		res &= ~IN0_3KHZ;

	if (avgdvg_done())
		res |= IN0_VG_HALT;
	else
		res &= ~IN0_VG_HALT;

	return res;
}

/* Translation table for one-joystick emulation */
static int one_joy_trans[32]={
        0x00,0x0A,0x05,0x00,0x06,0x02,0x01,0x00,
        0x09,0x08,0x04,0x00,0x00,0x00,0x00,0x00 };

static READ_HANDLER( bzone_IN3_r )
{
	int res,res1;

	res=readinputport(3);
	res1=readinputport(4);

	res|=one_joy_trans[res1&0x1f];

	return (res);
}

static int bzone_interrupt(void)
{
	if (readinputport(0) & 0x10)
		return nmi_interrupt();
	else
		return ignore_interrupt();
}

int rb_input_select;

static READ_HANDLER( redbaron_joy_r )
{
	if (rb_input_select)
		return readinputport (5);
	else
		return readinputport (6);
}


static struct MemoryReadAddress bzone_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0800, 0x0800, bzone_IN0_r },    /* IN0 */
	{ 0x0a00, 0x0a00, input_port_1_r },	/* DSW1 */
	{ 0x0c00, 0x0c00, input_port_2_r },	/* DSW2 */
	{ 0x1800, 0x1800, mb_status_r },
	{ 0x1810, 0x1810, mb_lo_r },
	{ 0x1818, 0x1818, mb_hi_r },
	{ 0x1820, 0x182f, pokey1_r },
	{ 0x2000, 0x2fff, MRA_RAM },
	{ 0x3000, 0x3fff, MRA_ROM },
	{ 0x5000, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },        /* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bzone_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x1000, 0x1000, coin_counter_w },
	{ 0x1200, 0x1200, avgdvg_go_w },
	{ 0x1400, 0x1400, watchdog_reset_w },
	{ 0x1600, 0x1600, avgdvg_reset_w },
	{ 0x1820, 0x182f, pokey1_w },
	{ 0x1840, 0x1840, bzone_sounds_w },
	{ 0x1860, 0x187f, mb_go_w },
	{ 0x2000, 0x2fff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x3000, 0x3fff, MWA_ROM },
	{ 0x5000, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( bzone )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1)
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2)
	PORT_BIT ( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	/* bit 6 is the VG HALT bit. We set it to "low" */
	/* per default (busy vector processor). */
 	/* handled by bzone_IN0_r() */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* bit 7 is tied to a 3khz clock */
 	/* handled by bzone_IN0_r() */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME(0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING (  0x00, "2" )
	PORT_DIPSETTING (  0x01, "3" )
	PORT_DIPSETTING (  0x02, "4" )
	PORT_DIPSETTING (  0x03, "5" )
	PORT_DIPNAME(0x0c, 0x04, "Missile appears at" )
	PORT_DIPSETTING (  0x00, "5000" )
	PORT_DIPSETTING (  0x04, "10000" )
	PORT_DIPSETTING (  0x08, "20000" )
	PORT_DIPSETTING (  0x0c, "30000" )
	PORT_DIPNAME(0x30, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING (  0x10, "15k and 100k" )
	PORT_DIPSETTING (  0x20, "20k and 100k" )
	PORT_DIPSETTING (  0x30, "50k and 100k" )
	PORT_DIPSETTING (  0x00, "None" )
	PORT_DIPNAME(0xc0, 0x00, "Language" )
	PORT_DIPSETTING (  0x00, "English" )
	PORT_DIPSETTING (  0x40, "German" )
	PORT_DIPSETTING (  0x80, "French" )
	PORT_DIPSETTING (  0xc0, "Spanish" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (  0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (  0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (  0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x04, "*4" )
	PORT_DIPSETTING (  0x08, "*5" )
	PORT_DIPSETTING (  0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x10, "*2" )
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING (  0x00, "None" )
	PORT_DIPSETTING (  0x20, "3 credits/2 coins" )
	PORT_DIPSETTING (  0x40, "5 credits/4 coins" )
	PORT_DIPSETTING (  0x60, "6 credits/4 coins" )
	PORT_DIPSETTING (  0x80, "6 credits/5 coins" )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP   | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN  | IPF_2WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP    | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* fake port for single joystick control */
	/* This fake port is handled via bzone_IN3_r */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
INPUT_PORTS_END


static struct MemoryReadAddress redbaron_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0800, 0x0800, bzone_IN0_r },    /* IN0 */
	{ 0x0a00, 0x0a00, input_port_1_r },	/* DSW1 */
	{ 0x0c00, 0x0c00, input_port_2_r },	/* DSW2 */
	{ 0x1800, 0x1800, mb_status_r },
	{ 0x1802, 0x1802, input_port_4_r },	/* IN4 */
	{ 0x1804, 0x1804, mb_lo_r },
	{ 0x1806, 0x1806, mb_hi_r },
	{ 0x1810, 0x181f, pokey1_r },
	{ 0x1820, 0x185f, atari_vg_earom_r },
	{ 0x2000, 0x2fff, MRA_RAM },
	{ 0x3000, 0x3fff, MRA_ROM },
	{ 0x5000, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },        /* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress redbaron_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x1000, 0x1000, MWA_NOP },			/* coin out */
	{ 0x1200, 0x1200, avgdvg_go_w },
	{ 0x1400, 0x1400, MWA_NOP },			/* watchdog clear */
	{ 0x1600, 0x1600, avgdvg_reset_w },
	{ 0x1808, 0x1808, redbaron_sounds_w },	/* and select joystick pot also */
	{ 0x180a, 0x180a, MWA_NOP },			/* sound reset, yet todo */
	{ 0x180c, 0x180c, atari_vg_earom_ctrl_w },
	{ 0x1810, 0x181f, pokey1_w },
	{ 0x1820, 0x185f, atari_vg_earom_w },
	{ 0x1860, 0x187f, mb_go_w },
	{ 0x2000, 0x2fff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x3000, 0x3fff, MWA_ROM },
	{ 0x5000, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( redbaron )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1)
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2)
	PORT_BIT ( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	/* bit 6 is the VG HALT bit. We set it to "low" */
	/* per default (busy vector processor). */
 	/* handled by bzone_IN0_r() */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* bit 7 is tied to a 3khz clock */
 	/* handled by bzone_IN0_r() */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	/* See the table above if you are really interested */
	PORT_DIPNAME(0xff, 0xfd, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0xfd, "Normal" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x03, "Language" )
	PORT_DIPSETTING (  0x00, "German" )
	PORT_DIPSETTING (  0x01, "French" )
	PORT_DIPSETTING (  0x02, "Spanish" )
	PORT_DIPSETTING (  0x03, "English" )
	PORT_DIPNAME(0x0c, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING (  0x0c, "2k 10k 30k" )
	PORT_DIPSETTING (  0x08, "4k 15k 40k" )
	PORT_DIPSETTING (  0x04, "6k 20k 50k" )
	PORT_DIPSETTING (  0x00, "None" )
	PORT_DIPNAME(0x30, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING (  0x30, "2" )
	PORT_DIPSETTING (  0x20, "3" )
	PORT_DIPSETTING (  0x10, "4" )
	PORT_DIPSETTING (  0x00, "5" )
	PORT_DIPNAME(0x40, 0x40, "One Play Minimum" )
	PORT_DIPSETTING (  0x40, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x00, DEF_STR( On ) )
	PORT_DIPNAME(0x80, 0x80, "Self Adjust Diff" )
	PORT_DIPSETTING (  0x80, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x00, DEF_STR( On ) )

	/* IN3 - the real machine reads either the X or Y axis from this port */
	/* Instead, we use the two fake 5 & 6 ports and bank-switch the proper */
	/* value based on the lsb of the byte written to the sound port */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )

	PORT_START	/* IN4 - misc controls */
	PORT_BIT( 0x3f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )

	/* These 2 are fake - they are bank-switched from reads to IN3 */
	/* Red Baron doesn't seem to use the full 0-255 range. */
	PORT_START	/* IN5 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 25, 10, 64, 192 )

	PORT_START	/* IN6 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y, 25, 10, 64, 192 )
INPUT_PORTS_END



static struct POKEYinterface bzone_pokey_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz??? */
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
	{ bzone_IN3_r },
};

static struct CustomSound_interface bzone_custom_interface =
{
	bzone_sh_start,
	bzone_sh_stop,
	bzone_sh_update
};


static struct MachineDriver machine_driver_bzone =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			bzone_readmem,bzone_writemem,0,0,
			bzone_interrupt,6 /* 4.1ms */
		}
	},
	40, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 580, 0, 400 },
	0,
	256+32768, 0,
	avg_init_palette_bzone,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start_bzone,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&bzone_pokey_interface
		},
		{
			SOUND_CUSTOM,
			&bzone_custom_interface
		}
	}

};


static struct POKEYinterface redbaron_pokey_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz??? */
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
	{ redbaron_joy_r },
};


static struct CustomSound_interface redbaron_custom_interface =
{
	redbaron_sh_start,
	redbaron_sh_stop,
	redbaron_sh_update
};

static struct MachineDriver machine_driver_redbaron =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			redbaron_readmem,redbaron_writemem,0,0,
			bzone_interrupt,4 /* 5.4ms */
		}
	},
	45, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 520, 0, 400 },
	0,
	256, 0,
	avg_init_palette_aqua,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start_redbaron,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&redbaron_pokey_interface
		},
		{
			SOUND_CUSTOM,
			&redbaron_custom_interface
		}
	},

	atari_vg_earom_handler
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( bzone )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "036414.01",  0x5000, 0x0800, 0xefbc3fa0 )
	ROM_LOAD( "036413.01",  0x5800, 0x0800, 0x5d9d9111 )
	ROM_LOAD( "036412.01",  0x6000, 0x0800, 0xab55cbd2 )
	ROM_LOAD( "036411.01",  0x6800, 0x0800, 0xad281297 )
	ROM_LOAD( "036410.01",  0x7000, 0x0800, 0x0b7bfaa4 )
	ROM_LOAD( "036409.01",  0x7800, 0x0800, 0x1e14e919 )
	ROM_RELOAD(             0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "036422.01",  0x3000, 0x0800, 0x7414177b )
	ROM_LOAD( "036421.01",  0x3800, 0x0800, 0x8ea8f939 )
ROM_END

ROM_START( bzone2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "036414a.01", 0x5000, 0x0800, 0x13de36d5 )
	ROM_LOAD( "036413.01",  0x5800, 0x0800, 0x5d9d9111 )
	ROM_LOAD( "036412.01",  0x6000, 0x0800, 0xab55cbd2 )
	ROM_LOAD( "036411.01",  0x6800, 0x0800, 0xad281297 )
	ROM_LOAD( "036410.01",  0x7000, 0x0800, 0x0b7bfaa4 )
	ROM_LOAD( "036409.01",  0x7800, 0x0800, 0x1e14e919 )
	ROM_RELOAD(             0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "036422.01",  0x3000, 0x0800, 0x7414177b )
	ROM_LOAD( "036421.01",  0x3800, 0x0800, 0x8ea8f939 )
ROM_END

ROM_START( redbaron )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "037587.01",  0x4800, 0x0800, 0x60f23983 )
	ROM_CONTINUE(           0x5800, 0x0800 )
	ROM_LOAD( "037000.01e", 0x5000, 0x0800, 0x69bed808 )
	ROM_LOAD( "036998.01e", 0x6000, 0x0800, 0xd1104dd7 )
	ROM_LOAD( "036997.01e", 0x6800, 0x0800, 0x7434acb4 )
	ROM_LOAD( "036996.01e", 0x7000, 0x0800, 0xc0e7589e )
	ROM_LOAD( "036995.01e", 0x7800, 0x0800, 0xad81d1da )
	ROM_RELOAD(             0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "037006.01e", 0x3000, 0x0800, 0x9fcffea0 )
	ROM_LOAD( "037007.01e", 0x3800, 0x0800, 0x60250ede )
ROM_END



GAME( 1980, bzone,    0,     bzone,    bzone,    0, ROT0, "Atari", "Battle Zone (set 1)" )
GAME( 1980, bzone2,   bzone, bzone,    bzone,    0, ROT0, "Atari", "Battle Zone (set 2)" )
GAME( 1980, redbaron, 0,     redbaron, redbaron, 0, ROT0, "Atari", "Red Baron" )
