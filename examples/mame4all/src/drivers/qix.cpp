#include "../machine/qix.cpp"
#include "../vidhrdw/qix.cpp"

/***************************************************************************

Qix/ZooKeeper/Space Dungeon
------------- ------ ---

driver by John Butler, Ed Mueller, Aaron Giles


Qix uses two 6809 CPUs:  one for data and sound and the other for video.
Communication between the two CPUs is done using a 4K RAM space at $8000
(for ZooKeeper the data cpu maps it at $0000 and the video cpu at $8000)
which both CPUs have direct access.  FIRQs (fast interrupts) are generated
by each CPU to interrupt the other at specific times.

A third CPU, a 6802, is used for sample playback.  It drives an 8-bit
DAC and according to the schematics a TMS5220 speech chip, which is never
accessed.  ROM u27 is the only code needed.  A sound command from the
data CPU causes an IRQ to fire on the 6802 and the sound playback is
started.

The coin door switches and player controls are connected to the CPUs by
Mototola 6821 PIAs.  These devices are memory mapped as shown below.

The screen is 256x256 with eight bit pixels (64K).  The screen is divided
into two halves each half mapped by the video CPU at $0000-$7FFF.  The
high order bit of the address latch at $9402 specifies which half of the
screen is being accessed.

Timing is critical in the hardware.  The data CPU must have an interrupt
signal generated externally at the right frequency to make the game play
correctly.

The address latch works as follows.  When the video CPU accesses $9400,
the screen address is computed by using the values at $9402 (high byte)
and $9403 (low byte) to get a value between $0000-$FFFF.  The value at
that location is either returned or written.

The scan line at $9800 on the video CPU records where the scan line is
on the display (0-255).  Several places in the ROM code wait until the
scan line reaches zero before continuing.

QIX CPU #1 (Data/Sound):
    $8000 - $83FF:  Dual port RAM accessible by both processors
    $8400 - $87FF:  Local Memory
    $8800        :  ACIA base address
    $8C00        :  Video FIRQ activation
    $8C01        :  Data FIRQ deactivation
    $9000        :  Sound PIA
    $9400        :  [76543210] Game PIA 1 (Port A)
                     o         Fast draw
                      o        1P button
                       o       2P button
                        o      Slow draw
                         o     Joystick Left
                          o    Joystick Down
                           o   Joystick Right
                            o  Joystick Up
    $9402        :  [76543210] Game PIA 1 (Port B)
                     o         Tilt
                      o        Coin sw      Unknown
                       o       Right coin
                        o      Left coin
                         o     Slew down
                          o    Slew up
                           o   Sub. test
                            o  Adv. test
    $9900        :  Game PIA 2
    $9C00        :  Game PIA 3

ZOOKEEPER CPU #1 (Data/Sound):
	$0000 - $03FF:  Dual Port RAM accessible by both processors
    $0400 - $07FF:  Local Memory
    $0800        :  ACIA base address
    $0C00        :  Video FIRQ activation
    $0C01        :  Data FIRQ deactivation
    $1000        :  Sound PIA
    $1400        :  [76543210] Game PIA 1 (Port A)
                     o         Fast draw
                      o        1P button
                       o       2P button
                        o      Slow draw
                         o     Joystick Left
                          o    Joystick Down
                           o   Joystick Right
                            o  Joystick Up
    $1402        :  [76543210] Game PIA 1 (Port B)
                     o         Tilt
                      o        Coin sw      Unknown
                       o       Right coin
                        o      Left coin
                         o     Slew down
                          o    Slew up
                           o   Sub. test
                            o  Adv. test
    $1900        :  Game PIA 2
    $1C00        :  Game PIA 3

CPU #2 (Video):
    $0000 - $7FFF:  Video/Screen RAM
    $8000 - $83FF:  Dual port RAM accessible by both processors
    $8400 - $87FF:  CMOS backup and local memory
    $8800        :  LED output and color RAM page select
    $8801		 :  EPROM page select (ZooKeeper only)
    $8C00        :  Data FIRQ activation
    $8C01        :  Video FIRQ deactivation
    $9000        :  Color RAM
    $9400        :  Address latch screen location
    $9402        :  Address latch Hi-byte
    $9403        :  Address latch Lo-byte
    $9800        :  Scan line location
    $9C00        :  CRT controller base address

QIX NONVOLATILE CMOS MEMORY MAP (CPU #2 -- Video) $8400-$87ff
	$86A9 - $86AA:	When CMOS is valid, these bytes are $55AA
	$86AC - $86C3:	AUDIT TOTALS -- 4 4-bit BCD digits per setting
					(All totals default to: 0000)
					$86AC: TOTAL PAID CREDITS
					$86AE: LEFT COINS
					$86B0: CENTER COINS
					$86B2: RIGHT COINS
					$86B4: PAID CREDITS
					$86B6: AWARDED CREDITS
					$86B8: % FREE PLAYS
					$86BA: MINUTES PLAYED
					$86BC: MINUTES AWARDED
					$86BE: % FREE TIME
					$86C0: AVG. GAME [SEC]
					$86C2: HIGH SCORES
	$86C4 - $86FF:	High scores -- 10 scores/names, consecutive in memory
					Six 4-bit BCD digits followed by 3 ascii bytes
					(Default: 030000 QIX)
	$8700		 :	LANGUAGE SELECT (Default: $32)
					ENGLISH = $32  FRANCAIS = $33  ESPANOL = $34  DEUTSCH = $35
	$87D9 - $87DF:	COIN SLOT PROGRAMMING -- 2 4-bit BCD digits per setting
					$87D9: STANDARD COINAGE SETTING  (Default: 01)
					$87DA: COIN MULTIPLIERS LEFT (Default: 01)
					$87DB: COIN MULTIPLIERS CENTER (Default: 04)
					$87DC: COIN MULTIPLIERS RIGHT (Default: 01)
					$87DD: COIN UNITS FOR CREDIT (Default: 01)
					$87DE: COIN UNITS FOR BONUS (Default: 00)
					$87DF: MINIMUM COINS (Default: 00)
	$87E0 - $87EA:	LOCATION PROGRAMMING -- 2 4-bit BCD digits per setting
					$87E0: BACKUP HSTD [0000] (Default: 03)
					$87E1: MAXIMUM CREDITS (Default: 10)
					$87E2: NUMBER OF TURNS (Default: 03)
					$87E3: THRESHOLD (Default: 75)
					$87E4: TIME LINE (Default: 37)
					$87E5: DIFFICULTY 1 (Default: 01)
					$87E6: DIFFICULTY 2 (Default: 01)
					$87E7: DIFFICULTY 3 (Default: 01)
					$87E8: DIFFICULTY 4 (Default: 01)
					$87E9: ATTRACT SOUND (Default: 01)
					$87EA: TABLE MODE (Default: 00)

COIN PROCESSOR

$0000 (PORTA)  : Bi directional communication to the data CPU
$0001 (PORTB)  :  [76543210] Game PIA 1 (Port B)
                   o         SPARE          output
                    o        Coin lockout   output
                     o       Coin lockout   output
                      o      Tilt           input
                       o     Slew down      input
                        o    Slew up        input
                         o   Sub. test      input
                          o  Adv. test      input

$0002 (PORTC)  :  [76543210] Game PIA 1 (Port B)
                       o     From DATA cpu      input
                        o    Aux coin switch    input
                         o   Right coin switch  input
                          o  Left coin switch   input

INT input   : From data cpu
Timer input : From data cpu

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/6821pia.h"


READ_HANDLER( sdungeon_68705_portA_r );
READ_HANDLER( sdungeon_68705_portB_r );
READ_HANDLER( sdungeon_68705_portC_r );
WRITE_HANDLER( sdungeon_68705_portA_w );
WRITE_HANDLER( sdungeon_68705_portB_w );
WRITE_HANDLER( sdungeon_68705_portC_w );
WRITE_HANDLER( sdungeon_68705_ddrA_w );
WRITE_HANDLER( sdungeon_68705_ddrB_w );
WRITE_HANDLER( sdungeon_68705_ddrC_w );
WRITE_HANDLER( sdungeon_pia_0_w );

extern unsigned char *qix_sharedram;
READ_HANDLER( qix_scanline_r );
WRITE_HANDLER( qix_data_firq_w );
WRITE_HANDLER( qix_video_firq_w );


extern unsigned char *qix_palettebank;
extern unsigned char *qix_videoaddress;

READ_HANDLER( qix_videoram_r );
WRITE_HANDLER( qix_videoram_w );
READ_HANDLER( qix_addresslatch_r );
WRITE_HANDLER( qix_addresslatch_w );
WRITE_HANDLER( qix_paletteram_w );
WRITE_HANDLER( qix_palettebank_w );

READ_HANDLER( qix_sharedram_r );
WRITE_HANDLER( qix_sharedram_w );
int qix_interrupt_video(void);
void qix_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int qix_vh_start(void);
void qix_vh_stop(void);
void qix_init_machine(void);
void withmcu_init_machine(void);

READ_HANDLER( qix_data_io_r );
READ_HANDLER( qix_sound_io_r );
WRITE_HANDLER( qix_data_io_w );
WRITE_HANDLER( qix_sound_io_w );

WRITE_HANDLER( zoo_bankswitch_w );
extern void zoo_init_machine(void);



static unsigned char *nvram;
static size_t nvram_size;

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0,nvram_size);
	}
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x8000, 0x83ff, qix_sharedram_r },
	{ 0x8400, 0x87ff, MRA_RAM },
	{ 0x8800, 0x8800, MRA_RAM },   /* ACIA */
	{ 0x9000, 0x9003, pia_3_r },
	{ 0x9400, 0x9403, pia_0_r },
	{ 0x9900, 0x9903, pia_1_r },
	{ 0x9c00, 0x9FFF, pia_2_r },
	{ 0xa000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress zoo_readmem[] =
{
	{ 0x0000, 0x03ff, qix_sharedram_r },
	{ 0x0400, 0x07ff, MRA_RAM },
	{ 0x1000, 0x1003, pia_3_r },	/* Sound PIA */
	{ 0x1400, 0x1403, pia_0_r },	/* Game PIA 1 - Player inputs, coin door switches */
	{ 0x1900, 0x1903, pia_1_r },	/* Game PIA 2 */
	{ 0x1c00, 0x1fff, pia_2_r },    /* Game PIA 3 - Player 2 */
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress readmem_video[] =
{
	{ 0x0000, 0x7fff, qix_videoram_r },
	{ 0x8000, 0x83ff, qix_sharedram_r },
	{ 0x8400, 0x87ff, MRA_RAM },
	{ 0x9400, 0x9400, qix_addresslatch_r },
	{ 0x9800, 0x9800, qix_scanline_r },
	{ 0xa000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress zoo_readmem_video[] =
{
	{ 0x0000, 0x7fff, qix_videoram_r },
	{ 0x8000, 0x83ff, qix_sharedram_r },
	{ 0x8400, 0x87ff, MRA_RAM },
	{ 0x9400, 0x9400, qix_addresslatch_r },
	{ 0x9800, 0x9800, qix_scanline_r },
	{ 0xa000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0x2000, 0x2003, pia_5_r },
	{ 0x4000, 0x4003, pia_4_r },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress zoo_readmem_sound[] =
{
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0x2000, 0x2003, pia_5_r },
	{ 0x4000, 0x4003, pia_4_r },
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x8000, 0x83ff, qix_sharedram_w, &qix_sharedram },
	{ 0x8400, 0x87ff, MWA_RAM },
	{ 0x8c00, 0x8c00, qix_video_firq_w },
	{ 0x9000, 0x9003, pia_3_w },
	{ 0x9400, 0x9403, sdungeon_pia_0_w },
	{ 0x9900, 0x9903, pia_1_w },
	{ 0x9c00, 0x9fff, pia_2_w },
	{ 0xa000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress zoo_writemem[] =
{
	{ 0x0000, 0x03ff, qix_sharedram_w, &qix_sharedram },
	{ 0x0400, 0x07ff, MWA_RAM },
	{ 0x0c00, 0x0c00, qix_video_firq_w },
	{ 0x0c01, 0x0c01, MWA_NOP },	/* interrupt acknowledge */
	{ 0x1000, 0x1003, pia_3_w },	/* Sound PIA */
	{ 0x1400, 0x1403, sdungeon_pia_0_w },	/* Game PIA 1 */
	{ 0x1900, 0x1903, pia_1_w },	/* Game PIA 2 */
	{ 0x1c00, 0x1fff, pia_2_w },    /* Game PIA 3 */
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress writemem_video[] =
{
	{ 0x0000, 0x7fff, qix_videoram_w },
	{ 0x8000, 0x83ff, qix_sharedram_w },
	{ 0x8400, 0x87ff, MWA_RAM, &nvram, &nvram_size },
	{ 0x8800, 0x8800, qix_palettebank_w, &qix_palettebank },
	{ 0x8c00, 0x8c00, qix_data_firq_w },
	{ 0x9000, 0x93ff, qix_paletteram_w, &paletteram },
	{ 0x9400, 0x9400, qix_addresslatch_w },
	{ 0x9402, 0x9403, MWA_RAM, &qix_videoaddress },
	{ 0x9c00, 0x9FFF, MWA_RAM }, /* Video controller */
	{ 0xa000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress zoo_writemem_video[] =
{
	{ 0x0000, 0x7fff, qix_videoram_w },
	{ 0x8000, 0x83ff, qix_sharedram_w },
	{ 0x8400, 0x87ff, MWA_RAM, &nvram, &nvram_size },	/////protected when coin door is closed
	{ 0x8800, 0x8800, qix_palettebank_w, &qix_palettebank },	/* LEDs are upper 6 bits */
	{ 0x8801, 0x8801, zoo_bankswitch_w },
	{ 0x8c00, 0x8c00, qix_data_firq_w },
	{ 0x8c01, 0x8c01, MWA_NOP },	/* interrupt acknowledge */
	{ 0x9000, 0x93ff, qix_paletteram_w, &paletteram },
	{ 0x9400, 0x9400, qix_addresslatch_w },
	{ 0x9402, 0x9403, MWA_RAM, &qix_videoaddress },
	{ 0xa000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0x2000, 0x2003, pia_5_w },
	{ 0x4000, 0x4003, pia_4_w },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress zoo_writemem_sound[] =
{
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0x2000, 0x2003, pia_5_w },
	{ 0x4000, 0x4003, pia_4_w },
	{ 0xd000, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress mcu_readmem[] =
{
	{ 0x0000, 0x0000, sdungeon_68705_portA_r },
	{ 0x0001, 0x0001, sdungeon_68705_portB_r },
	{ 0x0002, 0x0002, sdungeon_68705_portC_r },
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress mcu_writemem[] =
{
	{ 0x0000, 0x0000, sdungeon_68705_portA_w },
	{ 0x0001, 0x0001, sdungeon_68705_portB_w },
	{ 0x0002, 0x0002, sdungeon_68705_portC_w },
	{ 0x0004, 0x0004, sdungeon_68705_ddrA_w },
	{ 0x0005, 0x0005, sdungeon_68705_ddrB_w },
	{ 0x0006, 0x0006, sdungeon_68705_ddrC_w },
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
	{ -1 }  /* end of table */
};



#define COIN_PORT \
	PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_SERVICE, "Test Advance", KEYCODE_F1, IP_JOY_DEFAULT ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, "Test Next line", KEYCODE_F2, IP_JOY_DEFAULT ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, "Test Slew Up", KEYCODE_F5, IP_JOY_DEFAULT ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, "Test Slew Down", KEYCODE_F6, IP_JOY_DEFAULT ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_TILT )

#define UNKNOWN_PORT \
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )


INPUT_PORTS_START( qix )
	PORT_START	/* PIA 0 Port A (PLAYER 1) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* PIA 0 Port B (COIN) */
	COIN_PORT

	PORT_START	/* PIA 1 Port A (SPARE) */
	UNKNOWN_PORT

    PORT_START /* PIA 1 Port B (PLAYER 1/2) */
	UNKNOWN_PORT

	PORT_START	/* PIA 2 Port A (PLAYER 2) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
INPUT_PORTS_END

INPUT_PORTS_START( sdungeon )
	PORT_START	/* PIA 0 Port A (PLAYER 1) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )

	PORT_START	/* PIA 0 Port B (COIN) */
	COIN_PORT

	PORT_START	/* PIA 1 Port A (SPARE) */
	UNKNOWN_PORT

    PORT_START /* PIA 1 Port B (PLAYER 1/2) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PIA 2 Port A (PLAYER 2) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_COCKTAIL )
INPUT_PORTS_END

INPUT_PORTS_START( elecyoyo )
	PORT_START	/* PIA 0 Port A (PLAYER 1) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PIA 0 Port B (COIN) */
	COIN_PORT

	PORT_START	/* PIA 1 Port A (SPARE) */
	UNKNOWN_PORT

    PORT_START /* PIA 1 Port B (PLAYER 1/2) */
	UNKNOWN_PORT

	PORT_START	/* PIA 2 Port A (PLAYER 2) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( kram )
	PORT_START	/* PIA 0 Port A (PLAYER 1) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* PIA 0 Port B (COIN) */
	COIN_PORT

	PORT_START	/* PIA 1 Port A (SPARE) */
	UNKNOWN_PORT

    PORT_START /* PIA 1 Port B (PLAYER 1/2) */
	UNKNOWN_PORT

	PORT_START	/* PIA 2 Port A (PLAYER 2) */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
INPUT_PORTS_END

INPUT_PORTS_START( zookeep )
	PORT_START	/* PIA 0 Port A (PLAYER 1) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PIA 0 Port B (COIN) */
	COIN_PORT

	PORT_START	/* PIA 1 Port A (SPARE) */
	UNKNOWN_PORT

    PORT_START /* PIA 1 Port B (PLAYER 1/2) */
	UNKNOWN_PORT

	PORT_START	/* PIA 2 Port A (PLAYER 2) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END




static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};



static struct MachineDriver machine_driver_qix =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1250000,	/* 1.25 MHz */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_M6809,
			1250000,	/* 1.25 MHz */
			readmem_video,writemem_video,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M6802 | CPU_AUDIO_CPU,
			3680000/4,	/* 0.92 MHz */
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	60,	/* 60 CPU slices per frame - an high value to ensure proper */
		/* synchronization of the CPUs */
	qix_init_machine,			/* init machine routine */ /* JB 970526 */

	/* video hardware */
	256, 256,					/* screen_width, screen_height */
	{ 0, 255, 8, 247 }, 		/* struct rectangle visible_area - just a guess */
	0,							/* GfxDecodeInfo * */
	256,						/* total colors */
	0,							/* color table length */
	0,							/* convert color prom routine */

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE|VIDEO_SUPPORTS_DIRTY,
	0,							/* vh_init routine */
	qix_vh_start,				/* vh_start routine */
	qix_vh_stop,				/* vh_stop routine */
	qix_vh_screenrefresh,		/* vh_update routine */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_mcu =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1250000,	/* 1.25 MHz */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_M6809,
			1250000,	/* 1.25 MHz */
			readmem_video,writemem_video,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M6802 | CPU_AUDIO_CPU,
			3680000/4,	/* 0.92 MHz */
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M68705,
			4000000/2,	/* xtal is 4MHz, I think it's divided by 2 internally */
			mcu_readmem,mcu_writemem,0,0,
			ignore_interrupt,0      /* No periodic interrupt */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	600,	/* 60 CPU slices per frame - an high value to ensure proper */
		/* synchronization of the CPUs */
	withmcu_init_machine,                       /* init machine routine */ /* JB 970526 */

	/* video hardware */
	256, 256,					/* screen_width, screen_height */
	{ 0, 255, 8, 247 }, 		/* struct rectangle visible_area - just a guess */
	0,							/* GfxDecodeInfo * */
	256,						/* total colors */
	0,							/* color table length */
	0,							/* convert color prom routine */

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE|VIDEO_SUPPORTS_DIRTY,
	0,							/* vh_init routine */
	qix_vh_start,				/* vh_start routine */
	qix_vh_stop,				/* vh_stop routine */
	qix_vh_screenrefresh,		/* vh_update routine */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_zookeep =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1250000,		/* 1.25 MHz */
			zoo_readmem,zoo_writemem,0,0,
			interrupt,1
		},
		{
			CPU_M6809,
			1250000,		/* 1.25 MHz */
			zoo_readmem_video, zoo_writemem_video, 0, 0,
			ignore_interrupt,0
		},
		{
			CPU_M6802 | CPU_AUDIO_CPU,
			3680000/4,		/* 0.92 MHz */
			zoo_readmem_sound,zoo_writemem_sound,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M68705,
			4000000/2,	/* xtal is 4MHz, I think it's divided by 2 internally */
			mcu_readmem,mcu_writemem,0,0,
			ignore_interrupt,0      /* No periodic interrupt */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	60,	/* 60 CPU slices per frame - an high value to ensure proper */
		/* synchronization of the CPUs */
	zoo_init_machine,					/* init machine routine */

	/* video hardware */
	256, 256,					/* screen_width, screen_height */
	{ 0, 255, 8, 247 }, 		/* struct rectangle visible_area - just a guess */
	0,							/* GfxDecodeInfo * */
	256,						/* total colors */
	0,							/* color table length */
	0,							/* convert color prom routine */

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE|VIDEO_SUPPORTS_DIRTY,
	0,							/* vh_init routine */
	qix_vh_start,				/* vh_start routine */
	qix_vh_stop,				/* vh_stop routine */
	qix_vh_screenrefresh,		/* vh_update routine */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	},

	nvram_handler
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( qix )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "u12",          0xC000, 0x0800, 0xaad35508 )
	ROM_LOAD( "u13",          0xC800, 0x0800, 0x46c13504 )
	ROM_LOAD( "u14",          0xD000, 0x0800, 0x5115e896 )
	ROM_LOAD( "u15",          0xD800, 0x0800, 0xccd52a1b )
	ROM_LOAD( "u16",          0xE000, 0x0800, 0xcd1c36ee )
	ROM_LOAD( "u17",          0xE800, 0x0800, 0x1acb682d )
	ROM_LOAD( "u18",          0xF000, 0x0800, 0xde77728b )
	ROM_LOAD( "u19",          0xF800, 0x0800, 0xc0994776 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for code for the second CPU (Video) */
	ROM_LOAD( "u4",           0xC800, 0x0800, 0x5b906a09 )
	ROM_LOAD( "u5",           0xD000, 0x0800, 0x254a3587 )
	ROM_LOAD( "u6",           0xD800, 0x0800, 0xace30389 )
	ROM_LOAD( "u7",           0xE000, 0x0800, 0x8ebcfa7c )
	ROM_LOAD( "u8",           0xE800, 0x0800, 0xb8a3c8f9 )
	ROM_LOAD( "u9",           0xF000, 0x0800, 0x26cbcd55 )
	ROM_LOAD( "u10",          0xF800, 0x0800, 0x568be942 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "u27",          0xF800, 0x0800, 0xf3782bd0 )
ROM_END

ROM_START( qixa )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "qu12",         0xC000, 0x0800, 0x1c55b44d )
	ROM_LOAD( "qu13",         0xC800, 0x0800, 0x20279e8c )
	ROM_LOAD( "qu14",         0xD000, 0x0800, 0xbafe3ce3 )
	/* d800-dfff empty */
	ROM_LOAD( "qu16",         0xE000, 0x0800, 0xdb560753 )
	ROM_LOAD( "qu17",         0xE800, 0x0800, 0x8c7aeed8 )
	ROM_LOAD( "qu18",         0xF000, 0x0800, 0x353be980 )
	ROM_LOAD( "qu19",         0xF800, 0x0800, 0xf46a69ca )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for code for the second CPU (Video) */
	ROM_LOAD( "qu3",          0xC000, 0x0800, 0x8b4c0ef0 )
	ROM_LOAD( "qu4",          0xC800, 0x0800, 0x66a5c260 )
	ROM_LOAD( "qu5",          0xD000, 0x0800, 0x70160ea3 )
	/* d800-dfff empty */
	ROM_LOAD( "qu7",          0xE000, 0x0800, 0xd6733019 )
	ROM_LOAD( "qu8",          0xE800, 0x0800, 0x66870dcc )
	ROM_LOAD( "qu9",          0xF000, 0x0800, 0xc99bf94d )
	ROM_LOAD( "qu10",         0xF800, 0x0800, 0x88b45037 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "u27",          0xF800, 0x0800, 0xf3782bd0 )
ROM_END

ROM_START( qixb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "lk14.bin",     0xC000, 0x1000, 0x6d164986 )
	ROM_LOAD( "lk15.bin",     0xD000, 0x1000, 0x16c6ce0f )
	ROM_LOAD( "lk16.bin",     0xE000, 0x1000, 0x698b1f9c )
	ROM_LOAD( "lk17.bin",     0xF000, 0x1000, 0x7e3adde6 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for code for the second CPU (Video) */
	ROM_LOAD( "lk10.bin",     0xC000, 0x1000, 0x7eac67d0 )
	ROM_LOAD( "lk11.bin",     0xD000, 0x1000, 0x90ccbb6a )
	ROM_LOAD( "lk12.bin",     0xE000, 0x1000, 0xbe9b9f7d )
	ROM_LOAD( "lk13.bin",     0xF000, 0x1000, 0x51c9853b )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "u27",          0xF800, 0x0800, 0xf3782bd0 )
ROM_END

ROM_START( qix2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "u12.rmb",      0xC000, 0x0800, 0x484280fd )
	ROM_LOAD( "u13.rmb",      0xC800, 0x0800, 0x3d089fcb )
	ROM_LOAD( "u14.rmb",      0xD000, 0x0800, 0x362123a9 )
	ROM_LOAD( "u15.rmb",      0xD800, 0x0800, 0x60f3913d )
	ROM_LOAD( "u16.rmb",      0xE000, 0x0800, 0xcc139e34 )
	ROM_LOAD( "u17.rmb",      0xE800, 0x0800, 0xcf31dc49 )
	ROM_LOAD( "u18.rmb",      0xF000, 0x0800, 0x1f91ed7a )
	ROM_LOAD( "u19.rmb",      0xF800, 0x0800, 0x68e8d5a6 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for code for the second CPU (Video) */
	ROM_LOAD( "u3.rmb",       0xC000, 0x0800, 0x19cebaca )
	ROM_LOAD( "u4.rmb",       0xC800, 0x0800, 0x6cfb4185 )
	ROM_LOAD( "u5.rmb",       0xD000, 0x0800, 0x948f53f3 )
	ROM_LOAD( "u6.rmb",       0xD800, 0x0800, 0x8630120e )
	ROM_LOAD( "u7.rmb",       0xE000, 0x0800, 0xbad037c9 )
	ROM_LOAD( "u8.rmb",       0xE800, 0x0800, 0x3159bc00 )
	ROM_LOAD( "u9.rmb",       0xF000, 0x0800, 0xe80e9b1d )
	ROM_LOAD( "u10.rmb",      0xF800, 0x0800, 0x9a55d360 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "u27",          0xF800, 0x0800, 0xf3782bd0 )
ROM_END

ROM_START( sdungeon )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
    ROM_LOAD( "sd14.u14",     0xA000, 0x1000, 0x7024b55a )
    ROM_LOAD( "sd15.u15",     0xB000, 0x1000, 0xa3ac9040 )
    ROM_LOAD( "sd16.u16",     0xC000, 0x1000, 0xcc20b580 )
    ROM_LOAD( "sd17.u17",     0xD000, 0x1000, 0x4663e4b8 )
    ROM_LOAD( "sd18.u18",     0xE000, 0x1000, 0x7ef1ffc0 )
    ROM_LOAD( "sd19.u19",     0xF000, 0x1000, 0x7b20b7ac )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
    ROM_LOAD( "sd05.u5",      0x0A000, 0x1000, 0x0b2bf48e )
    ROM_LOAD( "sd06.u6",      0x0B000, 0x1000, 0xf86db512 )
    ROM_LOAD( "sd07.u7",      0x0C000, 0x1000, 0x7b796831 )
    ROM_LOAD( "sd08.u8",      0x0D000, 0x1000, 0x5fbe7068 )
    ROM_LOAD( "sd09.u9",      0x0E000, 0x1000, 0x89bc51ea )
    ROM_LOAD( "sd10.u10",     0x0F000, 0x1000, 0x754de734 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
    ROM_LOAD( "sd26.u26",     0xF000, 0x0800, 0x3df8630d )
    ROM_LOAD( "sd27.u27",     0xF800, 0x0800, 0x0386f351 )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "sd101",        0x0000, 0x0800, 0xe255af9a )
ROM_END

ROM_START( elecyoyo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
    ROM_LOAD( "yy14",         0xA000, 0x1000, 0x0d2edcb9 )
    ROM_LOAD( "yy15",         0xB000, 0x1000, 0xa91f01e3 )
    ROM_LOAD( "yy16-1",       0xC000, 0x1000, 0x2710f360 )
    ROM_LOAD( "yy17",         0xD000, 0x1000, 0x25fd489d )
    ROM_LOAD( "yy18",         0xE000, 0x1000, 0x0b6661c0 )
    ROM_LOAD( "yy19-1",       0xF000, 0x1000, 0x95b8b244 )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
    ROM_LOAD( "yy5",          0x0A000, 0x1000, 0x3793fec5 )
    ROM_LOAD( "yy6",          0x0B000, 0x1000, 0x2e8b1265 )
    ROM_LOAD( "yy7",          0x0C000, 0x1000, 0x20f93411 )
    ROM_LOAD( "yy8",          0x0D000, 0x1000, 0x926f90c8 )
    ROM_LOAD( "yy9",          0x0E000, 0x1000, 0x2f999480 )
    ROM_LOAD( "yy10",         0x0F000, 0x1000, 0xb31d20e2 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
    ROM_LOAD( "yy27",         0xF800, 0x0800, 0x5a2aa0f3 )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "yy101",        0x0000, 0x0800, 0x3cf13038 )
ROM_END

ROM_START( elecyoy2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
    ROM_LOAD( "yy14",         0xA000, 0x1000, 0x0d2edcb9 )
    ROM_LOAD( "yy15",         0xB000, 0x1000, 0xa91f01e3 )
    ROM_LOAD( "yy16",         0xC000, 0x1000, 0xcab19f3a )
    ROM_LOAD( "yy17",         0xD000, 0x1000, 0x25fd489d )
    ROM_LOAD( "yy18",         0xE000, 0x1000, 0x0b6661c0 )
    ROM_LOAD( "yy19",         0xF000, 0x1000, 0xd0215d2e )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
    ROM_LOAD( "yy5",          0x0A000, 0x1000, 0x3793fec5 )
    ROM_LOAD( "yy6",          0x0B000, 0x1000, 0x2e8b1265 )
    ROM_LOAD( "yy7",          0x0C000, 0x1000, 0x20f93411 )
    ROM_LOAD( "yy8",          0x0D000, 0x1000, 0x926f90c8 )
    ROM_LOAD( "yy9",          0x0E000, 0x1000, 0x2f999480 )
    ROM_LOAD( "yy10",         0x0F000, 0x1000, 0xb31d20e2 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
    ROM_LOAD( "yy27",         0xF800, 0x0800, 0x5a2aa0f3 )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "yy101",        0x0000, 0x0800, 0x3cf13038 )
ROM_END

ROM_START( kram )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
    ROM_LOAD( "ks14-1",       0xA000, 0x1000, 0xfe69ac79 )
    ROM_LOAD( "ks15",         0xB000, 0x1000, 0x4b2c175e )
    ROM_LOAD( "ks16",         0xC000, 0x1000, 0x9500a05d )
    ROM_LOAD( "ks17",         0xD000, 0x1000, 0xc752a3a1 )
    ROM_LOAD( "ks18",         0xE000, 0x1000, 0x79158b03 )
    ROM_LOAD( "ks19-1",       0xF000, 0x1000, 0x759ea6ce )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
    ROM_LOAD( "ks5",          0x0A000, 0x1000, 0x1c472080 )
    ROM_LOAD( "ks6",          0x0B000, 0x1000, 0xb8926622 )
    ROM_LOAD( "ks7",          0x0C000, 0x1000, 0xc98a7485 )
    ROM_LOAD( "ks8",          0x0D000, 0x1000, 0x1127c4e4 )
    ROM_LOAD( "ks9",          0x0E000, 0x1000, 0xd3bc8b5e )
    ROM_LOAD( "ks10",         0x0F000, 0x1000, 0xe0426444 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
    ROM_LOAD( "ks27",         0xf800, 0x0800, 0xc46530c8 )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "ks101.dat",    0x0000, 0x0800, 0xe53d97b7 )
ROM_END

ROM_START( kram2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
    ROM_LOAD( "ks14",         0xA000, 0x1000, 0xa2eac1ff )
    ROM_LOAD( "ks15",         0xB000, 0x1000, 0x4b2c175e )
    ROM_LOAD( "ks16",         0xC000, 0x1000, 0x9500a05d )
    ROM_LOAD( "ks17",         0xD000, 0x1000, 0xc752a3a1 )
    ROM_LOAD( "ks18",         0xE000, 0x1000, 0x79158b03 )
    ROM_LOAD( "ks19",         0xF000, 0x1000, 0x053c5e09 )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
    ROM_LOAD( "ks5",          0x0A000, 0x1000, 0x1c472080 )
    ROM_LOAD( "ks6",          0x0B000, 0x1000, 0xb8926622 )
    ROM_LOAD( "ks7",          0x0C000, 0x1000, 0xc98a7485 )
    ROM_LOAD( "ks8",          0x0D000, 0x1000, 0x1127c4e4 )
    ROM_LOAD( "ks9",          0x0E000, 0x1000, 0xd3bc8b5e )
    ROM_LOAD( "ks10",         0x0F000, 0x1000, 0xe0426444 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
    ROM_LOAD( "ks27",         0xf800, 0x0800, 0xc46530c8 )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "ks101.dat",    0x0000, 0x0800, 0xe53d97b7 )
ROM_END

ROM_START( zookeep )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "za12",         0x8000, 0x1000, 0x4e40d8dc )
	ROM_LOAD( "za13",         0x9000, 0x1000, 0xeebd5248 )
	ROM_LOAD( "za14",         0xA000, 0x1000, 0xfab43297 )
	ROM_LOAD( "za15",         0xB000, 0x1000, 0xef8cd67c )
	ROM_LOAD( "za16",         0xC000, 0x1000, 0xccfc15bc )
	ROM_LOAD( "za17",         0xD000, 0x1000, 0x358013f4 )
	ROM_LOAD( "za18",         0xE000, 0x1000, 0x37886afe )
	ROM_LOAD( "za19",         0xF000, 0x1000, 0xbbfb30d9 )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
	ROM_LOAD( "za5",          0x0A000, 0x1000, 0xdc0c3cbd )
	ROM_LOAD( "za3",          0x10000, 0x1000, 0xcc4d0aee )
	ROM_LOAD( "za6",          0x0B000, 0x1000, 0x27c787dd )
	ROM_LOAD( "za4",          0x11000, 0x1000, 0xec3b10b1 )

	ROM_LOAD( "za7",          0x0C000, 0x1000, 0x1479f480 )
	ROM_LOAD( "za8",          0x0D000, 0x1000, 0x4c96cdb2 )
	ROM_LOAD( "za9",          0x0E000, 0x1000, 0xa4f7d9e0 )
	ROM_LOAD( "za10",         0x0F000, 0x1000, 0x05df1a5a )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "za25",         0xD000, 0x1000, 0x779b8558 )
	ROM_LOAD( "za26",         0xE000, 0x1000, 0x60a810ce )
	ROM_LOAD( "za27",         0xF000, 0x1000, 0x99ed424e )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "za_coin.bin",  0x0000, 0x0800, 0x364d3557 )
ROM_END

ROM_START( zookeep2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "za12",         0x8000, 0x1000, 0x4e40d8dc )
	ROM_LOAD( "za13",         0x9000, 0x1000, 0xeebd5248 )
	ROM_LOAD( "za14",         0xA000, 0x1000, 0xfab43297 )
	ROM_LOAD( "za15",         0xB000, 0x1000, 0xef8cd67c )
	ROM_LOAD( "za16",         0xC000, 0x1000, 0xccfc15bc )
	ROM_LOAD( "za17",         0xD000, 0x1000, 0x358013f4 )
	ROM_LOAD( "za18",         0xE000, 0x1000, 0x37886afe )
	ROM_LOAD( "za19.red",     0xF000, 0x1000, 0xec01760e )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
	ROM_LOAD( "za5",          0x0A000, 0x1000, 0xdc0c3cbd )
	ROM_LOAD( "za3",          0x10000, 0x1000, 0xcc4d0aee )
	ROM_LOAD( "za6",          0x0B000, 0x1000, 0x27c787dd )
	ROM_LOAD( "za4",          0x11000, 0x1000, 0xec3b10b1 )

	ROM_LOAD( "za7",          0x0C000, 0x1000, 0x1479f480 )
	ROM_LOAD( "za8",          0x0D000, 0x1000, 0x4c96cdb2 )
	ROM_LOAD( "za9",          0x0E000, 0x1000, 0xa4f7d9e0 )
	ROM_LOAD( "za10",         0x0F000, 0x1000, 0x05df1a5a )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "za25",         0xD000, 0x1000, 0x779b8558 )
	ROM_LOAD( "za26",         0xE000, 0x1000, 0x60a810ce )
	ROM_LOAD( "za27",         0xF000, 0x1000, 0x99ed424e )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "za_coin.bin",  0x0000, 0x0800, 0x364d3557 )
ROM_END

ROM_START( zookeep3 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code for the first CPU (Data) */
	ROM_LOAD( "za12",         0x8000, 0x1000, 0x4e40d8dc )
	ROM_LOAD( "za13",         0x9000, 0x1000, 0xeebd5248 )
	ROM_LOAD( "za14",         0xA000, 0x1000, 0xfab43297 )
	ROM_LOAD( "za15",         0xB000, 0x1000, 0xef8cd67c )
	ROM_LOAD( "za16",         0xC000, 0x1000, 0xccfc15bc )
	ROM_LOAD( "za17",         0xD000, 0x1000, 0x358013f4 )
	ROM_LOAD( "za18",         0xE000, 0x1000, 0x37886afe )
	ROM_LOAD( "za19",         0xF000, 0x1000, 0xbbfb30d9 )

	ROM_REGION( 0x12000, REGION_CPU2 )     /* 64k for code + 2 ROM banks for the second CPU (Video) */
	ROM_LOAD( "za5",          0x0A000, 0x1000, 0xdc0c3cbd )
	ROM_LOAD( "za3",          0x10000, 0x1000, 0xcc4d0aee )
	ROM_LOAD( "za6",          0x0B000, 0x1000, 0x27c787dd )
	ROM_LOAD( "za4",          0x11000, 0x1000, 0xec3b10b1 )

	ROM_LOAD( "za7",          0x0C000, 0x1000, 0x1479f480 )
	ROM_LOAD( "za8",          0x0D000, 0x1000, 0x4c96cdb2 )
	ROM_LOAD( "zv35.9",       0x0E000, 0x1000, 0xd14123b7 )
	ROM_LOAD( "zv36.10",      0x0F000, 0x1000, 0x23705777 )

	ROM_REGION( 0x10000, REGION_CPU3 ) 	/* 64k for code for the third CPU (sound) */
	ROM_LOAD( "za25",         0xD000, 0x1000, 0x779b8558 )
	ROM_LOAD( "za26",         0xE000, 0x1000, 0x60a810ce )
	ROM_LOAD( "za27",         0xF000, 0x1000, 0x99ed424e )

	ROM_REGION( 0x0800, REGION_CPU4 )	/* 2k for the 68705 microcontroller */
	ROM_LOAD( "za_coin.bin",  0x0000, 0x0800, 0x364d3557 )
ROM_END



GAMEX( 1981, qix,      0,        qix,     qix,      0, ROT270, "Taito America Corporation", "Qix (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1981, qixa,     qix,      qix,     qix,      0, ROT270, "Taito America Corporation", "Qix (set 2)", GAME_NO_COCKTAIL )
GAMEX( 1981, qixb,     qix,      qix,     qix,      0, ROT270, "Taito America Corporation", "Qix (set 3)", GAME_NO_COCKTAIL )
GAMEX( 1981, qix2,     qix,      qix,     qix,      0, ROT270, "Taito America Corporation", "Qix II (Tournament)", GAME_NO_COCKTAIL )
GAMEX( 1981, sdungeon, 0,        mcu,     sdungeon, 0, ROT270, "Taito America Corporation", "Space Dungeon", GAME_NO_COCKTAIL )
GAMEX( 1982, elecyoyo, 0,        mcu,     elecyoyo, 0, ROT270, "Taito America Corporation", "The Electric Yo-Yo (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1982, elecyoy2, elecyoyo, mcu,     elecyoyo, 0, ROT270, "Taito America Corporation", "The Electric Yo-Yo (set 2)", GAME_NO_COCKTAIL )
GAMEX( 1982, kram,     0,        mcu,     kram,     0, ROT0,   "Taito America Corporation", "Kram (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1982, kram2,    kram,     mcu,     kram,     0, ROT0,   "Taito America Corporation", "Kram (set 2)", GAME_NO_COCKTAIL )
GAMEX( 1982, zookeep,  0,        zookeep, zookeep,  0, ROT0,   "Taito America Corporation", "Zoo Keeper (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1982, zookeep2, zookeep,  zookeep, zookeep,  0, ROT0,   "Taito America Corporation", "Zoo Keeper (set 2)", GAME_NO_COCKTAIL )
GAMEX( 1982, zookeep3, zookeep,  zookeep, zookeep,  0, ROT0,   "Taito America Corporation", "Zoo Keeper (set 3)", GAME_NO_COCKTAIL )
