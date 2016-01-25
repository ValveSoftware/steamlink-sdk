#include "../machine/slapfght.cpp"
#include "../vidhrdw/slapfght.cpp"

/***************************************************************************

Slap Fight driver by K.Wilkins Jan 1998

Slap Fight - Taito

The three drivers provided are identical, only the 1st CPU EPROM is different
which shows up in the boot message, one if Japanese domestic and the other
is English. The proms which MAY be the original slapfight ones currently
give a hardware error and fail to boot.

slapfigh - Arcade ROMs from Japan http://home.onestop.net/j_rom/
slapboot - Unknown source
slpboota - ROMS Dumped by KW 29/12/97 from unmarked Slap Fight board (bootleg?)

PCB Details from slpboota boardset:

Upper PCB (Sound board)
---------
Z80A CPU
Toshiba TMM2016BP-10 (2KB SRAM)
sf_s05 (Fujitsu MBM2764-25 8KB EPROM) - Sound CPU Code

Yamaha YM2149F (Qty 2 - Pin compatible with AY-3-8190)
Hitachi SRAM - HM6464 (8KB - Qty 4)

sf_s01 (OKI M27256-N 32KB PROM)              Sprite Data (16x16 4bpp)
sf_s02 (OKI M27256-N 32KB PROM)              Sprite Data
sf_s03 (OKI M27256-N 32KB PROM)              Sprite Data
sf_s04 (OKI M27256-N 32KB PROM)              Sprite Data


Lower PCB
---------
Z80B CPU
12MHz Xtal
Toshiba TMM2016BP-10 (2KB SRAM - Total Qty 6 = 2+2+1+1)

sf_s10 (Fujitsu MBM2764-25 8KB EPROM)        Font/Character Data (8x8 2bpp)
sf_s11 (Fujitsu MBM2764-25 8KB EPROM)

sf_s06 (OKI M27256-N 32KB PROM)              Tile Data (8x8 4bpp)
sf_s07 (OKI M27256-N 32KB PROM)              Tile Data
sf_s08 (OKI M27256-N 32KB PROM)              Tile Data
sf_s09 (OKI M27256-N 32KB PROM)              Tile Data

sf_s16 (Fujitsu MBM2764-25 8KB EPROM)        Colour Tables (512B used?)

sf_sH  (OKI M27256-N 32KB PROM)              Level Maps ???

sf_s19 (NEC S27128 16KB EPROM)               CPU Code $0000-$3fff
sf_s20 (Mitsubishi M5L27128K 16KB EPROM)     CPU Code $4000-$7fff


Main CPU Memory Map
-------------------

$0000-$3fff    ROM (SF_S19)
$4000-$7fff    ROM (SF_S20)
$8000-$bfff    ROM (SF_SH) - This is a 32K ROM - Paged ????? How ????

$c000-$c7ff    2K RAM
$c800-$cfff    READ:Unknown H/W  WRITE:Unknown H/W (Upper PCB)
$d000-$d7ff    Background RAM1
$d800-$dfff    Background RAM2
$e000-$e7ff    Sprite RAM
$e800-$efff    READ:Unknown H/W  WRITE:Unknown H/W
$f000-$f7ff    READ:SF_S16       WRITE:Character RAM
$f800-$ffff    READ:Unknown H/W  WRITE:Attribute RAM

$c800-$cfff    Appears to be RAM BUT 1st 0x10 bytes are swapped with
               the sound CPU and visversa for READ OPERATIONS


Write I/O MAP
-------------
Addr    Address based write                     Data based write

$00     Reset sound CPU
$01     Clear sound CPU reset
$02
$03
$04
$05
$06     Clear/Disable Hardware interrupt
$07     Enable Hardware interrupt
$08     LOW Bank select for SF_SH               X axis character scroll reg
$09     HIGH Bank select for SF_SH              X axis pixel scroll reg
$0a
$0b
$0c
$0e
$0f

Read I/O Map
------------

$00     Status regsiter - cycle 0xc7, 0x55, 0x00  (Thanks to Dave Spicer for the info)


Known Info
----------

2K Character RAM at write only address $f000-$f7fff looks to be organised
64x32 chars with the screen rotated thru 90 degrees clockwise. There
appears to be some kind of attribute(?) RAM above at $f800-$ffff organised
in the same manner.

From the look of data in the buffer it is arranged thus: 37x32 (HxW) which
would make the overall frame buffer 296x256.

Print function maybe around $09a2 based on info from log file.

$e000 looks like sprite ram, setup routines at $0008.


Sound System CPU Details
------------------------

Memory Map
$0000-$1fff  ROM(SF_S5)
$a080        AY-3-8910(PSG1) Register address
$a081        AY-3-8910(PSG1) Read register
$a082        AY-3-8910(PSG1) Write register
$a090        AY-3-8910(PSG2) Register address
$a091        AY-3-8910(PSG2) Read register
$a092        AY-3-8910(PSG2) Write register
$c800-$cfff  RAM(2K)

Strangely the RAM hardware registers seem to be overlaid at $c800
$00a6 routine here reads I/O ports and stores in, its not a straight
copy, the data is mangled before storage:
PSG1-E -> $c808
PSG1-F -> $c80b
PSG2-E -> $c809
PSG2-F -> $c80a - DIP Switch Bank 2 (Test mode is here)

-------------------------------GET STAR------------------------------------
		following info by Luca Elia (eliavit@unina.it)

				Interesting locations
				---------------------

c803	credits
c806	used as a watchdog: main cpu reads then writes FF.
	If FF was read, jp 0000h. Sound cpu zeroes it.

c807(1p)	left	7			c809	DSW1(cpl'd)
c808(2p)	down	6			c80a	DSW2(cpl'd)
active_H	right	5			c80b	ip 1(cpl'd)
		up	4
		0	3
		0	2
		but2	1
		but1	0

c21d(main)	1p lives

Main cpu writes to unmapped ports 0e,0f,05,03 at startup.
Before playing, f1 is written to e802 and 00 to port 03.
If flip screen dsw is on, ff is written to e802 an 00 to port 02, instead.

				Interesting routines (main cpu)
				-------------------------------
4a3	wait A irq's
432	init the Ath sprite
569	reads a sequence from e803
607	prints the Ath string (FF terminated). String info is stored at
	65bc in the form of: attribute, dest. address, string address (5 bytes)
b73	checks lives. If zero, writes 0 to port 04 then jp 0000h.
	Before that, sets I to FF as a flag, for the startup ram check
	routine, to not alter the credit counter.
1523	put name in hi-scores?

---------------------------------------------------------------------------


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* #define FASTSLAPBOOT */

