#include "../vidhrdw/bsktball.cpp"
#include "../machine/bsktball.cpp"

/***************************************************************************

Atari Basketball Driver

Note:  The original hardware uses the Player 1 and Player 2 Start buttons
as the Jump/Shoot buttons.	I've taken button 1 and mapped it to the Start
buttons to keep people from getting confused.

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* machine/bsktball.c */
WRITE_HANDLER( bsktball_nmion_w );
extern int bsktball_interrupt(void);
WRITE_HANDLER( bsktball_ld1_w );
WRITE_HANDLER( bsktball_ld2_w );
READ_HANDLER( bsktball_in0_r );
WRITE_HANDLER( bsktball_led1_w );
WRITE_HANDLER( bsktball_led2_w );

/* vidhrdw/bsktball.c */
extern unsigned char *bsktball_motion;
extern void bsktball_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


/* sound hardware - temporary */

static int note_timer=255;
static int note_count=256;
static WRITE_HANDLER( bsktball_note_w );
static void bsktball_note_32H(int foo);
static void bsktball_noise_256H(int foo);
static int init_timer=1;
static int crowd_mask=0;

#define TIME_32H 10582*2
#define TIME_256H TIME_32H*4

static WRITE_HANDLER( bsktball_note_w )
{

	note_timer=data;
	note_count=256;

	if ((init_timer) && (note_timer!=255))
	{
		timer_set (TIME_IN_NSEC(TIME_32H), 0, bsktball_note_32H);
		init_timer=0;
	}
}

static int noise_b10=0;
static int noise_a10=0;
static int noise=0;
static int noise_timer_set=0;

static WRITE_HANDLER( bsktball_noise_reset_w )
{
	noise_a10=0;
	noise_b10=0;
	DAC_data_w(2,0);

	if (!noise_timer_set)
		timer_set (TIME_IN_NSEC(TIME_256H), 0, bsktball_noise_256H);
	noise_timer_set=1;
}

static void bsktball_noise_256H(int foo)
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

	timer_set (TIME_IN_NSEC(TIME_256H), 0, bsktball_noise_256H);
	noise_timer_set=1;
}

static void bsktball_note_32H(int foo)
{
	note_count--;

	if (note_count==note_timer)
		note_count=256;

	if (note_count > ((256-note_timer)>>1)+note_timer)
		DAC_data_w(0,255);			/* MB: Generate a square, 50% duty cycle _|-|_| */
	else
		DAC_data_w(0,0);

	if (note_timer!=255)
		timer_set (TIME_IN_NSEC(TIME_32H), 0, bsktball_note_32H);
	else
		init_timer=1;
}

