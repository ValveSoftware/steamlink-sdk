#include "../sndhrdw/targ.cpp"
#include "../vidhrdw/exidy.cpp"
#include "../sndhrdw/exidy.cpp"

/***************************************************************************

Exidy memory map

0000-00FF R/W Zero Page RAM
0100-01FF R/W Stack RAM
0200-03FF R/W Scratchpad RAM
0800-3FFF  R  Program ROM              (Targ, Spectar only)
1A00       R  PX3 (Player 2 inputs)    (Fax only)
			  bit 4  D
			  bit 5  C
			  bit 6  B
			  bit 7  A
1C00       R  PX2 (Player 1 inputs)    (Fax only)
			  bit 0  2 player start
			  bit 1  1 player start
			  bit 4  D
			  bit 5  C
			  bit 6  B
			  bit 7  A
2000-3FFF  R  Banked question ROM      (Fax only)
4000-43FF R/W Screen RAM
4800-4FFF R/W Character Generator RAM (except Pepper II and Fax)
5000       W  Motion Object 1 Horizontal Position Latch (sprite 1 X)
5040       W  Motion Object 1 Vertical Position Latch   (sprite 1 Y)
5080       W  Motion Object 2 Horizontal Position Latch (sprite 2 X)
50C0       W  Motion Object 2 Vertical Position Latch   (sprite 2 Y)
5100       R  Option Dipswitch Port
			  bit 0  coin 2 (NOT inverted) (must activate together with $5103 bit 5)
			  bit 1-2  bonus
			  bit 3-4  coins per play
			  bit 5-6  lives
			  bit 7  US/UK coins
5100       W  Motion Objects Image Latch
			  Sprite number  bits 0-3 Sprite #1  4-7 Sprite #2
5101       R  Control Inputs Port
			  bit 0  start 1
			  bit 1  start 2
			  bit 2  right
			  bit 3  left
			  bit 5  up
			  bit 6  down
			  bit 7  coin 1 (must activate together with $5103 bit 6)
5101       W  Output Control Latch (not used in PEPPER II upright)
			  bit 7  Enable sprite #1
			  bit 6  Enable sprite #2
5103       R  Interrupt Condition Latch
			  bit 0  LNG0 - supposedly a language DIP switch
			  bit 1  LNG1 - supposedly a language DIP switch
			  bit 2  different for each game, but generally a collision bit
			  bit 3  TABLE - supposedly a cocktail table DIP switch
			  bit 4  different for each game, but generally a collision bit
			  bit 5  coin 2 (must activate together with $5100 bit 0)
			  bit 6  coin 1 (must activate together with $5101 bit 7)
			  bit 7  L256 - VBlank?
5213       R  IN2 (Mouse Trap)
			  bit 3  blue button
			  bit 2  free play
			  bit 1  red button
			  bit 0  yellow button
52XX      R/W Audio/Color Board Communications
6000-6FFF R/W Character Generator RAM (Pepper II, Fax only)
8000-FFF9  R  Program memory space
FFFA-FFFF  R  Interrupt and Reset Vectors

Exidy Sound Board:
0000-07FF R/W RAM (mirrored every 0x7f)
0800-0FFF R/W 6532 Timer
1000-17FF R/W 6520 PIA
1800-1FFF R/W 8253 Timer
2000-27FF bit 0 Channel 1 Filter 1 enable
		  bit 1 Channel 1 Filter 2 enable
		  bit 2 Channel 2 Filter 1 enable
		  bit 3 Channel 2 Filter 2 enable
		  bit 4 Channel 3 Filter 1 enable
		  bit 5 Channel 3 Filter 2 enable
2800-2FFF 6840 Timer
3000      Bit 0..1 Noise select
3001	  Bit 0..2 Channel 1 Amplitude
3002	  Bit 0..2 Channel 2 Amplitude
3003	  Bit 0..2 Channel 3 Amplitude
5800-7FFF ROM

Targ:
5200    Sound board control
		bit 0 Music
		bit 1 Shoot
		bit 2 unused
		bit 3 Swarn
		bit 4 Sspec
		bit 5 crash
		bit 6 long
		bit 7 game

5201    Sound board control
		bit 0 note
		bit 1 upper

MouseTrap Digital Sound:
0000-3FFF ROM

IO:
	A7 = 0: R Communication from sound processor
	A6 = 0: R CVSD Clock State
	A5 = 0: W Busy to sound processor
	A4 = 0: W Data to CVSD

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/6821pia.h"


/* These are defined in sndhrdw/exidy.c */
int exidy_sh_start(const struct MachineSound *msound);

WRITE_HANDLER( exidy_shriot_w );
WRITE_HANDLER( exidy_sfxctrl_w );
WRITE_HANDLER( exidy_sh8253_w );
WRITE_HANDLER( exidy_sh6840_w );
READ_HANDLER( exidy_shriot_r );
READ_HANDLER( exidy_sh8253_r );
READ_HANDLER( exidy_sh6840_r );

WRITE_HANDLER( mtrap_voiceio_w );
READ_HANDLER( mtrap_voiceio_r );


/* These are defined in sndhrdw/targ.c */
extern UINT8 targ_spec_flag;

int targ_sh_start(const struct MachineSound *msound);
void targ_sh_stop(void);

WRITE_HANDLER( targ_sh_w );


/* These are defined in vidhrdw/targ.c */
#define PALETTE_LEN 8
#define COLORTABLE_LEN 20

extern UINT8 *exidy_characterram;
extern UINT8 *exidy_sprite_no;
extern UINT8 *exidy_sprite_enable;
extern UINT8 *exidy_sprite1_xpos;
extern UINT8 *exidy_sprite1_ypos;
extern UINT8 *exidy_sprite2_xpos;
extern UINT8 *exidy_sprite2_ypos;
extern UINT8 *exidy_color_latch;
extern UINT8 *exidy_palette;
extern UINT16 *exidy_colortable;

