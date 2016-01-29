#include "../machine/wmstunit.cpp"
#include "../vidhrdw/wmstunit.cpp"

/*************************************************************************

	Driver for Midway T-unit games
  
	TMS34010 processor @ 6.25MHz
	Two sound options:
		standard Williams ADPCM board, with 6809 @ 2MHz, a YM2151, and an OKI6295 ADPCM decoder
		Williams compressed digital sound board, with ADSP2105 and a DAC


	Created by Alex Pasadyn and Zsolt Vasvari with some help from Kurt Mahan
	Enhancements by Aaron Giles and Ernesto Corvi


	Currently playable:
	------------------
	- Mortal Kombat (T-unit version)
	- Mortal Kombat 2
	- NBA Jam
	- NBA Jam Tournament Edition


	Known Bugs:
	----------
	none

**************************************************************************/


#include "driver.h"
#include "cpu/tms34010/tms34010.h"
#include "cpu/adsp2100/adsp2100.h"
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
void init_mk(void);
void init_mk2(void);
void init_mk2r14(void);
void init_mk2r32(void);
void init_nbajam(void);
void init_nbajam20(void);
void init_nbajamte(void);

/* general machine init */
void wms_tunit_init_machine(void);


/* external read handlers */
READ_HANDLER( wms_tunit_dma_r );
READ_HANDLER( wms_tunit_vram_r );
READ_HANDLER( wms_tunit_cmos_r );
READ_HANDLER( wms_tunit_input_r );
READ_HANDLER( wms_tunit_gfxrom_r );
READ_HANDLER( wms_tunit_sound_r );
READ_HANDLER( wms_tunit_sound_state_r );

/* external write handlers */
WRITE_HANDLER( wms_tunit_dma_w );
WRITE_HANDLER( wms_tunit_vram_w );
WRITE_HANDLER( wms_tunit_cmos_w );
WRITE_HANDLER( wms_tunit_cmos_enable_w );
WRITE_HANDLER( wms_tunit_control_w );
WRITE_HANDLER( wms_tunit_sound_w );
WRITE_HANDLER( wms_tunit_paletteram_w );


/* external video routines */
int wms_tunit_vh_start(void);
void wms_tunit_vh_stop(void);
void wms_tunit_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void wms_tunit_to_shiftreg(offs_t address, unsigned short *shiftreg);
void wms_tunit_from_shiftreg(offs_t address, unsigned short *shiftreg);
void wms_tunit_display_addr_changed(UINT32 offs, int rowbytes, int scanline);



/*************************************
 *
 *	CMOS read/write
 *
 *************************************/

static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file, wms_cmos_ram, 0x4000);
	else if (file)
		osd_fread(file, wms_cmos_ram, 0x4000);
	else
		memset(wms_cmos_ram, 0, 0x4000);
}



/*************************************
 *
 *	Memory maps
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ TOBYTE(0x00000000), TOBYTE(0x003fffff), wms_tunit_vram_r },
	{ TOBYTE(0x01000000), TOBYTE(0x013fffff), MRA_BANK2 },
	{ TOBYTE(0x01400000), TOBYTE(0x0141ffff), wms_tunit_cmos_r },
	{ TOBYTE(0x01600000), TOBYTE(0x0160003f), wms_tunit_input_r },
	{ TOBYTE(0x01800000), TOBYTE(0x0187ffff), paletteram_word_r },
	{ TOBYTE(0x01a80000), TOBYTE(0x01a800ff), wms_tunit_dma_r },
	{ TOBYTE(0x01d00000), TOBYTE(0x01d0001f), wms_tunit_sound_state_r },
	{ TOBYTE(0x01d01020), TOBYTE(0x01d0103f), wms_tunit_sound_r },
	{ TOBYTE(0x02000000), TOBYTE(0x07ffffff), wms_tunit_gfxrom_r },
	{ TOBYTE(0xc0000000), TOBYTE(0xc00001ff), tms34010_io_register_r },
	{ TOBYTE(0xff800000), TOBYTE(0xffffffff), MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ TOBYTE(0x00000000), TOBYTE(0x003fffff), wms_tunit_vram_w },
	{ TOBYTE(0x01000000), TOBYTE(0x013fffff), MWA_BANK2, &wms_scratch_ram },
	{ TOBYTE(0x01400000), TOBYTE(0x0141ffff), wms_tunit_cmos_w, &wms_cmos_ram },
	{ TOBYTE(0x01480000), TOBYTE(0x014fffff), wms_tunit_cmos_enable_w },
	{ TOBYTE(0x01800000), TOBYTE(0x0187ffff), wms_tunit_paletteram_w, &paletteram },
	{ TOBYTE(0x01a80000), TOBYTE(0x01a800ff), wms_tunit_dma_w },
	{ TOBYTE(0x01b00000), TOBYTE(0x01b0001f), wms_tunit_control_w },
/*	{ TOBYTE(0x01c00060), TOBYTE(0x01c0007f), wms_tunit_cmos_enable_w }, */
	{ TOBYTE(0x01d01020), TOBYTE(0x01d0103f), wms_tunit_sound_w },
	{ TOBYTE(0x01d81060), TOBYTE(0x01d8107f), watchdog_reset_w },
	{ TOBYTE(0x01f00000), TOBYTE(0x01f0001f), wms_tunit_control_w },
	{ TOBYTE(0x02000000), TOBYTE(0x07ffffff), MWA_ROM, &wms_gfx_rom, &wms_gfx_rom_size },
	{ TOBYTE(0xc0000000), TOBYTE(0xc00001ff), tms34010_io_register_w },
	{ TOBYTE(0xff800000), TOBYTE(0xffffffff), MWA_ROM, &wms_code_rom },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Input ports
 *
 *************************************/

