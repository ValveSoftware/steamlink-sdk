#include "../vidhrdw/karnov.cpp"

/***************************************************************************

  Karnov (USA version)                   (c) 1987 Data East USA
  Karnov (Japanese version)              (c) 1987 Data East Corporation
  Wonder Planet (Japanese version)       (c) 1987 Data East Corporation
  Chelnov (USA version)                  (c) 1988 Data East USA
  Chelnov (Japanese version)             (c) 1987 Data East Corporation


  Emulation by Bryan McPhail, mish@tendril.co.uk


NOTE!  Karnov USA & Karnov Japan sets have different gameplay!
  and Chelnov USA & Chelnov Japan sets have different gameplay!

These games use a 68000 main processor with a 6502, YM2203C and YM3526 for
sound.  Karnov was a major pain to get going because of the
'protection' on the main player sprite, probably connected to the Intel
microcontroller on the board.  The game is very sensitive to the wrong values
at the input ports...

There is another Karnov rom set - a bootleg version of the Japanese roms with
the Data East copyright removed - not supported because the original Japanese
roms work fine.

One of the two color PROMs for chelnov and chelnoj is different; one is most
likely a bad read, but I don't know which one.

Thanks to Oliver Stabel <stabel@rhein-neckar.netsurf.de> for confirming some
of the sprite & control information :)

Cheats:

Karnov - put 0x30 at 0x60201 to skip a level
Chelnov - level number at 0x60189 - enter a value at cartoon intro

*******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"

void karnov_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void karnov_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void wndrplnt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( karnov_foreground_w );

int karnov_vh_start (void);
void karnov_vh_stop (void);

static int i8751_return;
static int KARNOV, CHELNOV, WNDRPLNT; /* :) */
static unsigned char *karnov_ram;
extern int karnov_scroll[4];

/******************************************************************************/

/* Emulation of the protected microcontroller - for coins & general protection */
static void karnov_i8751_w(int data)
{
	i8751_return=0;
	if (data==0x100 && KARNOV==2) i8751_return=0x56a; /* Japan version */
	if (data==0x100 && KARNOV==1) i8751_return=0x56b; /* USA version */
	if ((data&0xf00)==0x300) i8751_return=(data&0xff)*0x12; /* Player sprite mapping */

	/* I'm not sure the ones marked ^ appear in the right order */
	if (data==0x400) i8751_return=0x4000; /* Get The Map... */
	if (data==0x402) i8751_return=0x40a6; /* Ancient Ruins */
	if (data==0x403) i8751_return=0x4054; /* Forest... */
	if (data==0x404) i8751_return=0x40de; /* ^Rocky hills */
	if (data==0x405) i8751_return=0x4182; /* Sea */
	if (data==0x406) i8751_return=0x41ca; /* Town */
	if (data==0x407) i8751_return=0x421e; /* Desert */
	if (data==0x401) i8751_return=0x4138; /* ^Whistling wind */
	if (data==0x408) i8751_return=0x4276; /* ^Heavy Gates */

//	if (!i8751_return && data!=0x300) logerror("CPU %04x - Unknown Write %02x intel\n",cpu_get_pc(),data);

	cpu_cause_interrupt(0,6); /* Signal main cpu task is complete */
}

static void wndrplnt_i8751_w(int data)
{
//	static int level;

	i8751_return=0;
	if (data==0x100) i8751_return=0x67a;
//	if (data==0x200) i8751_return=0x214;

	/* USA version will have different values for these commands */

	if (data==0x300) i8751_return=0x17; /* Copyright text on title screen */
//	if (data==0x300) i8751_return=0x1; /* (USA) Copyright text on title screen */

	//if (data!=0x600) logerror("CPU %04x - Unknown Write %02x intel\n",cpu_get_pc(),data);

	cpu_cause_interrupt(0,6); /* Signal main cpu task is complete */
}

