#include "../machine/mhavoc.cpp"

/***************************************************************************

MAJOR HAVOC (Driver)

started 7 OCT 97 - Mike Appolo
completed by the MAME vector team

Notes:

This game was provided in three configurations:
1) New Machine Purchase
2) Upgrade kit for Tempest (Kit "A")
3) Upgrade kit for Space Duel, Gravitar, and Black Window (Kit "B")

Controls on the machine were:
A backlit cylinder (roller) on new Major Havoc machines
		or
A Tempest-like spinner on upgrades


Memory Map for Major Havoc

Alpha Processor
                 D  D  D  D  D  D  D  D
Hex Address      7  6  5  4  3  2  1  0                    Function
--------------------------------------------------------------------------------
0000-01FF     |  D  D  D  D  D  D  D  D   | R/W  | Program RAM (1/2K)
0200-07FF     |  D  D  D  D  D  D  D  D   | R/W  | Paged Program RAM (3K)
0800-09FF     |  D  D  D  D  D  D  D  D   | R/W  | Program RAM (1/2K)
              |                           |      |
1000	      |  D  D  D  D  D  D  D  D   |  R   | Gamma Commuication Read Port
              |                           |      |
1200          |  D                        |  R   | Right Coin (Player 1=0)
1200	      |     D                     |  R   | Left Coin  (Player 1=0)
1200          |        D                  |  R   | Aux. Coin  (Player 1=0)
1200          |           D               |  R   | Diagnostic Step
1200          |  D                        |  R   | Self Test Switch (Player 1=1)
1200          |     D                     |  R   | Cabinet Switch (Player 1=1)
1200          |        D                  |  R   | Aux. Coin Switch (Player 1=1)
1200          |           D               |  R   | Diagnostic Step
1200          |              D            |  R   | Gammma Rcvd Flag
1200          |                 D         |  R   | Gamma Xmtd Flag
1200          |                    D      |  R   | 2.4 KHz
1200          |                       D   |  R   | Vector Generator Halt Flag
              |                           |      |
1400-141F     |              D  D  D  D   |  W   | ColorRAM
              |                           |      |
1600          |  D                        |  W   | Invert X
1600          |     D                     |  W   | Invert Y
1600          |        D                  |  W   | Player 1
1600          |           D               |  W   | Not Used
1600          |              D            |  W   | Gamma Proc. Reset
1600          |                 D         |  W   | Beta Proc. Reset
1600          |                    D      |  W   | Not Used
1600          |                       D   |  W   | Roller Controller Light
              |                           |      |
1640          |                           |  W   | Vector Generator Go
1680          |                           |  W   | Watchdog Clear
16C0          |                           |  W   | Vector Generator Reset
              |                           |      |
1700          |                           |  W   | IRQ Acknowledge
1740          |                    D  D   |  W   | Program ROM Page Select
1780          |                       D   |  W   | Program RAM Page Select
17C0          |  D  D  D  D  D  D  D  D   |  W   | Gamma Comm. Write Port
              |                           |      |
1800-1FFF     |  D  D  D  D  D  D  D  D   | R/W  | Shared Beta RAM(not used)
              |                           |      |
2000-3FFF     |  D  D  D  D  D  D  D  D   |  R   | Paged Program ROM (32K)
4000-4FFF     |  D  D  D  D  D  D  D  D   | R/W  | Vector Generator RAM (4K)
5000-5FFF     |  D  D  D  D  D  D  D  D   |  R   | Vector Generator ROM (4K)
6000-7FFF     |  D  D  D  D  D  D  D  D   |  R   | Paged Vector ROM (32K)
8000-FFFF     |  D  D  D  D  D  D  D  D   |  R   | Program ROM (32K)
-------------------------------------------------------------------------------

Gamma Processor

                 D  D  D  D  D  D  D  D
Hex Address      7  6  5  4  3  2  1  0                    Function
--------------------------------------------------------------------------------
0000-07FF     |  D  D  D  D  D  D  D  D   | R/W  | Program RAM (2K)
2000-203F     |  D  D  D  D  D  D  D  D   | R/W  | Quad-Pokey I/O
              |                           |      |
2800          |  D                        |  R   | Fire 1 Switch
2800          |     D                     |  R   | Shield 1 Switch
2800          |        D                  |  R   | Fire 2 Switch
2800          |           D               |  R   | Shield 2 Switch
2800          |              D            |  R   | Not Used
2800          |                 D         |  R   | Speech Chip Ready
2800          |                    D      |  R   | Alpha Rcvd Flag
2800          |                       D   |  R   | Alpha Xmtd Flag
              |                           |      |
3000          |  D  D  D  D  D  D  D  D   |  R   | Alpha Comm. Read Port
              |                           |      |
3800-3803     |  D  D  D  D  D  D  D  D   |  R   | Roller Controller Input
              |                           |      |
4000          |                           |  W   | IRQ Acknowledge
4800          |                    D      |  W   | Left Coin Counter
4800          |                       D   |  W   | Right Coin Counter
              |                           |      |
5000          |  D  D  D  D  D  D  D  D   |  W   | Alpha Comm. Write Port
              |                           |      |
6000-61FF     |  D  D  D  D  D  D  D  D   | R/W  | EEROM
8000-BFFF     |  D  D  D  D  D  D  D  D   |  R   | Program ROM (16K)
-----------------------------------------------------------------------------



MAJOR HAVOC DIP SWITCH SETTINGS

$=Default

DIP Switch at position 13/14S

                                  1    2    3    4    5    6    7    8
STARTING LIVES                  _________________________________________
Free Play   1 Coin   2 Coin     |    |    |    |    |    |    |    |    |
    2         3         5      $|Off |Off |    |    |    |    |    |    |
    3         4         4       | On | On |    |    |    |    |    |    |
    4         5         6       | On |Off |    |    |    |    |    |    |
    5         6         7       |Off | On |    |    |    |    |    |    |
GAME DIFFICULTY                 |    |    |    |    |    |    |    |    |
Hard                            |    |    | On | On |    |    |    |    |
Medium                         $|    |    |Off |Off |    |    |    |    |
Easy                            |    |    |Off | On |    |    |    |    |
Demo                            |    |    | On |Off |    |    |    |    |
BONUS LIFE                      |    |    |    |    |    |    |    |    |
50,000                          |    |    |    |    | On | On |    |    |
100,000                        $|    |    |    |    |Off |Off |    |    |
200,000                         |    |    |    |    |Off | On |    |    |
No Bonus Life                   |    |    |    |    | On |Off |    |    |
ATTRACT MODE SOUND              |    |    |    |    |    |    |    |    |
Silence                         |    |    |    |    |    |    | On |    |
Sound                          $|    |    |    |    |    |    |Off |    |
ADAPTIVE DIFFICULTY             |    |    |    |    |    |    |    |    |
No                              |    |    |    |    |    |    |    | On |
Yes                            $|    |    |    |    |    |    |    |Off |
                                -----------------------------------------

	DIP Switch at position 8S

                                  1    2    3    4    5    6    7    8
                                _________________________________________
Free Play                       |    |    |    |    |    |    | On |Off |
1 Coin for 1 Game               |    |    |    |    |    |    |Off |Off |
1 Coin for 2 Games              |    |    |    |    |    |    | On | On |
2 Coins for 1 Game             $|    |    |    |    |    |    |Off | On |
RIGHT COIN MECHANISM            |    |    |    |    |    |    |    |    |
x1                             $|    |    |    |    |Off |Off |    |    |
x4                              |    |    |    |    |Off | On |    |    |
x5                              |    |    |    |    | On |Off |    |    |
x6                              |    |    |    |    | On | On |    |    |
LEFT COIN MECHANISM             |    |    |    |    |    |    |    |    |
x1                             $|    |    |    |Off |    |    |    |    |
x2                              |    |    |    | On |    |    |    |    |
BONUS COIN ADDER                |    |    |    |    |    |    |    |    |
No Bonus Coins                 $|Off |Off |Off |    |    |    |    |    |
Every 4, add 1                  |Off | On |Off |    |    |    |    |    |
Every 4, add 2                  |Off | On | On |    |    |    |    |    |
Every 5, add 1                  | On |Off |Off |    |    |    |    |    |
Every 3, add 1                  | On |Off | On |    |    |    |    |    |
                                -----------------------------------------

	2 COIN MINIMUM OPTION: Short pin 6 @13N to ground.


***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/mathbox.h"
#include "machine/atari_vg.h"
#include "vidhrdw/avgdvg.h"
#include "vidhrdw/vector.h"

void mhavoc_init_machine(void);
int mhavoc_alpha_interrupt (void);
int mhavoc_gamma_interrupt (void);
WRITE_HANDLER( mhavoc_rom_banksel_w );
WRITE_HANDLER( mhavoc_ram_banksel_w );
READ_HANDLER( mhavoc_gamma_r );
WRITE_HANDLER( mhavoc_gamma_w );
READ_HANDLER( mhavoc_alpha_r );
WRITE_HANDLER( mhavoc_alpha_w );
READ_HANDLER( mhavoc_port_0_r );
READ_HANDLER( mhavoc_port_1_r );
WRITE_HANDLER( mhavoc_out_0_w );
WRITE_HANDLER( mhavoc_out_1_w );
WRITE_HANDLER( mhavoc_irqack_w );
WRITE_HANDLER( tempest_colorram_w );



static unsigned char *nvram;
static size_t nvram_size;

static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0xff,nvram_size);
	}
}


static unsigned char *gammaram;

static READ_HANDLER( mhavoc_gammaram_r )
{
	return gammaram[offset & 0x7ff];
}

static WRITE_HANDLER( mhavoc_gammaram_w )
{
	gammaram[offset & 0x7ff] = data;
}


/* Main board Readmem */
static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM },			/* 0.5K Program Ram */
	{ 0x0200, 0x07ff, MRA_BANK1 },			/* 3K Paged Program RAM	*/
	{ 0x0800, 0x09ff, MRA_RAM },			/* 0.5K Program RAM */
	{ 0x1000, 0x1000, mhavoc_gamma_r },		/* Gamma Read Port */
	{ 0x1200, 0x1200, mhavoc_port_0_r },	/* Alpha Input Port 0 */
	{ 0x1800, 0x1FFF, MRA_RAM},				/* Shared Beta Ram */
	{ 0x2000, 0x3fff, MRA_BANK2 },			/* Paged Program ROM (32K) */
	{ 0x4000, 0x4fff, MRA_RAM }, /* Vector RAM	(4K) */
	{ 0x5000, 0x5fff, MRA_ROM },			/* Vector ROM (4K) */
	{ 0x6000, 0x7fff, MRA_BANK3 },			/* Paged Vector ROM (32K) */
	{ 0x8000, 0xffff, MRA_ROM },			/* Program ROM (32K) */
	{ -1 }	/* end of table */
};