INPUT_PORTS_START( mk )
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
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Counters" )
	PORT_DIPSETTING(      0x0002, "One" )
	PORT_DIPSETTING(      0x0000, "Two" )
	PORT_DIPNAME( 0x007c, 0x007c, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x007c, "USA-1" )
	PORT_DIPSETTING(      0x003c, "USA-2" )
	PORT_DIPSETTING(      0x005c, "USA-3" )
	PORT_DIPSETTING(      0x001c, "USA-4" )
	PORT_DIPSETTING(      0x006c, "USA-ECA" )
	PORT_DIPSETTING(      0x0074, "German-1" )
	PORT_DIPSETTING(      0x0034, "German-2" )
	PORT_DIPSETTING(      0x0054, "German-3" )
	PORT_DIPSETTING(      0x0014, "German-4" )
	PORT_DIPSETTING(      0x0064, "German-5" )
	PORT_DIPSETTING(      0x0078, "French-1" )
	PORT_DIPSETTING(      0x0038, "French-2" )
	PORT_DIPSETTING(      0x0058, "French-3" )
	PORT_DIPSETTING(      0x0018, "French-4" )
	PORT_DIPSETTING(      0x0068, "French-ECA" )
	PORT_DIPSETTING(      0x000c, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0080, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0080, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0100, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0200, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0400, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Comic Book Offer" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0800, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Attract Sound" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x1000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Low Blows" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Blood" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x4000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Violence" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END

INPUT_PORTS_START( mk2 )
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
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED ) /* volume down */
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED ) /* volume up */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON5 )
	PORT_BIT( 0x000c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Counters" )
	PORT_DIPSETTING(      0x0002, "One" )
	PORT_DIPSETTING(      0x0000, "Two" )
	PORT_DIPNAME( 0x007c, 0x007c, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x007c, "USA-1" )
	PORT_DIPSETTING(      0x003c, "USA-2" )
	PORT_DIPSETTING(      0x005c, "USA-3" )
	PORT_DIPSETTING(      0x001c, "USA-4" )
	PORT_DIPSETTING(      0x006c, "USA-ECA" )
	PORT_DIPSETTING(      0x0074, "German-1" )
	PORT_DIPSETTING(      0x0034, "German-2" )
	PORT_DIPSETTING(      0x0054, "German-3" )
	PORT_DIPSETTING(      0x0014, "German-4" )
	PORT_DIPSETTING(      0x0064, "German-5" )
	PORT_DIPSETTING(      0x0078, "French-1" )
	PORT_DIPSETTING(      0x0038, "French-2" )
	PORT_DIPSETTING(      0x0058, "French-3" )
	PORT_DIPSETTING(      0x0018, "French-4" )
	PORT_DIPSETTING(      0x0068, "French-ECA" )
	PORT_DIPSETTING(      0x000c, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0080, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0080, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x0100, 0x0100, "Circuit Boards" )
	PORT_DIPSETTING(      0x0100, "2" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x0200, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0200, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Bill Validator" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Comic Book Offer" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0800, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Attract Sound" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x1000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Low Blows" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Blood" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x4000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Violence" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END

INPUT_PORTS_START( nbajam )
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
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED ) /* volume down */
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED ) /* volume up */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	    /* DS1 */
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0002, DEF_STR( On ))
	PORT_BIT( 0x001c, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME( 0x0020, 0x0020, "Video" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, "Show" )
	PORT_DIPNAME( 0x0040, 0x0040, "Validator" )
	PORT_DIPSETTING(      0x0000, "Installed" )
	PORT_DIPSETTING(      0x0040, "None" )
	PORT_DIPNAME( 0x0080, 0x0080, "Players" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPSETTING(      0x0080, "2" )
	PORT_DIPNAME( 0x0300, 0x0000, "Coin Counters" )
	PORT_DIPSETTING(      0x0300, "1 Counter, 1 count/coin" )
	PORT_DIPSETTING(      0x0200, "1 Counter, Totalizing" )
	PORT_DIPSETTING(      0x0100, "2 Counters, 1 count/coin" )
	PORT_DIPSETTING(      0x0000, "1 Counter, 1 count/coin" )
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
	wms_tunit_to_shiftreg,			/* write to shiftreg function */
	wms_tunit_from_shiftreg,		/* read from shiftreg function */
	wms_tunit_display_addr_changed,	/* display address changed */
	0								/* display interrupt callback */
};



/*************************************
 *
 *	Machine drivers
 *
 *************************************/

static struct MachineDriver machine_driver_tunit_adpcm =
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
		SOUND_CPU_WILLIAMS_ADPCM
	},
	MKLA5_FPS, MKLA5_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	wms_tunit_init_machine,

	/* video hardware */
	512, 288, { 56, 450, 0, 253 },

	0,
	65536,65536,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	wms_tunit_vh_start,
	wms_tunit_vh_stop,
	wms_tunit_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		SOUND_WILLIAMS_ADPCM(REGION_SOUND1)
	},
	nvram_handler
};

