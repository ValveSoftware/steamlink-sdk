#include "../vidhrdw/taito_b.cpp"

/***************************************************************************

Taito B System

heavily based on Taito F2 System driver by Brad Oliver, Andrew Prime

TODO:
- masterw: title screen is incomplete, has wrong colors and misses palette marking
- silentd: wrong scroll in attract mode, jerky background in level 1

The Taito F2 system is a fairly flexible hardware platform. It supports 4
separate layers of graphics - one 64x64 tiled scrolling background plane
of 8x8 tiles, a similar foreground plane, a sprite plane capable of handling
all the video chores by itself (used in e.g. Super Space Invaders) and a text
plane which may or may not scroll.

Sound is handled by a Z80 with a YM2610 connected to it.

The memory map for each of the games is similar but not identical.

Memory map for Rastan Saga 2 / Nastar / Nastar Warrior

CPU 1 : 68000, uses irqs 2 & 4. One of the IRQs just sets a flag which is
checked in the other IRQ routine. Could be timed to vblank...

  0x000000 - 0x07ffff : ROM
  0x200000 - 0x201fff : palette RAM, 4096 total colors (0x1000 words)
  0x400000 - 0x403fff : 64x64 foreground layer (offsets 0x0000-0x1fff tile codes; offsets 0x2000-0x3fff tile attributes)
  0x404000 - 0x807fff : 64x64 background layer (offsets 0x0000-0x1fff tile codes; offsets 0x2000-0x3fff tile attributes)
  0x408000 - 0x408fff : 64x64 text layer
  0x410000 - 0x41197f : ??k of sprite RAM (this is the range that Rastan Saga II tests at startup time)
  0x413800 - 0x413bff : foreground control RAM (413800.w - foreground x scroll, 413802.w - foreground y scroll)
  0x413c00 - 0x413fff : background control RAM (413c00.w - background x scroll, 413c02.w - background y scroll)

  0x600000 - 0x607fff : 32k of CPU RAM
  0x800000 - 0x800003 : communication with sound CPU
  0xa00000 - 0xa0000f : input ports and dipswitches (writes may be IRQ acknowledge)



*XXX.1988 Rastan Saga II (B81, , )
Ashura Blaster
Crime City
Rambo 3 (two different versions)
Tetris
Violence Fight (YM2203)
Hit The Ice (YM2203 sound)
Master of Weapons (YM2203 sound)
Puzzle Bobble
Silent Dragon

Other possible B-Sys games:
???

***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"


extern unsigned char *taitob_fscroll;
extern unsigned char *taitob_bscroll;

extern unsigned char *b_backgroundram;
extern unsigned char *b_foregroundram;
extern unsigned char *b_textram;
extern unsigned char *taitob_pixelram;

extern size_t b_backgroundram_size;
extern size_t b_foregroundram_size;
extern size_t b_textram_size;
extern size_t b_pixelram_size;

extern size_t b_paletteram_size;


/*TileMaps*/
int taitob_vh_start_color_order0 (void);
int taitob_vh_start_color_order1 (void);
int taitob_vh_start_color_order2 (void);
void taitob_vh_stop (void);

void taitob_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void ashura_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void crimec_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void tetrist_vh_screenrefresh (struct osd_bitmap *bitmap,int full_refresh);
void hitice_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void rambo3_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void puzbobb_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void qzshowby_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void masterw_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);
void silentd_vh_screenrefresh_tm (struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( taitob_text_w_tm );
WRITE_HANDLER( taitob_background_w_tm );
WRITE_HANDLER( taitob_foreground_w_tm );
READ_HANDLER ( taitob_text_r );
READ_HANDLER ( taitob_background_r );
READ_HANDLER ( taitob_foreground_r );
/*TileMaps end*/



READ_HANDLER ( taitob_pixelram_r );
WRITE_HANDLER( taitob_pixelram_w );

WRITE_HANDLER( masterw_pixelram_w );

READ_HANDLER ( taitob_text_video_control_r );
WRITE_HANDLER( taitob_text_video_control_w );
READ_HANDLER ( taitob_video_control_r );
WRITE_HANDLER( taitob_video_control_w );


READ_HANDLER( hitice_pixelram_r );
WRITE_HANDLER( hitice_pixelram_w );/*this doesn't look like a pixel layer*/


READ_HANDLER ( taitob_videoram_r );
WRITE_HANDLER( taitob_videoram_w );

WRITE_HANDLER( rastan_sound_port_w );
WRITE_HANDLER( rastan_sound_comm_w );
READ_HANDLER ( rastan_sound_comm_r );

WRITE_HANDLER( rastan_a000_w );
WRITE_HANDLER( rastan_a001_w );
READ_HANDLER ( rastan_a001_r );



static WRITE_HANDLER( bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);
	int banknum = (data - 1) & 3;

	cpu_setbank (2, &RAM [0x10000 + (banknum * 0x4000)]);
}


static READ_HANDLER( rastsag2_input_r )
{
	switch (offset)
	{
		case 0x00:
			return readinputport (3)<<8; /*DSW A*/
		case 0x02:
			return readinputport (4)<<8; /*DSW B*/
		case 0x04:
			return readinputport (0)<<8; /*player 1*/
		case 0x06:
			return readinputport (1)<<8; /*player 2*/
		case 0x0e:
			return readinputport (2)<<8; /*tilt, coins*/
		default:
            //logerror("WARNING: read input offs=%2x PC=%08x\n", offset, cpu_get_pc());
			return 0xff<<8;
	}
}

static READ_HANDLER(hitice_input_r )
{
	return ( readinputport (5)<<8 | readinputport(6) ); /*player 3 and 4*/
}

static READ_HANDLER( silentd_input_r )
{
	switch (offset)
	{
		case 0x00:
			return readinputport (3);
		case 0x02:
			return readinputport (4);
		case 0x04:
			return readinputport (0);
		case 0x06:
			return readinputport (1);

		case 0x08:
			return 0; /* ??? */

		case 0x0e:
			return readinputport (2);
		default:
            //logerror("WARNING: read input offs=%2x PC=%08x\n", offset, cpu_get_pc());
			return 0xff;
	}
}

static READ_HANDLER(silentd_input1_r )
{
	return readinputport (5);
}
static READ_HANDLER(silentd_input2_r )
{
	return readinputport (6);
}
static READ_HANDLER(silentd_input3_r )
{
	return readinputport (7);
}


static READ_HANDLER( eeprom_r );

static READ_HANDLER( puzbobb_input_r )
{
	switch (offset)
	{
		case 0x00:
			return readinputport (3)<<8; /*DSW A*/
		case 0x02:
			return ( eeprom_r(0) ) << 8; /*bit 0 - Eeprom data, other coin inputs*/
		case 0x04:
			return readinputport (0)<<8; /*player 1*/ /*tilt*/
		case 0x06:
			return readinputport (1)<<8; /*player 2*/
		case 0x08:
			return readinputport (5)<<8; /* ??? */
		case 0x0e:
			return readinputport (2)<<8; /*tilt, coins*/
		default:
            //logerror("WARNING: puzbobb read input offs=%x\n",offset);
			return 0xff;
	}
}


void rsaga2_interrupt2(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_2);
}

static int rastansaga2_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,rsaga2_interrupt2);
	return MC68000_IRQ_4;
}


void crimec_interrupt3(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_3);
}

static int crimec_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,crimec_interrupt3);
	return MC68000_IRQ_5;
}


void hitice_interrupt6(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_6);
}

static int hitice_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,hitice_interrupt6);
	return MC68000_IRQ_4;
}


void rambo3_interrupt1(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_1);
}

static int rambo3_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,rambo3_interrupt1);
	return MC68000_IRQ_6;
}


void puzbobb_interrupt5(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_5);
}

static int puzbobb_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,puzbobb_interrupt5);
	return MC68000_IRQ_3;
}

void viofight_interrupt1(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_1);
}

static int viofight_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,viofight_interrupt1);
	return MC68000_IRQ_4;
}

void masterw_interrupt4(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_4);
}

static int masterw_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,masterw_interrupt4);
	return MC68000_IRQ_5;
}


/*silentd
* int 6 - read inputs
* int 4 - ???
*/

void silentd_interrupt4(int x)
{
	cpu_cause_interrupt(0,MC68000_IRQ_6);
}

static int silentd_interrupt(void)
{
	timer_set(TIME_IN_CYCLES(5000,0),0,silentd_interrupt4);
	return MC68000_IRQ_4;
}


static WRITE_HANDLER( taitob_sound_w )
{
	if (offset == 0)
	{
		rastan_sound_port_w(0, (data>>8) & 0xff);
	}
	else if (offset == 2)
	{
		rastan_sound_comm_w(0, (data>>8) & 0xff);
	}
}

static READ_HANDLER( taitob_sound_r )
{
	if (offset == 2)
		return (rastan_sound_comm_r(0)<<8 );
	else return 0;
}