static void chelnov_i8751_w(int data)
{
	static int level;

	i8751_return=0;
	if (data==0x200 && CHELNOV==2) i8751_return=0x7734; /* Japan version */
	if (data==0x200 && CHELNOV==1) i8751_return=0x783e; /* USA version */
	if (data==0x100 && CHELNOV==2) i8751_return=0x71a; /* Japan version */
	if (data==0x100 && CHELNOV==1) i8751_return=0x71b; /* USA version */
	if (data>=0x6000 && data<0x8000) i8751_return=1;  /* patched */
	if ((data&0xf000)==0x1000) level=1; /* Level 1 */
	if ((data&0xf000)==0x2000) level++; /* Level Increment */
	if ((data&0xf000)==0x3000) {        /* Sprite table mapping */
		int b=data&0xff;
		switch (level) {
			case 1: /* Level 1, Sprite mapping tables */
				if (CHELNOV==1) { /* USA */
					if (b<2) i8751_return=0;
					else if (b<6) i8751_return=1;
					else if (b<0xb) i8751_return=2;
					else if (b<0xf) i8751_return=3;
					else if (b<0x13) i8751_return=4;
					else i8751_return=5;
				} else { /* Japan */
					if (b<3) i8751_return=0;
					else if (b<8) i8751_return=1;
					else if (b<0xc) i8751_return=2;
					else if (b<0x10) i8751_return=3;
					else if (b<0x19) i8751_return=4;
					else if (b<0x1b) i8751_return=5;
					else if (b<0x22) i8751_return=6;
					else if (b<0x28) i8751_return=7;
					else i8751_return=8;
				}
				break;
			case 2: /* Level 2, Sprite mapping tables, USA & Japan are the same */
				if (b<3) i8751_return=0;
				else if (b<9) i8751_return=1;
				else if (b<0x11) i8751_return=2;
				else if (b<0x1b) i8751_return=3;
				else if (b<0x21) i8751_return=4;
				else if (b<0x28) i8751_return=5;
				else i8751_return=6;
				break;
			case 3: /* Level 3, Sprite mapping tables, USA & Japan are the same */
				if (b<5) i8751_return=0;
				else if (b<9) i8751_return=1;
				else if (b<0xd) i8751_return=2;
				else if (b<0x11) i8751_return=3;
				else if (b<0x1b) i8751_return=4;
				else if (b<0x1c) i8751_return=5;
				else if (b<0x22) i8751_return=6;
				else if (b<0x27) i8751_return=7;
				else i8751_return=8;
				break;
			case 4: /* Level 4, Sprite mapping tables, USA & Japan are the same */
				if (b<4) i8751_return=0;
				else if (b<0xc) i8751_return=1;
				else if (b<0xf) i8751_return=2;
				else if (b<0x19) i8751_return=3;
				else if (b<0x1c) i8751_return=4;
				else if (b<0x22) i8751_return=5;
				else if (b<0x29) i8751_return=6;
				else i8751_return=7;
				break;
			case 5: /* Level 5, Sprite mapping tables */
				if (b<7) i8751_return=0;
				else if (b<0xe) i8751_return=1;
				else if (b<0x14) i8751_return=2;
				else if (b<0x1a) i8751_return=3;
				else if (b<0x23) i8751_return=4;
				else if (b<0x27) i8751_return=5;
				else i8751_return=6;
				break;
			case 6: /* Level 6, Sprite mapping tables */
				if (b<3) i8751_return=0;
				else if (b<0xb) i8751_return=1;
				else if (b<0x11) i8751_return=2;
				else if (b<0x17) i8751_return=3;
				else if (b<0x1d) i8751_return=4;
				else if (b<0x24) i8751_return=5;
				else i8751_return=6;
				break;
			case 7: /* Level 7, Sprite mapping tables */
				if (b<5) i8751_return=0;
				else if (b<0xb) i8751_return=1;
				else if (b<0x11) i8751_return=2;
				else if (b<0x1a) i8751_return=3;
				else if (b<0x21) i8751_return=4;
				else if (b<0x27) i8751_return=5;
				else i8751_return=6;
				break;
		}
	}

//	if (!i8751_return) logerror("CPU %04x - Unknown Write %02x intel\n",cpu_get_pc(),data);

	cpu_cause_interrupt(0,6); /* Signal main cpu task is complete */
}

