#include "../machine/wmsyunit.cpp"
#include "../vidhrdw/wmsyunit.cpp"

/*************************************************************************

	Driver for Williams/Midway Y/Z-unit games.

	TMS34010 processor @ 6.00 - 6.25MHz
	Two sound options:
		standard Williams CVSD board, with 6809 @ 2MHz, a YM2151, and an HC55516 CVSD decoder
		standard Williams ADPCM board, with 6809 @ 2MHz, a YM2151, and an OKI6295 ADPCM decoder


	Created by Alex Pasadyn and Zsolt Vasvari with some help from Kurt Mahan
	Enhancements by Aaron Giles and Ernesto Corvi


	Currently playable:
	------------------
	- Narc
	- Smash TV
	- Total Carnage
	- Mortal Kombat (Y-unit versions)
	- Strike Force
	- Trog (prototype and release versions)
	- Hi Impact Football
	- Terminator 2


	Not Playable:
	------------
	- Super Hi Impact  (freaks out during play, often after extra point)


	Known Bugs:
	----------
	- When the Porsche spins in Narc, the wheels are missing for a single
	  frame. This actually might be there on the original, because if the
	  game runs over 60% (as it does now on my PII 266) it's very hard to
	  notice. With 100% framerate, it would be invisible.

	- Terminator 2 freezes while playing the movies after destroying skynet
	  Currently we have a hack in which prevents the freeze, but we really
	  should eventually figure it out for real.

**************************************************************************/


#include "driver.h"
#include "cpu/tms34010/tms34010.h"
#include "sndhrdw/williams.h"


/* these are accurate for MK Rev 5 according to measurements done by Bryan on a real board */
/* due to the way the TMS34010 core operates, however, we need to use 0 for our VBLANK */
/* duration (263ms is the measured value) */
#define MKLA5_VBLANK_DURATION		0
#define MKLA5_FPS					53.204950


/* code-related variables */
extern UINT8 *	wms_code_rom;
extern UINT8 *	wms_scratch_ram;

/* CMOS-related variables */
extern UINT8 *	wms_cmos_ram;

/* graphics-related variables */
extern UINT8 *	wms_gfx_rom;
extern size_t 	wms_gfx_rom_size;


/* driver-specific initialization */
void init_narc(void);
void init_narc3(void);
void init_smashtv(void);
void init_smashtv4(void);
void init_trog(void);
void init_trog3(void);
void init_trogp(void);
void init_mkla1(void);
void init_mkla2(void);
void init_mkla3(void);
void init_mkla4(void);
void init_totcarn(void);
void init_totcarnp(void);
void init_hiimpact(void);
void init_shimpact(void);
void init_strkforc(void);
void init_term2(void);

/* general machine init */
void wms_yunit_init_machine(void);


/* external read handlers */
READ_HANDLER( wms_yunit_dma_r );
READ_HANDLER( wms_yunit_vram_r );
READ_HANDLER( wms_yunit_cmos_r );
READ_HANDLER( wms_yunit_input_r );
READ_HANDLER( wms_yunit_gfxrom_r );
READ_HANDLER( wms_yunit_protection_r );

/* external write handlers */
WRITE_HANDLER( wms_yunit_dma_w );
WRITE_HANDLER( wms_yunit_vram_w );
WRITE_HANDLER( wms_yunit_cmos_w );
WRITE_HANDLER( wms_yunit_cmos_enable_w );
WRITE_HANDLER( wms_yunit_control_w );
WRITE_HANDLER( wms_yunit_sound_w );
WRITE_HANDLER( wms_yunit_paletteram_w );


/* external video routines */
int wms_yunit_4bit_vh_start(void);
int wms_yunit_6bit_vh_start(void);
int wms_zunit_vh_start(void);
void wms_yunit_vh_stop(void);
void wms_yunit_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void wms_yunit_vh_eof(void);
void wms_zunit_vh_eof(void);
void wms_yunit_display_interrupt(int scanline);
void wms_yunit_display_addr_changed(UINT32 offs, int rowbytes, int scanline);
void wms_yunit_to_shiftreg(offs_t address, unsigned short *shiftreg);
void wms_yunit_from_shiftreg(offs_t address, unsigned short *shiftreg);



/*************************************
 *
 *	CMOS read/write
 *
 *************************************/

static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file, wms_cmos_ram, 0x8000);
	else if (file)
		osd_fread(file, wms_cmos_ram, 0x8000);
	else
		memset(wms_cmos_ram, 0, 0x8000);
}



/*************************************
 *
 *	Memory maps
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ TOBYTE(0x00000000), TOBYTE(0x001fffff), wms_yunit_vram_r },
	{ TOBYTE(0x01000000), TOBYTE(0x010fffff), MRA_BANK2 },
	{ TOBYTE(0x01400000), TOBYTE(0x0140ffff), wms_yunit_cmos_r },
	{ TOBYTE(0x01800000), TOBYTE(0x0181ffff), paletteram_word_r },
	{ TOBYTE(0x01a80000), TOBYTE(0x01a8009f), wms_yunit_dma_r },
	{ TOBYTE(0x01c00000), TOBYTE(0x01c0005f), wms_yunit_input_r },
	{ TOBYTE(0x01c00060), TOBYTE(0x01c0007f), wms_yunit_protection_r },
	{ TOBYTE(0x02000000), TOBYTE(0x05ffffff), wms_yunit_gfxrom_r },
	{ TOBYTE(0xc0000000), TOBYTE(0xc00001ff), tms34010_io_register_r },
	{ TOBYTE(0xff800000), TOBYTE(0xffffffff), MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ TOBYTE(0x00000000), TOBYTE(0x001fffff), wms_yunit_vram_w },
	{ TOBYTE(0x01000000), TOBYTE(0x010fffff), MWA_BANK2, &wms_scratch_ram },
	{ TOBYTE(0x01400000), TOBYTE(0x0140ffff), wms_yunit_cmos_w },
	{ TOBYTE(0x01800000), TOBYTE(0x0181ffff), wms_yunit_paletteram_w, &paletteram },
	{ TOBYTE(0x01a00000), TOBYTE(0x01a0009f), wms_yunit_dma_w },	/* do we need this? */
	{ TOBYTE(0x01a80000), TOBYTE(0x01a8009f), wms_yunit_dma_w },
	{ TOBYTE(0x01c00060), TOBYTE(0x01c0007f), wms_yunit_cmos_enable_w },
	{ TOBYTE(0x01e00000), TOBYTE(0x01e0001f), wms_yunit_sound_w },
	{ TOBYTE(0x01f00000), TOBYTE(0x01f0001f), wms_yunit_control_w },
	{ TOBYTE(0x02000000), TOBYTE(0x05ffffff), MWA_ROM, &wms_gfx_rom, &wms_gfx_rom_size },
	{ TOBYTE(0xc0000000), TOBYTE(0xc00001ff), tms34010_io_register_w },
	{ TOBYTE(0xff800000), TOBYTE(0xffffffff), MWA_ROM, &wms_code_rom },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Input ports
 *
 *************************************/

