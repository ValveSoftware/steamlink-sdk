#include "../machine/atarifb.cpp"
#include "../vidhrdw/atarifb.cpp"

/***************************************************************************

Atari Football Driver

Driver by Mike Balfour, Patrick Lawrence, Brad Oliver

Memory Map:
	0000-01FF	Working RAM
	0200-025F	Playfield - Player 1
	03A0-03FF	Playfield - Player 2
	1000-13BF	Scrollfield
	13C0-13FF	Motion Object Parameters:

	13C0		Motion Object 1 Picture #
	13C1		Motion Object 1 Vertical Position
	13C2		Motion Object 2 Picture #
	13C3		Motion Object 2 Vertical Position
	...
	13DE		Motion Object 16 Picture #
	13DF		Motion Object 16 Vertical Position

	13E0		Motion Object 1 Horizontal Position
	13E1		Spare
	13E2		Motion Object 2 Horizontal Position
	13E3		Spare
	...
	13FE		Motion Object 16 Horizontal Position
	13FF		Spare

	2000-2003	Output ports:

	2000		(OUT 0) Scrollfield Offset (8 bits)
	2001		(OUT 1)
				D0 = Whistle
				D1 = Hit
				D2 = Kicker
				D5 = CTRLD
	2002		(OUT 2)
				D0-D3 = Noise Amplitude
				D4 = Coin Counter
				D5 = Attract
	2003		(OUT 3)
				D0-D3 = LED Cathodes
				D4-D5 Spare

	3000		Interrupt Acknowledge
	4000-4003	Input ports:

	4000		(IN 0) = 0
				D0 = Trackball Direction PL2VD
				D1 = Trackball Direction PL2HD
				D2 = Trackball Direction PL1VD
				D3 = Trackball Direction PL1HD
				D4 = Select 1
				D5 = Slam
				D6 = End Screen
				D7 = Coin 1
	4000		(CTRLD) = 1
				D0-D3 = Track-ball Horiz. 1
				D4-D7 = Track-ball Vert. 1
	4002		(IN 2) = 0
				D0-D3 = Option Switches
				D4 = Select 2
				D5 = Spare
				D6 = Test
				D7 = Coin 2
	4002		(CTRLD) = 1
				D0-D3 = Track-ball Horiz. 2
				D4-D7 = Track-ball Vert. 2

	5000		Watchdog
	6800-7FFF	Program
	(F800-FFFF) - only needed for the 6502 vectors

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)

Changes:
	LBO - lots of cleanup, now it's playable.

TODO:
	* The down marker sprite is multiplexed so that it will be drawn at the
	  top and bottom of the screen. We fake this feature. Additionally, we
	  draw it at a different location which seems to make more sense.

	* The play which is chosen is drawn in text at the top of the screen;
	  no backdrop/overlay is supported yet. High quality artwork would be
	  appreciated.

	* The sound is ripped for the most part from the Basketball driver and
	  is missing the kick, hit and whistle tones. I don't know if the noise
	  is entirely accurate either.

	* I'm not good at reading the schematics, so I'm unsure about the
	  exact vblank duration. I'm pretty sure it is one of two values though.

	* The 4-player variation is slightly broken. I'm unsure of the
	  LED multiplexing.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* vidhrdw/atarifb.c */
extern size_t atarifb_alphap1_vram_size;
extern size_t atarifb_alphap2_vram_size;
extern unsigned char *atarifb_alphap1_vram;
extern unsigned char *atarifb_alphap2_vram;
extern unsigned char *atarifb_scroll_register;

WRITE_HANDLER( atarifb_scroll_w );
WRITE_HANDLER( atarifb_alphap1_vram_w );
WRITE_HANDLER( atarifb_alphap2_vram_w );
extern int atarifb_vh_start(void);
extern void atarifb_vh_stop(void);
extern void atarifb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* machine/atarifb.c */
WRITE_HANDLER( atarifb_out1_w );
READ_HANDLER( atarifb_in0_r );
READ_HANDLER( atarifb_in2_r );
WRITE_HANDLER( atarifb4_out1_w );
READ_HANDLER( atarifb4_in0_r );
READ_HANDLER( atarifb4_in2_r );
WRITE_HANDLER( soccer_out1_w );