/* VIDHRDW */

extern unsigned char *slapfight_videoram;
extern unsigned char *slapfight_colorram;
extern size_t slapfight_videoram_size;
extern unsigned char *slapfight_scrollx_lo,*slapfight_scrollx_hi,*slapfight_scrolly;
void slapfight_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void slapfight_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

/* MACHINE */

void slapfight_init_machine(void);

extern unsigned char *slapfight_dpram;
extern size_t slapfight_dpram_size;
WRITE_HANDLER( slapfight_dpram_w );
READ_HANDLER( slapfight_dpram_r );

READ_HANDLER( slapfight_port_00_r );

WRITE_HANDLER( slapfight_port_00_w );
WRITE_HANDLER( slapfight_port_01_w );
WRITE_HANDLER( getstar_port_04_w );
WRITE_HANDLER( slapfight_port_06_w );
WRITE_HANDLER( slapfight_port_07_w );
WRITE_HANDLER( slapfight_port_08_w );
WRITE_HANDLER( slapfight_port_09_w );


READ_HANDLER( getstar_e803_r );
WRITE_HANDLER( getstar_sh_intenable_w );
extern int getstar_sequence_index;
int getstar_interrupt(void);


/* Driver structure definition */

static struct MemoryReadAddress tigerh_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc80f, slapfight_dpram_r },
	{ 0xc810, 0xcfff, MRA_RAM },
	{ 0xd000, 0xd7ff, MRA_RAM },
	{ 0xd800, 0xdfff, MRA_RAM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 } /* end of table */
};

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc80f, slapfight_dpram_r },
	{ 0xc810, 0xcfff, MRA_RAM },
	{ 0xd000, 0xd7ff, MRA_RAM },
	{ 0xd800, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe7ff, MRA_RAM },		/* LE 151098 */
	{ 0xe803, 0xe803, getstar_e803_r }, /* LE 151098 */
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc80f, slapfight_dpram_w, &slapfight_dpram, &slapfight_dpram_size },
	{ 0xc810, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd7ff, videoram_w, &videoram, &videoram_size },
	{ 0xd800, 0xdfff, colorram_w, &colorram },
	{ 0xe000, 0xe7ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe800, 0xe800, MWA_RAM, &slapfight_scrollx_lo },
	{ 0xe801, 0xe801, MWA_RAM, &slapfight_scrollx_hi },
	{ 0xe802, 0xe802, MWA_RAM, &slapfight_scrolly },
	{ 0xf000, 0xf7ff, MWA_RAM, &slapfight_videoram, &slapfight_videoram_size },
	{ 0xf800, 0xffff, MWA_RAM, &slapfight_colorram },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress slapbtuk_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc80f, slapfight_dpram_w, &slapfight_dpram, &slapfight_dpram_size },
	{ 0xc810, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd7ff, videoram_w, &videoram, &videoram_size },
	{ 0xd800, 0xdfff, colorram_w, &colorram },
	{ 0xe000, 0xe7ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe800, 0xe800, MWA_RAM, &slapfight_scrollx_hi },
	{ 0xe802, 0xe802, MWA_RAM, &slapfight_scrolly },
	{ 0xe803, 0xe803, MWA_RAM, &slapfight_scrollx_lo },
	{ 0xf000, 0xf7ff, MWA_RAM, &slapfight_videoram, &slapfight_videoram_size },
	{ 0xf800, 0xffff, MWA_RAM, &slapfight_colorram },
	{ -1 } /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, slapfight_port_00_r },	/* status register */
	{ -1 } /* end of table */
};

