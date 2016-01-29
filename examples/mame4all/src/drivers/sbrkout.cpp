#include "../machine/sbrkout.cpp"
#include "../vidhrdw/sbrkout.cpp"

/***************************************************************************
Atari Super Breakout Driver

Note:  I'm cheating a little bit with the paddle control.  The original
game handles the paddle control as following.  The paddle is a potentiometer.
Every VBlank signal triggers the start of a voltage ramp.  Whenever the
ramp has the same value as the potentiometer, an NMI is generated.	In the
NMI code, the current scanline value is used to calculate the value to
put into location $1F in memory.  I cheat in this driver by just putting
the paddle value directly into $1F, which has the same net result.

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)

CHANGES:
MAB 05 MAR 99 - changed overlay support to use artwork functions
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* vidhrdw/sbrkout.c */
void sbrkout_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int sbrkout_vh_start(void);
extern unsigned char *sbrkout_horiz_ram;
extern unsigned char *sbrkout_vert_ram;

/* machine/sbrkout.c */
WRITE_HANDLER( sbrkout_serve_led_w );
WRITE_HANDLER( sbrkout_start_1_led_w );
WRITE_HANDLER( sbrkout_start_2_led_w );
READ_HANDLER( sbrkout_read_DIPs_r );
extern int sbrkout_interrupt(void);
READ_HANDLER( sbrkout_select1_r );
READ_HANDLER( sbrkout_select2_r );


/* sound hardware - temporary */

static WRITE_HANDLER( sbrkout_dac_w );
static void sbrkout_tones_4V(int foo);
static int init_timer=1;

#define TIME_4V 4.075/4

unsigned char *sbrkout_sound;

static WRITE_HANDLER( sbrkout_dac_w )
{
	sbrkout_sound[offset]=data;

	if (init_timer)
	{
		timer_set (TIME_IN_MSEC(TIME_4V), 0, sbrkout_tones_4V);
		init_timer=0;
	}
}

static void sbrkout_tones_4V(int foo)
{
	static int vlines=0;

	if ((*sbrkout_sound) & vlines)
		DAC_data_w(0,255);
	else
		DAC_data_w(0,0);

	vlines = (vlines+1) % 16;

	timer_set (TIME_IN_MSEC(TIME_4V), 0, sbrkout_tones_4V);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x001f, 0x001f, input_port_6_r }, /* paddle value */
	{ 0x0000, 0x00ff, MRA_RAM }, /* Zero Page RAM */
	{ 0x0100, 0x01ff, MRA_RAM }, /* ??? */
	{ 0x0400, 0x077f, MRA_RAM }, /* Video Display RAM */
	{ 0x0828, 0x0828, sbrkout_select1_r }, /* Select 1 */
	{ 0x082f, 0x082f, sbrkout_select2_r }, /* Select 2 */
	{ 0x082e, 0x082e, input_port_5_r }, /* Serve Switch */
	{ 0x0830, 0x0833, sbrkout_read_DIPs_r }, /* DIP Switches */
	{ 0x0840, 0x0840, input_port_1_r }, /* Coin Switches */
	{ 0x0880, 0x0880, input_port_2_r }, /* Start Switches */
	{ 0x08c0, 0x08c0, input_port_3_r }, /* Self Test Switch */
	{ 0x0c00, 0x0c00, input_port_4_r }, /* Vertical Sync Counter */
	{ 0x2c00, 0x3fff, MRA_ROM }, /* PROGRAM */
	{ 0xfff0, 0xffff, MRA_ROM }, /* PROM8 for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0011, 0x0011, sbrkout_dac_w, &sbrkout_sound }, /* Noise Generation Bits */
	{ 0x0010, 0x0014, MWA_RAM, &sbrkout_horiz_ram }, /* Horizontal Ball Position */
	{ 0x0018, 0x001d, MWA_RAM, &sbrkout_vert_ram }, /* Vertical Ball Position / ball picture */
	{ 0x0000, 0x00ff, MWA_RAM }, /* WRAM */
	{ 0x0100, 0x01ff, MWA_RAM }, /* ??? */
	{ 0x0400, 0x07ff, videoram_w, &videoram, &videoram_size }, /* DISPLAY */
