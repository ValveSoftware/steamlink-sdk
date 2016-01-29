#include "../vidhrdw/redalert.cpp"
#include "../sndhrdw/redalert.cpp"

/***************************************************************************

Irem Red Alert Driver

Everything in this driver is guesswork and speculation.  If something
seems wrong, it probably is.

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* vidhrdw/redalert.c */
extern unsigned char *redalert_backram;
extern unsigned char *redalert_spriteram1;
extern unsigned char *redalert_spriteram2;
extern unsigned char *redalert_characterram;
WRITE_HANDLER( redalert_backram_w );
WRITE_HANDLER( redalert_spriteram1_w );
WRITE_HANDLER( redalert_spriteram2_w );
WRITE_HANDLER( redalert_characterram_w );
extern void redalert_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( redalert_c040_w );
WRITE_HANDLER( redalert_backcolor_w );

/* sndhrdw/redalert.c */
WRITE_HANDLER( redalert_c030_w );
READ_HANDLER( redalert_voicecommand_r );
WRITE_HANDLER( redalert_soundlatch_w );
READ_HANDLER( redalert_AY8910_A_r );
WRITE_HANDLER( redalert_AY8910_B_w );
WRITE_HANDLER( redalert_AY8910_w );
READ_HANDLER( redalert_sound_register_IC1_r );
WRITE_HANDLER( redalert_sound_register_IC2_w );


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM }, /* Zero page / stack */
	{ 0x0200, 0x0fff, MRA_RAM }, /* ? */
	{ 0x1000, 0x1fff, MRA_RAM }, /* Scratchpad video RAM */
	{ 0x2000, 0x4fff, MRA_RAM }, /* Video RAM */
	{ 0x5000, 0xbfff, MRA_ROM },
	{ 0xc100, 0xc100, input_port_0_r },
	{ 0xc110, 0xc110, input_port_1_r },
	{ 0xc120, 0xc120, input_port_2_r },
	{ 0xc170, 0xc170, input_port_3_r }, /* Vertical Counter? */
	{ 0xf000, 0xffff, MRA_ROM }, /* remapped ROM for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x0200, 0x0fff, MWA_RAM }, /* ? */
	{ 0x1000, 0x1fff, MWA_RAM }, /* Scratchpad video RAM */
	{ 0x2000, 0x3fff, redalert_backram_w, &redalert_backram },
	{ 0x4000, 0x43ff, videoram_w, &videoram, &videoram_size },
	{ 0x4400, 0x47ff, redalert_spriteram1_w, &redalert_spriteram1 },
	{ 0x4800, 0x4bff, redalert_characterram_w, &redalert_characterram },
	{ 0x4c00, 0x4fff, redalert_spriteram2_w, &redalert_spriteram2 },
	{ 0x5000, 0xbfff, MWA_ROM },
	{ 0xc130, 0xc130, redalert_c030_w },
//	{ 0xc140, 0xc140, redalert_c040_w }, /* Output port? */
	{ 0xc150, 0xc150, redalert_backcolor_w },
	{ 0xc160, 0xc160, redalert_soundlatch_w },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x7800, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },
	{ 0x1001, 0x1001, redalert_sound_register_IC1_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x7800, 0x7fff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_ROM },
	{ 0x1000, 0x1000, redalert_AY8910_w },
	{ 0x1001, 0x1001, redalert_sound_register_IC2_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress voice_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x83ff, MRA_RAM },
//	{ 0xc000, 0xc000, redalert_voicecommand_r }, /* reads command from D0-D5? */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress voice_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x83ff, MWA_RAM },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( redalert )
	PORT_START			   /* DIP Switches */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x08, "7000" )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START			   /* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Meter */

	PORT_START			   /* IN2  */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Meter */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Meter */

	PORT_START			   /* IN3 - some type of video counter? */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START			   /* Fake input for coins */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_SERVICE )
INPUT_PORTS_END