static struct IOWritePort tigerh_writeport[] =
{
	{ 0x00, 0x00, slapfight_port_00_w },
	{ 0x01, 0x01, slapfight_port_01_w },
	{ 0x06, 0x06, slapfight_port_06_w },
	{ 0x07, 0x07, slapfight_port_07_w },
	{ -1 } /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, slapfight_port_00_w },
	{ 0x01, 0x01, slapfight_port_01_w },
//	{ 0x04, 0x04, getstar_port_04_w   },
	{ 0x06, 0x06, slapfight_port_06_w },
	{ 0x07, 0x07, slapfight_port_07_w },
	{ 0x08, 0x08, slapfight_port_08_w },	/* select bank 0 */
	{ 0x09, 0x09, slapfight_port_09_w },	/* select bank 1 */
	{ -1 } /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0xa081, 0xa081, AY8910_read_port_0_r },
	{ 0xa091, 0xa091, AY8910_read_port_1_r },
	{ 0xc800, 0xc80f, slapfight_dpram_r },
	{ 0xc810, 0xcfff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0xa080, 0xa080, AY8910_control_port_0_w },
	{ 0xa082, 0xa082, AY8910_write_port_0_w },
	{ 0xa090, 0xa090, AY8910_control_port_1_w },
	{ 0xa092, 0xa092, AY8910_write_port_1_w },
	{ 0xa0e0, 0xa0e0, getstar_sh_intenable_w }, /* LE 151098 (maybe a0f0 also)*/
	{ 0xc800, 0xc80f, slapfight_dpram_w },
	{ 0xc810, 0xcfff, MWA_RAM },
	{ -1 }  /* end of table */
};




INPUT_PORTS_START( tigerh )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
//	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Dipswitch Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Player Speed" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x00, "Fast" )

	PORT_START  /* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "20000 80000" )
	PORT_DIPSETTING(    0x00, "50000 120000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( slapfigh )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Screen Test", KEYCODE_F1, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START  /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x02, 0x02, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Dipswitch Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "30000 100000" )
	PORT_DIPSETTING(    0x10, "50000 200000" )
	PORT_DIPSETTING(    0x20, "50000" )
	PORT_DIPSETTING(    0x00, "100000" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0xc0, "Medium" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END


INPUT_PORTS_START( getstar )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
//	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Dipswitch Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START  /* DSW2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "30000 100000" )
	PORT_DIPSETTING(    0x00, "50000 150000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,   /* 8*8 characters */
	1024,  /* 1024 characters */
	2,     /* 2 bits per pixel */
	{ 0, 1024*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout tigerh_tilelayout =
{
	8,8,    /* 8*8 tiles */
	2048,   /* 2048 tiles */
	4,      /* 4 bits per pixel */
	{ 0, 2048*8*8, 2*2048*8*8, 3*2048*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8    /* every tile takes 8 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	8,8,    /* 8*8 tiles */
	4096,   /* 4096 tiles */
	4,      /* 4 bits per pixel */
	{ 0, 4096*8*8, 2*4096*8*8, 3*4096*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8    /* every tile takes 8 consecutive bytes */
};

static struct GfxLayout tigerh_spritelayout =
{
	16,16,   /* 16*16 sprites */
	512,     /* 512 sprites */
	4,       /* 4 bits per pixel */
	{ 0, 512*32*8, 2*512*32*8, 3*512*32*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8,
			9, 10 ,11, 12, 13, 14, 15 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	32*8    /* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,   /* 16*16 sprites */
	1024,    /* 1024 sprites */
	4,       /* 4 bits per pixel */
	{ 0, 1024*32*8, 2*1024*32*8, 3*1024*32*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8,
			9, 10 ,11, 12, 13, 14, 15 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	32*8    /* every sprite takes 64 consecutive bytes */
};


static struct GfxDecodeInfo tigerh_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0,  64 },
	{ REGION_GFX2, 0, &tigerh_tilelayout,   0,  16 },
	{ REGION_GFX3, 0, &tigerh_spritelayout, 0,  16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0,  64 },
	{ REGION_GFX2, 0, &tilelayout,   0,  16 },
	{ REGION_GFX3, 0, &spritelayout, 0,  16 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,			/* 2 chips */
	1500000,	/* 1.5 MHz ? */
	{ 25, 25 },
	{ input_port_0_r, input_port_2_r },
	{ input_port_1_r, input_port_3_r },
	{ 0, 0 },
	{ 0, 0 }
};


static struct MachineDriver machine_driver_tigerh =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			tigerh_readmem,writemem,readport,tigerh_writeport,
			interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			sound_readmem,sound_writemem,0,0,
			nmi_interrupt,6,    /* ??? */
		}
	},
	60,				/* fps - frames per second */
//	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	5000,	/* wrong, but fixes graphics glitches */
	10,     /* 10 CPU slices per frame - enough for the sound CPU to read all commands */
	slapfight_init_machine,

	/* video hardware */
	64*8, 32*8, { 1*8, 36*8-1, 2*8, 32*8-1 },
	tigerh_gfxdecodeinfo,
	256, 256,
	slapfight_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	slapfight_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_slapfigh =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			readmem,writemem,readport,writeport,
			interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			sound_readmem,sound_writemem,0,0,
				getstar_interrupt/*nmi_interrupt*/, 3,    /* p'tit Seb 980926 this way it sound much better ! */
			0,0                  /* I think music is not so far from correct speed */
/*			ignore_interrupt, 0,
			slapfight_sound_interrupt, 27306667 */
		}
	},
	60,				/* fps - frames per second */
