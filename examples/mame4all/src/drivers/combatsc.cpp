#include "../vidhrdw/combatsc.cpp"

/***************************************************************************

"Combat School" (also known as "Boot Camp") - (Konami GX611)

TODO:
- in combasc (and more generally the 007121) the number of sprites can be
  increased from 0x40 to 0x80. There is a hack in konamiic.c to handle that,
  but it is wrong. If you don't pass the Iron Man stage, a few sprites are
  left dangling on the screen.
- it seems that to get correct target colors in firing range III we have to
  use the WRONG lookup table (the one for tiles instead of the one for
  sprites).
- in combascb, wrong sprite/char priority (see cpu head at beginning of arm
  wrestling, and heads in intermission after firing range III)
- hook up sound in bootleg (the current sound is a hack, making use of the
  Konami ROMset)
- understand how the trackball really works
- YM2203 pitch is wrong. Fixing it screws up the tempo.

Credits:

	Hardware Info:
		Jose Tejada Gomez
		Manuel Abadia
		Cesareo Gutierrez

	MAME Driver:
		Phil Stroffolino
		Manuel Abadia

Memory Maps (preliminary):

***************************
* Combat School (bootleg) *
***************************

MAIN CPU:
---------
00c0-00c3	Objects control
0500		bankswitch control
0600-06ff	palette
0800-1fff	RAM
2000-2fff	Video RAM (banked)
3000-3fff	Object RAM (banked)
4000-7fff	Banked Area + IO + Video Registers
8000-ffff	ROM

SOUND CPU:
----------
0000-8000	ROM
8000-87ef	RAM
87f0-87ff	???
9000-9001	YM2203
9008		???
9800		OKIM5205?
a000		soundlatch?
a800		OKIM5205?
fffc-ffff	???


		Notes about the sound systsem of the bootleg:
        ---------------------------------------------
        The positions 0x87f0-0x87ff are very important, it
        does work similar to a semaphore (same as a lot of
        vblank bits). For example in the init code, it writes
        zero to 0x87fa, then it waits to it 'll be different
        to zero, but it isn't written by this cpu. (shareram?)
        I have tried put here a K007232 chip, but it didn't
        work.

		Sound chips: OKI M5205 & YM2203

		We are using the other sound hardware for now.

****************************
* Combat School (Original) *
****************************

0000-005f	Video Registers (banked)
0400-0407	input ports
0408		coin counters
0410		bankswitch control
0600-06ff	palette
0800-1fff	RAM
2000-2fff	Video RAM (banked)
3000-3fff	Object RAM (banked)
4000-7fff	Banked Area + IO + Video Registers
8000-ffff	ROM

SOUND CPU:
----------
0000-8000	ROM
8000-87ff	RAM
9000		uPD7759
b000		uPD7759
c000		uPD7759
d000		soundlatch_r
e000-e001	YM2203

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"

extern unsigned char* banked_area;

/* from vidhrdw/combasc.c */
void combasc_convert_color_prom( unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom );
void combascb_convert_color_prom( unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom );
READ_HANDLER( combasc_video_r );
WRITE_HANDLER( combasc_video_w );
int combasc_vh_start( void );
int combascb_vh_start( void );
void combasc_vh_stop( void );

WRITE_HANDLER( combascb_bankselect_w );
WRITE_HANDLER( combasc_bankselect_w );
void combasc_init_machine( void );
WRITE_HANDLER( combasc_pf_control_w );
READ_HANDLER( combasc_scrollram_r );
WRITE_HANDLER( combasc_scrollram_w );

void combascb_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh );
void combasc_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh );
WRITE_HANDLER( combasc_io_w );
WRITE_HANDLER( combasc_vreg_w );




static WRITE_HANDLER( combasc_coin_counter_w )
{
	/* b7-b3: unused? */
	/* b1: coin counter 2 */
	/* b0: coin counter 1 */

	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);
}

static READ_HANDLER( trackball_r )
{
	static UINT8 pos[4],sign[4];

	if (offset == 0)
	{
		int i,dir[4];

		for (i = 0;i < 4;i++)
		{
			UINT8 curr;

			curr = readinputport(4 + i);

			dir[i] = curr - pos[i];
			sign[i] = dir[i] & 0x80;
			pos[i] = curr;
		}

		/* fix sign for orthogonal movements */
		if (dir[0] || dir[1])
		{
			if (!dir[0]) sign[0] = sign[1] ^ 0x80;
			if (!dir[1]) sign[1] = sign[0];
		}
		if (dir[2] || dir[3])
		{
			if (!dir[2]) sign[2] = sign[3] ^ 0x80;
			if (!dir[3]) sign[3] = sign[2];
		}
	}

	return sign[offset] | (pos[offset] & 0x7f);
}