int atarifb_lamp1, atarifb_lamp2;
int atarifb_game;

/* sound code shamelessly ripped from bsktball.c */
static int crowd_mask;
static int noise_b10=0;
static int noise_a10=0;
static int noise=0;
static int noise_timer_set=0;
#define TIME_32H 10582*2
#define TIME_256H TIME_32H*4


static void atarifb_noise_256H(int foo)
{
	int b10_input;
	int a10_input;

	b10_input = (noise_b10 & 0x01) ^ (((~noise_a10) & 0x40) >> 6);
	a10_input = (noise_b10 & 0x80) >> 7;

	noise_b10 = ((noise_b10 << 1) | b10_input) & 0xFF;
	noise_a10 = ((noise_a10 << 1) | a10_input) & 0xFF;

	noise = (noise_a10 & 0x80) >> 7;

	if (noise)
		DAC_data_w(2,crowd_mask);
	else
		DAC_data_w(2,0);

	timer_set (TIME_IN_NSEC(TIME_256H), 0, atarifb_noise_256H);
	noise_timer_set=1;
}

static WRITE_HANDLER( atarifb_out2_w )
{
	/* D0-D3 = crowd */
	crowd_mask = (data & 0x0F) << 4;
	if (noise)
		DAC_data_w(2,crowd_mask);
	else
		DAC_data_w(2,0);

	if (!noise_timer_set)
		timer_set (TIME_IN_NSEC(TIME_256H), 0, atarifb_noise_256H);
	noise_timer_set=1;

	coin_counter_w (0, data & 0x10);
//	logerror("out2_w: %02x\n", data & ~0x0f);
}

static WRITE_HANDLER( soccer_out2_w )
{
	/* D0-D3 = crowd */
	crowd_mask = (data & 0x0F) << 4;
	if (noise)
		DAC_data_w(2,crowd_mask);
	else
		DAC_data_w(2,0);

	if (!noise_timer_set)
		timer_set (TIME_IN_NSEC(TIME_256H), 0, atarifb_noise_256H);
	noise_timer_set=1;

	coin_counter_w (0, data & 0x40);
	coin_counter_w (1, data & 0x20);
	coin_counter_w (2, data & 0x10);
//	logerror("out2_w: %02x\n", data & ~0x0f);
}