static struct MemoryReadAddress rastsag2_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x600000, 0x607fff, MRA_BANK1 },			/* Main RAM */
	{ 0x200000, 0x201fff, paletteram_word_r },	/*palette*/

	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x408fff, taitob_text_r },		/*text ram*/
	{ 0x409000, 0x409fff, MRA_BANK5 },			/*ashura only (textram continue ?)*/
	{ 0x410000, 0x41197f, taitob_videoram_r },		/*sprite ram*/
	{ 0x411980, 0x411fff, MRA_BANK6 },			/*ashura only (spriteram continue ?)*/

	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/
	{ 0x41800e, 0x41800f, taitob_video_control_r },

	{ 0xa00000, 0xa0000f, rastsag2_input_r },	/* DSW A/B, player inputs*/
	{ 0x800000, 0x800003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress rastsag2_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x600000, 0x607fff, MWA_BANK1 },	/* Main RAM */ /*ashura up to 603fff only*/

	{ 0x200000, 0x201fff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size }, /* foreground layer */
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size }, /* background layer */
	{ 0x408000, 0x408fff, taitob_text_w_tm, &b_textram, &b_textram_size }, /* text layer */
	{ 0x409000, 0x409fff, MWA_BANK5 }, /*ashura clears this area only*/

	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x411980, 0x411fff, MWA_BANK6 }, /*ashura clears this area only*/

	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll */
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll */

	{ 0x41800e, 0x41800f, taitob_video_control_w },

	{ 0x440000, 0x47ffff, taitob_pixelram_w, &taitob_pixelram, &b_pixelram_size }, /* ashura(US) pixel layer*/

	{ 0xa00000, 0xa0000f, MWA_NOP }, // ??
	{ 0x800000, 0x800003, taitob_sound_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress crimec_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0xa00000, 0xa0ffff, MRA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x408fff, taitob_text_r },
	{ 0x410000, 0x41197f, taitob_videoram_r },

	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x200000, 0x20000f, rastsag2_input_r },	/* DSW A/B, player inputs*/
	{ 0x600000, 0x600003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress crimec_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0xa00000, 0xa0ffff, MWA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x408fff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x409000, 0x40ffff, MWA_NOP }, /* unused (just set to zero at startup), not read by the game */
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },

	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x41800e, 0x41800f, taitob_video_control_w },

	{ 0x440000, 0x47ffff, taitob_pixelram_w, &taitob_pixelram, &b_pixelram_size }, /* pixel layer */

	{ 0x200000, 0x20000f, MWA_NOP }, /**/
	{ 0x600000, 0x600003, taitob_sound_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress tetrist_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x800000, 0x807fff, MRA_BANK1 },	/* Main RAM */

	{ 0xa00000, 0xa01fff, paletteram_word_r }, /*palette*/
//	{ 0x400000, 0x403fff, taitob_foreground_r },
//	{ 0x404000, 0x407fff, taitob_background_r },
//	{ 0x408000, 0x408fff, taitob_text_r }, /*text ram*/
	{ 0x400000, 0x408fff, MRA_BANK5 },

	{ 0x440000, 0x47ffff, taitob_pixelram_r },	/* Pixel Layer */
	{ 0x410000, 0x41197f, taitob_videoram_r },

	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x600000, 0x60000f, rastsag2_input_r },	/* DSW A/B, player inputs*/
	{ 0x200000, 0x200003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress tetrist_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x800000, 0x807fff, MWA_BANK1 },	/* Main RAM */

	{ 0xa00000, 0xa01fff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
//	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
//	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
//	{ 0x408000, 0x408fff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x400000, 0x408fff, MWA_BANK5 },
	{ 0x440000, 0x47ffff, taitob_pixelram_w, &taitob_pixelram, &b_pixelram_size }, /* pixel layer */
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },

	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x41800e, 0x41800f, taitob_video_control_w },

	{ 0x600000, 0x60000f, MWA_NOP }, // ??
	{ 0x200000, 0x200003, taitob_sound_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress hitice_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x800000, 0x803fff, MRA_BANK1 },	/* Main RAM */

	{ 0xa00000, 0xa01fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x40bfff, taitob_text_r },
	{ 0xb00000, 0xb7ffff, hitice_pixelram_r },	/* Pixel Layer ???????????? */
	{ 0x410000, 0x411fff /*97f*/, taitob_videoram_r },
	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x41800e, 0x41800f, taitob_video_control_r },

	{ 0x600000, 0x60000f, rastsag2_input_r },	/* DSW A/B, player 1,2 inputs*/
	{ 0x610000, 0x610001, hitice_input_r },		/* player 3,4 inputs*/

	{ 0x700000, 0x700003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress hitice_writemem[] =
{
	{ 0x000000, 0x05ffff, MWA_ROM },
	{ 0x800000, 0x803fff, MWA_BANK1 },	/* Main RAM */

	{ 0xa00000, 0xa01fff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x40bfff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x410000, 0x411fff /*97f*/, taitob_videoram_w, &videoram, &videoram_size  },

	//{ 0x411980, 0x411fff, MWA_BANK6 }, /*ashura and hitice*/

	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x41800c, 0x41800d, taitob_text_video_control_w },
	{ 0x41800e, 0x41800f, taitob_video_control_w },

	{ 0xb00000, 0xb7ffff, hitice_pixelram_w, &taitob_pixelram, &b_pixelram_size }, /* pixel layer ????????*/

	{ 0x600000, 0x60000f, MWA_NOP }, // ??
	{ 0x700000, 0x700003, taitob_sound_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress rambo3_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x800000, 0x803fff, MRA_BANK1 },	/* Main RAM */

	{ 0xa00000, 0xa01fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x40ffff, taitob_text_r },
	{ 0x410000, 0x411fff /*97f*/, taitob_videoram_r },
	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x41800c, 0x41800d, taitob_text_video_control_r },
	{ 0x41800e, 0x41800f, taitob_video_control_r },

	{ 0x600000, 0x60000f, rastsag2_input_r },	/* DSW A/B, player 1,2 inputs*/
	{ 0x200000, 0x200003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress rambo3_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x800000, 0x803fff, MWA_BANK1 },	/* Main RAM */

	{ 0xa00000, 0xa01fff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x40ffff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x410000, 0x411fff /*97f*/, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x41800c, 0x41800d, taitob_text_video_control_w },
	{ 0x41800e, 0x41800f, taitob_video_control_w },

	{ 0x600000, 0x60000f, MWA_NOP }, // ??
	{ 0x200000, 0x200003, taitob_sound_w },
	{ -1 }  /* end of table */
};

INLINE void taitob_changecolor_RRRRGGGGBBBBRGBx(int color,int data)
{
	int r,g,b;

	r = ((data >> 11) & 0x1e) | ((data>>3) & 0x01);
	g = ((data >>  7) & 0x1e) | ((data>>2) & 0x01);
	b = ((data >>  3) & 0x1e) | ((data>>1) & 0x01);
	r = (r<<3) | (r>>2);
	g = (g<<3) | (g>>2);
	b = (b<<3) | (b>>2);

	palette_change_color(color,r,g,b);
}

WRITE_HANDLER( taitob_paletteram_RRRRGGGGBBBBRGBx_word_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	WRITE_WORD(&paletteram[offset],newword);
	taitob_changecolor_RRRRGGGGBBBBRGBx(offset / 2,newword);
}


/***************************************************************************

  Puzzle Bobble EEPROM

***************************************************************************/

static struct EEPROM_interface eeprom_interface =
{
	6,				/* address bits */
	16,				/* data bits */
	"0110",			/*  read command */
	"0101",			/* write command */
	"0111",			/* erase command */
	"0100000000",	/*  lock command */
	"0100110000" 	/* unlock command*/
};

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface);
		if (file)
		{
			EEPROM_load(file);
		}
	}
}

static READ_HANDLER( eeprom_r )
{
	int res;

	res = EEPROM_read_bit() & 0x01;
	res |= readinputport( 4 ) & 0xfe; /* coin inputs */

	return res;
}

static int eep_latch = 0;

static READ_HANDLER ( eep_latch_r )
{
	return eep_latch;
}

static WRITE_HANDLER( eeprom_w )
{
	eep_latch = data;

	data >>= 8; /*M68k byte write*/

	/* bit 0 - Unused */
	/* bit 1 - Unused */
	/* bit 2 - Eeprom data  */
	/* bit 3 - Eeprom clock */
	/* bit 4 - Eeprom reset (active low) */
	/* bit 5 - Unused */
	/* bit 6 - Unused */
	/* bit 7 - set all the time (Chip Select?) */

	/* EEPROM */
	EEPROM_write_bit(data & 0x04);
	EEPROM_set_clock_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);
	EEPROM_set_cs_line((data & 0x10) ? CLEAR_LINE : ASSERT_LINE);
}