/******************************************************************************/

static WRITE_HANDLER( karnov_control_w )
{
	/* Mnemonics filled in from the schematics, brackets are my comments */
	switch (offset) {
		case 0: /* SECLR (Interrupt ack for Level 6 i8751 interrupt) */
			return;

		case 2: /* SONREQ (Sound CPU byte) */
			soundlatch_w(0,data&0xff);
			cpu_cause_interrupt (1, M6502_INT_NMI);
			break;

		case 4: /* DM (DMA to buffer spriteram) */
			buffer_spriteram_w(0,0);
			break;

		case 6: /* SECREQ (Interrupt & Data to i8751) */
			if (KARNOV) karnov_i8751_w(data);
			if (CHELNOV) chelnov_i8751_w(data);
			if (WNDRPLNT) wndrplnt_i8751_w(data);
			break;

		case 8: /* HSHIFT (9 bits) - Top bit indicates video flip */
			WRITE_WORD (&karnov_scroll[0], data);
			break;

		case 0xa: /* VSHIFT */
			WRITE_WORD (&karnov_scroll[2], data);
			break;

		case 0xc: /* SECR (Reset i8751) */
			//logerror("Reset i8751\n");
			i8751_return=0;
			break;

		case 0xe: /* INTCLR (Interrupt ack for Level 7 vbl interrupt) */
			break;
	}
}

/******************************************************************************/

static READ_HANDLER( karnov_control_r )
{
	switch (offset) {
		case 0: /* Player controls */
			return ( readinputport(0) + (readinputport(1)<<8));
		case 2: /* Start buttons & VBL */
			return readinputport(2);
		case 4: /* Dipswitch A & B */
			return ( readinputport(4) + (readinputport(5)<<8));
		case 6: /* i8751 return values */
			return i8751_return;
	}

	return 0xffff;
}

/******************************************************************************/

static WRITE_HANDLER( videoram_mirror_w ) { COMBINE_WORD_MEM(&videoram[offset],data);}