extern UINT8 sidetrac_palette[];
extern UINT8 targ_palette[];
extern UINT8 spectar_palette[];
extern UINT16 exidy_1bpp_colortable[];
extern UINT16 exidy_2bpp_colortable[];

extern UINT8 exidy_collision_mask;
extern UINT8 exidy_collision_invert;

int exidy_vh_start(void);
void exidy_vh_stop(void);
void exidy_vh_eof(void);
void exidy_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void exidy_vh_init_palette(UINT8 *game_palette, UINT16 *game_colortable,const UINT8 *color_prom);
int exidy_vblank_interrupt(void);

WRITE_HANDLER( exidy_characterram_w );
WRITE_HANDLER( exidy_color_w );

READ_HANDLER( exidy_interrupt_r );



/*************************************
 *
 *	Bankswitcher
 *
 *************************************/

static WRITE_HANDLER( fax_bank_select_w )
{
	UINT8 *RAM = memory_region(REGION_CPU1);

	cpu_setbank(1, &RAM[0x10000 + (0x2000 * (data & 0x1F))]);
	/*if ((data & 0x1F) > 0x17)
		logerror("Banking to unpopulated ROM bank %02X!\n",data & 0x1F);*/
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0800, 0x3fff, MRA_ROM },			/* Targ, Spectar only */
	{ 0x4000, 0x43ff, videoram_r },
	{ 0x4400, 0x47ff, videoram_r },			/* mirror (sidetrac requires this) */
	{ 0x4800, 0x4fff, MRA_RAM },
	{ 0x5100, 0x5100, input_port_0_r },		/* DSW */
	{ 0x5101, 0x5101, input_port_1_r },		/* IN0 */
	{ 0x5103, 0x5103, exidy_interrupt_r },	/* IN1 */
	{ 0x5105, 0x5105, input_port_4_r },		/* IN3 - Targ, Spectar only */
	{ 0x5200, 0x520F, pia_0_r },
	{ 0x5213, 0x5213, input_port_3_r },		/* IN2 */
	{ 0x6000, 0x6fff, MRA_RAM },			/* Pepper II only */
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0800, 0x3fff, MWA_ROM },
	{ 0x4000, 0x43ff, videoram_w, &videoram, &videoram_size },
	{ 0x4400, 0x47ff, videoram_w },
	{ 0x4800, 0x4fff, exidy_characterram_w, &exidy_characterram },
	{ 0x5000, 0x5000, MWA_RAM, &exidy_sprite1_xpos },
	{ 0x5040, 0x5040, MWA_RAM, &exidy_sprite1_ypos },
	{ 0x5080, 0x5080, MWA_RAM, &exidy_sprite2_xpos },
	{ 0x50C0, 0x50C0, MWA_RAM, &exidy_sprite2_ypos },
	{ 0x5100, 0x5100, MWA_RAM, &exidy_sprite_no },
	{ 0x5101, 0x5101, MWA_RAM, &exidy_sprite_enable },
	{ 0x5200, 0x520F, pia_0_w },
	{ 0x5210, 0x5212, exidy_color_w, &exidy_color_latch },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress fax_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0400, 0x07ff, MRA_RAM },			/* Fax only */
	{ 0x1a00, 0x1a00, input_port_4_r },		/* IN3 - Fax only */
	{ 0x1c00, 0x1c00, input_port_3_r },		/* IN2 - Fax only */
	{ 0x2000, 0x3fff, MRA_BANK1 },			/* Fax only */
	{ 0x4000, 0x43ff, MRA_RAM },
	{ 0x5100, 0x5100, input_port_0_r },		/* DSW */
	{ 0x5101, 0x5101, input_port_1_r },		/* IN0 */
	{ 0x5103, 0x5103, exidy_interrupt_r },	/* IN1 */
	{ 0x5200, 0x520F, pia_0_r },
	{ 0x5213, 0x5213, input_port_3_r },		/* IN2 */
	{ 0x6000, 0x6fff, MRA_RAM },			/* Fax, Pepper II only */
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress fax_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07ff, MWA_RAM },			/* Fax only */
	{ 0x2000, 0x2000, fax_bank_select_w },	/* Fax only */
	{ 0x4000, 0x43ff, videoram_w, &videoram, &videoram_size },
	{ 0x5000, 0x5000, MWA_RAM, &exidy_sprite1_xpos },
	{ 0x5040, 0x5040, MWA_RAM, &exidy_sprite1_ypos },
	{ 0x5080, 0x5080, MWA_RAM, &exidy_sprite2_xpos },
	{ 0x50C0, 0x50C0, MWA_RAM, &exidy_sprite2_ypos },
	{ 0x5100, 0x5100, MWA_RAM, &exidy_sprite_no },
	{ 0x5101, 0x5101, MWA_RAM, &exidy_sprite_enable },
	{ 0x5200, 0x520F, pia_0_w },
	{ 0x5210, 0x5212, exidy_color_w, &exidy_color_latch },
	{ 0x5213, 0x5217, MWA_NOP },			/* empty control lines on color/sound board */
	{ 0x6000, 0x6fff, exidy_characterram_w, &exidy_characterram }, /* two 6116 character RAMs */
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0fff, exidy_shriot_r },
	{ 0x1000, 0x100f, pia_1_r },
	{ 0x1800, 0x1fff, exidy_sh8253_r },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x2800, 0x2fff, exidy_sh6840_r },
	{ 0x5800, 0x7fff, MRA_ROM },
	{ 0x8000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0fff, exidy_shriot_w },
	{ 0x1000, 0x100f, pia_1_w },
	{ 0x1800, 0x1fff, exidy_sh8253_w },
	{ 0x2000, 0x27ff, MWA_RAM },
	{ 0x2800, 0x2fff, exidy_sh6840_w },
	{ 0x3000, 0x3700, exidy_sfxctrl_w },
	{ 0x5800, 0x7fff, MWA_ROM },
	{ 0x8000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress cvsd_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress cvsd_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct IOWritePort cvsd_iowrite[] =
{
	{ 0x00, 0xff, mtrap_voiceio_w },
	{ -1 }
};

static struct IOReadPort cvsd_ioread[] =
{
	{ 0x00, 0xff, mtrap_voiceio_r },
	{ -1 }
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( sidetrac )
	PORT_START              /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2")
	PORT_DIPSETTING(    0x01, "3")
	PORT_DIPSETTING(    0x02, "4")
	PORT_DIPSETTING(    0x03, "5")
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
/* 0x0c 2C_1C */
	PORT_DIPNAME( 0x10, 0x10, "Top Score Award" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0xFF, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


INPUT_PORTS_START( targ )
	PORT_START              /* DSW0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN2 ) /* upright/cocktail switch? */
	PORT_DIPNAME( 0x02, 0x00, "P Coinage" )
	PORT_DIPSETTING(    0x00, "10P/1 C 50P Coin/6 Cs" )
	PORT_DIPSETTING(    0x02, "2x10P/1 C 50P Coin/3 Cs" )
	PORT_DIPNAME( 0x04, 0x00, "Top Score Award" )
	PORT_DIPSETTING(    0x00, "Credit" )
	PORT_DIPSETTING(    0x04, "Extended Play" )
	PORT_DIPNAME( 0x18, 0x08, "Q Coinage" )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "1C/1C (no display)" )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, "Currency" )
	PORT_DIPSETTING(    0x80, "Quarters" )
	PORT_DIPSETTING(    0x00, "Pence" )

	PORT_START      /* IN0 */
	PORT_BIT( 0x7F, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0xFF, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


/* identical to Targ, the only difference is the additional Language dip switch */
INPUT_PORTS_START( spectar )
	PORT_START              /* DSW0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN2 ) /* upright/cocktail switch? */
	PORT_DIPNAME( 0x02, 0x00, "P Coinage" )
	PORT_DIPSETTING(    0x00, "10P/1 C 50P Coin/6 Cs" )
	PORT_DIPSETTING(    0x02, "2x10P/1 C 50P Coin/3 Cs" )
	PORT_DIPNAME( 0x04, 0x00, "Top Score Award" )
	PORT_DIPSETTING(    0x00, "Credit" )
	PORT_DIPSETTING(    0x04, "Extended Play" )
	PORT_DIPNAME( 0x18, 0x08, "Q Coinage" )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "1C/1C (no display)" )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, "Currency" )
	PORT_DIPSETTING(    0x80, "Quarters" )
	PORT_DIPSETTING(    0x00, "Pence" )

	PORT_START      /* IN0 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x01, "French" )
	PORT_DIPSETTING(    0x02, "German" )
	PORT_DIPSETTING(    0x03, "Spanish" )
	PORT_BIT( 0x1c, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


INPUT_PORTS_START( mtrap )
	PORT_START      /* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "30000" )
	PORT_DIPSETTING(    0x04, "40000" )
	PORT_DIPSETTING(    0x02, "50000" )
	PORT_DIPSETTING(    0x00, "60000" )
	PORT_DIPNAME( 0x98, 0x98, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 2C/1C Coin B 1C/3C" )
	PORT_DIPSETTING(    0x98, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "Coin A 1C/1C Coin B 1C/4C" )
	PORT_DIPSETTING(    0x18, "Coin A 1C/1C Coin B 1C/5C" )
	PORT_DIPSETTING(    0x88, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, "Coin A 1C/3C Coin B 2C/7C" )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x00, "5" )

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_BUTTON1, "Dog Button", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
/*
	The schematics claim these exist, but there's nothing in
	the ROMs to support that claim (as far as I can see):

	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x01, "French" )
	PORT_DIPSETTING(    0x02, "German" )
	PORT_DIPSETTING(    0x03, "Spanish" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
*/

	PORT_BIT( 0x1f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START              /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_BUTTON2, "Yellow Button", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_BUTTON3, "Red Button", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x04, 0x04, IPT_DIPSWITCH_NAME, DEF_STR( Free_Play ), IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(0x00, DEF_STR( On ) )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_BUTTON4, "Blue Button", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


INPUT_PORTS_START( venture )
	PORT_START      /* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "20000" )
	PORT_DIPSETTING(    0x04, "30000" )
	PORT_DIPSETTING(    0x02, "40000" )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPNAME( 0x98, 0x80, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x88, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x98, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, "Pence: A 2C/1C B 1C/3C" )
	PORT_DIPSETTING(    0x18, "Pence: A 1C/1C B 1C/6C" )
	/*0x10 same as 0x00 */
	/*0x90 same as 0x80 */
	PORT_DIPNAME( 0x60, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x60, "5" )

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
/*
	The schematics claim these exist, but there's nothing in
	the ROMs to support that claim (as far as I can see):

	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x01, "French" )
	PORT_DIPSETTING(    0x02, "German" )
	PORT_DIPSETTING(    0x03, "Spanish" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
*/

	PORT_BIT( 0x1f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
INPUT_PORTS_END


INPUT_PORTS_START( pepper2 )
	PORT_START              /* DSW */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "40000" )
	PORT_DIPSETTING(    0x04, "50000" )
	PORT_DIPSETTING(    0x02, "60000" )
	PORT_DIPSETTING(    0x00, "70000" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x98, 0x98, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 2C/1C Coin B 1C/3C" )
	PORT_DIPSETTING(    0x98, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "Coin A 1C/1C Coin B 1C/4C" )
	PORT_DIPSETTING(    0x18, "Coin A 1C/1C Coin B 1C/5C" )
	PORT_DIPSETTING(    0x88, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, "1 Coin/3 Credits 2C/7C" )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )

	PORT_START              /* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
/*
	The schematics claim these exist, but there's nothing in
	the ROMs to support that claim (as far as I can see):

	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x01, "French" )
	PORT_DIPSETTING(    0x02, "German" )
	PORT_DIPSETTING(    0x03, "Spanish" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
*/

	PORT_BIT( 0x1F, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
INPUT_PORTS_END


INPUT_PORTS_START( fax )
	PORT_START              /* DSW */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_DIPNAME( 0x06, 0x06, "Bonus Time" )
	PORT_DIPSETTING(    0x06, "8000" )
	PORT_DIPSETTING(    0x04, "13000" )
	PORT_DIPSETTING(    0x02, "18000" )
	PORT_DIPSETTING(    0x00, "25000" )
	PORT_DIPNAME( 0x60, 0x60, "Game/Bonus Times" )
	PORT_DIPSETTING(    0x60, ":32/:24" )
	PORT_DIPSETTING(    0x40, ":48/:36" )
	PORT_DIPSETTING(    0x20, "1:04/:48" )
	PORT_DIPSETTING(    0x00, "1:12/1:04" )
	PORT_DIPNAME( 0x98, 0x98, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 2C/1C Coin B 1C/3C" )
	PORT_DIPSETTING(    0x98, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "Coin A 1C/1C Coin B 1C/4C" )
	PORT_DIPSETTING(    0x18, "Coin A 1C/1C Coin B 1C/5C" )
	PORT_DIPSETTING(    0x88, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, "1 Coin/3 Credits 2C/7C" )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )

	PORT_START              /* IN0 */
	PORT_BIT ( 0x7f, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
/*
	The schematics claim these exist, but there's nothing in
	the ROMs to support that claim (as far as I can see):

	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x01, "French" )
	PORT_DIPSETTING(    0x02, "German" )
	PORT_DIPSETTING(    0x03, "Spanish" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
*/

	PORT_BIT( 0x1b, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* Set when motion object 1 is drawn? */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )   /* VBlank */

	PORT_START /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START /* IN3 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout charlayout_1bpp =
{
	8,8,
	256,
	1,
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout charlayout_2bpp =
{
	8,8,
	256,
	2,
	{ 0, 256*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,1),
	1,
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8},
	8*32
};


static struct GfxDecodeInfo gfxdecodeinfo_1bpp[] =
{
	{ REGION_CPU1, 0x4800, &charlayout_1bpp, 0, 4 },	/* the game dynamically modifies this */
	{ REGION_GFX1, 0x0000, &spritelayout,    8, 2 },
	{ -1 }
};


static struct GfxDecodeInfo gfxdecodeinfo_2bpp[] =
{
	{ REGION_CPU1, 0x6000, &charlayout_2bpp, 0, 4 },	/* the game dynamically modifies this */
	{ REGION_GFX1, 0x0000, &spritelayout,   16, 2 },
	{ -1 }
};



/*************************************
 *
 *	Sound  definitions
 *
 *************************************/

static const char *targ_sample_names[] =
{
	"*targ",
	"expl.wav",
	"shot.wav",
	"sexpl.wav",
	"spslow.wav",
	"spfast.wav",
	0       /* end of array */
};

static struct Samplesinterface targ_samples_interface =
{
	3,	/* 3 Channels */
	25,	/* volume */
	targ_sample_names
};

static struct CustomSound_interface targ_custom_interface =
{
	targ_sh_start,
	targ_sh_stop
};

static struct DACinterface targ_DAC_interface =
{
	1,
	{ 100 }
};

static struct hc55516_interface cvsd_interface =
{
	1,          /* 1 chip */
	{ 80 }
};

static struct CustomSound_interface exidy_custom_interface =
{
	exidy_sh_start
};



/*************************************
 *
 *	Machine drivers
 *
 *************************************/

static struct MachineDriver machine_driver_targ =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			11289000/16,
			main_readmem,main_writemem,0,0,
			exidy_vblank_interrupt,1
		},
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 31*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo_1bpp,
	PALETTE_LEN, COLORTABLE_LEN,
	exidy_vh_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	exidy_vh_eof,
	exidy_vh_start,
	exidy_vh_stop,
	exidy_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM,  &targ_custom_interface },
		{ SOUND_SAMPLES, &targ_samples_interface },
		{ SOUND_DAC,     &targ_DAC_interface }
	}
};


