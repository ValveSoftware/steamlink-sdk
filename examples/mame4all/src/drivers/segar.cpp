#include "../vidhrdw/segar.cpp"
#include "../sndhrdw/segar.cpp"
#include "../machine/segar.cpp"

/***************************************************************************

Sega G-80 Raster drivers

The known G-80 Raster games are Astro Blaster, Monster Bash, 005,
Space Odyssey, Pig Newton, and Sindbad Mystery.

See also sega.c for the Sega G-80 Vector games.

Many thanks go to Dave Fish for the fine detective work he did into the
G-80 security chips (315-0064, 315-0070, 315-0076, 315-0082) which provided
me with enough information to emulate those chips at runtime along with
the 315-0062 Astro Blaster chip and the 315-0063 Space Odyssey chip.

Special note (24-MAR-1999) - Sindbad Mystery does *not* use the standard
G-80 security chip; rather, it uses the Sega System 1 encryption.

Thanks also go to Paul Tonizzo, Clay Cowgill, John Bowes, and Kevin Klopp
for all the helpful information, samples, and schematics!

TODO:
- locate Pig Newton cocktail mode?
- verify Pig Newton and Sindbad Mystery DIPs
- attempt Pig Newton, 005 sound
- fix transparency issues (Pig Newton, Sindbad Mystery)
- fix Space Odyssey background
- figure out why Astro Blaster version 1 ends the game right away

- Mike Balfour (mab22@po.cwru.edu)

***************************************************************************

26/3/2000:	** Darren Hatton (UKVAC) / Adrian Purser (UKVAC) **
			Added a 3rd Astro Blaster ROM set (ASTROB2).
			Updated Dip Switches to be correct for the Astro Blaster sets.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/i8039/i8039.h"
#include "cpu/z80/z80.h"

/* sndhrdw/segar.c */

WRITE_HANDLER( astrob_speech_port_w );
WRITE_HANDLER( astrob_audio_ports_w );
WRITE_HANDLER( spaceod_audio_ports_w );
WRITE_HANDLER( monsterb_audio_8255_w );
 READ_HANDLER( monsterb_audio_8255_r );

 READ_HANDLER( monsterb_sh_rom_r );
 READ_HANDLER( monsterb_sh_t1_r );
 READ_HANDLER( monsterb_sh_command_r );
WRITE_HANDLER( monsterb_sh_dac_w );
WRITE_HANDLER( monsterb_sh_busy_w );
WRITE_HANDLER( monsterb_sh_offset_a0_a3_w );
WRITE_HANDLER( monsterb_sh_offset_a4_a7_w );
WRITE_HANDLER( monsterb_sh_offset_a8_a11_w );
WRITE_HANDLER( monsterb_sh_rom_select_w );

/* temporary speech handling through samples */
int astrob_speech_sh_start(const struct MachineSound *msound);
void astrob_speech_sh_update(void);

/* sample names */
extern const char *astrob_sample_names[];
extern const char *s005_sample_names[];
extern const char *monsterb_sample_names[];
extern const char *spaceod_sample_names[];

/* machine/segar.c */

void sega_security(int chip);
WRITE_HANDLER( segar_w );

extern unsigned char *segar_mem;

/* machine/segacrpt.c */

void sindbadm_decode(void);

/* vidhrdw/segar.c */

extern unsigned char *segar_characterram;
extern unsigned char *segar_characterram2;
extern unsigned char *segar_mem_colortable;
extern unsigned char *segar_mem_bcolortable;

WRITE_HANDLER( segar_characterram_w );
WRITE_HANDLER( segar_characterram2_w );
WRITE_HANDLER( segar_colortable_w );
WRITE_HANDLER( segar_bcolortable_w );
void segar_init_colors(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( segar_video_port_w );
int  segar_vh_start(void);
void segar_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( monsterb_back_port_w );
int  monsterb_vh_start(void);
void monsterb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int  spaceod_vh_start(void);
void spaceod_vh_stop(void);
WRITE_HANDLER( spaceod_back_port_w );
WRITE_HANDLER( spaceod_backshift_w );
WRITE_HANDLER( spaceod_backshift_clear_w );
WRITE_HANDLER( spaceod_backfill_w );
WRITE_HANDLER( spaceod_nobackfill_w );
void spaceod_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( pignewt_back_color_w );
WRITE_HANDLER( pignewt_back_ports_w );

WRITE_HANDLER( sindbadm_back_port_w );
void sindbadm_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/***************************************************************************

  The Sega games use NMI to trigger the self test. We use a fake input port to
  tie that event to a keypress.

***************************************************************************/
static int segar_interrupt(void)
{
	if (readinputport(5) & 1)       /* get status of the F2 key */
		return nmi_interrupt(); /* trigger self test */
	else return interrupt();
}

/***************************************************************************

  The Sega games store the DIP switches in a very mangled format that's
  not directly useable by MAME.  This function mangles the DIP switches
  into a format that can be used.

  Original format:
  Port 0 - 2-4, 2-8, 1-4, 1-8
  Port 1 - 2-3, 2-7, 1-3, 1-7
  Port 2 - 2-2, 2-6, 1-2, 1-6
  Port 3 - 2-1, 2-5, 1-1, 1-5
  MAME format:
  Port 6 - 1-1, 1-2, 1-3, 1-4, 1-5, 1-6, 1-7, 1-8
  Port 7 - 2-1, 2-2, 2-3, 2-4, 2-5, 2-6, 2-7, 2-8
***************************************************************************/
static READ_HANDLER( segar_ports_r )
{
	int dip1, dip2;

	dip1 = input_port_6_r(offset);
	dip2 = input_port_7_r(offset);

	switch(offset)
	{
		case 0:
		   return ((input_port_0_r(0) & 0xF0) | ((dip2 & 0x08)>>3) | ((dip2 & 0x80)>>6) |
						((dip1 & 0x08)>>1) | ((dip1 & 0x80)>>4));
		case 1:
		   return ((input_port_1_r(0) & 0xF0) | ((dip2 & 0x04)>>2) | ((dip2 & 0x40)>>5) |
						((dip1 & 0x04)>>0) | ((dip1 & 0x40)>>3));
		case 2:
		   return ((input_port_2_r(0) & 0xF0) | ((dip2 & 0x02)>>1) | ((dip2 & 0x20)>>4) |
						((dip1 & 0x02)<<1) | ((dip1 & 0x20)>>2));
		case 3:
		   return ((input_port_3_r(0) & 0xF0) | ((dip2 & 0x01)>>0) | ((dip2 & 0x10)>>3) |
						((dip1 & 0x01)<<2) | ((dip1 & 0x10)>>1));
		case 4:
		   return input_port_4_r(0);
	}

	return 0;
}


/***************************************************************************
 Main memory handlers
***************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xc7ff, MRA_ROM },
	{ 0xc800, 0xcfff, MRA_RAM },    /* Misc RAM */
	{ 0xe000, 0xe3ff, MRA_RAM },
	{ 0xe400, 0xe7ff, MRA_RAM },  /* Used by at least Monster Bash? */
	{ 0xe800, 0xefff, MRA_RAM },
	{ 0xf000, 0xf03f, MRA_RAM },     /* Dynamic color table */
	{ 0xf040, 0xf07f, MRA_RAM },    /* Dynamic color table for background (Monster Bash)*/
	{ 0xf080, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xffff, segar_w, &segar_mem },
	{ 0xe000, 0xe3ff, MWA_RAM, &videoram, &videoram_size },    /* handled by */
	{ 0xe800, 0xefff, MWA_RAM, &segar_characterram },    	/* the above, */
	{ 0xf000, 0xf03f, MWA_RAM, &segar_mem_colortable },     /* here only */
	{ 0xf040, 0xf07f, MWA_RAM, &segar_mem_bcolortable },    /* to initialize */
	{ 0xf800, 0xffff, MWA_RAM, &segar_characterram2 },    	/* the pointers */
	{ -1 }
};