static struct MemoryReadAddress karnov_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x060000, 0x063fff, MRA_BANK1 },
	{ 0x080000, 0x080fff, MRA_BANK2 },
	{ 0x0a0000, 0x0a07ff, MRA_BANK3 },
	{ 0x0c0000, 0x0c0007, karnov_control_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress karnov_writemem[] =
{
	{ 0x000000, 0x05ffff, MWA_ROM },
	{ 0x060000, 0x063fff, MWA_BANK1 , &karnov_ram },
	{ 0x080000, 0x080fff, MWA_BANK2 , &spriteram, &spriteram_size },
	{ 0x0a0000, 0x0a07ff, MWA_BANK3 , &videoram, &videoram_size },
	{ 0x0a0800, 0x0a0fff, videoram_mirror_w }, /* Wndrplnt only */
	{ 0x0a1000, 0x0a1fff, karnov_foreground_w },
	{ 0x0c0000, 0x0c000f, karnov_control_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

static struct MemoryReadAddress karnov_s_readmem[] =
{
	{ 0x0000, 0x05ff, MRA_RAM},
	{ 0x0800, 0x0800, soundlatch_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress karnov_s_writemem[] =
{
 	{ 0x0000, 0x05ff, MWA_RAM},
	{ 0x1000, 0x1000, YM2203_control_port_0_w }, /* OPN */
	{ 0x1001, 0x1001, YM2203_write_port_0_w },
	{ 0x1800, 0x1800, YM3526_control_port_0_w }, /* OPL */
	{ 0x1801, 0x1801, YM3526_write_port_0_w },
 	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( karnov )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED ) /* Button 4 on schematics */

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED ) /* Button 4 on schematics */

	PORT_START	/* start buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED ) /* PL1 Button 5 on schematics */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED ) /* PL2 Button 5 on schematics */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dummy input for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
/* 0x80 called No Die Mode according to the manual, but it doesn't seem
to have any effect */

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX(0,        0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, "K needed for Bonus Life" )
	PORT_DIPSETTING(    0x0c, "50" )
	PORT_DIPSETTING(    0x08, "70" )
	PORT_DIPSETTING(    0x04, "90" )
	PORT_DIPSETTING(    0x00, "100" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Easy" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Timer Speed" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x00, "Fast" )
INPUT_PORTS_END

INPUT_PORTS_START( chelnov )
	PORT_START	/* Player controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* start buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )   /* Active_low is strange! */

	PORT_START	/* Dummy input for i8751 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX(0,        0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Freeze" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout chars =
{
	8,8,
	1024,
	3,
	{ 0x6000*8,0x4000*8,0x2000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout sprites =
{
	16,16,
	4096,
	4,
 	{ 0x60000*8,0x00000*8,0x20000*8,0x40000*8 },
	{ 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8),
  	0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	16*16
};


/* 16x16 tiles, 4 Planes, each plane is 0x10000 bytes */
static struct GfxLayout tiles =
{
	16,16,
	2048,
	4,
 	{ 0x30000*8,0x00000*8,0x10000*8,0x20000*8 },
	{ 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8),
  	0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	16*16
};

static struct GfxDecodeInfo karnov_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars,     0,  4 },	/* colors 0-31 */
	{ REGION_GFX2, 0, &tiles,   512, 16 },	/* colors 512-767 */
	{ REGION_GFX3, 0, &sprites, 256, 16 },	/* colors 256-511 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static int karnov_interrupt(void)
{
	static int latch;

	/* Coin input to the i8751 generates an interrupt to the main cpu */
	if (readinputport(3) == 0xff) latch=1;
	if (readinputport(3) != 0xff && latch) {
		i8751_return=readinputport(3) | 0x8000;
		cpu_cause_interrupt(0,6);
		latch=0;
	}

	return 7;	/* VBL */
}

static void sound_irq(int linestate)
{
	cpu_set_irq_line(1,0,linestate); /* IRQ */
}

static struct YM2203interface ym2203_interface =
{
	1,
	1500000,	/* Accurate */
	{ YM2203_VOL(20,20) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3526interface ym3526_interface =
{
	1,			/* 1 chip */
	3000000,	/* Accurate */
	{ 60 },	/*  */
	{ sound_irq },
};

/******************************************************************************/

static void karnov_reset_init(void)
{
	memset(karnov_ram,0,0x4000); /* Chelnov likes ram clear on reset.. */
}

static struct MachineDriver machine_driver_karnov =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,	/* 10 Mhz */
			karnov_readmem,karnov_writemem,0,0,
			karnov_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,	/* Accurate */
			karnov_s_readmem,karnov_s_writemem,0,0,
			ignore_interrupt,0	/* Interrupts from OPL chip */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION*2,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	karnov_reset_init,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	karnov_gfxdecodeinfo,
	1024, 1024,
	karnov_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM,
	0,
	karnov_vh_start,
	karnov_vh_stop,
	karnov_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&ym3526_interface
		}
	}
};

static struct MachineDriver machine_driver_wndrplnt =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,	/* 10 Mhz */
			karnov_readmem,karnov_writemem,0,0,
			karnov_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,	/* Accurate */
			karnov_s_readmem,karnov_s_writemem,0,0,
			ignore_interrupt,0	/* Interrupts from OPL chip */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION*2,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	karnov_reset_init,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	karnov_gfxdecodeinfo,
	1024, 1024,
	karnov_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM,
	0,
	karnov_vh_start,
	karnov_vh_stop,
	wndrplnt_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&ym3526_interface
		}
	}
};

/******************************************************************************/