INPUT_PORTS_START( narc )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW,  0, "Advance", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x0040, IP_ACTIVE_LOW, 0, "Vault Switch", KEYCODE_7, IP_JOY_NONE )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNUSED ) /* T/B strobe */
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED ) /* memory protect */
	PORT_BIT( 0x3000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0xc000, 0xc000, "Language" )
	PORT_DIPSETTING(      0xc000, "English" )
	PORT_DIPSETTING(      0x8000, "French" )
	PORT_DIPSETTING(      0x4000, "German" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Unknown ) )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( trog )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x00e0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_SERVICE( 0x0010, IP_ACTIVE_LOW )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNUSED ) /* video freeze */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0007, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x0038, "1" )
	PORT_DIPSETTING(      0x0018, "2" )
	PORT_DIPSETTING(      0x0028, "3" )
	PORT_DIPSETTING(      0x0008, "4" )
	PORT_DIPSETTING(      0x0030, "ECA" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0040, 0x0040, "Coinage Select" )
	PORT_DIPSETTING(      0x0040, "Dipswitch Coinage" )
	PORT_DIPSETTING(      0x0000, "CMOS Coinage" )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Cabinet ))
	PORT_DIPSETTING(      0x0080, DEF_STR( Upright ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x0100, 0x0100, "Test Switch" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Video Freeze" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0c00, 0x0c00, "Players" )
	PORT_DIPSETTING(      0x0c00, "4 Players" )
	PORT_DIPSETTING(      0x0400, "3 Players" )
	PORT_DIPSETTING(      0x0800, "2 Players" )
	PORT_DIPSETTING(      0x0000, "1 Player" )
	PORT_DIPNAME( 0x1000, 0x0000, "Counters" )
	PORT_DIPSETTING(      0x1000, "One Counter" )
	PORT_DIPSETTING(      0x0000, "Two Counters" )
	PORT_DIPNAME( 0x2000, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0xc000, 0xc000, "Country" )
	PORT_DIPSETTING(      0xc000, "USA" )
	PORT_DIPSETTING(      0x8000, "French" )
	PORT_DIPSETTING(      0x4000, "German" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Unused ))

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( smashtv )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_SERVICE( 0x0010, IP_ACTIVE_LOW )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNUSED ) /* video freeze */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0xf800, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( strkforc )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_SERVICE( 0x0010, IP_ACTIVE_LOW )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0xfc00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Coin Meter" )
	PORT_DIPSETTING(      0x0001, "Shared" )
	PORT_DIPSETTING(      0x0000, "Independent" )
	PORT_DIPNAME( 0x0002, 0x0002, "Credits to Start" )
	PORT_DIPSETTING(      0x0002, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPNAME( 0x000c, 0x000c, "Points for Ship" )
	PORT_DIPSETTING(      0x0004, "40000" )
	PORT_DIPSETTING(      0x000c, "50000" )
	PORT_DIPSETTING(      0x0008, "75000" )
	PORT_DIPSETTING(      0x0000, "100000" )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Lives ))
	PORT_DIPSETTING(      0x0010, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x00e0, 0x00e0, DEF_STR( Difficulty ))
	PORT_DIPSETTING(      0x0020, "Level 1" )
	PORT_DIPSETTING(      0x0060, "Level 2" )
	PORT_DIPSETTING(      0x00a0, "Level 3" )
	PORT_DIPSETTING(      0x00e0, "Level 4" )
	PORT_DIPSETTING(      0x00c0, "Level 5" )
	PORT_DIPSETTING(      0x0040, "Level 6" )
	PORT_DIPSETTING(      0x0080, "Level 7" )
	PORT_DIPSETTING(      0x0000, "Level 8" )
	PORT_DIPNAME( 0x0700, 0x0700, "Coin 2" )
	PORT_DIPSETTING(      0x0700, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0300, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0500, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x0100, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x0600, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0200, DEF_STR( 1C_6C ))
	PORT_DIPSETTING(      0x0400, "UK Coinage" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x7800, 0x7800, "Coin 1" )
	PORT_DIPSETTING(      0x6000, DEF_STR( 5C_1C ))
	PORT_DIPSETTING(      0x6800, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(      0x1000, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x1800, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x7800, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x4800, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x2000, DEF_STR( 3C_4C ))
	PORT_DIPSETTING(      0x2800, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x7000, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_5C ))
	PORT_DIPSETTING(      0x3800, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x5800, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x5000, DEF_STR( 1C_6C ))
	PORT_DIPSETTING(      0x4000, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0800, "5 Coins/2 Credits" )
	PORT_DIPNAME( 0x8000, 0x8000, "Test Switch" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( mkla1 )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_SERVICE( 0x0010, IP_ACTIVE_LOW )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0007, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0008, 0x0008, "Comic Book Offer" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0008, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, "Attract Sound" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0010, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Low Blows" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Blood" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0040, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0080, "Violence" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0080, DEF_STR( On ))
	PORT_DIPNAME( 0x0100, 0x0100, "Test Switch" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0000, "Counters" )
	PORT_DIPSETTING(      0x0200, "One" )
	PORT_DIPSETTING(      0x0000, "Two" )
	PORT_DIPNAME( 0x7c00, 0x7c00, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x7c00, "USA-1" )
	PORT_DIPSETTING(      0x3c00, "USA-2" )
	PORT_DIPSETTING(      0x5c00, "USA-3" )
	PORT_DIPSETTING(      0x1c00, "USA-4" )
	PORT_DIPSETTING(      0x6c00, "USA-ECA" )
	PORT_DIPSETTING(      0x7400, "German-1" )
	PORT_DIPSETTING(      0x3400, "German-2" )
	PORT_DIPSETTING(      0x5400, "German-3" )
	PORT_DIPSETTING(      0x1400, "German-4" )
	PORT_DIPSETTING(      0x6400, "German-5" )
	PORT_DIPSETTING(      0x7800, "French-1" )
	PORT_DIPSETTING(      0x3800, "French-2" )
	PORT_DIPSETTING(      0x5800, "French-3" )
	PORT_DIPSETTING(      0x1800, "French-4" )
	PORT_DIPSETTING(      0x6800, "French-ECA" )
	PORT_DIPSETTING(      0x0c00, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x8000, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x8000, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( term2 )
	PORT_START
	PORT_BIT( 0x000f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x00c0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0f00, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xc000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_SERVICE( 0x0010, IP_ACTIVE_LOW )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNUSED ) /* video freeze */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0xf000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_ANALOG( 0x00ff, 0x0000, IPT_AD_STICK_X | IPF_REVERSE | IPF_PLAYER1, 20, 10, 0, 0xff)
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0003, "Credits" )
	PORT_DIPSETTING(      0x0007, "2 Start/1 Continue" )
	PORT_DIPSETTING(      0x0006, "4 Start/1 Continue" )
	PORT_DIPSETTING(      0x0005, "2 Start/2 Continue" )
	PORT_DIPSETTING(      0x0004, "4 Start/2 Continue" )
	PORT_DIPSETTING(      0x0003, "1 Start/1 Continue" )
	PORT_DIPSETTING(      0x0002, "3 Start/2 Continue" )
	PORT_DIPSETTING(      0x0001, "3 Start/1 Continue" )
	PORT_DIPSETTING(      0x0000, "3 Start/3 Continue" )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x0038, "1" )
	PORT_DIPSETTING(      0x0018, "2" )
	PORT_DIPSETTING(      0x0028, "3" )
	PORT_DIPSETTING(      0x0008, "4" )
	PORT_DIPSETTING(      0x0030, "USA ECA" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0040, 0x0040, "Dipswitch Coinage" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0000, "Normal Display" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0100, 0x0100, "Test Switch" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Video Freeze" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Players" )
	PORT_DIPSETTING(      0x0800, "2 Players" )
	PORT_DIPSETTING(      0x0000, "1 Player" )
	PORT_DIPNAME( 0x1000, 0x0000, "Two Counters" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0xc000, 0xc000, "Country" )
	PORT_DIPSETTING(      0xc000, "USA" )
	PORT_DIPSETTING(      0x8000, "French" )
	PORT_DIPSETTING(      0x4000, "German" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Unused ))

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_AD_STICK_Y | IPF_PLAYER1, 20, 10, 0, 0xff)
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_AD_STICK_X | IPF_REVERSE | IPF_PLAYER2, 20, 10, 0, 0xff)
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_AD_STICK_Y | IPF_PLAYER2, 20, 10, 0, 0xff)
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( totcarn )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_SERVICE( 0x0010, IP_ACTIVE_LOW )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNUSED ) /* video freeze */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0xfc00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



/*************************************
 *
 *	34010 configuration
 *
 *************************************/

static struct tms34010_config cpu_config =
{
	0,								/* halt on reset */
	NULL,							/* generate interrupt */
	wms_yunit_to_shiftreg,			/* write to shiftreg function */
	wms_yunit_from_shiftreg,		/* read from shiftreg function */
	wms_yunit_display_addr_changed,	/* display address changed */
	wms_yunit_display_interrupt		/* display interrupt callback */
};



/*************************************
 *
 *	Z-unit machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_zunit =
{
	/* basic machine hardware */
	{
		{
			CPU_TMS34010,
			((48000000/TMS34010_CLOCK_DIVIDER)/2)/0.8,	/* 48 Mhz */
			readmem,writemem,0,0,
			ignore_interrupt,0,
			0,0,&cpu_config
		},
		SOUND_CPU_WILLIAMS_NARC
	},
	57, 0,	/* frames per second, vblank duration */
	1,
	wms_yunit_init_machine,

	/* video hardware */
    512, 432, { 0, 511, 27, 427 },

	0,
	65536,65536,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	wms_zunit_vh_eof,
	wms_zunit_vh_start,
	wms_yunit_vh_stop,
	wms_yunit_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		SOUND_WILLIAMS_NARC
	},
	nvram_handler
};



