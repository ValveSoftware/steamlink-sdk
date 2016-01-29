#include "../machine/williams.cpp"
#include "../vidhrdw/williams.cpp"
#include "../sndhrdw/williams.cpp"

/*

	Ordering:

		* Defender
		* Defense Command
		* Mayday Mai'dez
		* Colony 7

		* Stargate
		* Robotron
		* Joust
		* Bubbles
		* Splat
		* Sinistar

		* Blaster

		* Mystic Marathon
		* Turkey Shoot
		* Inferno
		* Joust 2

*/


/***************************************************************************

Changes:
	Mar 12 98 LBO
	* Added Sinistar speech samples provided by Howie Cohen
	* Fixed clipping circuit with help from Sean Riddle and Pat Lawrence

	Mar 22 98 ASG
	* Fixed Sinistar head drawing, did another major cleanup
	* Rewrote the blitter routines
	* Fixed interrupt timing finally!!!

Note: the visible area (according to the service mode) seems to be
	{ 6, 297-1, 7, 246-1 },
however I use
	{ 6, 298-1, 7, 247-1 },
because the DOS port doesn't support well screen widths which are not multiple of 4.

------- Blaster Bubbles Joust Robotron Sinistar Splat Stargate -------------

0000-8FFF ROM   (for Blaster, 0000-3FFF is a bank of 12 ROMs)
0000-97FF Video  RAM Bank switched with ROM (96FF for Blaster)
9800-BFFF RAM
	0xBB00 Blaster only, Color 0 for each line (256 entry)
	0xBC00 Blaster only, Color 0 flags, latch color only if bit 0 = 1 (256 entry)
	    Do something else with the bit 1, I do not know what
C000-CFFF I/O
D000-FFFF ROM

c000-C00F color_registers  (16 bytes of BBGGGRRR)

c804 widget_pia_dataa (widget = I/O board)
c805 widget_pia_ctrla
c806 widget_pia_datab
c807 widget_pia_ctrlb (CB2 select between player 1 and player 2
		       controls if Table or Joust)
      bits 5-3 = 110 = player 2
      bits 5-3 = 111 = player 1

c80c rom_pia_dataa
c80d rom_pia_ctrla
c80e rom_pia_datab
      bit 0 \
      bit 1 |
      bit 2 |-6 bits to sound board
      bit 3 |
      bit 4 |
      bit 5 /
      bit 6 \
      bit 7 /Plus CA2 and CB2 = 4 bits to drive the LED 7 segment
c80f rom_pia_ctrlb

C900 rom_enable_scr_ctrl  Switch between video ram and rom at 0000-97FF

C940 Blaster only: Select bank in the color Prom for color remap
C980 Blaster only: Select which ROM is at 0000-3FFF
C9C0 Blaster only: bit 0 = enable the color 0 changing each lines
		   bit 1 = erase back each frame


***** Blitter ****** (Stargate and Defender do not have blitter)
CA00 start_blitter    Each bits has a function
      1000 0000 Do not process half the byte 4-7
      0100 0000 Do not process half the byte 0-3
      0010 0000 Shift the shape one pixel right (to display a shape on an odd pixel)
      0001 0000 Remap, if shape != 0 then pixel = mask
      0000 1000 Source  1 = take source 0 = take Mask only
      0000 0100 ?
      0000 0010 Transparent
      0000 0001
CA01 blitter_mask     Not really a mask, more a remap color, see Blitter
CA02 blitter_source   hi
CA03 blitter_source   lo
CA04 blitter_dest     hi
CA05 blitter_dest     lo
CA06 blitter_w_h      H  Do a XOR with 4 to have the real value (Except Splat)
CA07 blitter_w_h      W  Do a XOR with 4 to have the real value (Except Splat)

CB00 6 bits of the video counters bits 2-7

CBFF watchdog

CC00-CFFF 1K X 4 CMOS ram battery backed up (8 bits on Sinistar)




------------------ Robotron -------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  Move Up
  bit 1  Move Down
  bit 2  Move Left
  bit 3  Move Right
  bit 4  1 Player
  bit 5  2 Players
  bit 6  Fire Up
  bit 7  Fire Down

c806 widget_pia_datab
  bit 0  Fire Left
  bit 1  Fire Right
  bit 2
  bit 3
  bit 4
  bit 5
  bit 6
  bit 7

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin
  bit 3  High Score Reset
  bit 4  Left Coin
  bit 5  Center Coin
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board


------------------ Joust -------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  Move Left   player 1/2
  bit 1  Move Right  player 1/2
  bit 2  Flap        player 1/2
  bit 3
  bit 4  2 Player
  bit 5  1 Players
  bit 6
  bit 7

c806 widget_pia_datab
  bit 0
  bit 1
  bit 2
  bit 3
  bit 4
  bit 5
  bit 6
  bit 7

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin
  bit 3  High Score Reset
  bit 4  Left Coin
  bit 5  Center Coin
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board


------------------ Stargate -------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  Fire
  bit 1  Thrust
  bit 2  Smart Bomb
  bit 3  HyperSpace
  bit 4  2 Players
  bit 5  1 Player
  bit 6  Reverse
  bit 7  Down

c806 widget_pia_datab
  bit 0  Up
  bit 1  Inviso
  bit 2
  bit 3
  bit 4
  bit 5
  bit 6
  bit 7  0 = Upright  1 = Table

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin        (High Score Reset in schematics)
  bit 3  High Score Reset  (Left Coin in schematics)
  bit 4  Left Coin         (Center Coin in schematics)
  bit 5  Center Coin       (Right Coin in schematics)
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board


------------------ Splat -------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  Walk Up
  bit 1  Walk Down
  bit 2  Walk Left
  bit 3  Walk Right
  bit 4  1 Player
  bit 5  2 Players
  bit 6  Throw Up
  bit 7  Throw Down

c806 widget_pia_datab
  bit 0  Throw Left
  bit 1  Throw Right
  bit 2
  bit 3
  bit 4
  bit 5
  bit 6
  bit 7

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin
  bit 3  High Score Reset
  bit 4  Left Coin
  bit 5  Center Coin
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board


------------------ Blaster -------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  up/down switch a
  bit 1  up/down switch b
  bit 2  up/down switch c
  bit 3  up/down direction
  bit 4  left/right switch a
  bit 5  left/right switch b
  bit 6  left/right switch c
  bit 7  left/right direction

c806 widget_pia_datab
  bit 0  Thrust (Panel)
  bit 1  Blast
  bit 2  Thrust (Joystick)
  bit 3
  bit 4  1 Player
  bit 5  2 Player
  bit 6
  bit 7

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin
  bit 3  High Score Reset
  bit 4  Left Coin
  bit 5  Center Coin
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board

------------------------- Sinistar -------------------------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  up/down switch a
  bit 1  up/down switch b
  bit 2  up/down switch c
  bit 3  up/down direction
  bit 4  left/right switch a
  bit 5  left/right switch b
  bit 6  left/right switch c
  bit 7  left/right direction

c806 widget_pia_datab
  bit 0  Fire
  bit 1  Bomb
  bit 2
  bit 3
  bit 4  1 Player
  bit 5  2 Player
  bit 6
  bit 7

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin
  bit 3  High Score Reset
  bit 4  Left Coin
  bit 5  Center Coin
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board


------------------ Bubbles -------------------
c804 widget_pia_dataa (widget = I/O board)
  bit 0  Up
  bit 1  Down
  bit 2  Left
  bit 3  Right
  bit 4  2 Players
  bit 5  1 Player
  bit 6
  bit 7

c806 widget_pia_datab
  bit 0
  bit 1
  bit 2
  bit 3
  bit 4
  bit 5
  bit 6
  bit 7

c80c rom_pia_dataa
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin        (High Score Reset in schematics)
  bit 3  High Score Reset  (Left Coin in schematics)
  bit 4  Left Coin         (Center Coin in schematics)
  bit 5  Center Coin       (Right Coin in schematics)
  bit 6  Slam Door Tilt
  bit 7  Hand Shake from sound board

------------------------- Defender -------------------------------------
0000-9800 Video RAM
C000-CFFF ROM (4 banks) + I/O
d000-ffff ROM

c000-c00f color_registers  (16 bytes of BBGGGRRR)

C3FC      WatchDog

C400-C4FF CMOS ram battery backed up

C800      6 bits of the video counters bits 2-7

cc00 pia1_dataa (widget = I/O board)
  bit 0  Auto Up
  bit 1  Advance
  bit 2  Right Coin
  bit 3  High Score Reset
  bit 4  Left Coin
  bit 5  Center Coin
  bit 6
  bit 7
cc01 pia1_ctrla

cc02 pia1_datab
  bit 0 \
  bit 1 |
  bit 2 |-6 bits to sound board
  bit 3 |
  bit 4 |
  bit 5 /
  bit 6 \
  bit 7 /Plus CA2 and CB2 = 4 bits to drive the LED 7 segment
cc03 pia1_ctrlb (CB2 select between player 1 and player 2 controls if Table)

cc04 pia2_dataa
  bit 0  Fire
  bit 1  Thrust
  bit 2  Smart Bomb
  bit 3  HyperSpace
  bit 4  2 Players
  bit 5  1 Player
  bit 6  Reverse
  bit 7  Down
cc05 pia2_ctrla

cc06 pia2_datab
  bit 0  Up
  bit 1
  bit 2
  bit 3
  bit 4
  bit 5
  bit 6
  bit 7
cc07 pia2_ctrlb
  Control the IRQ

d000 Select bank (c000-cfff)
  0 = I/O
  1 = BANK 1
  2 = BANK 2
  3 = BANK 3
  7 = BANK 4

------------------------------------------------------------------------

Mystic Marathon (1983)
Turkey Shoot (1984)
Inferno (1984)
Joust2 Survival of the Fittest (1986)

All have two boards, a large board with lots of RAM and
three ROMs, and a smaller board with lots of ROMs,
the CPU, the 6821 PIAs, and the two "Special Chip 2"
custom BIT/BLT chips.
Joust 2 has an additional music/speech board that has a
68B09E CPU, 68B21 PIA, Harris 55564-5 CVSD, and a YM2151.

Contact Michael Soderstrom (ichael@geocities.com) if you
have any additional information or corrections.

Memory Map:

15 14 13 12  11 10  9  8   7  6  5  4   3  2  1  0
--------------------------------------------------
 x  x  x  x   x  x  x  x   x  x  x  x   x  x  x  x	0000-BFFF	48K DRAM

 0  0  0  x   x  x  x  x   x  x  x  x   x  x  x  x	0000-1FFF	8K ROM
 0  0  1  x   x  x  x  x   x  x  x  x   x  x  x  x	2000-3FFF	8K ROM
 0  1  0  x   x  x  x  x   x  x  x  x   x  x  x  x	4000-5FFF	8K ROM
 0  1  1  x   x  x  x  x   x  x  x  x   x  x  x  x	6000-7FFF	8K ROM

 1  0  0  0   x  x  x  x   x  x  x  x   x  x  x  x	8000-8FFF	EN_COLOR* (PAGE3 only)

 0  x  x  x   x  x  x  x   x  x  x  x   x  x  x  x	0000-7FFF	OE_DRAM* (PAGE0 and read only) or:
 1  0  x  x   x  x  x  x   x  x  x  x   x  x  x  x	9000-BFFF	OE_DRAM* (!EN COLOR and read only)

 1  1  0  0   x  x  x  x   x  x  x  x   x  x  x  x	C000-CFFF	I/O:
 1  1  0  0   0  x  x  x   x  x  x  x   x  x  x  x	C000-C7FF	MAP_EN*
 1  1  0  0   1  0  0  0   0  x  x  x   x  x  x  x	C800-C87F	CS_PAGE
 1  1  0  0   1  0  0  0   1  x  x  x   x  x  x  x	C880-C87F	CS_INT* (blitter)
 1  1  0  0   1  0  0  1   0  x  x  x   x  x  x  x	C900-C97F	CS_WDOG* (data = 0x14)
 1  1  0  0   1  0  0  1   1  x  x  x   x  x  x  x	C980-C9FF	CS_PIA
 1  1  0  0   1  0  0  1   1  x  x  x   0  0  x  x	C980-C983	PIA IC5
 1  1  0  0   1  0  0  1   1  x  x  x   0  1  x  x	C984-C987	PIA IC6
 1  1  0  0   1  0  0  1   1  x  x  x   1  1  x  x	C98C		7 segment LED

 1  1  0  0   1  0  1  1   0  0  0  x   x  x  x  x	CB00-CB1F	CK_FG
 1  1  0  0   1  0  1  1   0  0  1  x   x  x  x  x	CB20-CB3F	CK_BG
 1  1  0  0   1  0  1  1   0  1  0  x   x  x  x  x	CB40-CB5F	CK_SCL
 1  1  0  0   1  0  1  1   0  1  1  x   x  x  x  x	CB60-CB7F	CK_SCH
 1  1  0  0   1  0  1  1   1  0  0  x   x  x  x  x	CB80-CB9F	FLIP clk
 1  1  0  0   1  0  1  1   1  0  1  x   x  x  x  x	CBA0-CBBF	DMA_WRINH clk

 1  1  0  0   1  0  1  1   1  1  1  0   x  x  x  x	CBE0-CBEF	EN VPOS*

 1  1  0  0   1  1  0  0   x  x  x  x   x  x  x  x	CC00-CCFF	1Kx4 CMOS RAM MEM_PROT protected
 1  1  0  0   1  1  x  x   x  x  x  x   x  x  x  x	CD00-CFFF	          not MEM_PROT protected

 Mystic Marathon/Inferno:
 1  1  0  1   0  x  x  x   x  x  x  x   x  x  x  x	D000-D7FF	SRAM0*
 1  1  0  1   1  x  x  x   x  x  x  x   x  x  x  x	D800-DFFF	SRAM1*
 1  1  1  0   x  x  x  x   x  x  x  x   x  x  x  x	E000-EFFF	EXXX* 4K ROM
 1  1  1  1   x  x  x  x   x  x  x  x   x  x  x  x	F000-FFFF	FXXX* 4K ROM

 Turkey Shoot/Joust2:
 1  1  0  1   x  x  x  x   x  x  x  x   x  x  x  x	D000-DFFF	DXXX* 4K ROM
 1  1  1  0   x  x  x  x   x  x  x  x   x  x  x  x	E000-EFFF	EXXX* 4K ROM
 1  1  1  1   x  x  x  x   x  x  x  x   x  x  x  x	F000-FFFF	FXXX* 4K ROM

6802/6808 Sound

 0  0  0  x   x  x  x  x   0  x  x  x   x  x  x  x	0000-007F	128 bytes RAM
 0  0  1  x   x  x  x  x   x  x  x  x   x  x  x  x	2000-3FFF	CS PIA IC4
 1  1  1  x   x  x  x  x   x  x  x  x   x  x  x  x	E000-FFFF	8K ROM

***************************************************************************/