//	{ 0x0c10, 0x0c11, sbrkout_serve_led_w }, /* Serve LED */
	{ 0x0c30, 0x0c31, sbrkout_start_1_led_w }, /* 1 Player Start Light */
	{ 0x0c40, 0x0c41, sbrkout_start_2_led_w }, /* 2 Player Start Light */
	{ 0x0c50, 0x0c51, MWA_RAM }, /* NMI Pot Reading Enable */
	{ 0x0c70, 0x0c71, MWA_RAM }, /* Coin Counter */
	{ 0x0c80, 0x0c80, MWA_NOP }, /* Watchdog */
	{ 0x0e00, 0x0e00, MWA_NOP }, /* IRQ Enable? */
	{ 0x1000, 0x1000, MWA_RAM }, /* LSB of Pot Reading */
	{ 0x2c00, 0x3fff, MWA_ROM }, /* PROM1-PROM8 */
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( sbrkout )
	PORT_START		/* DSW - fake port, gets mapped to Super Breakout ports */
	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(	0x00, "English" )
	PORT_DIPSETTING(	0x01, "German" )
	PORT_DIPSETTING(	0x02, "French" )
	PORT_DIPSETTING(	0x03, "Spanish" )
	PORT_DIPNAME( 0x0C, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x0C, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x70, 0x00, "Extended Play" ) /* P=Progressive, C=Cavity, D=Double */
	PORT_DIPSETTING(	0x10, "200P/200C/200D" )
	PORT_DIPSETTING(	0x20, "400P/300C/400D" )
	PORT_DIPSETTING(	0x30, "600P/400C/600D" )
	PORT_DIPSETTING(	0x40, "900P/700C/800D" )
	PORT_DIPSETTING(	0x50, "1200P/900C/1000D" )
	PORT_DIPSETTING(	0x60, "1600P/1100C/1200D" )
	PORT_DIPSETTING(	0x70, "2000P/1400C/1500D" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x80, "3" )
	PORT_DIPSETTING(	0x00, "5" )

	PORT_START		/* IN0 */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_COIN2 )

	PORT_START		/* IN1 */
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_TILT )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START		/* IN3 */
	PORT_BIT ( 0xFF, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START		/* IN4 */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )

	PORT_START		/* IN5 */
	PORT_ANALOG( 0xff, 0x00, IPT_PADDLE | IPF_REVERSE, 50, 10, 0, 255 )

	PORT_START		/* IN6 - fake port, used to set the game select dial */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Progressive", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Double", KEYCODE_D, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Cavity", KEYCODE_C, IP_JOY_NONE )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },		  /* no separation in 1 bpp */
	{ 4, 5, 6, 7, 0x200*8 + 4, 0x200*8 + 5, 0x200*8 + 6, 0x200*8 + 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout balllayout =
{
	3,3,	/* 3*3 character? */
	2,	    /* 2 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* no separation in 1 bpp */
	{ 0, 1, 2 },
	{ 0*8, 1*8, 2*8 },
	3*8 /* every char takes 3 consecutive byte */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 2 },
	{ REGION_GFX2, 0, &balllayout, 0, 2 },
	{ -1 } /* end of array */
};

static unsigned char palette[] =
{
	0x00,0x00,0x00, /* BLACK  */
	0xff,0xff,0xff, /* WHITE  */
};

#define ARTWORK_COLORS (2 + 32768)

static unsigned short colortable[ARTWORK_COLORS] =
{
	0, 0,  /* Don't draw */
	0, 1,  /* Draw */
};

static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};



static struct MachineDriver machine_driver_sbrkout =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			375000, 	   /* 375 KHz? Should be 750KHz? */
			readmem,writemem,0,0,
			sbrkout_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	ARTWORK_COLORS,ARTWORK_COLORS,		/* Declare extra colors for the overlay */
	init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sbrkout_vh_start,
	generic_vh_stop,
	sbrkout_vh_screenrefresh,

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

ROM_START( sbrkout )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "033453.c1",    0x2800, 0x0800, 0xa35d00e3 )
	ROM_LOAD( "033454.d1",    0x3000, 0x0800, 0xd42ea79a )
	ROM_LOAD( "033455.e1",    0x3800, 0x0800, 0xe0a6871c )
	ROM_RELOAD(               0xf800, 0x0800 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "033280.p4",    0x0000, 0x0200, 0x5a69ce85 )
	ROM_LOAD( "033281.r4",    0x0200, 0x0200, 0x066bd624 )

	ROM_REGION( 0x0020, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "033282.k6",    0x0000, 0x0020, 0x6228736b )
ROM_END



GAME( 1978, sbrkout, 0, sbrkout, sbrkout, 0, ROT270, "Atari", "Super Breakout" )