static WRITE_HANDLER( bsktball_bounce_w )
{
	/* D0-D3 = crowd */
	crowd_mask = (data & 0x0F) << 4;
	if (noise)
		DAC_data_w(2,crowd_mask);
	else
		DAC_data_w(2,0);

	/* D4 = bounce */
	if (data & 0x10)
		DAC_data_w(1,255);
	else
		DAC_data_w(1,0);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM }, /* Zero Page RAM */
	{ 0x0800, 0x0800, bsktball_in0_r },
	{ 0x0802, 0x0802, input_port_5_r },
	{ 0x0803, 0x0803, input_port_6_r },
	{ 0x1800, 0x1cff, MRA_RAM }, /* video ram */
	{ 0x2000, 0x3fff, MRA_ROM }, /* PROGRAM */
	{ 0xfff0, 0xffff, MRA_ROM }, /* PROM8 for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM }, /* WRAM */
	{ 0x1000, 0x1000, MWA_RAM }, /* Timer Reset */
	{ 0x1010, 0x101f, bsktball_bounce_w }, /* Crowd Amp / Bounce */
	{ 0x1022, 0x1023, MWA_RAM }, /* Coin Counter */
	{ 0x1024, 0x1025, bsktball_led1_w }, /* LED 1 */
	{ 0x1026, 0x1027, bsktball_led2_w }, /* LED 2 */
	{ 0x1028, 0x1029, bsktball_ld1_w }, /* LD 1 */
	{ 0x102a, 0x102b, bsktball_ld2_w }, /* LD 2 */
	{ 0x102c, 0x102d, bsktball_noise_reset_w }, /* Noise Reset */
	{ 0x102e, 0x102f, bsktball_nmion_w }, /* NMI On */
	{ 0x1030, 0x103f, bsktball_note_w }, /* Music Ckt Note Dvsr */
	{ 0x1800, 0x1bbf, videoram_w, &videoram, &videoram_size }, /* DISPLAY */
	{ 0x1bc0, 0x1bff, MWA_RAM, &bsktball_motion },
	{ 0x2000, 0x3fff, MWA_ROM }, /* PROM1-PROM8 */
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( bsktball )
	PORT_START	/* IN0 */
	PORT_ANALOG( 0xFF, 0x00, IPT_TRACKBALL_X, 100, 10, 0, 0 ) /* Sensitivity, clip, min, max */

	PORT_START	/* IN0 */
	PORT_ANALOG( 0xFF, 0x00, IPT_TRACKBALL_Y, 100, 10, 0, 0 )

	PORT_START	/* IN0 */
	PORT_ANALOG( 0xFF, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 10, 0, 0 ) /* Sensitivity, clip, min, max */

	PORT_START	/* IN0 */
	PORT_ANALOG( 0xFF, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER2, 100, 10, 0, 0 )

	PORT_START		/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 ) /* SPARE */
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) /* SPARE */
	/* 0x10 - DR0 = PL2 H DIR */
	/* 0x20 - DR1 = PL2 V DIR */
	/* 0x40 - DR2 = PL1 H DIR */
	/* 0x80 - DR3 = PL1 V DIR */

	PORT_START		/* IN2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* SPARE */
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* TEST STEP */
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* COIN 0 */
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN2 ) /* COIN 1 */
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) /* COIN 2 */

	PORT_START		/* DSW */
	PORT_DIPNAME( 0x07, 0x00, "Coin Mode" )
	PORT_DIPSETTING(	0x07, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(	0x06, "2:30/Credit" )
	PORT_DIPSETTING(	0x05, "2:00/Credit" )
	PORT_DIPSETTING(	0x04, "1:30/Credit" )
	PORT_DIPSETTING(	0x03, "1:15/Credit" )
	PORT_DIPSETTING(	0x02, "0:45/Credit" )
	PORT_DIPSETTING(	0x01, "0:30/Credit" )
	PORT_DIPSETTING(	0x00, "1:00/Credit" )
	PORT_DIPNAME( 0x18, 0x00, "Dollar Coin Mode" )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x20, 0x00, "Cost" )
	PORT_DIPSETTING(	0x20, "Two Coin Minimum" )
	PORT_DIPSETTING(	0x00, "One Coin Minimum" )
	PORT_DIPNAME( 0xC0, 0x00, "Language" )
	PORT_DIPSETTING(	0xC0, "German" )
	PORT_DIPSETTING(	0x80, "French" )
	PORT_DIPSETTING(	0x40, "Spanish" )
	PORT_DIPSETTING(	0x00, "English" )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	2,		/* 2 bits per pixel */
	{ 0, 8*0x800 }, 	   /* bitplanes separated by $800 bytes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout motionlayout =
{
	8,32,	/* 8*32 characters */
	64, 	/* 64 characters */
	2,		/* 2 bits per pixel */
	{ 0, 8*0x800 }, 	   /* bitplanes separated by $800 bytes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{	0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8,
		16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8,
		24*8, 25*8, 26*8, 27*8, 28*8, 29*8, 30*8, 31*8 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0600, &charlayout,   0x00, 0x02 },
	{ REGION_GFX1, 0x0000, &motionlayout, 0x08, 0x40 },
	{ -1 } /* end of array */
};