/* the protection is a simple multiply */
static int prot[2];

static WRITE_HANDLER( protection_w )
{
	prot[offset] = data;
}
static READ_HANDLER( protection_r )
{
	return ((prot[0] * prot[1]) >> (offset * 8)) & 0xff;
}
static WRITE_HANDLER( protection_clock_w )
{
	/* 0x3f is written here every time before accessing the other registers */
}


/****************************************************************************/

static WRITE_HANDLER( combasc_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,0xff);
}

static WRITE_HANDLER( combasc_play_w )
{
	if (data & 0x02)
        UPD7759_start_w(0, 0);
}

static WRITE_HANDLER( combasc_voice_reset_w )
{
    UPD7759_reset_w(0,data & 1);
}

static WRITE_HANDLER( combasc_portA_w )
{
	/* unknown. always write 0 */
}

/****************************************************************************/

static struct MemoryReadAddress combasc_readmem[] =
{
	{ 0x0020, 0x005f, combasc_scrollram_r },
	{ 0x0200, 0x0201, protection_r },
	{ 0x0400, 0x0400, input_port_0_r },
	{ 0x0401, 0x0401, input_port_1_r },			/* DSW #3 */
	{ 0x0402, 0x0402, input_port_2_r },			/* DSW #1 */
	{ 0x0403, 0x0403, input_port_3_r },			/* DSW #2 */
	{ 0x0404, 0x0407, trackball_r },			/* 1P & 2P controls / trackball */
	{ 0x0600, 0x06ff, MRA_RAM },				/* palette */
	{ 0x0800, 0x1fff, MRA_RAM },
	{ 0x2000, 0x3fff, combasc_video_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },				/* banked ROM area */
	{ 0x8000, 0xffff, MRA_ROM },				/* ROM */
	{ -1 }
};

static struct MemoryWriteAddress combasc_writemem[] =
{
	{ 0x0000, 0x0007, combasc_pf_control_w },
	{ 0x0020, 0x005f, combasc_scrollram_w },
//	{ 0x0060, 0x00ff, MWA_RAM },					/* RAM */
	{ 0x0200, 0x0201, protection_w },
	{ 0x0206, 0x0206, protection_clock_w },
	{ 0x0408, 0x0408, combasc_coin_counter_w },	/* coin counters */
	{ 0x040c, 0x040c, combasc_vreg_w },
	{ 0x0410, 0x0410, combasc_bankselect_w },
	{ 0x0414, 0x0414, soundlatch_w },
	{ 0x0418, 0x0418, combasc_sh_irqtrigger_w },
	{ 0x041c, 0x041c, watchdog_reset_w },			/* watchdog reset? */
	{ 0x0600, 0x06ff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0x0800, 0x1fff, MWA_RAM },					/* RAM */
	{ 0x2000, 0x3fff, combasc_video_w },
	{ 0x4000, 0x7fff, MWA_ROM },					/* banked ROM area */
	{ 0x8000, 0xffff, MWA_ROM },					/* ROM */
	{ -1 }
};

static struct MemoryReadAddress combascb_readmem[] =
{
	{ 0x0000, 0x04ff, MRA_RAM },
	{ 0x0600, 0x06ff, MRA_RAM },	/* palette */
	{ 0x0800, 0x1fff, MRA_RAM },
	{ 0x2000, 0x3fff, combasc_video_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },				/* banked ROM/RAM area */
	{ 0x8000, 0xffff, MRA_ROM },				/* ROM */
	{ -1 }
};

static struct MemoryWriteAddress combascb_writemem[] =
{
	{ 0x0000, 0x04ff, MWA_RAM },
	{ 0x0500, 0x0500, combascb_bankselect_w },
	{ 0x0600, 0x06ff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0x0800, 0x1fff, MWA_RAM },
	{ 0x2000, 0x3fff, combasc_video_w },
	{ 0x4000, 0x7fff, MWA_BANK1, &banked_area },/* banked ROM/RAM area */
	{ 0x8000, 0xffff, MWA_ROM },				/* ROM */
	{ -1 }
};