static struct MemoryReadAddress sindbadm_readmem[] =
{
	{ 0x0000, 0xc7ff, MRA_ROM },
	{ 0xc800, 0xcfff, MRA_RAM },    /* Misc RAM */
	{ 0xe000, 0xe3ff, MRA_RAM },
	{ 0xe400, 0xe7ff, MRA_RAM },  /* Used by at least Monster Bash? */
	{ 0xe800, 0xefff, MRA_RAM },
	{ 0xf000, 0xf03f, MRA_RAM },    /* NOTE, the two color tables are flipped! */
	{ 0xf040, 0xf07f, MRA_RAM },
	{ 0xf080, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sindbadm_writemem[] =
{
	{ 0x0000, 0xc7ff, MWA_ROM },
	{ 0xc800, 0xcfff, MWA_RAM },
	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },
	{ 0xe400, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xefff, segar_characterram_w, &segar_characterram },
	{ 0xf000, 0xf03f, segar_bcolortable_w, &segar_mem_bcolortable },    /* NOTE, the two color tables are flipped! */
	{ 0xf040, 0xf07f, segar_colortable_w, &segar_mem_colortable },
	{ 0xf080, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xffff, segar_characterram2_w, &segar_characterram2 },
	{ -1 }  /* end of table */
};


static struct IOReadPort readport[] =
{
//{0x3f, 0x3f, MRA_NOP }, /* Pig Newton - read from 1D87 */
	{ 0x0e, 0x0e, monsterb_audio_8255_r },
	{ 0x81, 0x81, input_port_8_r },     /* only used by Sindbad Mystery */
	{ 0xf8, 0xfc, segar_ports_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort astrob_writeport[] =
{
	{ 0x38, 0x38, astrob_speech_port_w },
	{ 0x3e, 0x3f, astrob_audio_ports_w },
	{ 0xbf, 0xbf, segar_video_port_w }, /* bit0=cocktail flip, bit1=write to color RAM, bit2=always on? */
	{ -1 }  /* end of table */
};

static struct IOWritePort spaceod_writeport[] =
{
	{ 0x08, 0x08, spaceod_back_port_w },
	{ 0x09, 0x09, spaceod_backshift_clear_w },
	{ 0x0a, 0x0a, spaceod_backshift_w },
	{ 0x0b, 0x0c, spaceod_nobackfill_w }, /* I'm not sure what these ports really do */
	{ 0x0d, 0x0d, spaceod_backfill_w },
	{ 0x0e, 0x0f, spaceod_audio_ports_w },
	{ 0xbf, 0xbf, segar_video_port_w }, /* bit0=cocktail flip, bit1=write to color RAM, bit2=always on? */
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport_005[] =
{
	{ 0xbf, 0xbf, segar_video_port_w }, /* bit0=cocktail flip, bit1=write to color RAM, bit2=always on? */
	{ -1 }  /* end of table */
};

static struct IOWritePort monsterb_writeport[] =
{
	{ 0x0c, 0x0f, monsterb_audio_8255_w },
	{ 0xbc, 0xbc, monsterb_back_port_w },
	{ 0xbf, 0xbf, segar_video_port_w }, /* bit0=cocktail flip, bit1=write to color RAM, bit2=always on? */
	{ -1 }  /* end of table */
};

static struct IOWritePort pignewt_writeport[] =
{
	{ 0xb4, 0xb5, pignewt_back_color_w },   /* Just guessing */
	{ 0xb8, 0xbc, pignewt_back_ports_w },   /* Just guessing */
	{ 0xbe, 0xbe, MWA_NOP },    		/* probably some type of music register */
	{ 0xbf, 0xbf, segar_video_port_w }, /* bit0=cocktail flip, bit1=write to color RAM, bit2=always on? */
	{ -1 }  /* end of table */
};

static WRITE_HANDLER( sindbadm_soundport_w )
{
	soundlatch_w(0,data);
	cpu_cause_interrupt(1,Z80_NMI_INT);
	/* spin for a while to let the Z80 read the command (fixes hanging sound in Regulus) */
	cpu_spinuntil_time(TIME_IN_USEC(50));
}

/* the data lines are flipped */
static WRITE_HANDLER( sindbadm_SN76496_0_w )
{
	int flipped = ((data >> 7) & 0x01) | ((data >> 5) & 0x02) | ((data >> 3) & 0x04) | ((data >> 1) & 0x08) |
			      ((data << 1) & 0x10) | ((data << 3) & 0x20) | ((data << 5) & 0x40) | ((data << 7) & 0x80);

	SN76496_0_w(offset, flipped);
}

static WRITE_HANDLER( sindbadm_SN76496_1_w )
{
	int flipped = ((data >> 7) & 0x01) | ((data >> 5) & 0x02) | ((data >> 3) & 0x04) | ((data >> 1) & 0x08) |
			      ((data << 1) & 0x10) | ((data << 3) & 0x20) | ((data << 5) & 0x40) | ((data << 7) & 0x80);

	SN76496_1_w(offset, flipped);
}


static struct IOWritePort sindbadm_writeport[] =
{
//      { 0x00, 0x00, ???_w }, /* toggles on and off immediately (0x01, 0x00) */
	{ 0x41, 0x41, sindbadm_back_port_w },
	{ 0x43, 0x43, segar_video_port_w }, /* bit0=cocktail flip, bit1=write to color RAM, bit2=always on? */
	{ 0x80, 0x80, sindbadm_soundport_w },    /* sound commands */
	{ -1 }  /* end of table */
};


/***************************************************************************
 Sound memory handlers
***************************************************************************/

static struct MemoryReadAddress speech_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress speech_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct IOReadPort speech_readport[] =
{
	{ -1 }  /* end of table */
};

static struct IOWritePort speech_writeport[] =
{
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress monsterb_7751_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress monsterb_7751_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct IOReadPort monsterb_7751_readport[] =
{
	{ I8039_t1,  I8039_t1,  monsterb_sh_t1_r },
	{ I8039_p2,  I8039_p2,  monsterb_sh_command_r },
	{ I8039_bus, I8039_bus, monsterb_sh_rom_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort monsterb_7751_writeport[] =
{
	{ I8039_p1, I8039_p1, monsterb_sh_dac_w },
	{ I8039_p2, I8039_p2, monsterb_sh_busy_w },
	{ I8039_p4, I8039_p4, monsterb_sh_offset_a0_a3_w },
	{ I8039_p5, I8039_p5, monsterb_sh_offset_a4_a7_w },
	{ I8039_p6, I8039_p6, monsterb_sh_offset_a8_a11_w },
	{ I8039_p7, I8039_p7, monsterb_sh_rom_select_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sindbadm_sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sindbadm_sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa000, 0xa003, sindbadm_SN76496_0_w },    /* the four addresses are written */
	{ 0xc000, 0xc003, sindbadm_SN76496_1_w },    /* in sequence */
	{ -1 }  /* end of table */
};


/***************************************************************************
Input Ports
***************************************************************************/

/* This fake input port is used for DIP Switch 2
   For all games except Sindbad Mystery that has different coinage */
#define COINAGE PORT_START \
	PORT_DIPNAME( 0x0f, 0x0c, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x09, "2 Coins/1 Credit 5/3" ) \
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 4/3" ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x0d, "1 Coin/1 Credit 5/6" ) \
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 4/5" ) \
	PORT_DIPSETTING(    0x0b, "1 Coin/1 Credit 2/3" ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x0f, "1 Coin/2 Credits 4/9" ) \
	PORT_DIPSETTING(    0x07, "1 Coin/2 Credits 5/11" ) \
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_6C ) ) \
	PORT_DIPNAME( 0xf0, 0xc0, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x90, "2 Coins/1 Credit 5/3" ) \
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 4/3" ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0xd0, "1 Coin/1 Credit 5/6" ) \
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 4/5" ) \
	PORT_DIPSETTING(    0xb0, "1 Coin/1 Credit 2/3" ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0xf0, "1 Coin/2 Credits 4/9" ) \
	PORT_DIPSETTING(    0x70, "1 Coin/2 Credits 5/11" ) \
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )


INPUT_PORTS_START( astrob )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_BUTTON2, "Warp", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL, "Warp", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Demo Speech" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0xc0, "5" )