static unsigned char palette[] =
{
	0x00,0x00,0x00, /* BLACK */
	0x80,0x80,0x80, /* LIGHT GREY */
	0x50,0x50,0x50, /* DARK GREY */
	0xff,0xff,0xff, /* WHITE */
};
static unsigned short colortable[] =
{
	/* Playfield */
	0x01, 0x00, 0x00, 0x00,
	0x01, 0x03, 0x03, 0x03,

	/* Motion */
	0x01, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x02, 0x00,
	0x01, 0x00, 0x03, 0x00,

	0x01, 0x01, 0x00, 0x00,
	0x01, 0x01, 0x01, 0x00,
	0x01, 0x01, 0x02, 0x00,
	0x01, 0x01, 0x03, 0x00,

	0x01, 0x02, 0x00, 0x00,
	0x01, 0x02, 0x01, 0x00,
	0x01, 0x02, 0x02, 0x00,
	0x01, 0x02, 0x03, 0x00,

	0x01, 0x03, 0x00, 0x00,
	0x01, 0x03, 0x01, 0x00,
	0x01, 0x03, 0x02, 0x00,
	0x01, 0x03, 0x03, 0x00,

	0x01, 0x00, 0x00, 0x01,
	0x01, 0x00, 0x01, 0x01,
	0x01, 0x00, 0x02, 0x01,
	0x01, 0x00, 0x03, 0x01,

	0x01, 0x01, 0x00, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x02, 0x01,
	0x01, 0x01, 0x03, 0x01,

	0x01, 0x02, 0x00, 0x01,
	0x01, 0x02, 0x01, 0x01,
	0x01, 0x02, 0x02, 0x01,
	0x01, 0x02, 0x03, 0x01,

	0x01, 0x03, 0x00, 0x01,
	0x01, 0x03, 0x01, 0x01,
	0x01, 0x03, 0x02, 0x01,
	0x01, 0x03, 0x03, 0x01,

	0x01, 0x00, 0x00, 0x02,
	0x01, 0x00, 0x01, 0x02,
	0x01, 0x00, 0x02, 0x02,
	0x01, 0x00, 0x03, 0x02,

	0x01, 0x01, 0x00, 0x02,
	0x01, 0x01, 0x01, 0x02,
	0x01, 0x01, 0x02, 0x02,
	0x01, 0x01, 0x03, 0x02,

	0x01, 0x02, 0x00, 0x02,
	0x01, 0x02, 0x01, 0x02,
	0x01, 0x02, 0x02, 0x02,
	0x01, 0x02, 0x03, 0x02,

	0x01, 0x03, 0x00, 0x02,
	0x01, 0x03, 0x01, 0x02,
	0x01, 0x03, 0x02, 0x02,
	0x01, 0x03, 0x03, 0x02,

	0x01, 0x00, 0x00, 0x03,
	0x01, 0x00, 0x01, 0x03,
	0x01, 0x00, 0x02, 0x03,
	0x01, 0x00, 0x03, 0x03,

	0x01, 0x01, 0x00, 0x03,
	0x01, 0x01, 0x01, 0x03,
	0x01, 0x01, 0x02, 0x03,
	0x01, 0x01, 0x03, 0x03,

	0x01, 0x02, 0x00, 0x03,
	0x01, 0x02, 0x01, 0x03,
	0x01, 0x02, 0x02, 0x03,
	0x01, 0x02, 0x03, 0x03,

	0x01, 0x03, 0x00, 0x03,
	0x01, 0x03, 0x01, 0x03,
	0x01, 0x03, 0x02, 0x03,
	0x01, 0x03, 0x03, 0x03,

};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct DACinterface dac_interface =
{
	3,
	{ 100, 100, 100 }
};


static struct MachineDriver machine_driver_bsktball =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			750000, 	   /* 750 KHz */
			readmem,writemem,0,0,
			bsktball_interrupt,8
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	bsktball_vh_screenrefresh,

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

  Game driver(s)

***************************************************************************/

ROM_START( bsktball )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "034765.d1",    0x2000, 0x0800, 0x798cea39 )
	ROM_LOAD( "034764.c1",    0x2800, 0x0800, 0xa087109e )
	ROM_LOAD( "034766.f1",    0x3000, 0x0800, 0xa82e9a9f )
	ROM_LOAD( "034763.b1",    0x3800, 0x0800, 0x1fc69359 )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "034757.a6",    0x0000, 0x0800, 0x010e8ad3 )
	ROM_LOAD( "034758.b6",    0x0800, 0x0800, 0xf7bea344 )
ROM_END



GAME( 1979, bsktball, 0, bsktball, bsktball, 0, ROT0, "Atari", "Basketball" )