static struct MachineDriver machine_driver_tunit_dcs =
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
		SOUND_CPU_WILLIAMS_DCS
	},
	MKLA5_FPS, MKLA5_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	wms_tunit_init_machine,

	/* video hardware */
	512, 288, { 56, 450, 0, 253 },

	0,
	65536,65536,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	wms_tunit_vh_start,
	wms_tunit_vh_stop,
	wms_tunit_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		SOUND_WILLIAMS_DCS
	},
	nvram_handler
};



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( mk )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) 	/* sound CPU */
	ROM_LOAD (  "mks-u3.rom", 0x10000, 0x40000, 0xc615844c )

	ROM_REGION( 0xc0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "mks-u12.rom", 0x00000, 0x40000, 0x258bd7f9 )
	ROM_LOAD ( "mks-u13.rom", 0x40000, 0x40000, 0x7b7ec3b6 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "mkt-uj12.bin",  0x00000, 0x80000, 0xf4990bf2 )
	ROM_LOAD_EVEN( "mkt-ug12.bin",  0x00000, 0x80000, 0xb06aeac1 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "mkt-ug14.bin",  0x000000, 0x80000, 0x9e00834e )
	ROM_LOAD ( "mkt-ug16.bin",  0x080000, 0x80000, 0x52c9d1e5 )
	ROM_LOAD ( "mkt-ug17.bin",  0x100000, 0x80000, 0xe34fe253 )

	ROM_LOAD ( "mkt-uj14.bin",  0x300000, 0x80000, 0xf4b0aaa7 )
	ROM_LOAD ( "mkt-uj16.bin",  0x380000, 0x80000, 0xc94c58cf )
	ROM_LOAD ( "mkt-uj17.bin",  0x400000, 0x80000, 0xa56e12f5 )

	ROM_LOAD ( "mkt-ug19.bin",  0x600000, 0x80000, 0x2d8c7ba1 )
	ROM_LOAD ( "mkt-ug20.bin",  0x680000, 0x80000, 0x2f7e55d3 )
	ROM_LOAD ( "mkt-ug22.bin",  0x700000, 0x80000, 0xb537bb4e )

	ROM_LOAD ( "mkt-uj19.bin",  0x900000, 0x80000, 0x33b9b7a4 )
	ROM_LOAD ( "mkt-uj20.bin",  0x980000, 0x80000, 0xeae96df0 )
	ROM_LOAD ( "mkt-uj22.bin",  0xa00000, 0x80000, 0x5e12523b )
ROM_END

ROM_START( mk2 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2 )	/* ADSP-2105 data */
	ROM_LOAD (   "su2.l1",  ADSP2100_SIZE + 0x000000, 0x80000, 0x5f23d71d )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD (   "su3.l1",  ADSP2100_SIZE + 0x100000, 0x80000, 0xd6d92bf9 )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD (   "su4.l1",  ADSP2100_SIZE + 0x200000, 0x80000, 0xeebc8e0f )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD (   "su5.l1",  ADSP2100_SIZE + 0x300000, 0x80000, 0x2b0b7961 )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD (   "su6.l1",  ADSP2100_SIZE + 0x400000, 0x80000, 0xf694b27f )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD (   "su7.l1",  ADSP2100_SIZE + 0x500000, 0x80000, 0x20387e0a )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "uj12.l31",  0x00000, 0x80000, 0xcf100a75 )
	ROM_LOAD_EVEN( "ug12.l31",  0x00000, 0x80000, 0x582c7dfd )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "ug14-vid",  0x000000, 0x100000, 0x01e73af6 )
	ROM_LOAD ( "ug16-vid",  0x100000, 0x100000, 0x8ba6ae18 )
	ROM_LOAD ( "ug17-vid",  0x200000, 0x100000, 0x937d8620 )

	ROM_LOAD ( "uj14-vid",  0x300000, 0x100000, 0xd4985cbb )
	ROM_LOAD ( "uj16-vid",  0x400000, 0x100000, 0x39d885b4 )
	ROM_LOAD ( "uj17-vid",  0x500000, 0x100000, 0x218de160 )

	ROM_LOAD ( "ug19-vid",  0x600000, 0x100000, 0xfec137be )
	ROM_LOAD ( "ug20-vid",  0x700000, 0x100000, 0x809118c1 )
	ROM_LOAD ( "ug22-vid",  0x800000, 0x100000, 0x154d53b1 )

	ROM_LOAD ( "uj19-vid",  0x900000, 0x100000, 0x2d763156 )
	ROM_LOAD ( "uj20-vid",  0xa00000, 0x100000, 0xb96824f0 )
	ROM_LOAD ( "uj22-vid",  0xb00000, 0x100000, 0x8891d785 )