#include "driver.h"
#include "machine/6821pia.h"
#include "sndhrdw/williams.h"
#include "vidhrdw/generic.h"


/**** local stuff ****/

static UINT16 cmos_base;
static UINT16 cmos_length;



/**** from machine/williams.h ****/

/* Generic old-Williams PIA interfaces */
extern struct pia6821_interface williams_pia_0_intf;
extern struct pia6821_interface williams_muxed_pia_0_intf;
extern struct pia6821_interface williams_dual_muxed_pia_0_intf;
extern struct pia6821_interface williams_49way_pia_0_intf;
extern struct pia6821_interface williams_pia_1_intf;
extern struct pia6821_interface williams_snd_pia_intf;

/* Game-specific old-Williams PIA interfaces */
extern struct pia6821_interface defender_pia_0_intf;
extern struct pia6821_interface stargate_pia_0_intf;
extern struct pia6821_interface sinistar_snd_pia_intf;

/* Generic later-Williams PIA interfaces */
extern struct pia6821_interface williams2_muxed_pia_0_intf;
extern struct pia6821_interface williams2_pia_1_intf;
extern struct pia6821_interface williams2_snd_pia_intf;

/* Game-specific later-Williams PIA interfaces */
extern struct pia6821_interface mysticm_pia_0_intf;
extern struct pia6821_interface tshoot_pia_0_intf;
extern struct pia6821_interface tshoot_snd_pia_intf;
extern struct pia6821_interface joust2_pia_1_intf;

/* banking variables */
extern UINT8 *defender_bank_base;
extern const UINT32 *defender_bank_list;
extern UINT8 *williams_bank_base;

/* initialization */
void defender_init_machine(void);
void williams_init_machine(void);
void williams2_init_machine(void);
void joust2_init_machine(void);

/* banking */
WRITE_HANDLER( williams_vram_select_w );
WRITE_HANDLER( defender_bank_select_w );
WRITE_HANDLER( blaster_bank_select_w );
WRITE_HANDLER( blaster_vram_select_w );
WRITE_HANDLER( williams2_bank_select_w );

/* misc */
WRITE_HANDLER( williams2_7segment_w );

/* Mayday protection */
extern UINT8 *mayday_protection;
READ_HANDLER( mayday_protection_r );



/**** from vidhrdw/williams.h ****/

/* blitter variables */
extern UINT8 *williams_blitterram;
extern UINT8 williams_blitter_xor;
extern UINT8 williams_blitter_remap;
extern UINT8 williams_blitter_clip;

/* tilemap variables */
extern UINT8 williams2_tilemap_mask;
extern const UINT8 *williams2_row_to_palette;
extern UINT8 williams2_M7_flip;
extern INT8  williams2_videoshift;
extern UINT8 williams2_special_bg_color;

/* later-Williams video control variables */
extern UINT8 *williams2_blit_inhibit;
extern UINT8 *williams2_xscroll_low;
extern UINT8 *williams2_xscroll_high;

/* Blaster extra variables */
extern UINT8 *blaster_color_zero_table;
extern UINT8 *blaster_color_zero_flags;
extern UINT8 *blaster_video_bits;


WRITE_HANDLER( defender_videoram_w );
WRITE_HANDLER( williams_videoram_w );
WRITE_HANDLER( williams2_videoram_w );
WRITE_HANDLER( williams_blitter_w );
WRITE_HANDLER( blaster_remap_select_w );
WRITE_HANDLER( blaster_video_bits_w );
READ_HANDLER( williams_video_counter_r );


int williams_vh_start(void);
void williams_vh_stop(void);
void williams_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int blaster_vh_start(void);
void blaster_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int williams2_vh_start(void);
void williams2_vh_stop(void);

WRITE_HANDLER( williams2_fg_select_w );
WRITE_HANDLER( williams2_bg_select_w );



/**** configuration macros ****/

#define CONFIGURE_CMOS(a,l) \
	cmos_base = a;\
	cmos_length = l

#define CONFIGURE_BLITTER(x,r,c) \
	williams_blitter_xor = x;\
	williams_blitter_remap = r;\
	williams_blitter_clip = c

#define CONFIGURE_TILEMAP(m,p,f,s,b) \
	williams2_tilemap_mask = m;\
	williams2_row_to_palette = p;\
	williams2_M7_flip = (f) ? 0x80 : 0x00;\
	williams2_videoshift = s;\
	williams2_special_bg_color = b

#define CONFIGURE_PIAS(a,b,c) \
	pia_unconfig();\
	pia_config(0, PIA_STANDARD_ORDERING | PIA_8BIT, &a);\
	pia_config(1, PIA_STANDARD_ORDERING | PIA_8BIT, &b);\
	pia_config(2, PIA_STANDARD_ORDERING | PIA_8BIT, &c)



static void nvram_handler(void *file,int read_or_write)
{
	UINT8 *ram = memory_region(REGION_CPU1);

	if (read_or_write)
		osd_fwrite(file,&ram[cmos_base],cmos_length);
	else
	{
		if (file)
			osd_fread(file,&ram[cmos_base],cmos_length);
		else
			memset(&ram[cmos_base],0,cmos_length);
	}
}



/*************************************
 *
 *	Defender memory handlers
 *
 *************************************/