/* Main Board Writemem */
static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },			/* 0.5K Program Ram */
	{ 0x0200, 0x07ff, MWA_BANK1 },			/* 3K Paged Program RAM */
	{ 0x0800, 0x09ff, MWA_RAM },			/* 0.5K Program RAM */
	{ 0x1200, 0x1200, MWA_NOP },			/* don't care */
	{ 0x1400, 0x141f, mhavoc_colorram_w },	/* ColorRAM */
	{ 0x1600, 0x1600, mhavoc_out_0_w },		/* Control Signals */
	{ 0x1640, 0x1640, avgdvg_go_w },			/* Vector Generator GO */
	{ 0x1680, 0x1680, MWA_NOP },			/* Watchdog Clear */
	{ 0x16c0, 0x16c0, avgdvg_reset_w },		/* Vector Generator Reset */
	{ 0x1700, 0x1700, MWA_NOP },			/* IRQ ack */
	{ 0x1740, 0x1740, mhavoc_rom_banksel_w },/* Program ROM Page Select */
	{ 0x1780, 0x1780, mhavoc_ram_banksel_w },/* Program RAM Page Select */
	{ 0x17c0, 0x17c0, mhavoc_gamma_w },		/* Gamma Communication Write Port */
	{ 0x1800, 0x1fff, MWA_RAM },			/* Shared Beta Ram */
	{ 0x2000, 0x3fff, MWA_ROM },			/* Major Havoc writes here.*/
	{ 0x4000, 0x4fff, MWA_RAM, &vectorram, &vectorram_size },/* Vector Generator RAM	*/
	{ 0x6000, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};