ROM_END

ROM_START( mk2r32 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2 )	/* ADSP-2105 data */
	ROM_LOAD (   "su2.l1",  ADSP2100_SIZE + 0x000000, 0x80000, 0x5f23d71d )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD (   "su3.l1",  ADSP2100_SIZE + 0x100000, 0x80000, 0xd6d92bf9 )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD (   "su4.l1",  ADSP2100_SIZE + 0x200000, 0x80000, 0xeebc8e0f )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD (   "su5.l1",  ADSP2100_SIZE + 0x300000, 0x80000, 0x2b0b7961 )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD (   "su6.l1",  ADSP2100_SIZE + 0x400000, 0x80000, 0xf694b27f )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD (   "su7.l1",  ADSP2100_SIZE + 0x500000, 0x80000, 0x20387e0a )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "uj12.l32",  0x00000, 0x80000, 0x43f773a6 )
	ROM_LOAD_EVEN( "ug12.l32",  0x00000, 0x80000, 0xdcde9619 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "ug14-vid",  0x000000, 0x100000, 0x01e73af6 )
	ROM_LOAD ( "ug16-vid",  0x100000, 0x100000, 0x8ba6ae18 )
	ROM_LOAD ( "ug17-vid",  0x200000, 0x100000, 0x937d8620 )

	ROM_LOAD ( "uj14-vid",  0x300000, 0x100000, 0xd4985cbb )
	ROM_LOAD ( "uj16-vid",  0x400000, 0x100000, 0x39d885b4 )
	ROM_LOAD ( "uj17-vid",  0x500000, 0x100000, 0x218de160 )

	ROM_LOAD ( "ug19-vid",  0x600000, 0x100000, 0xfec137be )
	ROM_LOAD ( "ug20-vid",  0x700000, 0x100000, 0x809118c1 )
	ROM_LOAD ( "ug22-vid",  0x800000, 0x100000, 0x154d53b1 )

	ROM_LOAD ( "uj19-vid",  0x900000, 0x100000, 0x2d763156 )
	ROM_LOAD ( "uj20-vid",  0xa00000, 0x100000, 0xb96824f0 )
	ROM_LOAD ( "uj22-vid",  0xb00000, 0x100000, 0x8891d785 )
ROM_END

ROM_START( mk2r14 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2 )	/* ADSP-2105 data */
	ROM_LOAD (   "su2.l1",  ADSP2100_SIZE + 0x000000, 0x80000, 0x5f23d71d )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD (   "su3.l1",  ADSP2100_SIZE + 0x100000, 0x80000, 0xd6d92bf9 )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD (   "su4.l1",  ADSP2100_SIZE + 0x200000, 0x80000, 0xeebc8e0f )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD (   "su5.l1",  ADSP2100_SIZE + 0x300000, 0x80000, 0x2b0b7961 )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD (   "su6.l1",  ADSP2100_SIZE + 0x400000, 0x80000, 0xf694b27f )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD (   "su7.l1",  ADSP2100_SIZE + 0x500000, 0x80000, 0x20387e0a )
	ROM_RELOAD ( 			ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "uj12.l14",  0x00000, 0x80000, 0x6d43bc6d )
	ROM_LOAD_EVEN( "ug12.l14",  0x00000, 0x80000, 0x42b0da21 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "ug14-vid",  0x000000, 0x100000, 0x01e73af6 )
	ROM_LOAD ( "ug16-vid",  0x100000, 0x100000, 0x8ba6ae18 )
	ROM_LOAD ( "ug17-vid",  0x200000, 0x100000, 0x937d8620 )

	ROM_LOAD ( "uj14-vid",  0x300000, 0x100000, 0xd4985cbb )
	ROM_LOAD ( "uj16-vid",  0x400000, 0x100000, 0x39d885b4 )
	ROM_LOAD ( "uj17-vid",  0x500000, 0x100000, 0x218de160 )

	ROM_LOAD ( "ug19-vid",  0x600000, 0x100000, 0xfec137be )
	ROM_LOAD ( "ug20-vid",  0x700000, 0x100000, 0x809118c1 )
	ROM_LOAD ( "ug22-vid",  0x800000, 0x100000, 0x154d53b1 )

	ROM_LOAD ( "uj19-vid",  0x900000, 0x100000, 0x2d763156 )
	ROM_LOAD ( "uj20-vid",  0xa00000, 0x100000, 0xb96824f0 )
	ROM_LOAD ( "uj22-vid",  0xb00000, 0x100000, 0x8891d785 )