/*************************************
 *
 *	Y-unit machine drivers
 *
 *************************************/

static struct MachineDriver machine_driver_yunit_cvsd_4bit =
{
	/* basic machine hardware */
	{
		{
			CPU_TMS34010,
			((50000000/TMS34010_CLOCK_DIVIDER)/2)/0.8,	/* 50 Mhz */
			readmem,writemem,0,0,
			ignore_interrupt,0,
			0,0,&cpu_config
		},
		SOUND_CPU_WILLIAMS_CVSD
	},
	MKLA5_FPS, MKLA5_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	wms_yunit_init_machine,

	/* video hardware */
	512, 288, { 0, 399, 20, 274 },

	0,
	256,256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	wms_yunit_vh_eof,
	wms_yunit_4bit_vh_start,
	wms_yunit_vh_stop,
	wms_yunit_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		SOUND_WILLIAMS_CVSD
	},
	nvram_handler
};


static struct MachineDriver machine_driver_yunit_cvsd_6bit =
{
	/* basic machine hardware */
	{
		{
			CPU_TMS34010,
			((50000000/TMS34010_CLOCK_DIVIDER)/2)/0.8,	/* 50 Mhz */
			readmem,writemem,0,0,
			ignore_interrupt,0,
			0,0,&cpu_config
		},
		SOUND_CPU_WILLIAMS_CVSD
	},
	MKLA5_FPS, MKLA5_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	wms_yunit_init_machine,

	/* video hardware */
	512, 288, { 0, 399, 20, 274 },

	0,
	4096,4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	wms_yunit_vh_eof,
	wms_yunit_6bit_vh_start,
	wms_yunit_vh_stop,
	wms_yunit_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		SOUND_WILLIAMS_CVSD
	},
	nvram_handler
};


static struct MachineDriver machine_driver_yunit_adpcm =
{
	/* basic machine hardware */
	{
		{
			CPU_TMS34010,
			((48000000/TMS34010_CLOCK_DIVIDER)/2)/0.8, /* 3.75 Mhz */
			readmem,writemem,0,0,
			ignore_interrupt,0,
			0,0,&cpu_config
		},
		SOUND_CPU_WILLIAMS_ADPCM
	},
	MKLA5_FPS, MKLA5_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	wms_yunit_init_machine,

	/* video hardware */
	512, 304, { 0, 399, 27, 282 },

	0,
	4096,4096,
    0,

    VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	wms_yunit_vh_eof,
	wms_yunit_6bit_vh_start,
	wms_yunit_vh_stop,
	wms_yunit_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		SOUND_WILLIAMS_ADPCM(REGION_SOUND1)
	},
	nvram_handler
};



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( narc )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x30000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD ( "u4-snd", 0x10000, 0x10000, 0x450a591a )
	ROM_LOAD ( "u5-snd", 0x20000, 0x10000, 0xe551e5e3 )

	ROM_REGION( 0x50000, REGION_CPU3 )	/* slave sound CPU */
	ROM_LOAD ( "u35-snd", 0x10000, 0x10000, 0x81295892 )
	ROM_LOAD ( "u36-snd", 0x20000, 0x10000, 0x16cdbb13 )
	ROM_LOAD ( "u37-snd", 0x30000, 0x10000, 0x29dbeffd )
	ROM_LOAD ( "u38-snd", 0x40000, 0x10000, 0x09b03b80 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "u42",  0x80000, 0x20000, 0xd1111b76 )
	ROM_LOAD_EVEN( "u24",  0x80000, 0x20000, 0xaa0d3082 )
	ROM_LOAD_ODD ( "u41",  0xc0000, 0x20000, 0x3903191f )
	ROM_LOAD_EVEN( "u23",  0xc0000, 0x20000, 0x7a316582 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "u94",  0x000000, 0x10000, 0xca3194e4 )
	ROM_LOAD ( "u76",  0x200000, 0x10000, 0x1cd897f4 )
	ROM_LOAD ( "u93",  0x010000, 0x10000, 0x0ed7f7f5 )
	ROM_LOAD ( "u75",  0x210000, 0x10000, 0x78abfa01 )
	ROM_LOAD ( "u92",  0x020000, 0x10000, 0x40d2fc66 )
	ROM_LOAD ( "u74",  0x220000, 0x10000, 0x66d2a234 )
	ROM_LOAD ( "u91",  0x030000, 0x10000, 0xf39325e0 )
	ROM_LOAD ( "u73",  0x230000, 0x10000, 0xefa5cd4e )
	ROM_LOAD ( "u90",  0x040000, 0x10000, 0x0132aefa )
	ROM_LOAD ( "u72",  0x240000, 0x10000, 0x70638eb5 )
	ROM_LOAD ( "u89",  0x050000, 0x10000, 0xf7260c9e )
	ROM_LOAD ( "u71",  0x250000, 0x10000, 0x61226883 )
	ROM_LOAD ( "u88",  0x060000, 0x10000, 0xedc19f42 )
	ROM_LOAD ( "u70",  0x260000, 0x10000, 0xc808849f )
	ROM_LOAD ( "u87",  0x070000, 0x10000, 0xd9b42ff9 )
	ROM_LOAD ( "u69",  0x270000, 0x10000, 0xe7f9c34f )
	ROM_LOAD ( "u86",  0x080000, 0x10000, 0xaf7daad3 )
	ROM_LOAD ( "u68",  0x280000, 0x10000, 0x88a634d5 )
	ROM_LOAD ( "u85",  0x090000, 0x10000, 0x095fae6b )
	ROM_LOAD ( "u67",  0x290000, 0x10000, 0x4ab8b69e )
	ROM_LOAD ( "u84",  0x0a0000, 0x10000, 0x3fdf2057 )
	ROM_LOAD ( "u66",  0x2a0000, 0x10000, 0xe1da4b25 )
	ROM_LOAD ( "u83",  0x0b0000, 0x10000, 0xf2d27c9f )
	ROM_LOAD ( "u65",  0x2b0000, 0x10000, 0x6df0d125 )
	ROM_LOAD ( "u82",  0x0c0000, 0x10000, 0x962ce47c )
	ROM_LOAD ( "u64",  0x2c0000, 0x10000, 0xabab1b16 )
	ROM_LOAD ( "u81",  0x0d0000, 0x10000, 0x00fe59ec )
	ROM_LOAD ( "u63",  0x2d0000, 0x10000, 0x80602f31 )
	ROM_LOAD ( "u80",  0x0e0000, 0x10000, 0x147ba8e9 )
	ROM_LOAD ( "u62",  0x2e0000, 0x10000, 0xc2a476d1 )

	ROM_LOAD ( "u58",  0x400000, 0x10000, 0x8a7501e3 )
	ROM_LOAD ( "u40",  0x600000, 0x10000, 0x7fcaebc7 )
	ROM_LOAD ( "u57",  0x410000, 0x10000, 0xa504735f )
	ROM_LOAD ( "u39",  0x610000, 0x10000, 0x7db5cf52 )
	ROM_LOAD ( "u56",  0x420000, 0x10000, 0x55f8cca7 )
	ROM_LOAD ( "u38",  0x620000, 0x10000, 0x3f9f3ef7 )
	ROM_LOAD ( "u55",  0x430000, 0x10000, 0xd3c932c1 )
	ROM_LOAD ( "u37",  0x630000, 0x10000, 0xed81826c )
	ROM_LOAD ( "u54",  0x440000, 0x10000, 0xc7f4134b )
	ROM_LOAD ( "u36",  0x640000, 0x10000, 0xe5d855c0 )
	ROM_LOAD ( "u53",  0x450000, 0x10000, 0x6be4da56 )
	ROM_LOAD ( "u35",  0x650000, 0x10000, 0x3a7b1329 )
	ROM_LOAD ( "u52",  0x460000, 0x10000, 0x1ea36a4a )
	ROM_LOAD ( "u34",  0x660000, 0x10000, 0xfe982b0e )
	ROM_LOAD ( "u51",  0x470000, 0x10000, 0x9d4b0324 )
	ROM_LOAD ( "u33",  0x670000, 0x10000, 0x6bc7eb0f )
	ROM_LOAD ( "u50",  0x480000, 0x10000, 0x6f9f0c26 )
	ROM_LOAD ( "u32",  0x680000, 0x10000, 0x5875a6d3 )
	ROM_LOAD ( "u49",  0x490000, 0x10000, 0x80386fce )
	ROM_LOAD ( "u31",  0x690000, 0x10000, 0x2fa4b8e5 )
	ROM_LOAD ( "u48",  0x4a0000, 0x10000, 0x05c16185 )
	ROM_LOAD ( "u30",  0x6a0000, 0x10000, 0x7e4bb8ee )
	ROM_LOAD ( "u47",  0x4b0000, 0x10000, 0x4c0151f1 )
	ROM_LOAD ( "u29",  0x6b0000, 0x10000, 0x45136fd9 )
	ROM_LOAD ( "u46",  0x4c0000, 0x10000, 0x5670bfcb )
	ROM_LOAD ( "u28",  0x6c0000, 0x10000, 0xd6cdac24 )
	ROM_LOAD ( "u45",  0x4d0000, 0x10000, 0x27f10d98 )
	ROM_LOAD ( "u27",  0x6d0000, 0x10000, 0x4d33bbec )
	ROM_LOAD ( "u44",  0x4e0000, 0x10000, 0x93b8eaa4 )
	ROM_LOAD ( "u26",  0x6e0000, 0x10000, 0xcb19f784 )