static struct MemoryReadAddress defender_readmem[] =
{
	{ 0x0000, 0x97ff, MRA_BANK1 },
	{ 0x9800, 0xbfff, MRA_RAM },
	{ 0xc000, 0xcfff, MRA_BANK2 },
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress defender_writemem[] =
{
	{ 0x0000, 0x97ff, williams_videoram_w, &videoram, &videoram_size },
	{ 0x9800, 0xbfff, MWA_RAM },
	{ 0xc000, 0xcfff, MWA_BANK2, &defender_bank_base },
	{ 0xc000, 0xc00f, MWA_RAM, &paletteram },
	{ 0xd000, 0xdfff, defender_bank_select_w },
	{ 0xe000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	General Williams memory handlers
 *
 *************************************/

static struct MemoryReadAddress williams_readmem[] =
{
	{ 0x0000, 0x97ff, MRA_BANK1 },
	{ 0x9800, 0xbfff, MRA_RAM },
	{ 0xc804, 0xc807, pia_0_r },
	{ 0xc80c, 0xc80f, pia_1_r },
	{ 0xcb00, 0xcb00, williams_video_counter_r },
	{ 0xcc00, 0xcfff, MRA_RAM },
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress williams_writemem[] =
{
	{ 0x0000, 0x97ff, williams_videoram_w, &williams_bank_base, &videoram_size },
	{ 0x9800, 0xbfff, MWA_RAM },
	{ 0xc000, 0xc00f, paletteram_BBGGGRRR_w, &paletteram },
	{ 0xc804, 0xc807, pia_0_w },
	{ 0xc80c, 0xc80f, pia_1_w },
	{ 0xc900, 0xc900, williams_vram_select_w },
	{ 0xca00, 0xca07, williams_blitter_w, &williams_blitterram },
	{ 0xcbff, 0xcbff, watchdog_reset_w },
	{ 0xcc00, 0xcfff, MWA_RAM },
	{ 0xd000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Blaster memory handlers
 *
 *************************************/

static struct MemoryReadAddress blaster_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_BANK1 },
	{ 0x4000, 0x96ff, MRA_BANK2 },
	{ 0x9700, 0xbfff, MRA_RAM },
	{ 0xc804, 0xc807, pia_0_r },
	{ 0xc80c, 0xc80f, pia_1_r },
	{ 0xcb00, 0xcb00, williams_video_counter_r },
	{ 0xcc00, 0xcfff, MRA_RAM },
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress blaster_writemem[] =
{
	{ 0x0000, 0x96ff, williams_videoram_w, &williams_bank_base, &videoram_size },
	{ 0x9700, 0xbaff, MWA_RAM },
	{ 0xbb00, 0xbbff, MWA_RAM, &blaster_color_zero_table },
	{ 0xbc00, 0xbcff, MWA_RAM, &blaster_color_zero_flags },
	{ 0xbd00, 0xbfff, MWA_RAM },
	{ 0xc000, 0xc00f, paletteram_BBGGGRRR_w, &paletteram },
	{ 0xc804, 0xc807, pia_0_w },
	{ 0xc80c, 0xc80f, pia_1_w },
	{ 0xc900, 0xc900, blaster_vram_select_w },
	{ 0xc940, 0xc940, blaster_remap_select_w },
	{ 0xc980, 0xc980, blaster_bank_select_w },
	{ 0xc9C0, 0xc9c0, blaster_video_bits_w, &blaster_video_bits },
	{ 0xca00, 0xca07, williams_blitter_w, &williams_blitterram },
	{ 0xcbff, 0xcbff, watchdog_reset_w },
	{ 0xcc00, 0xcfff, MWA_RAM },
	{ 0xd000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Later Williams memory handlers
 *
 *************************************/

static struct MemoryReadAddress williams2_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0x87ff, MRA_BANK2 },
	{ 0x8800, 0x8fff, MRA_BANK3 },
	{ 0x9000, 0xbfff, MRA_RAM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc980, 0xc983, pia_1_r },
	{ 0xc984, 0xc987, pia_0_r },
	{ 0xcbe0, 0xcbe0, williams_video_counter_r },
	{ 0xcc00, 0xcfff, MRA_RAM },
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress williams2_writemem[] =
{
	{ 0x0000, 0x8fff, williams2_videoram_w, &videoram, &videoram_size },
	{ 0x9000, 0xbfff, MWA_RAM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc800, williams2_bank_select_w },
	{ 0xc880, 0xc887, williams_blitter_w, &williams_blitterram },
	{ 0xc900, 0xc900, watchdog_reset_w },
	{ 0xc980, 0xc983, pia_1_w },
	{ 0xc984, 0xc987, pia_0_w },
	{ 0xc98c, 0xc98c, williams2_7segment_w },
	{ 0xcb00, 0xcb00, williams2_fg_select_w },
	{ 0xcb20, 0xcb20, williams2_bg_select_w },
	{ 0xcb40, 0xcb40, MWA_RAM, &williams2_xscroll_low },
	{ 0xcb60, 0xcb60, MWA_RAM, &williams2_xscroll_high },
	{ 0xcb80, 0xcb80, MWA_RAM },
	{ 0xcba0, 0xcba0, MWA_RAM, &williams2_blit_inhibit },
	{ 0xcc00, 0xcfff, MWA_RAM },
	{ 0xd000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};



/*************************************
 *
 *	Sound board memory handlers
 *
 *************************************/

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0x0400, 0x0403, pia_2_r },
	{ 0x8400, 0x8403, pia_2_r },	/* used by Colony 7, perhaps others? */
	{ 0xb000, 0xffff, MRA_ROM },	/* most games start at $F000; Sinistar starts at $B000 */
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0x0400, 0x0403, pia_2_w },
	{ 0x8400, 0x8403, pia_2_w },	/* used by Colony 7, perhaps others? */
	{ 0xb000, 0xffff, MWA_ROM },	/* most games start at $F000; Sinistar starts at $B000 */
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Later sound board memory handlers
 *
 *************************************/

static struct MemoryReadAddress williams2_sound_readmem[] =
{
	{ 0x0000, 0x00ff, MRA_RAM },
	{ 0x2000, 0x2003, pia_2_r },
	{ 0xe000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress williams2_sound_writemem[] =
{
	{ 0x0000, 0x00ff, MWA_RAM },
	{ 0x2000, 0x2003, pia_2_w },
	{ 0xe000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( defender )
	PORT_START      /* IN0 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_BUTTON1, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON2, "Thrust", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, IPT_BUTTON3, "Smart Bomb", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON4, "Hyperspace", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BITX(0x40, IP_ACTIVE_HIGH, IPT_BUTTON6, "Reverse", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START      /* IN3 - fake port for better joystick control */
	/* This fake port is handled via defender_input_port_1 */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
INPUT_PORTS_END

INPUT_PORTS_START( colony7 )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END


INPUT_PORTS_START( stargate )
	PORT_START      /* IN0 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_BUTTON1, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON2, "Thrust", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, IPT_BUTTON3, "Smart Bomb", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON6, "Hyperspace", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BITX(0x40, IP_ACTIVE_HIGH, IPT_BUTTON4, "Reverse", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON5, "Inviso", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START      /* IN3 - fake port for better joystick control */
	/* This fake port is handled via stargate_input_port_0 */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
INPUT_PORTS_END


INPUT_PORTS_START( joust )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START      /* IN3 (muxed with IN0) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( robotron )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
INPUT_PORTS_END


INPUT_PORTS_START( bubbles )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
INPUT_PORTS_END


INPUT_PORTS_START( splat )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN | IPF_8WAY | IPF_PLAYER2 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN | IPF_8WAY | IPF_PLAYER1 )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END


INPUT_PORTS_START( sinistar )
	PORT_START      /* IN0 */
	/* pseudo analog joystick, see below */

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START	/* fake, converted by sinistar_input_port_0() */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X, 100, 10, 0x00, 0x6f )

	PORT_START	/* fake, converted by sinistar_input_port_0() */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_REVERSE, 100, 10, 0x00, 0x6f )
INPUT_PORTS_END


INPUT_PORTS_START( blaster )
	PORT_START      /* IN0 */
	/* pseudo analog joystick, see below */

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START	/* fake, converted by sinistar_input_port_0() */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X, 100, 10, 0x00, 0x6f )

	PORT_START	/* fake, converted by sinistar_input_port_0() */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_REVERSE, 100, 10, 0x00, 0x6f )
INPUT_PORTS_END


INPUT_PORTS_START( mysticm )
	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED ) /* Key */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN0 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( tshoot )
	PORT_START	/* IN0 (muxed with IN3)*/
	PORT_ANALOG(0x3F, 0x20, IPT_AD_STICK_Y, 25, 10, 0, 0x3F)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_BUTTON1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x3C, IP_ACTIVE_HIGH, IPT_UNUSED ) /* 0011-1100 output */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START	/* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN3 (muxed with IN0) */
   	PORT_ANALOG(0x3F, 0x20, IPT_AD_STICK_X, 25, 10, 0, 0x3F)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
INPUT_PORTS_END


INPUT_PORTS_START( inferno )
	PORT_START	/* IN0 (muxed with IN3) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKLEFT_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKLEFT_RIGHT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKLEFT_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKRIGHT_UP )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKRIGHT_LEFT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPF_PLAYER1 | IPT_JOYSTICKRIGHT_DOWN )

	PORT_START /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPF_PLAYER1 | IPT_BUTTON6 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPF_PLAYER2 | IPT_BUTTON6 )
	PORT_BIT( 0x3C, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START	/* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN3 (muxed with IN0) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKLEFT_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKLEFT_RIGHT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKLEFT_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKRIGHT_UP )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKRIGHT_LEFT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICKRIGHT_DOWN )
INPUT_PORTS_END


INPUT_PORTS_START( joust2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0xFF, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Auto Up", KEYCODE_F1, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, 0, "High Score Reset", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START /* IN3 (muxed with IN0) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout williams2_layout =
{
	24, 16,
	256,
	4,
	{ 0, 1, 2, 3 },
	{ 0+0*8, 4+0*8, 0+0*8+0x4000*8, 4+0*8+0x4000*8, 0+0*8+0x8000*8, 4+0*8+0x8000*8,
	  0+1*8, 4+1*8, 0+1*8+0x4000*8, 4+1*8+0x4000*8, 0+1*8+0x8000*8, 4+1*8+0x8000*8,
	  0+2*8, 4+2*8, 0+2*8+0x4000*8, 4+2*8+0x4000*8, 0+2*8+0x8000*8, 4+2*8+0x8000*8,
	  0+3*8, 4+3*8, 0+3*8+0x4000*8, 4+3*8+0x4000*8, 0+3*8+0x8000*8, 4+3*8+0x8000*8
	},
	{ 0*8, 4*8, 8*8, 12*8, 16*8, 20*8, 24*8, 28*8, 32*8, 36*8, 40*8, 44*8, 48*8, 52*8, 56*8, 60*8 },
	4*16*8
};


static struct GfxDecodeInfo williams2_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &williams2_layout, 16, 8 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct DACinterface dac_interface =
{
	1,
	{ 50 }
};


static struct hc55516_interface sinistar_cvsd_interface =
{
	1,	/* 1 chip */
	{ 80 },
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_defender =
{
	/* basic machine hardware  */
	{
		{
			CPU_M6809,
			1000000,
			defender_readmem,defender_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6808 | CPU_AUDIO_CPU,
			3579000/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	defender_init_machine,

	/* video hardware */
	304, 256,
	{ 6, 298-1, 7, 247-1 },
	0,
	16,16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE /*SQ | VIDEO_SUPPORTS_DIRTY*/,
	0,
	williams_vh_start,
	williams_vh_stop,
	williams_vh_screenrefresh,

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


static struct MachineDriver machine_driver_williams =
{
	/* basic machine hardware  */
	{
		{
			CPU_M6809,
			1000000,
			williams_readmem,williams_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6808 | CPU_AUDIO_CPU,
			3579000/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	williams_init_machine,

	/* video hardware */
	304, 256,
	{ 6, 298-1, 7, 247-1 },
	0,
	16,16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE /*SQ | VIDEO_SUPPORTS_DIRTY*/,
	0,
	williams_vh_start,
	williams_vh_stop,
	williams_vh_screenrefresh,

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


static struct MachineDriver machine_driver_sinistar =
{
	/* basic machine hardware  */
	{
		{
			CPU_M6809,
			1000000,
			williams_readmem,williams_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6808 | CPU_AUDIO_CPU,
			3579000/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	williams_init_machine,

	/* video hardware */
	304, 256,
	{ 6, 298-1, 7, 247-1 },
	0,
	16,16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE /*SQ | VIDEO_SUPPORTS_DIRTY */,
	0,
	williams_vh_start,
	williams_vh_stop,
	williams_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		},
		{
			SOUND_HC55516,
			&sinistar_cvsd_interface
		}
	},

	nvram_handler
};


static struct MachineDriver machine_driver_blaster =
{
	/* basic machine hardware  */
	{
		{
			CPU_M6809,
			1000000,
			blaster_readmem,blaster_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6808 | CPU_AUDIO_CPU,
			3579000/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	williams_init_machine,

	/* video hardware */
	304, 256,
	{ 6, 298-1, 7, 247-1 },
	0,
	16+256,16+256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	blaster_vh_start,
	williams_vh_stop,
	blaster_vh_screenrefresh,

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


static struct MachineDriver machine_driver_williams2 =
{
	/* basic machine hardware  */
	{
		{
			CPU_M6809,
			1000000,
			williams2_readmem,williams2_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6808 | CPU_AUDIO_CPU,
			3579000/4,
			williams2_sound_readmem,williams2_sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	williams2_init_machine,

	/* video hardware */
	288, 256,
	{ 4, 288-1, 8, 248-1 },
	williams2_gfxdecodeinfo,
	16+8*16,16+8*16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	williams2_vh_start,
	williams2_vh_stop,
	williams_vh_screenrefresh,

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


static struct MachineDriver machine_driver_joust2 =
{
	/* basic machine hardware  */
	{
		{
			CPU_M6809,
			1000000,
			williams2_readmem,williams2_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6808 | CPU_AUDIO_CPU,
			3579000/4,
			williams2_sound_readmem,williams2_sound_writemem,0,0,
			ignore_interrupt,1
		},
		SOUND_CPU_WILLIAMS_CVSD
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	joust2_init_machine,

	/* video hardware */
	288, 256,
	{ 4, 288-1, 8, 248-1 },
	williams2_gfxdecodeinfo,
	16+8*16,16+8*16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	williams2_vh_start,
	williams2_vh_stop,
	williams_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		SOUND_WILLIAMS_CVSD
	},

	nvram_handler
};



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_defender(void)
{
	static const UINT32 bank[8] = { 0x0c000, 0x10000, 0x11000, 0x12000, 0x0c000, 0x0c000, 0x0c000, 0x13000 };
	defender_bank_list = bank;

	/* CMOS configuration */
	CONFIGURE_CMOS(0xc400, 0x100);

	/* PIA configuration */
	CONFIGURE_PIAS(defender_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_defndjeu(void)
{
/*
	Note: Please do not remove these comments in BETA versions. They are
			helpful to get the games working. When they will work, useless
			comments may be removed as desired.

	The encryption in this game is one of the silliest I have ever seen.
	I just wondered if the ROMs were encrypted, and figured out how they
	were in just about 5 mins...
	Very simple: bits 0 and 7 are swapped in the ROMs (not sound).

	Game does not work due to bad ROMs 16 and 20. However, the others are
	VERY similar (if not nearly SAME) to MAYDAY and DEFENSE ones (and NOT
	DEFENDER), although MAYDAY ROMs are more similar than DEFENSE ones...
	By putting MAYDAY ROMs and encrypting them, I got a first machine test
	and then, reboot... The test was the random graphic drawings, which
	were bad. Each time the full screen was drawn, the game rebooted.
	Unfortunately, I don't remember which roms I took to get that, and I
	could not get the same result anymore (I did not retry ALL the
	possibilities I did at 01:30am). :-(

	ROM equivalences (not including the sound ROM):

	MAYDAY      MAYDAY (Alternate)    DEFENSE       JEUTEL's Defender
	-----------------------------------------------------------------
	ROMC.BIN    IC03-3.BIN            DFNDR-C.ROM           15
	ROMB.BIN    IC02-2.BIN            DFNDR-B.ROM           16
	ROMA.BIN    IC01-1.BIN            DFNDR-A.ROM           17
	ROMG.BIN    IC07-7D.BIN           DFNDR-G.ROM           18
	ROMF.BIN    IC06-6.BIN            DFNDR-F.ROM           19
	ROME.BIN    IC05-5.BIN            DFNDR-E.ROM           20
	ROMD.BIN    IC04-4.BIN            DFNDR-D.ROM           21
*/

	UINT8 *rom = memory_region(REGION_CPU1);
	int x;

	for (x = 0xd000; x < 0x15000; x++)
	{
		UINT8 src = rom[x];
		rom[x] = (src & 0x7e) | (src >> 7) | (src << 7);
	}
}

#if 0
static void init_defcmnd(void)
{
	static const UINT32 bank[8] = { 0x0c000, 0x10000, 0x11000, 0x12000, 0x13000, 0x0c000, 0x0c000, 0x14000 };
	defender_bank_list = bank;

	/* CMOS configuration */
	CONFIGURE_CMOS(0xc400, 0x100);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}
#endif

static void init_mayday(void)
{
	static const UINT32 bank[8] = { 0x0c000, 0x10000, 0x11000, 0x12000, 0x0c000, 0x0c000, 0x0c000, 0x13000 };
	defender_bank_list = bank;

	/* CMOS configuration */
	CONFIGURE_CMOS(0xc400, 0x100);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);

	/* install a handler to catch protection checks */
	mayday_protection = (UINT8*)install_mem_read_handler(0, 0xa190, 0xa191, mayday_protection_r);
}


static void init_colony7(void)
{
	static const UINT32 bank[8] = { 0x0c000, 0x10000, 0x11000, 0x12000, 0x0c000, 0x0c000, 0x0c000, 0x0c000 };
	defender_bank_list = bank;

	/* CMOS configuration */
	CONFIGURE_CMOS(0xc400, 0x100);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_stargate(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* PIA configuration */
	CONFIGURE_PIAS(stargate_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_joust(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(4, 0, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_muxed_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_robotron(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(4, 0, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_bubbles(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(4, 0, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_splat(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(0, 0, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_dual_muxed_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_sinistar(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(4, 0, 1);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_49way_pia_0_intf, williams_pia_1_intf, sinistar_snd_pia_intf);

	/* install RAM instead of ROM in the Dxxx slot */
	install_mem_read_handler (0, 0xd000, 0xdfff, MRA_RAM);
	install_mem_write_handler(0, 0xd000, 0xdfff, MWA_RAM);
}


static void init_blaster(void)
{
	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(0, 1, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams_49way_pia_0_intf, williams_pia_1_intf, williams_snd_pia_intf);
}


static void init_tshoot(void)
{
	static const UINT8 tilemap_colors[] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };

	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(0, 0, 0);
	CONFIGURE_TILEMAP(0x7f, tilemap_colors, 1, 0, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(tshoot_pia_0_intf, williams2_pia_1_intf, tshoot_snd_pia_intf);
}


static void init_joust2(void)
{
	static const UINT8 tilemap_colors[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(0, 0, 0);
	CONFIGURE_TILEMAP(0xff, tilemap_colors, 0, -2, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams2_muxed_pia_0_intf, joust2_pia_1_intf, williams2_snd_pia_intf);

	/* expand the sound ROMs */
	memcpy(&memory_region(REGION_CPU3)[0x18000], &memory_region(REGION_CPU3)[0x10000], 0x08000);
	memcpy(&memory_region(REGION_CPU3)[0x20000], &memory_region(REGION_CPU3)[0x10000], 0x10000);
	memcpy(&memory_region(REGION_CPU3)[0x38000], &memory_region(REGION_CPU3)[0x30000], 0x08000);
	memcpy(&memory_region(REGION_CPU3)[0x40000], &memory_region(REGION_CPU3)[0x30000], 0x10000);
	memcpy(&memory_region(REGION_CPU3)[0x58000], &memory_region(REGION_CPU3)[0x50000], 0x08000);
	memcpy(&memory_region(REGION_CPU3)[0x60000], &memory_region(REGION_CPU3)[0x50000], 0x10000);
}


static void init_mysticm(void)
{
	static const UINT8 tilemap_colors[] = { 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(0, 0, 0);
	CONFIGURE_TILEMAP(0x7f, tilemap_colors, 1, 0, 1);

	/* PIA configuration */
	CONFIGURE_PIAS(mysticm_pia_0_intf, williams2_pia_1_intf, williams2_snd_pia_intf);

	/* install RAM instead of ROM in the Dxxx slot */
	install_mem_read_handler (0, 0xd000, 0xdfff, MRA_RAM);
	install_mem_write_handler(0, 0xd000, 0xdfff, MWA_RAM);
}


static void init_inferno(void)
{
	static const UINT8 tilemap_colors[] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };

	/* CMOS configuration */
	CONFIGURE_CMOS(0xcc00, 0x400);

	/* video configuration */
	CONFIGURE_BLITTER(0, 0, 0);
	CONFIGURE_TILEMAP(0x7f, tilemap_colors, 1, 0, 0);

	/* PIA configuration */
	CONFIGURE_PIAS(williams2_muxed_pia_0_intf, williams2_pia_1_intf, williams2_snd_pia_intf);

	/* install RAM instead of ROM in the Dxxx slot */
	install_mem_read_handler (0, 0xd000, 0xdfff, MRA_RAM);
	install_mem_write_handler(0, 0xd000, 0xdfff, MWA_RAM);
}



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( defender )
	ROM_REGION( 0x14000, REGION_CPU1 )
	ROM_LOAD( "defend.1",     0x0d000, 0x0800, 0xc3e52d7e )
	ROM_LOAD( "defend.4",     0x0d800, 0x0800, 0x9a72348b )
	ROM_LOAD( "defend.2",     0x0e000, 0x1000, 0x89b75984 )
	ROM_LOAD( "defend.3",     0x0f000, 0x1000, 0x94f51e9b )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "defend.9",     0x10000, 0x0800, 0x6870e8a5 )
	ROM_LOAD( "defend.12",    0x10800, 0x0800, 0xf1f88938 )
	ROM_LOAD( "defend.8",     0x11000, 0x0800, 0xb649e306 )
	ROM_LOAD( "defend.11",    0x11800, 0x0800, 0x9deaf6d9 )
	ROM_LOAD( "defend.7",     0x12000, 0x0800, 0x339e092e )
	ROM_LOAD( "defend.10",    0x12800, 0x0800, 0xa543b167 )
	ROM_RELOAD(               0x13800, 0x0800 )
	ROM_LOAD( "defend.6",     0x13000, 0x0800, 0x65f4efd1 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "defend.snd",   0xf800, 0x0800, 0xfefd5b48 )
ROM_END


ROM_START( defendg )
	ROM_REGION( 0x14000, REGION_CPU1 )
	ROM_LOAD( "defeng01.bin", 0x0d000, 0x0800, 0x6111d74d )
	ROM_LOAD( "defeng04.bin", 0x0d800, 0x0800, 0x3cfc04ce )
	ROM_LOAD( "defeng02.bin", 0x0e000, 0x1000, 0xd184ab6b )
	ROM_LOAD( "defeng03.bin", 0x0f000, 0x1000, 0x788b76d7 )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "defeng09.bin", 0x10000, 0x0800, 0xf57caa62 )
	ROM_LOAD( "defeng12.bin", 0x10800, 0x0800, 0x33db686f )
	ROM_LOAD( "defeng08.bin", 0x11000, 0x0800, 0x9a9eb3d2 )
	ROM_LOAD( "defeng11.bin", 0x11800, 0x0800, 0x5ca4e860 )
	ROM_LOAD( "defeng07.bin", 0x12000, 0x0800, 0x545c3326 )
	ROM_LOAD( "defeng10.bin", 0x12800, 0x0800, 0x941cf34e )
	ROM_RELOAD(               0x13800, 0x0800 )
	ROM_LOAD( "defeng06.bin", 0x13000, 0x0800, 0x3af34c05 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "defend.snd",   0xf800, 0x0800, 0xfefd5b48 )
ROM_END


ROM_START( defendw )
	ROM_REGION( 0x14000, REGION_CPU1 )
	ROM_LOAD( "wb01.bin",     0x0d000, 0x1000, 0x0ee1019d )
	ROM_LOAD( "defeng02.bin", 0x0e000, 0x1000, 0xd184ab6b )
	ROM_LOAD( "wb03.bin",     0x0f000, 0x1000, 0xa732d649 )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "defeng09.bin", 0x10000, 0x0800, 0xf57caa62 )
	ROM_LOAD( "defeng12.bin", 0x10800, 0x0800, 0x33db686f )
	ROM_LOAD( "defeng08.bin", 0x11000, 0x0800, 0x9a9eb3d2 )
	ROM_LOAD( "defeng11.bin", 0x11800, 0x0800, 0x5ca4e860 )
	ROM_LOAD( "defeng07.bin", 0x12000, 0x0800, 0x545c3326 )
	ROM_LOAD( "defeng10.bin", 0x12800, 0x0800, 0x941cf34e )
	ROM_RELOAD(               0x13800, 0x0800 )
	ROM_LOAD( "defeng06.bin", 0x13000, 0x0800, 0x3af34c05 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "defend.snd",   0xf800, 0x0800, 0xfefd5b48 )
ROM_END


ROM_START( defndjeu )
	ROM_REGION( 0x15000, REGION_CPU1 )
	ROM_LOAD( "15", 0x0d000, 0x1000, 0x706a24bd )
	ROM_LOAD( "16", 0x0e000, 0x1000, 0x03201532 )
	ROM_LOAD( "17", 0x0f000, 0x1000, 0x25287eca )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "18", 0x10000, 0x1000, 0xe99d5679 )
	ROM_LOAD( "19", 0x11000, 0x1000, 0x769f5984 )
	ROM_LOAD( "20", 0x12000, 0x1000, 0x12fa0788 )
	ROM_LOAD( "21", 0x13000, 0x1000, 0xbddb71a3 )
	ROM_RELOAD(     0x14000, 0x1000 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "s", 0xf800, 0x0800, 0xcb79ae42 )
ROM_END


ROM_START( defcmnd )
	ROM_REGION( 0x15000, REGION_CPU1 )
	ROM_LOAD( "defcmnda.1",   0x0d000, 0x1000, 0x68effc1d )
	ROM_LOAD( "defcmnda.2",   0x0e000, 0x1000, 0x1126adc9 )
	ROM_LOAD( "defcmnda.3",   0x0f000, 0x1000, 0x7340209d )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "defcmnda.10",  0x10000, 0x0800, 0x3dddae75 )
	ROM_LOAD( "defcmnda.7",   0x10800, 0x0800, 0x3f1e7cf8 )
	ROM_LOAD( "defcmnda.9",   0x11000, 0x0800, 0x8882e1ff )
	ROM_LOAD( "defcmnda.6",   0x11800, 0x0800, 0xd068f0c5 )
	ROM_LOAD( "defcmnda.8",   0x12000, 0x0800, 0xfef4cb77 )
	ROM_LOAD( "defcmnda.5",   0x12800, 0x0800, 0x49b50b40 )
	ROM_LOAD( "defcmnda.4",   0x13000, 0x0800, 0x43d42a1b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "defcmnda.snd", 0xf800, 0x0800, 0xf122d9c9 )
ROM_END


ROM_START( defence )
	ROM_REGION( 0x15000, REGION_CPU1 )
	ROM_LOAD( "1",            0x0d000, 0x1000, 0xebc93622 )
	ROM_LOAD( "2",            0x0e000, 0x1000, 0x2a4f4f44 )
	ROM_LOAD( "3",            0x0f000, 0x1000, 0xa4112f91 )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "0",            0x10000, 0x0800, 0x7a1e5998 )
	ROM_LOAD( "7",            0x10800, 0x0800, 0x4c2616a3 )
	ROM_LOAD( "9",            0x11000, 0x0800, 0x7b146003 )
	ROM_LOAD( "6",            0x11800, 0x0800, 0x6d748030 )
	ROM_LOAD( "8",            0x12000, 0x0800, 0x52d5438b )
	ROM_LOAD( "5",            0x12800, 0x0800, 0x4a270340 )
	ROM_LOAD( "4",            0x13000, 0x0800, 0xe13f457c )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "defcmnda.snd", 0xf800, 0x0800, 0xf122d9c9 )
ROM_END


ROM_START( mayday )
	ROM_REGION( 0x15000, REGION_CPU1 )
	ROM_LOAD( "ic03-3.bin",  0x0d000, 0x1000, 0xa1ff6e62 )
	ROM_LOAD( "ic02-2.bin",  0x0e000, 0x1000, 0x62183aea )
	ROM_LOAD( "ic01-1.bin",  0x0f000, 0x1000, 0x5dcb113f )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "ic04-4.bin",  0x10000, 0x1000, 0xea6a4ec8 )
	ROM_LOAD( "ic05-5.bin",  0x11000, 0x1000, 0x0d797a3e )
	ROM_LOAD( "ic06-6.bin",  0x12000, 0x1000, 0xee8bfcd6 )
	ROM_LOAD( "ic07-7d.bin", 0x13000, 0x1000, 0xd9c065e7 )
	ROM_RELOAD(              0x14000, 0x1000 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "ic28-8.bin",  0xf800, 0x0800, 0xfefd5b48 )
	/* Sound ROM is same in both versions. Can be merged !!! */
ROM_END


ROM_START( maydaya )
	ROM_REGION( 0x15000, REGION_CPU1 )
	ROM_LOAD( "mayday.c", 0x0d000, 0x1000, 0x872a2f2d )
	ROM_LOAD( "mayday.b", 0x0e000, 0x1000, 0xc4ab5e22 )
	ROM_LOAD( "mayday.a", 0x0f000, 0x1000, 0x329a1318 )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "mayday.d", 0x10000, 0x1000, 0xc2ae4716 )
	ROM_LOAD( "mayday.e", 0x11000, 0x1000, 0x41225666 )
	ROM_LOAD( "mayday.f", 0x12000, 0x1000, 0xc39be3c0 )
	ROM_LOAD( "mayday.g", 0x13000, 0x1000, 0x2bd0f106 )
	ROM_RELOAD(           0x14000, 0x1000 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "ic28-8.bin", 0xf800, 0x0800, 0xfefd5b48 )
ROM_END


ROM_START( colony7 )
	ROM_REGION( 0x14000, REGION_CPU1 )
	ROM_LOAD( "cs03.bin",     0x0d000, 0x1000, 0x7ee75ae5 )
	ROM_LOAD( "cs02.bin",     0x0e000, 0x1000, 0xc60b08cb )
	ROM_LOAD( "cs01.bin",     0x0f000, 0x1000, 0x1bc97436 )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "cs06.bin",     0x10000, 0x0800, 0x318b95af )
	ROM_LOAD( "cs04.bin",     0x10800, 0x0800, 0xd740faee )
	ROM_LOAD( "cs07.bin",     0x11000, 0x0800, 0x0b23638b )
	ROM_LOAD( "cs05.bin",     0x11800, 0x0800, 0x59e406a8 )
	ROM_LOAD( "cs08.bin",     0x12000, 0x0800, 0x3bfde87a )
	ROM_RELOAD(           0x12800, 0x0800 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "cs11.bin",     0xf800, 0x0800, 0x6032293c ) /* Sound ROM */
ROM_END


ROM_START( colony7a )
	ROM_REGION( 0x14000, REGION_CPU1 )
	ROM_LOAD( "cs03a.bin",    0x0d000, 0x1000, 0xe0b0d23b )
	ROM_LOAD( "cs02a.bin",    0x0e000, 0x1000, 0x370c6f41 )
	ROM_LOAD( "cs01a.bin",    0x0f000, 0x1000, 0xba299946 )
	/* bank 0 is the place for CMOS ram */
	ROM_LOAD( "cs06.bin",     0x10000, 0x0800, 0x318b95af )
	ROM_LOAD( "cs04.bin",     0x10800, 0x0800, 0xd740faee )
	ROM_LOAD( "cs07.bin",     0x11000, 0x0800, 0x0b23638b )
	ROM_LOAD( "cs05.bin",     0x11800, 0x0800, 0x59e406a8 )
	ROM_LOAD( "cs08.bin",     0x12000, 0x0800, 0x3bfde87a )
	ROM_RELOAD(            0x12800, 0x0800 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "cs11.bin",     0xf800, 0x0800, 0x6032293c ) /* Sound ROM */
ROM_END


ROM_START( stargate )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "01",           0x0000, 0x1000, 0x88824d18 )
	ROM_LOAD( "02",           0x1000, 0x1000, 0xafc614c5 )
	ROM_LOAD( "03",           0x2000, 0x1000, 0x15077a9d )
	ROM_LOAD( "04",           0x3000, 0x1000, 0xa8b4bf0f )
	ROM_LOAD( "05",           0x4000, 0x1000, 0x2d306074 )
	ROM_LOAD( "06",           0x5000, 0x1000, 0x53598dde )
	ROM_LOAD( "07",           0x6000, 0x1000, 0x23606060 )
	ROM_LOAD( "08",           0x7000, 0x1000, 0x4ec490c7 )
	ROM_LOAD( "09",           0x8000, 0x1000, 0x88187b64 )
	ROM_LOAD( "10",           0xd000, 0x1000, 0x60b07ff7 )
	ROM_LOAD( "11",           0xe000, 0x1000, 0x7d2c5daf )
	ROM_LOAD( "12",           0xf000, 0x1000, 0xa0396670 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "sg.snd",       0xf800, 0x0800, 0x2fcf6c4d )
ROM_END


ROM_START( joust )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "joust.wg1",    0x0000, 0x1000, 0xfe41b2af )
	ROM_LOAD( "joust.wg2",    0x1000, 0x1000, 0x501c143c )
	ROM_LOAD( "joust.wg3",    0x2000, 0x1000, 0x43f7161d )
	ROM_LOAD( "joust.wg4",    0x3000, 0x1000, 0xdb5571b6 )
	ROM_LOAD( "joust.wg5",    0x4000, 0x1000, 0xc686bb6b )
	ROM_LOAD( "joust.wg6",    0x5000, 0x1000, 0xfac5f2cf )
	ROM_LOAD( "joust.wg7",    0x6000, 0x1000, 0x81418240 )
	ROM_LOAD( "joust.wg8",    0x7000, 0x1000, 0xba5359ba )
	ROM_LOAD( "joust.wg9",    0x8000, 0x1000, 0x39643147 )
	ROM_LOAD( "joust.wga",    0xd000, 0x1000, 0x3f1c4f89 )
	ROM_LOAD( "joust.wgb",    0xe000, 0x1000, 0xea48b359 )
	ROM_LOAD( "joust.wgc",    0xf000, 0x1000, 0xc710717b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "joust.snd",    0xf000, 0x1000, 0xf1835bdd )
ROM_END


ROM_START( joustwr )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "joust.wg1",    0x0000, 0x1000, 0xfe41b2af )
	ROM_LOAD( "joust.wg2",    0x1000, 0x1000, 0x501c143c )
	ROM_LOAD( "joust.wg3",    0x2000, 0x1000, 0x43f7161d )
	ROM_LOAD( "joust.wg4",    0x3000, 0x1000, 0xdb5571b6 )
	ROM_LOAD( "joust.wg5",    0x4000, 0x1000, 0xc686bb6b )
	ROM_LOAD( "joust.wg6",    0x5000, 0x1000, 0xfac5f2cf )
	ROM_LOAD( "joust.wr7",    0x6000, 0x1000, 0xe6f439c4 )
	ROM_LOAD( "joust.wg8",    0x7000, 0x1000, 0xba5359ba )
	ROM_LOAD( "joust.wg9",    0x8000, 0x1000, 0x39643147 )
	ROM_LOAD( "joust.wra",    0xd000, 0x1000, 0x2039014a )
	ROM_LOAD( "joust.wgb",    0xe000, 0x1000, 0xea48b359 )
	ROM_LOAD( "joust.wgc",    0xf000, 0x1000, 0xc710717b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "joust.snd",    0xf000, 0x1000, 0xf1835bdd )
ROM_END


ROM_START( joustr )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "joust.wg1",    0x0000, 0x1000, 0xfe41b2af )
	ROM_LOAD( "joust.wg2",    0x1000, 0x1000, 0x501c143c )
	ROM_LOAD( "joust.wg3",    0x2000, 0x1000, 0x43f7161d )
	ROM_LOAD( "joust.sr4",    0x3000, 0x1000, 0xab347170 )
	ROM_LOAD( "joust.wg5",    0x4000, 0x1000, 0xc686bb6b )
	ROM_LOAD( "joust.sr6",    0x5000, 0x1000, 0x3d9a6fac )
	ROM_LOAD( "joust.sr7",    0x6000, 0x1000, 0x0a70b3d1 )
	ROM_LOAD( "joust.sr8",    0x7000, 0x1000, 0xa7f01504 )
	ROM_LOAD( "joust.sr9",    0x8000, 0x1000, 0x978687ad )
	ROM_LOAD( "joust.sra",    0xd000, 0x1000, 0xc0c6e52a )
	ROM_LOAD( "joust.srb",    0xe000, 0x1000, 0xab11bcf9 )
	ROM_LOAD( "joust.src",    0xf000, 0x1000, 0xea14574b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "joust.snd",    0xf000, 0x1000, 0xf1835bdd )
ROM_END


ROM_START( robotron )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "robotron.sb1", 0x0000, 0x1000, 0x66c7d3ef )
	ROM_LOAD( "robotron.sb2", 0x1000, 0x1000, 0x5bc6c614 )
	ROM_LOAD( "robotron.sb3", 0x2000, 0x1000, 0xe99a82be )
	ROM_LOAD( "robotron.sb4", 0x3000, 0x1000, 0xafb1c561 )
	ROM_LOAD( "robotron.sb5", 0x4000, 0x1000, 0x62691e77 )
	ROM_LOAD( "robotron.sb6", 0x5000, 0x1000, 0xbd2c853d )
	ROM_LOAD( "robotron.sb7", 0x6000, 0x1000, 0x49ac400c )
	ROM_LOAD( "robotron.sb8", 0x7000, 0x1000, 0x3a96e88c )
	ROM_LOAD( "robotron.sb9", 0x8000, 0x1000, 0xb124367b )
	ROM_LOAD( "robotron.sba", 0xd000, 0x1000, 0x13797024 )
	ROM_LOAD( "robotron.sbb", 0xe000, 0x1000, 0x7e3c1b87 )
	ROM_LOAD( "robotron.sbc", 0xf000, 0x1000, 0x645d543e )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "robotron.snd", 0xf000, 0x1000, 0xc56c1d28 )
ROM_END


ROM_START( robotryo )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "robotron.sb1", 0x0000, 0x1000, 0x66c7d3ef )
	ROM_LOAD( "robotron.sb2", 0x1000, 0x1000, 0x5bc6c614 )
	ROM_LOAD( "robotron.yo3", 0x2000, 0x1000, 0x67a369bc )
	ROM_LOAD( "robotron.yo4", 0x3000, 0x1000, 0xb0de677a )
	ROM_LOAD( "robotron.yo5", 0x4000, 0x1000, 0x24726007 )
	ROM_LOAD( "robotron.yo6", 0x5000, 0x1000, 0x028181a6 )
	ROM_LOAD( "robotron.yo7", 0x6000, 0x1000, 0x4dfcceae )
	ROM_LOAD( "robotron.sb8", 0x7000, 0x1000, 0x3a96e88c )
	ROM_LOAD( "robotron.sb9", 0x8000, 0x1000, 0xb124367b )
	ROM_LOAD( "robotron.yoa", 0xd000, 0x1000, 0x4a9d5f52 )
	ROM_LOAD( "robotron.yob", 0xe000, 0x1000, 0x2afc5e7f )
	ROM_LOAD( "robotron.yoc", 0xf000, 0x1000, 0x45da9202 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "robotron.snd", 0xf000, 0x1000, 0xc56c1d28 )
ROM_END


ROM_START( bubbles )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "bubbles.1b",   0x0000, 0x1000, 0x8234f55c )
	ROM_LOAD( "bubbles.2b",   0x1000, 0x1000, 0x4a188d6a )
	ROM_LOAD( "bubbles.3b",   0x2000, 0x1000, 0x7728f07f )
	ROM_LOAD( "bubbles.4b",   0x3000, 0x1000, 0x040be7f9 )
	ROM_LOAD( "bubbles.5b",   0x4000, 0x1000, 0x0b5f29e0 )
	ROM_LOAD( "bubbles.6b",   0x5000, 0x1000, 0x4dd0450d )
	ROM_LOAD( "bubbles.7b",   0x6000, 0x1000, 0xe0a26ec0 )
	ROM_LOAD( "bubbles.8b",   0x7000, 0x1000, 0x4fd23d8d )
	ROM_LOAD( "bubbles.9b",   0x8000, 0x1000, 0xb48559fb )
	ROM_LOAD( "bubbles.10b",  0xd000, 0x1000, 0x26e7869b )
	ROM_LOAD( "bubbles.11b",  0xe000, 0x1000, 0x5a5b572f )
	ROM_LOAD( "bubbles.12b",  0xf000, 0x1000, 0xce22d2e2 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "bubbles.snd",  0xf000, 0x1000, 0x689ce2aa )
ROM_END


ROM_START( bubblesr )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "bubblesr.1b",  0x0000, 0x1000, 0xdda4e782 )
	ROM_LOAD( "bubblesr.2b",  0x1000, 0x1000, 0x3c8fa7f5 )
	ROM_LOAD( "bubblesr.3b",  0x2000, 0x1000, 0xf869bb9c )
	ROM_LOAD( "bubblesr.4b",  0x3000, 0x1000, 0x0c65eaab )
	ROM_LOAD( "bubblesr.5b",  0x4000, 0x1000, 0x7ece4e13 )
	ROM_LOAD( "bubbles.6b",   0x5000, 0x1000, 0x4dd0450d )
	ROM_LOAD( "bubbles.7b",   0x6000, 0x1000, 0xe0a26ec0 )
	ROM_LOAD( "bubblesr.8b",  0x7000, 0x1000, 0x598b9bd6 )
	ROM_LOAD( "bubbles.9b",   0x8000, 0x1000, 0xb48559fb )
	ROM_LOAD( "bubblesr.10b", 0xd000, 0x1000, 0x8b396db0 )
	ROM_LOAD( "bubblesr.11b", 0xe000, 0x1000, 0x096af43e )
	ROM_LOAD( "bubblesr.12b", 0xf000, 0x1000, 0x5c1244ef )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "bubbles.snd",  0xf000, 0x1000, 0x689ce2aa )
ROM_END


ROM_START( splat )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "splat.01",     0x0000, 0x1000, 0x1cf26e48 )
	ROM_LOAD( "splat.02",     0x1000, 0x1000, 0xac0d4276 )
	ROM_LOAD( "splat.03",     0x2000, 0x1000, 0x74873e59 )
	ROM_LOAD( "splat.04",     0x3000, 0x1000, 0x70a7064e )
	ROM_LOAD( "splat.05",     0x4000, 0x1000, 0xc6895221 )
	ROM_LOAD( "splat.06",     0x5000, 0x1000, 0xea4ab7fd )
	ROM_LOAD( "splat.07",     0x6000, 0x1000, 0x82fd8713 )
	ROM_LOAD( "splat.08",     0x7000, 0x1000, 0x7dded1b4 )
	ROM_LOAD( "splat.09",     0x8000, 0x1000, 0x71cbfe5a )
	ROM_LOAD( "splat.10",     0xd000, 0x1000, 0xd1a1f632 )
	ROM_LOAD( "splat.11",     0xe000, 0x1000, 0xca8cde95 )
	ROM_LOAD( "splat.12",     0xf000, 0x1000, 0x5bee3e60 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "splat.snd",    0xf000, 0x1000, 0xa878d5f3 )
ROM_END


ROM_START( sinistar )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "sinistar.01",  0x0000, 0x1000, 0xf6f3a22c )
	ROM_LOAD( "sinistar.02",  0x1000, 0x1000, 0xcab3185c )
	ROM_LOAD( "sinistar.03",  0x2000, 0x1000, 0x1ce1b3cc )
	ROM_LOAD( "sinistar.04",  0x3000, 0x1000, 0x6da632ba )
	ROM_LOAD( "sinistar.05",  0x4000, 0x1000, 0xb662e8fc )
	ROM_LOAD( "sinistar.06",  0x5000, 0x1000, 0x2306183d )
	ROM_LOAD( "sinistar.07",  0x6000, 0x1000, 0xe5dd918e )
	ROM_LOAD( "sinistar.08",  0x7000, 0x1000, 0x4785a787 )
	ROM_LOAD( "sinistar.09",  0x8000, 0x1000, 0x50cb63ad )
	ROM_LOAD( "sinistar.10",  0xe000, 0x1000, 0x3d670417 )
	ROM_LOAD( "sinistar.11",  0xf000, 0x1000, 0x3162bc50 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "speech.ic7",   0xb000, 0x1000, 0xe1019568 )
	ROM_LOAD( "speech.ic5",   0xc000, 0x1000, 0xcf3b5ffd )
	ROM_LOAD( "speech.ic6",   0xd000, 0x1000, 0xff8d2645 )
	ROM_LOAD( "speech.ic4",   0xe000, 0x1000, 0x4b56a626 )
	ROM_LOAD( "sinistar.snd", 0xf000, 0x1000, 0xb82f4ddb )
ROM_END


ROM_START( sinista1 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "sinrev1.01",   0x0000, 0x1000, 0x3810d7b8 )
	ROM_LOAD( "sinistar.02",  0x1000, 0x1000, 0xcab3185c )
	ROM_LOAD( "sinrev1.03",   0x2000, 0x1000, 0x7c984ca9 )
	ROM_LOAD( "sinrev1.04",   0x3000, 0x1000, 0xcc6c4f24 )
	ROM_LOAD( "sinrev1.05",   0x4000, 0x1000, 0x12285bfe )
	ROM_LOAD( "sinrev1.06",   0x5000, 0x1000, 0x7a675f35 )
	ROM_LOAD( "sinrev1.07",   0x6000, 0x1000, 0xb0463243 )
	ROM_LOAD( "sinrev1.08",   0x7000, 0x1000, 0x909040d4 )
	ROM_LOAD( "sinrev1.09",   0x8000, 0x1000, 0xcc949810 )
	ROM_LOAD( "sinrev1.10",   0xe000, 0x1000, 0xea87a53f )
	ROM_LOAD( "sinrev1.11",   0xf000, 0x1000, 0x88d36e80 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "speech.ic7",   0xb000, 0x1000, 0xe1019568 )
	ROM_LOAD( "speech.ic5",   0xc000, 0x1000, 0xcf3b5ffd )
	ROM_LOAD( "speech.ic6",   0xd000, 0x1000, 0xff8d2645 )
	ROM_LOAD( "speech.ic4",   0xe000, 0x1000, 0x4b56a626 )
	ROM_LOAD( "sinistar.snd", 0xf000, 0x1000, 0xb82f4ddb )
ROM_END


ROM_START( sinista2 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "sinistar.01",  0x0000, 0x1000, 0xf6f3a22c )
	ROM_LOAD( "sinistar.02",  0x1000, 0x1000, 0xcab3185c )
	ROM_LOAD( "sinistar.03",  0x2000, 0x1000, 0x1ce1b3cc )
	ROM_LOAD( "sinistar.04",  0x3000, 0x1000, 0x6da632ba )
	ROM_LOAD( "sinistar.05",  0x4000, 0x1000, 0xb662e8fc )
	ROM_LOAD( "sinistar.06",  0x5000, 0x1000, 0x2306183d )
	ROM_LOAD( "sinistar.07",  0x6000, 0x1000, 0xe5dd918e )
	ROM_LOAD( "sinrev2.08",   0x7000, 0x1000, 0xd7ecee45 )
	ROM_LOAD( "sinistar.09",  0x8000, 0x1000, 0x50cb63ad )
	ROM_LOAD( "sinistar.10",  0xe000, 0x1000, 0x3d670417 )
	ROM_LOAD( "sinrev2.11",   0xf000, 0x1000, 0x792c8b00 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "speech.ic7",   0xb000, 0x1000, 0xe1019568 )
	ROM_LOAD( "speech.ic5",   0xc000, 0x1000, 0xcf3b5ffd )
	ROM_LOAD( "speech.ic6",   0xd000, 0x1000, 0xff8d2645 )
	ROM_LOAD( "speech.ic4",   0xe000, 0x1000, 0x4b56a626 )
	ROM_LOAD( "sinistar.snd", 0xf000, 0x1000, 0xb82f4ddb )
ROM_END


ROM_START( blaster )
	ROM_REGION( 0x3c000, REGION_CPU1 )
	ROM_LOAD( "blaster.11",   0x04000, 0x2000, 0x6371e62f )
	ROM_LOAD( "blaster.12",   0x06000, 0x2000, 0x9804faac )
	ROM_LOAD( "blaster.17",   0x08000, 0x1000, 0xbf96182f )
	ROM_LOAD( "blaster.16",   0x0d000, 0x1000, 0x54a40b21 )
	ROM_LOAD( "blaster.13",   0x0e000, 0x2000, 0xf4dae4c8 )

	ROM_LOAD( "blaster.15",   0x00000, 0x4000, 0x1ad146a4 )
	ROM_LOAD( "blaster.8",    0x10000, 0x4000, 0xf110bbb0 )
	ROM_LOAD( "blaster.9",    0x14000, 0x4000, 0x5c5b0f8a )
	ROM_LOAD( "blaster.10",   0x18000, 0x4000, 0xd47eb67f )
	ROM_LOAD( "blaster.6",    0x1c000, 0x4000, 0x47fc007e )
	ROM_LOAD( "blaster.5",    0x20000, 0x4000, 0x15c1b94d )
	ROM_LOAD( "blaster.14",   0x24000, 0x4000, 0xaea6b846 )
	ROM_LOAD( "blaster.7",    0x28000, 0x4000, 0x7a101181 )
	ROM_LOAD( "blaster.1",    0x2c000, 0x4000, 0x8d0ea9e7 )
	ROM_LOAD( "blaster.2",    0x30000, 0x4000, 0x03c4012c )
	ROM_LOAD( "blaster.4",    0x34000, 0x4000, 0xfc9d39fb )
	ROM_LOAD( "blaster.3",    0x38000, 0x4000, 0x253690fb )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the sound CPU */
	ROM_LOAD( "blaster.18",   0xf000, 0x1000, 0xc33a3145 )

	ROM_REGION( 0x0800, REGION_PROMS )		/* color PROM data */
	ROM_LOAD( "blaster.col",  0x0000, 0x0800, 0xbac50bc4 )
ROM_END


ROM_START( tshoot )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "rom18.cpu", 0x0D000, 0x1000, 0xeffc33f1 )	/* IC55 */
	ROM_LOAD( "rom2.cpu",  0x0E000, 0x1000, 0xfd982687 )	/* IC9	*/
	ROM_LOAD( "rom3.cpu",  0x0F000, 0x1000, 0x9617054d )	/* IC10 */

	ROM_LOAD( "rom11.cpu", 0x10000, 0x2000, 0x60d5fab8 )	/* IC18 */
	ROM_LOAD( "rom9.cpu",  0x12000, 0x2000, 0xa4dd4a0e )	/* IC16 */
	ROM_LOAD( "rom7.cpu",  0x14000, 0x2000, 0xf25505e6 )	/* IC14 */
	ROM_LOAD( "rom5.cpu",  0x16000, 0x2000, 0x94a7c0ed )	/* IC12 */

	ROM_LOAD( "rom17.cpu", 0x20000, 0x2000, 0xb02d1ccd )	/* IC26 */
	ROM_LOAD( "rom15.cpu", 0x22000, 0x2000, 0x11709935 )	/* IC24 */

	ROM_LOAD( "rom10.cpu", 0x30000, 0x2000, 0x0f32bad8 )	/* IC17 */
	ROM_LOAD( "rom8.cpu",  0x32000, 0x2000, 0xe9b6cbf7 )	/* IC15 */
	ROM_LOAD( "rom6.cpu",  0x34000, 0x2000, 0xa49f617f )	/* IC13 */
	ROM_LOAD( "rom4.cpu",  0x36000, 0x2000, 0xb026dc00 )	/* IC11 */

	ROM_LOAD( "rom16.cpu", 0x40000, 0x2000, 0x69ce38f8 )	/* IC25 */
	ROM_LOAD( "rom14.cpu", 0x42000, 0x2000, 0x769a4ae5 )	/* IC23 */
	ROM_LOAD( "rom13.cpu", 0x44000, 0x2000, 0xec016c9b )	/* IC21 */
	ROM_LOAD( "rom12.cpu", 0x46000, 0x2000, 0x98ae7afa )	/* IC19 */

	/* sound CPU */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "rom1.cpu", 0xE000, 0x2000, 0x011a94a7 )		/* IC8	*/

	ROM_REGION( 0xc000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rom20.cpu", 0x00000, 0x2000, 0xc6e1d253 )	/* IC57 */
	ROM_LOAD( "rom21.cpu", 0x04000, 0x2000, 0x9874e90f )	/* IC58 */
	ROM_LOAD( "rom19.cpu", 0x08000, 0x2000, 0xb9ce4d2a )	/* IC41 */
ROM_END


ROM_START( joust2 )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "ic55_r1.cpu", 0x0D000, 0x1000, 0x08b0d5bd )	/* IC55 ROM02 */
	ROM_LOAD( "ic09_r2.cpu", 0x0E000, 0x1000, 0x951175ce )	/* IC09 ROM03 */
	ROM_LOAD( "ic10_r2.cpu", 0x0F000, 0x1000, 0xba6e0f6c )	/* IC10 ROM04 */

	ROM_LOAD( "ic18_r1.cpu", 0x10000, 0x2000, 0x9dc986f9 )	/* IC18 ROM11 */
	ROM_LOAD( "ic16_r2.cpu", 0x12000, 0x2000, 0x56e2b550 )	/* IC16 ROM09 */
	ROM_LOAD( "ic14_r2.cpu", 0x14000, 0x2000, 0xf3bce576 )	/* IC14 ROM07 */
	ROM_LOAD( "ic12_r2.cpu", 0x16000, 0x2000, 0x5f8b4919 )	/* IC12 ROM05 */

	ROM_LOAD( "ic26_r1.cpu", 0x20000, 0x2000, 0x4ef5e805 )	/* IC26 ROM19 */
	ROM_LOAD( "ic24_r1.cpu", 0x22000, 0x2000, 0x4861f063 )	/* IC24 ROM17 */
	ROM_LOAD( "ic22_r1.cpu", 0x24000, 0x2000, 0x421aafa8 )	/* IC22 ROM15 */
	ROM_LOAD( "ic20_r1.cpu", 0x26000, 0x2000, 0x3432ff55 )	/* IC20 ROM13 */

	ROM_LOAD( "ic17_r1.cpu", 0x30000, 0x2000, 0x3e01b597 )	/* IC17 ROM10 */
	ROM_LOAD( "ic15_r1.cpu", 0x32000, 0x2000, 0xff26fb29 )	/* IC15 ROM08 */
	ROM_LOAD( "ic13_r2.cpu", 0x34000, 0x2000, 0x5f107db5 )	/* IC13 ROM06 */

	ROM_LOAD( "ic25_r1.cpu", 0x40000, 0x2000, 0x47580af5 )	/* IC25 ROM18 */
	ROM_LOAD( "ic23_r1.cpu", 0x42000, 0x2000, 0x869b5942 )	/* IC23 ROM16 */
	ROM_LOAD( "ic21_r1.cpu", 0x44000, 0x2000, 0x0bbd867c )	/* IC21 ROM14 */
	ROM_LOAD( "ic19_r1.cpu", 0x46000, 0x2000, 0xb9221ed1 )	/* IC19 ROM12 */

	/* sound CPU */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "ic08_r1.cpu", 0x0E000, 0x2000, 0x84517c3c )	/* IC08 ROM08 */

	/* sound board */
	ROM_REGION( 0x70000, REGION_CPU3 )
	ROM_LOAD( "u04_r1.snd", 0x10000, 0x8000, 0x3af6b47d )	/* IC04 ROM23 */
	ROM_LOAD( "u19_r1.snd", 0x30000, 0x8000, 0xe7f9ed2e )	/* IC19 ROM24 */
	ROM_LOAD( "u20_r1.snd", 0x50000, 0x8000, 0xc85b29f7 )	/* IC20 ROM25 */

	ROM_REGION( 0xc000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic57_r1.vid", 0x00000, 0x4000, 0x572c6b01 )	/* IC57 ROM20 */
	ROM_LOAD( "ic58_r1.vid", 0x04000, 0x4000, 0xaa94bf05 )	/* IC58 ROM21 */
	ROM_LOAD( "ic41_r1.vid", 0x08000, 0x4000, 0xc41e3daa )	/* IC41 ROM22 */
ROM_END


ROM_START( mysticm )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "mm02_2.a09", 0x0E000, 0x1000, 0x3a776ea8 )	/* IC9	*/
	ROM_LOAD( "mm03_2.a10", 0x0F000, 0x1000, 0x6e247c75 )	/* IC10 */

	ROM_LOAD( "mm11_1.a18", 0x10000, 0x2000, 0xf537968e )	/* IC18 */
	ROM_LOAD( "mm09_1.a16", 0x12000, 0x2000, 0x3bd12f6c )	/* IC16 */
	ROM_LOAD( "mm07_1.a14", 0x14000, 0x2000, 0xea2a2a68 )	/* IC14 */
	ROM_LOAD( "mm05_1.a12", 0x16000, 0x2000, 0xb514eef3 )	/* IC12 */

	ROM_LOAD( "mm18_1.a26", 0x20000, 0x2000, 0x9b391a81 )	/* IC26 */
	ROM_LOAD( "mm16_1.a24", 0x22000, 0x2000, 0x399e175d )	/* IC24 */
	ROM_LOAD( "mm14_1.a22", 0x24000, 0x2000, 0x191153b1 )	/* IC22 */

	ROM_LOAD( "mm10_1.a17", 0x30000, 0x2000, 0xd6a37509 )	/* IC17 */
	ROM_LOAD( "mm08_1.a15", 0x32000, 0x2000, 0x6f1a64f2 )	/* IC15 */
	ROM_LOAD( "mm06_1.a13", 0x34000, 0x2000, 0x2e6795d4 )	/* IC13 */
	ROM_LOAD( "mm04_1.a11", 0x36000, 0x2000, 0xc222fb64 )	/* IC11 */

	ROM_LOAD( "mm17_1.a25", 0x40000, 0x2000, 0xd36f0a96 )	/* IC25 */
	ROM_LOAD( "mm15_1.a23", 0x42000, 0x2000, 0xcd5d99da )	/* IC23 */
	ROM_LOAD( "mm13_1.a21", 0x44000, 0x2000, 0xef4b79db )	/* IC21 */
	ROM_LOAD( "mm12_1.a19", 0x46000, 0x2000, 0xa1f04bf0 )	/* IC19 */

	/* sound CPU */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "mm01_1.a08", 0x0E000, 0x2000, 0x65339512 )	/* IC8	*/