ROM_END

/*
    equivalences for the extension board version (same contents, split in half)

	ROM_LOAD ( "ug14.l1",   0x000000, 0x080000, 0x74f5aaf1 )
	ROM_LOAD ( "ug16.l11",  0x080000, 0x080000, 0x1cf58c4c )
	ROM_LOAD ( "u8.l1",     0x200000, 0x080000, 0x56e22ff5 )
	ROM_LOAD ( "u11.l1",    0x280000, 0x080000, 0x559ca4a3 )
	ROM_LOAD ( "ug17.l1",   0x100000, 0x080000, 0x4202d8bf )
	ROM_LOAD ( "ug18.l1",   0x180000, 0x080000, 0xa3deab6a )

	ROM_LOAD ( "uj14.l1",   0x300000, 0x080000, 0x869a3c55 )
	ROM_LOAD ( "uj16.l11",  0x380000, 0x080000, 0xc70cf053 )
	ROM_LOAD ( "u9.l1",     0x500000, 0x080000, 0x67da0769 )
	ROM_LOAD ( "u10.l1",    0x580000, 0x080000, 0x69000ac3 )
	ROM_LOAD ( "uj17.l1",   0x400000, 0x080000, 0xec3e1884 )
	ROM_LOAD ( "uj18.l1",   0x480000, 0x080000, 0xc9f5aef4 )

	ROM_LOAD ( "u6.l1",     0x600000, 0x080000, 0x8d4c496a )
	ROM_LOAD ( "u13.l11",   0x680000, 0x080000, 0x7fb20a45 )
	ROM_LOAD ( "ug19.l1",   0x800000, 0x080000, 0xd6c1f75e )
	ROM_LOAD ( "ug20.l1",   0x880000, 0x080000, 0x19a33cff )
	ROM_LOAD ( "ug22.l1",   0x700000, 0x080000, 0xdb6cfa45 )
	ROM_LOAD ( "ug23.l1",   0x780000, 0x080000, 0xbfd8b656 )

	ROM_LOAD ( "u7.l1",     0x900000, 0x080000, 0x3988aac8 )
	ROM_LOAD ( "u12.l11",   0x980000, 0x080000, 0x2ef12cc6 )
	ROM_LOAD ( "uj19.l1",   0xb00000, 0x080000, 0x4eed6f18 )
	ROM_LOAD ( "uj20.l1",   0xb80000, 0x080000, 0x337b1e20 )
	ROM_LOAD ( "uj22.l1",   0xa00000, 0x080000, 0xa6546b15 )
	ROM_LOAD ( "uj23.l1",   0xa80000, 0x080000, 0x45867c6f )
*/

ROM_START( nbajam )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "nbau3.bin",  0x010000, 0x20000, 0x3a3ea480 )
	ROM_RELOAD (              0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "nbau12.bin",  0x000000, 0x80000, 0xb94847f1 )
	ROM_LOAD ( "nbau13.bin",  0x080000, 0x80000, 0xb6fe24bd )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "nbauj12.bin",  0x00000, 0x80000, 0xb93e271c )
	ROM_LOAD_EVEN( "nbaug12.bin",  0x00000, 0x80000, 0x407d3390 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "nbaug14.bin",  0x000000, 0x80000, 0x04bb9f64 )
	ROM_LOAD ( "nbaug16.bin",  0x080000, 0x80000, 0x8591c572 )
	ROM_LOAD ( "nbaug17.bin",  0x100000, 0x80000, 0x6f921886 )
	ROM_LOAD ( "nbaug18.bin",  0x180000, 0x80000, 0x5162d3d6 )

	ROM_LOAD ( "nbauj14.bin",  0x300000, 0x80000, 0xb34b7af3 )
	ROM_LOAD ( "nbauj16.bin",  0x380000, 0x80000, 0xd2e554f1 )
	ROM_LOAD ( "nbauj17.bin",  0x400000, 0x80000, 0xb2e14981 )
	ROM_LOAD ( "nbauj18.bin",  0x480000, 0x80000, 0xfdee0037 )

	ROM_LOAD ( "nbaug19.bin",  0x600000, 0x80000, 0xa8f22fbb )
	ROM_LOAD ( "nbaug20.bin",  0x680000, 0x80000, 0x44fd6221 )
	ROM_LOAD ( "nbaug22.bin",  0x700000, 0x80000, 0xab05ed89 )
	ROM_LOAD ( "nbaug23.bin",  0x780000, 0x80000, 0x7b934c7a )

	ROM_LOAD ( "nbauj19.bin",  0x900000, 0x80000, 0x8130a8a2 )
	ROM_LOAD ( "nbauj20.bin",  0x980000, 0x80000, 0xf9cebbb6 )
	ROM_LOAD ( "nbauj22.bin",  0xa00000, 0x80000, 0x59a95878 )
	ROM_LOAD ( "nbauj23.bin",  0xa80000, 0x80000, 0x427d2eee )