/* Gamma board readmem */
static struct MemoryReadAddress gamma_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },			/* Program RAM (2K)	*/
	{ 0x0800, 0x1fff, mhavoc_gammaram_r },	/* wraps to 0x000-0x7ff */
	{ 0x2000, 0x203f, quad_pokey_r },		/* Quad Pokey read	*/
	{ 0x2800, 0x2800, mhavoc_port_1_r },	/* Gamma Input Port	*/
	{ 0x3000, 0x3000, mhavoc_alpha_r },		/* Alpha Comm. Read Port*/
	{ 0x3800, 0x3803, input_port_2_r },		/* Roller Controller Input*/
	{ 0x4000, 0x4000, input_port_4_r },		/* DSW at 8S */
	{ 0x6000, 0x61ff, MRA_RAM },			/* EEROM		*/
	{ 0x8000, 0xffff, MRA_ROM },			/* Program ROM (16K)	*/
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress gamma_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },			/* Program RAM (2K)	*/
	{ 0x0800, 0x1fff, mhavoc_gammaram_w, &gammaram },	/* wraps to 0x000-0x7ff */
	{ 0x2000, 0x203f, quad_pokey_w },		/* Quad Pokey write	*/
	{ 0x4000, 0x4000, mhavoc_irqack_w },	/* IRQ Acknowledge	*/
	{ 0x4800, 0x4800, mhavoc_out_1_w },		/* Coin Counters 	*/
	{ 0x5000, 0x5000, mhavoc_alpha_w },		/* Alpha Comm. Write Port */
	{ 0x6000, 0x61ff, MWA_RAM, &nvram, &nvram_size },	/* EEROM		*/
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( mhavoc )
	PORT_START	/* IN0 - alpha (player_1 = 0) */
	PORT_BIT ( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_SERVICE, "Diag Step", KEYCODE_T, IP_JOY_NONE ) */
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diag Step/Coin C", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )	/* Left Coin Switch  */
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )	/* Right Coin */

	PORT_START	/* IN1 - gamma */
	PORT_BIT ( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN2 - gamma */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 100, 40, 0, 0 )

	PORT_START /* DIP Switch at position 13/14S */
	PORT_DIPNAME( 0x01, 0x00, "Adaptive Difficulty" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "50000")
	PORT_DIPSETTING(    0x00, "100000")
	PORT_DIPSETTING(    0x04, "200000")
	PORT_DIPSETTING(    0x08, "None")
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy")
	PORT_DIPSETTING(    0x00, "Medium")
	PORT_DIPSETTING(    0x30, "Hard")
	PORT_DIPSETTING(    0x20, "Demo")
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3 (2 in Free Play)")
	PORT_DIPSETTING(    0xc0, "4 (3 in Free Play)")
	PORT_DIPSETTING(    0x80, "5 (4 in Free Play)")
	PORT_DIPSETTING(    0x40, "6 (5 in Free Play)")

	PORT_START /* DIP Switch at position 8S */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Right Coin Mechanism" )
	PORT_DIPSETTING(    0x0c, "x1" )
	PORT_DIPSETTING(    0x08, "x4" )
	PORT_DIPSETTING(    0x04, "x5" )
	PORT_DIPSETTING(    0x00, "x6" )
	PORT_DIPNAME( 0x10, 0x10, "Left Coin Mechanism" )
	PORT_DIPSETTING(    0x10, "x1" )
	PORT_DIPSETTING(    0x00, "x2" )
	PORT_DIPNAME( 0xe0, 0xe0, "Bonus Credits" )
	PORT_DIPSETTING(    0x80, "2 each 4" )
	PORT_DIPSETTING(    0x40, "1 each 3" )
	PORT_DIPSETTING(    0xa0, "1 each 4" )
	PORT_DIPSETTING(    0x60, "1 each 5" )
	PORT_DIPSETTING(    0xe0, "None" )

	PORT_START	/* IN5 - dummy for player_1 = 1 on alpha */
	PORT_BIT ( 0x3f, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME( 0x40, 0x40, "Credit to start" )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( mhavocp )
	PORT_START	/* IN0 - alpha (player_1 = 0) */
	PORT_BIT ( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_SERVICE, "Diag Step", KEYCODE_T, IP_JOY_NONE ) */
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diag Step/Coin C", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )	/* Left Coin Switch  */
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )	/* Right Coin */

	PORT_START	/* IN1 - gamma */
	PORT_BIT ( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN2 - gamma */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 100, 40, 0, 0 )

	PORT_START /* DIP Switch at position 13/14S */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "50000")
	PORT_DIPSETTING(    0x00, "100000")
	PORT_DIPSETTING(    0x04, "200000")
	PORT_DIPSETTING(    0x08, "None")
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy")
	PORT_DIPSETTING(    0x00, "Medium")
	PORT_DIPSETTING(    0x30, "Hard")
	PORT_DIPSETTING(    0x20, "Demo")
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3 (2 in Free Play)")
	PORT_DIPSETTING(    0xc0, "4 (3 in Free Play)")
	PORT_DIPSETTING(    0x80, "5 (4 in Free Play)")
	PORT_DIPSETTING(    0x40, "6 (5 in Free Play)")

	PORT_START /* DIP Switch at position 8S */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Right Coin Mechanism" )
	PORT_DIPSETTING(    0x0c, "x1" )
	PORT_DIPSETTING(    0x08, "x4" )
	PORT_DIPSETTING(    0x04, "x5" )
	PORT_DIPSETTING(    0x00, "x6" )
	PORT_DIPNAME( 0x10, 0x10, "Left Coin Mechanism" )
	PORT_DIPSETTING(    0x10, "x1" )
	PORT_DIPSETTING(    0x00, "x2" )
	PORT_DIPNAME( 0xe0, 0xe0, "Bonus Credits" )
	PORT_DIPSETTING(    0x80, "2 each 4" )
	PORT_DIPSETTING(    0x40, "1 each 3" )
	PORT_DIPSETTING(    0xa0, "1 each 4" )
	PORT_DIPSETTING(    0x60, "1 each 5" )
	PORT_DIPSETTING(    0xe0, "None" )

	PORT_START	/* IN5 - dummy for player_1 = 1 on alpha */
	PORT_BIT ( 0x3f, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME( 0x40, 0x40, "Credit to start" )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



static struct POKEYinterface pokey_interface =
{
	4,	/* 4 chips */
	1250000,	/* 1.25 MHz??? */
	{ 25, 25, 25, 25 },
	/* The 8 pot handlers */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	/* The allpot handler */
	{ input_port_3_r, 0, 0, 0 },
};



static struct MachineDriver machine_driver_mhavoc =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			2500000,	/* 2.5 Mhz */
			readmem,writemem,0,0,
			interrupt,8 /* 2.4576 milliseconds period */
		},
		{
			CPU_M6502,
			1250000,	/* 1.25 Mhz */
			gamma_readmem,gamma_writemem,0,0,
			0, 0, /* no vblank interrupt */
#if 0		/* HJB has it's own timer in machine/mhavoc now */
			interrupt, 305 /* 3.2768 milliseconds period? */
#endif
		}
	},
	50, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
			/* fps should be 30, but MH draws "empty" frames */
	1,		/* 1 CPU slice. see machine.c */
	mhavoc_init_machine,

	/* video hardware */
	400, 300, { 0, 300, 0, 260 },
	0,
	256,0,
	avg_init_palette_multi,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start_mhavoc,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	nvram_handler
};