	ROM_REGION( 0xc000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mm20_1.b57", 0x00000, 0x2000, 0x5c0f4f46 )	/* IC57 */
	ROM_LOAD( "mm21_1.b58", 0x04000, 0x2000, 0xcb90b3c5 )	/* IC58 */
	ROM_LOAD( "mm19_1.b41", 0x08000, 0x2000, 0xe274df86 )	/* IC41 */
ROM_END


ROM_START( inferno )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "ic9.inf", 0x0E000, 0x1000, 0x1a013185 )		/* IC9	*/
	ROM_LOAD( "ic10.inf", 0x0F000, 0x1000, 0xdbf64a36 ) 	/* IC10 */

	ROM_LOAD( "ic18.inf", 0x10000, 0x2000, 0x95bcf7b1 )		/* IC18 */
	ROM_LOAD( "ic16.inf", 0x12000, 0x2000, 0x8bc4f935 )		/* IC16 */
	ROM_LOAD( "ic14.inf", 0x14000, 0x2000, 0xa70508a7 )		/* IC14 */
	ROM_LOAD( "ic12.inf", 0x16000, 0x2000, 0x7ffb87f9 )		/* IC12 */

	ROM_LOAD( "ic17.inf", 0x30000, 0x2000, 0xb4684139 ) 	/* IC17 */
	ROM_LOAD( "ic15.inf", 0x32000, 0x2000, 0x128a6ad6 ) 	/* IC15 */
	ROM_LOAD( "ic13.inf", 0x34000, 0x2000, 0x83a9e4d6 ) 	/* IC13 */
	ROM_LOAD( "ic11.inf", 0x36000, 0x2000, 0xc2e9c909 ) 	/* IC11 */