#if 0
static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },					/* ROM */
	{ 0x8000, 0x87ef, MRA_RAM },					/* RAM */
	{ 0x87f0, 0x87ff, MRA_RAM },					/* ??? */
	{ 0x9000, 0x9000, YM2203_status_port_0_r },		/* YM 2203 */
	{ 0x9008, 0x9008, YM2203_status_port_0_r },		/* ??? */
	{ 0xa000, 0xa000, soundlatch_r },				/* soundlatch_r? */
	{ 0x8800, 0xfffb, MRA_ROM },					/* ROM? */
	{ 0xfffc, 0xffff, MRA_RAM },					/* ??? */
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },				/* ROM */
	{ 0x8000, 0x87ef, MWA_RAM },				/* RAM */
	{ 0x87f0, 0x87ff, MWA_RAM },				/* ??? */
 	{ 0x9000, 0x9000, YM2203_control_port_0_w },/* YM 2203 */
	{ 0x9001, 0x9001, YM2203_write_port_0_w },	/* YM 2203 */
	//{ 0x9800, 0x9800, combasc_unknown_w_1 },	/* OKIM5205? */
	//{ 0xa800, 0xa800, combasc_unknown_w_2 },	/* OKIM5205? */
	{ 0x8800, 0xfffb, MWA_ROM },				/* ROM */
	{ 0xfffc, 0xffff, MWA_RAM },				/* ??? */
	{ -1 }
};
#endif

static struct MemoryReadAddress combasc_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },					/* ROM */
	{ 0x8000, 0x87ff, MRA_RAM },					/* RAM */
	{ 0xb000, 0xb000, UPD7759_0_busy_r },			/* UPD7759 busy? */
	{ 0xd000, 0xd000, soundlatch_r },				/* soundlatch_r? */
    { 0xe000, 0xe000, YM2203_status_port_0_r },		/* YM 2203 */
	{ -1 }
};

static struct MemoryWriteAddress combasc_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },				/* ROM */
	{ 0x8000, 0x87ff, MWA_RAM },				/* RAM */
	{ 0x9000, 0x9000, combasc_play_w },			/* uPD7759 play voice */
	{ 0xa000, 0xa000, UPD7759_0_message_w },	/* uPD7759 voice select */
	{ 0xc000, 0xc000, combasc_voice_reset_w },	/* uPD7759 reset? */
 	{ 0xe000, 0xe000, YM2203_control_port_0_w },/* YM 2203 */
	{ 0xe001, 0xe001, YM2203_write_port_0_w },	/* YM 2203 */
	{ -1 }
};


#define COINAGE \
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) ) \
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) ) \
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) ) \
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) ) \
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) ) \
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) ) \
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) ) \
	PORT_DIPSETTING(    0x00, "coin 2 invalidity" )

INPUT_PORTS_START( combasc )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW #3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On )  )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	/* DSW # 1 */
	COINAGE

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING( 0x60, "Easy" )
	PORT_DIPSETTING( 0x40, "Normal" )
	PORT_DIPSETTING( 0x20, "Difficult" )
	PORT_DIPSETTING( 0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING( 0x80, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )

	PORT_START	/* only used in trackball version */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* only used in trackball version */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* only used in trackball version */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( combasct )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW #3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On )  )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	/* DSW # 1 */
	COINAGE

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING( 0x60, "Easy" )
	PORT_DIPSETTING( 0x40, "Normal" )
	PORT_DIPSETTING( 0x20, "Difficult" )
	PORT_DIPSETTING( 0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING( 0x80, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )

	/* trackball 1P */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1 | IPF_REVERSE, 10, 10, 0, 0 )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1, 10, 10, 0, 0 )

	/* trackball 2P (not implemented yet) */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER2 | IPF_REVERSE, 10, 10, 0, 0 )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 10, 10, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( combascb )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )

	PORT_START
	COINAGE

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_DIPNAME( 0x10, 0x00, "Allow Continue" )
	PORT_DIPSETTING( 0x10, DEF_STR( No ) )
	PORT_DIPSETTING( 0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING( 0x60, "Easy" )
	PORT_DIPSETTING( 0x40, "Normal" )
	PORT_DIPSETTING( 0x20, "Difficult" )
	PORT_DIPSETTING( 0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING( 0x80, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout gfx_layout =
{
	8,8,
	0x4000,
	4,
	{ 0,1,2,3 },
	{ 0, 4, 8, 12, 16, 20, 24, 28},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout tile_layout =
{
	8,8,
	0x2000, /* number of tiles */
	4,		/* bitplanes */
	{ 0*0x10000*8, 1*0x10000*8, 2*0x10000*8, 3*0x10000*8 }, /* plane offsets */
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	8*8
};

static struct GfxLayout sprite_layout =
{
	16,16,
	0x800,	/* number of sprites */
	4,		/* bitplanes */
	{ 3*0x10000*8, 2*0x10000*8, 1*0x10000*8, 0*0x10000*8 }, /* plane offsets */
	{
		0,1,2,3,4,5,6,7,
		16*8+0,16*8+1,16*8+2,16*8+3,16*8+4,16*8+5,16*8+6,16*8+7
	},
	{
		0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
		8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8
	},
	8*8*4
};

static struct GfxDecodeInfo combasc_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &gfx_layout, 0, 8*16 },
	{ REGION_GFX2, 0x00000, &gfx_layout, 0, 8*16 },
	{ -1 }
};

static struct GfxDecodeInfo combascb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &tile_layout,   0, 8*16 },
	{ REGION_GFX1, 0x40000, &tile_layout,   0, 8*16 },
	{ REGION_GFX2, 0x00000, &sprite_layout, 0, 8*16 },
	{ REGION_GFX2, 0x40000, &sprite_layout, 0, 8*16 },
	{ -1 }
};