static struct MachineDriver machine_driver_mtrap =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			11289000/16,
			main_readmem,main_writemem,0,0,
			exidy_vblank_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			3579545/4,
			sound_readmem,sound_writemem,0,0,
	    	ignore_interrupt,0
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545/2,
			cvsd_readmem,cvsd_writemem,cvsd_ioread,cvsd_iowrite,
			ignore_interrupt,0
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
    32,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 31*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo_1bpp,
	PALETTE_LEN, COLORTABLE_LEN,
	exidy_vh_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	exidy_vh_eof,
	exidy_vh_start,
	exidy_vh_stop,
	exidy_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_HC55516, &cvsd_interface },
		{ SOUND_CUSTOM,  &exidy_custom_interface }
	}
};


static struct MachineDriver machine_driver_venture =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			11289000/16,
			main_readmem,main_writemem,0,0,
			exidy_vblank_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			3579545/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 31*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo_1bpp,
	PALETTE_LEN, COLORTABLE_LEN,
	exidy_vh_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	exidy_vh_eof,
	exidy_vh_start,
	exidy_vh_stop,
	exidy_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM,  &exidy_custom_interface }
	}
};


static struct MachineDriver machine_driver_pepper2 =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			11289000/16,
			main_readmem,main_writemem,0,0,
			exidy_vblank_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			3579545/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 31*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo_2bpp,
	PALETTE_LEN, COLORTABLE_LEN,
	exidy_vh_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	exidy_vh_eof,
	exidy_vh_start,
	exidy_vh_stop,
	exidy_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM, &exidy_custom_interface }
	}

};