/*
 * Notes:
 * the R3 roms are supported as "mhavoc", the R2 roms (with a bug in gameplay)
 * are supported as "mhavoc2".
 * "Return to Vax" - Jess Askey's souped up version (errors on self test)
 * are supported as "mhavocrv".
 * Prototype is supported as "mhavocp"
 */

ROM_START( mhavoc )
	/* Alpha Processor ROMs */
	ROM_REGION( 0x21000, REGION_CPU1 )	/* 152KB for ROMs */
	/* Vector Generator ROM */
	ROM_LOAD( "136025.210",   0x05000, 0x2000, 0xc67284ca )

	/* Program ROM */
	ROM_LOAD( "136025.216",   0x08000, 0x4000, 0x522a9cc0 )
	ROM_LOAD( "136025.217",   0x0c000, 0x4000, 0xea3d6877 )

	/* Paged Program ROM */
	ROM_LOAD( "136025.215",   0x10000, 0x4000, 0xa4d380ca ) /* page 0+1 */
	ROM_LOAD( "136025.318",   0x14000, 0x4000, 0xba935067 ) /* page 2+3 */

	/* Paged Vector Generator ROM */
	ROM_LOAD( "136025.106",   0x18000, 0x4000, 0x2ca83c76 ) /* page 0+1 */
	ROM_LOAD( "136025.107",   0x1c000, 0x4000, 0x5f81c5f3 ) /* page 2+3 */

	/* Gamma Processor ROM */
	ROM_REGION( 0x10000, REGION_CPU2 ) /* 16k for code */
	ROM_LOAD( "136025.108",   0x08000, 0x4000, 0x93faf210 )
	ROM_RELOAD(               0x0c000, 0x4000 ) /* reset+interrupt vectors */