	ROM_LOAD( "ic25.inf", 0x40000, 0x2000, 0x103a5951 ) 	/* IC25 */
	ROM_LOAD( "ic23.inf", 0x42000, 0x2000, 0xc04749a0 ) 	/* IC23 */
	ROM_LOAD( "ic21.inf", 0x44000, 0x2000, 0xc405f853 ) 	/* IC21 */
	ROM_LOAD( "ic19.inf", 0x46000, 0x2000, 0xade7645a ) 	/* IC19 */

	/* sound CPU */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "ic8.inf", 0x0E000, 0x2000, 0x4e3123b8 )		/* IC8	*/

	ROM_REGION( 0xc000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic57.inf", 0x00000, 0x2000, 0x65a4ef79 ) 	/* IC57 */
	ROM_LOAD( "ic58.inf", 0x04000, 0x2000, 0x4bb1c2a0 ) 	/* IC58 */
	ROM_LOAD( "ic41.inf", 0x08000, 0x2000, 0xf3f7238f ) 	/* IC41 */
ROM_END



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1980, defender, 0,        defender, defender, defender, ROT0,   "Williams", "Defender (Red label)" )
GAME( 1980, defendg,  defender, defender, defender, defender, ROT0,   "Williams", "Defender (Green label)" )
GAME( 1980, defendw,  defender, defender, defender, defender, ROT0,   "Williams", "Defender (White label)" )
GAMEX(1980, defndjeu, defender, defender, defender, defndjeu, ROT0,   "Jeutel", "Defender ? (bootleg)", GAME_NOT_WORKING )
GAME( 1980, defcmnd,  defender, defender, defender, defender, ROT0,   "bootleg", "Defense Command (set 1)" )
GAME( 1981, defence,  defender, defender, defender, defender, ROT0,   "Outer Limits", "Defence Command" )