ROM_END

ROM_START( narc3 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x30000, REGION_CPU2 )     /* sound CPU */
	ROM_LOAD ( "u4-snd", 0x10000, 0x10000, 0x450a591a )
	ROM_LOAD ( "u5-snd", 0x20000, 0x10000, 0xe551e5e3 )

	ROM_REGION( 0x50000, REGION_CPU3 )     /* slave sound CPU */
	ROM_LOAD ( "u35-snd", 0x10000, 0x10000, 0x81295892 )
	ROM_LOAD ( "u36-snd", 0x20000, 0x10000, 0x16cdbb13 )
	ROM_LOAD ( "u37-snd", 0x30000, 0x10000, 0x29dbeffd )
	ROM_LOAD ( "u38-snd", 0x40000, 0x10000, 0x09b03b80 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "narcrev3.u78",  0x00000, 0x10000, 0x388581b0 )
	ROM_LOAD_EVEN( "narcrev3.u60",  0x00000, 0x10000, 0xf273bc04 )
	ROM_LOAD_ODD ( "narcrev3.u77",  0x40000, 0x10000, 0xbdafaccc )
	ROM_LOAD_EVEN( "narcrev3.u59",  0x40000, 0x10000, 0x96314a99 )
	ROM_LOAD_ODD ( "narcrev3.u42",  0x80000, 0x10000, 0x56aebc81 )
	ROM_LOAD_EVEN( "narcrev3.u24",  0x80000, 0x10000, 0x11d7e143 )
	ROM_LOAD_ODD ( "narcrev3.u41",  0xc0000, 0x10000, 0x6142fab7 )
	ROM_LOAD_EVEN( "narcrev3.u23",  0xc0000, 0x10000, 0x98cdd178 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "u94",  0x000000, 0x10000, 0xca3194e4 )
	ROM_LOAD ( "u76",  0x200000, 0x10000, 0x1cd897f4 )
	ROM_LOAD ( "u93",  0x010000, 0x10000, 0x0ed7f7f5 )
	ROM_LOAD ( "u75",  0x210000, 0x10000, 0x78abfa01 )
	ROM_LOAD ( "u92",  0x020000, 0x10000, 0x40d2fc66 )
	ROM_LOAD ( "u74",  0x220000, 0x10000, 0x66d2a234 )
	ROM_LOAD ( "u91",  0x030000, 0x10000, 0xf39325e0 )
	ROM_LOAD ( "u73",  0x230000, 0x10000, 0xefa5cd4e )
	ROM_LOAD ( "u90",  0x040000, 0x10000, 0x0132aefa )
	ROM_LOAD ( "u72",  0x240000, 0x10000, 0x70638eb5 )
	ROM_LOAD ( "u89",  0x050000, 0x10000, 0xf7260c9e )
	ROM_LOAD ( "u71",  0x250000, 0x10000, 0x61226883 )
	ROM_LOAD ( "u88",  0x060000, 0x10000, 0xedc19f42 )
	ROM_LOAD ( "u70",  0x260000, 0x10000, 0xc808849f )
	ROM_LOAD ( "u87",  0x070000, 0x10000, 0xd9b42ff9 )
	ROM_LOAD ( "u69",  0x270000, 0x10000, 0xe7f9c34f )
	ROM_LOAD ( "u86",  0x080000, 0x10000, 0xaf7daad3 )
	ROM_LOAD ( "u68",  0x280000, 0x10000, 0x88a634d5 )
	ROM_LOAD ( "u85",  0x090000, 0x10000, 0x095fae6b )
	ROM_LOAD ( "u67",  0x290000, 0x10000, 0x4ab8b69e )
	ROM_LOAD ( "u84",  0x0a0000, 0x10000, 0x3fdf2057 )
	ROM_LOAD ( "u66",  0x2a0000, 0x10000, 0xe1da4b25 )
	ROM_LOAD ( "u83",  0x0b0000, 0x10000, 0xf2d27c9f )
	ROM_LOAD ( "u65",  0x2b0000, 0x10000, 0x6df0d125 )
	ROM_LOAD ( "u82",  0x0c0000, 0x10000, 0x962ce47c )
	ROM_LOAD ( "u64",  0x2c0000, 0x10000, 0xabab1b16 )
	ROM_LOAD ( "u81",  0x0d0000, 0x10000, 0x00fe59ec )
	ROM_LOAD ( "u63",  0x2d0000, 0x10000, 0x80602f31 )
	ROM_LOAD ( "u80",  0x0e0000, 0x10000, 0x147ba8e9 )
	ROM_LOAD ( "u62",  0x2e0000, 0x10000, 0xc2a476d1 )

	ROM_LOAD ( "u58",  0x400000, 0x10000, 0x8a7501e3 )
	ROM_LOAD ( "u40",  0x600000, 0x10000, 0x7fcaebc7 )
	ROM_LOAD ( "u57",  0x410000, 0x10000, 0xa504735f )
	ROM_LOAD ( "u39",  0x610000, 0x10000, 0x7db5cf52 )
	ROM_LOAD ( "u56",  0x420000, 0x10000, 0x55f8cca7 )
	ROM_LOAD ( "u38",  0x620000, 0x10000, 0x3f9f3ef7 )
	ROM_LOAD ( "u55",  0x430000, 0x10000, 0xd3c932c1 )
	ROM_LOAD ( "u37",  0x630000, 0x10000, 0xed81826c )
	ROM_LOAD ( "u54",  0x440000, 0x10000, 0xc7f4134b )
	ROM_LOAD ( "u36",  0x640000, 0x10000, 0xe5d855c0 )
	ROM_LOAD ( "u53",  0x450000, 0x10000, 0x6be4da56 )
	ROM_LOAD ( "u35",  0x650000, 0x10000, 0x3a7b1329 )
	ROM_LOAD ( "u52",  0x460000, 0x10000, 0x1ea36a4a )
	ROM_LOAD ( "u34",  0x660000, 0x10000, 0xfe982b0e )
	ROM_LOAD ( "u51",  0x470000, 0x10000, 0x9d4b0324 )
	ROM_LOAD ( "u33",  0x670000, 0x10000, 0x6bc7eb0f )
	ROM_LOAD ( "u50",  0x480000, 0x10000, 0x6f9f0c26 )
	ROM_LOAD ( "u32",  0x680000, 0x10000, 0x5875a6d3 )
	ROM_LOAD ( "u49",  0x490000, 0x10000, 0x80386fce )
	ROM_LOAD ( "u31",  0x690000, 0x10000, 0x2fa4b8e5 )
	ROM_LOAD ( "u48",  0x4a0000, 0x10000, 0x05c16185 )
	ROM_LOAD ( "u30",  0x6a0000, 0x10000, 0x7e4bb8ee )
	ROM_LOAD ( "u47",  0x4b0000, 0x10000, 0x4c0151f1 )
	ROM_LOAD ( "u29",  0x6b0000, 0x10000, 0x45136fd9 )
	ROM_LOAD ( "u46",  0x4c0000, 0x10000, 0x5670bfcb )
	ROM_LOAD ( "u28",  0x6c0000, 0x10000, 0xd6cdac24 )
	ROM_LOAD ( "u45",  0x4d0000, 0x10000, 0x27f10d98 )
	ROM_LOAD ( "u27",  0x6d0000, 0x10000, 0x4d33bbec )
	ROM_LOAD ( "u44",  0x4e0000, 0x10000, 0x93b8eaa4 )
	ROM_LOAD ( "u26",  0x6e0000, 0x10000, 0xcb19f784 )