ROM_END

ROM_START( nbajamr2 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "nbau3.bin",  0x010000, 0x20000, 0x3a3ea480 )
	ROM_RELOAD (              0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "nbau12.bin",  0x000000, 0x80000, 0xb94847f1 )
	ROM_LOAD ( "nbau13.bin",  0x080000, 0x80000, 0xb6fe24bd )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "jam2uj12.bin",  0x00000, 0x80000, 0x0fe80b36 )
	ROM_LOAD_EVEN( "jam2ug12.bin",  0x00000, 0x80000, 0x5d106315 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "nbaug14.bin",  0x000000, 0x80000, 0x04bb9f64 )
	ROM_LOAD ( "nbaug16.bin",  0x080000, 0x80000, 0x8591c572 )
	ROM_LOAD ( "nbaug17.bin",  0x100000, 0x80000, 0x6f921886 )
	ROM_LOAD ( "nbaug18.bin",  0x180000, 0x80000, 0x5162d3d6 )

	ROM_LOAD ( "nbauj14.bin",  0x300000, 0x80000, 0xb34b7af3 )
	ROM_LOAD ( "nbauj16.bin",  0x380000, 0x80000, 0xd2e554f1 )
	ROM_LOAD ( "nbauj17.bin",  0x400000, 0x80000, 0xb2e14981 )
	ROM_LOAD ( "nbauj18.bin",  0x480000, 0x80000, 0xfdee0037 )

	ROM_LOAD ( "nbaug19.bin",  0x600000, 0x80000, 0xa8f22fbb )
	ROM_LOAD ( "nbaug20.bin",  0x680000, 0x80000, 0x44fd6221 )
	ROM_LOAD ( "nbaug22.bin",  0x700000, 0x80000, 0xab05ed89 )
	ROM_LOAD ( "nbaug23.bin",  0x780000, 0x80000, 0x7b934c7a )

	ROM_LOAD ( "nbauj19.bin",  0x900000, 0x80000, 0x8130a8a2 )
	ROM_LOAD ( "nbauj20.bin",  0x980000, 0x80000, 0xf9cebbb6 )
	ROM_LOAD ( "nbauj22.bin",  0xa00000, 0x80000, 0x59a95878 )
	ROM_LOAD ( "nbauj23.bin",  0xa80000, 0x80000, 0x427d2eee )
ROM_END

ROM_START( nbajamte )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "te-u3.bin",  0x010000, 0x20000, 0xd4551195 )
	ROM_RELOAD (              0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "te-u12.bin",  0x000000, 0x80000, 0x4fac97bc )
	ROM_LOAD ( "te-u13.bin",  0x080000, 0x80000, 0x6f27b202 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "te-uj12.l4",  0x00000, 0x80000, 0xd7c21bc4 )
	ROM_LOAD_EVEN( "te-ug12.l4",  0x00000, 0x80000, 0x7ad49229 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "nbaug14.bin",  0x000000, 0x80000, 0x04bb9f64 )
	ROM_LOAD ( "te-ug16.bin",  0x080000, 0x80000, 0xc7ce74d0 )
	ROM_LOAD ( "te-ug17.bin",  0x100000, 0x80000, 0x9401be62 )
	ROM_LOAD ( "te-ug18.bin",  0x180000, 0x80000, 0x6fd08f57 )

	ROM_LOAD ( "nbauj14.bin",  0x300000, 0x80000, 0xb34b7af3 )
	ROM_LOAD ( "te-uj16.bin",  0x380000, 0x80000, 0x905ad88b )
	ROM_LOAD ( "te-uj17.bin",  0x400000, 0x80000, 0x8a852b9e )
	ROM_LOAD ( "te-uj18.bin",  0x480000, 0x80000, 0x4eb73c26 )

	ROM_LOAD ( "nbaug19.bin",  0x600000, 0x80000, 0xa8f22fbb )
	ROM_LOAD ( "te-ug20.bin",  0x680000, 0x80000, 0x8a48728c )
	ROM_LOAD ( "te-ug22.bin",  0x700000, 0x80000, 0x3b05133b )
	ROM_LOAD ( "te-ug23.bin",  0x780000, 0x80000, 0x854f73bc )

	ROM_LOAD ( "nbauj19.bin",  0x900000, 0x80000, 0x8130a8a2 )
	ROM_LOAD ( "te-uj20.bin",  0x980000, 0x80000, 0xbf263d61 )
	ROM_LOAD ( "te-uj22.bin",  0xa00000, 0x80000, 0x39791051 )
	ROM_LOAD ( "te-uj23.bin",  0xa80000, 0x80000, 0xf8c30998 )