ROM_START( karnov )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "dn08-5",       0x00000, 0x10000, 0xdb92c264 )
	ROM_LOAD_ODD ( "dn11-5",       0x00000, 0x10000, 0x05669b4b )
	ROM_LOAD_EVEN( "dn07-",        0x20000, 0x10000, 0xfc14291b )
	ROM_LOAD_ODD ( "dn10-",        0x20000, 0x10000, 0xa4a34e37 )
	ROM_LOAD_EVEN( "dn06-5",       0x40000, 0x10000, 0x29d64e42 )
	ROM_LOAD_ODD ( "dn09-5",       0x40000, 0x10000, 0x072d7c49 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 Sound CPU */
	ROM_LOAD( "dn05-5",       0x8000, 0x8000, 0xfa1a31a8 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dn00-",        0x00000, 0x08000, 0x0ed77c6d )	/* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dn04-",        0x00000, 0x10000, 0xa9121653 )	/* Backgrounds */
	ROM_LOAD( "dn01-",        0x10000, 0x10000, 0x18697c9e )
	ROM_LOAD( "dn03-",        0x20000, 0x10000, 0x90d9dd9c )
	ROM_LOAD( "dn02-",        0x30000, 0x10000, 0x1e04d7b9 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dn12-",        0x00000, 0x10000, 0x9806772c )	/* Sprites - 2 sets of 4, interleaved here */
	ROM_LOAD( "dn14-5",       0x10000, 0x08000, 0xac9e6732 )
	ROM_LOAD( "dn13-",        0x20000, 0x10000, 0xa03308f9 )
	ROM_LOAD( "dn15-5",       0x30000, 0x08000, 0x8933fcb8 )
	ROM_LOAD( "dn16-",        0x40000, 0x10000, 0x55e63a11 )
	ROM_LOAD( "dn17-5",       0x50000, 0x08000, 0xb70ae950 )
	ROM_LOAD( "dn18-",        0x60000, 0x10000, 0x2ad53213 )
	ROM_LOAD( "dn19-5",       0x70000, 0x08000, 0x8fd4fa40 )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "karnprom.21",  0x0000, 0x0400, 0xaab0bb93 )
	ROM_LOAD( "karnprom.20",  0x0400, 0x0400, 0x02f78ffb )
ROM_END

ROM_START( karnovj )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "kar8",         0x00000, 0x10000, 0x3e17e268 )
	ROM_LOAD_ODD ( "kar11",        0x00000, 0x10000, 0x417c936d )
	ROM_LOAD_EVEN( "dn07-",        0x20000, 0x10000, 0xfc14291b )
	ROM_LOAD_ODD ( "dn10-",        0x20000, 0x10000, 0xa4a34e37 )
	ROM_LOAD_EVEN( "kar6",         0x40000, 0x10000, 0xc641e195 )
	ROM_LOAD_ODD ( "kar9",         0x40000, 0x10000, 0xd420658d )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 Sound CPU */
	ROM_LOAD( "kar5",         0x8000, 0x8000, 0x7c9158f1 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dn00-",        0x00000, 0x08000, 0x0ed77c6d )	/* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dn04-",        0x00000, 0x10000, 0xa9121653 )	/* Backgrounds */
	ROM_LOAD( "dn01-",        0x10000, 0x10000, 0x18697c9e )
	ROM_LOAD( "dn03-",        0x20000, 0x10000, 0x90d9dd9c )
	ROM_LOAD( "dn02-",        0x30000, 0x10000, 0x1e04d7b9 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dn12-",        0x00000, 0x10000, 0x9806772c )	/* Sprites - 2 sets of 4, interleaved here */
	ROM_LOAD( "kar14",        0x10000, 0x08000, 0xc6b39595 )
	ROM_LOAD( "dn13-",        0x20000, 0x10000, 0xa03308f9 )
	ROM_LOAD( "kar15",        0x30000, 0x08000, 0x2f72cac0 )
	ROM_LOAD( "dn16-",        0x40000, 0x10000, 0x55e63a11 )
	ROM_LOAD( "kar17",        0x50000, 0x08000, 0x7851c70f )
	ROM_LOAD( "dn18-",        0x60000, 0x10000, 0x2ad53213 )
	ROM_LOAD( "kar19",        0x70000, 0x08000, 0x7bc174bb )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "karnprom.21",  0x0000, 0x0400, 0xaab0bb93 )
	ROM_LOAD( "karnprom.20",  0x0400, 0x0400, 0x02f78ffb )
