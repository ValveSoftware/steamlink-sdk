#include "../machine/stfight.cpp"
#include "../vidhrdw/stfight.cpp"

/*****************************          (by Mark McDougall)
 *** STREET FIGHT hardware ***	This has been adapted from the excellent
 *****************************  Psychic 5 description (by Roberto Ventura)

Street Fight (c) Seibu Kaihatsu (1986)


0) GENERAL.

The game has two Z80s.
The second CPU controls the two YM2203 sound chips.
There is an OKI M5205 ADPCM chip fed directly from a ROM.
Screen resolution is 256x224 (horizontal CRT).
256 colors on screen.
128 sprites (16x16).


1) ROM CONTENTS.

SF01    Main program code (encrypted)
SF02    Main program code bank-switched? into main cpu space
SF03    Sound program code
SF04    ADPCM voice data
SF05-07 Foreground tile pixel data
SF09    Foreground map data
SF10    Foreground map/tile data
SF11-14 Background tile pixel data
SF15    Background map data
SF16    Background map/tile data
SF17    Character pixel data
SF18-21 Sprite pixel data

All ROMS are 32K except SF17 which is 8K.

Graphics format is a little messy, the 4 planes come from a bit in
the high and low nibble of each byte of a consecutive pair of roms.

All graphics are made of 16x16 (composite) tiles, each of which is composed
of 4 consecutive 8x8 tiles. In all there are 1024 composite (16x16) tiles
for each of the foreground, background and sprite layers. These can be
considered as four banks each of 256 tiles.

Text characters are defined as 8x8 tiles.


2) CPU.

The board has a single? crystal @12.000 MHz.
Both CPU clocks run at 3MHz (12/4).

The main Z80 runs in Interrupt mode 0 (IM0), the game program expects
execution of two different restart (RST) instructions.
RST 10,the main IRQ, is to be triggered each time the screen is refreshed.
RST 08 must be triggered in order to make the game work properly. I haven't
ascertained the exact frequency of this interrupt yet, though the game
appears to run at the correct speed with RST08 at 30Hz. Curiously a trace
on the interrupt pin shows two interrupts occuring at 60Hz, obviously the
VBlank interrupt followed by a second interrupt some 3.3ms later. At some
stage I'll get around to probing the data lines to find the interrupt
vector addresses.

Sound CPU runs in IM1.

The sound CPU lies idle waiting the external IRQ occurrence executing code
from $0100 to $010D.

Game code/data is directly accessible by the main CPU. There is what
appears to be some level data in the second half of sf02 that may
never be bank-switched in!?! Graphics data, ADPCM samples and level maps
are not accessed by the CPUs.

Text video RAM is situated at $D000-$D7FF.


3) MAIN CPU MEMORY MAP.

$0000-$7FFF R   ROM sf01 (encrypted)
$8000-$BFFF R   ROM sf02 (2 x 16k banks selected by D2 of ?)
$C000-$C1FF W   Palette RAM
$C200       R   player 1 controls hard value (negative logic)
                - MSB:x,x,B2,B1,RIGHT,LEFT,DOWN,UP
$C201       R   player 2 controls hard value (negative logic)
                - MSB:x,x,B2,B1,RIGHT,LEFT,DOWN,UP
$C202       R   start buttons (negative logic)
                - MSB:x,x,x,P2,P1,x,x,x:LSB
$C203       R   dipswitch #1 hard value (negative logic)
                - 76543210
                  xxxxx000  - coin A - 1 coin  / 1 credit
                  xxxxx001           - 2 coins / 1 credit
                  xxxxx010           - 1 coin  / 3 credits
                  xxxxx011           - 4 coins / 1 credit
                  xxxxx100           - 1 coin  / 2 credits
                  xxxxx101           - 3 coins / 1 credit
                  xxxxx110           - 1 coin  / 5 credits
                  xxxxx111           - 5 coins / 1 credit
                  xxx00xxx  - coin B - 1 coin  / 1 credit
                  xxx01xxx           - 2 coins / 1 credit
                  xxx10xxx           - 1 coin  / 2 credits
                  xxx11xxx           - 2 coins / 3 credits
                  xx1xxxxx  - test mode setting
                  x1xxxxxx  - continue setting
                  1xxxxxxx  - bullet colour setting
$C204       R   dipswitch #2 hard value (negative logic)
                - 76543210
                  xxxxxxx1  - cabinet style
                  xxxxx11x  - difficulty
                  xxx11xxx  - number of lives (-1)
                  x00xxxxx  - 10,000 & 30,000
                  x01xxxxx  - 20,000 & 40,000
                  x10xxxxx  - 30,000 & 60,000
                  x11xxxxx  - 40,000 & 80,000
                  1xxxxxxx  - demo sound on/off
$C205       R   read to determine coin circuit check status
$C500       W   play fm number
$C600       W   play voice number
$C700       W   ?coin mechanism control?
$C804       W   ?watchdog?
$C806       W   ???
$C807       W   current sprite bank h/w register
                - bank = b2,b0
$D000-$D3FF W   VRAM (Character RAM)
$D400-$D7FF W   VRAM (Attribute RAM)
                - b7    Character bank from SF17
                - b6    Flip Y
                - b5    Flip X
                - b4    ?
                - b3-0  ? Colour/Palette
$D800-$D801 W   foreground layer x coordinate h/w register
$D802-$D803 W   foreground layer y coordinate h/w register
$D804-$D805 W   background layer x coordinate h/w register
$D806,$D808 W   background layer y coordinate h/w register
$D807       W   layer control h/w register
                - b7 = text layer
                - b6 = sprite layer
                - b5 = background layer
                - b4 = foreground layer
                - b0 = video orientation (1=upside-down)
$E000-$FFFF RW  RAM SRM2064C (8k)
$Fxx0-$Fxx3 W   Sprite Ram (every 32 ($20) bytes)
                - xx0 sprite number
                - xx1 sprite attribute
                  - b7  sign extension of x coord
                  - b4  flip x
                - xx2 y coord of sprite
                - xx3 x coord of sprite

4) SOUND CPU MEMORY MAP.

$0000-$7FFF R   ROM (sf03)
$C000       W   YM2203 #1 address register
$C001       W   YM2203 #1 data register
$C800       W   YM2203 #2 address register
$C801       W   YM2203 #2 data register
$E800       W   ??
$F000       R   FM Voice number to play
                - b7    set for valid data latched
                - b6-b0 voice number
$F800-$FFFF RW  RAM


5) COLOR RAM

The palette system is dynamic, the game can show up to 256 different
colors on screen.

Each color component (RGB) depth is 4 bits, two bytes $100 apart are used
for each color code (12 bits).

format: unknown - probably RRRRGGGG - 0000BBBB

I suspect that the colors are organized in sub-palettes, since the graphics
layers are all 4 bits (16 colors) each. Each of the text/graphics layers
have 'attribute' bytes associated with them that would define the palette
usage for each character/tile.

The 16 colours at offset $C0 appear to be the text palette. This group of
colours does not appear to change throughout the game, and the lower 192
colours fade in/out independantly of these 16 - consistent with observations
of the real game. You'd think then that the palette would be reaonably
easy to deduce from the text video ram attribute byte - go ahead and try! :P

The mapping of graphics pixels to palette similarly escapes me, though I
must admit I haven't exhausted all avenues of investigation just yet!

There is a related mystery with the transparency colour. For the most part
colour 15 corresponds to the transparent colour, except in a few cases.

6) TILE-BASED LAYERS

The foreground and background layers comprise static virtual layers which
are 8 screens wide and 16 screens deep. The hardware scrolls around the
layers by reading registers which are updated by sofware every VBlank.
The text layer is fixed and cannot scroll.

The maps that define the foreground and background layers are stored in
ROMs accessed directly by the hardware. They consist of 256 bytes for
each screen which define the tile number, and a corresponding byte in
a matching ROM which defines the tile bank and presumably palette info.

The top and bottom rows of the screen are not visible - resulting in a
256x224 viewport rather than 256 square. The layers can be individually
enabled/disabled. Inactive sprites are 'parked' at row 0.

The rom layout for the foreground and sprite tiles are as you would expect,
with the four 8x8 tiles that make a single composite tile consecutive in
address. The background tiles are interleaved for presumably some good
reason, the first two 8x8 tiles from composite tile n are followed by two
8x8 tiles from the (n+512)'th composite tile.

The map roms are similarly interleaved for the background layer only.

7) SPRITES

The sprites are mapped into RAM locations $F000-$FFFF using only the first
4 bytes from each 32-byte slice. Intervening addresses appear to be
conventional RAM. See the memory map for sprite data format.

 ****************************************************************************

TODO:
- palette is incorporated - fix!!!
- handle transparency in text layer properly (how?)
- second bank of sf02 is this used? (probably NOT)

DONE? (check on real board)
- sound (fm)
- sound (adpcm)

*****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

// machine
void init_empcity(void);
void init_stfight(void);
void stfight_init_machine(void);
int  stfight_vb_interrupt( void );
int  stfight_interrupt_1( void );
int  stfight_interrupt_2( void );
READ_HANDLER( stfight_dsw_r );
WRITE_HANDLER( stfight_fm_w );
WRITE_HANDLER( stfight_voice_w );
READ_HANDLER( stfight_coin_r );
WRITE_HANDLER( stfight_coin_w );
WRITE_HANDLER( stfight_e800_w );
READ_HANDLER( stfight_fm_r );
void stfight_adpcm_int( int data );
WRITE_HANDLER( stfight_adpcm_control_w );

// vidhrdw
void stfight_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( stfight_text_char_w );
WRITE_HANDLER( stfight_text_attr_w );
WRITE_HANDLER( stfight_vh_latch_w );
WRITE_HANDLER( stfight_sprite_bank_w );
int  stfight_vh_start( void );
void stfight_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh );

// vidhrdw
extern unsigned char *stfight_text_char_ram;
extern unsigned char *stfight_text_attr_ram;
extern unsigned char *stfight_vh_latch_ram;
extern unsigned char *stfight_sprite_ram;

static struct MemoryReadAddress readmem_cpu1[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },          /* sf02.bin */
	{ 0xc000, 0xc1ff, MRA_RAM },            /* palette ram */
	{ 0xc200, 0xc200, input_port_0_r },     /* IN1 */
	{ 0xc201, 0xc201, input_port_1_r },     /* IN2 */
	{ 0xc202, 0xc202, input_port_2_r },     /* IN3 */
	{ 0xc203, 0xc204, stfight_dsw_r },      /* DS0,1 */
	{ 0xc205, 0xc205, stfight_coin_r },     /* coin mech */
	{ 0xd000, 0xd7ff, MRA_RAM },            /* video */
	{ 0xe000, 0xffff, MRA_RAM },

	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_cpu1[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xbfff, MWA_BANK1 },                  /* sf02.bin */
	{ 0xc000, 0xc0ff, paletteram_xxxxBBBBRRRRGGGG_split1_w, &paletteram },
	{ 0xc100, 0xc1ff, paletteram_xxxxBBBBRRRRGGGG_split2_w, &paletteram_2 },
	{ 0xc500, 0xc500, stfight_fm_w },               /* play fm sound */
	{ 0xc600, 0xc600, stfight_adpcm_control_w },    /* voice control */
	{ 0xc700, 0xc700, stfight_coin_w },             /* coin mech */
	{ 0xc804, 0xc806, MWA_NOP },                    /* TBD */
	{ 0xc807, 0xc807, stfight_sprite_bank_w },
	{ 0xd000, 0xd3ff, stfight_text_char_w,      &stfight_text_char_ram },
	{ 0xd400, 0xd7ff, stfight_text_attr_w,      &stfight_text_attr_ram },
	{ 0xd800, 0xd808, stfight_vh_latch_w,       &stfight_vh_latch_ram },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf000, 0xffff, MWA_RAM,                  &stfight_sprite_ram },

    { -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_cpu2[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc001, 0xc001, YM2203_read_port_0_r },
	{ 0xc801, 0xc801, YM2203_read_port_1_r },
	{ 0xf000, 0xf000, stfight_fm_r },
	{ 0xf800, 0xffff, MRA_RAM },

	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_cpu2[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc000, YM2203_control_port_0_w },
	{ 0xc001, 0xc001, YM2203_write_port_0_w },
	{ 0xc800, 0xc800, YM2203_control_port_1_w },
	{ 0xc801, 0xc801, YM2203_write_port_1_w },
	{ 0xe800, 0xe800, stfight_e800_w },
	{ 0xf800, 0xffff, MWA_RAM },

	{ -1 }	/* end of table */
};