static struct YM2203interface ym2203_interface =
{
	1,							/* 1 chip */
	3500000,					/* this is wrong but gives the correct music tempo. */
	/* the correct value is 20MHz/8=2.5MHz, which gives correct pitch but wrong tempo */
	{ YM2203_VOL(20,20) },
	{ 0 },
	{ 0 },
	{ combasc_portA_w },
	{ 0 }
};

static struct UPD7759_interface upd7759_interface =
{
	1,							/* number of chips */
	UPD7759_STANDARD_CLOCK,
	{ 70 },						/* volume */
	{ REGION_SOUND1 },			/* memory region */
	UPD7759_STANDALONE_MODE,	/* chip mode */
	{0}
};



/* combat school (original) */
static struct MachineDriver machine_driver_combasc =
{
	{
		{
			CPU_HD6309,
			3000000,	/* 3 MHz? */
			combasc_readmem,combasc_writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			1500000,	/* 1.5 MHz? */
			combasc_readmem_sound,combasc_writemem_sound,0,0,
			ignore_interrupt,1 	/* IRQs are caused by the main CPU */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10, /* CPU slices */
	combasc_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	combasc_gfxdecodeinfo,
	128,8*16*16,
	combasc_convert_color_prom,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	combasc_vh_start,
	combasc_vh_stop,
	combasc_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_UPD7759,
			&upd7759_interface
		}
	}
};

/* combat school (bootleg on different hardware) */
static struct MachineDriver machine_driver_combascb =
{
	{
		{
			CPU_HD6309,
			3000000,	/* 3 MHz? */
			combascb_readmem,combascb_writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			1500000,
			combasc_readmem_sound,combasc_writemem_sound,0,0, /* FAKE */
			ignore_interrupt,0 	/* IRQs are caused by the main CPU */
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10, /* CPU slices */
	combasc_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	combascb_gfxdecodeinfo,
	128,8*16*16,
	combascb_convert_color_prom,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	combascb_vh_start,
	combasc_vh_stop,
	combascb_vh_screenrefresh,

	/* We are using the original sound subsystem */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_UPD7759,
			&upd7759_interface
		}
	}
};