static WRITE_HANDLER( atarifb_out3_w )
{
	int loop = cpu_getiloops ();

	switch (loop)
	{
		case 0x00:
			/* Player 1 play select lamp */
			atarifb_lamp1 = data;
			break;
		case 0x01:
			break;
		case 0x02:
			/* Player 2 play select lamp */
			atarifb_lamp2 = data;
			break;
		case 0x03:
			break;
	}
//	logerror("out3_w, %02x:%02x\n", loop, data);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x1000, 0x13bf, MRA_RAM },
	{ 0x13c0, 0x13ff, MRA_RAM },
	{ 0x3000, 0x3000, MRA_RAM },
	{ 0x4000, 0x4000, atarifb_in0_r },
	{ 0x4002, 0x4002, atarifb_in2_r },
	{ 0x6000, 0x7fff, MRA_ROM }, /* PROM */
	{ 0xfff0, 0xffff, MRA_ROM }, /* PROM for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x0200, 0x025f, atarifb_alphap1_vram_w, &atarifb_alphap1_vram, &atarifb_alphap1_vram_size },
	{ 0x0260, 0x039f, MWA_RAM },
	{ 0x03a0, 0x03ff, atarifb_alphap2_vram_w, &atarifb_alphap2_vram, &atarifb_alphap2_vram_size },
	{ 0x1000, 0x13bf, videoram_w, &videoram, &videoram_size },
	{ 0x13c0, 0x13ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x2000, 0x2000, atarifb_scroll_w, &atarifb_scroll_register }, /* OUT 0 */
	{ 0x2001, 0x2001, atarifb4_out1_w }, /* OUT 1 */
	{ 0x2002, 0x2002, atarifb_out2_w }, /* OUT 2 */
	{ 0x2003, 0x2003, atarifb_out3_w }, /* OUT 3 */
	{ 0x3000, 0x3000, MWA_NOP }, /* Interrupt Acknowledge */
	{ 0x5000, 0x5000, watchdog_reset_w },
	{ 0x6000, 0x7fff, MWA_ROM }, /* PROM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress atarifb4_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x1000, 0x13bf, MRA_RAM },
	{ 0x13c0, 0x13ff, MRA_RAM },
	{ 0x3000, 0x3000, MRA_RAM },
	{ 0x4000, 0x4000, atarifb4_in0_r },
	{ 0x4001, 0x4001, input_port_1_r },
	{ 0x4002, 0x4002, atarifb4_in2_r },
	{ 0x6000, 0x7fff, MRA_ROM }, /* PROM */
	{ 0xfff0, 0xffff, MRA_ROM }, /* PROM for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress atarifb4_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x0200, 0x025f, atarifb_alphap1_vram_w, &atarifb_alphap1_vram, &atarifb_alphap1_vram_size },
	{ 0x0260, 0x039f, MWA_RAM },
	{ 0x03a0, 0x03ff, atarifb_alphap2_vram_w, &atarifb_alphap2_vram, &atarifb_alphap2_vram_size },
	{ 0x1000, 0x13bf, videoram_w, &videoram, &videoram_size },
	{ 0x13c0, 0x13ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x2000, 0x2000, atarifb_scroll_w, &atarifb_scroll_register }, /* OUT 0 */
	{ 0x2001, 0x2001, atarifb_out1_w }, /* OUT 1 */
	{ 0x2002, 0x2002, atarifb_out2_w }, /* OUT 2 */
	{ 0x2003, 0x2003, atarifb_out3_w }, /* OUT 3 */
	{ 0x3000, 0x3000, MWA_NOP }, /* Interrupt Acknowledge */
	{ 0x5000, 0x5000, watchdog_reset_w },
	{ 0x6000, 0x7fff, MWA_ROM }, /* PROM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress soccer_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0800, 0x0bff, MRA_RAM },	/* playfield/object RAM */
	{ 0x2000, 0x3fff, MRA_ROM }, /* PROM */
	{ 0x1800, 0x1800, atarifb4_in0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, atarifb4_in2_r },
	{ 0x1803, 0x1803, input_port_11_r },
	{ 0xfff0, 0xffff, MRA_ROM }, /* PROM for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress soccer_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x0200, 0x025f, atarifb_alphap1_vram_w, &atarifb_alphap1_vram, &atarifb_alphap1_vram_size },
	{ 0x0260, 0x039f, MWA_RAM },
	{ 0x03a0, 0x03ff, atarifb_alphap2_vram_w, &atarifb_alphap2_vram, &atarifb_alphap2_vram_size },
	{ 0x0800, 0x0bbf, videoram_w, &videoram, &videoram_size },
	{ 0x0bc0, 0x0bff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x1000, 0x1000, atarifb_scroll_w, &atarifb_scroll_register }, /* OUT 0 */
	{ 0x1001, 0x1001, soccer_out1_w }, /* OUT 1 */
	{ 0x1002, 0x1002, soccer_out2_w }, /* OUT 2 */
	{ 0x1004, 0x1004, MWA_NOP }, /* Interrupt Acknowledge */
	{ 0x1005, 0x1005, watchdog_reset_w },
	{ 0x2000, 0x3fff, MWA_ROM }, /* PROM */
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( atarifb )
	PORT_START		/* IN0 */
	PORT_BIT ( 0x0F, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW,  IPT_BUTTON1 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW,  IPT_TILT )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW,  IPT_COIN1 )

	PORT_START		/* IN1 */
	PORT_DIPNAME( 0x03, 0x00, "Time per coin" )
	PORT_DIPSETTING(	0x00, "1:30" )
	PORT_DIPSETTING(	0x01, "2:00" )
	PORT_DIPSETTING(	0x02, "2:30" )
	PORT_DIPSETTING(	0x03, "3:00" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Atari logo" )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* IN2 - Player 1 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN3 - Player 1 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN4 - Player 2 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN5 - Player 2 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */
INPUT_PORTS_END

INPUT_PORTS_START( atarifb4 )
	PORT_START		/* IN0 */
	PORT_BIT ( 0xff, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START		/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW,  IPT_COIN3 )
	PORT_BIT ( 0x38, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START		/* IN2 */
	PORT_DIPNAME( 0x03, 0x00, "Time per coin" )
	PORT_DIPSETTING(	0x00, "1:30" )
	PORT_DIPSETTING(	0x01, "2:00" )
	PORT_DIPSETTING(	0x02, "2:30" )
	PORT_DIPSETTING(	0x03, "3:00" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Atari logo" )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN3 - Player 1 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN4 - Player 1 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN5 - Player 2 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN6 - Player 2 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN7 - Player 3 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER3, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN8 - Player 3 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER3, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN9 - Player 4 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER4, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN10 - Player 4 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER4, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */
INPUT_PORTS_END

INPUT_PORTS_START( abaseb )
	PORT_START		/* IN0 */
	PORT_BIT ( 0x0F, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW,  IPT_BUTTON1 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW,  IPT_TILT )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW,  IPT_COIN1 )

	PORT_START		/* IN1 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x00, "Hardest" )
	PORT_DIPSETTING(	0x01, "Hard" )
	PORT_DIPSETTING(	0x02, "Fair" )
	PORT_DIPSETTING(	0x03, "Easy" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* IN2 - Player 1 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN3 - Player 1 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN4 - Player 2 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN5 - Player 2 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */
INPUT_PORTS_END

INPUT_PORTS_START( soccer )
	PORT_START		/* IN0 */
	PORT_BIT ( 0xff, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START		/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW,  IPT_UNKNOWN ) /* unused on schematics */
	PORT_BIT ( 0x02, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW,  IPT_COIN3 )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* unused on schematics */
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_TILT )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START		/* IN2 */
	PORT_DIPNAME( 0x01, 0x00, "2/4 Players" )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x01, "4" )
	PORT_DIPNAME( 0x02, 0x00, "Rule Switch" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, "Language" )
	PORT_DIPSETTING(	0x00, "English" )
	PORT_DIPSETTING(	0x04, "German" )
	PORT_DIPSETTING(	0x08, "French" )
	PORT_DIPSETTING(	0x0c, "Spanish" )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )

	PORT_START	/* IN3 - Player 1 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN4 - Player 1 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN5 - Player 2 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN6 - Player 2 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN7 - Player 3 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER3, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN8 - Player 3 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER3, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN9 - Player 4 trackball, y */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER4, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START	/* IN10 - Player 4 trackball, x */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER4, 100, 10, 0, 0 )
	/* The lower 4 bits are the input */

	PORT_START		/* IN11 */
	PORT_DIPNAME( 0x07, 0x00, "Time per coin" )
	PORT_DIPSETTING(	0x00, "1:00" )
	PORT_DIPSETTING(	0x01, "1:20" )
	PORT_DIPSETTING(	0x02, "1:40" )
	PORT_DIPSETTING(	0x03, "2:00" )
	PORT_DIPSETTING(	0x04, "2:30" )
	PORT_DIPSETTING(	0x05, "3:00" )
	PORT_DIPSETTING(	0x06, "3:30" )
	PORT_DIPSETTING(	0x07, "4:00" )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x00, "1 Coin Minimum" )
	PORT_DIPSETTING(	0x40, "2 Coin Minimum" )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* unused on schematics */