INPUT_PORTS_START( stfight )
	PORT_START	/* PLAYER 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* START BUTTONS */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xe7, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */

	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x20, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Bullet Colour" )
	PORT_DIPSETTING(    0x80, "Red" )
	PORT_DIPSETTING(    0x00, "Blue" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x06, "Hardest" )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x18, "4" )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000 30000" )
	PORT_DIPSETTING(    0x20, "20000 40000" )
	PORT_DIPSETTING(    0x40, "30000 60000" )
	PORT_DIPSETTING(    0x60, "40000 80000" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* COIN MECH */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN2, 2 )
INPUT_PORTS_END


/* text-layer characters */
static struct GfxLayout charlayout =
{
	8,8,	    /* 8*8 pixels */
	512,	    /* 512 characters */
	2,	        /* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16	    /* every char takes 16 consecutive bytes */
};

/* foreground tiles */
static struct GfxLayout fglayout =
{
	16,16,	    /* 16*16 pixels */
	1024,	    /* 1024 tiles */
	4,	        /* 4 bits per pixel */
	{ 64*1024*8+0, 64*1024*8+4, 0, 4 },
	{      0,      1,       2,       3,
           8,      9,      10,      11,
      32*8+0, 32*8+1, 32*8+ 2, 32*8+ 3,
      32*8+8, 32*8+9, 32*8+10, 32*8+11 },
	{  0*8,  2*8,  4*8,  6*8,
       8*8, 10*8, 12*8, 14*8,
      16*8, 18*8, 20*8, 22*8,
      24*8, 26*8, 28*8, 30*8 },
	64*8	    /* every char takes 64 consecutive bytes */
};