ROM_END

ROM_START( nbajamt1 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "te-u3.bin",  0x010000, 0x20000, 0xd4551195 )
	ROM_RELOAD (              0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "te-u12.bin",  0x000000, 0x80000, 0x4fac97bc )
	ROM_LOAD ( "te-u13.bin",  0x080000, 0x80000, 0x6f27b202 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "te-uj12.l1",  0x00000, 0x80000, 0xa9f555ad )
	ROM_LOAD_EVEN( "te-ug12.l1",  0x00000, 0x80000, 0xbd4579b5 )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "nbaug14.bin",  0x000000, 0x80000, 0x04bb9f64 )
	ROM_LOAD ( "te-ug16.bin",  0x080000, 0x80000, 0xc7ce74d0 )
	ROM_LOAD ( "te-ug17.bin",  0x100000, 0x80000, 0x9401be62 )
	ROM_LOAD ( "te-ug18.bin",  0x180000, 0x80000, 0x6fd08f57 )

	ROM_LOAD ( "nbauj14.bin",  0x300000, 0x80000, 0xb34b7af3 )
	ROM_LOAD ( "te-uj16.bin",  0x380000, 0x80000, 0x905ad88b )
	ROM_LOAD ( "te-uj17.bin",  0x400000, 0x80000, 0x8a852b9e )
	ROM_LOAD ( "te-uj18.bin",  0x480000, 0x80000, 0x4eb73c26 )

	ROM_LOAD ( "nbaug19.bin",  0x600000, 0x80000, 0xa8f22fbb )
	ROM_LOAD ( "te-ug20.bin",  0x680000, 0x80000, 0x8a48728c )
	ROM_LOAD ( "te-ug22.bin",  0x700000, 0x80000, 0x3b05133b )
	ROM_LOAD ( "te-ug23.bin",  0x780000, 0x80000, 0x854f73bc )

	ROM_LOAD ( "nbauj19.bin",  0x900000, 0x80000, 0x8130a8a2 )
	ROM_LOAD ( "te-uj20.bin",  0x980000, 0x80000, 0xbf263d61 )
	ROM_LOAD ( "te-uj22.bin",  0xa00000, 0x80000, 0x39791051 )
	ROM_LOAD ( "te-uj23.bin",  0xa80000, 0x80000, 0xf8c30998 )
ROM_END

ROM_START( nbajamt2 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "te-u3.bin",  0x010000, 0x20000, 0xd4551195 )
	ROM_RELOAD (              0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "te-u12.bin",  0x000000, 0x80000, 0x4fac97bc )
	ROM_LOAD ( "te-u13.bin",  0x080000, 0x80000, 0x6f27b202 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "te-uj12.l2",  0x00000, 0x80000, 0xeaa6fb32 )
	ROM_LOAD_EVEN( "te-ug12.l2",  0x00000, 0x80000, 0x5a694d9a )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "nbaug14.bin",  0x000000, 0x80000, 0x04bb9f64 )
	ROM_LOAD ( "te-ug16.bin",  0x080000, 0x80000, 0xc7ce74d0 )
	ROM_LOAD ( "te-ug17.bin",  0x100000, 0x80000, 0x9401be62 )
	ROM_LOAD ( "te-ug18.bin",  0x180000, 0x80000, 0x6fd08f57 )

	ROM_LOAD ( "nbauj14.bin",  0x300000, 0x80000, 0xb34b7af3 )
	ROM_LOAD ( "te-uj16.bin",  0x380000, 0x80000, 0x905ad88b )
	ROM_LOAD ( "te-uj17.bin",  0x400000, 0x80000, 0x8a852b9e )
	ROM_LOAD ( "te-uj18.bin",  0x480000, 0x80000, 0x4eb73c26 )

	ROM_LOAD ( "nbaug19.bin",  0x600000, 0x80000, 0xa8f22fbb )
	ROM_LOAD ( "te-ug20.bin",  0x680000, 0x80000, 0x8a48728c )
	ROM_LOAD ( "te-ug22.bin",  0x700000, 0x80000, 0x3b05133b )
	ROM_LOAD ( "te-ug23.bin",  0x780000, 0x80000, 0x854f73bc )

	ROM_LOAD ( "nbauj19.bin",  0x900000, 0x80000, 0x8130a8a2 )
	ROM_LOAD ( "te-uj20.bin",  0x980000, 0x80000, 0xbf263d61 )
	ROM_LOAD ( "te-uj22.bin",  0xa00000, 0x80000, 0x39791051 )
	ROM_LOAD ( "te-uj23.bin",  0xa80000, 0x80000, 0xf8c30998 )