static struct MachineDriver machine_driver_fax =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			11289000/16,
			fax_readmem,fax_writemem,0,0,
			exidy_vblank_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			3579545/4,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 31*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo_2bpp,
	PALETTE_LEN, COLORTABLE_LEN,
	exidy_vh_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	exidy_vh_eof,
	exidy_vh_start,
	exidy_vh_stop,
	exidy_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM, &exidy_custom_interface }
	}
};



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( sidetrac )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "stl8a-1",     0x2800, 0x0800, 0xe41750ff )
	ROM_LOAD( "stl7a-2",     0x3000, 0x0800, 0x57fb28dc )
	ROM_LOAD( "stl6a-2",     0x3800, 0x0800, 0x4226d469 )
	ROM_RELOAD(              0xf800, 0x0800 ) /* for the reset/interrupt vectors */
	ROM_LOAD( "stl9c-1",     0x4800, 0x0400, 0x08710a84 ) /* prom instead of ram chr gen*/

	ROM_REGION( 0x0200, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "stl11d",      0x0000, 0x0200, 0x3bd1acc1 )
ROM_END


ROM_START( targ )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "targ10a1",    0x1800, 0x0800, 0x969744e1 )
	ROM_LOAD( "targ09a1",    0x2000, 0x0800, 0xa177a72d )
	ROM_LOAD( "targ08a1",    0x2800, 0x0800, 0x6e6928a5 )
	ROM_LOAD( "targ07a4",    0x3000, 0x0800, 0xe2f37f93 )
	ROM_LOAD( "targ06a3",    0x3800, 0x0800, 0xa60a1bfc )
	ROM_RELOAD(              0xf800, 0x0800 ) /* for the reset/interrupt vectors */

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "targ11d1",    0x0000, 0x0400, 0x9f03513e )
ROM_END