GAME( 1980, mayday,   0,        defender, defender, mayday,   ROT0,   "<unknown>", "Mayday (set 1)" )
GAME( 1980, maydaya,  mayday,   defender, defender, mayday,   ROT0,   "<unknown>", "Mayday (set 2)" )

GAME( 1981, colony7,  0,        defender, colony7,  colony7,  ROT270, "Taito", "Colony 7 (set 1)" )
GAME( 1981, colony7a, colony7,  defender, colony7,  colony7,  ROT270, "Taito", "Colony 7 (set 2)" )

GAME( 1981, stargate, 0,        williams, stargate, stargate, ROT0,   "Williams", "Stargate" )

GAME( 1982, robotron, 0,        williams, robotron, robotron, ROT0,   "Williams", "Robotron (Solid Blue label)" )
GAME( 1982, robotryo, robotron, williams, robotron, robotron, ROT0,   "Williams", "Robotron (Yellow/Orange label)" )

GAME( 1982, joust,    0,        williams, joust,    joust,    ROT0,   "Williams", "Joust (White/Green label)" )
GAME( 1982, joustr,   joust,    williams, joust,    joust,    ROT0,   "Williams", "Joust (Solid Red label)" )
GAME( 1982, joustwr,  joust,    williams, joust,    joust,    ROT0,   "Williams", "Joust (White/Red label)" )