	COINAGE

INPUT_PORTS_END


INPUT_PORTS_START( astrob2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_BUTTON2, "Warp", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL, "Warp", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Demo Speech" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "3" )
  //PORT_DIPSETTING(    0x40, "3" )
  //PORT_DIPSETTING(    0xc0, "3" )

	COINAGE

INPUT_PORTS_END


INPUT_PORTS_START( astrob1 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_BUTTON2, "Warp", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL, "Warp", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0xc0, "5" )

	COINAGE

INPUT_PORTS_END


INPUT_PORTS_START( 005 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	/* better test those impulse */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0xc0, "6" )

	COINAGE

INPUT_PORTS_END


INPUT_PORTS_START( monsterb )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, "Zap", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL)
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL, "Zap", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 1 */
	PORT_BITX( 0x01,    0x01, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "10000" )
	PORT_DIPSETTING(    0x02, "20000" )
	PORT_DIPSETTING(    0x06, "40000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x18, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0xc0, "6" )

	COINAGE

INPUT_PORTS_END

INPUT_PORTS_START( spaceod )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON2, "Bomb", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 1 */
	PORT_BITX( 0x01,    0x01, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x08, "40000" )
	PORT_DIPSETTING(    0x10, "60000" )
	PORT_DIPSETTING(    0x18, "80000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0xc0, "6" )

	COINAGE

INPUT_PORTS_END


INPUT_PORTS_START( pignewt )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL)

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPSETTING(    0x30, "6" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	COINAGE

INPUT_PORTS_END


INPUT_PORTS_START( pignewta )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPSETTING(    0x30, "6" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	COINAGE

INPUT_PORTS_END



INPUT_PORTS_START( sindbadm )
	PORT_START      /* IN0 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 3 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0xFF, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 1 */
	PORT_BITX( 0x01,    0x01, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0xc0, "6" )

	PORT_START      /* FAKE */
	/* This fake input port is used for DIP Switch 2 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 5/3" )
	PORT_DIPSETTING(    0x04, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x02, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )

	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 5/3" )
	PORT_DIPSETTING(    0x40, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x20, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )

	PORT_START      /* IN8 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters? */
	2,      /* 2 bits per pixel */
	{ 0x1000*8, 0 },    /* separated by 0x1000 bytes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout backlayout =
{
	8,8,    /* 8*8 characters */
	256,  /* 256 characters per scene, 4 scenes */
	2,      /* 2 bits per pixel */
	{ 0x2000*8, 0 },    /* separated by 0x2000 bytes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spaceod_layout =
{
	8,8,   /* 16*8 characters */
	256,    /* 256 characters */
	6,      /* 6 bits per pixel */
	{ 0, 0x1000*8, 0x2000*8, 0x3000*8, 0x4000*8, 0x5000*8 },    /* separated by 0x1000 bytes (1 EPROM per bit) */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 16 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_CPU1, 0xe800, &charlayout, 0x01, 0x10 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo monsterb_gfxdecodeinfo[] =
{
	{ REGION_CPU1, 0xe800, &charlayout, 0x01, 0x10 },
	{ REGION_GFX1, 0x0000, &backlayout, 0x41, 0x10 },
	{ REGION_GFX1, 0x0800, &backlayout, 0x41, 0x10 },
	{ REGION_GFX1, 0x1000, &backlayout, 0x41, 0x10 },
	{ REGION_GFX1, 0x1800, &backlayout, 0x41, 0x10 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo spaceod_gfxdecodeinfo[] =
{
	{ REGION_CPU1, 0xe800, &charlayout,     0x01, 0x10 },
	{ REGION_GFX1, 0x0000, &spaceod_layout, 0x41, 1 },
	{ REGION_GFX1, 0x0800, &spaceod_layout, 0x41, 1 },
	{ -1 } /* end of array */
};



static struct Samplesinterface astrob_samples_interface =
{
	12,    /* 12 channels */
	25,    /* volume */
	astrob_sample_names
};

/* TODO: someday this will become a speech synthesis interface */
static struct CustomSound_interface astrob_custom_interface =
{
	astrob_speech_sh_start,
	0,
	astrob_speech_sh_update
};

static struct MachineDriver machine_driver_astrob =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3867120,    /* 3.86712 Mhz ??? */
			readmem,writemem,readport,astrob_writeport,
			segar_interrupt,1
		},
		{
			CPU_I8035 | CPU_AUDIO_CPU,
			3120000/15,    /* 3.12Mhz crystal ??? */
			speech_readmem,speech_writemem,speech_readport,speech_writeport,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	16*4+1,16*4+1,      // 16 2-bit colors + 1 transparent black
	segar_init_colors,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	segar_vh_start,
	generic_vh_stop,
	segar_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&astrob_custom_interface
		},
		{
			SOUND_SAMPLES,
			&astrob_samples_interface
		}
	}
};

static struct Samplesinterface spaceod_samples_interface =
{
	12,    /* 12 channels */
	25,    /* volume */
	spaceod_sample_names
};

static struct MachineDriver machine_driver_spaceod =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3867120,    /* 3.86712 Mhz ??? */
			readmem,writemem,readport,spaceod_writeport,
			segar_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	spaceod_gfxdecodeinfo,
	16*4*2+1,16*4*2+1,          // 16 2-bit colors for foreground, 1 6-bit color for background
	segar_init_colors,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	spaceod_vh_start,
	spaceod_vh_stop,
	spaceod_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&spaceod_samples_interface
		}
	}
};

static struct Samplesinterface samples_interface_005 =
{
	12,    /* 12 channels */
	25,    /* volume */
	s005_sample_names
};

static struct MachineDriver machine_driver_005 =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3867120,    /* 3.86712 Mhz ??? */
			readmem,writemem,readport,writeport_005,
			segar_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	16*4+1,16*4+1,      // 16 2-bit colors for foreground and background
	segar_init_colors,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	segar_vh_start,
	generic_vh_stop,
	segar_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&samples_interface_005
		}
	}
};