INPUT_PORTS_END




static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* no separation in 1 bpp */
	{ 15, 14, 13, 12, 7, 6, 5, 4 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout fieldlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* no separation in 1 bpp */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout soccer_fieldlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* no separation in 1 bpp */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,16,	/* 8*16 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* no separation in 1 bpp */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8 /* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritemasklayout =
{
	8,16,	/* 8*6 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* no separation in 1 bpp */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,  0x00, 0x01 }, /* offset into colors, # of colors */
	{ REGION_GFX2, 0, &fieldlayout, 0x02, 0x01 }, /* offset into colors, # of colors */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo soccer_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,         0x00, 0x01 }, /* offset into colors, # of colors */
	{ REGION_GFX3, 0x0400, &soccer_fieldlayout, 0x06, 0x01 }, /* offset into colors, # of colors */
	{ REGION_GFX2, 0x0000, &spritelayout,       0x02, 0x02 }, /* offset into colors, # of colors */
	{ REGION_GFX3, 0x0000, &spritemasklayout,   0x06, 0x03 }, /* offset into colors, # of colors */
	{ -1 } /* end of array */
};

static unsigned char palette[] =
{
	0x00,0x00,0x00, /* black  */
	0x80,0x80,0x80, /* grey  */
	0xff,0xff,0xff, /* white  */
	0x40,0x40,0x40, /* dark grey (?) - used in Soccer only */
};
static unsigned short colortable[] =
{
	0x02, 0x00, /* chars */
	0x03, 0x02, /* sprites */
	0x03, 0x00,
	0x03, 0x01, /* sprite masks */
	0x03, 0x00,
	0x03, 0x02,
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct DACinterface dac_interface =
{
	3,
	{ 50, 50, 50 }
};

static struct MachineDriver machine_driver_atarifb =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			750000, 	   /* 750 KHz */
			readmem,writemem,0,0,
			interrupt,4
		}
	},
	60, 2037,	/* frames per second, vblank duration: 16.3ms * 1/8 = 2037.5. Is it 1/8th or 3/32nds? (1528?) */