READ_HANDLER( p_read )
{
	//logerror("puzzle_read off%x\n",offset);
	return 0xffff;
}

WRITE_HANDLER( p_write )
{
	//logerror("puzzle_write off%2x data=%8x   pc=%8x\n",offset,data, cpu_get_pc());
}

static struct MemoryReadAddress puzbobb_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x900000, 0x90ffff, MRA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x40bfff, taitob_text_r },
	{ 0x410000, 0x41197f, taitob_videoram_r },
	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x500010, 0x50002f, p_read }, //????

	{ 0x500000, 0x50000f, puzbobb_input_r },	/* DSW A/B, player inputs*/
	{ 0x700000, 0x700003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress puzbobb_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x900000, 0x90ffff, MWA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, taitob_paletteram_RRRRGGGGBBBBRGBx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x40bfff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x418000, 0x41800d, MWA_NOP }, //temporarily disabled
	{ 0x41800e, 0x41800f, taitob_video_control_w },
	{ 0x418014, 0x418017, MWA_NOP }, //temporarily disabled

	{ 0x500028, 0x50002f, p_write }, //?????

	{ 0x500026, 0x500027, eeprom_w },
	{ 0x500000, 0x500001, MWA_NOP }, /*lots of zero writes here - watchdog ?*/
	{ 0x700000, 0x700003, taitob_sound_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress spacedx_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x900000, 0x90ffff, MRA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x40ffff, taitob_text_r },
	{ 0x410000, 0x41197f, taitob_videoram_r },
	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x41800c, 0x41800d, taitob_text_video_control_r },
	{ 0x41800e, 0x41800f, taitob_video_control_r },
	{ 0x440000, 0x47ffff, taitob_pixelram_r }, /* pixel layer */

	{ 0x500010, 0x500025, p_read }, //????

    { 0x500026, 0x500027, eep_latch_r },

	{ 0x500028, 0x50002f, p_read }, //????

	{ 0x500000, 0x50000f, puzbobb_input_r },	/* DSW A/B, player inputs*/
	{ 0x700000, 0x700003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress spacedx_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x900000, 0x90ffff, MWA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, taitob_paletteram_RRRRGGGGBBBBRGBx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x40ffff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x41800c, 0x41800d, taitob_text_video_control_w },
	{ 0x41800e, 0x41800f, taitob_video_control_w },
	{ 0x418014, 0x418017, MWA_NOP }, //temporarily disabled

	{ 0x440000, 0x47ffff, taitob_pixelram_w, &taitob_pixelram, &b_pixelram_size }, /* pixel layer */

	{ 0x500028, 0x50002f, p_write }, //?????

	{ 0x500026, 0x500027, eeprom_w },
	{ 0x500000, 0x500001, MWA_NOP }, /*lots of zero writes here - watchdog ?*/
	{ 0x700000, 0x700003, taitob_sound_w },
	{ -1 }  /* end of table */
};



READ_HANDLER( puzbobb_input_r2 )
{
	return 0xffff;
}

static struct MemoryReadAddress qzshowby_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0x900000, 0x90ffff, MRA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x40bfff, taitob_text_r },
	{ 0x410000, 0x41197f, taitob_videoram_r },
	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

//{ 0x200010, 0x20002f, p_read }, //????

	{ 0x200000, 0x20000f, puzbobb_input_r },	/* DSW A/B, player inputs*/
	{ 0x200024, 0x20002f, puzbobb_input_r2},
	{ 0x600000, 0x600003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress qzshowby_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },
	{ 0x900000, 0x90ffff, MWA_BANK1 },	/* Main RAM */

	{ 0x800000, 0x801fff, taitob_paletteram_RRRRGGGGBBBBRGBx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x40bfff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x418000, 0x41800d, MWA_NOP }, //temporarily disabled
	{ 0x41800e, 0x41800f, taitob_video_control_w },
	{ 0x418014, 0x418017, MWA_NOP }, //temporarily disabled

//{ 0x200028, 0x20002f, p_write }, //?????

	{ 0x200026, 0x200027, eeprom_w },
	{ 0x200000, 0x200001, MWA_NOP }, /*lots of zero writes here - watchdog ?*/
	{ 0x600000, 0x600003, taitob_sound_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress viofight_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0xa00000, 0xa03fff, MRA_BANK1 },	/* Main RAM */

	{ 0x600000, 0x601fff, paletteram_word_r },
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x40ffff, taitob_text_r },
	{ 0x410000, 0x41197f, taitob_videoram_r },

	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/
	{ 0x41800e, 0x41800f, taitob_video_control_r },

	{ 0x800000, 0x80000f, rastsag2_input_r },	/* DSW A/B, player inputs*/
	{ 0x200000, 0x200003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress viofight_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0xa00000, 0xa03fff, MWA_BANK1 },	/* Main RAM */

	{ 0x600000, 0x601fff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x408000, 0x40ffff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },

	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x41800c, 0x41800d, taitob_text_video_control_w },
	{ 0x41800e, 0x41800f, taitob_video_control_w },

	//{ 0x440000, 0x47ffff, taitob_pixelram_w, &taitob_pixelram }, /* pixel layer */

	{ 0x800000, 0x80000f, MWA_NOP }, /**/
	{ 0x200000, 0x200003, taitob_sound_w },
	{ -1 }  /* end of table */
};

static int device_no =0 ;

static WRITE_HANDLER( taitob_input_w )
{
//static int aa[8] = {1,1,1,1,1,1,1,1};

	if (offset==2)
	{
//		if (aa[(data>>8)&0xff] != 0)
//			logerror("control_w = %4x  ofs=%2x pc=%x\n",data,offset,cpu_get_pc() );
//		aa[(data>>8)&0xff] = 0;
		device_no = (data>>8)&0xff;
	}

//if (offset==0)
//      usrintf_showmessage("mow write to input offset 0 =%4x",data);

}
static READ_HANDLER( taitob_input_r )
{
	//logerror("control_r ofs=%2x pc=%x\n",offset,cpu_get_pc() );

if (offset==0)
{
   	switch (device_no)
	{
		case 0x00:
			return readinputport (3)<<8; /*DSW A*/
		case 0x01:
			return readinputport (4)<<8; /*DSW B*/
		case 0x02:
			return readinputport (0)<<8; /*player 1*/
		case 0x03:
			return readinputport (1)<<8; /*player 2*/
		case 0x04:
        case 0x05:
        case 0x06:
             return rand()&0xffff;
        case 0x07:
			return readinputport (2)<<8; /*tilt, coins*/
		default:
            //logerror("WARNING: mow read input offs=%2x PC=%08x\n", offset, cpu_get_pc());
			return 0xff;
	}
}

	return 0x0200;
}

#if 0
static int palette_buf[0x1000];

static WRITE_HANDLER( taitob_pal_w )
{
	paletteram_RRRRGGGGBBBBxxxx_word_w(offset+0x800, data);
    palette_buf[offset]=data;
}
static READ_HANDLER( taitob_pal_r )
{
	return palette_buf[offset];
}
#endif