static struct Samplesinterface monsterb_samples_interface =
{
	2,    /* 2 channels */
	25,    /* volume */
	monsterb_sample_names
};

static struct DACinterface monsterb_dac_interface =
{
	1,
	{ 100 }
};

static struct TMS36XXinterface monsterb_tms3617_interface =
{
	1,
    { 50 },         /* mixing levels */
	{ TMS3617 },	/* TMS36xx subtype(s) */
	{ 247 },		/* base clock (one octave below A) */
	{ {0.5,0.5,0.5,0.5,0.5,0.5} }  /* decay times of voices */
};

static struct MachineDriver machine_driver_monsterb =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3867120,    /* 3.86712 Mhz ??? */
			readmem,writemem,readport,monsterb_writeport,
			segar_interrupt,1
		},
		{
			CPU_N7751 | CPU_AUDIO_CPU,
			6000000/15,    /* 6Mhz crystal */
			monsterb_7751_readmem,monsterb_7751_writemem,monsterb_7751_readport,monsterb_7751_writeport,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	monsterb_gfxdecodeinfo,
	16*4*2+1,16*4*2+1,          // 16 2-bit colors for foreground and background
	segar_init_colors,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	monsterb_vh_start,
	generic_vh_stop,
	monsterb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_TMS36XX,
			&monsterb_tms3617_interface
		},
		{
			SOUND_SAMPLES,
			&monsterb_samples_interface
		},
		{
			SOUND_DAC,
			&monsterb_dac_interface
		}
	}
};

static struct MachineDriver machine_driver_pignewt =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3867120,    /* 3.86712 Mhz ??? */
			readmem,writemem,readport,pignewt_writeport,
			segar_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	monsterb_gfxdecodeinfo,
	16*4*2+1,16*4*2+1,          // 16 2-bit colors for foreground and background
	segar_init_colors,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	monsterb_vh_start,
	generic_vh_stop,
	sindbadm_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0
};


static struct SN76496interface sn76496_interface =
{
	2,          /* 2 chips */
	{ 4000000, 2000000 },    /* I'm assuming that the sound board is the same as System 1 */
	{ 100, 100 }
};

static struct MachineDriver machine_driver_sindbadm =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,    /* 3.072 Mhz ? */
			sindbadm_readmem,sindbadm_writemem,readport,sindbadm_writeport,
			segar_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,    /* 4 Mhz ? - see system1.c */
			sindbadm_sound_readmem,sindbadm_sound_writemem,0,0,
			interrupt,4		     /* NMIs are caused by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	monsterb_gfxdecodeinfo,
	16*4*2+1,16*4*2+1,          // 16 2-bit colors for foreground and background
	segar_init_colors,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	monsterb_vh_start,
	generic_vh_stop,
	sindbadm_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		}
	}
};





ROM_START( astrob )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "829b",	  0x0000, 0x0800, 0x14ae953c ) /* U25 */
	ROM_LOAD( "907a",     0x0800, 0x0800, 0xa9aaaf38 ) /* U1 */
	ROM_LOAD( "908a",     0x1000, 0x0800, 0x897f2b87 ) /* U2 */
	ROM_LOAD( "909a",     0x1800, 0x0800, 0x55a339e6 ) /* U3 */
	ROM_LOAD( "910a",     0x2000, 0x0800, 0x7972b60a ) /* U4 */
	ROM_LOAD( "911a",     0x2800, 0x0800, 0xaf87520f ) /* U5 */
	ROM_LOAD( "912a",     0x3000, 0x0800, 0xb656f929 ) /* U6 */
	ROM_LOAD( "913a",     0x3800, 0x0800, 0x321074b3 ) /* U7 */
	ROM_LOAD( "914a",     0x4000, 0x0800, 0x90d2493e ) /* U8 */
	ROM_LOAD( "915a",     0x4800, 0x0800, 0xaaf828d1 ) /* U9 */
	ROM_LOAD( "916a",     0x5000, 0x0800, 0x56d92ab9 ) /* U10 */
	ROM_LOAD( "917a",     0x5800, 0x0800, 0x9dcdaf2d ) /* U11 */
	ROM_LOAD( "918a",     0x6000, 0x0800, 0xc9d09655 ) /* U12 */
	ROM_LOAD( "919a",     0x6800, 0x0800, 0x448bd318 ) /* U13 */
	ROM_LOAD( "920a",     0x7000, 0x0800, 0x3524a383 ) /* U14 */
	ROM_LOAD( "921a",     0x7800, 0x0800, 0x98c14834 ) /* U15 */
	ROM_LOAD( "922a",     0x8000, 0x0800, 0x4311513c ) /* U16 */
	ROM_LOAD( "923a",     0x8800, 0x0800, 0x50f0462c ) /* U17 */
	ROM_LOAD( "924a",     0x9000, 0x0800, 0x120a39c7 ) /* U18 */
	ROM_LOAD( "925a",     0x9800, 0x0800, 0x790a7f4e ) /* U19 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for speech code */
	ROM_LOAD( "808b",     0x0000, 0x0800, 0x5988c767 ) /* U7 */
	ROM_LOAD( "809a",     0x0800, 0x0800, 0x893f228d ) /* U6 */
	ROM_LOAD( "810",      0x1000, 0x0800, 0xff0163c5 ) /* U5 */
	ROM_LOAD( "811",      0x1800, 0x0800, 0x219f3978 ) /* U4 */
	ROM_LOAD( "812a",     0x2000, 0x0800, 0x410ad0d2 ) /* U3 */