//	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	5000,	/* wrong, but fixes graphics glitches */
	10,     /* 10 CPU slices per frame - enough for the sound CPU to read all commands */
	slapfight_init_machine,

	/* video hardware */
	64*8, 32*8, { 1*8, 36*8-1, 2*8, 32*8-1 },
	gfxdecodeinfo,
	256, 256,
	slapfight_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	slapfight_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

/* identical to slapfigh_ but writemem has different scroll registers */
static struct MachineDriver machine_driver_slapbtuk =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			readmem,slapbtuk_writemem,readport,writeport,
			interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			sound_readmem,sound_writemem,0,0,
			getstar_interrupt/*nmi_interrupt*/, 3,    /* p'tit Seb 980926 this way it sound much better ! */
			0,0                  /* I think music is not so far from correct speed */
/*			ignore_interrupt, 0,
			slapfight_sound_interrupt, 27306667 */
		}
	},
	60,				/* fps - frames per second */
//	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	5000,	/* wrong, but fixes graphics glitches */
	10,     /* 10 CPU slices per frame - enough for the sound CPU to read all commands */
	slapfight_init_machine,

	/* video hardware */
	64*8, 32*8, { 1*8, 36*8-1, 2*8, 32*8-1 },
	gfxdecodeinfo,
	256, 256,
	slapfight_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	slapfight_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