static struct MemoryReadAddress masterw_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x200000, 0x203fff, MRA_BANK1 },			/* Main RAM */

	{ 0x600000, 0x6007ff, paletteram_word_r },	/*palette*/
	{ 0x400000, 0x403fff, taitob_foreground_r },
	{ 0x404000, 0x407fff, taitob_background_r },
	{ 0x408000, 0x408fff, taitob_text_r },		/*text ram*/
	{ 0x409000, 0x40bfff, MRA_BANK5 },			/*ashura only (textram continue ?)*/

	{ 0x40c000, 0x40ffff, taitob_pixelram_r },	/* Pixel Layer ???*/
	{ 0x410000, 0x41197f, taitob_videoram_r },		/*sprite ram*/
	{ 0x411980, 0x411fff, MRA_BANK6 },			/*ashura only (spriteram continue ?)*/

	{ 0x413800, 0x413bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x413c00, 0x413fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/
	{ 0x41800c, 0x41800d, taitob_text_video_control_r },
	{ 0x41800e, 0x41800f, taitob_video_control_r },

	//{ 0x800000, 0x80000f, rastsag2_input_r },	/* DSW A/B, player inputs*/
	{ 0x800000, 0x800003, taitob_input_r },	/* DSW A/B, player inputs*/
	{ 0xa00000, 0xa00003, taitob_sound_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress masterw_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x200000, 0x203fff, MWA_BANK1 },	/* Main RAM */ /*ashura up to 603fff only*/

	{ 0x600000, 0x6007ff, paletteram_RRRRGGGGBBBBxxxx_word_w, &paletteram, &b_paletteram_size },
	{ 0x400000, 0x403fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size }, /* foreground layer */
	{ 0x404000, 0x407fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size }, /* background layer */
	{ 0x408000, 0x408fff, taitob_text_w_tm, &b_textram, &b_textram_size }, /* text layer */
{ 0x409000, 0x40bfff, MWA_BANK5 }, /*ashura clears this area only*/
	{ 0x40c000, 0x40ffff, masterw_pixelram_w, &taitob_pixelram, &b_pixelram_size },	/* Pixel Layer ???*/
	{ 0x410000, 0x41197f, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x411980, 0x411fff, MWA_BANK6 }, /*ashura clears this area only*/
	{ 0x413800, 0x413bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll */
	{ 0x413c00, 0x413fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll */
	{ 0x41800c, 0x41800d, taitob_text_video_control_w },
	{ 0x41800e, 0x41800f, taitob_video_control_w },

	//{ 0x440000, 0x47ffff, taitob_pixelram_w, &taitob_pixelram }, /* ashura(US) pixel layer*/

	{ 0x800000, 0x800003, taitob_input_w }, // ??
	{ 0xa00000, 0xa00003, taitob_sound_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress silentd_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x400000, 0x403fff, MRA_BANK1 },	/* Main RAM */

	{ 0x300000, 0x301fff, paletteram_word_r },
	{ 0x500000, 0x503fff, taitob_foreground_r },
	{ 0x504000, 0x507fff, taitob_background_r },
	{ 0x508000, 0x50bfff, taitob_text_r },
	{ 0x50c000, 0x50ffff, MRA_BANK5 }, //????
	{ 0x510000, 0x511fff /*97f*/, taitob_videoram_r },
	{ 0x512000, 0x5137ff, MRA_BANK6 },

	{ 0x513800, 0x513bff, MRA_BANK3 }, /*1st.w foreground x, 2nd.w foreground y scroll*/
	{ 0x513c00, 0x513fff, MRA_BANK4 }, /*1st.w background x, 2nd.w background y scroll*/

	{ 0x51800c, 0x51800d, taitob_text_video_control_r },
	{ 0x51800e, 0x51800f, taitob_video_control_r },

	{ 0x200000, 0x20000f, silentd_input_r },	/* DSW A/B, player 1,2 inputs*/
	{ 0x210000, 0x210001, silentd_input1_r },
	{ 0x220000, 0x220001, silentd_input2_r },
	{ 0x230000, 0x230001, silentd_input3_r },
	{ 0x100000, 0x100003, taitob_sound_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress silentd_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x400000, 0x403fff, MWA_BANK1 },	/* Main RAM */

	{ 0x300000, 0x301fff, taitob_paletteram_RRRRGGGGBBBBRGBx_word_w, &paletteram, &b_paletteram_size },
	{ 0x500000, 0x503fff, taitob_foreground_w_tm, &b_foregroundram, &b_foregroundram_size },
	{ 0x504000, 0x507fff, taitob_background_w_tm, &b_backgroundram, &b_backgroundram_size },
	{ 0x508000, 0x50bfff, taitob_text_w_tm, &b_textram, &b_textram_size },
	{ 0x50c000, 0x50ffff, MWA_BANK5 }, //????
	{ 0x510000, 0x511fff /*97f*/, taitob_videoram_w, &videoram, &videoram_size  },
	{ 0x512000, 0x5137ff, MWA_BANK6 },

	{ 0x513800, 0x513bff, MWA_BANK3, &taitob_fscroll }, /*1st.w foreground x scroll, 2nd.w foreground y scroll*/
	{ 0x513c00, 0x513fff, MWA_BANK4, &taitob_bscroll }, /*1st.w background x scroll, 2nd.w background y scroll*/

	{ 0x51800c, 0x51800d, taitob_text_video_control_w },
	{ 0x51800e, 0x51800f, taitob_video_control_w },

	{ 0x200000, 0x20000f, MWA_NOP }, // ??
	{ 0x240000, 0x240001, MWA_NOP }, // ??
	{ 0x100000, 0x100003, taitob_sound_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x7fff, MRA_BANK2 },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe000, YM2610_status_port_0_A_r },
	{ 0xe001, 0xe001, YM2610_read_port_0_r },
	{ 0xe002, 0xe002, YM2610_status_port_0_B_r },
	{ 0xe200, 0xe200, MRA_NOP },
	{ 0xe201, 0xe201, rastan_a001_r },
	{ 0xea00, 0xea00, MRA_NOP },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe000, YM2610_control_port_0_A_w },
	{ 0xe001, 0xe001, YM2610_data_port_0_A_w },
	{ 0xe002, 0xe002, YM2610_control_port_0_B_w },
	{ 0xe003, 0xe003, YM2610_data_port_0_B_w },
	{ 0xe200, 0xe200, rastan_a000_w },
	{ 0xe201, 0xe201, rastan_a001_w },
	{ 0xe400, 0xe403, MWA_NOP }, /* pan */
	{ 0xe600, 0xe600, MWA_NOP }, /* ? */
	{ 0xee00, 0xee00, MWA_NOP }, /* ? */
	{ 0xf000, 0xf000, MWA_NOP }, /* ? */
	{ 0xf200, 0xf200, bankswitch_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress hitice_sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x7fff, MRA_BANK2 },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9000, 0x9000, YM2203_status_port_0_r },
	{ 0xb000, 0xb000, OKIM6295_status_0_r },
	{ 0xa001, 0xa001, rastan_a001_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress hitice_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },
	{ 0x9000, 0x9000, YM2203_control_port_0_w },
	{ 0x9001, 0x9001, YM2203_write_port_0_w },
	{ 0xb000, 0xb000, OKIM6295_data_0_w },
	{ 0xb001, 0xb001, OKIM6295_data_1_w },
	{ 0xa000, 0xa000, rastan_a000_w },
	{ 0xa001, 0xa001, rastan_a001_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress masterw_sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x7fff, MRA_BANK2 },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9000, 0x9000, YM2203_status_port_0_r },
	{ 0xa001, 0xa001, rastan_a001_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress masterw_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },
	{ 0x9000, 0x9000, YM2203_control_port_0_w },
	{ 0x9001, 0x9001, YM2203_write_port_0_w },
	{ 0xa000, 0xa000, rastan_a000_w },
	{ 0xa001, 0xa001, rastan_a001_w },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( rastsag2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
    PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START /* DSW B */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x02, "Easy" )
    PORT_DIPSETTING(    0x03, "Medium" )
    PORT_DIPSETTING(    0x01, "Hard" )
    PORT_DIPSETTING(    0x00, "Hardest" )
    PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
    PORT_DIPSETTING(    0x0c, "100000 Only" )
    PORT_DIPSETTING(    0x08, "150000 Only" )
    PORT_DIPSETTING(    0x04, "200000 Only" )
    PORT_DIPSETTING(    0x00, "250000 Only" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
    PORT_DIPSETTING(    0x00, DEF_STR( No ) )
    PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

INPUT_PORTS_END

INPUT_PORTS_START( crimec )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* DSW A */
    PORT_DIPNAME( 0x01, 0x01, "Hi Score" )
    PORT_DIPSETTING(    0x01, "Scribble" )
    PORT_DIPSETTING(    0x00, "3 Characters" )
    PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START /* DSW B */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x02, "Easy" )
    PORT_DIPSETTING(    0x03, "Medium" )
    PORT_DIPSETTING(    0x01, "Hard" )
    PORT_DIPSETTING(    0x00, "Hardest" )
    PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
    PORT_DIPSETTING(    0x08, "Every 80000" )
    PORT_DIPSETTING(    0x0c, "80000 Only" )
    PORT_DIPSETTING(    0x04, "160000 Only" )
    PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x10, "1" )
    PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
    PORT_DIPSETTING(    0x00, "4" )
    PORT_DIPNAME( 0xc0, 0x40, "Allow Continue" )
    PORT_DIPSETTING(    0x80, DEF_STR( No ) )
    PORT_DIPSETTING(    0xc0, "5 Times" )
    PORT_DIPSETTING(    0x00, "8 Times" )
    PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )

INPUT_PORTS_END

INPUT_PORTS_START( tetrist )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START /* DSW B */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x02, "Easy" )
    PORT_DIPSETTING(    0x03, "Medium" )
    PORT_DIPSETTING(    0x01, "Hard" )
    PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
    PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
    PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x20, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
    PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ))
    PORT_DIPSETTING(    0x40, DEF_STR( On ))
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

INPUT_PORTS_END

INPUT_PORTS_START( ashura )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
    PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START /* DSW B */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x02, "Easy" )
    PORT_DIPSETTING(    0x03, "Medium" )
    PORT_DIPSETTING(    0x01, "Hard" )
    PORT_DIPSETTING(    0x00, "Hardest" )
    PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
    PORT_DIPSETTING(    0x08, "Every 100000" )
    PORT_DIPSETTING(    0x0c, "Every 150000" )
    PORT_DIPSETTING(    0x04, "Every 200000" )
    PORT_DIPSETTING(    0x00, "Every 250000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x00, "1" )
    PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x30, "3" )
    PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
    PORT_DIPSETTING(    0x00, DEF_STR( No ) )
    PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