ROM_END

ROM_START( wndrplnt )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "ea08.bin",   0x00000, 0x10000, 0xb0578a14 )
	ROM_LOAD_ODD ( "ea11.bin",   0x00000, 0x10000, 0x271edc6c )
	ROM_LOAD_EVEN( "ea07.bin",   0x20000, 0x10000, 0x7095a7d5 )
	ROM_LOAD_ODD ( "ea10.bin",   0x20000, 0x10000, 0x81a96475 )
	ROM_LOAD_EVEN( "ea06.bin",   0x40000, 0x10000, 0x5951add3 )
	ROM_LOAD_ODD ( "ea09.bin",   0x40000, 0x10000, 0xc4b3cb1e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 6502 Sound CPU */
	ROM_LOAD( "ea05.bin",     0x8000, 0x8000, 0x8dbb6231 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ea00.bin",    0x00000, 0x08000, 0x9f3cac4c )	/* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ea04.bin",    0x00000, 0x10000, 0x7d701344 )	/* Backgrounds */
	ROM_LOAD( "ea01.bin",    0x10000, 0x10000, 0x18df55fb )
	ROM_LOAD( "ea03.bin",    0x20000, 0x10000, 0x922ef050 )
	ROM_LOAD( "ea02.bin",    0x30000, 0x10000, 0x700fde70 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ea12.bin",    0x00000, 0x10000, 0xa6d4e99d )	/* Sprites - 2 sets of 4, interleaved here */
	ROM_LOAD( "ea14.bin",    0x10000, 0x10000, 0x915ffdc9 )
	ROM_LOAD( "ea13.bin",    0x20000, 0x10000, 0xcd839f3a )
	ROM_LOAD( "ea15.bin",    0x30000, 0x10000, 0xa1f14f16 )
	ROM_LOAD( "ea16.bin",    0x40000, 0x10000, 0x7a1d8a9c )
	ROM_LOAD( "ea17.bin",    0x50000, 0x10000, 0x21a3223d )
	ROM_LOAD( "ea18.bin",    0x60000, 0x10000, 0x3fb2cec7 )
	ROM_LOAD( "ea19.bin",    0x70000, 0x10000, 0x87cf03b5 )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "ea21.prm",      0x0000, 0x0400, 0xc8beab49 )
	ROM_LOAD( "ea20.prm",      0x0400, 0x0400, 0x619f9d1e )
ROM_END

ROM_START( chelnov )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "ee08-a.j15",   0x00000, 0x10000, 0x2f2fb37b )
	ROM_LOAD_ODD ( "ee11-a.j20",   0x00000, 0x10000, 0xf306d05f )
	ROM_LOAD_EVEN( "ee07-a.j14",   0x20000, 0x10000, 0x9c69ed56 )
	ROM_LOAD_ODD ( "ee10-a.j18",   0x20000, 0x10000, 0xd5c5fe4b )
	ROM_LOAD_EVEN( "ee06-e.j13",   0x40000, 0x10000, 0x55acafdb )
	ROM_LOAD_ODD ( "ee09-e.j17",   0x40000, 0x10000, 0x303e252c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 6502 Sound CPU */
	ROM_LOAD( "ee05-.f3",     0x8000, 0x8000, 0x6a8936b4 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ee00-e.c5",    0x00000, 0x08000, 0xe06e5c6b )	/* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ee04-.d18",    0x00000, 0x10000, 0x96884f95 )	/* Backgrounds */
	ROM_LOAD( "ee01-.c15",    0x10000, 0x10000, 0xf4b54057 )
	ROM_LOAD( "ee03-.d15",    0x20000, 0x10000, 0x7178e182 )
	ROM_LOAD( "ee02-.c18",    0x30000, 0x10000, 0x9d7c45ae )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ee12-.f8",     0x00000, 0x10000, 0x9b1c53a5 )	/* Sprites */
	ROM_LOAD( "ee13-.f9",     0x20000, 0x10000, 0x72b8ae3e )
	ROM_LOAD( "ee14-.f13",    0x40000, 0x10000, 0xd8f4bbde )
	ROM_LOAD( "ee15-.f15",    0x60000, 0x10000, 0x81e3e68b )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "ee21.k8",      0x0000, 0x0400, 0xb1db6586 )	/* different from the other set; */
															/* might be bad */
	ROM_LOAD( "ee20.l6",      0x0400, 0x0400, 0x41816132 )