/*
 *      The background tiles are interleaved in banks of 2
 *      - so we need to create two separate layout structs
 *        to handle them properly with tilemaps
 */

/* background tiles */
static struct GfxLayout bglayout =
{
	16,16,	    /* 16*16 pixels */
	512,	    /* 512 tiles */
	4,	        /* 4 bits per pixel */
	{ 64*1024*8+4, 64*1024*8+0, 4, 0 },
	{      0,      1,       2,       3,
           8,      9,      10,      11,
      64*8+0, 64*8+1, 64*8+ 2, 64*8+ 3,
      64*8+8, 64*8+9, 64*8+10, 64*8+11 },
	{  0*8,  2*8,  4*8,  6*8,
       8*8, 10*8, 12*8, 14*8,
      16*8, 18*8, 20*8, 22*8,
      24*8, 26*8, 28*8, 30*8 },
	128*8	    /* every tile takes 64/128 consecutive bytes */
};

/* sprites */
static struct GfxLayout spritelayout =
{
	16,16,	    /* 16*16 pixels */
	1024,	    /* 1024 sprites */
	4,	        /* 4 bits per pixel */
	{ 64*1024*8+0, 64*1024*8+4, 0, 4 },
	{      0,      1,       2,       3,
           8,      9,      10,      11,
      32*8+0, 32*8+1, 32*8+ 2, 32*8+ 3,
      32*8+8, 32*8+9, 32*8+10, 32*8+11 },
	{  0*8,  2*8,  4*8,  6*8,
       8*8, 10*8, 12*8, 14*8,
      16*8, 18*8, 20*8, 22*8,
      24*8, 26*8, 28*8, 30*8 },
	64*8	    /* every sprite takes 64 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,   0,                16 },
	{ REGION_GFX2, 0x0000, &fglayout,     16*4,             16 },
	{ REGION_GFX3, 0x0000, &bglayout,     16*4+16*16,       16 },
	{ REGION_GFX3, 0x0020, &bglayout,     16*4+16*16,       16 },
	{ REGION_GFX4, 0x0000, &spritelayout, 16*4+16*16+16*16, 16 },
	{ -1 } /* end of array */
};