INPUT_PORTS_END

/*
Hit the Ice dipswitches
(info from Kevin Watson)

[1 is switch on and 0 is switch off]

Dip switch A
------------

Setting             Options          1 2 3 4 5 6 7 8
cabinet style       4 player         0
                    2 player         1
Test mode           normal               0
                    test mode            1
Attract mode        on                     0
                    off                    1
Game price          1 coin 1 play            0 0 0 0
                    2 coin 1 play            1 0 0 0
                    3 coin 1 play            0 1 0 0
           coin1    1 coin 2 play            0 0 1 0
           coin2    1 coin 3 play            1 1 0 0
                    1 coin 4 play            0 1 0 0
                    1 coin 5 play            1 0 1 0
                    1 coin 6 play            1 1 1 0

switch 2 and 8 are always set to off

Dip switch table B
------------------

Setting             Options          1 2 3 4 5 6 7 8
Difficulty          normal           0 0
                    easy             1 0
                    hard             0 1
                    hardest          1 1
Timer count         1 sec = 58/60        0 0
                    1 sec = 56/60        1 0
                    1 sec = 62/60        0 1
                    1 sec = 45/60        1 1
maximum credit      9                             0
                    99                            1

5,6,7 are set to off
*/

INPUT_PORTS_START( hitice )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT(         0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT(         0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(         0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT(         0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(         0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, "Cabinet Style" )
	PORT_DIPSETTING(    0x01, "4 Players")
	PORT_DIPSETTING(    0x00, "2 Players")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START /* DSW B */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x01, "Easy" )
    PORT_DIPSETTING(    0x00, "Hardest" )
    PORT_DIPSETTING(    0x02, "Hard" )
    PORT_DIPSETTING(    0x03, "Normal" )
    PORT_DIPNAME( 0x0c, 0x0c, "Timer count" )
    PORT_DIPSETTING(    0x0c, "1 sec = 58/60" )
    PORT_DIPSETTING(    0x04, "1 sec = 56/60" )
    PORT_DIPSETTING(    0x08, "1 sec = 62/60" )
    PORT_DIPSETTING(    0x00, "1 sec = 45/60" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x10, DEF_STR( On ))
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x20, DEF_STR( On ))
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x40, DEF_STR( On ))
	PORT_DIPNAME( 0x80, 0x80, "Maximum credits" )
	PORT_DIPSETTING(    0x00, "99" )
	PORT_DIPSETTING(    0x80, "9"  )

	PORT_START      /* IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START      /* IN6 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

INPUT_PORTS_END

INPUT_PORTS_START( rambo3a )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT(         0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT(         0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(         0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_HIGH, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN2, 2 )
	PORT_BIT(         0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(         0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START /* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) ) /* verified */
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x40, DEF_STR( On ))
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

INPUT_PORTS_END


/* Helps document the input ports. */

#define PORT_SERVICE_NO_TOGGLE(mask,default)	\
	PORT_BITX(    mask, mask & default, IPT_SERVICE1, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )


INPUT_PORTS_START( puzbobb )
	PORT_START      /* IN2 */ /*all OK*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_SERVICE3, 2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START /* IN X */ /*all OK*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/

	PORT_START      /* IN0 */ /*all OK*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START /* DSW B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /*unused in test mode*/
	PORT_SERVICE_NO_TOGGLE( 0x80, IP_ACTIVE_LOW ) /*ok*/

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 2 ) /*ok*/
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 2 ) /*ok*/
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 2 ) /*ok*/
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN4, 2 ) /*ok*/

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_END

INPUT_PORTS_START( viofight )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Price to Continue" )
	PORT_DIPSETTING(    0xc0, "Same as Start" )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_1C ) )

	PORT_START /* DSW B */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x03, "Medium" )
    PORT_DIPSETTING(    0x02, "Easy" )
    PORT_DIPSETTING(    0x01, "Hard" )
    PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x04, DEF_STR( Off ))
    PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ))
    PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x40, DEF_STR( Off ))
    PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

INPUT_PORTS_END

INPUT_PORTS_START( silentd )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT(         0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )
	PORT_BIT(         0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(         0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT(         0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(         0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x08, DEF_STR( On ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) ) /* Doesn't seem to currently have an effect on coinage */
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )	/* Coinage is not right... */

	PORT_START /* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) ) /* Verified */
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

/* These bits are tied together... Maybe "Credits" should be "Coin Slots"???  You can play
	the game with 2, 3, or 4 players and the last option maybe a linked 4 players.
	Using bit6 and bit7&8 you end up with 1, 2 or 4 seperate "Credits" on the demo screens.
	Using bits7&8 you can have 2-4 players as shown at the top of the game screens.
	I have no clue about the rest of them.  I cannot seem to find a way to disable the
	continue option.  I've set all other bits to off, then all to on and it makes no
	difference.  Also Coin B doesn't seem to be affected by the dip settings.
*/

	PORT_DIPNAME( 0x20, 0x20, "Credits" )	/* Only shows 4 seperate credits with 4p/1m below */
	PORT_DIPSETTING(    0x20, "Combined" )
	PORT_DIPSETTING(    0x00, "Seperate" )	/* When multiple credits show, Coin B will affect p2 credits */
	PORT_DIPNAME( 0xc0, 0x80, "Cabinet Style" )
	PORT_DIPSETTING(    0xc0, "3 Players")
	PORT_DIPSETTING(    0x80, "2 Players")
	PORT_DIPSETTING(    0x40, "4 Players/1 Machine??") /* with bit6, shows 4 seperate credits */
	PORT_DIPSETTING(    0x00, "4 Players/2 Machines??")	/* with bit6 shows 2 seperate credits */


	PORT_START      /* IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )

	PORT_START      /* IN6 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )

	PORT_START      /* IN7 */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN3, 2 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN3, 2 ) /*not sure if this is legal under MAME*/
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN4, 2 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN4, 2 ) /*not sure if this is legal under MAME*/
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 512*1024*8 , 512*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};
static struct GfxLayout tilelayout =
{
	16,16,	/* 16*16 tiles */
	8192,	/* 8192 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 512*1024*8 , 512*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7, 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0, &charlayout,  0, 256 },  /* sprites & playfield */
	{ REGION_GFX1, 0x0, &tilelayout,  0, 256 },  /* sprites & playfield */
	{ -1 } /* end of array */
};

static struct GfxLayout rambo3_charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 512*1*1024*8 , 512*2*1024*8 , 512*3*1024*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};
static struct GfxLayout rambo3_tilelayout =
{
	16,16,	/* 16*16 tiles */
	16384,	/* 16384 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 512*1*1024*8 , 512*2*1024*8 , 512*3*1024*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 64+0, 64+1, 64+2, 64+3, 64+4, 64+5, 64+6, 64+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo rambo3_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0, &rambo3_charlayout,  0, 256 },  /* sprites & playfield */
	{ REGION_GFX1, 0x0, &rambo3_tilelayout,  0, 256 },  /* sprites & playfield */
	{ -1 } /* end of array */
};


static struct GfxLayout qzshowby_charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 2048*1024*8 , 2048*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};
static struct GfxLayout qzshowby_tilelayout =
{
	16,16,	/* 16*16 tiles */
	32768,	/* 32768 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 2048*1024*8 , 2048*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7, 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo qzshowby_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x1000*64, &qzshowby_charlayout,  0, 256 },  /* sprites & playfield */
	{ REGION_GFX1, 0x0, &qzshowby_tilelayout,  0, 256 },  /* sprites & playfield */
	{ -1 } /* end of array */
};

static struct GfxLayout viofight_charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 1024*1024*8 , 1024*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};
static struct GfxLayout viofight_tilelayout =
{
	16,16,	/* 16*16 tiles */
	16384,	/* 16384 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 1024*1024*8 , 1024*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7, 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo viofight_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0, &viofight_charlayout,  0, 256 },  /* sprites & playfield */
	{ REGION_GFX1, 0x0, &viofight_tilelayout,  0, 256 },  /* sprites & playfield */
	{ -1 } /* end of array */
};

static struct GfxLayout silentd_charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 2048*1024*8 , 2048*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};
static struct GfxLayout silentd_tilelayout =
{
	16,16,	/* 16*16 tiles */
	32768,	/* 32768 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 8 , 2048*1024*8 , 2048*1024*8+8},
	{ 0, 1, 2, 3, 4, 5, 6, 7, 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo silentd_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0, &silentd_charlayout,  0, 256 },  /* sprites & playfield */
	{ REGION_GFX1, 0x0, &silentd_tilelayout,  0, 256 },  /* sprites & playfield */
	{ -1 } /* end of array */
};



/* handler called by the YM2610 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2610interface ym2610_interface_rsaga2 =
{
	1,	/* 1 chip */
	8000000,	/* 8 MHz */
	{ 30 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler },
	{ REGION_SOUND1 },
	{ REGION_SOUND2 },
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) }
};