ROM_END

ROM_START( chelnovj )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "a-j15.bin",    0x00000, 0x10000, 0x1978cb52 )
	ROM_LOAD_ODD ( "a-j20.bin",    0x00000, 0x10000, 0xe0ed3d99 )
	ROM_LOAD_EVEN( "a-j14.bin",    0x20000, 0x10000, 0x51465486 )
	ROM_LOAD_ODD ( "a-j18.bin",    0x20000, 0x10000, 0xd09dda33 )
	ROM_LOAD_EVEN( "a-j13.bin",    0x40000, 0x10000, 0xcd991507 )
	ROM_LOAD_ODD ( "a-j17.bin",    0x40000, 0x10000, 0x977f601c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 6502 Sound CPU */
	ROM_LOAD( "ee05-.f3",     0x8000, 0x8000, 0x6a8936b4 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a-c5.bin",     0x00000, 0x08000, 0x1abf2c6d )	/* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ee04-.d18",    0x00000, 0x10000, 0x96884f95 )	/* Backgrounds */
	ROM_LOAD( "ee01-.c15",    0x10000, 0x10000, 0xf4b54057 )
	ROM_LOAD( "ee03-.d15",    0x20000, 0x10000, 0x7178e182 )
	ROM_LOAD( "ee02-.c18",    0x30000, 0x10000, 0x9d7c45ae )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ee12-.f8",     0x00000, 0x10000, 0x9b1c53a5 )	/* Sprites */
	ROM_LOAD( "ee13-.f9",     0x20000, 0x10000, 0x72b8ae3e )
	ROM_LOAD( "ee14-.f13",    0x40000, 0x10000, 0xd8f4bbde )
	ROM_LOAD( "ee15-.f15",    0x60000, 0x10000, 0x81e3e68b )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "a-k7.bin",     0x0000, 0x0400, 0x309c49d8 )	/* different from the other set; */
															/* might be bad */
	ROM_LOAD( "ee20.l6",      0x0400, 0x0400, 0x41816132 )
ROM_END

/******************************************************************************/

static READ_HANDLER( karnov_cycle_r )
{
	if (cpu_get_pc()==0x8f2 && (READ_WORD(&karnov_ram[0])&0xff00)!=0) {cpu_spinuntil_int(); return 0;} return READ_WORD(&karnov_ram[0]);
}

static READ_HANDLER( karnovj_cycle_r )
{
	if (cpu_get_pc()==0x8ec && (READ_WORD(&karnov_ram[0])&0xff00)!=0) {cpu_spinuntil_int(); return 0;} return READ_WORD(&karnov_ram[0]);
}

static READ_HANDLER( chelnov_cycle_r )
{
	if (cpu_get_pc()==0xdfe && (READ_WORD(&karnov_ram[0])&0xff00)!=0) {cpu_spinuntil_int(); return 0;} return READ_WORD(&karnov_ram[0]);
}