//	60, 1528,	/* frames per second, vblank duration: 16.3ms * 3/32 = 1528.125. Is it 1/8th or 3/32nds? (1528?) */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	38*8, 32*8, { 0*8, 38*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	atarifb_vh_start,
	atarifb_vh_stop,
	atarifb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_atarifb4 =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			750000, 	   /* 750 KHz */
			atarifb4_readmem,atarifb4_writemem,0,0,
			interrupt,4
		}
	},
	60, 2037,	/* frames per second, vblank duration: 16.3ms * 1/8 = 2037.5. Is it 1/8th or 3/32nds? (1528?) */
//	60, 1528,	/* frames per second, vblank duration: 16.3ms * 3/32 = 1528.125. Is it 1/8th or 3/32nds? (1528?) */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	38*8, 32*8, { 0*8, 38*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	atarifb_vh_start,
	atarifb_vh_stop,
	atarifb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_soccer =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			750000, 	   /* 750 KHz */
			soccer_readmem, soccer_writemem,0,0,
			interrupt,4
		}
	},
	60, 2037,	/* frames per second, vblank duration: 16.3ms * 1/8 = 2037.5. Is it 1/8th or 3/32nds? (1528?) */
//	60, 1528,	/* frames per second, vblank duration: 16.3ms * 3/32 = 1528.125. Is it 1/8th or 3/32nds? (1528?) */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	38*8, 32*8, { 0*8, 38*8-1, 0*8, 32*8-1 },
	soccer_gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	atarifb_vh_start,
	atarifb_vh_stop,
	atarifb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( atarifb )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "03302602.m1", 0x6800, 0x0800, 0x352e35db )
	ROM_LOAD( "03302801.p1", 0x7000, 0x0800, 0xa79c79ca )
	ROM_LOAD( "03302702.n1", 0x7800, 0x0800, 0xe7e916ae )
	ROM_RELOAD( 			    0xf800, 0x0800 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "033029.n7", 0x0000, 0x0400, 0x12f43dca )

	ROM_REGION( 0x0200, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "033030.c5", 0x0000, 0x0200, 0xeac9ef90 )
	ROM_LOAD_NIB_HIGH( "033031.d5", 0x0000, 0x0200, 0x89d619b8 )