ROM_END

ROM_START( mhavoc2 )
	/* Alpha Processor ROMs */
	ROM_REGION( 0x21000, REGION_CPU1 )
	/* Vector Generator ROM */
	ROM_LOAD( "136025.110",   0x05000, 0x2000, 0x16eef583 )

	/* Program ROM */
	ROM_LOAD( "136025.103",   0x08000, 0x4000, 0xbf192284 )
	ROM_LOAD( "136025.104",   0x0c000, 0x4000, 0x833c5d4e )

	/* Paged Program ROM - switched to 2000-3fff */
	ROM_LOAD( "136025.101",   0x10000, 0x4000, 0x2b3b591f ) /* page 0+1 */
	ROM_LOAD( "136025.109",   0x14000, 0x4000, 0x4d766827 ) /* page 2+3 */

	/* Paged Vector Generator ROM */
	ROM_LOAD( "136025.106",   0x18000, 0x4000, 0x2ca83c76 ) /* page 0+1 */
	ROM_LOAD( "136025.107",   0x1c000, 0x4000, 0x5f81c5f3 ) /* page 2+3 */

	/* the last 0x1000 is used for the 2 RAM pages */

	/* Gamma Processor ROM */
	ROM_REGION( 0x10000, REGION_CPU2 )	/* 16k for code */
	ROM_LOAD( "136025.108",   0x08000, 0x4000, 0x93faf210 )
	ROM_RELOAD(               0x0c000, 0x4000 ) /* reset+interrupt vectors */