ROM_START( combasc )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 6309 code */
	ROM_LOAD( "611g01.rom", 0x30000, 0x08000, 0x857ffffe )
	ROM_CONTINUE(           0x08000, 0x08000 )
	ROM_LOAD( "611g02.rom", 0x10000, 0x20000, 0x9ba05327 )
	/* extra 0x8000 for banked RAM */

	ROM_REGION( 0x10000 , REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "611g03.rom", 0x00000, 0x08000, 0x2a544db5 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g07.rom",    0x00000, 0x40000, 0x73b38720 )
	ROM_LOAD_GFX_ODD ( "611g08.rom",    0x00000, 0x40000, 0x46e7d28c )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g11.rom",    0x00000, 0x40000, 0x69687538 )
	ROM_LOAD_GFX_ODD ( "611g12.rom",    0x00000, 0x40000, 0x9c6bf898 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "611g06.h14",  0x0000, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g05.h15",  0x0100, 0x0100, 0x207a7b07 ) /* chars lookup table */
	ROM_LOAD( "611g10.h6",   0x0200, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g09.h7",   0x0300, 0x0100, 0x207a7b07 ) /* chars lookup table */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* uPD7759 data */
	ROM_LOAD( "611g04.rom",  0x00000, 0x20000, 0x2987e158 )
ROM_END

ROM_START( combasct )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 6309 code */
	ROM_LOAD( "g01.rom",     0x30000, 0x08000, 0x489c132f )
	ROM_CONTINUE(            0x08000, 0x08000 )
	ROM_LOAD( "611g02.rom",  0x10000, 0x20000, 0x9ba05327 )
	/* extra 0x8000 for banked RAM */

	ROM_REGION( 0x10000 , REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "611g03.rom", 0x00000, 0x08000, 0x2a544db5 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g07.rom",    0x00000, 0x40000, 0x73b38720 )
	ROM_LOAD_GFX_ODD ( "611g08.rom",    0x00000, 0x40000, 0x46e7d28c )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g11.rom",    0x00000, 0x40000, 0x69687538 )
	ROM_LOAD_GFX_ODD ( "611g12.rom",    0x00000, 0x40000, 0x9c6bf898 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "611g06.h14",  0x0000, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g05.h15",  0x0100, 0x0100, 0x207a7b07 ) /* chars lookup table */
	ROM_LOAD( "611g10.h6",   0x0200, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g09.h7",   0x0300, 0x0100, 0x207a7b07 ) /* chars lookup table */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* uPD7759 data */
	ROM_LOAD( "611g04.rom",  0x00000, 0x20000, 0x2987e158 )
ROM_END

ROM_START( combascj )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 6309 code */
	ROM_LOAD( "611p01.a14",  0x30000, 0x08000, 0xd748268e )
	ROM_CONTINUE(            0x08000, 0x08000 )
	ROM_LOAD( "611g02.rom",  0x10000, 0x20000, 0x9ba05327 )
	/* extra 0x8000 for banked RAM */

	ROM_REGION( 0x10000 , REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "611g03.rom", 0x00000, 0x08000, 0x2a544db5 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g07.rom",    0x00000, 0x40000, 0x73b38720 )
	ROM_LOAD_GFX_ODD ( "611g08.rom",    0x00000, 0x40000, 0x46e7d28c )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g11.rom",    0x00000, 0x40000, 0x69687538 )
	ROM_LOAD_GFX_ODD ( "611g12.rom",    0x00000, 0x40000, 0x9c6bf898 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "611g06.h14",  0x0000, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g05.h15",  0x0100, 0x0100, 0x207a7b07 ) /* chars lookup table */
	ROM_LOAD( "611g10.h6",   0x0200, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g09.h7",   0x0300, 0x0100, 0x207a7b07 ) /* chars lookup table */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* uPD7759 data */
	ROM_LOAD( "611g04.rom",  0x00000, 0x20000, 0x2987e158 )
ROM_END

ROM_START( bootcamp )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 6309 code */
	ROM_LOAD( "xxx-v01.12a", 0x30000, 0x08000, 0xc10dca64 )
	ROM_CONTINUE(            0x08000, 0x08000 )
	ROM_LOAD( "611g02.rom",  0x10000, 0x20000, 0x9ba05327 )
	/* extra 0x8000 for banked RAM */

	ROM_REGION( 0x10000 , REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "611g03.rom", 0x00000, 0x08000, 0x2a544db5 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g07.rom",    0x00000, 0x40000, 0x73b38720 )
	ROM_LOAD_GFX_ODD ( "611g08.rom",    0x00000, 0x40000, 0x46e7d28c )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "611g11.rom",    0x00000, 0x40000, 0x69687538 )
	ROM_LOAD_GFX_ODD ( "611g12.rom",    0x00000, 0x40000, 0x9c6bf898 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "611g06.h14",  0x0000, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g05.h15",  0x0100, 0x0100, 0x207a7b07 ) /* chars lookup table */
	ROM_LOAD( "611g10.h6",   0x0200, 0x0100, 0xf916129a ) /* sprites lookup table */
	ROM_LOAD( "611g09.h7",   0x0300, 0x0100, 0x207a7b07 ) /* chars lookup table */

    ROM_REGION( 0x20000, REGION_SOUND1 )	/* uPD7759 data */
	ROM_LOAD( "611g04.rom",  0x00000, 0x20000, 0x2987e158 )
ROM_END

ROM_START( combascb )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 6809 code */
	ROM_LOAD( "combat.002",	 0x30000, 0x08000, 0x0996755d )
	ROM_CONTINUE(            0x08000, 0x08000 )
	ROM_LOAD( "combat.003",	 0x10000, 0x10000, 0x229c93b2 )
	ROM_LOAD( "combat.004",	 0x20000, 0x10000, 0xa069cb84 )
	/* extra 0x8000 for banked RAM */

	ROM_REGION( 0x10000 , REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "combat.001",  0x00000, 0x10000, 0x61456b3b )
	ROM_LOAD( "611g03.rom",  0x00000, 0x08000, 0x2a544db5 ) /* FAKE - from Konami set! */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "combat.006",  0x00000, 0x10000, 0x8dc29a1f ) /* tiles, bank 0 */
	ROM_LOAD( "combat.008",  0x10000, 0x10000, 0x61599f46 )
	ROM_LOAD( "combat.010",  0x20000, 0x10000, 0xd5cda7cd )
	ROM_LOAD( "combat.012",  0x30000, 0x10000, 0xca0a9f57 )
	ROM_LOAD( "combat.005",  0x40000, 0x10000, 0x0803a223 ) /* tiles, bank 1 */
	ROM_LOAD( "combat.007",  0x50000, 0x10000, 0x23caad0c )
	ROM_LOAD( "combat.009",  0x60000, 0x10000, 0x5ac80383 )
	ROM_LOAD( "combat.011",  0x70000, 0x10000, 0xcda83114 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "combat.013",  0x00000, 0x10000, 0x4bed2293 ) /* sprites, bank 0 */
	ROM_LOAD( "combat.015",  0x10000, 0x10000, 0x26c41f31 )
	ROM_LOAD( "combat.017",  0x20000, 0x10000, 0x6071e6da )
	ROM_LOAD( "combat.019",  0x30000, 0x10000, 0x3b1cf1b8 )
	ROM_LOAD( "combat.014",  0x40000, 0x10000, 0x82ea9555 ) /* sprites, bank 1 */
	ROM_LOAD( "combat.016",  0x50000, 0x10000, 0x2e39bb70 )
	ROM_LOAD( "combat.018",  0x60000, 0x10000, 0x575db729 )
	ROM_LOAD( "combat.020",  0x70000, 0x10000, 0x8d748a1a )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "prom.d10",    0x0000, 0x0100, 0x265f4c97 ) /* sprites lookup table */
	ROM_LOAD( "prom.c11",    0x0100, 0x0100, 0xa7a5c0b4 ) /* priority? */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* uPD7759 data */
	ROM_LOAD( "611g04.rom",  0x00000, 0x20000, 0x2987e158 )	/* FAKE - from Konami set! */
ROM_END



static void init_combasc( void )
{
	/* joystick instead of trackball */
	install_mem_read_handler(0,0x0404,0x0404,input_port_4_r);
}

static void init_combascb( void )
{
	unsigned char *gfx;
	int i;

	gfx = memory_region(REGION_GFX1);
	for (i = 0;i < memory_region_length(REGION_GFX1);i++)
		gfx[i] = ~gfx[i];

	gfx = memory_region(REGION_GFX2);
	for (i = 0;i < memory_region_length(REGION_GFX2);i++)
		gfx[i] = ~gfx[i];
}



GAME( 1988, combasc,  0,       combasc,  combasc,  combasc,  ROT0, "Konami", "Combat School (joystick)" )
GAME( 1987, combasct, combasc, combasc,  combasct, 0,        ROT0, "Konami", "Combat School (trackball)" )
GAME( 1987, combascj, combasc, combasc,  combasct, 0,        ROT0, "Konami", "Combat School (Japan trackball)" )
GAME( 1987, bootcamp, combasc, combasc,  combasct, 0,        ROT0, "Konami", "Boot Camp" )
GAMEX(1988, combascb, combasc, combascb, combascb, combascb, ROT0, "bootleg", "Combat School (bootleg)", GAME_IMPERFECT_COLORS )