ROM_END

ROM_START( trog )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (   "trogu4.bin", 0x10000, 0x10000, 0x759d0bf4 )
	ROM_LOAD (  "trogu19.bin", 0x30000, 0x10000, 0x960c333d )
	ROM_LOAD (  "trogu20.bin", 0x50000, 0x10000, 0x67f1658a )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "trogu105.bin",  0xc0000, 0x20000, 0xe6095189 )
	ROM_LOAD_EVEN( "trogu89.bin",   0xc0000, 0x20000, 0xfdd7cc65 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "trogu111.bin",  0x000000, 0x20000, 0x9ded08c1 )
	ROM_LOAD ( "trogu112.bin",  0x020000, 0x20000, 0x42293843 )
	ROM_LOAD ( "trogu113.bin",  0x040000, 0x20000, 0x77f50cbb )

	ROM_LOAD (  "trogu95.bin",  0x200000, 0x20000, 0xf3ba2838 )
	ROM_LOAD (  "trogu96.bin",  0x220000, 0x20000, 0xcfed2e77 )
	ROM_LOAD (  "trogu97.bin",  0x240000, 0x20000, 0x3262d1f8 )

	ROM_LOAD ( "trogu106.bin",  0x080000, 0x20000, 0xaf2eb0d8 )
 	ROM_LOAD ( "trogu107.bin",  0x0a0000, 0x20000, 0x88a7b3f6 )

	ROM_LOAD (  "trogu90.bin",  0x280000, 0x20000, 0x16e06753 )
	ROM_LOAD (  "trogu91.bin",  0x2a0000, 0x20000, 0x880a02c7 )
ROM_END

ROM_START( trog3 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (   "trogu4.bin", 0x10000, 0x10000, 0x759d0bf4 )
	ROM_LOAD (  "trogu19.bin", 0x30000, 0x10000, 0x960c333d )
	ROM_LOAD (  "trogu20.bin", 0x50000, 0x10000, 0x67f1658a )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "u105-la3",  0xc0000, 0x20000, 0xd09cea97 )
	ROM_LOAD_EVEN( "u89-la3",   0xc0000, 0x20000, 0xa61e3572 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "trogu111.bin",  0x000000, 0x20000, 0x9ded08c1 )
	ROM_LOAD ( "trogu112.bin",  0x020000, 0x20000, 0x42293843 )
	ROM_LOAD ( "trogu113.bin",  0x040000, 0x20000, 0x77f50cbb )

	ROM_LOAD (  "trogu95.bin",  0x200000, 0x20000, 0xf3ba2838 )
	ROM_LOAD (  "trogu96.bin",  0x220000, 0x20000, 0xcfed2e77 )
	ROM_LOAD (  "trogu97.bin",  0x240000, 0x20000, 0x3262d1f8 )

	ROM_LOAD ( "trogu106.bin",  0x080000, 0x20000, 0xaf2eb0d8 )
 	ROM_LOAD ( "trogu107.bin",  0x0a0000, 0x20000, 0x88a7b3f6 )

	ROM_LOAD (  "trogu90.bin",  0x280000, 0x20000, 0x16e06753 )
	ROM_LOAD (  "trogu91.bin",  0x2a0000, 0x20000, 0x880a02c7 )
ROM_END

ROM_START( trogp )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (   "trogu4.bin", 0x10000, 0x10000, 0x759d0bf4 )
	ROM_LOAD (  "trogu19.bin", 0x30000, 0x10000, 0x960c333d )
	ROM_LOAD (  "trogu20.bin", 0x50000, 0x10000, 0x67f1658a )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "trog105.dat",  0xc0000, 0x20000, 0x526a3f5b )
	ROM_LOAD_EVEN( "trog89.dat",   0xc0000, 0x20000, 0x38d68685 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "trogu111.bin",  0x000000, 0x20000, 0x9ded08c1 )
	ROM_LOAD ( "trogu112.bin",  0x020000, 0x20000, 0x42293843 )
	ROM_LOAD ( "trog113.dat",   0x040000, 0x20000, 0x2980a56f )

	ROM_LOAD (  "trogu95.bin",  0x200000, 0x20000, 0xf3ba2838 )
	ROM_LOAD (  "trogu96.bin",  0x220000, 0x20000, 0xcfed2e77 )
	ROM_LOAD (  "trog97.dat",   0x240000, 0x20000, 0xf94b77c1 )

	ROM_LOAD ( "trogu106.bin",  0x080000, 0x20000, 0xaf2eb0d8 )
 	ROM_LOAD ( "trogu107.bin",  0x0a0000, 0x20000, 0x88a7b3f6 )

	ROM_LOAD (  "trogu90.bin",  0x280000, 0x20000, 0x16e06753 )
	ROM_LOAD (  "trogu91.bin",  0x2a0000, 0x20000, 0x880a02c7 )
ROM_END

ROM_START( smashtv )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "u4.snd", 0x10000, 0x10000, 0x29d3f6c8 )
	ROM_LOAD ( "u19.snd", 0x30000, 0x10000, 0xac5a402a )
	ROM_LOAD ( "u20.snd", 0x50000, 0x10000, 0x875c66d9 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "u105.l8",  0xc0000, 0x20000, 0x48cd793f )
	ROM_LOAD_EVEN( "u89.l8",   0xc0000, 0x20000, 0x8e7fe463 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "u111.gam",  0x000000, 0x20000, 0x72f0ba84 )
	ROM_LOAD ( "u112.gam",  0x020000, 0x20000, 0x436f0283 )
	ROM_LOAD ( "u113.gam",  0x040000, 0x20000, 0x4a4b8110 )

	ROM_LOAD (  "u95.gam",  0x200000, 0x20000, 0xe864a44b )
	ROM_LOAD (  "u96.gam",  0x220000, 0x20000, 0x15555ea7 )
	ROM_LOAD (  "u97.gam",  0x240000, 0x20000, 0xccac9d9e )

 	ROM_LOAD ( "u106.gam",  0x400000, 0x20000, 0x5c718361 )
 	ROM_LOAD ( "u107.gam",  0x420000, 0x20000, 0x0fba1e36 )
 	ROM_LOAD ( "u108.gam",  0x440000, 0x20000, 0xcb0a092f )
ROM_END

ROM_START( smashtv6 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "u4.snd", 0x10000, 0x10000, 0x29d3f6c8 )
	ROM_LOAD ( "u19.snd", 0x30000, 0x10000, 0xac5a402a )
	ROM_LOAD ( "u20.snd", 0x50000, 0x10000, 0x875c66d9 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "la6-u105",  0xc0000, 0x20000, 0xf1666017 )
	ROM_LOAD_EVEN( "la6-u89",   0xc0000, 0x20000, 0x908aca5d )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "u111.gam",  0x000000, 0x20000, 0x72f0ba84 )
	ROM_LOAD ( "u112.gam",  0x020000, 0x20000, 0x436f0283 )
	ROM_LOAD ( "u113.gam",  0x040000, 0x20000, 0x4a4b8110 )

	ROM_LOAD (  "u95.gam",  0x200000, 0x20000, 0xe864a44b )
	ROM_LOAD (  "u96.gam",  0x220000, 0x20000, 0x15555ea7 )
	ROM_LOAD (  "u97.gam",  0x240000, 0x20000, 0xccac9d9e )

 	ROM_LOAD ( "u106.gam",  0x400000, 0x20000, 0x5c718361 )
 	ROM_LOAD ( "u107.gam",  0x420000, 0x20000, 0x0fba1e36 )
 	ROM_LOAD ( "u108.gam",  0x440000, 0x20000, 0xcb0a092f )