static READ_HANDLER( chelnovj_cycle_r )
{
	if (cpu_get_pc()==0xe06 && (READ_WORD(&karnov_ram[0])&0xff00)!=0) {cpu_spinuntil_int(); return 0;} return READ_WORD(&karnov_ram[0]);
}

static void init_karnov(void)
{
	if (!strcmp(Machine->gamedrv->name,"karnov")) {
		install_mem_read_handler(0, 0x60000, 0x60001, karnov_cycle_r);
		KARNOV=1;
		CHELNOV=WNDRPLNT=0;
	}

	if (!strcmp(Machine->gamedrv->name,"karnovj")) {
		install_mem_read_handler(0, 0x60000, 0x60001, karnovj_cycle_r);
		KARNOV=2;
		CHELNOV=WNDRPLNT=0;
	}

	if (!strcmp(Machine->gamedrv->name,"wndrplnt")) {
//		install_mem_read_handler(0, 0x60000, 0x60001, karnovj_cycle_r);
		KARNOV=CHELNOV=0;
		WNDRPLNT=1;
	}

	if (!strcmp(Machine->gamedrv->name,"chelnov")) {
		install_mem_read_handler(0, 0x60000, 0x60001, chelnov_cycle_r);
		KARNOV=WNDRPLNT=0;
		CHELNOV=1;
	}

	if (!strcmp(Machine->gamedrv->name,"chelnovj")) {
		install_mem_read_handler(0, 0x60000, 0x60001, chelnovj_cycle_r);
		KARNOV=WNDRPLNT=0;
		CHELNOV=2;
	}
}

static void init_wndrplnt(void)
{
//	unsigned char *RAM = memory_region(REGION_CPU1);

	init_karnov();

//	WRITE_WORD (&RAM[0x1106],0x4E71);
//	WRITE_WORD (&RAM[0x110e],0x4E71);
//	WRITE_WORD (&RAM[0xc0c],0x4E71);
//	WRITE_WORD (&RAM[0xc0e],0x4E71);
//	WRITE_WORD (&RAM[0xc4c],0x4E71);
//	WRITE_WORD (&RAM[0xc0e],0x4E71);
//WRITE_WORD (&RAM[0x5b0a],0x4E71);
//WRITE_WORD (&RAM[0x5b0c],0x4E71);
//WRITE_WORD (&RAM[0x5b0e],0x4E71);
//WRITE_WORD (&RAM[0x5b1e],0x4E71);
//WRITE_WORD (&RAM[0xd58],0x4E71);
}

static void init_chelnov(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	init_karnov();

	WRITE_WORD (&RAM[0x0A26],0x4E71);  /* removes a protection lookup table */
	WRITE_WORD (&RAM[0x062a],0x4E71);  /* hangs waiting on i8751 int */
}

static void init_chelnovj(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	init_karnov();

	WRITE_WORD (&RAM[0x0A2E],0x4E71);  /* removes a protection lookup table */
	WRITE_WORD (&RAM[0x062a],0x4E71);  /* hangs waiting on i8751 int */
}

/******************************************************************************/

GAMEX( 1987, karnov,   0,       karnov,   karnov,  karnov,   ROT0,   "Data East USA", "Karnov (US)", GAME_NO_COCKTAIL )
GAMEX( 1987, karnovj,  karnov,  karnov,   karnov,  karnov,   ROT0,   "Data East Corporation", "Karnov (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1987, wndrplnt, 0,       wndrplnt, karnov,  wndrplnt, ROT270, "Data East Corporation", "Wonder Planet (Japan)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1988, chelnov,  0,       karnov,   chelnov, chelnov,  ROT0,   "Data East USA", "Chelnov - Atomic Runner (US)", GAME_NO_COCKTAIL )
GAMEX( 1988, chelnovj, chelnov, karnov,   chelnov, chelnovj, ROT0,   "Data East Corporation", "Chelnov - Atomic Runner (Japan)", GAME_NO_COCKTAIL )