static struct GfxLayout backlayout =
{
	8,8,	/* 8*8 characters */
	0x400,	  /* 1024 characters */
	1,	/* 1 bits per pixel */
	{ 0 }, /* No info needed for bit offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	128,	/* 128 characters */
	1,	/* 1 bits per pixel */
	{ 0 }, /* No info needed for bit offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,8,	/* 8*8 characters */
	128,	/* 128 characters */
	2,		/* 1 bits per pixel */
	{ 0, 0x800*8 }, /* No info needed for bit offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 0, 0x3000, &backlayout,	0, 8 }, 	/* the game dynamically modifies this */
	{ 0, 0x4800, &charlayout,	0, 8 }, 	/* the game dynamically modifies this */
	{ 0, 0x4400, &spritelayout,16, 4 }, 	/* the game dynamically modifies this */
	{ -1 } /* end of array */
};

/* Arbitrary colors */
static unsigned char palette[] =
{
	0x40,0x80,0xff,	/* Background */
	0x00,0x00,0xff,	/* Blue */
	0xff,0x00,0xff,	/* Magenta */
	0x00,0xff,0xff,	/* Cyan */
	0xff,0x00,0x00,	/* Red */
	0xff,0x80,0x00,	/* Orange */
	0xff,0xff,0x00,	/* Yellow */
	0xff,0xff,0xff,	/* White */
	0x00,0x00,0x00,	/* Black */
};

/* Arbitrary colortable */
static unsigned short colortable[] =
{
	0,7,
	0,6,
	0,2,
	0,4,
	0,3,
	0,6,
	0,1,
	0,8,

	0,8,8,8,
	0,6,4,7,
	0,6,4,1,
	0,8,5,1,
};

static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}



static int redalert_interrupt(void)
{
	static int lastcoin = 0;
	int newcoin;

	newcoin = input_port_4_r(0);

	if (newcoin)
	{
		if ((newcoin & 0x01) && !(lastcoin & 0x01))
		{
			lastcoin = newcoin;
			return nmi_interrupt();
		}
		if ((newcoin & 0x02) && !(lastcoin & 0x02))
		{
			lastcoin = newcoin;
			return nmi_interrupt();
		}
	}

	lastcoin = newcoin;
	return interrupt();
}

static struct AY8910interface ay8910_interface =
{
	1,			/* 1 chip */
	2000000,	/* 2 MHz */
	{ 50 },		/* Volume */
	{ redalert_AY8910_A_r },		/* Port A Read */
	{ 0 },		/* Port B Read */
	{ 0 },		/* Port A Write */
	{ redalert_AY8910_B_w }		/* Port B Write */
};



static struct MachineDriver machine_driver_redalert =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1000000,	   /* ???? */
			readmem,writemem,0,0,
			redalert_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1000000,	   /* 1 MHz */
			sound_readmem,sound_writemem,0,0,
			/* IRQ is hooked to a 555 timer, whose freq is 1150 Hz */
			0,0,
			interrupt,1150
		},
		{
			CPU_8085A | CPU_AUDIO_CPU,
			1000000,	   /* 1 MHz? */
			voice_readmem,voice_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	redalert_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}

};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( redalert )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "rag5",         	0x5000, 0x1000, 0xd7c9cdd6 )
	ROM_LOAD( "rag6",         	0x6000, 0x1000, 0xcb2a308c )
	ROM_LOAD( "rag7n",        	0x7000, 0x1000, 0x82ab2dae )
	ROM_LOAD( "rag8n",        	0x8000, 0x1000, 0xb80eece9 )
	ROM_RELOAD(                 0xf000, 0x1000 )
	ROM_LOAD( "rag9",         	0x9000, 0x1000, 0x2b7d1295 )
	ROM_LOAD( "ragab",        	0xa000, 0x1000, 0xab99f5ed )
	ROM_LOAD( "ragb",         	0xb000, 0x1000, 0x8e0d1661 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for code */
	ROM_LOAD( "w3s1",         	0x7800, 0x0800, 0x4af956a5 )
	ROM_RELOAD(                0xf800, 0x0800 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* 64k for code */
	ROM_LOAD( "ras1b",        	0x0000, 0x1000, 0xec690845 )
	ROM_LOAD( "ras2",         	0x1000, 0x1000, 0xfae94cfc )
	ROM_LOAD( "ras3",         	0x2000, 0x1000, 0x20d56f3e )
	ROM_LOAD( "ras4",         	0x3000, 0x1000, 0x130e66db )
ROM_END



GAMEX( 1981, redalert, 0, redalert, redalert, 0, ROT270, "Irem + GDI", "Red Alert", GAME_WRONG_COLORS | GAME_IMPERFECT_SOUND)