ROM_END

ROM_START( atarifb1 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "03302601.m1", 0x6800, 0x0800, 0xf8ce7ed8 )
	ROM_LOAD( "03302801.p1", 0x7000, 0x0800, 0xa79c79ca )
	ROM_LOAD( "03302701.n1", 0x7800, 0x0800, 0x7740be51 )
	ROM_RELOAD( 			    0xf800, 0x0800 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "033029.n7", 0x0000, 0x0400, 0x12f43dca )

	ROM_REGION( 0x0200, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "033030.c5", 0x0000, 0x0200, 0xeac9ef90 )
	ROM_LOAD_NIB_HIGH( "033031.d5", 0x0000, 0x0200, 0x89d619b8 )
ROM_END

ROM_START( atarifb4 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code, the ROMs are nibble-wide */
	ROM_LOAD_NIB_LOW ( "34889.m1", 0x6000, 0x0400, 0x5c63974a )
	ROM_LOAD_NIB_HIGH( "34891.m2", 0x6000, 0x0400, 0x9d03baa1 )
	ROM_LOAD_NIB_LOW ( "34890.n1", 0x6400, 0x0400, 0x2deb5844 )
	ROM_LOAD_NIB_HIGH( "34892.n2", 0x6400, 0x0400, 0xad212d2d )
	ROM_LOAD_NIB_LOW ( "34885.k1", 0x6800, 0x0400, 0xfdd272a1 )
	ROM_LOAD_NIB_HIGH( "34887.k2", 0x6800, 0x0400, 0xfa2b8b52 )
	ROM_LOAD_NIB_LOW ( "34886.l1", 0x6c00, 0x0400, 0xbe912ccb )
	ROM_LOAD_NIB_HIGH( "34888.l2", 0x6c00, 0x0400, 0x3f8e96c1 )
	ROM_LOAD_NIB_LOW ( "34877.e1", 0x7000, 0x0400, 0xfd8832fa )
	ROM_LOAD_NIB_HIGH( "34879.e2", 0x7000, 0x0400, 0x7053ffbc )
	ROM_LOAD_NIB_LOW ( "34878.f1", 0x7400, 0x0400, 0x329eb720 )
	ROM_LOAD_NIB_HIGH( "34880.f2", 0x7400, 0x0400, 0xe0c9b4c2 )
	ROM_LOAD_NIB_LOW ( "34881.h1", 0x7800, 0x0400, 0xd9055541 )
	ROM_LOAD_NIB_HIGH( "34883.h2", 0x7800, 0x0400, 0x8a912448 )
	ROM_LOAD_NIB_LOW ( "34882.j1", 0x7c00, 0x0400, 0x060c9cdb )
	ROM_RELOAD_NIB_LOW (           0xfc00, 0x0400 ) /* for 6502 vectors */
	ROM_LOAD_NIB_HIGH( "34884.j2", 0x7c00, 0x0400, 0xaa699a3a )
	ROM_RELOAD_NIB_HIGH(           0xfc00, 0x0400 ) /* for 6502 vectors */

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "033029.n7", 0x0000, 0x0400, 0x12f43dca )

	ROM_REGION( 0x0200, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "033030.c5", 0x0000, 0x0200, 0xeac9ef90 )
	ROM_LOAD_NIB_HIGH( "033031.d5", 0x0000, 0x0200, 0x89d619b8 )
