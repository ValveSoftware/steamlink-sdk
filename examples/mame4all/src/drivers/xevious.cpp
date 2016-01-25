#include "../vidhrdw/xevious.cpp"
#include "../machine/xevious.cpp"

/***************************************************************************

Xevious

driver by Mirko Buffoni


general map
-----------+---+-----------------+-------------------------
   hex     |r/w| D D D D D D D D |
 location  |   | 7 6 5 4 3 2 1 0 | function
-----------+---+-----------------+-------------------------
0000-3FFF  | R | D D D D D D D D | CPU 1 master rom (16k)
0000-1FFF  | R | D D D D D D D D | CPU 2 motion rom (8k)
0000-0FFF  | R | D D D D D D D D | CPU 3 sound  rom (4k)
-----------+---+-----------------+-------------------------
6800-680F  | W | - - - - D D D D | Audio control
6810-681F  | W | - - - - D D D D | Audio control
-----------+---+-----------------+-------------------------
6820       | W | - - - - - - - D | 0 = Reset IRQ1(latched)
6821       | W | - - - - - - - D | 0 = Reset IRQ2(latched)
6822       | W | - - - - - - - D | 0 = Reset NMI3(latched)
6823       | W | - - - - - - - D | 0 = Reset #2,#3 CPU
-----------+---+-----------------+-------------------------
6830       | W |                 | watchdog reset
-----------+---+-----------------+-------------------------
7000       |R/W| D D D D D D D D | custom 06 Data
7100       |R/W| D D D D D D D D | custom 06 Command
7???                               CPU #1 NMI REQUEST
-----------+---+-----------------+-------------------------
7800-7FFF  |R/W| D D D D D D D D | 2k work ram
-----------+---+-----------------+-------------------------
8000-87FF  |R/W| D D D D D D D D | 2k playfeild RAM
-----------+---+-----------------+-------------------------
8000-877F  |R/W| D D D D D D D D | RAM
8780-87FF  |R/W| D D D D D D D D | 2k sprite RAM (X-POS)
9000-977F  |R/W| D D D D D D D D | RAM
9780-97FF  |R/W| D D D D D D D D | 2k sprite RAM (HSIZE,MODE ??)
A000-A77F  |R/W| D D D D D D D D | RAM
A780-A7FF  |R/W| D D D D D D D D | 2k sprite RAM (PIC)
-----------+---+-----------------+-------------------------
B000-BFFF  |R/W| D D D D D D D D | 4k playfeild RAM ( ATTRIB)
C000-CFFF  |R/W| D D D D D D D D | 4k playfeild RAM ( PIC )
-----------+---+-----------------+-------------------------
D000-D00F  | W | D D D D D D D D | A0->D8:background Y scroll position
D010-D01F  | W | D D D D D D D D | A0->D8:font Y scroll position ??
D020-D02F  | W | D D D D D D D D | A0->D8:background X scroll position ??
D030-D03F  | W | D D D D D D D D | A0->D8:font X scroll position ?
D070-D07F  | W |               D | display flip mode ?
-----------+---+-----------------+-------------------------
F000       | W | D D D D D D D D | planet map position low ?
F001       | W | D D D D D D D D | planet map position high ?
F000       | R | D D D D D D D D | planet map data low ?
F001       | R | D D D D D D D D | planet map data high ?
-----------+---+-----------------+-------------------------
Xevious memory map (preliminary)

  Z80-1:MASTER CPU
    0000H-3FFFH R   P-ROM
    4000H-FFFFH R/W shared area
  Z80-2:MOTION CUP
    0000H-1FFFH R   P-ROM
    4000H-FFFFH R/W shared area
  Z80-3:SOUND CPU
    0000H-1FFFH R   P-ROM
    4000H-FFFFH R/W shared area
  shared area.
    6800H-6807H R   DIP SWITCH READ
                      A0-2 : bit select(6800H=bit0 , 6801=bit1,6807=bit7)
                      D0   : DIP SW.A bit read
                      D1   : DIP SW.B bit read
                    DIP SW.A
                      Bit0 = BLASTER SWITCH
    6800H-680FH R   shadow(6800H-6807H)
    6800H-681FH W   Sound RAM0 (same to DIGDUG ?)
                      D0-3:data
    6810H-681FH W   Sound RAM1
                      D0-3:data
    6820H       W   MASTER-INTERRUPT CLEAR & ENABLE
                      D0 = 0:CLEAR & DISABLE / 1:ENABLE
    6821H       W   MOTION-INTERRUPT CLEAR & ENABLE
                      D0 = 0:CLEAR & DISABLE / 1:ENABLE
    6822H       W   SOUND -NMI CONTROLL
                      D0 = 0:NMI ON / 1:NMI OFF
    6823H       W   CPU 2,3 RESET CONTROLL
                      D0 = 0:RESET
    6830H-683F  W   WDR Watch dock timer clear
    6840H-6FFFH R/W shadow(6800H-603FH)
    7000H       R   custom-io data read ( after CPU #1 NMI )
    7000H       W   custom-io data write( after CPU #1 NMI )
    7100H       R   custom-io timming port
                     bit7  :1=busy ?
                     bit6  :1=busy ?
                     bit5  :1=busy ?
    7100H       W   custom-io command write
    7???H       W   sound ganarator (controll by custom_ic ?)
    7800H-7FFFH R/W S-RAM
    8000H-87FFh R/W ram ( master cpu )
    8780H-87FFH R/W sprite X position ( 80-bf:used by master,c0-ff:used by motion)
                      A0=0:sprite Y position (OFF = 0xef )
                      A0=1:sprite X position
    8800H-8FFFH R/W shadow(8000H-87FFH)
    9000H-97FFH R/W ram ( motion cpu )
    9780H-97FFH R/W sprite attribute
                      A0=0:attribute Y
                        bit0  :HSIZE  :sprite Y size ?
                        bit1  :HUNDZ  :?
                        bit2  :FLOP   :Y flip
                        bit3  :HUKAIRS:?
                        bit4-6:MD4-6  :?
                        bit7  :BIT3   :select sprite set (0=4M,4P,4R 3bit:1=4N 2bit )
                      A0=0:attribute X
                        bit0  :HSIZE  :sprite X size ?
                        bit1  :HUNDZ  :?
                        bit2  :FLOP   :X flip
                        bit3  :HUKAIRS:?
                        bit4-6:MD4-6  :?
                        bit7  :BIT3   :?
    9800H-9FFFH R/W shadow(9000H-97FFH)
    A000H-A7FFH R/W ram ( sound cpu , master cpu )
    A780H-a7FFH R/W sprite character nnumber
                      A0=0 character pattern name
                      A0=1 color map select
    A800H-AFFFH R/W shadow(A000H-A7FFH)
    B000H-BFFFH R/W background attrivute ( have 2 scroll planes ? )
                    D0-D1:COL0,1 palette set ?
                    D1-D5:ANI0-3 color code  ?( font,bg use )
                    D6   :ANF    X flip
                    D7   :PFF    Y flip
    C000H-CFFFH R/W background character ( have 2 scroll planes ? )
    D000H-D07FH  W  CRTC access? (custom-13)
    D???H        W  display flip select (TABLE 1P/2P mode)
                      bit0  :FLIP
    F000H        W  BS0 xevious planet map select low ?
    F001H        W  BB1 xeviosu planet map select high ?
    F000H        R  BB0 xevious planet map get low ?
    F001H        R  BS1 xevious planet map get high ?
    F002-FFFFH      shodow(F000H-F001H)

3)schematic diagram block.

   Sheet 4A : main cpu , address decoder(4000h-7fffh) , sram
   Sheet 4B : motion cpu
   Sheet 5A : sound cpu
   Sheet 5B : clock generator , 'digdug' sound , irq controller
   Sheet 6A : joystick read , dip switch , audio(TONE?) , NMI to master cpu
   Sheet 6B : address decoder (8000h-ffffh) , sprite ram r/w , disp. flip latch
   Sheet 7A : sprite drawing engine
   Sheet 7B : background ram , CRTC ?,graphic drawing engene ?
   Sheet 8A : background(&font) drawing engene
   Sheet 8B : display line buffer ?
   Sheet 9A : video dac ( palette rom )
   Sheet 9B : xevious planet map rom ?

P-ROMS in schematic

1M  master cpu rom 0000H-1FFFH
1L  master cpu rom 2000H-3FFFH
4C  motion cpu rom 0000H-1FFFH
2C  sound  cpu rom 0000H-1FFFH
8M  sound pcm rom ?
6M  sound pcm decode rom ?
4M  spright pattern low (BIT3=0,000-127)
4P  spright pattern low (BIT3=0,128-255)
4R  spright pattern high(BIT3=0)
      D0-3:000-127
      D4-7:128-255
4N  spright pattern (BIT3=1)
3L  spright color map table
3M  spright color map table
3D  background pattern bit0
3C  background pattern bit1
3B  background font pattern
4H  background color map table
4F  background color map table
6E  palette rom blue
6D  palette rom green
6A  palette rom red
2A  xevious planet map table ??
2B  xevious planet map table ??
2C  xevious planet map table ??

S-RAMS in schematic

1H  sram           7800H-7FFFH
7L  sound sram0    6800H-680FH
7K  sound sram1    6810H-681FH
2S  spright line   8000H-87FFH
2A  spright att    9000H-97FFH
2P  spright chr    A000H-A7FFH
2J  background ram B000H-B7FFH
2H  background ram B800H-BFFFH
2F  background ram C000H-C7FFH
2E  background ram C800H-CFFFH
5N,5M sprite display line buffer ? (even disp,odd draw)
6N,6M sprite display line buffer ? (even draw,odd disp)

*****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *xevious_sharedram;
READ_HANDLER( xevious_sharedram_r );
WRITE_HANDLER( xevious_sharedram_w );
READ_HANDLER( xevious_dsw_r );
WRITE_HANDLER( xevious_interrupt_enable_1_w );
WRITE_HANDLER( xevious_interrupt_enable_2_w );
WRITE_HANDLER( xevious_interrupt_enable_3_w );
WRITE_HANDLER( xevious_bs_w );
READ_HANDLER( xevious_bb_r );
READ_HANDLER( xevious_customio_r );
READ_HANDLER( xevious_customio_data_r );
WRITE_HANDLER( xevious_customio_w );
WRITE_HANDLER( xevious_customio_data_w );
WRITE_HANDLER( xevious_halt_w );
int  xevious_interrupt_1(void);
int  xevious_interrupt_2(void);
int  xevious_interrupt_3(void);
void xevious_init_machine(void);

WRITE_HANDLER( xevious_vh_latch_w );

extern unsigned char *xevious_fg_videoram,*xevious_fg_colorram;
extern unsigned char *xevious_bg_videoram,*xevious_bg_colorram;
WRITE_HANDLER( xevious_fg_videoram_w );
WRITE_HANDLER( xevious_fg_colorram_w );
WRITE_HANDLER( xevious_bg_videoram_w );
WRITE_HANDLER( xevious_bg_colorram_w );
int  xevious_vh_start( void );
void xevious_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void xevious_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( pengo_sound_w );
extern unsigned char *pengo_soundregs;



static struct MemoryReadAddress readmem_cpu1[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x6800, 0x6807, xevious_dsw_r },
	{ 0x7000, 0x700f, xevious_customio_data_r },
	{ 0x7100, 0x7100, xevious_customio_r },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
	{ 0xf000, 0xffff, xevious_bb_r },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_cpu2[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x6800, 0x6807, xevious_dsw_r },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
	{ 0xf000, 0xffff, xevious_bb_r },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_cpu3[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_cpu1[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x6820, 0x6820, xevious_interrupt_enable_1_w },
	{ 0x6821, 0x6821, xevious_interrupt_enable_2_w },
	{ 0x6822, 0x6822, xevious_interrupt_enable_3_w },
	{ 0x6823, 0x6823, xevious_halt_w },			/* reset controll */
	{ 0x6830, 0x683f, MWA_NOP },				/* watch dock reset */
	{ 0x7000, 0x700f, xevious_customio_data_w },
	{ 0x7100, 0x7100, xevious_customio_w },
	{ 0x7800, 0xafff, xevious_sharedram_w, &xevious_sharedram },
	{ 0xb000, 0xb7ff, xevious_fg_colorram_w, &xevious_fg_colorram },
	{ 0xb800, 0xbfff, xevious_bg_colorram_w, &xevious_bg_colorram },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_w, &xevious_fg_videoram },
	{ 0xc800, 0xcfff, xevious_bg_videoram_w, &xevious_bg_videoram },
	{ 0xd000, 0xd07f, xevious_vh_latch_w }, /* ?? */
	{ 0xf000, 0xffff, xevious_bs_w },
	{ 0x8780, 0x87ff, MWA_RAM, &spriteram_2 },	/* here only */
	{ 0x9780, 0x97ff, MWA_RAM, &spriteram_3 },	/* to initialize */
	{ 0xa780, 0xa7ff, MWA_RAM, &spriteram, &spriteram_size },	/* the pointers */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_cpu2[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x6830, 0x683f, MWA_NOP },				/* watch dog reset */
	{ 0x7800, 0xafff, xevious_sharedram_w },
	{ 0xb000, 0xb7ff, xevious_fg_colorram_w },
	{ 0xb800, 0xbfff, xevious_bg_colorram_w },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_w },
	{ 0xc800, 0xcfff, xevious_bg_videoram_w },
	{ 0xd000, 0xd07f, xevious_vh_latch_w }, /* ?? */
	{ 0xf000, 0xffff, xevious_bs_w },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_cpu3[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x6800, 0x681f, pengo_sound_w, &pengo_soundregs },
	{ 0x6822, 0x6822, xevious_interrupt_enable_3_w },
	{ 0x7800, 0xcfff, xevious_sharedram_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( xevious )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Right Coin" )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, "Freeze?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Left Coin" )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* FAKE */
	/* The player inputs are not memory mapped, they are handled by an I/O chip. */
	/* These fake input ports are read by galaga_customio_data_r() */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/* same as xevious, the only difference is DSW0 bit 7 */
INPUT_PORTS_START( xeviousa )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Right Coin" )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	/* when switch is on Namco, high score names are 10 letters long */
	PORT_DIPNAME( 0x80, 0x80, "Copyright" )
	PORT_DIPSETTING(    0x00, "Namco" )
	PORT_DIPSETTING(    0x80, "Atari/Namco" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Left Coin" )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* FAKE */
	/* The player inputs are not memory mapped, they are handled by an I/O chip. */
	/* These fake input ports are read by galaga_customio_data_r() */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/* same as xevious, the only difference is DSW0 bit 7. Note that the bit is */
/* inverted wrt xevious. */
INPUT_PORTS_START( sxevious )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Right Coin" )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, "Freeze?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Left Coin" )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* FAKE */
	/* The player inputs are not memory mapped, they are handled by an I/O chip. */
	/* These fake input ports are read by galaga_customio_data_r() */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



/* foreground characters */
static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	1,	/* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
/* background tiles */
static struct GfxLayout bgcharlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 512*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
/* sprite set #1 */
static struct GfxLayout spritelayout1 =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 128*64*8+4, 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 128 consecutive bytes */
};
/* sprite set #2 */
static struct GfxLayout spritelayout2 =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 0, 128*64*8, 128*64*8+4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 128 consecutive bytes */
};
/* sprite set #3 */
static struct GfxLayout spritelayout3 =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	3,	/* 3 bits per pixel (one is always 0) */
	{ 64*64*8, 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout, 128*4+64*8,  64 },
	{ REGION_GFX2, 0x0000, &bgcharlayout,        0, 128 },
	{ REGION_GFX3, 0x0000, &spritelayout1,   128*4,  64 },
	{ REGION_GFX3, 0x2000, &spritelayout2,   128*4,  64 },
	{ REGION_GFX3, 0x6000, &spritelayout3,   128*4,  64 },
	{ -1 } /* end of array */
};