ROM_END

ROM_START( smashtv5 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "u4.snd", 0x10000, 0x10000, 0x29d3f6c8 )
	ROM_LOAD ( "u19.snd", 0x30000, 0x10000, 0xac5a402a )
	ROM_LOAD ( "u20.snd", 0x50000, 0x10000, 0x875c66d9 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "u105-v5",  0xc0000, 0x20000, 0x81f564b9 )
	ROM_LOAD_EVEN( "u89-v5",   0xc0000, 0x20000, 0xe5017d25 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "u111.gam",  0x000000, 0x20000, 0x72f0ba84 )
	ROM_LOAD ( "u112.gam",  0x020000, 0x20000, 0x436f0283 )
	ROM_LOAD ( "u113.gam",  0x040000, 0x20000, 0x4a4b8110 )

	ROM_LOAD (  "u95.gam",  0x200000, 0x20000, 0xe864a44b )
	ROM_LOAD (  "u96.gam",  0x220000, 0x20000, 0x15555ea7 )
	ROM_LOAD (  "u97.gam",  0x240000, 0x20000, 0xccac9d9e )

 	ROM_LOAD ( "u106.gam",  0x400000, 0x20000, 0x5c718361 )
 	ROM_LOAD ( "u107.gam",  0x420000, 0x20000, 0x0fba1e36 )
 	ROM_LOAD ( "u108.gam",  0x440000, 0x20000, 0xcb0a092f )
ROM_END

ROM_START( smashtv4 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "u4.snd", 0x10000, 0x10000, 0x29d3f6c8 )
	ROM_LOAD ( "u19.snd", 0x30000, 0x10000, 0xac5a402a )
	ROM_LOAD ( "u20.snd", 0x50000, 0x10000, 0x875c66d9 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "la4-u105",  0xc0000, 0x20000, 0xa50ccb71 )
	ROM_LOAD_EVEN( "la4-u89",   0xc0000, 0x20000, 0xef0b0279 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "u111.gam",  0x000000, 0x20000, 0x72f0ba84 )
	ROM_LOAD ( "u112.gam",  0x020000, 0x20000, 0x436f0283 )
	ROM_LOAD ( "u113.gam",  0x040000, 0x20000, 0x4a4b8110 )

	ROM_LOAD (  "u95.gam",  0x200000, 0x20000, 0xe864a44b )
	ROM_LOAD (  "u96.gam",  0x220000, 0x20000, 0x15555ea7 )
	ROM_LOAD (  "u97.gam",  0x240000, 0x20000, 0xccac9d9e )

 	ROM_LOAD ( "u106.gam",  0x400000, 0x20000, 0x5c718361 )
 	ROM_LOAD ( "u107.gam",  0x420000, 0x20000, 0x0fba1e36 )
 	ROM_LOAD ( "u108.gam",  0x440000, 0x20000, 0xcb0a092f )
ROM_END

ROM_START( hiimpact )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "sl1u4.bin", 0x10000, 0x20000, 0x28effd6a )
	ROM_LOAD ( "sl1u19.bin", 0x30000, 0x20000, 0x0ea22c89 )
	ROM_LOAD ( "sl1u20.bin", 0x50000, 0x20000, 0x4e747ab5 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "la3u105.bin",  0xc0000, 0x20000, 0xb9190c4a )
	ROM_LOAD_EVEN( "la3u89.bin",   0xc0000, 0x20000, 0x1cbc72a5 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "la1u111.bin",  0x000000, 0x20000, 0x49560560 )
	ROM_LOAD ( "la1u112.bin",  0x020000, 0x20000, 0x4dd879dc )
	ROM_LOAD ( "la1u113.bin",  0x040000, 0x20000, 0xb67aeb70 )
	ROM_LOAD ( "la1u114.bin",  0x060000, 0x20000, 0x9a4bc44b )

	ROM_LOAD (  "la1u95.bin",  0x200000, 0x20000, 0xe1352dc0 )
	ROM_LOAD (  "la1u96.bin",  0x220000, 0x20000, 0x197d0f34 )
	ROM_LOAD (  "la1u97.bin",  0x240000, 0x20000, 0x908ea575 )
	ROM_LOAD (  "la1u98.bin",  0x260000, 0x20000, 0x6dcbab11 )

 	ROM_LOAD ( "la1u106.bin",  0x400000, 0x20000, 0x7d0ead0d )
	ROM_LOAD ( "la1u107.bin",  0x420000, 0x20000, 0xef48e8fa )
	ROM_LOAD ( "la1u108.bin",  0x440000, 0x20000, 0x5f363e12 )
	ROM_LOAD ( "la1u109.bin",  0x460000, 0x20000, 0x3689fbbc )
ROM_END

ROM_START( shimpact )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (   "shiu4.bin", 0x10000, 0x20000, 0x1e5a012c )
	ROM_LOAD (  "shiu19.bin", 0x30000, 0x20000, 0x10f9684e )
	ROM_LOAD (  "shiu20.bin", 0x50000, 0x20000, 0x1b4a71c1 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "shiu105.bin",  0xc0000, 0x20000, 0xf2cf8de3 )
	ROM_LOAD_EVEN( "shiu89.bin",   0xc0000, 0x20000, 0xf97d9b01 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "shiu111.bin",  0x000000, 0x40000, 0x80ae2a86 )
	ROM_LOAD ( "shiu112.bin",  0x040000, 0x40000, 0x3ffc27e9 )
	ROM_LOAD ( "shiu113.bin",  0x080000, 0x40000, 0x01549d00 )
	ROM_LOAD ( "shiu114.bin",  0x0c0000, 0x40000, 0xa68af319 )

	ROM_LOAD (  "shiu95.bin",  0x200000, 0x40000, 0xe8f56ef5 )
	ROM_LOAD (  "shiu96.bin",  0x240000, 0x40000, 0x24ed04f9 )
	ROM_LOAD (  "shiu97.bin",  0x280000, 0x40000, 0xdd7f41a9 )
	ROM_LOAD (  "shiu98.bin",  0x2c0000, 0x40000, 0x23ef65dd )

	ROM_LOAD ( "shiu106.bin",  0x400000, 0x40000, 0x6f5bf337 )
	ROM_LOAD ( "shiu107.bin",  0x440000, 0x40000, 0xa8815dad )
	ROM_LOAD ( "shiu108.bin",  0x480000, 0x40000, 0xd39685a3 )
	ROM_LOAD ( "shiu109.bin",  0x4c0000, 0x40000, 0x36e0b2b2 )
ROM_END

ROM_START( strkforc )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x70000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "sfu4.bin", 0x10000, 0x10000, 0x8f747312 )
	ROM_LOAD ( "sfu19.bin", 0x30000, 0x10000, 0xafb29926 )
	ROM_LOAD ( "sfu20.bin", 0x50000, 0x10000, 0x1bc9b746 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "sfu105.bin",  0xc0000, 0x20000, 0x7895e0e3 )
	ROM_LOAD_EVEN( "sfu89.bin",   0xc0000, 0x20000, 0x26114d9e )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "sfu111.bin",  0x000000, 0x20000, 0x878efc80 )
	ROM_LOAD ( "sfu112.bin",  0x020000, 0x20000, 0x93394399 )
	ROM_LOAD ( "sfu113.bin",  0x040000, 0x20000, 0x9565a79b )
	ROM_LOAD ( "sfu114.bin",  0x060000, 0x20000, 0xb71152da )

	ROM_LOAD (  "sfu95.bin",  0x200000, 0x20000, 0x519cb2b4 )
	ROM_LOAD (  "sfu96.bin",  0x220000, 0x20000, 0x61214796 )
	ROM_LOAD (  "sfu97.bin",  0x240000, 0x20000, 0xeb5dee5f )
	ROM_LOAD (  "sfu98.bin",  0x260000, 0x20000, 0xc5c079e7 )

 	ROM_LOAD ( "sfu106.bin",  0x080000, 0x20000, 0xa394d4cf )
 	ROM_LOAD ( "sfu107.bin",  0x0a0000, 0x20000, 0xedef1419 )

	ROM_LOAD (  "sfu90.bin",  0x280000, 0x20000, 0x607bcdc0 )
	ROM_LOAD (  "sfu91.bin",  0x2a0000, 0x20000, 0xda02547e )