ROM_END

ROM_START( nbajamt3 )
	ROM_REGION( 0x10, REGION_CPU1 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD (  "te-u3.bin",  0x010000, 0x20000, 0xd4551195 )
	ROM_RELOAD (              0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1 )	/* ADPCM */
	ROM_LOAD ( "te-u12.bin",  0x000000, 0x80000, 0x4fac97bc )
	ROM_LOAD ( "te-u13.bin",  0x080000, 0x80000, 0x6f27b202 )

	ROM_REGION( 0x100000, REGION_USER1 | REGIONFLAG_DISPOSE )	/* 34010 code */
	ROM_LOAD_ODD ( "te-uj12.l3",  0x00000, 0x80000, 0x8fdf77b4 )
	ROM_LOAD_EVEN( "te-ug12.l3",  0x00000, 0x80000, 0x656579ed )

	ROM_REGION( 0xc00000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD ( "nbaug14.bin",  0x000000, 0x80000, 0x04bb9f64 )
	ROM_LOAD ( "te-ug16.bin",  0x080000, 0x80000, 0xc7ce74d0 )
	ROM_LOAD ( "te-ug17.bin",  0x100000, 0x80000, 0x9401be62 )
	ROM_LOAD ( "te-ug18.bin",  0x180000, 0x80000, 0x6fd08f57 )

	ROM_LOAD ( "nbauj14.bin",  0x300000, 0x80000, 0xb34b7af3 )
	ROM_LOAD ( "te-uj16.bin",  0x380000, 0x80000, 0x905ad88b )
	ROM_LOAD ( "te-uj17.bin",  0x400000, 0x80000, 0x8a852b9e )
	ROM_LOAD ( "te-uj18.bin",  0x480000, 0x80000, 0x4eb73c26 )

	ROM_LOAD ( "nbaug19.bin",  0x600000, 0x80000, 0xa8f22fbb )
	ROM_LOAD ( "te-ug20.bin",  0x680000, 0x80000, 0x8a48728c )
	ROM_LOAD ( "te-ug22.bin",  0x700000, 0x80000, 0x3b05133b )
	ROM_LOAD ( "te-ug23.bin",  0x780000, 0x80000, 0x854f73bc )

	ROM_LOAD ( "nbauj19.bin",  0x900000, 0x80000, 0x8130a8a2 )
	ROM_LOAD ( "te-uj20.bin",  0x980000, 0x80000, 0xbf263d61 )
	ROM_LOAD ( "te-uj22.bin",  0xa00000, 0x80000, 0x39791051 )
	ROM_LOAD ( "te-uj23.bin",  0xa80000, 0x80000, 0xf8c30998 )
ROM_END



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1992, mk,       0,       tunit_adpcm, mk,      mk,       ROT0_16BIT, "Midway",   "Mortal Kombat (rev 5.0 T-Unit 03/19/93)" )

GAME( 1993, mk2,      0,       tunit_dcs,   mk2,     mk2,      ROT0_16BIT, "Midway",   "Mortal Kombat II (rev L3.1)" )
GAME( 1993, mk2r32,   mk2,     tunit_dcs,   mk2,     mk2r32,   ROT0_16BIT, "Midway",   "Mortal Kombat II (rev L3.2 (European))" )
GAME( 1993, mk2r14,   mk2,     tunit_dcs,   mk2,     mk2r14,   ROT0_16BIT, "Midway",   "Mortal Kombat II (rev L1.4)" )

GAME( 1993, nbajam,   0,       tunit_adpcm, nbajam,  nbajam,   ROT0_16BIT, "Midway",   "NBA Jam (rev 3.01 04/07/93)" )
GAME( 1993, nbajamr2, nbajam,  tunit_adpcm, nbajam,  nbajam20, ROT0_16BIT, "Midway",   "NBA Jam (rev 2.00 02/10/93)" )
GAME( 1994, nbajamte, nbajam,  tunit_adpcm, nbajam,  nbajamte, ROT0_16BIT, "Midway",   "NBA Jam TE (rev 4.0 03/23/94)" )
GAME( 1994, nbajamt1, nbajam,  tunit_adpcm, nbajam,  nbajamte, ROT0_16BIT, "Midway",   "NBA Jam TE (rev 1.0 01/17/94)" )
GAME( 1994, nbajamt2, nbajam,  tunit_adpcm, nbajam,  nbajamte, ROT0_16BIT, "Midway",   "NBA Jam TE (rev 2.0 01/28/94)" )
GAME( 1994, nbajamt3, nbajam,  tunit_adpcm, nbajam,  nbajamte, ROT0_16BIT, "Midway",   "NBA Jam TE (rev 3.0 03/04/94)" )