static struct namco_interface namco_interface =
{
	3072000/32,	/* sample rate */
	3,			/* number of voices */
	100,		/* playback volume */
	REGION_SOUND1	/* memory region */
};

static const char *xevious_sample_names[] =
{
	"*xevious",
	"explo1.wav",	/* ground target explosion */
	"explo2.wav",	/* Solvalou explosion */
	0	/* end of array */
};

struct Samplesinterface samples_interface =
{
	1,	/* one channel */
	80,	/* volume */
	xevious_sample_names
};



static struct MachineDriver machine_driver_xevious =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3125000,	/* 3.125 Mhz (?) */
			readmem_cpu1,writemem_cpu1,0,0,
			xevious_interrupt_1,1
		},
		{
			CPU_Z80,
			3125000,	/* 3.125 Mhz */
			readmem_cpu2,writemem_cpu2,0,0,
			xevious_interrupt_2,1
		},
		{
			CPU_Z80,
			3125000,	/* 3.125 Mhz */
			readmem_cpu3,writemem_cpu3,0,0,
			xevious_interrupt_3,2
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	100,	/* 100 CPU slices per frame - an high value to ensure proper */
			/* synchronization of the CPUs */
	xevious_init_machine,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	128+1,128*4+64*8+64*2,
	xevious_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	xevious_vh_start,
	0,
	xevious_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( xevious )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for the first CPU */
	ROM_LOAD( "xvi_1.3p",     0x0000, 0x1000, 0x09964dda )
	ROM_LOAD( "xvi_2.3m",     0x1000, 0x1000, 0x60ecce84 )
	ROM_LOAD( "xvi_3.2m",     0x2000, 0x1000, 0x79754b7d )
	ROM_LOAD( "xvi_4.2l",     0x3000, 0x1000, 0xc7d4bbf0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "xvi_5.3f",     0x0000, 0x1000, 0xc85b703f )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, 0xe18cdaad )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, 0xdd35cf1c )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, 0x088c8b26 )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, 0xde60ba25 )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, 0x535cdbbc )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, 0xdc2c0ecb )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, 0x02417d19 )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, 0xdfb587ce )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, 0x605ca889 )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, 0x57ed9879 )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, 0xae3ba9e5 )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, 0x31e244dd )

	ROM_REGION( 0x0b00, REGION_PROMS )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, 0x5cc2727f ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, 0x5c8796cc ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, 0x3cb60975 ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, 0x22d98032 ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, 0x3a7599f0 ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, 0xfd8b9d91 ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, 0xbf906d82 ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, 0x550f06bc )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( xeviousa )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for the first CPU */
	ROM_LOAD( "xea-1m-a.bin", 0x0000, 0x2000, 0x8c2b50ec )
	ROM_LOAD( "xea-1l-a.bin", 0x2000, 0x2000, 0x0821642b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "xea-4c-a.bin", 0x0000, 0x2000, 0x14d8fa03 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, 0xdd35cf1c )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, 0x088c8b26 )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, 0xde60ba25 )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, 0x535cdbbc )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, 0xdc2c0ecb )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, 0x02417d19 )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, 0xdfb587ce )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, 0x605ca889 )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, 0x57ed9879 )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, 0xae3ba9e5 )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, 0x31e244dd )

	ROM_REGION( 0x0b00, REGION_PROMS )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, 0x5cc2727f ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, 0x5c8796cc ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, 0x3cb60975 ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, 0x22d98032 ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, 0x3a7599f0 ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, 0xfd8b9d91 ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, 0xbf906d82 ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, 0x550f06bc )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( xevios )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for the first CPU */
	ROM_LOAD( "4.7h",         0x0000, 0x1000, 0x1f8ca4c0 )
	ROM_LOAD( "5.6h",         0x1000, 0x1000, 0x2e47ce8f )
	ROM_LOAD( "xvi_3.2m",     0x2000, 0x1000, 0x79754b7d )
	ROM_LOAD( "7.4h",         0x3000, 0x1000, 0x7033f2e3 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "xvi_5.3f",     0x0000, 0x1000, 0xc85b703f )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, 0xe18cdaad )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, 0xdd35cf1c )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, 0x088c8b26 )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, 0xde60ba25 )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, 0x535cdbbc )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, 0xdc2c0ecb )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "16.8d",        0x2000, 0x2000, 0x44262c04 )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, 0xdfb587ce )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, 0x605ca889 )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "10.1d",        0x0000, 0x1000, 0x10baeebb )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, 0xae3ba9e5 )
	ROM_LOAD( "12.3d",        0x3000, 0x1000, 0x51a4e83b )

	ROM_REGION( 0x0b00, REGION_PROMS )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, 0x5cc2727f ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, 0x5c8796cc ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, 0x3cb60975 ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, 0x22d98032 ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, 0x3a7599f0 ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, 0xfd8b9d91 ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, 0xbf906d82 ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, 0x550f06bc )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, 0x77245b66 )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_USER1 )
	/* extra ROMs (function unknown, could be emulation of the custom I/O */
	/* chip with a Z80): */
	ROM_LOAD( "1.16j",        0x0000, 0x1000, 0x2618f0ce )
	ROM_LOAD( "2.17b",        0x1000, 0x2000, 0xde359fac )