ROM_END

ROM_START( astrob2 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "829b",     0x0000, 0x0800, 0x14ae953c ) /* U25 */
	ROM_LOAD( "888",      0x0800, 0x0800, 0x42601744 ) /* U1 */
	ROM_LOAD( "889",      0x1000, 0x0800, 0xdd9ab173 ) /* U2 */
	ROM_LOAD( "890",      0x1800, 0x0800, 0x26f5b4cf ) /* U3 */
	ROM_LOAD( "891",      0x2000, 0x0800, 0x6437c95f ) /* U4 */
	ROM_LOAD( "892",      0x2800, 0x0800, 0x2d3c949b ) /* U5 */
	ROM_LOAD( "893",      0x3000, 0x0800, 0xccdb1a76 ) /* U6 */
	ROM_LOAD( "894",      0x3800, 0x0800, 0x66ae5ced ) /* U7 */
	ROM_LOAD( "895",      0x4000, 0x0800, 0x202cf3a3 ) /* U8 */
	ROM_LOAD( "896",      0x4800, 0x0800, 0xb603fe23 ) /* U9 */
	ROM_LOAD( "897",      0x5000, 0x0800, 0x989198c6 ) /* U10 */
	ROM_LOAD( "898",      0x5800, 0x0800, 0xef2bab04 ) /* U11 */
	ROM_LOAD( "899",      0x6000, 0x0800, 0xe0d189ee ) /* U12 */
	ROM_LOAD( "900",      0x6800, 0x0800, 0x682d4604 ) /* U13 */
	ROM_LOAD( "901",      0x7000, 0x0800, 0x9ed11c61 ) /* U14 */
	ROM_LOAD( "902",      0x7800, 0x0800, 0xb4d6c330 ) /* U15 */
	ROM_LOAD( "903",      0x8000, 0x0800, 0x84acc38c ) /* U16 */
	ROM_LOAD( "904",      0x8800, 0x0800, 0x5eba3097 ) /* U17 */
	ROM_LOAD( "905",      0x9000, 0x0800, 0x4f08f9f4 ) /* U18 */
	ROM_LOAD( "906",      0x9800, 0x0800, 0x58149df1 ) /* U19 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for speech code */
	ROM_LOAD( "808b",     0x0000, 0x0800, 0x5988c767 ) /* U7 */
	ROM_LOAD( "809a",     0x0800, 0x0800, 0x893f228d ) /* U6 */
	ROM_LOAD( "810",      0x1000, 0x0800, 0xff0163c5 ) /* U5 */
	ROM_LOAD( "811",      0x1800, 0x0800, 0x219f3978 ) /* U4 */
	ROM_LOAD( "812a",     0x2000, 0x0800, 0x410ad0d2 ) /* U3 */
ROM_END

ROM_START( astrob1 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "829",      0x0000, 0x0800, 0x5f66046e ) /* U25 */
	ROM_LOAD( "837",      0x0800, 0x0800, 0xce9c3763 ) /* U1 */
	ROM_LOAD( "838",      0x1000, 0x0800, 0x3557289e ) /* U2 */
	ROM_LOAD( "839",      0x1800, 0x0800, 0xc88bda24 ) /* U3 */
	ROM_LOAD( "840",      0x2000, 0x0800, 0x24c9fe23 ) /* U4 */
	ROM_LOAD( "841",      0x2800, 0x0800, 0xf153c683 ) /* U5 */
	ROM_LOAD( "842",      0x3000, 0x0800, 0x4c5452b2 ) /* U6 */
	ROM_LOAD( "843",      0x3800, 0x0800, 0x673161a6 ) /* U7 */
	ROM_LOAD( "844",      0x4000, 0x0800, 0x6bfc59fd ) /* U8 */
	ROM_LOAD( "845",      0x4800, 0x0800, 0x018623f3 ) /* U9 */
	ROM_LOAD( "846",      0x5000, 0x0800, 0x4d7c5fb3 ) /* U10 */
	ROM_LOAD( "847",      0x5800, 0x0800, 0x24d1d50a ) /* U11 */
	ROM_LOAD( "848",      0x6000, 0x0800, 0x1c145541 ) /* U12 */
	ROM_LOAD( "849",      0x6800, 0x0800, 0xd378c169 ) /* U13 */
	ROM_LOAD( "850",      0x7000, 0x0800, 0x9da673ae ) /* U14 */
	ROM_LOAD( "851",      0x7800, 0x0800, 0x3d4cf9f0 ) /* U15 */
	ROM_LOAD( "852",      0x8000, 0x0800, 0xaf88a97e ) /* U16 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for speech code */
	ROM_LOAD( "808b",     0x0000, 0x0800, 0x5988c767 ) /* U7 */
	ROM_LOAD( "809a",     0x0800, 0x0800, 0x893f228d ) /* U6 */
	ROM_LOAD( "810",      0x1000, 0x0800, 0xff0163c5 ) /* U5 */
	ROM_LOAD( "811",      0x1800, 0x0800, 0x219f3978 ) /* U4 */
	ROM_LOAD( "812a",     0x2000, 0x0800, 0x410ad0d2 ) /* U3 */
ROM_END