ROM_END

ROM_START( mkla1 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "mks-u3.rom", 0x10000, 0x40000, 0xc615844c )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "mks-u12.rom", 0x00000, 0x40000, 0x258bd7f9 )
	ROM_LOAD ( "mks-u13.rom", 0x40000, 0x40000, 0x7b7ec3b6 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "mkg-u105.la1",  0x00000, 0x80000, 0xe1f7b4c9 )
	ROM_LOAD_EVEN(  "mkg-u89.la1",  0x00000, 0x80000, 0x9d38ac75 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "mkg-u111.rom",  0x000000, 0x80000, 0xd17096c4 )
	ROM_LOAD ( "mkg-u112.rom",  0x080000, 0x80000, 0x993bc2e4 )
	ROM_LOAD ( "mkg-u113.rom",  0x100000, 0x80000, 0x6fb91ede )
	ROM_LOAD ( "mkg-u114.rom",  0x180000, 0x80000, 0xed1ff88a )

 	ROM_LOAD (  "mkg-u95.rom",  0x200000, 0x80000, 0xa002a155 )
 	ROM_LOAD (  "mkg-u96.rom",  0x280000, 0x80000, 0xdcee8492 )
	ROM_LOAD (  "mkg-u97.rom",  0x300000, 0x80000, 0xde88caef )
	ROM_LOAD (  "mkg-u98.rom",  0x380000, 0x80000, 0x37eb01b4 )

	ROM_LOAD ( "mkg-u106.rom",  0x400000, 0x80000, 0x45acaf21 )
 	ROM_LOAD ( "mkg-u107.rom",  0x480000, 0x80000, 0x2a6c10a0 )
  	ROM_LOAD ( "mkg-u108.rom",  0x500000, 0x80000, 0x23308979 )
 	ROM_LOAD ( "mkg-u109.rom",  0x580000, 0x80000, 0xcafc47bb )
ROM_END

ROM_START( mkla2 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "mks-u3.rom", 0x10000, 0x40000, 0xc615844c )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "mks-u12.rom", 0x00000, 0x40000, 0x258bd7f9 )
	ROM_LOAD ( "mks-u13.rom", 0x40000, 0x40000, 0x7b7ec3b6 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "mkg-u105.la2",  0x00000, 0x80000, 0x8531d44e )
	ROM_LOAD_EVEN(  "mkg-u89.la2",  0x00000, 0x80000, 0xb88dc26e )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "mkg-u111.rom",  0x000000, 0x80000, 0xd17096c4 )
	ROM_LOAD ( "mkg-u112.rom",  0x080000, 0x80000, 0x993bc2e4 )
	ROM_LOAD ( "mkg-u113.rom",  0x100000, 0x80000, 0x6fb91ede )
	ROM_LOAD ( "mkg-u114.rom",  0x180000, 0x80000, 0xed1ff88a )

 	ROM_LOAD (  "mkg-u95.rom",  0x200000, 0x80000, 0xa002a155 )
 	ROM_LOAD (  "mkg-u96.rom",  0x280000, 0x80000, 0xdcee8492 )
	ROM_LOAD (  "mkg-u97.rom",  0x300000, 0x80000, 0xde88caef )
	ROM_LOAD (  "mkg-u98.rom",  0x380000, 0x80000, 0x37eb01b4 )

	ROM_LOAD ( "mkg-u106.rom",  0x400000, 0x80000, 0x45acaf21 )
 	ROM_LOAD ( "mkg-u107.rom",  0x480000, 0x80000, 0x2a6c10a0 )
  	ROM_LOAD ( "mkg-u108.rom",  0x500000, 0x80000, 0x23308979 )
 	ROM_LOAD ( "mkg-u109.rom",  0x580000, 0x80000, 0xcafc47bb )
ROM_END

ROM_START( mkla3 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "mks-u3.rom", 0x10000, 0x40000, 0xc615844c )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "mks-u12.rom", 0x00000, 0x40000, 0x258bd7f9 )
	ROM_LOAD ( "mks-u13.rom", 0x40000, 0x40000, 0x7b7ec3b6 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "mkg-u105.la3",  0x00000, 0x80000, 0x2ce843c5 )
	ROM_LOAD_EVEN(  "mkg-u89.la3",  0x00000, 0x80000, 0x49a46e10 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "mkg-u111.rom",  0x000000, 0x80000, 0xd17096c4 )
	ROM_LOAD ( "mkg-u112.rom",  0x080000, 0x80000, 0x993bc2e4 )
	ROM_LOAD ( "mkg-u113.rom",  0x100000, 0x80000, 0x6fb91ede )
	ROM_LOAD ( "mkg-u114.rom",  0x180000, 0x80000, 0xed1ff88a )

 	ROM_LOAD (  "mkg-u95.rom",  0x200000, 0x80000, 0xa002a155 )
 	ROM_LOAD (  "mkg-u96.rom",  0x280000, 0x80000, 0xdcee8492 )
	ROM_LOAD (  "mkg-u97.rom",  0x300000, 0x80000, 0xde88caef )
	ROM_LOAD (  "mkg-u98.rom",  0x380000, 0x80000, 0x37eb01b4 )

	ROM_LOAD ( "mkg-u106.rom",  0x400000, 0x80000, 0x45acaf21 )
 	ROM_LOAD ( "mkg-u107.rom",  0x480000, 0x80000, 0x2a6c10a0 )
  	ROM_LOAD ( "mkg-u108.rom",  0x500000, 0x80000, 0x23308979 )
 	ROM_LOAD ( "mkg-u109.rom",  0x580000, 0x80000, 0xcafc47bb )
ROM_END

ROM_START( mkla4 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "mks-u3.rom", 0x10000, 0x40000, 0xc615844c )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "mks-u12.rom", 0x00000, 0x40000, 0x258bd7f9 )
	ROM_LOAD ( "mks-u13.rom", 0x40000, 0x40000, 0x7b7ec3b6 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "mkg-u105.la4",  0x00000, 0x80000, 0x29af348f )
	ROM_LOAD_EVEN(  "mkg-u89.la4",  0x00000, 0x80000, 0x1ad76662 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "mkg-u111.rom",  0x000000, 0x80000, 0xd17096c4 )
	ROM_LOAD ( "mkg-u112.rom",  0x080000, 0x80000, 0x993bc2e4 )
	ROM_LOAD ( "mkg-u113.rom",  0x100000, 0x80000, 0x6fb91ede )
	ROM_LOAD ( "mkg-u114.rom",  0x180000, 0x80000, 0xed1ff88a )

 	ROM_LOAD (  "mkg-u95.rom",  0x200000, 0x80000, 0xa002a155 )
 	ROM_LOAD (  "mkg-u96.rom",  0x280000, 0x80000, 0xdcee8492 )
	ROM_LOAD (  "mkg-u97.rom",  0x300000, 0x80000, 0xde88caef )
	ROM_LOAD (  "mkg-u98.rom",  0x380000, 0x80000, 0x37eb01b4 )

	ROM_LOAD ( "mkg-u106.rom",  0x400000, 0x80000, 0x45acaf21 )
 	ROM_LOAD ( "mkg-u107.rom",  0x480000, 0x80000, 0x2a6c10a0 )
  	ROM_LOAD ( "mkg-u108.rom",  0x500000, 0x80000, 0x23308979 )
 	ROM_LOAD ( "mkg-u109.rom",  0x580000, 0x80000, 0xcafc47bb )
ROM_END