ROM_START( spectar )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "spl11a-3",    0x1000, 0x0800, 0x08880aff )
	ROM_LOAD( "spl10a-2",    0x1800, 0x0800, 0xfca667c1 )
	ROM_LOAD( "spl9a-3",     0x2000, 0x0800, 0x9d4ce8ba )
	ROM_LOAD( "spl8a-2",     0x2800, 0x0800, 0xcfacbadf )
	ROM_LOAD( "spl7a-2",     0x3000, 0x0800, 0x4c4741ff )
	ROM_LOAD( "spl6a-2",     0x3800, 0x0800, 0x0cb46b25 )
	ROM_RELOAD(              0xf800, 0x0800 )  /* for the reset/interrupt vectors */

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hrl11d-2",    0x0000, 0x0400, 0xc55b645d )  /* this is actually not used (all FF) */
	ROM_CONTINUE(            0x0000, 0x0400 )  /* overwrite with the real one */
ROM_END

ROM_START( spectar1 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "spl12a1",     0x0800, 0x0800, 0x7002efb4 )
	ROM_LOAD( "spl11a1",     0x1000, 0x0800, 0x8eb8526a )
	ROM_LOAD( "spl10a1",     0x1800, 0x0800, 0x9d169b3d )
	ROM_LOAD( "spl9a1",      0x2000, 0x0800, 0x40e3eba1 )
	ROM_LOAD( "spl8a1",      0x2800, 0x0800, 0x64d8eb84 )
	ROM_LOAD( "spl7a1",      0x3000, 0x0800, 0xe08b0d8d )
	ROM_LOAD( "spl6a1",      0x3800, 0x0800, 0xf0e4e71a )
	ROM_RELOAD(              0xf800, 0x0800 )   /* for the reset/interrupt vectors */

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hrl11d-2",    0x0000, 0x0400, 0xc55b645d )  /* this is actually not used (all FF) */
	ROM_CONTINUE(            0x0000, 0x0400 )  /* overwrite with the real one */
ROM_END