static struct YM2610interface ym2610_interface_crimec =
{
	1,	/* 1 chip */
	8000000,	/* 8 MHz */
	{ 30 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler },
	{ REGION_SOUND1 },
	{ REGION_SOUND1 },
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) }
};

WRITE_HANDLER( portAwrite )
{
	static int a;

	unsigned char *RAM = memory_region(REGION_CPU2);
	int banknum = (data - 1) & 1;

	if (a!=data)
		cpu_setbank (2, &RAM [0x10000 + (banknum * 0x4000)]);

	a=data;
}

static struct YM2203interface ym2203_interface =
{
	1,
	4000000,				/* 3.5 MHz complete guess*/
	{ YM2203_VOL(80,25) },	/* complete guess */
	{ 0 },
	{ 0 },
	{ portAwrite },
	{ 0 },
	{ irqhandler }
};

static struct OKIM6295interface okim6295_interface =
{
	2,
	{ 8000,8000 },			/* complete guess */
	{ REGION_SOUND1,REGION_SOUND1 }, /* memory regions */
	{ 50,65 }				/*complete guess */
};


static struct MachineDriver machine_driver_rastsag2 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			rastsag2_readmem,rastsag2_writemem,0,0,
			rastansaga2_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order0,
	taitob_vh_stop,
	taitob_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_rsaga2
		}
	}
};

static struct MachineDriver machine_driver_ashura =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			rastsag2_readmem,rastsag2_writemem,0,0,
			rastansaga2_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order0,
	taitob_vh_stop,
	ashura_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_crimec
		}
	}
};

static struct MachineDriver machine_driver_crimec =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			crimec_readmem,crimec_writemem,0,0,
			crimec_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order1,
	taitob_vh_stop,
	crimec_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_crimec
		}
	}
};

static struct MachineDriver machine_driver_tetrist =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz ???*/
			tetrist_readmem,tetrist_writemem,0,0,
			rastansaga2_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	0, /*either no graphics rom dump, or the game does not use them. It uses pixel layer for sure*/
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order0,
	taitob_vh_stop,
	tetrist_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_rsaga2
		}
	}
};


static struct MachineDriver machine_driver_hitice =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			hitice_readmem,hitice_writemem,0,0,
			hitice_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			hitice_sound_readmem, hitice_sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2203 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order0,
	taitob_vh_stop,
	hitice_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_rambo3 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			rambo3_readmem,rambo3_writemem,0,0,
			rambo3_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	rambo3_gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order0,
	taitob_vh_stop,
	rambo3_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_crimec
		}
	}
};

static struct MachineDriver machine_driver_rambo3a =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			rambo3_readmem,rambo3_writemem,0,0,
			rambo3_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	viofight_gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order2,
	taitob_vh_stop,
	rambo3_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_crimec
		}
	}
};

#if 0
static void patch_puzzb(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	WRITE_WORD(&RAM[0x7fffe],0x0003);
/*
0x0004 - Puzzle Bobble US Version "by Taito Japan",
0x0003 - Puzzle Buster US Version "by Taito Japan",
0x0002 - Puzzle Buster US Version,
0x0001 - Puzzle Bobble Japan Version,
0x0000 - test version (prototype)
*/
}
#endif

static struct MachineDriver machine_driver_puzbobb =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			puzbobb_readmem,puzbobb_writemem,0,0,
			puzbobb_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0, /*patch_puzzb,*/

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order1,
	taitob_vh_stop,
	puzbobb_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610B,
			&ym2610_interface_crimec
		}
	},

	nvram_handler

};

static struct MachineDriver machine_driver_spacedx =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			spacedx_readmem,spacedx_writemem,0,0,
			puzbobb_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0, /*patch_puzzb,*/

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order1,
	taitob_vh_stop,
	crimec_vh_screenrefresh_tm, //puzbobb_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_crimec
		}
	},

	nvram_handler

};


static struct MachineDriver machine_driver_qzshowby =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz according to the readme*/
			qzshowby_readmem,qzshowby_writemem,0,0,
			puzbobb_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	qzshowby_gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order1,
	taitob_vh_stop,
	qzshowby_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610B,
			&ym2610_interface_crimec
		}
	},

	nvram_handler

};

static struct MachineDriver machine_driver_viofight =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			viofight_readmem,viofight_writemem,0,0,
			viofight_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			hitice_sound_readmem, hitice_sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2203 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	viofight_gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order2,
	taitob_vh_stop,
	hitice_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_masterw =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			masterw_readmem,masterw_writemem,0,0,
			masterw_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			masterw_sound_readmem, masterw_sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2203 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order2,
	taitob_vh_stop,
	masterw_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_silentd =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz ??? */
			silentd_readmem,silentd_writemem,0,0,
			silentd_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 2*8, 30*8-1 },

	silentd_gfxdecodeinfo,
	4096, 4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	taitob_vh_start_color_order2,
	taitob_vh_stop,
	silentd_vh_screenrefresh_tm,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface_rsaga2
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rastsag2 )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b81-08.50" , 0x00000, 0x20000, 0xd6da9169 )
	ROM_LOAD_ODD ( "b81-07.bin", 0x00000, 0x20000, 0x8edf17d7 )
	ROM_LOAD_EVEN( "b81-10.49" , 0x40000, 0x20000, 0x53f34344 )
	ROM_LOAD_ODD ( "b81-09.30" , 0x40000, 0x20000, 0x630d34af )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b81-11.bin", 0x00000, 0x4000, 0x3704bf09 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "b81-03.bin", 0x000000, 0x080000, 0x551b75e6 )
	ROM_LOAD( "b81-04.bin", 0x080000, 0x080000, 0xcf734e12 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "b81-01.bin", 0x00000, 0x80000, 0xb33f796b )

	ROM_REGION( 0x80000, REGION_SOUND2 )
	ROM_LOAD( "b81-02.bin", 0x00000, 0x80000, 0x20ec3b86 )
ROM_END

ROM_START( nastarw )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b81-08.50" , 0x00000, 0x20000, 0xd6da9169 )
	ROM_LOAD_ODD ( "b81-12.31",  0x00000, 0x20000, 0xf9d82741 )
	ROM_LOAD_EVEN( "b81-10.49" , 0x40000, 0x20000, 0x53f34344 )
	ROM_LOAD_ODD ( "b81-09.30" , 0x40000, 0x20000, 0x630d34af )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b81-11.bin", 0x00000, 0x4000, 0x3704bf09 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "b81-03.bin", 0x000000, 0x080000, 0x551b75e6 )
	ROM_LOAD( "b81-04.bin", 0x080000, 0x080000, 0xcf734e12 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "b81-01.bin", 0x00000, 0x80000, 0xb33f796b )

	ROM_REGION( 0x80000, REGION_SOUND2 )
	ROM_LOAD( "b81-02.bin", 0x00000, 0x80000, 0x20ec3b86 )
ROM_END

ROM_START( nastar )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b81-08.50" , 0x00000, 0x20000, 0xd6da9169 )
	ROM_LOAD_ODD ( "b81-13.bin", 0x00000, 0x20000, 0x60d176fb )
	ROM_LOAD_EVEN( "b81-10.49" , 0x40000, 0x20000, 0x53f34344 )
	ROM_LOAD_ODD ( "b81-09.30" , 0x40000, 0x20000, 0x630d34af )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b81-11.bin", 0x00000, 0x4000, 0x3704bf09 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "b81-03.bin", 0x000000, 0x080000, 0x551b75e6 )
	ROM_LOAD( "b81-04.bin", 0x080000, 0x080000, 0xcf734e12 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "b81-01.bin", 0x00000, 0x80000, 0xb33f796b )

	ROM_REGION( 0x80000, REGION_SOUND2 )
	ROM_LOAD( "b81-02.bin", 0x00000, 0x80000, 0x20ec3b86 )
ROM_END

ROM_START( crimecu )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b99-07"   , 0x00000, 0x20000, 0x26e886e6 )
	ROM_LOAD_ODD ( "b99-05"   , 0x00000, 0x20000, 0xff7f9a9d )
	ROM_LOAD_EVEN( "b99-06"   , 0x40000, 0x20000, 0x1f26aa92 )
	ROM_LOAD_ODD ( "b99_14-2" , 0x40000, 0x20000, 0x06cf8441 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b99-08", 0x00000, 0x4000, 0x26135451 )
	ROM_CONTINUE(       0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "b99-02.ch1", 0x000000, 0x080000, 0x2a5d4a26 )
	ROM_LOAD( "b99-01.ch0", 0x080000, 0x080000, 0xa19e373a )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "b99-03.roa", 0x00000, 0x80000, 0xdda10df7 )
ROM_END