ROM_START( tigerh )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "0.4",          0x00000, 0x4000, 0x4be73246 )
	ROM_LOAD( "1.4",          0x04000, 0x4000, 0xaad04867 )
	ROM_LOAD( "2.4",          0x08000, 0x4000, 0x4843f15c )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a47_03.bin",   0x0000,  0x2000, 0xd105260f )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 8k for the 68705 (missing!) */
	ROM_LOAD( "a47_14.mcu",   0x0000, 0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_05.bin",   0x00000, 0x2000, 0xc5325b49 )  /* Chars */
	ROM_LOAD( "a47_04.bin",   0x02000, 0x2000, 0xcd59628e )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_09.bin",   0x00000, 0x4000, 0x31fae8a8 )  /* Tiles */
	ROM_LOAD( "a47_08.bin",   0x04000, 0x4000, 0xe539af2b )
	ROM_LOAD( "a47_07.bin",   0x08000, 0x4000, 0x02fdd429 )
	ROM_LOAD( "a47_06.bin",   0x0c000, 0x4000, 0x11fbcc8c )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_13.bin",   0x00000, 0x4000, 0x739a7e7e )  /* Sprites */
	ROM_LOAD( "a47_12.bin",   0x04000, 0x4000, 0xc064ecdb )
	ROM_LOAD( "a47_11.bin",   0x08000, 0x4000, 0x744fae9b )
	ROM_LOAD( "a47_10.bin",   0x0c000, 0x4000, 0xe1cf844e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "82s129.12q",   0x0000,  0x0100, 0x2c69350d )
	ROM_LOAD( "82s129.12m",   0x0100,  0x0100, 0x7142e972 )
	ROM_LOAD( "82s129.12n",   0x0200,  0x0100, 0x25f273f2 )
ROM_END

ROM_START( tigerh2 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "b0.5",         0x00000, 0x4000, 0x6ae7e13c )
	ROM_LOAD( "a47_01.bin",   0x04000, 0x4000, 0x65df2152 )
	ROM_LOAD( "a47_02.bin",   0x08000, 0x4000, 0x633d324b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a47_03.bin",   0x0000,  0x2000, 0xd105260f )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 8k for the 68705 (missing!) */
	ROM_LOAD( "a47_14.mcu",   0x0000, 0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_05.bin",   0x00000, 0x2000, 0xc5325b49 )  /* Chars */
	ROM_LOAD( "a47_04.bin",   0x02000, 0x2000, 0xcd59628e )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_09.bin",   0x00000, 0x4000, 0x31fae8a8 )  /* Tiles */
	ROM_LOAD( "a47_08.bin",   0x04000, 0x4000, 0xe539af2b )
	ROM_LOAD( "a47_07.bin",   0x08000, 0x4000, 0x02fdd429 )
	ROM_LOAD( "a47_06.bin",   0x0c000, 0x4000, 0x11fbcc8c )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_13.bin",   0x00000, 0x4000, 0x739a7e7e )  /* Sprites */
	ROM_LOAD( "a47_12.bin",   0x04000, 0x4000, 0xc064ecdb )
	ROM_LOAD( "a47_11.bin",   0x08000, 0x4000, 0x744fae9b )
	ROM_LOAD( "a47_10.bin",   0x0c000, 0x4000, 0xe1cf844e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "82s129.12q",   0x0000,  0x0100, 0x2c69350d )
	ROM_LOAD( "82s129.12m",   0x0100,  0x0100, 0x7142e972 )
	ROM_LOAD( "82s129.12n",   0x0200,  0x0100, 0x25f273f2 )
ROM_END

ROM_START( tigerhj )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "a47_00.bin",   0x00000, 0x4000, 0xcbdbe3cc )
	ROM_LOAD( "a47_01.bin",   0x04000, 0x4000, 0x65df2152 )
	ROM_LOAD( "a47_02.bin",   0x08000, 0x4000, 0x633d324b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a47_03.bin",   0x0000,  0x2000, 0xd105260f )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 8k for the 68705 (missing!) */
	ROM_LOAD( "a47_14.mcu",   0x0000, 0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_05.bin",   0x00000, 0x2000, 0xc5325b49 )  /* Chars */
	ROM_LOAD( "a47_04.bin",   0x02000, 0x2000, 0xcd59628e )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_09.bin",   0x00000, 0x4000, 0x31fae8a8 )  /* Tiles */
	ROM_LOAD( "a47_08.bin",   0x04000, 0x4000, 0xe539af2b )
	ROM_LOAD( "a47_07.bin",   0x08000, 0x4000, 0x02fdd429 )
	ROM_LOAD( "a47_06.bin",   0x0c000, 0x4000, 0x11fbcc8c )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_13.bin",   0x00000, 0x4000, 0x739a7e7e )  /* Sprites */
	ROM_LOAD( "a47_12.bin",   0x04000, 0x4000, 0xc064ecdb )
	ROM_LOAD( "a47_11.bin",   0x08000, 0x4000, 0x744fae9b )
	ROM_LOAD( "a47_10.bin",   0x0c000, 0x4000, 0xe1cf844e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "82s129.12q",   0x0000,  0x0100, 0x2c69350d )
	ROM_LOAD( "82s129.12m",   0x0100,  0x0100, 0x7142e972 )
	ROM_LOAD( "82s129.12n",   0x0200,  0x0100, 0x25f273f2 )
ROM_END

ROM_START( tigerhb1 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "14",           0x00000, 0x4000, 0xca59dd73 )
	ROM_LOAD( "13",           0x04000, 0x4000, 0x38bd54db )
	ROM_LOAD( "a47_02.bin",   0x08000, 0x4000, 0x633d324b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a47_03.bin",   0x0000,  0x2000, 0xd105260f )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_05.bin",   0x00000, 0x2000, 0xc5325b49 )  /* Chars */
	ROM_LOAD( "a47_04.bin",   0x02000, 0x2000, 0xcd59628e )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_09.bin",   0x00000, 0x4000, 0x31fae8a8 )  /* Tiles */
	ROM_LOAD( "a47_08.bin",   0x04000, 0x4000, 0xe539af2b )
	ROM_LOAD( "a47_07.bin",   0x08000, 0x4000, 0x02fdd429 )
	ROM_LOAD( "a47_06.bin",   0x0c000, 0x4000, 0x11fbcc8c )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_13.bin",   0x00000, 0x4000, 0x739a7e7e )  /* Sprites */
	ROM_LOAD( "a47_12.bin",   0x04000, 0x4000, 0xc064ecdb )
	ROM_LOAD( "a47_11.bin",   0x08000, 0x4000, 0x744fae9b )
	ROM_LOAD( "a47_10.bin",   0x0c000, 0x4000, 0xe1cf844e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "82s129.12q",   0x0000,  0x0100, 0x2c69350d )
	ROM_LOAD( "82s129.12m",   0x0100,  0x0100, 0x7142e972 )
	ROM_LOAD( "82s129.12n",   0x0200,  0x0100, 0x25f273f2 )
ROM_END

ROM_START( tigerhb2 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "rom00_09.bin", 0x00000, 0x4000, 0xef738c68 )
	ROM_LOAD( "a47_01.bin",   0x04000, 0x4000, 0x65df2152 )
	ROM_LOAD( "rom02_07.bin", 0x08000, 0x4000, 0x36e250b9 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a47_03.bin",   0x0000,  0x2000, 0xd105260f )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_05.bin",   0x00000, 0x2000, 0xc5325b49 )  /* Chars */
	ROM_LOAD( "a47_04.bin",   0x02000, 0x2000, 0xcd59628e )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_09.bin",   0x00000, 0x4000, 0x31fae8a8 )  /* Tiles */
	ROM_LOAD( "a47_08.bin",   0x04000, 0x4000, 0xe539af2b )
	ROM_LOAD( "a47_07.bin",   0x08000, 0x4000, 0x02fdd429 )
	ROM_LOAD( "a47_06.bin",   0x0c000, 0x4000, 0x11fbcc8c )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a47_13.bin",   0x00000, 0x4000, 0x739a7e7e )  /* Sprites */
	ROM_LOAD( "a47_12.bin",   0x04000, 0x4000, 0xc064ecdb )
	ROM_LOAD( "a47_11.bin",   0x08000, 0x4000, 0x744fae9b )
	ROM_LOAD( "a47_10.bin",   0x0c000, 0x4000, 0xe1cf844e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "82s129.12q",   0x0000,  0x0100, 0x2c69350d )
	ROM_LOAD( "82s129.12m",   0x0100,  0x0100, 0x7142e972 )
	ROM_LOAD( "82s129.12n",   0x0200,  0x0100, 0x25f273f2 )
ROM_END

ROM_START( slapfigh )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "sf_r19.bin",   0x00000, 0x8000, 0x674c0e0f )
	ROM_LOAD( "sf_rh.bin",    0x10000, 0x8000, 0x3c42e4a7 )	/* banked at 8000 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sf_r05.bin",   0x0000,  0x2000, 0x87f4705a )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "68705.bin",    0x0000,  0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r11.bin",   0x00000, 0x2000, 0x2ac7b943 )  /* Chars */
	ROM_LOAD( "sf_r10.bin",   0x02000, 0x2000, 0x33cadc93 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r06.bin",   0x00000, 0x8000, 0xb6358305 )  /* Tiles */
	ROM_LOAD( "sf_r09.bin",   0x08000, 0x8000, 0xe92d9d60 )
	ROM_LOAD( "sf_r08.bin",   0x10000, 0x8000, 0x5faeeea3 )
	ROM_LOAD( "sf_r07.bin",   0x18000, 0x8000, 0x974e2ea9 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r03.bin",   0x00000, 0x8000, 0x8545d397 )  /* Sprites */
	ROM_LOAD( "sf_r01.bin",   0x08000, 0x8000, 0xb1b7b925 )
	ROM_LOAD( "sf_r04.bin",   0x10000, 0x8000, 0x422d946b )
	ROM_LOAD( "sf_r02.bin",   0x18000, 0x8000, 0x587113ae )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "sf_col21.bin", 0x0000,  0x0100, 0xa0efaf99 )
	ROM_LOAD( "sf_col20.bin", 0x0100,  0x0100, 0xa56d57e5 )
	ROM_LOAD( "sf_col19.bin", 0x0200,  0x0100, 0x5cbf9fbf )
ROM_END

ROM_START( slapbtjp )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "sf_r19jb.bin", 0x00000, 0x8000, 0x9a7ac8b3 )
	ROM_LOAD( "sf_rh.bin",    0x10000, 0x8000, 0x3c42e4a7 )	/* banked at 8000 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sf_r05.bin",   0x0000,  0x2000, 0x87f4705a )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r11.bin",   0x00000, 0x2000, 0x2ac7b943 )  /* Chars */
	ROM_LOAD( "sf_r10.bin",   0x02000, 0x2000, 0x33cadc93 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r06.bin",   0x00000, 0x8000, 0xb6358305 )  /* Tiles */
	ROM_LOAD( "sf_r09.bin",   0x08000, 0x8000, 0xe92d9d60 )
	ROM_LOAD( "sf_r08.bin",   0x10000, 0x8000, 0x5faeeea3 )
	ROM_LOAD( "sf_r07.bin",   0x18000, 0x8000, 0x974e2ea9 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r03.bin",   0x00000, 0x8000, 0x8545d397 )  /* Sprites */
	ROM_LOAD( "sf_r01.bin",   0x08000, 0x8000, 0xb1b7b925 )
	ROM_LOAD( "sf_r04.bin",   0x10000, 0x8000, 0x422d946b )
	ROM_LOAD( "sf_r02.bin",   0x18000, 0x8000, 0x587113ae )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "sf_col21.bin", 0x0000,  0x0100, 0xa0efaf99 )
	ROM_LOAD( "sf_col20.bin", 0x0100,  0x0100, 0xa56d57e5 )
	ROM_LOAD( "sf_col19.bin", 0x0200,  0x0100, 0x5cbf9fbf )
ROM_END

ROM_START( slapbtuk )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "sf_r19eb.bin", 0x00000, 0x4000, 0x2efe47af )
	ROM_LOAD( "sf_r20eb.bin", 0x04000, 0x4000, 0xf42c7951 )
	ROM_LOAD( "sf_rh.bin",    0x10000, 0x8000, 0x3c42e4a7 )	/* banked at 8000 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sf_r05.bin",   0x0000,  0x2000, 0x87f4705a )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r11.bin",   0x00000, 0x2000, 0x2ac7b943 )  /* Chars */
	ROM_LOAD( "sf_r10.bin",   0x02000, 0x2000, 0x33cadc93 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r06.bin",   0x00000, 0x8000, 0xb6358305 )  /* Tiles */
	ROM_LOAD( "sf_r09.bin",   0x08000, 0x8000, 0xe92d9d60 )
	ROM_LOAD( "sf_r08.bin",   0x10000, 0x8000, 0x5faeeea3 )
	ROM_LOAD( "sf_r07.bin",   0x18000, 0x8000, 0x974e2ea9 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r03.bin",   0x00000, 0x8000, 0x8545d397 )  /* Sprites */
	ROM_LOAD( "sf_r01.bin",   0x08000, 0x8000, 0xb1b7b925 )
	ROM_LOAD( "sf_r04.bin",   0x10000, 0x8000, 0x422d946b )
	ROM_LOAD( "sf_r02.bin",   0x18000, 0x8000, 0x587113ae )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "sf_col21.bin", 0x0000,  0x0100, 0xa0efaf99 )
	ROM_LOAD( "sf_col20.bin", 0x0100,  0x0100, 0xa56d57e5 )
	ROM_LOAD( "sf_col19.bin", 0x0200,  0x0100, 0x5cbf9fbf )
ROM_END

ROM_START( alcon )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "00",           0x00000, 0x8000, 0x2ba82d60 )
	ROM_LOAD( "01",           0x10000, 0x8000, 0x18bb2f12 )	/* banked at 8000 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sf_r05.bin",   0x0000,  0x2000, 0x87f4705a )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "68705.bin",    0x0000,  0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "04",           0x00000, 0x2000, 0x31003483 )  /* Chars */
	ROM_LOAD( "03",           0x02000, 0x2000, 0x404152c0 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r06.bin",   0x00000, 0x8000, 0xb6358305 )  /* Tiles */
	ROM_LOAD( "sf_r09.bin",   0x08000, 0x8000, 0xe92d9d60 )
	ROM_LOAD( "sf_r08.bin",   0x10000, 0x8000, 0x5faeeea3 )
	ROM_LOAD( "sf_r07.bin",   0x18000, 0x8000, 0x974e2ea9 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf_r03.bin",   0x00000, 0x8000, 0x8545d397 )  /* Sprites */
	ROM_LOAD( "sf_r01.bin",   0x08000, 0x8000, 0xb1b7b925 )
	ROM_LOAD( "sf_r04.bin",   0x10000, 0x8000, 0x422d946b )
	ROM_LOAD( "sf_r02.bin",   0x18000, 0x8000, 0x587113ae )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "sf_col21.bin", 0x0000,  0x0100, 0xa0efaf99 )
	ROM_LOAD( "sf_col20.bin", 0x0100,  0x0100, 0xa56d57e5 )
	ROM_LOAD( "sf_col19.bin", 0x0200,  0x0100, 0x5cbf9fbf )
ROM_END

ROM_START( getstar )
	ROM_REGION( 0x18000, REGION_CPU1 )		/* Region 0 - main cpu code */
	ROM_LOAD( "rom0",         0x00000, 0x4000, 0x6a8bdc6c )
	ROM_LOAD( "rom1",         0x04000, 0x4000, 0xebe8db3c )
	ROM_LOAD( "rom2",         0x10000, 0x8000, 0x343e8415 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Region 3 - sound cpu code */
	ROM_LOAD( "a68-03",       0x0000,  0x2000, 0x18daa44c)

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "68705.bin",    0x0000,  0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68_05-1",     0x00000, 0x2000, 0x06f60107 )  /* Chars */
	ROM_LOAD( "a68_04-1",     0x02000, 0x2000, 0x1fc8f277 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68_09",       0x00000, 0x8000, 0xa293cc2e )  /* Tiles */
	ROM_LOAD( "a68_08",       0x08000, 0x8000, 0x37662375 )
	ROM_LOAD( "a68_07",       0x10000, 0x8000, 0xcf1a964c )
	ROM_LOAD( "a68_06",       0x18000, 0x8000, 0x05f9eb9a )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68-13",       0x00000, 0x8000, 0x643fb282 )  /* Sprites */
	ROM_LOAD( "a68-12",       0x08000, 0x8000, 0x11f74e32 )
	ROM_LOAD( "a68-11",       0x10000, 0x8000, 0xf24158cf )
	ROM_LOAD( "a68-10",       0x18000, 0x8000, 0x83161ed0 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "rom21",        0x0000,  0x0100, 0xd6360b4d )
	ROM_LOAD( "rom20",        0x0100,  0x0100, 0x4ca01887 )
	ROM_LOAD( "rom19",        0x0200,  0x0100, 0x513224f0 )
ROM_END

ROM_START( getstarj )
	ROM_REGION( 0x18000, REGION_CPU1 )		/* Region 0 - main cpu code */
	ROM_LOAD( "a68_00.bin",   0x00000, 0x4000, 0xad1a0143 )
	ROM_LOAD( "a68_01.bin",   0x04000, 0x4000, 0x3426eb7c )
	ROM_LOAD( "a68_02.bin",   0x10000, 0x8000, 0x3567da17 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Region 3 - sound cpu code */
	ROM_LOAD( "a68-03",       0x00000, 0x2000, 0x18daa44c)

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "68705.bin",    0x0000,  0x0800, 0x00000000 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68_05.bin",   0x00000, 0x2000, 0xe3d409e7 )  /* Chars */
	ROM_LOAD( "a68_04.bin",   0x02000, 0x2000, 0x6e5ac9d4 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68_09",       0x00000, 0x8000, 0xa293cc2e )  /* Tiles */
	ROM_LOAD( "a68_08",       0x08000, 0x8000, 0x37662375 )
	ROM_LOAD( "a68_07",       0x10000, 0x8000, 0xcf1a964c )
	ROM_LOAD( "a68_06",       0x18000, 0x8000, 0x05f9eb9a )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68-13",       0x00000, 0x8000, 0x643fb282 )  /* Sprites */
	ROM_LOAD( "a68-12",       0x08000, 0x8000, 0x11f74e32 )
	ROM_LOAD( "a68-11",       0x10000, 0x8000, 0xf24158cf )
	ROM_LOAD( "a68-10",       0x18000, 0x8000, 0x83161ed0 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "rom21",        0x0000, 0x0100, 0xd6360b4d )
	ROM_LOAD( "rom20",        0x0100, 0x0100, 0x4ca01887 )
	ROM_LOAD( "rom19",        0x0200, 0x0100, 0x513224f0 )
ROM_END

ROM_START( getstarb )
	ROM_REGION( 0x18000, REGION_CPU1 )		/* Region 0 - main cpu code */
	ROM_LOAD( "gs_14.rom",    0x00000, 0x4000, 0x1a57a920 )
	ROM_LOAD( "gs_13.rom",    0x04000, 0x4000, 0x805f8e77 )
	ROM_LOAD( "a68_02.bin",   0x10000, 0x8000, 0x3567da17 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Region 3 - sound cpu code */
	ROM_LOAD( "a68-03",       0x0000, 0x2000, 0x18daa44c)

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68_05.bin",   0x00000, 0x2000, 0xe3d409e7 )  /* Chars */
	ROM_LOAD( "a68_04.bin",   0x02000, 0x2000, 0x6e5ac9d4 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68_09",       0x00000, 0x8000, 0xa293cc2e )  /* Tiles */
	ROM_LOAD( "a68_08",       0x08000, 0x8000, 0x37662375 )
	ROM_LOAD( "a68_07",       0x10000, 0x8000, 0xcf1a964c )
	ROM_LOAD( "a68_06",       0x18000, 0x8000, 0x05f9eb9a )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Region 1 - temporary for gfx */
	ROM_LOAD( "a68-13",       0x00000, 0x8000, 0x643fb282 )  /* Sprites */
	ROM_LOAD( "a68-12",       0x08000, 0x8000, 0x11f74e32 )
	ROM_LOAD( "a68-11",       0x10000, 0x8000, 0xf24158cf )
	ROM_LOAD( "a68-10",       0x18000, 0x8000, 0x83161ed0 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "rom21",        0x0000, 0x0100, 0xd6360b4d )
	ROM_LOAD( "rom20",        0x0100, 0x0100, 0x4ca01887 )
	ROM_LOAD( "rom19",        0x0200, 0x0100, 0x513224f0 )
ROM_END



GAMEX( 1985, tigerh,   0,        tigerh,   tigerh,   0, ROT270, "Taito", "Tiger Heli (set 1)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1985, tigerh2,  tigerh,   tigerh,   tigerh,   0, ROT270, "Taito", "Tiger Heli (set 2)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1985, tigerhj,  tigerh,   tigerh,   tigerh,   0, ROT270, "Taito", "Tiger Heli (Japan)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1985, tigerhb1, tigerh,   tigerh,   tigerh,   0, ROT270, "bootleg", "Tiger Heli (bootleg 1)", GAME_NO_COCKTAIL )
GAMEX( 1985, tigerhb2, tigerh,   tigerh,   tigerh,   0, ROT270, "bootleg", "Tiger Heli (bootleg 2)", GAME_NO_COCKTAIL )
GAMEX( 1986, slapfigh, 0,        slapfigh, slapfigh, 0, ROT270, "Taito", "Slap Fight", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1986, slapbtjp, slapfigh, slapfigh, slapfigh, 0, ROT270, "bootleg", "Slap Fight (Japan bootleg)", GAME_NO_COCKTAIL )
GAMEX( 1986, slapbtuk, slapfigh, slapbtuk, slapfigh, 0, ROT270, "bootleg", "Slap Fight (English bootleg)", GAME_NO_COCKTAIL )
GAMEX( 1986, alcon,    slapfigh, slapfigh, slapfigh, 0, ROT270, "<unknown>", "Alcon", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1986, getstar,  0,        slapfigh, getstar,  0, ROT0,   "Taito", "Guardian", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1986, getstarj, getstar,  slapfigh, getstar,  0, ROT0,   "Taito", "Get Star (Japan)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1986, getstarb, getstar,  slapfigh, getstar,  0, ROT0,   "bootleg", "Get Star (bootleg)", GAME_NO_COCKTAIL )