ROM_START( mtrap )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "mtl11a.bin",  0xa000, 0x1000, 0xbd6c3eb5 )
	ROM_LOAD( "mtl10a.bin",  0xb000, 0x1000, 0x75b0593e )
	ROM_LOAD( "mtl9a.bin",   0xc000, 0x1000, 0x28dd20ff )
	ROM_LOAD( "mtl8a.bin",   0xd000, 0x1000, 0xcc09f7a4 )
	ROM_LOAD( "mtl7a.bin",   0xe000, 0x1000, 0xcaafbb6d )
	ROM_LOAD( "mtl6a.bin",   0xf000, 0x1000, 0xd85e52ca )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "mta5a.bin",   0x6800, 0x0800, 0xdbe4ec02 )
	ROM_LOAD( "mta6a.bin",   0x7000, 0x0800, 0xc00f0c05 )
	ROM_LOAD( "mta7a.bin",   0x7800, 0x0800, 0xf3f16ca7 )
	ROM_RELOAD(              0xf800, 0x0800 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* 64k for digital sound processor */
	ROM_LOAD( "mta2a.bin", 0x0000, 0x1000, 0x13db8ed3 )
	ROM_LOAD( "mta3a.bin", 0x1000, 0x1000, 0x31bdfe5c )
	ROM_LOAD( "mta4a.bin", 0x2000, 0x1000, 0x1502d0e8 )
	ROM_LOAD( "mta1a.bin", 0x3000, 0x1000, 0x658482a6 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mtl11d.bin",  0x0000, 0x0800, 0xc6e4d339 )
ROM_END

ROM_START( mtrap3 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "mtl-3.11a",   0xa000, 0x1000, 0x4091be6e )
	ROM_LOAD( "mtl-3.10a",   0xb000, 0x1000, 0x38250c2f )
	ROM_LOAD( "mtl-3.9a",    0xc000, 0x1000, 0x2eec988e )
	ROM_LOAD( "mtl-3.8a",    0xd000, 0x1000, 0x744b4b1c )
	ROM_LOAD( "mtl-3.7a",    0xe000, 0x1000, 0xea8ec479 )
	ROM_LOAD( "mtl-3.6a",    0xf000, 0x1000, 0xd72ba72d )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "mta5a.bin",   0x6800, 0x0800, 0xdbe4ec02 )
	ROM_LOAD( "mta6a.bin",   0x7000, 0x0800, 0xc00f0c05 )
	ROM_LOAD( "mta7a.bin",   0x7800, 0x0800, 0xf3f16ca7 )
	ROM_RELOAD(              0xf800, 0x0800 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* 64k for digital sound processor */
	ROM_LOAD( "mta2a.bin", 0x0000, 0x1000, 0x13db8ed3 )
	ROM_LOAD( "mta3a.bin", 0x1000, 0x1000, 0x31bdfe5c )
	ROM_LOAD( "mta4a.bin", 0x2000, 0x1000, 0x1502d0e8 )
	ROM_LOAD( "mta1a.bin", 0x3000, 0x1000, 0x658482a6 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mtl11d.bin",  0x0000, 0x0800, 0xc6e4d339 )
ROM_END

ROM_START( mtrap4 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "mta411a.bin",  0xa000, 0x1000, 0x2879cb8d )
	ROM_LOAD( "mta410a.bin",  0xb000, 0x1000, 0xd7378af9 )
	ROM_LOAD( "mta49.bin",    0xc000, 0x1000, 0xbe667e64 )
	ROM_LOAD( "mta48a.bin",   0xd000, 0x1000, 0xde0442f8 )
	ROM_LOAD( "mta47a.bin",   0xe000, 0x1000, 0xcdf8c6a8 )
	ROM_LOAD( "mta46a.bin",   0xf000, 0x1000, 0x77d3f2e6 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "mta5a.bin",    0x6800, 0x0800, 0xdbe4ec02 )
	ROM_LOAD( "mta6a.bin",    0x7000, 0x0800, 0xc00f0c05 )
	ROM_LOAD( "mta7a.bin",    0x7800, 0x0800, 0xf3f16ca7 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* 64k for digital sound processor */
	ROM_LOAD( "mta2a.bin", 0x0000,0x1000,0x13db8ed3 )
	ROM_LOAD( "mta3a.bin", 0x1000,0x1000,0x31bdfe5c )
	ROM_LOAD( "mta4a.bin", 0x2000,0x1000,0x1502d0e8 )
	ROM_LOAD( "mta1a.bin", 0x3000,0x1000,0x658482a6 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mtl11d.bin",   0x0000, 0x0800, 0xc6e4d339 )
ROM_END


ROM_START( venture )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "13a-cpu",      0x8000, 0x1000, 0xf4e4d991 )
	ROM_LOAD( "12a-cpu",      0x9000, 0x1000, 0xc6d8cb04 )
	ROM_LOAD( "11a-cpu",      0xa000, 0x1000, 0x3bdb01f4 )
	ROM_LOAD( "10a-cpu",      0xb000, 0x1000, 0x0da769e9 )
	ROM_LOAD( "9a-cpu",       0xc000, 0x1000, 0x0ae05855 )
	ROM_LOAD( "8a-cpu",       0xd000, 0x1000, 0x4ae59676 )
	ROM_LOAD( "7a-cpu",       0xe000, 0x1000, 0x48d66220 )
	ROM_LOAD( "6a-cpu",       0xf000, 0x1000, 0x7b78cf49 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "3a-ac",        0x5800, 0x0800, 0x4ea1c3d9 )
	ROM_LOAD( "4a-ac",        0x6000, 0x0800, 0x5154c39e )
	ROM_LOAD( "5a-ac",        0x6800, 0x0800, 0x1e1e3916 )
	ROM_LOAD( "6a-ac",        0x7000, 0x0800, 0x80f3357a )
	ROM_LOAD( "7a-ac",        0x7800, 0x0800, 0x466addc7 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11d-cpu",      0x0000, 0x0800, 0xb4bb2503 )
ROM_END

ROM_START( venture2 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "vent_a13.cpu", 0x8000, 0x1000, 0x4c833f99 )
	ROM_LOAD( "vent_a12.cpu", 0x9000, 0x1000, 0x8163cefc )
	ROM_LOAD( "vent_a11.cpu", 0xa000, 0x1000, 0x324a5054 )
	ROM_LOAD( "vent_a10.cpu", 0xb000, 0x1000, 0x24358203 )
	ROM_LOAD( "vent_a9.cpu",  0xc000, 0x1000, 0x04428165 )
	ROM_LOAD( "vent_a8.cpu",  0xd000, 0x1000, 0x4c1a702a )
	ROM_LOAD( "vent_a7.cpu",  0xe000, 0x1000, 0x1aab27c2 )
	ROM_LOAD( "vent_a6.cpu",  0xf000, 0x1000, 0x767bdd71 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "3a-ac",        0x5800, 0x0800, 0x4ea1c3d9 )
	ROM_LOAD( "4a-ac",        0x6000, 0x0800, 0x5154c39e )
	ROM_LOAD( "5a-ac",        0x6800, 0x0800, 0x1e1e3916 )
	ROM_LOAD( "6a-ac",        0x7000, 0x0800, 0x80f3357a )
	ROM_LOAD( "7a-ac",        0x7800, 0x0800, 0x466addc7 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11d-cpu",      0x0000, 0x0800, 0xb4bb2503 )
ROM_END

ROM_START( venture4 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "vel13a-4",     0x8000, 0x1000, 0x1c5448f9 )
	ROM_LOAD( "vel12a-4",     0x9000, 0x1000, 0xe62491cc )
	ROM_LOAD( "vel11a-4",     0xa000, 0x1000, 0xe91faeaf )
	ROM_LOAD( "vel10a-4",     0xb000, 0x1000, 0xda3a2991 )
	ROM_LOAD( "vel9a-4",      0xc000, 0x1000, 0xd1887b11 )
	ROM_LOAD( "vel8a-4",      0xd000, 0x1000, 0x8e8153fc )
	ROM_LOAD( "vel7a-4",      0xe000, 0x1000, 0x0a091701 )
	ROM_LOAD( "vel6a-4",      0xf000, 0x1000, 0x7b165f67 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "vea3a-2",      0x5800, 0x0800, 0x83b8836f )
	ROM_LOAD( "4a-ac",        0x6000, 0x0800, 0x5154c39e )
	ROM_LOAD( "5a-ac",        0x6800, 0x0800, 0x1e1e3916 )
	ROM_LOAD( "6a-ac",        0x7000, 0x0800, 0x80f3357a )
	ROM_LOAD( "7a-ac",        0x7800, 0x0800, 0x466addc7 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vel11d-2",     0x0000, 0x0800, 0xea6fd981 )
ROM_END


ROM_START( pepper2 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "main_12a",     0x9000, 0x1000, 0x33db4737 )
	ROM_LOAD( "main_11a",     0xa000, 0x1000, 0xa1f43b1f )
	ROM_LOAD( "main_10a",     0xb000, 0x1000, 0x4d7d7786 )
	ROM_LOAD( "main_9a",      0xc000, 0x1000, 0xb3362298 )
	ROM_LOAD( "main_8a",      0xd000, 0x1000, 0x64d106ed )
	ROM_LOAD( "main_7a",      0xe000, 0x1000, 0xb1c6f07c )
	ROM_LOAD( "main_6a",      0xf000, 0x1000, 0x515b1046 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "audio_5a",     0x6800, 0x0800, 0x90e3c781 )
	ROM_LOAD( "audio_6a",     0x7000, 0x0800, 0xdd343e34 )
	ROM_LOAD( "audio_7a",     0x7800, 0x0800, 0xe02b4356 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "main_11d",     0x0000, 0x0800, 0xb25160cd )
ROM_END


ROM_START( hardhat )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "hhl-2.11a",    0xa000, 0x1000, 0x7623deea )
	ROM_LOAD( "hhl-2.10a",    0xb000, 0x1000, 0xe6bf2fb1 )
	ROM_LOAD( "hhl-2.9a",     0xc000, 0x1000, 0xacc2bce5 )
	ROM_LOAD( "hhl-2.8a",     0xd000, 0x1000, 0x23c7a2f8 )
	ROM_LOAD( "hhl-2.7a",     0xe000, 0x1000, 0x6f7ce1c2 )
	ROM_LOAD( "hhl-2.6a",     0xf000, 0x1000, 0x2a20cf10 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "hha-1.5a",     0x6800, 0x0800, 0x16a5a183 )
	ROM_LOAD( "hha-1.6a",     0x7000, 0x0800, 0xbde64021 )
	ROM_LOAD( "hha-1.7a",     0x7800, 0x0800, 0x505ee5d3 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hhl-1.11d",    0x0000, 0x0800, 0xdbcdf353 )
ROM_END


ROM_START( fax )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 64k for code + 192k for extra memory */
	ROM_LOAD( "fxl8-13a.32",  0x8000, 0x1000, 0x8e30bf6b )
	ROM_LOAD( "fxl8-12a.32",  0x9000, 0x1000, 0x60a41ff1 )
	ROM_LOAD( "fxl8-11a.32",  0xA000, 0x1000, 0x2c9cee8a )
	ROM_LOAD( "fxl8-10a.32",  0xB000, 0x1000, 0x9b03938f )
	ROM_LOAD( "fxl8-9a.32",   0xC000, 0x1000, 0xfb869f62 )
	ROM_LOAD( "fxl8-8a.32",   0xD000, 0x1000, 0xdb3470bc )
	ROM_LOAD( "fxl8-7a.32",   0xE000, 0x1000, 0x1471fef5 )
	ROM_LOAD( "fxl8-6a.32",   0xF000, 0x1000, 0x812e39f3 )
	/* Banks of question ROMs */
	ROM_LOAD( "fxd-1c.64",  0x10000, 0x2000, 0xfd7e3137 )
	ROM_LOAD( "fxd-2c.64",  0x12000, 0x2000, 0xe78cb16f )
	ROM_LOAD( "fxd-3c.64",  0x14000, 0x2000, 0x57a94c6f )
	ROM_LOAD( "fxd-4c.64",  0x16000, 0x2000, 0x9036c5a2 )
	ROM_LOAD( "fxd-5c.64",  0x18000, 0x2000, 0x38c03405 )
	ROM_LOAD( "fxd-6c.64",  0x1A000, 0x2000, 0xf48fc308 )
	ROM_LOAD( "fxd-7c.64",  0x1C000, 0x2000, 0xcf93b924 )
	ROM_LOAD( "fxd-8c.64",  0x1E000, 0x2000, 0x607b48da )
	ROM_LOAD( "fxd-1b.64",  0x20000, 0x2000, 0x62872d4f )
	ROM_LOAD( "fxd-2b.64",  0x22000, 0x2000, 0x625778d0 )
	ROM_LOAD( "fxd-3b.64",  0x24000, 0x2000, 0xc3473dee )
	ROM_LOAD( "fxd-4b.64",  0x26000, 0x2000, 0xe39a15f5 )
	ROM_LOAD( "fxd-5b.64",  0x28000, 0x2000, 0x101a9d70 )
	ROM_LOAD( "fxd-6b.64",  0x2A000, 0x2000, 0x374a8f05 )
	ROM_LOAD( "fxd-7b.64",  0x2C000, 0x2000, 0xf7e7f824 )
	ROM_LOAD( "fxd-8b.64",  0x2E000, 0x2000, 0x8f1a5287 )
	ROM_LOAD( "fxd-1a.64",  0x30000, 0x2000, 0xfc5e6344 )
	ROM_LOAD( "fxd-2a.64",  0x32000, 0x2000, 0x43cf60b3 )
	ROM_LOAD( "fxd-3a.64",  0x34000, 0x2000, 0x6b7d29cb )
	ROM_LOAD( "fxd-4a.64",  0x36000, 0x2000, 0xb9de3c2d )
	ROM_LOAD( "fxd-5a.64",  0x38000, 0x2000, 0x67285bc6 )
	ROM_LOAD( "fxd-6a.64",  0x3A000, 0x2000, 0xba67b7b2 )
	/* The last two ROM sockets were apparently never populated */
//	ROM_LOAD( "fxd-7a.64",  0x3C000, 0x2000, 0x00000000 )
//	ROM_LOAD( "fxd-8a.64",  0x3E000, 0x2000, 0x00000000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for audio */
	ROM_LOAD( "fxa2-5a.16",   0x6800, 0x0800, 0x7c525aec )
	ROM_LOAD( "fxa2-6a.16",   0x7000, 0x0800, 0x2b3bfc44 )
	ROM_LOAD( "fxa2-7a.16",   0x7800, 0x0800, 0x578c62b7 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fxl1-11d.32",  0x0000, 0x0800, 0x54fc873d )
	ROM_CONTINUE(             0x0000, 0x0800 )       /* overwrite with the real one - should be a 2716? */
ROM_END



/*************************************
 *
 *	Driver init
 *
 *************************************/

void init_sidetrac(void)
{
	exidy_palette 			= sidetrac_palette;
	exidy_colortable 		= exidy_1bpp_colortable;
	exidy_collision_mask 	= 0x00;
	exidy_collision_invert	= 0x00;

	/* there is no sprite enable register so we have to fake it out */
	*exidy_sprite_enable 	= 0x10;
	targ_spec_flag 			= 0;

	/* sound is handled directly instead of via a PIA */
	install_mem_write_handler(0, 0x5200, 0x5201, targ_sh_w);
}

void init_targ(void)
{
	exidy_palette 			= targ_palette;
	exidy_colortable 		= exidy_1bpp_colortable;
	exidy_collision_mask 	= 0x00;
	exidy_collision_invert	= 0x00;

	/* there is no sprite enable register so we have to fake it out */
	*exidy_sprite_enable 	= 0x10;
	targ_spec_flag 			= 1;

	/* sound is handled directly instead of via a PIA */
	install_mem_write_handler(0, 0x5200, 0x5201, targ_sh_w);
}

void init_spectar(void)
{
	exidy_palette 			= spectar_palette;
	exidy_colortable 		= exidy_1bpp_colortable;
	exidy_collision_mask 	= 0x00;
	exidy_collision_invert	= 0x00;

	/* there is no sprite enable register so we have to fake it out */
	*exidy_sprite_enable 	= 0x10;
	targ_spec_flag 			= 0;

	/* sound is handled directly instead of via a PIA */
	install_mem_write_handler(0, 0x5200, 0x5201, targ_sh_w);
}

void init_mtrap(void)
{
	exidy_colortable 		= exidy_1bpp_colortable;
	exidy_collision_mask 	= 0x14;
	exidy_collision_invert	= 0x00;
}

void init_venture(void)
{
	exidy_colortable 		= exidy_1bpp_colortable;
	exidy_collision_mask 	= 0x04;
	exidy_collision_invert	= 0x04;
}

void init_pepper2(void)
{
	exidy_colortable 		= exidy_2bpp_colortable;
	exidy_collision_mask 	= 0x14;
	exidy_collision_invert	= 0x04;

	/* two 6116 character RAMs */
	install_mem_write_handler(0, 0x4800, 0x4fff, MWA_NOP);
	exidy_characterram = (UINT8*)install_mem_write_handler(0, 0x6000, 0x6fff, exidy_characterram_w);
}

void init_fax(void)
{
	exidy_colortable 		= exidy_2bpp_colortable;
	exidy_collision_mask 	= 0x04;
	exidy_collision_invert	= 0x04;

	/* Initialize our ROM question bank */
	fax_bank_select_w(0,0);
}



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1979, sidetrac, 0,       targ,    sidetrac, sidetrac, ROT0, "Exidy", "Side Track" )
GAME( 1980, targ,     0,       targ,    targ,     targ,     ROT0, "Exidy", "Targ" )
GAME( 1980, spectar,  0,       targ,    spectar,  spectar,  ROT0, "Exidy", "Spectar (revision 3)" )
GAME( 1980, spectar1, spectar, targ,    spectar,  spectar,  ROT0, "Exidy", "Spectar (revision 1?)" )
GAME( 1981, mtrap,    0,       mtrap,   mtrap,    mtrap,    ROT0, "Exidy", "Mouse Trap (version 5)" )
GAME( 1981, mtrap3,   mtrap,   mtrap,   mtrap,    mtrap,    ROT0, "Exidy", "Mouse Trap (version 3)" )
GAME( 1981, mtrap4,   mtrap,   mtrap,   mtrap,    mtrap,    ROT0, "Exidy", "Mouse Trap (version 4)" )
GAME( 1981, venture,  0,       venture, venture,  venture,  ROT0, "Exidy", "Venture (version 5 set 1)" )
GAME( 1981, venture2, venture, venture, venture,  venture,  ROT0, "Exidy", "Venture (version 5 set 2)" )
GAME( 1981, venture4, venture, venture, venture,  venture,  ROT0, "Exidy", "Venture (version 4)" )
GAME( 1982, pepper2,  0,       pepper2, pepper2,  pepper2,  ROT0, "Exidy", "Pepper II" )
GAME( 1982, hardhat,  0,       pepper2, pepper2,  pepper2,  ROT0, "Exidy", "Hard Hat" )
GAME( 1983, fax,      0,       fax,     fax,      fax,      ROT0, "Exidy", "Fax" )