ROM_START( crimec )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b99-07"   , 0x00000, 0x20000, 0x26e886e6 )
	ROM_LOAD_ODD ( "b99-05"   , 0x00000, 0x20000, 0xff7f9a9d )
	ROM_LOAD_EVEN( "b99-06"   , 0x40000, 0x20000, 0x1f26aa92 )
	ROM_LOAD_ODD ( "b99-14"   , 0x40000, 0x20000, 0x71c8b4d7 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b99-08", 0x00000, 0x4000, 0x26135451 )
	ROM_CONTINUE(       0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "b99-02.ch1", 0x000000, 0x080000, 0x2a5d4a26 )
	ROM_LOAD( "b99-01.ch0", 0x080000, 0x080000, 0xa19e373a )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "b99-03.roa", 0x00000, 0x80000, 0xdda10df7 )
ROM_END

ROM_START( crimecj )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b99-07", 0x00000, 0x20000, 0x26e886e6 )
	ROM_LOAD_ODD ( "b99-05", 0x00000, 0x20000, 0xff7f9a9d )
	ROM_LOAD_EVEN( "b99-06", 0x40000, 0x20000, 0x1f26aa92 )
	ROM_LOAD_ODD ( "15"    , 0x40000, 0x20000, 0xe8c1e56d )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b99-08", 0x00000, 0x4000, 0x26135451 )
	ROM_CONTINUE(       0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "b99-02.ch1", 0x000000, 0x080000, 0x2a5d4a26 )
	ROM_LOAD( "b99-01.ch0", 0x080000, 0x080000, 0xa19e373a )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "b99-03.roa", 0x00000, 0x80000, 0xdda10df7 )
ROM_END

ROM_START( ashura )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "c4307-1.50", 0x00000, 0x20000, 0xd5ceb20f )
	ROM_LOAD_ODD ( "c4305-1.31", 0x00000, 0x20000, 0xa6f3bb37 )
	ROM_LOAD_EVEN( "c4306-1.49", 0x40000, 0x20000, 0x0f331802 )
	ROM_LOAD_ODD ( "c4304-1.30", 0x40000, 0x20000, 0xe06a2414 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "c43-16",  0x00000, 0x4000, 0xcb26fce1 )
	ROM_CONTINUE(        0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "c43-02",  0x00000, 0x80000, 0x105722ae )
	ROM_LOAD( "c43-03",  0x80000, 0x80000, 0x426606ba )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "c43-01",  0x00000, 0x80000, 0xdb953f37 )

ROM_END

ROM_START( ashurau )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "c43-11", 0x00000, 0x20000, 0xd5aefc9b )
	ROM_LOAD_ODD ( "c43-09", 0x00000, 0x20000, 0xe91d0ab1 )
	ROM_LOAD_EVEN( "c43-10", 0x40000, 0x20000, 0xc218e7ea )
	ROM_LOAD_ODD ( "c43-08", 0x40000, 0x20000, 0x5ef4f19f )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "c43-16",  0x00000, 0x4000, 0xcb26fce1 )
	ROM_CONTINUE(        0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "c43-02",  0x00000, 0x80000, 0x105722ae )
	ROM_LOAD( "c43-03",  0x80000, 0x80000, 0x426606ba )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "c43-01",  0x00000, 0x80000, 0xdb953f37 )

ROM_END

ROM_START( tetrist )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "c12-03.bin", 0x000000, 0x020000, 0x38f1ed41 )
	ROM_LOAD_ODD ( "c12-02.bin", 0x000000, 0x020000, 0xed9530bc )
	ROM_LOAD_EVEN( "c12-05.bin", 0x040000, 0x020000, 0x128e9927 )
	ROM_LOAD_ODD ( "c12-04.bin", 0x040000, 0x020000, 0x5da7a319 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "c12-06.bin", 0x00000, 0x4000, 0xf2814b38 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	/*NOTE: no graphics roms*/

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* adpcm samples */
	/* empty */

	ROM_REGION( 0x80000, REGION_SOUND2 )	/* DELTA-T samples */
	/* empty */
ROM_END


ROM_START( hitice )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "c59-10", 0x00000, 0x20000, 0xe4ffad15 )
	ROM_LOAD_ODD ( "c59-12", 0x00000, 0x20000, 0xa080d7af )
	ROM_LOAD_EVEN( "c59-09", 0x40000, 0x10000, 0xe243e3b0 )
	ROM_LOAD_ODD ( "c59-11", 0x40000, 0x10000, 0x4d4dfa52 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "c59-08",  0x00000, 0x4000, 0xd3cbc10b )
	ROM_CONTINUE(        0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "c59-03",  0x00000, 0x80000, 0x9e513048 )
	ROM_LOAD( "c59-02",  0x80000, 0x80000, 0xaffb5e07 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "c59-01",  0x00000, 0x20000, 0x46ae291d )

ROM_END

ROM_START( rambo3 )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "r3-0e.rom",  0x00000, 0x10000, 0x3efa4177 )
	ROM_LOAD_ODD ( "r3-0o.rom",  0x00000, 0x10000, 0x55c38d92 )

/*NOTE: there is a hole in address space here */

	ROM_LOAD_EVEN( "r3-1e.rom" , 0x40000, 0x20000, 0x40e363c7 )
	ROM_LOAD_ODD ( "r3-1o.rom" , 0x40000, 0x20000, 0x7f1fe6ab )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "r3-00.rom", 0x00000, 0x4000, 0xdf7a6ed6 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "r3-ch1ll.rom", 0x000000, 0x020000, 0xc86ea5fc )
	ROM_LOAD( "r3-ch1hl.rom", 0x020000, 0x020000, 0x7525eb92 )
	ROM_LOAD( "r3-ch3ll.rom", 0x040000, 0x020000, 0xabe54b1e )
	ROM_LOAD( "r3-ch3hl.rom", 0x060000, 0x020000, 0x80e5647e )
	ROM_LOAD( "r3-ch1lh.rom", 0x080000, 0x020000, 0x75568cf0 )
	ROM_LOAD( "r3-ch1hh.rom", 0x0a0000, 0x020000, 0xe39cff37 )
	ROM_LOAD( "r3-ch3lh.rom", 0x0c0000, 0x020000, 0x5a155c04 )
	ROM_LOAD( "r3-ch3hh.rom", 0x0e0000, 0x020000, 0xabe58fdb )
	ROM_LOAD( "r3-ch0ll.rom", 0x100000, 0x020000, 0xb416f1bf )
	ROM_LOAD( "r3-ch0hl.rom", 0x120000, 0x020000, 0xa4cad36d )
	ROM_LOAD( "r3-ch2ll.rom", 0x140000, 0x020000, 0xd0ce3051 )
	ROM_LOAD( "r3-ch2hl.rom", 0x160000, 0x020000, 0x837d8677 )
	ROM_LOAD( "r3-ch0lh.rom", 0x180000, 0x020000, 0x76a330a2 )
	ROM_LOAD( "r3-ch0hh.rom", 0x1a0000, 0x020000, 0x4dc69751 )
	ROM_LOAD( "r3-ch2lh.rom", 0x1c0000, 0x020000, 0xdf3bc48f )
	ROM_LOAD( "r3-ch2hh.rom", 0x1e0000, 0x020000, 0xbf37dfac )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "r3-a1.rom", 0x00000, 0x20000, 0x4396fa19 )
	ROM_LOAD( "r3-a2.rom", 0x20000, 0x20000, 0x41fe53a8 )
	ROM_LOAD( "r3-a3.rom", 0x40000, 0x20000, 0xe89249ba )
	ROM_LOAD( "r3-a4.rom", 0x60000, 0x20000, 0x9cf4c21b )

ROM_END

ROM_START( rambo3a )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "ramb3-11.bin",  0x00000, 0x20000, 0x1cc42247 )
	ROM_LOAD_ODD ( "ramb3-13.bin",  0x00000, 0x20000, 0x0a964cb7 )
	ROM_LOAD_EVEN( "ramb3-07.bin",  0x40000, 0x20000, 0xc973ff6f )
	ROM_LOAD_ODD ( "ramb3-06.bin",  0x40000, 0x20000, 0xa83d3fd5 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "ramb3-10.bin", 0x00000, 0x4000, 0xb18bc020 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ramb3-03.bin",  0x000000, 0x80000, 0xf5808c41 )
	ROM_LOAD( "ramb3-04.bin",  0x080000, 0x80000, 0xc57831ce )
	ROM_LOAD( "ramb3-01.bin",  0x100000, 0x80000, 0xc55fcf54 )
	ROM_LOAD( "ramb3-02.bin",  0x180000, 0x80000, 0x9dd014c6 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "ramb3-05.bin", 0x00000, 0x80000, 0x0179dc40 )
ROM_END

ROM_START( puzbobb )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "pb-1c18.bin", 0x00000, 0x40000, 0x5de14f49 )
	ROM_LOAD_ODD ( "pb-ic2.bin",  0x00000, 0x40000, 0x2abe07d1 )

	ROM_REGION( 0x2c000, REGION_CPU2 )     /* 128k for Z80 code */
	ROM_LOAD( "pb-ic27.bin", 0x00000, 0x04000, 0x26efa4c4 )
	ROM_CONTINUE(            0x10000, 0x1c000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "pb-ic14.bin", 0x00000, 0x80000, 0x55f90ea4 )
	ROM_LOAD( "pb-ic9.bin",  0x80000, 0x80000, 0x3253aac9 )

	ROM_REGION( 0x100000, REGION_SOUND1 )
	ROM_LOAD( "pb-ic15.bin", 0x000000, 0x100000, 0x0840cbc4 )