ROM_END

ROM_START( abaseb )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "34738-01.n0", 0x6000, 0x0800, 0xedcfffe8 )
	ROM_LOAD( "34737-03.m1", 0x6800, 0x0800, 0x7250863f )
	ROM_LOAD( "34735-01.p1", 0x7000, 0x0800, 0x54854d7c )
	ROM_LOAD( "34736-01.n1", 0x7800, 0x0800, 0xaf444eb0 )
	ROM_RELOAD( 			 0xf800, 0x0800 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "034710.d5", 0x0000, 0x0400, 0x31275d86 )

	ROM_REGION( 0x0200, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "034708.n7", 0x0000, 0x0200, 0x8a0f971b )
	ROM_LOAD_NIB_HIGH( "034709.c5", 0x0000, 0x0200, 0x021d1067 )
ROM_END

ROM_START( abaseb2 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code, the ROMs are nibble-wide */
	ROM_LOAD_NIB_LOW ( "034725.c0", 0x6000, 0x0400, 0x95912c58 )
	ROM_LOAD_NIB_HIGH( "034723.m0", 0x6000, 0x0400, 0x5eb1597f )
	ROM_LOAD_NIB_LOW ( "034726.b0", 0x6400, 0x0400, 0x1f8d506c )
	ROM_LOAD_NIB_HIGH( "034724.l0", 0x6400, 0x0400, 0xecd18ed2 )
	ROM_LOAD_NIB_LOW ( "034721.d1", 0x6800, 0x0400, 0x1a0541f2 )
	ROM_LOAD_NIB_HIGH( "034715.j1", 0x6800, 0x0400, 0xaccb96f5 ) /* created from 8-bit set */
	ROM_LOAD_NIB_LOW ( "034722.d0", 0x6c00, 0x0400, 0xf9c1174e ) /* The code in these 2 differs */
	ROM_LOAD_NIB_HIGH( "034716.j0", 0x6c00, 0x0400, 0xd5622749 ) /* from the 8-bit set */
	ROM_LOAD_NIB_LOW ( "034717.e1", 0x7000, 0x0400, 0xc941f64b )
	ROM_LOAD_NIB_HIGH( "034711.k1", 0x7000, 0x0400, 0xfab61782 )
	ROM_LOAD_NIB_LOW ( "034718.e0", 0x7400, 0x0400, 0x3fe7dc1c )
	ROM_LOAD_NIB_HIGH( "034712.k0", 0x7400, 0x0400, 0x0e368e1a )
	ROM_LOAD_NIB_LOW ( "034719.h1", 0x7800, 0x0400, 0x85046ee5 )
	ROM_LOAD_NIB_HIGH( "034713.f1", 0x7800, 0x0400, 0x0c67c48d )
	ROM_LOAD_NIB_LOW ( "034720.h0", 0x7c00, 0x0400, 0x37c5f149 )
	ROM_RELOAD_NIB_LOW (            0xfc00, 0x0400 ) /* for 6502 vectors */
	ROM_LOAD_NIB_HIGH( "034714.f0", 0x7c00, 0x0400, 0x920979ea )
	ROM_RELOAD_NIB_HIGH(            0xfc00, 0x0400 ) /* for 6502 vectors */

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "034710.d5", 0x0000, 0x0400, 0x31275d86 )

	ROM_REGION( 0x0200, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "034708.n7", 0x0000, 0x0200, 0x8a0f971b )
	ROM_LOAD_NIB_HIGH( "034709.c5", 0x0000, 0x0200, 0x021d1067 )
ROM_END