ROM_END

ROM_START( sxevious )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for the first CPU */
	ROM_LOAD( "cpu_3p.rom",   0x0000, 0x1000, 0x1c8d27d5 )
	ROM_LOAD( "cpu_3m.rom",   0x1000, 0x1000, 0xfd04e615 )
	ROM_LOAD( "cpu_2m.rom",   0x2000, 0x1000, 0x294d5404 )
	ROM_LOAD( "cpu_2l.rom",   0x3000, 0x1000, 0x6a44bf92 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "cpu_3f.rom",   0x0000, 0x1000, 0xd4bd3d81 )
	ROM_LOAD( "cpu_3j.rom",   0x1000, 0x1000, 0xaf06be5f )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, 0xdd35cf1c )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, 0x088c8b26 )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, 0xde60ba25 )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, 0x535cdbbc )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, 0xdc2c0ecb )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, 0x02417d19 )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, 0xdfb587ce )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, 0x605ca889 )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, 0x57ed9879 )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, 0xae3ba9e5 )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, 0x31e244dd )

	ROM_REGION( 0x0b00, REGION_PROMS )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, 0x5cc2727f ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, 0x5c8796cc ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, 0x3cb60975 ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, 0x22d98032 ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, 0x3a7599f0 ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, 0xfd8b9d91 ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, 0xbf906d82 ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, 0x550f06bc )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END