ROM_START( 005 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "1346b.u25",    0x0000, 0x0800, 0x8e68533e ) /* U25 */
	ROM_LOAD( "5092.u1",      0x0800, 0x0800, 0x29e10a81 ) /* U1 */
	ROM_LOAD( "5093.u2",      0x1000, 0x0800, 0xe1edc3df ) /* U2 */
	ROM_LOAD( "5094.u3",      0x1800, 0x0800, 0x995773bb ) /* U3 */
	ROM_LOAD( "5095.u4",      0x2000, 0x0800, 0xf887f575 ) /* U4 */
	ROM_LOAD( "5096.u5",      0x2800, 0x0800, 0x5545241e ) /* U5 */
	ROM_LOAD( "5097.u6",      0x3000, 0x0800, 0x428edb54 ) /* U6 */
	ROM_LOAD( "5098.u7",      0x3800, 0x0800, 0x5bcb9d63 ) /* U7 */
	ROM_LOAD( "5099.u8",      0x4000, 0x0800, 0x0ea24ba3 ) /* U8 */
	ROM_LOAD( "5100.u9",      0x4800, 0x0800, 0xa79af131 ) /* U9 */
	ROM_LOAD( "5101.u10",     0x5000, 0x0800, 0x8a1cdae0 ) /* U10 */
	ROM_LOAD( "5102.u11",     0x5800, 0x0800, 0x70826a15 ) /* U11 */
	ROM_LOAD( "5103.u12",     0x6000, 0x0800, 0x7f80c5b0 ) /* U12 */
	ROM_LOAD( "5104.u13",     0x6800, 0x0800, 0x0140930e ) /* U13 */
	ROM_LOAD( "5105.u14",     0x7000, 0x0800, 0x17807a05 ) /* U14 */
	ROM_LOAD( "5106.u15",     0x7800, 0x0800, 0xc7cdfa9d ) /* U15 */
	ROM_LOAD( "5107.u16",     0x8000, 0x0800, 0x95f8a2e6 ) /* U16 */
	ROM_LOAD( "5108.u17",     0x8800, 0x0800, 0xd371cacd ) /* U17 */
	ROM_LOAD( "5109.u18",     0x9000, 0x0800, 0x48a20617 ) /* U18 */
	ROM_LOAD( "5110.u19",     0x9800, 0x0800, 0x7d26111a ) /* U19 */
	ROM_LOAD( "5111.u20",     0xa000, 0x0800, 0xa888e175 ) /* U20 */

	ROM_REGION( 0x0800, REGION_SOUND1 )      /* 2k for sound */
	ROM_LOAD( "epr-1286.16",  0x0000, 0x0800, 0xfbe0d501 )
ROM_END

ROM_START( monsterb )
	ROM_REGION( 0x14000, REGION_CPU1 )     /* 64k for code + space for background */
	ROM_LOAD( "1778cpu.bin",  0x0000, 0x0800, 0x19761be3 ) /* U25 */
	ROM_LOAD( "1779.bin",     0x0800, 0x0800, 0x5b67dc4c ) /* U1 */
	ROM_LOAD( "1780.bin",     0x1000, 0x0800, 0xfac5aac6 ) /* U2 */
	ROM_LOAD( "1781.bin",     0x1800, 0x0800, 0x3b104103 ) /* U3 */
	ROM_LOAD( "1782.bin",     0x2000, 0x0800, 0xc1523553 ) /* U4 */
	ROM_LOAD( "1783.bin",     0x2800, 0x0800, 0xe0ea08c5 ) /* U5 */
	ROM_LOAD( "1784.bin",     0x3000, 0x0800, 0x48976d11 ) /* U6 */
	ROM_LOAD( "1785.bin",     0x3800, 0x0800, 0x297d33ae ) /* U7 */
	ROM_LOAD( "1786.bin",     0x4000, 0x0800, 0xef94c8f4 ) /* U8 */
	ROM_LOAD( "1787.bin",     0x4800, 0x0800, 0x1b62994e ) /* U9 */
	ROM_LOAD( "1788.bin",     0x5000, 0x0800, 0xa2e32d91 ) /* U10 */
	ROM_LOAD( "1789.bin",     0x5800, 0x0800, 0x08a172dc ) /* U11 */
	ROM_LOAD( "1790.bin",     0x6000, 0x0800, 0x4e320f9d ) /* U12 */
	ROM_LOAD( "1791.bin",     0x6800, 0x0800, 0x3b4cba31 ) /* U13 */
	ROM_LOAD( "1792.bin",     0x7000, 0x0800, 0x7707b9f8 ) /* U14 */
	ROM_LOAD( "1793.bin",     0x7800, 0x0800, 0xa5d05155 ) /* U15 */
	ROM_LOAD( "1794.bin",     0x8000, 0x0800, 0xe4813da9 ) /* U16 */
	ROM_LOAD( "1795.bin",     0x8800, 0x0800, 0x4cd6ed88 ) /* U17 */
	ROM_LOAD( "1796.bin",     0x9000, 0x0800, 0x9f141a42 ) /* U18 */
	ROM_LOAD( "1797.bin",     0x9800, 0x0800, 0xec14ad16 ) /* U19 */
	ROM_LOAD( "1798.bin",     0xa000, 0x0800, 0x86743a4f ) /* U20 */
	ROM_LOAD( "1799.bin",     0xa800, 0x0800, 0x41198a83 ) /* U21 */
	ROM_LOAD( "1800.bin",     0xb000, 0x0800, 0x6a062a04 ) /* U22 */
	ROM_LOAD( "1801.bin",     0xb800, 0x0800, 0xf38488fe ) /* U23 */

	ROM_REGION( 0x1000, REGION_CPU2 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* background graphics */
	ROM_LOAD( "1516.bin",     0x0000, 0x2000, 0xe93a2281 ) /* ??? */
	ROM_LOAD( "1517.bin",     0x2000, 0x2000, 0x1e589101 ) /* ??? */

	ROM_REGION( 0x2000, REGION_SOUND1 )      /* 8k for sound */
	ROM_LOAD( "1543snd.bin",  0x0000, 0x1000, 0xb525ce8f ) /* U19 */
	ROM_LOAD( "1544snd.bin",  0x1000, 0x1000, 0x56c79fb0 ) /* U23 */

	ROM_REGION( 0x0020, REGION_SOUND2 )      /* 32 bytes for sound PROM */
	ROM_LOAD( "pr1512.u31",   0x0000, 0x0020, 0x414ebe9b )  /* U31 */

	ROM_REGION( 0x2000, REGION_USER1 )		      /* background charmaps */
	ROM_LOAD( "1518a.bin",    0x0000, 0x2000, 0x2d5932fe ) /* ??? */
ROM_END

ROM_START( spaceod )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "so-959.dat",   0x0000, 0x0800, 0xbbae3cd1 ) /* U25 */
	ROM_LOAD( "so-941.dat",   0x0800, 0x0800, 0x8b63585a ) /* U1 */
	ROM_LOAD( "so-942.dat",   0x1000, 0x0800, 0x93e7d900 ) /* U2 */
	ROM_LOAD( "so-943.dat",   0x1800, 0x0800, 0xe2f5dc10 ) /* U3 */
	ROM_LOAD( "so-944.dat",   0x2000, 0x0800, 0xb5ab01e9 ) /* U4 */
	ROM_LOAD( "so-945.dat",   0x2800, 0x0800, 0x6c5fa1b1 ) /* U5 */
	ROM_LOAD( "so-946.dat",   0x3000, 0x0800, 0x4cef25d6 ) /* U6 */
	ROM_LOAD( "so-947.dat",   0x3800, 0x0800, 0x7220fc42 ) /* U7 */
	ROM_LOAD( "so-948.dat",   0x4000, 0x0800, 0x94bcd726 ) /* U8 */
	ROM_LOAD( "so-949.dat",   0x4800, 0x0800, 0xe11e7034 ) /* U9 */
	ROM_LOAD( "so-950.dat",   0x5000, 0x0800, 0x70a7a3b4 ) /* U10 */
	ROM_LOAD( "so-951.dat",   0x5800, 0x0800, 0xf5f0d3f9 ) /* U11 */
	ROM_LOAD( "so-952.dat",   0x6000, 0x0800, 0x5bf19a12 ) /* U12 */
	ROM_LOAD( "so-953.dat",   0x6800, 0x0800, 0x8066ac83 ) /* U13 */
	ROM_LOAD( "so-954.dat",   0x7000, 0x0800, 0x44ed6a0d ) /* U14 */
	ROM_LOAD( "so-955.dat",   0x7800, 0x0800, 0xb5e2748d ) /* U15 */
	ROM_LOAD( "so-956.dat",   0x8000, 0x0800, 0x97de45a9 ) /* U16 */
	ROM_LOAD( "so-957.dat",   0x8800, 0x0800, 0xc14b98c4 ) /* U17 */
	ROM_LOAD( "so-958.dat",   0x9000, 0x0800, 0x4c0a7242 ) /* U18 */

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* background graphics */
	ROM_LOAD( "epr-13.dat",   0x0000, 0x1000, 0x74bd7f9a )
	ROM_LOAD( "epr-14.dat",   0x1000, 0x1000, 0xd2ebd915 )
	ROM_LOAD( "epr-15.dat",   0x2000, 0x1000, 0xae0e5d71 )
	ROM_LOAD( "epr-16.dat",   0x3000, 0x1000, 0xacdf203e )
	ROM_LOAD( "epr-17.dat",   0x4000, 0x1000, 0x6c7490c0 )
	ROM_LOAD( "epr-18.dat",   0x5000, 0x1000, 0x24a81c04 )

	ROM_REGION( 0x4000, REGION_USER1 )		      /* background charmaps */
	ROM_LOAD( "epr-09.dat",  0x0000, 0x1000, 0xa87bfc0a )
	ROM_LOAD( "epr-10.dat",  0x1000, 0x1000, 0x8ce88100 )
	ROM_LOAD( "epr-11.dat",  0x2000, 0x1000, 0x1bdbdab5 )
	ROM_LOAD( "epr-12.dat",  0x3000, 0x1000, 0x629a4a1f )