static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	1500000,	/* 1.5 MHz */
	{ YM2203_VOL(10,15), YM2203_VOL(10,15) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MSM5205interface msm5205_interface =
{
	1,					/* 1 chip            */
	384000,				/* 384KHz             */
	{ stfight_adpcm_int },  /* interrupt function */
	{ MSM5205_S48_4B },	/* 8KHz               */
	{ 50 }
};

static struct MachineDriver machine_driver_stfight =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3000000,	/* 3 Mhz */
			readmem_cpu1, writemem_cpu1, 0, 0,
			stfight_vb_interrupt, 1,
            stfight_interrupt_1, 30
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 3 Mhz */
			readmem_cpu2, writemem_cpu2, 0, 0,
			0, 0,
            stfight_interrupt_2, 120
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	stfight_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256+1, 16*4+16*16+16*16+16*16,
	stfight_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,

	stfight_vh_start,
	0,
	stfight_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( empcity )
	ROM_REGION( 2*0x18000, REGION_CPU1 )	/* 96k for code + 96k for decrypted opcodes */
	ROM_LOAD( "ec_01.rom",  0x00000, 0x8000, 0xfe01d9b1 )
	ROM_LOAD( "ec_02.rom",  0x10000, 0x8000, 0xb3cf1ef7 )	/* bank switched */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "ec_04.rom",  0x0000,  0x8000, 0xaa3e7d1e )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* character data */
	ROM_LOAD( "sf17.bin",   0x0000, 0x2000, 0x1b3706b5 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* foreground tile pixel data */
	ROM_LOAD( "sf07.bin",   0x10000, 0x8000, 0x2c6caa5f )
	ROM_LOAD( "sf08.bin",   0x18000, 0x8000, 0xe11ded31 )
	ROM_LOAD( "sf05.bin",   0x00000, 0x8000, 0x0c099a31 )
	ROM_LOAD( "sf06.bin",   0x08000, 0x8000, 0x3cc77c31 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* background tile pixel data */
	ROM_LOAD( "sf13.bin",   0x10000, 0x8000, 0x0ae48dd3 )
	ROM_LOAD( "sf14.bin",   0x18000, 0x8000, 0xdebf5d76 )
	ROM_LOAD( "sf11.bin",   0x00000, 0x8000, 0x8261ecfe )
	ROM_LOAD( "sf12.bin",   0x08000, 0x8000, 0x71137301 )

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* sprite data */
	ROM_LOAD( "sf20.bin",   0x10000, 0x8000, 0x8299f247 )
	ROM_LOAD( "sf21.bin",   0x18000, 0x8000, 0xb57dc037 )
	ROM_LOAD( "sf18.bin",   0x00000, 0x8000, 0x68acd627 )
	ROM_LOAD( "sf19.bin",   0x08000, 0x8000, 0x5170a057 )

	ROM_REGION( 0x10000, REGION_GFX5 )	/* foreground map data */
	ROM_LOAD( "sf09.bin",   0x00000, 0x8000, 0x8ceaf4fe )
	ROM_LOAD( "sf10.bin",   0x08000, 0x8000, 0x5a1a227a )

	ROM_REGION( 0x10000, REGION_GFX6 )	/* background map data */
	ROM_LOAD( "sf15.bin",   0x00000, 0x8000, 0x27a310bc )
	ROM_LOAD( "sf16.bin",   0x08000, 0x8000, 0x3d19ce18 )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "82s129.006", 0x0000, 0x0100, 0xf9424b5b )	/* text lookup table */
	ROM_LOAD( "82s129.002", 0x0100, 0x0100, 0xc883d49b )	/* fg lookup table */
	ROM_LOAD( "82s129.003", 0x0200, 0x0100, 0xaf81882a )
	ROM_LOAD( "82s129.004", 0x0300, 0x0100, 0x1831ce7c )	/* bg lookup table */
	ROM_LOAD( "82s129.005", 0x0400, 0x0100, 0x96cb6293 )
	ROM_LOAD( "82s129.052", 0x0500, 0x0100, 0x3d915ffc )	/* sprite lookup table */
	ROM_LOAD( "82s129.066", 0x0600, 0x0100, 0x51e8832f )
	ROM_LOAD( "82s129.015", 0x0700, 0x0100, 0x0eaf5158 )	/* timing? (not used) */

	ROM_REGION( 0x08000, REGION_SOUND1 )	/* adpcm voice data */
	ROM_LOAD( "sf04.bin",   0x00000, 0x8000, 0x1b8d0c07 )
ROM_END

ROM_START( empcityj )
	ROM_REGION( 2*0x18000, REGION_CPU1 )	/* 96k for code + 96k for decrypted opcodes */
	ROM_LOAD( "1.bin",      0x00000, 0x8000, 0x8162331c )
	ROM_LOAD( "2.bin",      0x10000, 0x8000, 0x960edea6 )	/* bank switched */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "ec_04.rom",  0x0000,  0x8000, 0xaa3e7d1e )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* character data */
	ROM_LOAD( "sf17.bin",   0x0000, 0x2000, 0x1b3706b5 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* foreground tile pixel data */
	ROM_LOAD( "sf07.bin",   0x10000, 0x8000, 0x2c6caa5f )
	ROM_LOAD( "sf08.bin",   0x18000, 0x8000, 0xe11ded31 )
	ROM_LOAD( "sf05.bin",   0x00000, 0x8000, 0x0c099a31 )
	ROM_LOAD( "sf06.bin",   0x08000, 0x8000, 0x3cc77c31 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* background tile pixel data */
	ROM_LOAD( "sf13.bin",   0x10000, 0x8000, 0x0ae48dd3 )
	ROM_LOAD( "sf14.bin",   0x18000, 0x8000, 0xdebf5d76 )
	ROM_LOAD( "sf11.bin",   0x00000, 0x8000, 0x8261ecfe )
	ROM_LOAD( "sf12.bin",   0x08000, 0x8000, 0x71137301 )

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* sprite data */
	ROM_LOAD( "sf20.bin",   0x10000, 0x8000, 0x8299f247 )
	ROM_LOAD( "sf21.bin",   0x18000, 0x8000, 0xb57dc037 )
	ROM_LOAD( "sf18.bin",   0x00000, 0x8000, 0x68acd627 )
	ROM_LOAD( "sf19.bin",   0x08000, 0x8000, 0x5170a057 )

	ROM_REGION( 0x10000, REGION_GFX5 )	/* foreground map data */
	ROM_LOAD( "sf09.bin",   0x00000, 0x8000, 0x8ceaf4fe )
	ROM_LOAD( "sf10.bin",   0x08000, 0x8000, 0x5a1a227a )

	ROM_REGION( 0x10000, REGION_GFX6 )	/* background map data */
	ROM_LOAD( "sf15.bin",   0x00000, 0x8000, 0x27a310bc )
	ROM_LOAD( "sf16.bin",   0x08000, 0x8000, 0x3d19ce18 )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "82s129.006", 0x0000, 0x0100, 0xf9424b5b )	/* text lookup table */
	ROM_LOAD( "82s129.002", 0x0100, 0x0100, 0xc883d49b )	/* fg lookup table */
	ROM_LOAD( "82s129.003", 0x0200, 0x0100, 0xaf81882a )
	ROM_LOAD( "82s129.004", 0x0300, 0x0100, 0x1831ce7c )	/* bg lookup table */
	ROM_LOAD( "82s129.005", 0x0400, 0x0100, 0x96cb6293 )
	ROM_LOAD( "82s129.052", 0x0500, 0x0100, 0x3d915ffc )	/* sprite lookup table */
	ROM_LOAD( "82s129.066", 0x0600, 0x0100, 0x51e8832f )
	ROM_LOAD( "82s129.015", 0x0700, 0x0100, 0x0eaf5158 )	/* timing? (not used) */

	ROM_REGION( 0x08000, REGION_SOUND1 )	/* adpcm voice data */
	ROM_LOAD( "sf04.bin",   0x00000, 0x8000, 0x1b8d0c07 )
ROM_END

ROM_START( stfight )
	ROM_REGION( 2*0x18000, REGION_CPU1 )	/* 96k for code + 96k for decrypted opcodes */
	ROM_LOAD( "a-1.4q",     0x00000, 0x8000, 0xff83f316 )
	ROM_LOAD( "sf02.bin",   0x10000, 0x8000, 0xe626ce9e )	/* bank switched */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "sf03.bin",   0x0000,  0x8000, 0x6a8cb7a6 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* character data */
	ROM_LOAD( "sf17.bin",   0x0000, 0x2000, 0x1b3706b5 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* foreground tile pixel data */
	ROM_LOAD( "sf07.bin",   0x10000, 0x8000, 0x2c6caa5f )
	ROM_LOAD( "sf08.bin",   0x18000, 0x8000, 0xe11ded31 )
	ROM_LOAD( "sf05.bin",   0x00000, 0x8000, 0x0c099a31 )
	ROM_LOAD( "sf06.bin",   0x08000, 0x8000, 0x3cc77c31 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* background tile pixel data */
	ROM_LOAD( "sf13.bin",   0x10000, 0x8000, 0x0ae48dd3 )
	ROM_LOAD( "sf14.bin",   0x18000, 0x8000, 0xdebf5d76 )
	ROM_LOAD( "sf11.bin",   0x00000, 0x8000, 0x8261ecfe )
	ROM_LOAD( "sf12.bin",   0x08000, 0x8000, 0x71137301 )

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* sprite data */
	ROM_LOAD( "sf20.bin",   0x10000, 0x8000, 0x8299f247 )
	ROM_LOAD( "sf21.bin",   0x18000, 0x8000, 0xb57dc037 )
	ROM_LOAD( "sf18.bin",   0x00000, 0x8000, 0x68acd627 )
	ROM_LOAD( "sf19.bin",   0x08000, 0x8000, 0x5170a057 )

	ROM_REGION( 0x10000, REGION_GFX5 )	/* foreground map data */
	ROM_LOAD( "sf09.bin",   0x00000, 0x8000, 0x8ceaf4fe )
	ROM_LOAD( "sf10.bin",   0x08000, 0x8000, 0x5a1a227a )

	ROM_REGION( 0x10000, REGION_GFX6 )	/* background map data */
	ROM_LOAD( "sf15.bin",   0x00000, 0x8000, 0x27a310bc )
	ROM_LOAD( "sf16.bin",   0x08000, 0x8000, 0x3d19ce18 )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "82s129.006", 0x0000, 0x0100, 0xf9424b5b )	/* text lookup table */
	ROM_LOAD( "82s129.002", 0x0100, 0x0100, 0xc883d49b )	/* fg lookup table */
	ROM_LOAD( "82s129.003", 0x0200, 0x0100, 0xaf81882a )
	ROM_LOAD( "82s129.004", 0x0300, 0x0100, 0x1831ce7c )	/* bg lookup table */
	ROM_LOAD( "82s129.005", 0x0400, 0x0100, 0x96cb6293 )
	ROM_LOAD( "82s129.052", 0x0500, 0x0100, 0x3d915ffc )	/* sprite lookup table */
	ROM_LOAD( "82s129.066", 0x0600, 0x0100, 0x51e8832f )
	ROM_LOAD( "82s129.015", 0x0700, 0x0100, 0x0eaf5158 )	/* timing? (not used) */

	ROM_REGION( 0x08000, REGION_SOUND1 )	/* adpcm voice data */
	ROM_LOAD( "sf04.bin",   0x00000, 0x8000, 0x1b8d0c07 )
ROM_END



GAME( 1986, empcity,  0,       stfight, stfight, empcity, ROT0, "Seibu Kaihatsu", "Empire City: 1931 (bootleg?)" )
GAME( 1986, empcityj, empcity, stfight, stfight, stfight, ROT0, "[Seibu Kaihatsu] (Taito license)", "Empire City: 1931 (Japan)" )
GAME( 1986, stfight,  empcity, stfight, stfight, stfight, ROT0, "Seibu Kaihatsu", "Street Fight (Germany)" )