static void init_xevios(void)
{
	int A,i;


	/* convert one of the sprite ROMs to the format used by Xevious */
	for (A = 0x2000;A < 0x4000;A++)
	{
		int bit[8];
		unsigned char *RAM = memory_region(REGION_GFX3);

		/* 76543210 -> 13570246 bit rotation */
		for (i = 0;i < 8;i++)
			bit[i] = (RAM[A] >> i) & 1;

		RAM[A] =
			(bit[6] << 0) +
			(bit[4] << 1) +
			(bit[2] << 2) +
			(bit[0] << 3) +
			(bit[7] << 4) +
			(bit[5] << 5) +
			(bit[3] << 6) +
			(bit[1] << 7);
	}

	/* convert one of tile map ROMs to the format used by Xevious */
	for (A = 0x0000;A < 0x1000;A++)
	{
		int bit[8];
		unsigned char *RAM = memory_region(REGION_GFX4);

		/* 76543210 -> 37512640 bit rotation */
		for (i = 0;i < 8;i++)
			bit[i] = (RAM[A] >> i) & 1;

		RAM[A] =
			(bit[0] << 0) +
			(bit[4] << 1) +
			(bit[6] << 2) +
			(bit[2] << 3) +
			(bit[1] << 4) +
			(bit[5] << 5) +
			(bit[7] << 6) +
			(bit[3] << 7);
	}
}



GAME( 1982, xevious,  0,       xevious, xevious,  0,      ROT90, "Namco", "Xevious (Namco)" )
GAME( 1982, xeviousa, xevious, xevious, xeviousa, 0,      ROT90, "Namco (Atari license)", "Xevious (Atari)" )
GAME( 1983, xevios,   xevious, xevious, xevious,  xevios, ROT90, "bootleg", "Xevios" )
GAME( 1984, sxevious, xevious, xevious, sxevious, 0,      ROT90, "Namco", "Super Xevious" )