ROM_END

ROM_START( pignewt )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "cpu.u25",    0x0000, 0x0800, 0x00000000 ) /* U25 */
	ROM_LOAD( "1888c",      0x0800, 0x0800, 0xfd18ed09 ) /* U1 */
	ROM_LOAD( "1889c",      0x1000, 0x0800, 0xf633f5ff ) /* U2 */
	ROM_LOAD( "1890c",      0x1800, 0x0800, 0x22009d7f ) /* U3 */
	ROM_LOAD( "1891c",      0x2000, 0x0800, 0x1540a7d6 ) /* U4 */
	ROM_LOAD( "1892c",      0x2800, 0x0800, 0x960385d0 ) /* U5 */
	ROM_LOAD( "1893c",      0x3000, 0x0800, 0x58c5c461 ) /* U6 */
	ROM_LOAD( "1894c",      0x3800, 0x0800, 0x5817a59d ) /* U7 */
	ROM_LOAD( "1895c",      0x4000, 0x0800, 0x812f67d7 ) /* U8 */
	ROM_LOAD( "1896c",      0x4800, 0x0800, 0xcc0ecdd0 ) /* U9 */
	ROM_LOAD( "1897c",      0x5000, 0x0800, 0x7820e93b ) /* U10 */
	ROM_LOAD( "1898c",      0x5800, 0x0800, 0xe9a10ded ) /* U11 */
	ROM_LOAD( "1899c",      0x6000, 0x0800, 0xd7ddf02b ) /* U12 */
	ROM_LOAD( "1900c",      0x6800, 0x0800, 0x8deff4e5 ) /* U13 */
	ROM_LOAD( "1901c",      0x7000, 0x0800, 0x46051305 ) /* U14 */
	ROM_LOAD( "1902c",      0x7800, 0x0800, 0xcb937e19 ) /* U15 */
	ROM_LOAD( "1903c",      0x8000, 0x0800, 0x53239f12 ) /* U16 */
	ROM_LOAD( "1913c",      0x8800, 0x0800, 0x4652cb0c ) /* U17 */
	ROM_LOAD( "1914c",      0x9000, 0x0800, 0xcb758697 ) /* U18 */
	ROM_LOAD( "1915c",      0x9800, 0x0800, 0x9f3bad66 ) /* U19 */
	ROM_LOAD( "1916c",      0xa000, 0x0800, 0x5bb6f61e ) /* U20 */
	ROM_LOAD( "1917c",      0xa800, 0x0800, 0x725e2c87 ) /* U21 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* background graphics */
	ROM_LOAD( "1904c.bg",   0x0000, 0x2000, 0xe9de2c8b ) /* ??? */
	ROM_LOAD( "1905c.bg",   0x2000, 0x2000, 0xaf7cfe0b ) /* ??? */

	ROM_REGION( 0x4000, REGION_USER1 )		      /* background charmaps */
	ROM_LOAD( "1906c.bg",  0x0000, 0x1000, 0xc79d33ce ) /* ??? */
	ROM_LOAD( "1907c.bg",  0x1000, 0x1000, 0xbc839d3c ) /* ??? */
	ROM_LOAD( "1908c.bg",  0x2000, 0x1000, 0x92cb14da ) /* ??? */

	/* SOUND ROMS ARE PROBABLY MISSING! */
ROM_END