ROM_END

ROM_START( spacedx )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "d89-06", 0x00000, 0x40000, 0x7122751e )
	ROM_LOAD_ODD ( "d89-05", 0x00000, 0x40000, 0xbe1638af )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "d89-07",  0x00000, 0x4000, 0xbd743401 )
	ROM_CONTINUE(        0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "d89-02",  0x00000, 0x80000, 0xc36544b9 )
	ROM_LOAD( "d89-01",  0x80000, 0x80000, 0xfffa0660 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* adpcm samples */
	ROM_LOAD( "d89-03",  0x00000, 0x80000, 0x218f31a4 )
ROM_END

ROM_START( qzshowby )
	ROM_REGION( 0x100000, REGION_CPU1 )     /* 1M for 68000 code */
	ROM_LOAD_EVEN( "d72-13.bin", 0x00000, 0x80000, 0xa867759f )
	ROM_LOAD_ODD ( "d72-12.bin", 0x00000, 0x80000, 0x522c09a7 )

	ROM_REGION( 0x2c000, REGION_CPU2 )     /* 128k for Z80 code */
	ROM_LOAD(  "d72-11.bin", 0x00000, 0x04000, 0x2ca046e2 )
	ROM_CONTINUE(            0x10000, 0x1c000 ) /* banked stuff */

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "d72-03.bin", 0x000000, 0x200000, 0x1de257d0 )
	ROM_LOAD( "d72-02.bin", 0x200000, 0x200000, 0xbf0da640 )

	ROM_REGION( 0x200000, REGION_SOUND1 )
	ROM_LOAD( "d72-01.bin", 0x00000, 0x200000, 0xb82b8830 )

ROM_END

ROM_START( viofight )
	ROM_REGION( 0x080000, REGION_CPU1 )     /* 1M for 68000 code */
	ROM_LOAD_EVEN( "c16-11.rom", 0x00000, 0x10000, 0x23dbd388 )
	ROM_LOAD_ODD ( "c16-14.rom", 0x00000, 0x10000, 0xdc934f6a )
	ROM_LOAD_EVEN( "c16-07.rom", 0x40000, 0x20000, 0x64d1d059 )
	ROM_LOAD_ODD ( "c16-06.rom", 0x40000, 0x20000, 0x043761d8 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 128k for Z80 code */
	ROM_LOAD(  "c16-12.rom", 0x00000, 0x04000, 0x6fb028c7 )
	ROM_CONTINUE(            0x10000, 0x0c000 ) /* banked stuff */

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "c16-01.rom", 0x000000, 0x080000, 0x7059ce83 )
	ROM_LOAD( "c16-02.rom", 0x080000, 0x080000, 0xb458e905 )
	ROM_LOAD( "c16-03.rom", 0x100000, 0x080000, 0x515a9431 )
	ROM_LOAD( "c16-04.rom", 0x180000, 0x080000, 0xebf285e2 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "c16-05.rom", 0x000000, 0x80000, 0xa49d064a )

ROM_END

ROM_START( masterw )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "b72-06.rom"   , 0x00000, 0x20000, 0xae848eff )
	ROM_LOAD_ODD ( "b72-12.rom"   , 0x00000, 0x20000, 0x7176ce70 )
	ROM_LOAD_EVEN( "b72-04.rom"   , 0x40000, 0x20000, 0x141e964c )
	ROM_LOAD_ODD ( "b72-03.rom"   , 0x40000, 0x20000, 0xf4523496 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "b72-07.rom", 0x00000, 0x4000, 0x2b1a946f )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "mow-m02.rom", 0x000000, 0x080000, 0xc519f65a )
	ROM_LOAD( "mow-m01.rom", 0x080000, 0x080000, 0xa24ac26e )

ROM_END

ROM_START( silentd )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 256k for 68000 code */
	ROM_LOAD_EVEN( "sr_12-1.rom", 0x00000, 0x20000, 0x5883d362 )
	ROM_LOAD_ODD ( "sr_15-1.rom", 0x00000, 0x20000, 0x8c0a72ae )
	ROM_LOAD_EVEN( "sr_11.rom",   0x40000, 0x20000, 0x35da4428 )
	ROM_LOAD_ODD ( "sr_09.rom",   0x40000, 0x20000, 0x2f05b14a )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD(  "sr_13.rom", 0x00000, 0x04000, 0x651861ab )
	ROM_CONTINUE(           0x10000, 0x0c000 ) /* banked stuff */

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "sd_m04.rom", 0x000000, 0x100000, 0x53237217 )
	ROM_LOAD( "sd_m06.rom", 0x100000, 0x100000, 0xe6e6dfa7 )
	ROM_LOAD( "sd_m03.rom", 0x200000, 0x100000, 0x1b9b2846 )
	ROM_LOAD( "sd_m05.rom", 0x300000, 0x100000, 0xe02472c5 )

	ROM_REGION( 0x80000, REGION_SOUND1 )
	ROM_LOAD( "sd_m02.rom", 0x00000, 0x80000, 0xe0de5c39 )

	ROM_REGION( 0x80000, REGION_SOUND2 )
	ROM_LOAD( "sd_m01.rom", 0x00000, 0x80000, 0xb41fff1a )
ROM_END


/*     year  rom       parent   machine   inp       init */
GAMEX( 1988, nastar,   0,       rastsag2, rastsag2, 0, ROT0,        "Taito Corporation Japan", "Nastar (World)", GAME_NO_COCKTAIL )
GAMEX( 1988, nastarw,  nastar,  rastsag2, rastsag2, 0, ROT0,        "Taito America Corporation", "Nastar Warrior (US)", GAME_NO_COCKTAIL )
GAMEX( 1988, rastsag2, nastar,  rastsag2, rastsag2, 0, ROT0,        "Taito Corporation", "Rastan Saga 2 (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1989, crimec,   0,       crimec,   crimec,   0, ROT0,        "Taito Corporation Japan", "Crime City (World)", GAME_NO_COCKTAIL )
GAMEX( 1989, crimecu,  crimec,  crimec,   crimec,   0, ROT0,        "Taito America Corporation", "Crime City (US)", GAME_NO_COCKTAIL )
GAMEX( 1989, crimecj,  crimec,  crimec,   crimec,   0, ROT0,        "Taito Corporation", "Crime City (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1989, tetrist,  tetris,  tetrist,  tetrist,  0, ROT0,        "Sega", "Tetris (Japan, B-System)", GAME_NO_COCKTAIL )
GAMEX( 1990, ashura,   0,       ashura,   ashura,   0, ROT270,      "Taito Corporation", "Ashura Blaster (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1990, ashurau,  ashura,  ashura,   ashura,   0, ROT270,      "Taito America Corporation", "Ashura Blaster (US)", GAME_NO_COCKTAIL )
GAMEX( 1990, hitice,   0,       hitice,   hitice,   0, ROT180,      "Williams", "Hit the Ice (US)", GAME_NO_COCKTAIL )
GAMEX( 1989, rambo3,   0,       rambo3,   rastsag2, 0, ROT180,      "Taito Europe Corporation", "Rambo III (set 1, Europe)", GAME_NO_COCKTAIL )
GAMEX( 1989, rambo3a,  rambo3,  rambo3a,  rambo3a,  0, ROT180,      "Taito America Corporation", "Rambo III (set 2, US)", GAME_NO_COCKTAIL)
GAMEX( 1994, puzbobb,  0,       puzbobb,  puzbobb,  0, ROT0,        "Taito Corporation", "Puzzle Bobble (Japan, B-System)", GAME_NO_COCKTAIL )
GAMEX( 1994, spacedx,  0,       spacedx,  puzbobb,  0, ROT0,        "Taito Corporation", "Space Invaders DX (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1993, qzshowby, 0,       qzshowby, puzbobb,  0, ROT0,        "Taito Corporation", "Quiz Sekai wa SHOW by shobai (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1989, viofight, 0,       viofight, viofight, 0, ROT180,      "Taito Corporation Japan", "Violence Fight (World)", GAME_NO_COCKTAIL)
GAMEX( 1989, masterw,  0,       masterw,  rastsag2, 0, ROT270,      "Taito Corporation Japan", "Master of Weapon (World)", GAME_NO_COCKTAIL)
GAMEX( 1992, silentd,  0,       silentd,  silentd,  0, ROT0_16BIT,  "Taito Corporation Japan", "Silent Dragon (World)", GAME_NO_COCKTAIL )