ROM_END

ROM_START( mhavocrv )
	/* Alpha Processor ROMs */
	ROM_REGION( 0x21000, REGION_CPU1 )	/* 152KB for ROMs */
	/* Vector Generator ROM */
	ROM_LOAD( "136025.210",   0x05000, 0x2000, 0xc67284ca )

	/* Program ROM */
	ROM_LOAD( "136025.916",   0x08000, 0x4000, 0x1255bd7f )
	ROM_LOAD( "136025.917",   0x0c000, 0x4000, 0x21889079 )

	/* Paged Program ROM */
	ROM_LOAD( "136025.915",   0x10000, 0x4000, 0x4c7235dc ) /* page 0+1 */
	ROM_LOAD( "136025.918",   0x14000, 0x4000, 0x84735445 ) /* page 2+3 */

	/* Paged Vector Generator ROM */
	ROM_LOAD( "136025.106",   0x18000, 0x4000, 0x2ca83c76 ) /* page 0+1 */
	ROM_LOAD( "136025.907",   0x1c000, 0x4000, 0x4deea2c9 ) /* page 2+3 */

	/* Gamma Processor ROM */
	ROM_REGION( 0x10000, REGION_CPU2 ) /* 16k for code */
	ROM_LOAD( "136025.908",   0x08000, 0x4000, 0xc52ec664 )
	ROM_RELOAD(               0x0c000, 0x4000 ) /* reset+interrupt vectors */
ROM_END

ROM_START( mhavocp )
	/* Alpha Processor ROMs */
	ROM_REGION( 0x21000, REGION_CPU1 )
	/* Vector Generator ROM */
	ROM_LOAD( "136025.010",   0x05000, 0x2000, 0x3050c0e6 )

	/* Program ROM */
	ROM_LOAD( "136025.016",   0x08000, 0x4000, 0x94caf6c0 )
	ROM_LOAD( "136025.017",   0x0c000, 0x4000, 0x05cba70a )

	/* Paged Program ROM - switched to 2000-3fff */
	ROM_LOAD( "136025.015",   0x10000, 0x4000, 0xc567c11b )
	ROM_LOAD( "136025.018",   0x14000, 0x4000, 0xa8c35ccd )

	/* Paged Vector Generator ROM */
	ROM_LOAD( "136025.006",   0x18000, 0x4000, 0xe272ed41 )
	ROM_LOAD( "136025.007",   0x1c000, 0x4000, 0xe152c9d8 )

	/* the last 0x1000 is used for the 2 RAM pages */

	/* Gamma Processor ROM */
	ROM_REGION( 0x10000, REGION_CPU2 ) /* 16k for code */
	ROM_LOAD( "136025.008",   0x8000, 0x4000, 0x22ea7399 )
	ROM_RELOAD(               0xc000, 0x4000 )/* reset+interrupt vectors */
ROM_END



GAME( 1983, mhavoc,   0,      mhavoc, mhavoc,  0, ROT0, "Atari", "Major Havoc (rev 3)" )
GAME( 1983, mhavoc2,  mhavoc, mhavoc, mhavoc,  0, ROT0, "Atari", "Major Havoc (rev 2)" )
GAME( 1983, mhavocrv, mhavoc, mhavoc, mhavoc,  0, ROT0, "hack", "Major Havoc (Return to Vax)" )
GAME( 1983, mhavocp,  mhavoc, mhavoc, mhavocp, 0, ROT0, "Atari", "Major Havoc (prototype)" )