ROM_START( pignewta )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "cpu.u25",    0x0000, 0x0800, 0x00000000 ) /* U25 */
	ROM_LOAD( "1888a",      0x0800, 0x0800, 0x491c0835 ) /* U1 */
	ROM_LOAD( "1889a",      0x1000, 0x0800, 0x0dcf0af2 ) /* U2 */
	ROM_LOAD( "1890a",      0x1800, 0x0800, 0x640b8b2e ) /* U3 */
	ROM_LOAD( "1891a",      0x2000, 0x0800, 0x7b8aa07f ) /* U4 */
	ROM_LOAD( "1892a",      0x2800, 0x0800, 0xafc545cb ) /* U5 */
	ROM_LOAD( "1893a",      0x3000, 0x0800, 0x82448619 ) /* U6 */
	ROM_LOAD( "1894a",      0x3800, 0x0800, 0x4302dbfb ) /* U7 */
	ROM_LOAD( "1895a",      0x4000, 0x0800, 0x137ebaaf ) /* U8 */
	ROM_LOAD( "1896",       0x4800, 0x0800, 0x1604c811 ) /* U9 */
	ROM_LOAD( "1897",       0x5000, 0x0800, 0x3abee406 ) /* U10 */
	ROM_LOAD( "1898",       0x5800, 0x0800, 0xa96410dc ) /* U11 */
	ROM_LOAD( "1899",       0x6000, 0x0800, 0x612568a5 ) /* U12 */
	ROM_LOAD( "1900",       0x6800, 0x0800, 0x5b231cea ) /* U13 */
	ROM_LOAD( "1901",       0x7000, 0x0800, 0x3fd74b05 ) /* U14 */
	ROM_LOAD( "1902",       0x7800, 0x0800, 0xd568fc22 ) /* U15 */
	ROM_LOAD( "1903",       0x8000, 0x0800, 0x7d16633b ) /* U16 */
	ROM_LOAD( "1913",       0x8800, 0x0800, 0xfa4be04f ) /* U17 */
	ROM_LOAD( "1914",       0x9000, 0x0800, 0x08253c50 ) /* U18 */
	ROM_LOAD( "1915",       0x9800, 0x0800, 0xde786c3b ) /* U19 */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* background graphics */
	ROM_LOAD( "1904a.bg",   0x0000, 0x2000, 0x00000000 ) /* ??? */
	ROM_LOAD( "1905a.bg",   0x2000, 0x2000, 0x00000000 ) /* ??? */

	ROM_REGION( 0x4000, REGION_USER1 )		      /* background charmaps */
	/* NOTE: No background ROMs for set A have been dumped, so the
	ROMs from set C have been copied and renamed. This is to
	provide a reminder that these ROMs still need to be dumped. */
	ROM_LOAD( "1906a.bg",  0x0000, 0x1000, BADCRC(0xc79d33ce) ) /* ??? */
	ROM_LOAD( "1907a.bg",  0x1000, 0x1000, BADCRC(0xbc839d3c) ) /* ??? */
	ROM_LOAD( "1908a.bg",  0x2000, 0x1000, BADCRC(0x92cb14da) ) /* ??? */

	/* SOUND ROMS ARE PROBABLY MISSING! */
ROM_END


ROM_START( sindbadm )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr5393.new",  0x0000, 0x2000, 0x51f2e51e )
	ROM_LOAD( "epr5394.new",  0x2000, 0x2000, 0xd39ce2ee )
	ROM_LOAD( "epr5395.new",  0x4000, 0x2000, 0xb1d15c82 )
	ROM_LOAD( "epr5396.new",  0x6000, 0x2000, 0xea9d40bf )
	ROM_LOAD( "epr5397.new",  0x8000, 0x2000, 0x595d16dc )
	ROM_LOAD( "epr5398.new",  0xa000, 0x2000, 0xe57ff63c )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound cpu (Z80) */
	ROM_LOAD( "epr5400.new",  0x0000, 0x2000, 0x5114f18e )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* background graphics */
	ROM_LOAD( "epr5428.new",  0x0000, 0x2000, 0xf6044a1e )
	ROM_LOAD( "epr5429.new",  0x2000, 0x2000, 0xb23eca10 )

	ROM_REGION( 0x8000, REGION_USER1 )		      /* background charmaps */
	ROM_LOAD( "epr5424.new",  0x0000, 0x2000, 0x4bfc2e95 )
	ROM_LOAD( "epr5425.new",  0x2000, 0x2000, 0xb654841a )
	ROM_LOAD( "epr5426.new",  0x4000, 0x2000, 0x9de0da28 )
	ROM_LOAD( "epr5427.new",  0x6000, 0x2000, 0xa94f4d41 )
ROM_END


/***************************************************************************
  Security Decode "chips"
***************************************************************************/

static void init_astrob(void)
{
	/* This game uses the 315-0062 security chip */
	sega_security(62);
}

static void init_005(void)
{
	/* This game uses the 315-0070 security chip */
	sega_security(70);
}

static void init_monsterb(void)
{
	/* This game uses the 315-0082 security chip */
	sega_security(82);
}

static void init_spaceod(void)
{
	/* This game uses the 315-0063 security chip */
	sega_security(63);
}

static void init_pignewt(void)
{
	/* This game uses the 315-0063? security chip */
	sega_security(63);
}

static void init_sindbadm(void)
{
	/* This game uses an encrypted CPU */
	sindbadm_decode();
}


GAME( 1981, astrob,   0,       astrob,   astrob,   astrob,   ROT270, "Sega", "Astro Blaster (version 3)" )
GAME( 1981, astrob2,  astrob,  astrob,   astrob2,  astrob,   ROT270, "Sega", "Astro Blaster (version 2)" )
GAMEX(1981, astrob1,  astrob,  astrob,   astrob1,  astrob,   ROT270, "Sega", "Astro Blaster (version 1)", GAME_NOT_WORKING )
GAMEX(1981, 005,      0,       005,      005,      005,      ROT270, "Sega", "005", GAME_NO_SOUND )
GAME( 1982, monsterb, 0,       monsterb, monsterb, monsterb, ROT270, "Sega", "Monster Bash" )
GAME( 1981, spaceod,  0,       spaceod,  spaceod,  spaceod,  ROT270, "Sega", "Space Odyssey" )
GAMEX(1983, pignewt,  0,       pignewt,  pignewt,  pignewt,  ROT270, "Sega", "Pig Newton (version C)", GAME_NO_SOUND )
GAMEX(1983, pignewta, pignewt, pignewt,  pignewta, pignewt,  ROT270, "Sega", "Pig Newton (version A)", GAME_NO_SOUND )
GAME( 1983, sindbadm, 0,       sindbadm, sindbadm, sindbadm, ROT270, "Sega", "Sindbad Mystery" )