GAME( 1982, bubbles,  0,        williams, bubbles,  bubbles,  ROT0,   "Williams", "Bubbles" )
GAME( 1982, bubblesr, bubbles,  williams, bubbles,  bubbles,  ROT0,   "Williams", "Bubbles (Solid Red label)" )

GAME( 1982, splat,    0,        williams, splat,    splat,    ROT0,   "Williams", "Splat!" )

GAME( 1982, sinistar, 0,        sinistar, sinistar, sinistar, ROT270, "Williams", "Sinistar (revision 3)" )
GAME( 1982, sinista1, sinistar, sinistar, sinistar, sinistar, ROT270, "Williams", "Sinistar (prototype version)" )
GAME( 1982, sinista2, sinistar, sinistar, sinistar, sinistar, ROT270, "Williams", "Sinistar (revision 2)" )

GAME( 1983, blaster,  0,        blaster,  blaster,  blaster,  ROT0,   "Williams", "Blaster" )

GAME( 1983, mysticm,  0,        williams2,mysticm,  mysticm,  ROT0,   "Williams", "Mystic Marathon" )
GAME( 1984, tshoot,   0,        williams2,tshoot,   tshoot,   ROT0,   "Williams", "Turkey Shoot" )
GAMEX(1984, inferno,  0,        williams2,inferno,  inferno,  ROT0,   "Williams", "Inferno", GAME_IMPERFECT_SOUND )
GAME( 1986, joust2,   0,        joust2,   joust2,   joust2,   ROT270, "Williams", "Joust 2 - Survival of the Fittest (set 1)" )