ROM_START( soccer )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code, the ROMs are nibble-wide */
	ROM_LOAD_NIB_LOW ( "035222.e1", 0x2000, 0x0400, 0x03ec6bce )
	ROM_LOAD_NIB_HIGH( "035224.e2", 0x2000, 0x0400, 0xa1aeaa70 )
	ROM_LOAD_NIB_LOW ( "035223.f1", 0x2400, 0x0400, 0x9c600726 )
	ROM_LOAD_NIB_HIGH( "035225.f2", 0x2400, 0x0400, 0x2aa06521 )
	ROM_LOAD_NIB_LOW ( "035226.h1", 0x2800, 0x0400, 0xd57c0cfb )
	ROM_LOAD_NIB_HIGH( "035228.h2", 0x2800, 0x0400, 0x594574cb )
	ROM_LOAD_NIB_LOW ( "035227.j1", 0x2c00, 0x0400, 0x4112b257 )
	ROM_LOAD_NIB_HIGH( "035229.j2", 0x2c00, 0x0400, 0x412d129c )

	ROM_LOAD_NIB_LOW ( "035230.k1", 0x3000, 0x0400, 0x747f6e4a )
	ROM_LOAD_NIB_HIGH( "035232.k2", 0x3000, 0x0400, 0x55f43e7f )
	ROM_LOAD_NIB_LOW ( "035231.l1", 0x3400, 0x0400, 0xd584c199 )
	ROM_LOAD_NIB_HIGH( "035233.l2", 0x3400, 0x0400, 0xb343f500 )
	ROM_LOAD_NIB_LOW ( "035234.m1", 0x3800, 0x0400, 0x83524bb7 )
	ROM_LOAD_NIB_HIGH( "035236.m2", 0x3800, 0x0400, 0xc53f4d13 )
	ROM_LOAD_NIB_LOW ( "035235.n1", 0x3c00, 0x0400, 0xd6855b0e )
	ROM_RELOAD_NIB_LOW (            0xfc00, 0x0400 ) /* for 6502 vectors */
	ROM_LOAD_NIB_HIGH( "035237.n2", 0x3c00, 0x0400, 0x1d01b054 )
	ROM_RELOAD_NIB_HIGH(            0xfc00, 0x0400 ) /* for 6502 vectors */

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "035250.r2", 0x0000, 0x0400, 0x12f43dca ) /* characters */

	ROM_REGION( 0x0800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_NIB_LOW ( "035247.n7", 0x0000, 0x0400, 0x3adb5f4e ) /* sprites */
	ROM_LOAD_NIB_HIGH( "035248.m7", 0x0000, 0x0400, 0xa890cd48 )

	ROM_REGION( 0x0800, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "035246.r6", 0x0000, 0x0800, 0x4a996136 ) /* spritemask - playfield */
ROM_END


void init_atarifb (void)
{
	/* Tell the video code to draw the plays for this version */
	atarifb_game = 1;
}

void init_atarifb4(void)
{
	/* Tell the video code to draw the plays for this version */
	atarifb_game = 2;
}

void init_abaseb(void)
{
	/* Tell the video code to draw the plays for this version */
	atarifb_game = 3;
}

void init_soccer(void)
{
	/* Tell the video code to draw the plays for this version */
	atarifb_game = 4;
}



GAME( 1978, atarifb,  0,       atarifb,  atarifb,  atarifb,  ROT0, "Atari", "Atari Football (revision 2)" )
GAME( 1978, atarifb1, atarifb, atarifb,  atarifb,  atarifb,  ROT0, "Atari", "Atari Football (revision 1)" )
GAME( 1979, atarifb4, atarifb, atarifb4, atarifb4, atarifb4, ROT0, "Atari", "Atari Football (4 players)" )
GAME( 1979, abaseb,   0,       atarifb,  abaseb,   abaseb,   ROT0, "Atari", "Atari Baseball (set 1)" )
GAME( 1979, abaseb2,  abaseb,  atarifb,  abaseb,   abaseb,   ROT0, "Atari", "Atari Baseball (set 2)" )
GAME( 197?, soccer,   0,       soccer,   soccer,   soccer,   ROT0, "Atari", "Atari Soccer" )