ROM_START( term2 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "t2_snd.3", 0x10000, 0x20000, 0x73c3f5c4 )
	ROM_RELOAD (            0x30000, 0x20000 )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "t2_snd.12", 0x00000, 0x40000, 0xe192a40d )
	ROM_LOAD ( "t2_snd.13", 0x40000, 0x40000, 0x956fa80b )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "t2.105",  0x00000, 0x80000, 0x34142b28 )
	ROM_LOAD_EVEN( "t2.89",   0x00000, 0x80000, 0x5ffea427 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "t2.111",  0x000000, 0x80000, 0x916d0197 )
	ROM_LOAD ( "t2.112",  0x080000, 0x80000, 0x39ae1c87 )
	ROM_LOAD ( "t2.113",  0x100000, 0x80000, 0xcb5084e5 )
	ROM_LOAD ( "t2.114",  0x180000, 0x80000, 0x53c516ec )

	ROM_LOAD (  "t2.95",  0x200000, 0x80000, 0xdd39cf73 )
	ROM_LOAD (  "t2.96",  0x280000, 0x80000, 0x31f4fd36 )
	ROM_LOAD (  "t2.97",  0x300000, 0x80000, 0x7f72e775 )
	ROM_LOAD (  "t2.98",  0x380000, 0x80000, 0x1a20ce29 )

	ROM_LOAD ( "t2.106",  0x400000, 0x80000, 0xf08a9536 )
 	ROM_LOAD ( "t2.107",  0x480000, 0x80000, 0x268d4035 )
 	ROM_LOAD ( "t2.108",  0x500000, 0x80000, 0x379fdaed )
 	ROM_LOAD ( "t2.109",  0x580000, 0x80000, 0x306a9366 )
ROM_END

ROM_START( totcarn )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "tcu3.bin", 0x10000, 0x20000, 0x5bdb4665 )
	ROM_RELOAD (            0x30000, 0x20000 )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "tcu12.bin", 0x00000, 0x40000, 0xd0000ac7 )
	ROM_LOAD ( "tcu13.bin", 0x40000, 0x40000, 0xe48e6f0c )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "tcu105.bin",  0x80000, 0x40000, 0x7c651047 )
	ROM_LOAD_EVEN( "tcu89.bin",   0x80000, 0x40000, 0x6761daf3 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "tcu111.bin",  0x000000, 0x40000, 0x13f3f231 )
	ROM_LOAD ( "tcu112.bin",  0x040000, 0x40000, 0x72e45007 )
	ROM_LOAD ( "tcu113.bin",  0x080000, 0x40000, 0x2c8ec753 )
	ROM_LOAD ( "tcu114.bin",  0x0c0000, 0x40000, 0x6210c36c )

	ROM_LOAD (  "tcu95.bin",  0x200000, 0x40000, 0x579caeba )
	ROM_LOAD (  "tcu96.bin",  0x240000, 0x40000, 0xf43f1ffe )
	ROM_LOAD (  "tcu97.bin",  0x280000, 0x40000, 0x1675e50d )
	ROM_LOAD (  "tcu98.bin",  0x2c0000, 0x40000, 0xab06c885 )

	ROM_LOAD ( "tcu106.bin",  0x400000, 0x40000, 0x146e3863 )
 	ROM_LOAD ( "tcu107.bin",  0x440000, 0x40000, 0x95323320 )
 	ROM_LOAD ( "tcu108.bin",  0x480000, 0x40000, 0xed152acc )
 	ROM_LOAD ( "tcu109.bin",  0x4c0000, 0x40000, 0x80715252 )
ROM_END

ROM_START( totcarnp )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD (  "tcu3.bin", 0x10000, 0x20000, 0x5bdb4665 )
	ROM_RELOAD (            0x30000, 0x20000 )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "tcu12.bin", 0x00000, 0x40000, 0xd0000ac7 )
	ROM_LOAD ( "tcu13.bin", 0x40000, 0x40000, 0xe48e6f0c )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "u105",  0x80000, 0x40000, 0x7a782cae )
	ROM_LOAD_EVEN( "u89",   0x80000, 0x40000, 0x1c899a8d )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "tcu111.bin",  0x000000, 0x40000, 0x13f3f231 )
	ROM_LOAD ( "tcu112.bin",  0x040000, 0x40000, 0x72e45007 )
	ROM_LOAD ( "tcu113.bin",  0x080000, 0x40000, 0x2c8ec753 )
	ROM_LOAD ( "tcu114.bin",  0x0c0000, 0x40000, 0x6210c36c )

	ROM_LOAD (  "tcu95.bin",  0x200000, 0x40000, 0x579caeba )
	ROM_LOAD (  "tcu96.bin",  0x240000, 0x40000, 0xf43f1ffe )
	ROM_LOAD (  "tcu97.bin",  0x280000, 0x40000, 0x1675e50d )
	ROM_LOAD (  "tcu98.bin",  0x2c0000, 0x40000, 0xab06c885 )

	ROM_LOAD ( "tcu106.bin",  0x400000, 0x40000, 0x146e3863 )
 	ROM_LOAD ( "tcu107.bin",  0x440000, 0x40000, 0x95323320 )
 	ROM_LOAD ( "tcu108.bin",  0x480000, 0x40000, 0xed152acc )
 	ROM_LOAD ( "tcu109.bin",  0x4c0000, 0x40000, 0x80715252 )
ROM_END



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1988, narc,     0,       zunit,           narc,    narc,     ROT0_16BIT, "Williams", "Narc (rev 7.00)" )
GAME( 1988, narc3,    narc,    zunit,           narc,    narc3,    ROT0_16BIT, "Williams", "Narc (rev 3.20)" )

GAME( 1990, trog,     0,       yunit_cvsd_4bit, trog,    trog,     ROT0,       "Midway",   "Trog (rev LA4 03/11/91)" )
GAME( 1990, trog3,    trog,    yunit_cvsd_4bit, trog,    trog3,    ROT0,       "Midway",   "Trog (rev LA3 02/14/91)" )
GAME( 1990, trogp,    trog,    yunit_cvsd_4bit, trog,    trogp,    ROT0,       "Midway",   "Trog (prototype, rev 4.00 07/27/90)" )
GAME( 1991, strkforc, 0,       yunit_cvsd_4bit, strkforc,strkforc, ROT0,       "Midway",   "Strike Force (rev 1 02/25/91)" )

GAME( 1990, smashtv,  0,       yunit_cvsd_6bit, smashtv, smashtv,  ROT0_16BIT, "Williams", "Smash T.V. (rev 8.00)" )
GAME( 1990, smashtv6, smashtv, yunit_cvsd_6bit, smashtv, smashtv,  ROT0_16BIT, "Williams", "Smash T.V. (rev 6.00)" )
GAME( 1990, smashtv5, smashtv, yunit_cvsd_6bit, smashtv, smashtv,  ROT0_16BIT, "Williams", "Smash T.V. (rev 5.00)" )
GAME( 1990, smashtv4, smashtv, yunit_cvsd_6bit, smashtv, smashtv4, ROT0_16BIT, "Williams", "Smash T.V. (rev 4.00)" )
GAME( 1990, hiimpact, 0,       yunit_cvsd_6bit, trog,    hiimpact, ROT0_16BIT, "Williams", "High Impact Football (rev LA3 12/27/90)" )
GAMEX(1991, shimpact, 0,       yunit_cvsd_6bit, trog,    shimpact, ROT0_16BIT, "Midway",   "Super High Impact (rev LA1 09/30/91)", GAME_NOT_WORKING )

GAME( 1991, term2,    0,       yunit_adpcm,     term2,   term2,    ROT0_16BIT, "Midway",   "Terminator 2 - Judgment Day (rev LA3 03/27/92)" )
GAME( 1992, mkla1,    mk,      yunit_adpcm,     mkla1,   mkla1,    ROT0_16BIT, "Midway",   "Mortal Kombat (rev 1.0 08/08/92)" )
GAME( 1992, mkla2,    mk,      yunit_adpcm,     mkla1,   mkla2,    ROT0_16BIT, "Midway",   "Mortal Kombat (rev 2.0 08/18/92)" )
GAME( 1992, mkla3,    mk,      yunit_adpcm,     mkla1,   mkla3,    ROT0_16BIT, "Midway",   "Mortal Kombat (rev 3.0 08/31/92)" )
GAME( 1992, mkla4,    mk,      yunit_adpcm,     mkla1,   mkla4,    ROT0_16BIT, "Midway",   "Mortal Kombat (rev 4.0 09/28/92)" )
GAME( 1992, totcarn,  0,       yunit_adpcm,     totcarn, totcarn,  ROT0_16BIT, "Midway",   "Total Carnage (rev LA1 03/10/92)" )
GAME( 1992, totcarnp, totcarn, yunit_adpcm,     totcarn, totcarnp, ROT0_16BIT, "Midway",   "Total Carnage (prototype, rev 1.0 01/25/92)" )
