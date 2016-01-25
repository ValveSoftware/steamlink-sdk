#include "../machine/system16.cpp"
#include "../sndhrdw/system16.cpp"
#include "../vidhrdw/system16.cpp"

/*


--------
  ASTORMBL
          3. In the ending, the 3 heroes are floating into a half bubble. (see picture). Also colour problems during ending as well.
          4. In the later Shooting gallery stage (like inside the car shop and the factory (mission 3)),
		  there is some garbage graphics (sprite of death monsters that appear where they should not)


	working:
		Alex Kidd
		Alien Storm (bootleg)
		Alien Syndrome
		Altered Beast (Ver 1)
		Altered Beast (Ver 2)	(No Sound)
		Atomic Point			(No Sound)
		Aurail					(Speech quality sounds poor)
		Aurail (317-0168)
		Bay Route
		Body Slam
	    Dump Matsumoto (Japan, Body Slam)
		Dynamite Dux (bootleg)
		Enduro Racer (bootleg)
		Enduro Racer (custom bootleg)
		E-Swat (bootleg)
		Fantasy Zone (Old Ver.)
		Fantasy Zone (New Ver.)
		Flash Point  (bootleg)
		Golden Axe (Ver 1)
		Golden Axe (Ver 2)
		Hang-on
		Heavyweight Champ: some minor graphics glitches
		Major League: No game over.
		Moonwalker (bootleg): Music Speed varies
		Outrun (set 1)
		Outrun (set 2)
		Outrun (custom bootleg)
		Passing Shot (bootleg)
		Passing Shot (4 player bootleg)
		Quartet: Glitch on highscore list
		Quartet (Japan): Glitch on highscore list
		Quartet 2: Glitch on highscore list
		Riot City
		SDI
		Shadow Dancer
		Shadow Dancer (Japan)
		Shinobi
		Shinobi (Sys16A Bootleg?)
		Space Harrier
		Super Hangon (bootleg)
		Tetris (bootleg)
		Time Scanner
		Toryumon
		Tough Turf (Japan)			(No Sound)
		Tough Turf (US)				(No Sound)
		Tough Turf (bootleg)	(No Speech Roms)
		Wonderboy 3 - Monster Lair
		Wonderboy 3 - Monster Lair (bootleg)
		Wrestle War

	not really working:
		Shadow Dancer (bootleg)

	protected:
		Alex Kidd (jpn?)
		Alien Syndrome
		Alien Syndrome
		Alien Syndrome (Japan)
		Alien Storm
		Alien Storm (2 Player)
		Bay Route (317-0116)
		Bay Route (protected bootleg 1)
		Bay Route (protected bootleg 2)
		Enduro Racer
		E-Swat
		Flash Point
		Golden Axe (Ver 1 317-0121 Japan)
		Golden Axe (Ver 2 317-0110)
		Golden Axe (Ver 2 317-0122)
		Golden Axe (protected bootleg)
		Jyuohki (Japan, altered beast)
		Moonwalker (317-0158)
		Moonwalker (317-0159)
		Passing Shot (317-0080)
		Shinobi (Sys16B 317-0049)
		Shinobi (Sys16A 317-0050)
		SDI (Japan, old version)
		Super Hangon
		Tetris (Type A)
		Tetris (Type B 317-0092)
		Wonderboy 3 - Monster Lair (317-0089)

	protected (No driver):
		Ace Attacker
		Action Fighter
		Bloxeed
		Clutch Hitter
		Cotton (Japan)
		Cotton
		DD Crew
		Dunk Shot
		Excite League
		Laser Ghost
		Line of Fire
		MVP
		Ryukyu
		Super Leagu
		Thunder Blade
		Thunder Blade (Japan)
		Turbo Outrun
		Turbo Outrun (Set 2)

	not working (No driver):
		After Burner
		After Burner II

*/

#define SYS16_CREDITS \
	"Thierry Lescot & Nao (Hardware Info)\n" \
	"Mirko Buffoni (MAME driver)\n" \
	"Andrew Prime\n" \
	"Phil Stroffolino"


//#define SPACEHARRIER_OFFSETS


/*
This should be enabled when the sprite manager fully handles the special
left/right side markers. This will fix graphics glitches in several games,
including ESwat, Alien Storm and Altered Beast.
*/
#define SPRITE_SIDE_MARKERS
#define TRANSPARENT_SHADOWS

#ifdef TRANSPARENT_SHADOWS
#define NumOfShadowColors 32
#define ShadowColorsMultiplier 2
extern int sys16_sh_shadowpal;
#else
#define ShadowColorsMultiplier 1
#endif
#ifdef TRANSPARENT_SHADOWS
extern int sys16_MaxShadowColors;
#endif

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "cpu/i8039/i8039.h"

/***************************************************************************/

READ_HANDLER( sys16_tileram_r );
WRITE_HANDLER( sys16_tileram_w );
READ_HANDLER( sys16_textram_r );
WRITE_HANDLER( sys16_textram_w );
extern int sys16_vh_start( void );
extern void sys16_vh_stop( void );
extern void sys16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( sys16_paletteram_w );

extern int sys16_ho_vh_start( void );
extern void sys16_ho_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern int sys16_or_vh_start( void );
extern void sys16_or_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern int sys18_vh_start( void );
extern void sys18_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* video driver constants (vary with game) */
extern int sys16_spritesystem;
extern int sys16_sprxoffset;
extern int sys16_bgxoffset;
extern int sys16_fgxoffset;
extern int *sys16_obj_bank;
extern int sys16_textmode;
extern int sys16_textlayer_lo_min;
extern int sys16_textlayer_lo_max;
extern int sys16_textlayer_hi_min;
extern int sys16_textlayer_hi_max;
extern int sys16_dactype;
extern int sys16_bg1_trans;
extern int sys16_bg_priority_mode;
extern int sys16_fg_priority_mode;
extern int sys16_bg_priority_value;
extern int sys16_fg_priority_value;
extern int sys16_spritelist_end;
extern int sys16_tilebank_switch;
extern int sys16_rowscroll_scroll;
extern int sys16_quartet_title_kludge;
void (* sys16_update_proc)( void );

/* video driver registers */
extern int sys16_refreshenable;
extern int sys16_clear_screen;
extern int sys16_tile_bank0;
extern int sys16_tile_bank1;
extern int sys16_bg_scrollx, sys16_bg_scrolly;
extern int sys16_bg_page[4];
extern int sys16_fg_scrollx, sys16_fg_scrolly;
extern int sys16_fg_page[4];

extern int sys16_bg2_scrollx, sys16_bg2_scrolly;
extern int sys16_bg2_page[4];
extern int sys16_fg2_scrollx, sys16_fg2_scrolly;
extern int sys16_fg2_page[4];

extern int sys18_bg2_active;
extern int sys18_fg2_active;
extern unsigned char *sys18_splittab_bg_x;
extern unsigned char *sys18_splittab_bg_y;
extern unsigned char *sys18_splittab_fg_x;
extern unsigned char *sys18_splittab_fg_y;

#ifdef SPACEHARRIER_OFFSETS
extern unsigned char *spaceharrier_patternoffsets;
#endif
extern unsigned char *gr_ver;
extern unsigned char *gr_hor;
extern unsigned char *gr_pal;
extern unsigned char *gr_flip;
extern int gr_palette;
extern int gr_palette_default;
extern unsigned char gr_colorflip[2][4];
extern unsigned char *gr_second_road;

/* video driver has access to these memory regions */
unsigned char *sys16_tileram;
unsigned char *sys16_textram;
unsigned char *sys16_spriteram;

/* other memory regions */
static unsigned char *sys16_workingram;
static unsigned char *sys16_extraram;
static unsigned char *sys16_extraram2;
static unsigned char *sys16_extraram3;
static unsigned char *sys16_extraram4;
static unsigned char *sys16_extraram5;

// 7751 emulation
WRITE_HANDLER( sys16_7751_audio_8255_w );
 READ_HANDLER( sys16_7751_audio_8255_r );
 READ_HANDLER( sys16_7751_sh_rom_r );
 READ_HANDLER( sys16_7751_sh_t1_r );
 READ_HANDLER( sys16_7751_sh_command_r );
WRITE_HANDLER( sys16_7751_sh_dac_w );
WRITE_HANDLER( sys16_7751_sh_busy_w );
WRITE_HANDLER( sys16_7751_sh_offset_a0_a3_w );
WRITE_HANDLER( sys16_7751_sh_offset_a4_a7_w );
WRITE_HANDLER( sys16_7751_sh_offset_a8_a11_w );
WRITE_HANDLER( sys16_7751_sh_rom_select_w );


// encryption decoding
void endurob2_decode_data(unsigned char *dest,unsigned char *source,int size);
void endurob2_decode_data2(unsigned char *dest,unsigned char *source,int size);
void enduror_decode_data(unsigned char *dest,unsigned char *source,int size);
void enduror_decode_data2(unsigned char *dest,unsigned char *source,int size);

void aurail_decode_data(unsigned char *dest,unsigned char *source,int size);
void aurail_decode_opcode1(unsigned char *dest,unsigned char *source,int size);
void aurail_decode_opcode2(unsigned char *dest,unsigned char *source,int size);

/***************************************************************************/

#define MWA_PALETTERAM	sys16_paletteram_w, &paletteram
#define MRA_PALETTERAM	paletteram_word_r

#define MRA_WORKINGRAM	MRA_BANK1
#define MWA_WORKINGRAM	MWA_BANK1,&sys16_workingram

#define MRA_SPRITERAM	MRA_BANK2
#define MWA_SPRITERAM	MWA_BANK2,&sys16_spriteram

#define MRA_TILERAM		sys16_tileram_r
#define MWA_TILERAM		sys16_tileram_w,&sys16_tileram

#define MRA_TEXTRAM		sys16_textram_r
#define MWA_TEXTRAM		sys16_textram_w,&sys16_textram

#define MRA_EXTRAM		MRA_BANK3
#define MWA_EXTRAM		MWA_BANK3,&sys16_extraram

#define MRA_EXTRAM2		MRA_BANK4
#define MWA_EXTRAM2		MWA_BANK4,&sys16_extraram2

#define MRA_EXTRAM3		MRA_BANK5
#define MWA_EXTRAM3		MWA_BANK5,&sys16_extraram3

#define MRA_EXTRAM4		MRA_BANK6
#define MWA_EXTRAM4		MWA_BANK6,&sys16_extraram4

#define MRA_EXTRAM5		MRA_BANK7
#define MWA_EXTRAM5		MWA_BANK7,&sys16_extraram5

/***************************************************************************/

#define MACHINE_DRIVER( GAMENAME,READMEM,WRITEMEM,INITMACHINE,GFXSIZE) \
static struct MachineDriver GAMENAME = \
{ \
	{ \
		{ \
			CPU_M68000, \
			10000000, \
			READMEM,WRITEMEM,0,0, \
			sys16_interrupt,1 \
		}, \
		{ \
			CPU_Z80 | CPU_AUDIO_CPU, \
			4096000, \
			sound_readmem,sound_writemem,sound_readport,sound_writeport, \
			ignore_interrupt,1 \
		}, \
	}, \
	60, DEFAULT_60HZ_VBLANK_DURATION, \
	1, \
	INITMACHINE, \
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 }, \
	GFXSIZE, \
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier, \
	0, \
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE, \
	0, \
	sys16_vh_start, \
	sys16_vh_stop, \
	sys16_vh_screenrefresh, \
	SOUND_SUPPORTS_STEREO,0,0,0, \
	{ \
		{ \
			SOUND_YM2151, \
			&ym2151_interface \
		} \
	} \
};

#define MACHINE_DRIVER_7759( GAMENAME,READMEM,WRITEMEM,INITMACHINE,GFXSIZE, UPD7759INTF ) \
static struct MachineDriver GAMENAME = \
{ \
	{ \
		{ \
			CPU_M68000, \
			10000000, \
			READMEM,WRITEMEM,0,0, \
			sys16_interrupt,1 \
		}, \
		{ \
			CPU_Z80 | CPU_AUDIO_CPU, \
			4096000, \
			sound_readmem_7759,sound_writemem,sound_readport,sound_writeport_7759, \
			ignore_interrupt,1 \
		}, \
	}, \
	60, DEFAULT_60HZ_VBLANK_DURATION, \
	1, \
	INITMACHINE, \
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 }, \
	GFXSIZE, \
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier, \
	0, \
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE, \
	0, \
	sys16_vh_start, \
	sys16_vh_stop, \
	sys16_vh_screenrefresh, \
	SOUND_SUPPORTS_STEREO,0,0,0, \
	{ \
		{ \
			SOUND_YM2151, \
			&ym2151_interface \
		}, { \
			SOUND_UPD7759, \
			&UPD7759INTF \
		} \
	} \
};


#define MACHINE_DRIVER_7751( GAMENAME,READMEM,WRITEMEM,INITMACHINE,GFXSIZE ) \
static struct MachineDriver GAMENAME = \
{ \
	{ \
		{ \
			CPU_M68000, \
			10000000, \
			READMEM,WRITEMEM,0,0, \
			sys16_interrupt,1 \
		}, \
		{ \
			CPU_Z80 | CPU_AUDIO_CPU, \
			4096000, \
			sound_readmem_7751,sound_writemem,sound_readport_7751,sound_writeport_7751, \
			ignore_interrupt,1 \
		}, \
		{ \
			CPU_N7751 | CPU_AUDIO_CPU, \
			6000000/15,        /* 6Mhz crystal */ \
			readmem_7751,writemem_7751,readport_7751,writeport_7751, \
			ignore_interrupt,1 \
		} \
	}, \
	60, DEFAULT_60HZ_VBLANK_DURATION, \
	1, \
	INITMACHINE, \
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 }, \
	GFXSIZE, \
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier, \
	0, \
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE, \
	0, \
	sys16_vh_start, \
	sys16_vh_stop, \
	sys16_vh_screenrefresh, \
	SOUND_SUPPORTS_STEREO,0,0,0, \
	{ \
		{ \
			SOUND_YM2151, \
			&ym2151_interface \
		}, \
		{ \
			SOUND_DAC, \
			&sys16_7751_dac_interface \
		} \
	} \
};


#define MACHINE_DRIVER_18( GAMENAME,READMEM,WRITEMEM,INITMACHINE,GFXSIZE) \
static struct MachineDriver GAMENAME = \
{ \
	{ \
		{ \
			CPU_M68000, \
			10000000, \
			READMEM,WRITEMEM,0,0, \
			sys16_interrupt,1 \
		}, \
		{ \
			CPU_Z80 | CPU_AUDIO_CPU, \
			4096000*2, /* overclocked to fix sound, but wrong! */ \
			sound_readmem_18,sound_writemem_18,sound_readport_18,sound_writeport_18, \
			ignore_interrupt,1 \
		}, \
	}, \
	60, DEFAULT_60HZ_VBLANK_DURATION, \
	1, \
	INITMACHINE, \
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 }, \
	GFXSIZE, \
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier, \
	0, \
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE, \
	0, \
	sys18_vh_start, \
	sys16_vh_stop, \
	sys18_vh_screenrefresh, \
	SOUND_SUPPORTS_STEREO,0,0,0, \
	{ \
		{ \
			SOUND_YM3438, \
			&ym3438_interface \
		}, \
		{ \
			SOUND_RF5C68, \
			&rf5c68_interface, \
		} \
	} \
};



static void (*sys16_custom_irq)(void);

static void sys16_onetime_init_machine(void)
{
	sys16_bg1_trans=0;
	sys16_rowscroll_scroll=0;
	sys18_splittab_bg_x=0;
	sys18_splittab_bg_y=0;
	sys18_splittab_fg_x=0;
	sys18_splittab_fg_y=0;

	sys16_quartet_title_kludge=0;

	sys16_custom_irq=NULL;

#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=NumOfShadowColors;
#endif

#ifdef SPACEHARRIER_OFFSETS
	spaceharrier_patternoffsets=0;
#endif
}

/***************************************************************************/

int sys16_interrupt( void ){
	if(sys16_custom_irq) sys16_custom_irq();
	return 4; /* Interrupt vector 4, used by VBlank */
}

/***************************************************************************/

static void sound_cause_nmi(int chip)
{
	cpu_set_nmi_line(1, PULSE_LINE);
}

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xe800, 0xe800, soundlatch_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x01, 0x01, YM2151_status_port_0_r },
	{ 0xc0, 0xc0, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, YM2151_register_port_0_w },
	{ 0x01, 0x01, YM2151_data_port_0_w },
	{ -1 }
};



// 7751 Sound

static struct MemoryReadAddress sound_readmem_7751[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xe800, 0xe800, soundlatch_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort sound_readport_7751[] =
{
	{ 0x01, 0x01, YM2151_status_port_0_r },
//    { 0x0e, 0x0e, sys16_7751_audio_8255_r },
	{ 0xc0, 0xc0, soundlatch_r },
	{ -1 }	/* end of table */
};



static struct IOWritePort sound_writeport_7751[] =
{
	{ 0x00, 0x00, YM2151_register_port_0_w },
	{ 0x01, 0x01, YM2151_data_port_0_w },
	{ 0x80, 0x80, sys16_7751_audio_8255_w },
	{ -1 }
};

static struct MemoryReadAddress readmem_7751[] =
{
        { 0x0000, 0x03ff, MRA_ROM },
        { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem_7751[] =
{
        { 0x0000, 0x03ff, MWA_ROM },
        { -1 }  /* end of table */
};

static struct IOReadPort readport_7751[] =
{
        { I8039_t1,  I8039_t1,  sys16_7751_sh_t1_r },
        { I8039_p2,  I8039_p2,  sys16_7751_sh_command_r },
        { I8039_bus, I8039_bus, sys16_7751_sh_rom_r },
        { -1 }  /* end of table */
};

static struct IOWritePort writeport_7751[] =
{
        { I8039_p1, I8039_p1, sys16_7751_sh_dac_w },
        { I8039_p2, I8039_p2, sys16_7751_sh_busy_w },
        { I8039_p4, I8039_p4, sys16_7751_sh_offset_a0_a3_w },
        { I8039_p5, I8039_p5, sys16_7751_sh_offset_a4_a7_w },
        { I8039_p6, I8039_p6, sys16_7751_sh_offset_a8_a11_w },
        { I8039_p7, I8039_p7, sys16_7751_sh_rom_select_w },
        { -1 }  /* end of table */
};

static struct DACinterface sys16_7751_dac_interface =
{
        1,
        { 100 }
};


// 7759


static struct MemoryReadAddress sound_readmem_7759[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xdfff, UPD7759_0_data_r },
	{ 0xe800, 0xe800, soundlatch_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

// some games (aurail, riotcity, eswat), seem to send different format data to the 7759
// this function changes that data to what the 7759 expects, but it sounds quite poor.
static WRITE_HANDLER( UPD7759_process_message_w )
{
	if((data & 0xc0) == 0x40) data=0xc0;
	else data&=0x3f;

	UPD7759_message_w(offset,data);
}

static struct IOWritePort sound_writeport_7759[] =
{
	{ 0x00, 0x00, YM2151_register_port_0_w },
	{ 0x01, 0x01, YM2151_data_port_0_w },
	{ 0x40, 0x40, UPD7759_process_message_w },
	{ 0x80, 0x80, UPD7759_0_start_w },
	{ -1 }
};

static struct UPD7759_interface upd7759_interface =
{
	1,			/* 1 chip */
	UPD7759_STANDARD_CLOCK,
	{ 60 }, 	/* volumes */
	{ REGION_CPU2 },			/* memory region 3 contains the sample data */
    UPD7759_SLAVE_MODE,
	{ sound_cause_nmi },
};

// SYS18 Sound

unsigned char *sys18_SoundMemBank;

static READ_HANDLER( system18_bank_r )
{
	return sys18_SoundMemBank[offset];
}

static struct MemoryReadAddress sound_readmem_18[] =
{
	{ 0x0000, 0x9fff, MRA_ROM },
	{ 0xa000, 0xbfff, system18_bank_r },
	/**** D/A register ****/
	{ 0xd000, 0xdfff, RF5C68_r },
	{ 0xe000, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem_18[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	/**** D/A register ****/
	{ 0xc000, 0xc008, RF5C68_reg_w },
	{ 0xd000, 0xdfff, RF5C68_w },
	{ 0xe000, 0xffff, MWA_RAM },	//??
	{ -1 }  /* end of table */
};

static struct RF5C68interface rf5c68_interface = {
  //3580000 * 2,
  3579545*2,
  100
};


static WRITE_HANDLER( sys18_soundbank_w )
{
// select access bank for a000~bfff
	unsigned char *RAM = memory_region(REGION_CPU2);
	int Bank=0;

	switch (data&0xc0)
	{
		case 0x00:
			Bank = data<<13;
			break;
		case 0x40:
			Bank = ((data&0x1f) + 128/8)<<13;
			break;
		case 0x80:
			Bank = ((data&0x1f) + (256+128)/8)<<13;
			break;
		case 0xc0:
			Bank = ((data&0x1f) + (512+128)/8)<<13;
			break;
	}
	sys18_SoundMemBank = &RAM[Bank+0x10000];
}

static struct IOReadPort sound_readport_18[] =
{
	{ 0x80, 0x80, YM2612_status_port_0_A_r },
//	{ 0x82, 0x82, YM2612_status_port_0_B_r },
//	{ 0x90, 0x90, YM2612_status_port_1_A_r },
//	{ 0x92, 0x92, YM2612_status_port_1_B_r },
	{ 0xc0, 0xc0, soundlatch_r },
	{ -1 }	/* end of table */
};


static struct IOWritePort sound_writeport_18[] =
{
	{ 0x80, 0x80, YM2612_control_port_0_A_w },
	{ 0x81, 0x81, YM2612_data_port_0_A_w },
	{ 0x82, 0x82, YM2612_control_port_0_B_w },
	{ 0x83, 0x83, YM2612_data_port_0_B_w },
	{ 0x90, 0x90, YM2612_control_port_1_A_w },
	{ 0x91, 0x91, YM2612_data_port_1_A_w },
	{ 0x92, 0x92, YM2612_control_port_1_B_w },
	{ 0x93, 0x93, YM2612_data_port_1_B_w },
	{ 0xa0, 0xa0, sys18_soundbank_w },
	{ -1 }
};

static struct YM2612interface ym3438_interface =
{
	2,	/* 2 chips */
	8000000,
	{ 40,40 },
	{ 0 },	{ 0 },	{ 0 },	{ 0 }
};


// Sega 3D Sound


static struct YM2203interface ym2203_interface =
{
	1,	/* 1 chips */
	4096000,	/* 3.58 MHZ ? */
	{ YM2203_VOL(50,50) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM2203interface ym2203_interface2 =
{
	3,	/* 1 chips */
	4096000,	/* 3.58 MHZ ? */
	{ YM2203_VOL(50,50),YM2203_VOL(50,50),YM2203_VOL(50,50) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct SEGAPCMinterface segapcm_interface_15k = {
	SEGAPCM_SAMPLE15K,
	BANK_256,
	REGION_SOUND1,		// memory region
	50
};

static struct SEGAPCMinterface segapcm_interface_15k_512 = {
	SEGAPCM_SAMPLE15K,
	BANK_512,
	REGION_SOUND1,		// memory region
	50
};

static struct SEGAPCMinterface segapcm_interface_32k = {
	SEGAPCM_SAMPLE32K,
	BANK_256,
	REGION_SOUND1,
	50
};


// Super hang-on, outrun

// hopefully this is endian safe!
static unsigned char *sound_shared_ram;
static READ_HANDLER( sound_shared_ram_r )
{
	return (sound_shared_ram[offset] << 8) + sound_shared_ram[offset+1];
}

static WRITE_HANDLER( sound_shared_ram_w )
{
	int val=(sound_shared_ram[offset] << 8) + sound_shared_ram[offset+1];
	val=(val & (data>>16)) | (data &0xffff);

	sound_shared_ram[offset] = val>>8;
	sound_shared_ram[offset+1] = val&0xff;
}


static READ_HANDLER( sound2_shared_ram_r ) { return sound_shared_ram[offset]; }
static WRITE_HANDLER( sound2_shared_ram_w ) { sound_shared_ram[offset] = data; }

static WRITE_HANDLER( sound_command_w ){
	//logerror("SOUND COMMAND %04x <- %02x\n", offset, data&0xff );
	soundlatch_w( 0,data&0xff );
	cpu_cause_interrupt( 1, 0 );
}

static WRITE_HANDLER( sound_command_nmi_w ){
	//logerror("SOUND COMMAND %04x <- %02x\n", offset, data&0xff );
	soundlatch_w( 0,data&0xff );
	cpu_set_nmi_line(1, PULSE_LINE);
}

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	4096000,	/* 3.58 MHZ ? */
	{ YM3012_VOL(40,MIXER_PAN_LEFT,40,MIXER_PAN_RIGHT) },
	{ 0 }
};




/***************************************************************************/

static struct GfxLayout charlayout1 =
{
	8,8,	/* 8*8 chars */
	8192,	/* 8192 chars */
	3,	/* 3 bits per pixel */
	{ 0x20000*8, 0x10000*8, 0 },
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout charlayout2 =
{
	8,8,	/* 8*8 chars */
	16384,	/* 16384 chars */
	3,	/* 3 bits per pixel */
	{ 0x40000*8, 0x20000*8, 0 },
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout charlayout4 =
{
	8,8,	/* 8*8 chars */
	32768,	/* 32768 chars */
	3,	/* 3 bits per pixel */
	{ 0x80000*8, 0x40000*8, 0 },
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout charlayout8 =
{
	8,8,	/* 8*8 chars */
	4096,	/* 4096 chars */
	3,	/* 3 bits per pixel */
	{ 0x10000*8, 0x08000*8, 0 },
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfx1[] =
{
	{ REGION_GFX1, 0x00000, &charlayout1,	0, 256 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfx2[] =
{
	{ REGION_GFX1, 0x00000, &charlayout2,	0, 256 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfx4[] =
{
	{ REGION_GFX1, 0x00000, &charlayout4,	0, 256 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfx8[] =
{
	{ REGION_GFX1, 0x00000, &charlayout8,	0, 256 },
	{ -1 } /* end of array */
};

/***************************************************************************/

static void set_refresh( int data ){
	sys16_refreshenable = data&0x20;
	sys16_clear_screen  = data&1;
}

static void set_refresh_18( int data ){
	sys16_refreshenable = data&0x2;
//	sys16_clear_screen  = data&4;
}

static void set_refresh_3d( int data ){
	sys16_refreshenable = data&0x10;
}


static void set_tile_bank( int data ){
	sys16_tile_bank1 = data&0xf;
	sys16_tile_bank0 = (data>>4)&0xf;
}

static void set_tile_bank18( int data ){
	sys16_tile_bank0 = data&0xf;
	sys16_tile_bank1 = (data>>4)&0xf;
}

static void set_fg_page( int data ){
	sys16_fg_page[0] = data>>12;
	sys16_fg_page[1] = (data>>8)&0xf;
	sys16_fg_page[2] = (data>>4)&0xf;
	sys16_fg_page[3] = data&0xf;
}

static void set_bg_page( int data ){
	sys16_bg_page[0] = data>>12;
	sys16_bg_page[1] = (data>>8)&0xf;
	sys16_bg_page[2] = (data>>4)&0xf;
	sys16_bg_page[3] = data&0xf;
}

static void set_fg_page1( int data ){
	sys16_fg_page[1] = data>>12;
	sys16_fg_page[0] = (data>>8)&0xf;
	sys16_fg_page[3] = (data>>4)&0xf;
	sys16_fg_page[2] = data&0xf;
}

static void set_bg_page1( int data ){
	sys16_bg_page[1] = data>>12;
	sys16_bg_page[0] = (data>>8)&0xf;
	sys16_bg_page[3] = (data>>4)&0xf;
	sys16_bg_page[2] = data&0xf;
}

static void set_fg2_page( int data ){
	sys16_fg2_page[0] = data>>12;
	sys16_fg2_page[1] = (data>>8)&0xf;
	sys16_fg2_page[2] = (data>>4)&0xf;
	sys16_fg2_page[3] = data&0xf;
}

static void set_bg2_page( int data ){
	sys16_bg2_page[0] = data>>12;
	sys16_bg2_page[1] = (data>>8)&0xf;
	sys16_bg2_page[2] = (data>>4)&0xf;
	sys16_bg2_page[3] = data&0xf;
}


/***************************************************************************/
/*	Important: you must leave extra space when listing sprite ROMs
	in a ROM module definition.  This routine unpacks each sprite nibble
	into a byte, doubling the memory consumption. */

static void sys16_sprite_decode( int num_banks, int bank_size ){
	unsigned char *base = memory_region(REGION_GFX2);
	unsigned char *temp = (unsigned char*)malloc( bank_size );
	int i;

	if( !temp ) return;

	for( i = num_banks; i >0; i-- ){
		unsigned char *finish	= base + 2*bank_size*i;
		unsigned char *dest = finish - 2*bank_size;

		unsigned char *p1 = temp;
		unsigned char *p2 = temp+bank_size/2;

		unsigned char data;

		memcpy (temp, base+bank_size*(i-1), bank_size);

/*
	note: both pen#0 and pen#15 are transparent.
	we replace references to pen#15 with pen#0, to simplify the sprite rendering
*/
		do {
			data = *p2++;
#ifdef SPRITE_SIDE_MARKERS
			if( (data&0x0f) == 0x0f )
			{
				if((data&0xf0) !=0xf0 && (data&0xf0) !=0)
					*dest++ = data >> 4;
				else
					*dest++ = 0xff;
				*dest++ = 0xff;
			}
			else if( (data&0xf0) == 0xf0 )
			{
				*dest++ = 0x00;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data &0xf;
			}
			else
			{
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}
#else
			{
				if( (data&0xf0) == 0xf0 ) data &= 0x0f;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}
#endif

			data = *p1++;
#ifdef SPRITE_SIDE_MARKERS
			if( (data&0x0f) == 0x0f )
			{
				if((data&0xf0) !=0xf0 && (data&0xf0) !=0)
					*dest++ = data >> 4;
				else
					*dest++ = 0xff;
				*dest++ = 0xff;
			}
			else if( (data&0xf0) == 0xf0 )
			{
				*dest++ = 0x00;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data &0xf;
			}
			else
			{
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}
#else
			{
				if( (data&0xf0) == 0xf0 ) data &= 0x0f;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}
#endif
		} while( dest<finish );
	}
	free( temp );
}

static void sys16_sprite_decode2( int num_banks, int bank_size, int side_markers ){
	unsigned char *base = memory_region(REGION_GFX2);
	unsigned char *temp = (unsigned char*)malloc( bank_size );
	int i;

	if( !temp ) return;

	for( i = num_banks; i >0; i-- ){
		unsigned char *finish	= base + 2*bank_size*i;
		unsigned char *dest = finish - 2*bank_size;

		unsigned char *p1 = temp;
		unsigned char *p2 = temp+bank_size/4;
		unsigned char *p3 = temp+bank_size/2;
		unsigned char *p4 = temp+bank_size/4*3;

		unsigned char data;

		memcpy (temp, base+bank_size*(i-1), bank_size);

/*
	note: both pen#0 and pen#15 are transparent.
	we replace references to pen#15 with pen#0, to simplify the sprite rendering
*/
		do {
			data = *p4++;
#ifdef SPRITE_SIDE_MARKERS
			if( side_markers )
			{
				if( (data&0x0f) == 0x0f )
				{
					if((data&0xf0) !=0xf0 && (data&0xf0) !=0)
						*dest++ = data >> 4;
					else
						*dest++ = 0xff;
					*dest++ = 0xff;
				}
				else if( (data&0xf0) == 0xf0 )
				{
					*dest++ = 0x00;
					if( (data&0x0f) == 0x0f ) data &= 0xf0;
					*dest++ = data &0xf;
				}
				else
				{
					*dest++ = data >> 4;
					*dest++ = data & 0xF;
				}
			}
			else
#endif
			{
				if( (data&0xf0) == 0xf0 ) data &= 0x0f;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}

			data = *p3++;
#ifdef SPRITE_SIDE_MARKERS
			if( side_markers )
			{
				if( (data&0x0f) == 0x0f )
				{
					if((data&0xf0) !=0xf0 && (data&0xf0) !=0)
						*dest++ = data >> 4;
					else
						*dest++ = 0xff;
					*dest++ = 0xff;
				}
				else if( (data&0xf0) == 0xf0 )
				{
					*dest++ = 0x00;
					if( (data&0x0f) == 0x0f ) data &= 0xf0;
					*dest++ = data &0xf;
				}
				else
				{
					*dest++ = data >> 4;
					*dest++ = data & 0xF;
				}
			}
			else
#endif
			{
				if( (data&0xf0) == 0xf0 ) data &= 0x0f;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}


			data = *p2++;
#ifdef SPRITE_SIDE_MARKERS
			if( side_markers )
			{
				if( (data&0x0f) == 0x0f )
				{
					if((data&0xf0) !=0xf0 && (data&0xf0) !=0)
						*dest++ = data >> 4;
					else
						*dest++ = 0xff;
					*dest++ = 0xff;
				}
				else if( (data&0xf0) == 0xf0 )
				{
					*dest++ = 0x00;
					if( (data&0x0f) == 0x0f ) data &= 0xf0;
					*dest++ = data &0xf;
				}
				else
				{
					*dest++ = data >> 4;
					*dest++ = data & 0xF;
				}
			}
			else
#endif
			{
				if( (data&0xf0) == 0xf0 ) data &= 0x0f;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}

			data = *p1++;
#ifdef SPRITE_SIDE_MARKERS
			if( side_markers )
			{
				if( (data&0x0f) == 0x0f )
				{
					if((data&0xf0) !=0xf0 && (data&0xf0) !=0)
						*dest++ = data >> 4;
					else
						*dest++ = 0xff;
					*dest++ = 0xff;
				}
				else if( (data&0xf0) == 0xf0 )
				{
					*dest++ = 0x00;
					if( (data&0x0f) == 0x0f ) data &= 0xf0;
					*dest++ = data &0xf;
				}
				else
				{
					*dest++ = data >> 4;
					*dest++ = data & 0xF;
				}
			}
			else
#endif
			{
				if( (data&0xf0) == 0xf0 ) data &= 0x0f;
				if( (data&0x0f) == 0x0f ) data &= 0xf0;
				*dest++ = data >> 4;
				*dest++ = data & 0xF;
			}

		} while( dest<finish );
	}
	free( temp );
}

int gr_bitmap_width;

static void generate_gr_screen(int w,int bitmap_width,int skip,int start_color,int end_color,int source_size)
{
	UINT8 *buf;
	UINT8 *gr = memory_region(REGION_GFX3);
	UINT8 *grr = NULL;
    int i,j,k;
    int center_offset=0;


	buf=(UINT8*)malloc(source_size);
	if(buf==NULL) return;

	gr_bitmap_width = bitmap_width;

	memcpy(buf,gr,source_size);
	memset(gr,0,256*bitmap_width);

	if (w!=gr_bitmap_width)
	{
		if (skip>0) // needs mirrored RHS
			grr=gr;
		else
		{
			center_offset=(gr_bitmap_width-w);
			gr+=center_offset/2;
		}
	}

    // build gr_bitmap
	for (i=0; i<256; i++)
	{
		UINT8 last_bit;
		UINT8 color_data[4];

		color_data[0]=start_color; color_data[1]=start_color+1;
		color_data[2]=start_color+2; color_data[3]=start_color+3;
		last_bit=((buf[0]&0x80)==0)|(((buf[0x4000]&0x80)==0)<<1);
		for (j=0; j<w/8; j++)
		{
			for (k=0; k<8; k++)
			{
				UINT8 bit=((buf[0]&0x80)==0)|(((buf[0x4000]&0x80)==0)<<1);
				if (bit!=last_bit && bit==0 && i>1)
				{ // color flipped to 0,advance color[0]
					if (color_data[0]+end_color <= end_color)
					{
						color_data[0]+=end_color;
					}
					else
					{
						color_data[0]-=end_color;
					}
				}
				*gr = color_data[bit];
				last_bit=bit;
				buf[0] <<= 1; buf[0x4000] <<= 1; gr++;
			}
			buf++;
		}

		if (grr!=NULL)
		{ // need mirrored RHS
			UINT8 *_gr=gr-1;
			_gr -= skip;
			for (j=0; j<w-skip; j++)
			{
				*gr++ = *_gr--;
			}
			for (j=0; j<skip; j++) *gr++ = 0;
		}
		else if (center_offset!=0)
		{
			gr+=center_offset;
		}
	}

	i=1;
	while ( (1<<i) < gr_bitmap_width ) i++;
	gr_bitmap_width=i; // power of 2

}


/***************************************************************************/

#define io_player1_r input_port_0_r
#define io_player2_r input_port_1_r
#define io_player3_r input_port_5_r
#define io_player4_r input_port_6_r
#define io_service_r input_port_2_r

#define io_dip1_r input_port_3_r
#define io_dip2_r input_port_4_r
#define io_dip3_r input_port_5_r

/***************************************************************************/


static void patch_codeX( int offset, int data, int cpu ){
	int aligned_offset = offset&0xfffffe;
	unsigned char *RAM = memory_region(REGION_CPU1+cpu);
	int old_word = READ_WORD( &RAM[aligned_offset] );

	if( offset&1 )
		data = (old_word&0xff00)|data;
	else
		data = (old_word&0x00ff)|(data<<8);

	WRITE_WORD (&RAM[aligned_offset], data);
}

static void patch_code( int offset, int data ) {patch_codeX(offset,data,0);}
//static void patch_code2( int offset, int data ) {patch_codeX(offset,data,2);}

static void patch_z80code( int offset, int data ){
	unsigned char *RAM = memory_region(REGION_CPU2);
	RAM[offset] = data;
}

/***************************************************************************/

#define SYS16_JOY1 PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

#define SYS16_JOY2 PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

#define SYS16_JOY3 PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

#define SYS16_JOY1_SWAPPEDBUTTONS PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

#define SYS16_JOY2_SWAPPEDBUTTONS PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

#define SYS16_SERVICE PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define SYS16_COINAGE PORT_START \
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 5/3 6/4") \
	PORT_DIPSETTING(    0x04, "2 Coins/1 Credit 4/3") \
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x01, "1 Coin/1 Credit 2/3") \
	PORT_DIPSETTING(    0x02, "1 Coin/1 Credit 4/5") \
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 5/6") \
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x00, "Free Play (if Coin B too) or 1/1") \
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 5/3 6/4") \
	PORT_DIPSETTING(    0x40, "2 Coins/1 Credit 4/3") \
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x10, "1 Coin/1 Credit 2/3") \
	PORT_DIPSETTING(    0x20, "1 Coin/1 Credit 4/5") \
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 5/6") \
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x00, "Free Play (if Coin A too) or 1/1")



/***************************************************************************/
// sys16A
ROM_START( alexkidd )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr10429.42", 0x000000, 0x10000, 0xbdf49eca )
	ROM_LOAD_ODD ( "epr10427.26", 0x000000, 0x10000, 0xf6e3dd29 )
	ROM_LOAD_EVEN( "epr10430.43", 0x020000, 0x10000, 0x89e3439f )
	ROM_LOAD_ODD ( "epr10428.25", 0x020000, 0x10000, 0xdbed3210 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10431.95", 0x00000, 0x08000, 0xa7962c39 )
	ROM_LOAD( "10432.94", 0x08000, 0x08000, 0xdb8cd24e )
	ROM_LOAD( "10433.93", 0x10000, 0x08000, 0xe163c8c2 )

	ROM_REGION( 0x050000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10437.10", 0x000000, 0x008000, 0x522f7618 )
	ROM_LOAD( "10441.11", 0x008000, 0x008000, 0x74e3a35c )
	ROM_LOAD( "10438.17", 0x010000, 0x008000, 0x738a6362 )
	ROM_LOAD( "10442.18", 0x018000, 0x008000, 0x86cb9c14 )
	ROM_LOAD( "10439.23", 0x020000, 0x008000, 0xb391aca7 )
	ROM_LOAD( "10443.24", 0x028000, 0x008000, 0x95d32635 )
	ROM_LOAD( "10440.29", 0x030000, 0x008000, 0x23939508 )
	ROM_LOAD( "10444.30", 0x038000, 0x008000, 0x82115823 )
//	ROM_LOAD( "10437.10", 0x040000, 0x008000, 0x522f7618 ) twice?
//	ROM_LOAD( "10441.11", 0x048000, 0x008000, 0x74e3a35c ) twice?

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10434.12", 0x0000, 0x8000, 0x77141cce )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* 7751 sound data (not used yet) */
	ROM_LOAD( "10435.1", 0x0000, 0x8000, 0xad89f6e3 )
	ROM_LOAD( "10436.2", 0x8000, 0x8000, 0x96c76613 )
ROM_END

ROM_START( alexkida )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "10447.43", 0x000000, 0x10000, 0x29e87f71 )
	ROM_LOAD_ODD ( "10445.26", 0x000000, 0x10000, 0x25ce5b6f )
	ROM_LOAD_EVEN( "10448.42", 0x020000, 0x10000, 0x05baedb5 )
	ROM_LOAD_ODD ( "10446.25", 0x020000, 0x10000, 0xcd61d23c )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10431.95", 0x00000, 0x08000, 0xa7962c39 )
	ROM_LOAD( "10432.94", 0x08000, 0x08000, 0xdb8cd24e )
	ROM_LOAD( "10433.93", 0x10000, 0x08000, 0xe163c8c2 )

	ROM_REGION( 0x050000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10437.10", 0x000000, 0x008000, 0x522f7618 )
	ROM_LOAD( "10441.11", 0x008000, 0x008000, 0x74e3a35c )
	ROM_LOAD( "10438.17", 0x010000, 0x008000, 0x738a6362 )
	ROM_LOAD( "10442.18", 0x018000, 0x008000, 0x86cb9c14 )
	ROM_LOAD( "10439.23", 0x020000, 0x008000, 0xb391aca7 )
	ROM_LOAD( "10443.24", 0x028000, 0x008000, 0x95d32635 )
	ROM_LOAD( "10440.29", 0x030000, 0x008000, 0x23939508 )
	ROM_LOAD( "10444.30", 0x038000, 0x008000, 0x82115823 )
//	ROM_LOAD( "10437.10", 0x040000, 0x008000, 0x522f7618 ) twice?
//	ROM_LOAD( "10441.11", 0x048000, 0x008000, 0x74e3a35c ) twice?

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10434.12", 0x0000, 0x8000, 0x77141cce )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "10435.1", 0x0000, 0x8000, 0xad89f6e3 )
	ROM_LOAD( "10436.2", 0x8000, 0x8000, 0x96c76613 )
ROM_END

/***************************************************************************/

static READ_HANDLER( alexkidd_skip_r )
{
	if (cpu_get_pc()==0x242c) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x3108]);
}

static struct MemoryReadAddress alexkidd_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc40002, 0xc40005, MRA_NOP },		//??
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc60000, 0xc60001, MRA_NOP },
	{ 0xfff108, 0xfff109, alexkidd_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress alexkidd_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40002, 0xc40005, MWA_NOP },		//??
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void alexkidd_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	set_fg_page1( READ_WORD( &sys16_textram[0x0e9e] ) );
	set_bg_page1( READ_WORD( &sys16_textram[0x0e9c] ) );
}

static void alexkidd_init_machine( void ){
	static int bank[16] = { 00,01,02,03,00,01,02,03,00,01,02,03,00,01,02,03};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 2;
	sys16_sprxoffset = -0xbc;
	sys16_fgxoffset = sys16_bgxoffset = 7;
	sys16_bg_priority_mode=1;

	sys16_update_proc = alexkidd_update_proc;
}

static void init_alexkidd( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 5,0x010000 );
}
/***************************************************************************/

INPUT_PORTS_START( alexkidd )
	SYS16_JOY1_SWAPPEDBUTTONS
	SYS16_JOY2_SWAPPEDBUTTONS
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "Continues" )
	PORT_DIPSETTING(    0x01, "Only before level 5" )
	PORT_DIPSETTING(    0x00, "Unlimited" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "240", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "10000" )
	PORT_DIPSETTING(    0x30, "20000" )
	PORT_DIPSETTING(    0x10, "40000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0xc0, "Time Adjust" )
	PORT_DIPSETTING(    0x80, "70" )
	PORT_DIPSETTING(    0xc0, "60" )
	PORT_DIPSETTING(    0x40, "50" )
	PORT_DIPSETTING(    0x00, "40" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7751( machine_driver_alexkidd, \
	alexkidd_readmem,alexkidd_writemem,alexkidd_init_machine,gfx8 )

/***************************************************************************/
// sys16B
ROM_START( aliensyn )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "11083.a4", 0x00000, 0x8000, 0xcb2ad9b3 )
	ROM_LOAD_ODD ( "11080.a1", 0x00000, 0x8000, 0xfe7378d9 )
	ROM_LOAD_EVEN( "11084.a5", 0x10000, 0x8000, 0x2e1ec7b1 )
	ROM_LOAD_ODD ( "11081.a2", 0x10000, 0x8000, 0x1308ee63 )
	ROM_LOAD_EVEN( "11085.a6", 0x20000, 0x8000, 0xcff78f39 )
	ROM_LOAD_ODD ( "11082.a3", 0x20000, 0x8000, 0x9cdc2a14 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10702.b9",  0x00000, 0x10000, 0x393bc813 )
	ROM_LOAD( "10703.b10", 0x10000, 0x10000, 0x6b6dd9f5 )
	ROM_LOAD( "10704.b11", 0x20000, 0x10000, 0x911e7ebc )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10709.b1", 0x00000, 0x10000, 0xaddf0a90 )
	ROM_LOAD( "10713.b5", 0x10000, 0x10000, 0xececde3a )
	ROM_LOAD( "10710.b2", 0x20000, 0x10000, 0x992369eb )
	ROM_LOAD( "10714.b6", 0x30000, 0x10000, 0x91bf42fb )
	ROM_LOAD( "10711.b3", 0x40000, 0x10000, 0x29166ef6 )
	ROM_LOAD( "10715.b7", 0x50000, 0x10000, 0xa7c57384 )
	ROM_LOAD( "10712.b4", 0x60000, 0x10000, 0x876ad019 )
	ROM_LOAD( "10716.b8", 0x70000, 0x10000, 0x40ba1d48 )

	ROM_REGION( 0x28000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10723.a7", 0x0000, 0x8000, 0x99953526 )
	ROM_LOAD( "10724.a8", 0x10000, 0x8000, 0xf971a817 )
	ROM_LOAD( "10725.a9", 0x18000, 0x8000, 0x6a50e08f )
	ROM_LOAD( "10726.a10",0x20000, 0x8000, 0xd50b7736 )
ROM_END

// sys16A - use a different sound chip?
ROM_START( aliensya )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code. I guessing the order a bit here */
	ROM_LOAD_EVEN( "10808", 0x00000, 0x8000, 0xe669929f )
	ROM_LOAD_ODD ( "10806", 0x00000, 0x8000, 0x9f7f8fdd )
	ROM_LOAD_EVEN( "10809", 0x10000, 0x8000, 0x9a424919 )
	ROM_LOAD_ODD ( "10807", 0x10000, 0x8000, 0x3d2c3530 )
	ROM_LOAD_EVEN( "10701", 0x20000, 0x8000, 0x92171751 )
	ROM_LOAD_ODD ( "10698", 0x20000, 0x8000, 0xc1e4fdc0 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10739", 0x00000, 0x10000, 0xa29ec207 )
	ROM_LOAD( "10740", 0x10000, 0x10000, 0x47f93015 )
	ROM_LOAD( "10741", 0x20000, 0x10000, 0x4970739c )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10709.b1", 0x00000, 0x10000, 0xaddf0a90 )
	ROM_LOAD( "10713.b5", 0x10000, 0x10000, 0xececde3a )
	ROM_LOAD( "10710.b2", 0x20000, 0x10000, 0x992369eb )
	ROM_LOAD( "10714.b6", 0x30000, 0x10000, 0x91bf42fb )
	ROM_LOAD( "10711.b3", 0x40000, 0x10000, 0x29166ef6 )
	ROM_LOAD( "10715.b7", 0x50000, 0x10000, 0xa7c57384 )
	ROM_LOAD( "10712.b4", 0x60000, 0x10000, 0x876ad019 )
	ROM_LOAD( "10716.b8", 0x70000, 0x10000, 0x40ba1d48 )

	ROM_REGION( 0x28000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10705", 0x00000, 0x8000, 0x777b749e )
	ROM_LOAD( "10706", 0x10000, 0x8000, 0xaa114acc )
	ROM_LOAD( "10707", 0x18000, 0x8000, 0x800c1d82 )
	ROM_LOAD( "10708", 0x20000, 0x8000, 0x5921ef52 )
ROM_END

ROM_START( aliensyj )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* Custom 68000 code . I guessing the order a bit here */
// custom cpu 317-0033
	ROM_LOAD_EVEN( "epr10699.43", 0x00000, 0x8000, 0x3fd38d17 )
	ROM_LOAD_ODD ( "epr10696.26", 0x00000, 0x8000, 0xd734f19f )
	ROM_LOAD_EVEN( "epr10700.42", 0x10000, 0x8000, 0x3b04b252 )
	ROM_LOAD_ODD ( "epr10697.25", 0x10000, 0x8000, 0xf2bc123d )
	ROM_LOAD_EVEN( "10701", 0x20000, 0x8000, 0x92171751 )
	ROM_LOAD_ODD ( "10698", 0x20000, 0x8000, 0xc1e4fdc0 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10739", 0x00000, 0x10000, 0xa29ec207 )
	ROM_LOAD( "10740", 0x10000, 0x10000, 0x47f93015 )
	ROM_LOAD( "10741", 0x20000, 0x10000, 0x4970739c )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10709.b1", 0x00000, 0x10000, 0xaddf0a90 )
	ROM_LOAD( "10713.b5", 0x10000, 0x10000, 0xececde3a )
	ROM_LOAD( "10710.b2", 0x20000, 0x10000, 0x992369eb )
	ROM_LOAD( "10714.b6", 0x30000, 0x10000, 0x91bf42fb )
	ROM_LOAD( "10711.b3", 0x40000, 0x10000, 0x29166ef6 )
	ROM_LOAD( "10715.b7", 0x50000, 0x10000, 0xa7c57384 )
	ROM_LOAD( "10712.b4", 0x60000, 0x10000, 0x876ad019 )
	ROM_LOAD( "10716.b8", 0x70000, 0x10000, 0x40ba1d48 )

	ROM_REGION( 0x28000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10705", 0x00000, 0x8000, 0x777b749e )
	ROM_LOAD( "10706", 0x10000, 0x8000, 0xaa114acc )
	ROM_LOAD( "10707", 0x18000, 0x8000, 0x800c1d82 )
	ROM_LOAD( "10708", 0x20000, 0x8000, 0x5921ef52 )
ROM_END


ROM_START( aliensyb )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "as_typeb.a4", 0x00000, 0x8000, 0x17bf5304 )
	ROM_LOAD_ODD ( "as_typeb.a1", 0x00000, 0x8000, 0x4cd134df )
	ROM_LOAD_EVEN( "as_typeb.a5", 0x10000, 0x8000, 0xc8b791b0 )
	ROM_LOAD_ODD ( "as_typeb.a2", 0x10000, 0x8000, 0xbdcf4a30 )
	ROM_LOAD_EVEN( "as_typeb.a6", 0x20000, 0x8000, 0x1d0790aa )
	ROM_LOAD_ODD ( "as_typeb.a3", 0x20000, 0x8000, 0x1e7586b7 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10702.b9",  0x00000, 0x10000, 0x393bc813 )
	ROM_LOAD( "10703.b10", 0x10000, 0x10000, 0x6b6dd9f5 )
	ROM_LOAD( "10704.b11", 0x20000, 0x10000, 0x911e7ebc )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10709.b1", 0x00000, 0x10000, 0xaddf0a90 )
	ROM_LOAD( "10713.b5", 0x10000, 0x10000, 0xececde3a )
	ROM_LOAD( "10710.b2", 0x20000, 0x10000, 0x992369eb )
	ROM_LOAD( "10714.b6", 0x30000, 0x10000, 0x91bf42fb )
	ROM_LOAD( "10711.b3", 0x40000, 0x10000, 0x29166ef6 )
	ROM_LOAD( "10715.b7", 0x50000, 0x10000, 0xa7c57384 )
	ROM_LOAD( "10712.b4", 0x60000, 0x10000, 0x876ad019 )
	ROM_LOAD( "10716.b8", 0x70000, 0x10000, 0x40ba1d48 )

	ROM_REGION( 0x28000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10723.a7", 0x0000, 0x8000, 0x99953526 )
	ROM_LOAD( "10724.a8", 0x10000, 0x8000, 0xf971a817 )
	ROM_LOAD( "10725.a9", 0x18000, 0x8000, 0x6a50e08f )
	ROM_LOAD( "10726.a10",0x20000, 0x8000, 0xd50b7736 )
ROM_END




/***************************************************************************/

static struct MemoryReadAddress aliensyn_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc40fff, MRA_EXTRAM },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress aliensyn_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc00006, 0xc00007, sound_command_w },
	{ 0xc40000, 0xc40fff, MWA_EXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void aliensyn_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram[0] ) ); // 0xc40001
}

static void aliensyn_init_machine( void ){
	static int bank[16] = { 0,0,0,0,0,0,0,6,0,0,0,4,0,2,0,0 };
	sys16_obj_bank = bank;
	sys16_bg_priority_mode=1;
	sys16_fg_priority_mode=1;

	sys16_update_proc = aliensyn_update_proc;
}

static void aliensyn_sprite_decode( void ){
	sys16_sprite_decode( 4,0x20000 );
}

static void init_aliensyn( void )
{
	sys16_onetime_init_machine();
	sys16_bg1_trans=1;
	aliensyn_sprite_decode();
}

/***************************************************************************/

INPUT_PORTS_START( aliensyn )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "127", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, "Timer" )
	PORT_DIPSETTING(    0x00, "120" )
	PORT_DIPSETTING(    0x10, "130" )
	PORT_DIPSETTING(    0x20, "140" )
	PORT_DIPSETTING(    0x30, "150" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END

/***************************************************************************/

static struct UPD7759_interface aliensyn_upd7759_interface =
{
	1,			/* 1 chip */
	480000,
	{ 60 }, 	/* volumes */
	{ REGION_CPU2 },			/* memory region 3 contains the sample data */
    UPD7759_SLAVE_MODE,
	{ sound_cause_nmi },
};

/****************************************************************************/

MACHINE_DRIVER_7759( machine_driver_aliensyn, \
	aliensyn_readmem,aliensyn_writemem,aliensyn_init_machine, gfx1, aliensyn_upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( altbeast )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "11705", 0x000000, 0x20000, 0x57dc5c7a )
	ROM_LOAD_ODD ( "11704", 0x000000, 0x20000, 0x33bbcf07 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11674", 0x00000, 0x20000, 0xa57a66d5 )
	ROM_LOAD( "11675", 0x20000, 0x20000, 0x2ef2f144 )
	ROM_LOAD( "11676", 0x40000, 0x20000, 0x0c04acac )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11677.b1", 0x00000, 0x10000, 0xa01425cd )
	ROM_CONTINUE(            0x20000, 0x10000 )
	ROM_LOAD( "epr11681.b5", 0x10000, 0x10000, 0xd9e03363 )
	ROM_CONTINUE(            0x30000, 0x10000 )
	ROM_LOAD( "epr11678.b2", 0x40000, 0x10000, 0x17a9fc53 )
	ROM_CONTINUE(            0x60000, 0x10000 )
	ROM_LOAD( "epr11682.b6", 0x50000, 0x10000, 0xe3f77c5e )
	ROM_CONTINUE(            0x70000, 0x10000 )
	ROM_LOAD( "epr11679.b3", 0x80000, 0x10000, 0x14dcc245 )
	ROM_CONTINUE(            0xa0000, 0x10000 )
	ROM_LOAD( "epr11683.b7", 0x90000, 0x10000, 0xf9a60f06 )
	ROM_CONTINUE(            0xb0000, 0x10000 )
	ROM_LOAD( "epr11680.b4", 0xc0000, 0x10000, 0xf43dcdec )
	ROM_CONTINUE(            0xe0000, 0x10000 )
	ROM_LOAD( "epr11684.b8", 0xd0000, 0x10000, 0xb20c0edb )
	ROM_CONTINUE(            0xf0000, 0x10000 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "11671",		 0x00000, 0x08000, 0x2b71343b )
	ROM_LOAD( "opr11672",    0x10000, 0x20000, 0xbbd7f460 )
	ROM_LOAD( "opr11673",    0x30000, 0x20000, 0x400c4a36 )
ROM_END

ROM_START( jyuohki )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* Custom 68000 code. */
// custom cpu 317-0065
	ROM_LOAD_EVEN( "epr11670.a7", 0x000000, 0x20000, 0xb748eb07 )
	ROM_LOAD_ODD ( "epr11669.a5", 0x000000, 0x20000, 0x005ecd11 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11674", 0x00000, 0x20000, 0xa57a66d5 )
	ROM_LOAD( "11675", 0x20000, 0x20000, 0x2ef2f144 )
	ROM_LOAD( "11676", 0x40000, 0x20000, 0x0c04acac )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11677.b1", 0x00000, 0x10000, 0xa01425cd )
	ROM_CONTINUE(            0x20000, 0x10000 )
	ROM_LOAD( "epr11681.b5", 0x10000, 0x10000, 0xd9e03363 )
	ROM_CONTINUE(            0x30000, 0x10000 )
	ROM_LOAD( "epr11678.b2", 0x40000, 0x10000, 0x17a9fc53 )
	ROM_CONTINUE(            0x60000, 0x10000 )
	ROM_LOAD( "epr11682.b6", 0x50000, 0x10000, 0xe3f77c5e )
	ROM_CONTINUE(            0x70000, 0x10000 )
	ROM_LOAD( "epr11679.b3", 0x80000, 0x10000, 0x14dcc245 )
	ROM_CONTINUE(            0xa0000, 0x10000 )
	ROM_LOAD( "epr11683.b7", 0x90000, 0x10000, 0xf9a60f06 )
	ROM_CONTINUE(            0xb0000, 0x10000 )
	ROM_LOAD( "epr11680.b4", 0xc0000, 0x10000, 0xf43dcdec )
	ROM_CONTINUE(            0xe0000, 0x10000 )
	ROM_LOAD( "epr11684.b8", 0xd0000, 0x10000, 0xb20c0edb )
	ROM_CONTINUE(            0xf0000, 0x10000 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "11671",		 0x00000, 0x08000, 0x2b71343b )
	ROM_LOAD( "opr11672",    0x10000, 0x20000, 0xbbd7f460 )
	ROM_LOAD( "opr11673",    0x30000, 0x20000, 0x400c4a36 )
ROM_END

// sys16B
ROM_START( altbeas2 )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr11740", 0x000000, 0x20000, 0xce227542 )
	ROM_LOAD_ODD ( "epr11739", 0x000000, 0x20000, 0xe466eb65 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11674", 0x00000, 0x20000, 0xa57a66d5 )
	ROM_LOAD( "11675", 0x20000, 0x20000, 0x2ef2f144 )
	ROM_LOAD( "11676", 0x40000, 0x20000, 0x0c04acac )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11677.b1", 0x00000, 0x10000, 0xa01425cd )
	ROM_CONTINUE(            0x20000, 0x10000 )
	ROM_LOAD( "epr11681.b5", 0x10000, 0x10000, 0xd9e03363 )
	ROM_CONTINUE(            0x30000, 0x10000 )
	ROM_LOAD( "epr11678.b2", 0x40000, 0x10000, 0x17a9fc53 )
	ROM_CONTINUE(            0x60000, 0x10000 )
	ROM_LOAD( "epr11682.b6", 0x50000, 0x10000, 0xe3f77c5e )
	ROM_CONTINUE(            0x70000, 0x10000 )
	ROM_LOAD( "epr11679.b3", 0x80000, 0x10000, 0x14dcc245 )
	ROM_CONTINUE(            0xa0000, 0x10000 )
	ROM_LOAD( "epr11683.b7", 0x90000, 0x10000, 0xf9a60f06 )
	ROM_CONTINUE(            0xb0000, 0x10000 )
	ROM_LOAD( "epr11680.b4", 0xc0000, 0x10000, 0xf43dcdec )
	ROM_CONTINUE(            0xe0000, 0x10000 )
	ROM_LOAD( "epr11684.b8", 0xd0000, 0x10000, 0xb20c0edb )
	ROM_CONTINUE(            0xf0000, 0x10000 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "opr11686",	 0x00000, 0x08000, 0x828a45b3 )	// ???
	ROM_LOAD( "opr11672",    0x10000, 0x20000, 0xbbd7f460 )
	ROM_LOAD( "opr11673",    0x30000, 0x20000, 0x400c4a36 )
ROM_END



/***************************************************************************/

static READ_HANDLER( altbeast_skip_r )
{
	if (cpu_get_pc()==0x3994) {cpu_spinuntil_int(); return 1<<8;}

	return READ_WORD(&sys16_workingram[0x301c]);
}

// ??? What is this, input test shows 4 bits to each player, but what does it do?
static READ_HANDLER( altbeast_io_r )
{
	return 0xff;
}

static struct MemoryReadAddress altbeast_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41004, 0xc41005, altbeast_io_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc40fff, MRA_EXTRAM },
	{ 0xfff01c, 0xfff01d, altbeast_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress altbeast_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40fff, MWA_EXTRAM },
	{ 0xfe0006, 0xfe0007, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void altbeast_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_tile_bank( READ_WORD( &sys16_workingram[0x3094] ) );
	set_refresh( READ_WORD( &sys16_extraram[0] ) );
}

static void altbeast_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	sys16_obj_bank = bank;
	sys16_update_proc = altbeast_update_proc;
}

static void altbeas2_init_machine( void ){
	static int bank[16] = {0x00,0x00,0x02,0x00,0x04,0x00,0x06,0x00,0x08,0x00,0x0A,0x00,0x0C,0x00,0x00,0x00};
	sys16_obj_bank = bank;
	sys16_update_proc = altbeast_update_proc;
}

static void init_altbeast( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 7,0x20000 );
}

/***************************************************************************/

INPUT_PORTS_START( altbeast )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Credits needed" )
	PORT_DIPSETTING(    0x01, "1 to start, 1 to continue")
	PORT_DIPSETTING(    0x00, "2 to start, 1 to continue")
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "240", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, "Energy Meter" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_altbeast, \
	altbeast_readmem,altbeast_writemem,altbeast_init_machine, gfx2,upd7759_interface )

MACHINE_DRIVER_7759( machine_driver_altbeas2, \
	altbeast_readmem,altbeast_writemem,altbeas2_init_machine, gfx2,upd7759_interface )

/***************************************************************************/
// sys18
ROM_START( astorm )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr13085.bin", 0x000000, 0x40000, 0x15f74e2d )
	ROM_LOAD_ODD ( "epr13084.bin", 0x000000, 0x40000, 0x9687b38f )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, 0xdf5d0a61 )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, 0x787afab8 )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, 0x4e01b477 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13082.bin", 0x000000, 0x40000, 0xa782b704 )
	ROM_LOAD( "mpr13089.bin", 0x040000, 0x40000, 0x2a4227f0 )
	ROM_LOAD( "mpr13081.bin", 0x080000, 0x40000, 0xeb510228 )
	ROM_LOAD( "mpr13088.bin", 0x0c0000, 0x40000, 0x3b6b4c55 )
	ROM_LOAD( "mpr13080.bin", 0x100000, 0x40000, 0xe668eefb )
	ROM_LOAD( "mpr13087.bin", 0x140000, 0x40000, 0x2293427d )
	ROM_LOAD( "epr13079.bin", 0x180000, 0x40000, 0xde9221ed )
	ROM_LOAD( "epr13086.bin", 0x1c0000, 0x40000, 0x8c9a71c4 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr13083.bin", 0x10000, 0x20000, 0x5df3af20 )
	ROM_LOAD( "epr13076.bin", 0x30000, 0x40000, 0x94e6c76e )
	ROM_LOAD( "epr13077.bin", 0x70000, 0x40000, 0xe2ec0d8d )
	ROM_LOAD( "epr13078.bin", 0xb0000, 0x40000, 0x15684dc5 )
ROM_END

ROM_START( astorm2p )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr13182.bin", 0x000000, 0x40000, 0xe31f2a1c )
	ROM_LOAD_ODD ( "epr13181.bin", 0x000000, 0x40000, 0x78cd3b26 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, 0xdf5d0a61 )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, 0x787afab8 )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, 0x4e01b477 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13082.bin", 0x000000, 0x40000, 0xa782b704 )
	ROM_LOAD( "mpr13089.bin", 0x040000, 0x40000, 0x2a4227f0 )
	ROM_LOAD( "mpr13081.bin", 0x080000, 0x40000, 0xeb510228 )
	ROM_LOAD( "mpr13088.bin", 0x0c0000, 0x40000, 0x3b6b4c55 )
	ROM_LOAD( "mpr13080.bin", 0x100000, 0x40000, 0xe668eefb )
	ROM_LOAD( "mpr13087.bin", 0x140000, 0x40000, 0x2293427d )
	ROM_LOAD( "epr13079.bin", 0x180000, 0x40000, 0xde9221ed )
	ROM_LOAD( "epr13086.bin", 0x1c0000, 0x40000, 0x8c9a71c4 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ep13083a.bin", 0x10000, 0x20000, 0xe7528e06 )
	ROM_LOAD( "epr13076.bin", 0x30000, 0x40000, 0x94e6c76e )
	ROM_LOAD( "epr13077.bin", 0x70000, 0x40000, 0xe2ec0d8d )
	ROM_LOAD( "epr13078.bin", 0xb0000, 0x40000, 0x15684dc5 )
ROM_END

ROM_START( astormbl )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "astorm.a6", 0x000000, 0x40000, 0x7682ed3e )
	ROM_LOAD_ODD ( "astorm.a5", 0x000000, 0x40000, 0xefe9711e )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, 0xdf5d0a61 )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, 0x787afab8 )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, 0x4e01b477 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13082.bin", 0x000000, 0x40000, 0xa782b704 )
	ROM_LOAD( "astorm.a11",   0x040000, 0x40000, 0x7829c4f3 )
	ROM_LOAD( "mpr13081.bin", 0x080000, 0x40000, 0xeb510228 )
	ROM_LOAD( "mpr13088.bin", 0x0c0000, 0x40000, 0x3b6b4c55 )
	ROM_LOAD( "mpr13080.bin", 0x100000, 0x40000, 0xe668eefb )
	ROM_LOAD( "mpr13087.bin", 0x140000, 0x40000, 0x2293427d )
	ROM_LOAD( "epr13079.bin", 0x180000, 0x40000, 0xde9221ed )
	ROM_LOAD( "epr13086.bin", 0x1c0000, 0x40000, 0x8c9a71c4 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr13083.bin", 0x10000, 0x20000, 0x5df3af20 )
	ROM_LOAD( "epr13076.bin", 0x30000, 0x40000, 0x94e6c76e )
	ROM_LOAD( "epr13077.bin", 0x70000, 0x40000, 0xe2ec0d8d )
	ROM_LOAD( "epr13078.bin", 0xb0000, 0x40000, 0x15684dc5 )
ROM_END

/***************************************************************************/

static READ_HANDLER( astorm_skip_r )
{
	if (cpu_get_pc()==0x3d4c) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x2c2c]);
}

static struct MemoryReadAddress astorm_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x10ffff, MRA_TILERAM },
	{ 0x110000, 0x110fff, MRA_TEXTRAM },
	{ 0x140000, 0x140fff, MRA_PALETTERAM },
	{ 0x200000, 0x200fff, MRA_SPRITERAM },
	{ 0xa00000, 0xa00001, io_dip1_r },
	{ 0xa00002, 0xa00003, io_dip2_r },
	{ 0xa01002, 0xa01003, io_player1_r },
	{ 0xa01004, 0xa01005, io_player2_r },
	{ 0xa01006, 0xa01007, io_player3_r },
	{ 0xa01000, 0xa01001, io_service_r },
	{ 0xa00000, 0xa0ffff, MRA_EXTRAM2 },
	{ 0xc00000, 0xc0ffff, MRA_EXTRAM },
	{ 0xc40000, 0xc4ffff, MRA_EXTRAM3 },
	{ 0xffec2c, 0xffec2d, astorm_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress astorm_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x10ffff, MWA_TILERAM },
	{ 0x110000, 0x110fff, MWA_TEXTRAM },
	{ 0x140000, 0x140fff, MWA_PALETTERAM },
	{ 0x200000, 0x200fff, MWA_SPRITERAM },
	{ 0xa00006, 0xa00007, sound_command_nmi_w },
	{ 0xa00000, 0xa0ffff, MWA_EXTRAM2 },
	{ 0xc00000, 0xc0ffff, MWA_EXTRAM },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM3 },
	{ 0xfe0020, 0xfe003f, MWA_NOP },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void astorm_update_proc( void ){
	int data;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	data = READ_WORD( &sys16_textram[0x0e80] );
	sys16_fg_page[1] = data>>12;
	sys16_fg_page[3] = (data>>8)&0xf;
	sys16_fg_page[0] = (data>>4)&0xf;
	sys16_fg_page[2] = data&0xf;

	data = READ_WORD( &sys16_textram[0x0e82] );
	sys16_bg_page[1] = data>>12;
	sys16_bg_page[3] = (data>>8)&0xf;
	sys16_bg_page[0] = (data>>4)&0xf;
	sys16_bg_page[2] = data&0xf;


	sys16_fg2_scrollx = READ_WORD( &sys16_textram[0x0e9c] );
	sys16_bg2_scrollx = READ_WORD( &sys16_textram[0x0e9e] );
	sys16_fg2_scrolly = READ_WORD( &sys16_textram[0x0e94] );
	sys16_bg2_scrolly = READ_WORD( &sys16_textram[0x0e96] );

	data = READ_WORD( &sys16_textram[0x0e84] );
	sys16_fg2_page[1] = data>>12;
	sys16_fg2_page[3] = (data>>8)&0xf;
	sys16_fg2_page[0] = (data>>4)&0xf;
	sys16_fg2_page[2] = data&0xf;

	data = READ_WORD( &sys16_textram[0x0e86] );
	sys16_bg2_page[1] = data>>12;
	sys16_bg2_page[3] = (data>>8)&0xf;
	sys16_bg2_page[0] = (data>>4)&0xf;
	sys16_bg2_page[2] = data&0xf;

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | READ_WORD( &sys16_textram[0x0e84] ))
		sys18_fg2_active=1;
	else
		sys18_fg2_active=0;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | READ_WORD( &sys16_textram[0x0e86] ))
		sys18_bg2_active=1;
	else
		sys18_bg2_active=0;

	set_tile_bank18( READ_WORD( &sys16_extraram2[0xe] ) ); // 0xa0000f
	set_refresh_18( READ_WORD( &sys16_extraram3[0x6600] ) ); // 0xc46601
}

static void astorm_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};
	sys16_obj_bank = bank;
	sys16_fgxoffset = sys16_bgxoffset = -9;

	patch_code( 0x2D6E, 0x32 );
	patch_code( 0x2D6F, 0x3c );
	patch_code( 0x2D70, 0x80 );
	patch_code( 0x2D71, 0x00 );
	patch_code( 0x2D72, 0x33 );
	patch_code( 0x2D73, 0xc1 );
	patch_code( 0x2ea2, 0x30 );
	patch_code( 0x2ea3, 0x38 );
	patch_code( 0x2ea4, 0xec );
	patch_code( 0x2ea5, 0xf6 );
	patch_code( 0x2ea6, 0x30 );
	patch_code( 0x2ea7, 0x80 );
	patch_code( 0x2e5c, 0x30 );
	patch_code( 0x2e5d, 0x38 );
	patch_code( 0x2e5e, 0xec );
	patch_code( 0x2e5f, 0xe2 );
	patch_code( 0x2e60, 0xc0 );
	patch_code( 0x2e61, 0x7c );

	patch_code( 0x4cd8, 0x02 );
	patch_code( 0x4cec, 0x03 );
	patch_code( 0x2dc6c, 0xe9 );
	patch_code( 0x2dc64, 0x10 );
	patch_code( 0x2dc65, 0x10 );
	patch_code( 0x3a100, 0x10 );
	patch_code( 0x3a101, 0x13 );
	patch_code( 0x3a102, 0x90 );
	patch_code( 0x3a103, 0x2b );
	patch_code( 0x3a104, 0x00 );
	patch_code( 0x3a105, 0x01 );
	patch_code( 0x3a106, 0x0c );
	patch_code( 0x3a107, 0x00 );
	patch_code( 0x3a108, 0x00 );
	patch_code( 0x3a109, 0x01 );
	patch_code( 0x3a10a, 0x66 );
	patch_code( 0x3a10b, 0x06 );
	patch_code( 0x3a10c, 0x42 );
	patch_code( 0x3a10d, 0x40 );
	patch_code( 0x3a10e, 0x54 );
	patch_code( 0x3a10f, 0x8b );
	patch_code( 0x3a110, 0x60 );
	patch_code( 0x3a111, 0x02 );
	patch_code( 0x3a112, 0x30 );
	patch_code( 0x3a113, 0x1b );
	patch_code( 0x3a114, 0x34 );
	patch_code( 0x3a115, 0xc0 );
	patch_code( 0x3a116, 0x34 );
	patch_code( 0x3a117, 0xdb );
	patch_code( 0x3a118, 0x24 );
	patch_code( 0x3a119, 0xdb );
	patch_code( 0x3a11a, 0x24 );
	patch_code( 0x3a11b, 0xdb );
	patch_code( 0x3a11c, 0x4e );
	patch_code( 0x3a11d, 0x75 );
	patch_code( 0xaf8e, 0x66 );

	/* fix missing credit text */
	patch_code( 0x3f9a, 0xec );
	patch_code( 0x3f9b, 0x36 );

	sys16_update_proc = astorm_update_proc;
}

static void init_astorm( void )
{
	unsigned char *RAM= memory_region(REGION_CPU2);
	sys16_onetime_init_machine();
	sys18_splittab_fg_x=&sys16_textram[0x0f80];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0];

	memcpy(RAM,&RAM[0x10000],0xa000);
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=0;		// doesn't seem to use transparent shadows
#endif

	sys16_sprite_decode( 4,0x080000 );
}

/***************************************************************************/

INPUT_PORTS_START( astorm )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )
	SYS16_COINAGE

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easiest" )
	PORT_DIPSETTING(    0x08, "Easier" )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x1c, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x14, "Harder" )
	PORT_DIPSETTING(    0x18, "Hardest" )
	PORT_DIPSETTING(    0x00, "Special" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chutes" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_18( machine_driver_astorm, \
	astorm_readmem,astorm_writemem,astorm_init_machine, gfx4 )

/***************************************************************************/
// sys16B
ROM_START( atomicp )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ap-t2.bin", 0x000000, 0x10000, 0x97421047 )
	ROM_LOAD_ODD ( "ap-t1.bin", 0x000000, 0x10000, 0x5c65fe56 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ap-t4.bin",  0x00000, 0x8000, 0x332e58f4 )
	ROM_LOAD( "ap-t3.bin",  0x08000, 0x8000, 0xdddc122c )
	ROM_LOAD( "ap-t5.bin",  0x10000, 0x8000, 0xef5ecd6b )

	ROM_REGION( 0x1, REGION_GFX2 ) /* sprites */

ROM_END

/***************************************************************************/

/*static READ_HANDLER( atomicp_skip_r )
{
	if (cpu_get_pc()==0x7fc) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0902]);
}*/


static struct MemoryReadAddress atomicp_readmem[] =
{
	{ 0x000000, 0x01ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41000, 0xc41001, io_player1_r },
	{ 0xc41002, 0xc41003, io_player2_r },
	{ 0xc41004, 0xc41005, io_dip1_r },
	{ 0xc41006, 0xc41007, io_dip2_r },
//	{ 0xffc902, 0xffc903, atomicp_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static WRITE_HANDLER( atomicp_sound_w )
{
	if(offset==0)
		YM2413_register_port_0_w(0,(data>>8)&0xff);
	else
		YM2413_data_port_0_w(0,(data>>8)&0xff);
}

static struct MemoryWriteAddress atomicp_writemem[] =
{
	{ 0x000000, 0x01ffff, MWA_ROM },
	{ 0x080000, 0x080003, atomicp_sound_w },
	{ 0x3f0000, 0x3f0003, MWA_NOP },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x44ffff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

//	{ 0x0a, 0x0a, YM2413_register_port_0_w },
//	{ 0x0b, 0x0b, YM2413_data_port_0_w },

/***************************************************************************/

static void atomicp_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );
}

static void atomicp_init_machine( void ){
	static int bank[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	sys16_obj_bank = bank;
	sys16_update_proc = atomicp_update_proc;
}

static void init_atomicp( void )
{
	sys16_onetime_init_machine();
}

/***************************************************************************/

INPUT_PORTS_START( atomicp )

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

PORT_START	// dummy

PORT_START	// dip1
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )

	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_5C ) )

	PORT_DIPNAME( 0xC0, 0xC0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xC0, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x00, "5" )

PORT_START  //dip2
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Instructions" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Continuation" )
	PORT_DIPSETTING(    0x20, "Continue" )
	PORT_DIPSETTING(    0x00, "No Continue" )
	PORT_DIPNAME( 0x40, 0x00, "Level Select" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )
INPUT_PORTS_END

/***************************************************************************/
static int ap_interrupt( void ){
	int intleft=cpu_getiloops();
	if(intleft!=0) return 2;
	else return 4;
}

static struct YM2413interface ym2413_interface=
{
    1,
    8000000,	/* ??? */
    { 30 },
};

static struct MachineDriver machine_driver_atomicp =
{
	{
		{
			CPU_M68000,
			10000000,
			atomicp_readmem,atomicp_writemem,0,0,
			ap_interrupt,2
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	atomicp_init_machine,
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	gfx8,
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sys16_vh_start,
	sys16_vh_stop,
	sys16_vh_screenrefresh,
	0,0,0,0,
	{
		{
			SOUND_YM2413,
			&ym2413_interface
		}
	}
};

/***************************************************************************

   Aurail

***************************************************************************/
// sys16B
ROM_START( aurail )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "13577", 0x000000, 0x20000, 0x6701b686 )
	ROM_LOAD_ODD ( "13576", 0x000000, 0x20000, 0x1e428d94 )
	/* empty 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "13447", 0x080000, 0x20000, 0x70a52167 )
	ROM_LOAD_ODD ( "13445", 0x080000, 0x20000, 0x28dfc3dd )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "aurail.a14", 0x00000, 0x20000, 0x0fc4a7a8 ) /* plane 1 */
	ROM_LOAD( "aurail.b14", 0x20000, 0x20000, 0xe08135e0 )
	ROM_LOAD( "aurail.a15", 0x40000, 0x20000, 0x1c49852f ) /* plane 2 */
	ROM_LOAD( "aurail.b15", 0x60000, 0x20000, 0xe14c6684 )
	ROM_LOAD( "aurail.a16", 0x80000, 0x20000, 0x047bde5e ) /* plane 3 */
	ROM_LOAD( "aurail.b16", 0xa0000, 0x20000, 0x6309fec4 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "aurail.b1",  0x000000, 0x020000, 0x5fa0a9f8 )
	ROM_LOAD( "aurail.b5",  0x020000, 0x020000, 0x0d1b54da )
	ROM_LOAD( "aurail.b2",  0x040000, 0x020000, 0x5f6b33b1 )
	ROM_LOAD( "aurail.b6",  0x060000, 0x020000, 0xbad340c3 )
	ROM_LOAD( "aurail.b3",  0x080000, 0x020000, 0x4e80520b )
	ROM_LOAD( "aurail.b7",  0x0a0000, 0x020000, 0x7e9165ac )
	ROM_LOAD( "aurail.b4",  0x0c0000, 0x020000, 0x5733c428 )
	ROM_LOAD( "aurail.b8",  0x0e0000, 0x020000, 0x66b8f9b3 )
	ROM_LOAD( "aurail.a1",  0x100000, 0x020000, 0x4f370b2b )
	ROM_LOAD( "aurail.b10", 0x120000, 0x020000, 0xf76014bf )
	ROM_LOAD( "aurail.a2",  0x140000, 0x020000, 0x37cf9cb4 )
	ROM_LOAD( "aurail.b11", 0x160000, 0x020000, 0x1061e7da )
	ROM_LOAD( "aurail.a3",  0x180000, 0x020000, 0x049698ef )
	ROM_LOAD( "aurail.b12", 0x1a0000, 0x020000, 0x7dbcfbf1 )
	ROM_LOAD( "aurail.a4",  0x1c0000, 0x020000, 0x77a8989e )
	ROM_LOAD( "aurail.b13", 0x1e0000, 0x020000, 0x551df422 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "13448",      0x0000, 0x8000, 0xb5183fb9 )
	ROM_LOAD( "aurail.a12", 0x10000,0x20000, 0xd3d9aaf9 )
	ROM_LOAD( "aurail.a12", 0x30000,0x20000, 0xd3d9aaf9 )
ROM_END

ROM_START( auraila )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0168
	ROM_LOAD_EVEN( "epr13469.a7", 0x000000, 0x20000, 0xc628b69d )
	ROM_LOAD_ODD ( "epr13468.a5", 0x000000, 0x20000, 0xce092218 )
	/* 0x40000 - 0x80000 is empty, I will place decrypted opcodes here */
	ROM_LOAD_EVEN( "13447", 0x080000, 0x20000, 0x70a52167 )
	ROM_LOAD_ODD ( "13445", 0x080000, 0x20000, 0x28dfc3dd )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "aurail.a14", 0x00000, 0x20000, 0x0fc4a7a8 ) /* plane 1 */
	ROM_LOAD( "aurail.b14", 0x20000, 0x20000, 0xe08135e0 )
	ROM_LOAD( "aurail.a15", 0x40000, 0x20000, 0x1c49852f ) /* plane 2 */
	ROM_LOAD( "aurail.b15", 0x60000, 0x20000, 0xe14c6684 )
	ROM_LOAD( "aurail.a16", 0x80000, 0x20000, 0x047bde5e ) /* plane 3 */
	ROM_LOAD( "aurail.b16", 0xa0000, 0x20000, 0x6309fec4 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "aurail.b1",  0x000000, 0x020000, 0x5fa0a9f8 )
	ROM_LOAD( "aurail.b5",  0x020000, 0x020000, 0x0d1b54da )
	ROM_LOAD( "aurail.b2",  0x040000, 0x020000, 0x5f6b33b1 )
	ROM_LOAD( "aurail.b6",  0x060000, 0x020000, 0xbad340c3 )
	ROM_LOAD( "aurail.b3",  0x080000, 0x020000, 0x4e80520b )
	ROM_LOAD( "aurail.b7",  0x0a0000, 0x020000, 0x7e9165ac )
	ROM_LOAD( "aurail.b4",  0x0c0000, 0x020000, 0x5733c428 )
	ROM_LOAD( "aurail.b8",  0x0e0000, 0x020000, 0x66b8f9b3 )
	ROM_LOAD( "aurail.a1",  0x100000, 0x020000, 0x4f370b2b )
	ROM_LOAD( "aurail.b10", 0x120000, 0x020000, 0xf76014bf )
	ROM_LOAD( "aurail.a2",  0x140000, 0x020000, 0x37cf9cb4 )
	ROM_LOAD( "aurail.b11", 0x160000, 0x020000, 0x1061e7da )
	ROM_LOAD( "aurail.a3",  0x180000, 0x020000, 0x049698ef )
	ROM_LOAD( "aurail.b12", 0x1a0000, 0x020000, 0x7dbcfbf1 )
	ROM_LOAD( "aurail.a4",  0x1c0000, 0x020000, 0x77a8989e )
	ROM_LOAD( "aurail.b13", 0x1e0000, 0x020000, 0x551df422 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "13448",      0x0000, 0x8000, 0xb5183fb9 )
	ROM_LOAD( "aurail.a12", 0x10000,0x20000, 0xd3d9aaf9 )
	ROM_LOAD( "aurail.a12", 0x30000,0x20000, 0xd3d9aaf9 )
ROM_END


/***************************************************************************/

static READ_HANDLER( aurail_skip_r )
{
	if (cpu_get_pc()==0xe4e) {cpu_spinuntil_int(); return 0;}

	return READ_WORD(&sys16_workingram[0x274e]);
}

static struct MemoryReadAddress aurail_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x3f0000, 0x3fffff, MRA_EXTRAM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc4ffff, MRA_EXTRAM2 },
	{ 0xfc0000, 0xfc0fff, MRA_EXTRAM3 },
	{ 0xffe74e, 0xffe74f, aurail_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress aurail_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x3f0000, 0x3fffff, MWA_EXTRAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM2 },
	{ 0xfc0000, 0xfc0fff, MWA_EXTRAM3 },
	{ 0xfe0006, 0xfe0007, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void aurail_update_proc (void)
{
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_tile_bank( READ_WORD( &sys16_extraram3[0x0002] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void aurail_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};

	sys16_obj_bank = bank;
	sys16_spritesystem = 4;
	sys16_spritelist_end=0x8000;
	sys16_bg_priority_mode=1;

	sys16_update_proc = aurail_update_proc;
}

static void init_aurail (void)
{
	sys16_onetime_init_machine();
	sys16_sprite_decode (8,0x40000);
}

static void init_auraila(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = 0x40000;	/* place decrypted opcodes in a empty hole */

	init_aurail();

	memory_set_opcode_base(0,rom+diff);

	memcpy(rom+diff,rom,0x40000);

	aurail_decode_data(rom,rom,0x10000);
	aurail_decode_opcode1(rom+diff,rom+diff,0x10000);
	aurail_decode_opcode2(rom+diff+0x10000,rom+diff+0x10000,0x10000);
}

/***************************************************************************/

INPUT_PORTS_START( aurail )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x40, 0x40, "Controller select" )
	PORT_DIPSETTING(    0x40, "1 Player side" )
	PORT_DIPSETTING(    0x00, "2 Players side" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_aurail, \
	aurail_readmem,aurail_writemem,aurail_init_machine, gfx4,upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( bayroute )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "br.4a", 0x000000, 0x10000, 0x91c6424b )
	ROM_LOAD_ODD ( "br.1a", 0x000000, 0x10000, 0x76954bf3 )
	/* empty 0x20000-0x80000*/
	ROM_LOAD_EVEN( "br.5a", 0x080000, 0x10000, 0x9d6fd183 )
	ROM_LOAD_ODD ( "br.2a", 0x080000, 0x10000, 0x5ca1e3d2 )
	ROM_LOAD_EVEN( "br.6a", 0x0a0000, 0x10000, 0xed97ad4c )
	ROM_LOAD_ODD ( "br.3a", 0x0a0000, 0x10000, 0x0d362905 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr12462.a14", 0x00000, 0x10000, 0xa19943b5 )
	ROM_LOAD( "opr12463.a15", 0x10000, 0x10000, 0x62f8200d )
	ROM_LOAD( "opr12464.a16", 0x20000, 0x10000, 0xc8c59703 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "br_obj0o.1b", 0x00000, 0x10000, 0x098a5e82 )
	ROM_LOAD( "br_obj0e.5b", 0x10000, 0x10000, 0x85238af9 )
	ROM_LOAD( "br_obj1o.2b", 0x20000, 0x10000, 0xcc641da1 )
	ROM_LOAD( "br_obj1e.6b", 0x30000, 0x10000, 0xd3123315 )
	ROM_LOAD( "br_obj2o.3b", 0x40000, 0x10000, 0x84efac1f )
	ROM_LOAD( "br_obj2e.7b", 0x50000, 0x10000, 0xb73b12cb )
	ROM_LOAD( "br_obj3o.4b", 0x60000, 0x10000, 0xa2e238ac )
	ROM_LOAD( "br.8b",		 0x70000, 0x10000, 0xd8de78ff )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12459.a10", 0x00000, 0x08000, 0x3e1d29d0 )
	ROM_LOAD( "mpr12460.a11", 0x10000, 0x20000, 0x0bae570d )
	ROM_LOAD( "mpr12461.a12", 0x30000, 0x20000, 0xb03b8b46 )
ROM_END

ROM_START( bayrouta )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0116
	ROM_LOAD_EVEN( "epr12517.a7", 0x000000, 0x20000, 0x436728a9 )
	ROM_LOAD_ODD ( "epr12516.a5", 0x000000, 0x20000, 0x4ff0353f )
	/* empty 0x40000-0x80000*/
	ROM_LOAD_EVEN( "epr12458.a8", 0x080000, 0x20000, 0xe7c7476a )
	ROM_LOAD_ODD ( "epr12456.a6", 0x080000, 0x20000, 0x25dc2eaf )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr12462.a14", 0x00000, 0x10000, 0xa19943b5 )
	ROM_LOAD( "opr12463.a15", 0x10000, 0x10000, 0x62f8200d )
	ROM_LOAD( "opr12464.a16", 0x20000, 0x10000, 0xc8c59703 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr12465.b1", 0x00000, 0x20000, 0x11d61b45 )
	ROM_LOAD( "mpr12467.b5", 0x20000, 0x20000, 0xc3b4e4c0 )
	ROM_LOAD( "mpr12466.b2", 0x40000, 0x20000, 0xa57f236f )
	ROM_LOAD( "mpr12468.b6", 0x60000, 0x20000, 0xd89c77de )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12459.a10", 0x00000, 0x08000, 0x3e1d29d0 )
	ROM_LOAD( "mpr12460.a11", 0x10000, 0x20000, 0x0bae570d )
	ROM_LOAD( "mpr12461.a12", 0x30000, 0x20000, 0xb03b8b46 )
ROM_END

ROM_START( bayrtbl1 )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "b4.bin", 0x000000, 0x10000, 0xeb6646ae )
	ROM_LOAD_ODD ( "b2.bin", 0x000000, 0x10000, 0xecd9cd0e )
	/* empty 0x20000-0x80000*/
	ROM_LOAD_EVEN( "br.5a",  0x080000, 0x10000, 0x9d6fd183 )
	ROM_LOAD_ODD ( "br.2a",  0x080000, 0x10000, 0x5ca1e3d2 )
	ROM_LOAD_EVEN( "b8.bin", 0x0a0000, 0x10000, 0xe7ca0331 )
	ROM_LOAD_ODD ( "b6.bin", 0x0a0000, 0x10000, 0x2bc748a6 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "bs16.bin", 0x00000, 0x10000, 0xa8a5b310 )
	ROM_LOAD( "bs14.bin", 0x10000, 0x10000, 0x6bc4d0a8 )
	ROM_LOAD( "bs12.bin", 0x20000, 0x10000, 0xc1f967a6 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "br_obj0o.1b", 0x00000, 0x10000, 0x098a5e82 )
	ROM_LOAD( "br_obj0e.5b", 0x10000, 0x10000, 0x85238af9 )
	ROM_LOAD( "br_obj1o.2b", 0x20000, 0x10000, 0xcc641da1 )
	ROM_LOAD( "br_obj1e.6b", 0x30000, 0x10000, 0xd3123315 )
	ROM_LOAD( "br_obj2o.3b", 0x40000, 0x10000, 0x84efac1f )
	ROM_LOAD( "br_obj2e.7b", 0x50000, 0x10000, 0xb73b12cb )
	ROM_LOAD( "br_obj3o.4b", 0x60000, 0x10000, 0xa2e238ac )
	ROM_LOAD( "bs7.bin",     0x70000, 0x10000, 0x0c91abcc )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12459.a10", 0x00000, 0x08000, 0x3e1d29d0 )
	ROM_LOAD( "mpr12460.a11", 0x10000, 0x20000, 0x0bae570d )
	ROM_LOAD( "mpr12461.a12", 0x30000, 0x20000, 0xb03b8b46 )
ROM_END

ROM_START( bayrtbl2 )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "br_04", 0x000000, 0x10000, 0x2e33ebfc )
	ROM_LOAD_ODD ( "br_06", 0x000000, 0x10000, 0x3db42313 )
	/* empty 0x20000-0x80000*/
	ROM_LOAD_EVEN( "br_03", 0x080000, 0x20000, 0x285d256b )
	ROM_LOAD_ODD ( "br_05", 0x080000, 0x20000, 0x552e6384 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "br_15",    0x00000, 0x10000, 0x050079a9 )
	ROM_LOAD( "br_16",    0x10000, 0x10000, 0xfc371928 )
	ROM_LOAD( "bs12.bin", 0x20000, 0x10000, 0xc1f967a6 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "br_11",       0x00000, 0x10000, 0x65232905 )
	ROM_LOAD( "br_obj0e.5b", 0x10000, 0x10000, 0x85238af9 )
	ROM_LOAD( "br_obj1o.2b", 0x20000, 0x10000, 0xcc641da1 )
	ROM_LOAD( "br_obj1e.6b", 0x30000, 0x10000, 0xd3123315 )
	ROM_LOAD( "br_obj2o.3b", 0x40000, 0x10000, 0x84efac1f )
	ROM_LOAD( "br_09",       0x50000, 0x10000, 0x05e9b840 )
	ROM_LOAD( "br_14",       0x60000, 0x10000, 0x4c4a177b )
	ROM_LOAD( "bs7.bin",     0x70000, 0x10000, 0x0c91abcc )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "br_01", 0x00000, 0x10000, 0xb87156ec )
	ROM_LOAD( "br_02", 0x10000, 0x10000, 0xef63991b )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress bayroute_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x500000, 0x503fff, MRA_EXTRAM3 },
	{ 0x600000, 0x600fff, MRA_SPRITERAM },
	{ 0x700000, 0x70ffff, MRA_TILERAM },
	{ 0x710000, 0x710fff, MRA_TEXTRAM },
	{ 0x800000, 0x800fff, MRA_PALETTERAM },
	{ 0x901002, 0x901003, io_player1_r },
	{ 0x901006, 0x901007, io_player2_r },
	{ 0x901000, 0x901001, io_service_r },
	{ 0x902002, 0x902003, io_dip1_r },
	{ 0x902000, 0x902001, io_dip2_r },
	{ 0x900000, 0x900fff, MRA_EXTRAM2 },

	{-1}
};

static struct MemoryWriteAddress bayroute_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x500000, 0x503fff, MWA_EXTRAM3 },
	{ 0x600000, 0x600fff, MWA_SPRITERAM },
	{ 0x700000, 0x70ffff, MWA_TILERAM },
	{ 0x710000, 0x710fff, MWA_TEXTRAM },
	{ 0x800000, 0x800fff, MWA_PALETTERAM },
	{ 0x900000, 0x900fff, MWA_EXTRAM2 },
	{ 0xff0006, 0xff0007, sound_command_w },

	{-1}
};

/***************************************************************************/

static void bayroute_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram2[0x0] ) );
}

static void bayroute_init_machine( void ){
	static int bank[16] = { 0,0,0,0,0,0,0,6,0,0,0,4,0,2,0,0 };
	sys16_obj_bank = bank;
	sys16_update_proc = bayroute_update_proc;
	sys16_spritesystem = 4;
	sys16_spritelist_end=0xc000;
}

static void init_bayroute( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x20000 );
}

static void init_bayrouta( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 2,0x40000 );
}

static void init_bayrtbl1( void )
{
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x30000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	sys16_sprite_decode( 4,0x20000 );
}
/***************************************************************************/

INPUT_PORTS_START( bayroute )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Unlimited", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "10000" )
	PORT_DIPSETTING(    0x20, "15000" )
	PORT_DIPSETTING(    0x10, "20000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0xc0, "A" )
	PORT_DIPSETTING(    0x80, "B" )
	PORT_DIPSETTING(    0x40, "C" )
	PORT_DIPSETTING(    0x00, "D" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_bayroute, \
	bayroute_readmem,bayroute_writemem,bayroute_init_machine, gfx1,upd7759_interface )

/***************************************************************************

   Body Slam

***************************************************************************/
// pre16
ROM_START( bodyslam )
	ROM_REGION( 0x30000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr10066.b9", 0x000000, 0x8000, 0x6cd53290 )
	ROM_LOAD_ODD ( "epr10063.b6", 0x000000, 0x8000, 0xdd849a16 )
	ROM_LOAD_EVEN( "epr10067.b10",0x010000, 0x8000, 0xdb22a5ce )
	ROM_LOAD_ODD ( "epr10064.b7", 0x010000, 0x8000, 0x53d6b7e0 )
	ROM_LOAD_EVEN( "epr10068.b11",0x020000, 0x8000, 0x15ccc665 )
	ROM_LOAD_ODD ( "epr10065.b8", 0x020000, 0x8000, 0x0e5fa314 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr10321.c9",  0x00000, 0x8000, 0xcd3e7cba ) /* plane 1 */
	ROM_LOAD( "epr10322.c10", 0x08000, 0x8000, 0xb53d3217 ) /* plane 2 */
	ROM_LOAD( "epr10323.c11", 0x10000, 0x8000, 0x915a3e61 ) /* plane 3 */

	ROM_REGION( 0x50000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr10012.c5",  0x000000, 0x08000, 0x990824e8 )
	ROM_RELOAD(               0x040000, 0x08000 )
	ROM_LOAD( "epr10016.b2",  0x008000, 0x08000, 0xaf5dc72f )
	ROM_RELOAD(               0x048000, 0x08000 )
	ROM_LOAD( "epr10013.c6",  0x010000, 0x08000, 0x9a0919c5 )
	ROM_LOAD( "epr10017.b3",  0x018000, 0x08000, 0x62aafd95 )
	ROM_LOAD( "epr10027.c7",  0x020000, 0x08000, 0x3f1c57c7 )
	ROM_LOAD( "epr10028.b4",  0x028000, 0x08000, 0x80d4946d )
	ROM_LOAD( "epr10015.c8",  0x030000, 0x08000, 0x582d3b6a )
	ROM_LOAD( "epr10019.b5",  0x038000, 0x08000, 0xe020c38b )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr10026.b1", 0x00000, 0x8000, 0x123b69b8 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr10029.c1", 0x00000, 0x8000, 0x7e4aca83 )
	ROM_LOAD( "epr10030.c2", 0x08000, 0x8000, 0xdcc1df0b )
	ROM_LOAD( "epr10031.c3", 0x10000, 0x8000, 0xea3c4472 )
	ROM_LOAD( "epr10032.c4", 0x18000, 0x8000, 0x0aabebce )

ROM_END

ROM_START( dumpmtmt )
	ROM_REGION( 0x30000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "7704a.bin", 0x000000, 0x8000, 0x96de6c7b )
	ROM_LOAD_ODD ( "7701a.bin", 0x000000, 0x8000, 0x786d1009 )
	ROM_LOAD_EVEN( "7705a.bin", 0x010000, 0x8000, 0xfc584391 )
	ROM_LOAD_ODD ( "7702a.bin", 0x010000, 0x8000, 0x2241a8fd )
	ROM_LOAD_EVEN( "7706a.bin", 0x020000, 0x8000, 0x6bbcc9d0 )
	ROM_LOAD_ODD ( "7703a.bin", 0x020000, 0x8000, 0xfcb0cd40 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "7707a.bin",  0x00000, 0x8000, 0x45318738 ) /* plane 1 */
	ROM_LOAD( "7708a.bin",  0x08000, 0x8000, 0x411be9a4 ) /* plane 2 */
	ROM_LOAD( "7709a.bin",  0x10000, 0x8000, 0x74ceb5a8 ) /* plane 3 */

	ROM_REGION( 0x50000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "7715.bin",  0x000000, 0x08000, 0xbf47e040 )
	ROM_RELOAD(               0x040000, 0x08000 )
	ROM_LOAD( "7719.bin",  0x008000, 0x08000, 0xfa5c5d6c )
	ROM_RELOAD(               0x048000, 0x08000 )
	ROM_LOAD( "epr10013.c6",  0x010000, 0x08000, 0x9a0919c5 )	/* 7716 */
	ROM_LOAD( "epr10017.b3",  0x018000, 0x08000, 0x62aafd95 )	/* 7720 */
	ROM_LOAD( "7717.bin",  0x020000, 0x08000, 0xfa64c86d )
	ROM_LOAD( "7721.bin",  0x028000, 0x08000, 0x62a9143e )
	ROM_LOAD( "epr10015.c8",  0x030000, 0x08000, 0x582d3b6a )	/* 7718 */
	ROM_LOAD( "epr10019.b5",  0x038000, 0x08000, 0xe020c38b )	/* 7722 */

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "7710a.bin", 0x00000, 0x8000, 0xa19b8ba8 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "7711.bin", 0x00000, 0x8000, 0xefa9aabd )
	ROM_LOAD( "7712.bin", 0x08000, 0x8000, 0x7bcd85cf )
	ROM_LOAD( "7713.bin", 0x10000, 0x8000, 0x33f292e7 )
	ROM_LOAD( "7714.bin", 0x18000, 0x8000, 0x8fd48c47 )

ROM_END

/***************************************************************************/

static struct MemoryReadAddress bodyslam_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc40000, 0xc400ff, MRA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress bodyslam_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40000, 0xc400ff, MWA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void bodyslam_update_proc (void)
{
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x01ff;

	set_fg_page1( READ_WORD( &sys16_textram[0x0e9e] ) );
	set_bg_page1( READ_WORD( &sys16_textram[0x0e9c] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram2[2] ) );
}


static void bodyslam_init_machine( void ){
	static int bank[16] = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 2;
	sys16_sprxoffset = -0xbc;
	sys16_fgxoffset = sys16_bgxoffset = 7;
	sys16_bg_priority_mode = 2;
	sys16_bg_priority_value=0x0e00;

	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0x1f;
	sys16_textlayer_hi_min=0x20;
	sys16_textlayer_hi_max=0xff;

	sys16_update_proc = bodyslam_update_proc;
}

static void bodyslam_sprite_decode (void)
{
	sys16_sprite_decode (5,0x10000);
}


// I have no idea if this is needed, but I cannot find any code for the countdown
// timer in the code and this seems to work ok.
static void bodyslam_irq_timer(void)
{
	int flag=READ_WORD(&sys16_workingram[0x200])>>8;
	int tick=READ_WORD(&sys16_workingram[0x200])&0xff;
	int sec=READ_WORD(&sys16_workingram[0x202])>>8;
	int min=READ_WORD(&sys16_workingram[0x202])&0xff;

	if(tick == 0 && sec == 0 && min == 0)
		flag=1;
	else
	{
		if(tick==0)
		{
			tick=0x40;	// The game initialise this to 0x40
			if(sec==0)
			{
				sec=0x59;
				if(min==0)
				{
					flag=1;
					tick=sec=min=0;
				}
				else
					min--;
			}
			else
			{
				if((sec&0xf)==0)
				{
					sec-=0x10;
					sec|=9;
				}
				else
					sec--;

			}
		}
		else
			tick--;
	}
	WRITE_WORD(&sys16_workingram[0x200],(flag<<8)+tick);
	WRITE_WORD(&sys16_workingram[0x202],(sec<<8)+min);
}

static void init_bodyslam( void ){
	sys16_onetime_init_machine();
	sys16_bg1_trans=1;
	sys16_custom_irq=bodyslam_irq_timer;
	bodyslam_sprite_decode();
}

/***************************************************************************/

INPUT_PORTS_START( bodyslam )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7751( machine_driver_bodyslam, \
	bodyslam_readmem,bodyslam_writemem,bodyslam_init_machine, gfx8 )

/***************************************************************************/
// sys16B
ROM_START( dduxbl )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "dduxb03.bin", 0x000000, 0x20000, 0xe7526012 )
	ROM_LOAD_ODD ( "dduxb05.bin", 0x000000, 0x20000, 0x459d1237 )
	/* empty 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "dduxb02.bin", 0x080000, 0x20000, 0xd8ed3132 )
	ROM_LOAD_ODD ( "dduxb04.bin", 0x080000, 0x20000, 0x30c6cb92 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "dduxb14.bin", 0x00000, 0x10000, 0x664bd135 )
	ROM_LOAD( "dduxb15.bin", 0x10000, 0x10000, 0xce0d2b30 )
	ROM_LOAD( "dduxb16.bin", 0x20000, 0x10000, 0x6de95434 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "dduxb10.bin", 0x000000, 0x010000, 0x0be3aee5 )
	ROM_LOAD( "dduxb06.bin", 0x010000, 0x010000, 0xb0079e99 )
	ROM_LOAD( "dduxb11.bin", 0x020000, 0x010000, 0xcfb2af18 )
	ROM_LOAD( "dduxb07.bin", 0x030000, 0x010000, 0x0217369c )
	ROM_LOAD( "dduxb12.bin", 0x040000, 0x010000, 0x28ce9b15 )
	ROM_LOAD( "dduxb08.bin", 0x050000, 0x010000, 0x8844f336 )
	ROM_LOAD( "dduxb13.bin", 0x060000, 0x010000, 0xefe57759 )
	ROM_LOAD( "dduxb09.bin", 0x070000, 0x010000, 0x6b64f665 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "dduxb01.bin", 0x0000, 0x8000, 0x0dbef0d7 )
ROM_END

/***************************************************************************/
static READ_HANDLER( dduxbl_skip_r )
{
	if (cpu_get_pc()==0x502) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x36e0]);
}

static struct MemoryReadAddress dduxbl_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41004, 0xc41005, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xfff6e0, 0xfff6e1, dduxbl_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress dduxbl_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40006, 0xc40007, sound_command_w },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void dduxbl_update_proc( void ){
	sys16_fg_scrollx = (READ_WORD( &sys16_extraram2[0x6018] ) ^ 0xffff) & 0x01ff;
	sys16_bg_scrollx = (READ_WORD( &sys16_extraram2[0x6008] ) ^ 0xffff) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_extraram2[0x6010] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_extraram2[0x6000] );

	{
		unsigned char lu = READ_WORD( &sys16_extraram2[0x6020] ) & 0xff;
		unsigned char ru = READ_WORD( &sys16_extraram2[0x6022] ) & 0xff;
		unsigned char ld = READ_WORD( &sys16_extraram2[0x6024] ) & 0xff;
		unsigned char rd = READ_WORD( &sys16_extraram2[0x6026] ) & 0xff;

		if (lu==4 && ld==4 && ru==5 && rd==5)
		{ // fix a bug in chicago round (un-tested in MAME)
			int vs=READ_WORD(&sys16_workingram[0x36ec]);
			sys16_bg_scrolly = vs & 0xff;
			sys16_fg_scrolly = vs & 0xff;
			if (vs >= 0x100)
			{
				lu=0x26; ru=0x37;
				ld=0x04; rd=0x15;
			} else {
				ld=0x26; rd=0x37;
				lu=0x04; ru=0x15;
			}
		}
		sys16_fg_page[0] = ld&0xf;
		sys16_fg_page[1] = rd&0xf;
		sys16_fg_page[2] = lu&0xf;
		sys16_fg_page[3] = ru&0xf;

		sys16_bg_page[0] = ld>>4;
		sys16_bg_page[1] = rd>>4;
		sys16_bg_page[2] = lu>>4;
		sys16_bg_page[3] = ru>>4;
	}

	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void dduxbl_init_machine( void ){
	static int bank[16] = {00,00,00,00,00,00,00,0x06,00,00,00,0x04,00,0x02,00,00};

	sys16_obj_bank = bank;

	patch_code( 0x1eb2e, 0x01 );
	patch_code( 0x1eb2f, 0x01 );
	patch_code( 0x1eb3c, 0x00 );
	patch_code( 0x1eb3d, 0x00 );
	patch_code( 0x23132, 0x01 );
	patch_code( 0x23133, 0x01 );
	patch_code( 0x23140, 0x00 );
	patch_code( 0x23141, 0x00 );
	patch_code( 0x24a9a, 0x01 );
	patch_code( 0x24a9b, 0x01 );
	patch_code( 0x24aa8, 0x00 );
	patch_code( 0x24aa9, 0x00 );

	sys16_update_proc = dduxbl_update_proc;
	sys16_sprxoffset = -0x48;
}

static void init_dduxbl(void)
{
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x30000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	sys16_sprite_decode( 4,0x020000 );
}
/***************************************************************************/

INPUT_PORTS_START( dduxbl )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x06, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x18, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x40, "150000" )
	PORT_DIPSETTING(    0x60, "200000" )
	PORT_DIPSETTING(    0x20, "300000" )
	PORT_DIPSETTING(    0x00, "400000" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER( machine_driver_dduxbl, \
	dduxbl_readmem,dduxbl_writemem,dduxbl_init_machine, gfx1)

/***************************************************************************/
// sys16B
ROM_START( eswat )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "12657", 0x000000, 0x40000, 0xcfb935e9 )
	ROM_LOAD_ODD ( "12656", 0x000000, 0x40000, 0xbe3f9d28 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "e12624r", 0x00000, 0x40000, 0xe7b8545e )
	ROM_LOAD( "e12625r", 0x40000, 0x40000, 0xb418582c )
	ROM_LOAD( "e12626r", 0x80000, 0x40000, 0xba65789b )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "e12618r", 0x000000, 0x040000, 0x2d9ae975 )
	ROM_LOAD( "e12621r", 0x040000, 0x040000, 0x1e6c4cf7 )
	ROM_LOAD( "e12619r", 0x080000, 0x040000, 0x5f7ee6f6 )
	ROM_LOAD( "e12622r", 0x0c0000, 0x040000, 0x33251fde )
	ROM_LOAD( "e12620r", 0x100000, 0x040000, 0x905f9be2 )
	ROM_LOAD( "e12623r", 0x140000, 0x040000, 0xa25ea1fc )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "e12617", 0x00000, 0x08000, 0x537930cb )
	ROM_LOAD( "e12616r",0x10000, 0x20000, 0xf213fa4a )
ROM_END

ROM_START( eswatbl )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "eswat_c.rom", 0x000000, 0x10000, 0x1028cc81 )
	ROM_LOAD_ODD ( "eswat_f.rom", 0x000000, 0x10000, 0xf7b2d388 )
	ROM_LOAD_EVEN( "eswat_b.rom", 0x020000, 0x10000, 0x87c6b1b5 )
	ROM_LOAD_ODD ( "eswat_e.rom", 0x020000, 0x10000, 0x937ddf9a )
	ROM_LOAD_EVEN( "eswat_a.rom", 0x040000, 0x08000, 0x2af4fc62 )
	ROM_LOAD_ODD ( "eswat_d.rom", 0x040000, 0x08000, 0xb4751e19 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ic19.bin", 0x00000, 0x40000, 0x375a5ec4 )
	ROM_LOAD( "ic20.bin", 0x40000, 0x40000, 0x3b8c757e )
	ROM_LOAD( "ic21.bin", 0x80000, 0x40000, 0x3efca25c )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ic9.bin",  0x000000, 0x040000, 0x0d1530bf )
	ROM_LOAD( "ic12.bin", 0x040000, 0x040000, 0x18ff0799 )
	ROM_LOAD( "ic10.bin", 0x080000, 0x040000, 0x32069246 )
	ROM_LOAD( "ic13.bin", 0x0c0000, 0x040000, 0xa3dfe436 )
	ROM_LOAD( "ic11.bin", 0x100000, 0x040000, 0xf6b096e0 )
	ROM_LOAD( "ic14.bin", 0x140000, 0x040000, 0x6773fef6 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ic8.bin", 0x0000, 0x8000, 0x7efecf23 )
	ROM_LOAD( "ic6.bin", 0x10000, 0x40000, 0x254347c2 )
ROM_END
/***************************************************************************/

static READ_HANDLER( eswatbl_skip_r )
{
	if (cpu_get_pc()==0x65c) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0454]);
}

static struct MemoryReadAddress eswat_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x418fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xffc454, 0xffc455, eswatbl_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static int eswat_tilebank0;

static WRITE_HANDLER( eswat_tilebank0_w )
{
	eswat_tilebank0 = data;
}

static struct MemoryWriteAddress eswat_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x3e2000, 0x3e2001, eswat_tilebank0_w },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x418fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc42006, 0xc42007, sound_command_w },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM2 },
	{ 0xc80000, 0xc80001, MWA_NOP },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void eswat_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x8008] ) ^ 0xffff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x8018] ) ^ 0xffff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x8000] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x8010] );

	set_fg_page( READ_WORD( &sys16_textram[0x8020] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x8028] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );

	sys16_tile_bank1 = (READ_WORD( &sys16_textram[0x8030] ))&0xf;
	sys16_tile_bank0 = eswat_tilebank0;
}

static void eswat_init_machine( void ){
	static int bank[16] = { 0,2,8,10,16,18,24,26,4,6,12,14,20,22,28,30};

	sys16_obj_bank = bank;
	sys16_sprxoffset = -0x23c;

	patch_code( 0x3897, 0x11 );

	sys16_update_proc = eswat_update_proc;
}

static void init_eswat( void ){
	sys16_onetime_init_machine();
	sys16_rowscroll_scroll=0x8000;
	sys18_splittab_fg_x=&sys16_textram[0x0f80];

	sys16_sprite_decode( 3,0x080000 );
}

/***************************************************************************/

INPUT_PORTS_START( eswat )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Display Flip" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Time" )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_eswat, \
	eswat_readmem,eswat_writemem,eswat_init_machine, gfx4,upd7759_interface )

/***************************************************************************/
// sys16A
ROM_START( fantzono )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "7385.43", 0x000000, 0x8000, 0x5cb64450 )
	ROM_LOAD_ODD ( "7382.26", 0x000000, 0x8000, 0x3fda7416 )
	ROM_LOAD_EVEN( "7386.42", 0x010000, 0x8000, 0x15810ace )
	ROM_LOAD_ODD ( "7383.25", 0x010000, 0x8000, 0xa001e10a )
	ROM_LOAD_EVEN( "7387.41", 0x020000, 0x8000, 0x0acd335d )
	ROM_LOAD_ODD ( "7384.24", 0x020000, 0x8000, 0xfd909341 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "7388.95", 0x00000, 0x08000, 0x8eb02f6b )
	ROM_LOAD( "7389.94", 0x08000, 0x08000, 0x2f4f71b8 )
	ROM_LOAD( "7390.93", 0x10000, 0x08000, 0xd90609c6 )

	ROM_REGION( 0x030000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "7392.10", 0x000000, 0x008000, 0x5bb7c8b6 )
	ROM_LOAD( "7396.11", 0x008000, 0x008000, 0x74ae4b57 )
	ROM_LOAD( "7393.17", 0x010000, 0x008000, 0x14fc7e82 )
	ROM_LOAD( "7397.18", 0x018000, 0x008000, 0xe05a1e25 )
	ROM_LOAD( "7394.23", 0x020000, 0x008000, 0x531ca13f )
	ROM_LOAD( "7398.24", 0x028000, 0x008000, 0x68807b49 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "7535.12", 0x0000, 0x8000, 0x0cb2126a )
ROM_END

ROM_START( fantzone )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr7385a.43", 0x000000, 0x8000, 0x4091af42 )
	ROM_LOAD_ODD ( "epr7382a.26", 0x000000, 0x8000, 0x77d67bfd )
	ROM_LOAD_EVEN( "epr7386a.42", 0x010000, 0x8000, 0xb0a67cd0 )
	ROM_LOAD_ODD ( "epr7383a.25", 0x010000, 0x8000, 0x5f79b2a9 )
	ROM_LOAD_EVEN( "7387.41", 0x020000, 0x8000, 0x0acd335d )
	ROM_LOAD_ODD ( "7384.24", 0x020000, 0x8000, 0xfd909341 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "7388.95", 0x00000, 0x08000, 0x8eb02f6b )
	ROM_LOAD( "7389.94", 0x08000, 0x08000, 0x2f4f71b8 )
	ROM_LOAD( "7390.93", 0x10000, 0x08000, 0xd90609c6 )

	ROM_REGION( 0x030000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "7392.10", 0x000000, 0x008000, 0x5bb7c8b6 )
	ROM_LOAD( "7396.11", 0x008000, 0x008000, 0x74ae4b57 )
	ROM_LOAD( "7393.17", 0x010000, 0x008000, 0x14fc7e82 )
	ROM_LOAD( "7397.18", 0x018000, 0x008000, 0xe05a1e25 )
	ROM_LOAD( "7394.23", 0x020000, 0x008000, 0x531ca13f )
	ROM_LOAD( "7398.24", 0x028000, 0x008000, 0x68807b49 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr7535a.12", 0x0000, 0x8000, 0xbc1374fa )
ROM_END


/***************************************************************************/

static READ_HANDLER( fantzone_skip_r )
{
	if (cpu_get_pc()==0x91b2) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x022a]);
}

static struct MemoryReadAddress fantzono_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc40000, 0xc40003, MRA_EXTRAM2 },
	{ 0xffc22a, 0xffc22b, fantzone_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress fantzono_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40000, 0xc40003, MWA_EXTRAM2 },
	{ 0xc60000, 0xc60003, MWA_NOP },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

static struct MemoryReadAddress fantzone_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc40000, 0xc40003, MRA_EXTRAM2 },
	{ 0xffc22a, 0xffc22b, fantzone_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress fantzone_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40000, 0xc40003, MWA_EXTRAM2 },
	{ 0xc60000, 0xc60003, MWA_NOP },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void fantzone_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	set_fg_page1( READ_WORD( &sys16_textram[0x0e9e] ) );
	set_bg_page1( READ_WORD( &sys16_textram[0x0e9c] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram2[2] ) );	// c40003
}

static void fantzono_init_machine( void ){
	static int bank[16] = { 00,01,02,03,00,01,02,03,00,01,02,03,00,01,02,03};

	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 3;
	sys16_sprxoffset = -0xbe;
//	sys16_fgxoffset = sys16_bgxoffset = 8;
	sys16_fg_priority_mode=3;				// fixes end of game priority
	sys16_fg_priority_value=0xd000;

	patch_code( 0x20e7, 0x16 );
	patch_code( 0x30ef, 0x16 );

	// solving Fantasy Zone scrolling bug
	patch_code(0x308f,0x00);

	// invincible
/*	patch_code(0x224e,0x4e);
	patch_code(0x224f,0x71);
	patch_code(0x2250,0x4e);
	patch_code(0x2251,0x71);

	patch_code(0x2666,0x4e);
	patch_code(0x2667,0x71);
	patch_code(0x2668,0x4e);
	patch_code(0x2669,0x71);

	patch_code(0x25c0,0x4e);
	patch_code(0x25c1,0x71);
	patch_code(0x25c2,0x4e);
	patch_code(0x25c3,0x71);
*/

	sys16_update_proc = fantzone_update_proc;
}

static void fantzone_init_machine( void ){
	static int bank[16] = { 00,01,02,03,00,01,02,03,00,01,02,03,00,01,02,03};

	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 3;
	sys16_sprxoffset = -0xbe;
	sys16_fg_priority_mode=3;				// fixes end of game priority
	sys16_fg_priority_value=0xd000;

	patch_code( 0x2135, 0x16 );
	patch_code( 0x3649, 0x16 );

	// solving Fantasy Zone scrolling bug
	patch_code(0x35e9,0x00);

	sys16_update_proc = fantzone_update_proc;
}

static void init_fantzone( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 3,0x010000 );
}
/***************************************************************************/

INPUT_PORTS_START( fantzone )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "240", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, "Extra Ship Cost" )
	PORT_DIPSETTING(    0x30, "5000" )
	PORT_DIPSETTING(    0x20, "10000" )
	PORT_DIPSETTING(    0x10, "15000" )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER( machine_driver_fantzono, \
	fantzono_readmem,fantzono_writemem,fantzono_init_machine, gfx8 )
MACHINE_DRIVER( machine_driver_fantzone, \
	fantzone_readmem,fantzone_writemem,fantzone_init_machine, gfx8 )

/***************************************************************************/
// sys16B
ROM_START( fpoint )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "12591b.bin", 0x000000, 0x10000, 0x248b3e1b )
	ROM_LOAD_ODD ( "12590b.bin", 0x000000, 0x10000, 0x75256e3d )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "12595.bin", 0x00000, 0x10000, 0x5b18d60b )
	ROM_LOAD( "12594.bin", 0x10000, 0x10000, 0x8bfc4815 )
	ROM_LOAD( "12593.bin", 0x20000, 0x10000, 0xcc0582d8 )

	ROM_REGION( 0x020000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "12596.bin", 0x000000, 0x010000, 0x4a4041f3 )
	ROM_LOAD( "12597.bin", 0x010000, 0x010000, 0x6961e676 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "12592.bin", 0x0000, 0x8000, 0x9a8c11bb )
ROM_END

ROM_START( fpointbl )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "flpoint.003", 0x000000, 0x10000, 0x4d6df514 )
	ROM_LOAD_ODD ( "flpoint.002", 0x000000, 0x10000, 0x4dff2ee8 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "flpoint.006", 0x00000, 0x10000, 0xc539727d )
	ROM_LOAD( "flpoint.005", 0x10000, 0x10000, 0x82c0b8b0 )
	ROM_LOAD( "flpoint.004", 0x20000, 0x10000, 0x522426ae )

	ROM_REGION( 0x020000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "12596.bin", 0x000000, 0x010000, 0x4a4041f3 )
	ROM_LOAD( "12597.bin", 0x010000, 0x010000, 0x6961e676 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "12592.bin",   0x0000, 0x8000, 0x9a8c11bb )	// wrong sound rom? (this ones from the original)
//	ROM_LOAD( "flpoint.001", 0x0000, 0x8000, 0xc5b8e0fe )	// bootleg rom doesn't work!
ROM_END
/***************************************************************************/

static READ_HANDLER( fp_io_service_dummy_r ){
	int data = input_port_2_r( 0 ) & 0xff;
	return (data << 8) + data;
}

static struct MemoryReadAddress fpoint_readmem[] =
{
	{ 0x000000, 0x01ffff, MRA_ROM },
	{ 0x02002e, 0x020049, fp_io_service_dummy_r },
	{ 0x601002, 0x601003, io_player1_r },
	{ 0x601004, 0x601005, io_player2_r },
	{ 0x601000, 0x601001, io_service_r },
	{ 0x600000, 0x600001, io_dip2_r },
	{ 0x600002, 0x600003, io_dip1_r },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x44302a, 0x44304d, fp_io_service_dummy_r },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xfe003e, 0xfe003f, fp_io_service_dummy_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress fpoint_writemem[] =
{
	{ 0x000000, 0x01ffff, MWA_ROM },
	{ 0x600006, 0x600007, sound_command_w },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void fpoint_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );
}

static void fpoint_init_machine( void ){
	static int bank[16] = {00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00};

	sys16_obj_bank = bank;

	patch_code( 0x454, 0x33 );
	patch_code( 0x455, 0xf8 );
	patch_code( 0x456, 0xe0 );
	patch_code( 0x457, 0xe2 );
	patch_code( 0x8ce8, 0x16 );
	patch_code( 0x8ce9, 0x66 );
	patch_code( 0x17687, 0x00 );
	patch_code( 0x7bed, 0x04 );

	patch_code( 0x7ea8, 0x61 );
	patch_code( 0x7ea9, 0x00 );
	patch_code( 0x7eaa, 0x84 );
	patch_code( 0x7eab, 0x16 );
	patch_code( 0x2c0, 0xe7 );
	patch_code( 0x2c1, 0x48 );
	patch_code( 0x2c2, 0xe7 );
	patch_code( 0x2c3, 0x49 );
	patch_code( 0x2c4, 0x04 );
	patch_code( 0x2c5, 0x40 );
	patch_code( 0x2c6, 0x00 );
	patch_code( 0x2c7, 0x10 );
	patch_code( 0x2c8, 0x4e );
	patch_code( 0x2c9, 0x75 );

	sys16_update_proc = fpoint_update_proc;
}

static void init_fpoint(void)
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 1,0x020000 );
}

static void init_fpointbl(void)
{
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x30000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	sys16_sprite_decode( 1,0x020000 );
}
/***************************************************************************/

INPUT_PORTS_START( fpoint )
PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, "Clear round allowed" ) /* Use button 3 */
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER( machine_driver_fpoint, \
	fpoint_readmem,fpoint_writemem,fpoint_init_machine, gfx1)

/***************************************************************************/
// sys16B
ROM_START( goldnaxe )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr12523.a7", 0x00000, 0x20000, 0x8e6128d7 )
	ROM_LOAD_ODD ( "epr12522.a5", 0x00000, 0x20000, 0xb6c35160 )
	/* emtpy 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "epr12521.a8", 0x80000, 0x20000, 0x5001d713 )
	ROM_LOAD_ODD ( "epr12519.a6", 0x80000, 0x20000, 0x4438ca8e )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12385", 0x00000, 0x20000, 0xb8a4e7e0 )
	ROM_LOAD( "epr12386", 0x20000, 0x20000, 0x25d7d779 )
	ROM_LOAD( "epr12387", 0x40000, 0x20000, 0xc7fcadf3 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr12378.b1", 0x000000, 0x40000, 0x119e5a82 )
	ROM_LOAD( "mpr12379.b4", 0x040000, 0x40000, 0x1a0e8c57 )
	ROM_LOAD( "mpr12380.b2", 0x080000, 0x40000, 0xbb2c0853 )
	ROM_LOAD( "mpr12381.b5", 0x0c0000, 0x40000, 0x81ba6ecc )
	ROM_LOAD( "mpr12382.b3", 0x100000, 0x40000, 0x81601c6f )
	ROM_LOAD( "mpr12383.b6", 0x140000, 0x40000, 0x5dbacf7a )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12390",     0x00000, 0x08000, 0x399fc5f5 )
	ROM_LOAD( "mpr12384.a11", 0x10000, 0x20000, 0x6218d8e7 )
ROM_END

ROM_START( goldnaxj )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
// Custom cpu 317-0121
	ROM_LOAD_EVEN( "epr12540.a7", 0x00000, 0x20000, 0x0c7ccc6d )
	ROM_LOAD_ODD ( "epr12539.a5", 0x00000, 0x20000, 0x1f24f7d0 )
	/* emtpy 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "epr12521.a8", 0x80000, 0x20000, 0x5001d713 )
	ROM_LOAD_ODD ( "epr12519.a6", 0x80000, 0x20000, 0x4438ca8e )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12385", 0x00000, 0x20000, 0xb8a4e7e0 )
	ROM_LOAD( "epr12386", 0x20000, 0x20000, 0x25d7d779 )
	ROM_LOAD( "epr12387", 0x40000, 0x20000, 0xc7fcadf3 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr12378.b1", 0x000000, 0x40000, 0x119e5a82 )
	ROM_LOAD( "mpr12379.b4", 0x040000, 0x40000, 0x1a0e8c57 )
	ROM_LOAD( "mpr12380.b2", 0x080000, 0x40000, 0xbb2c0853 )
	ROM_LOAD( "mpr12381.b5", 0x0c0000, 0x40000, 0x81ba6ecc )
	ROM_LOAD( "mpr12382.b3", 0x100000, 0x40000, 0x81601c6f )
	ROM_LOAD( "mpr12383.b6", 0x140000, 0x40000, 0x5dbacf7a )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12390",     0x00000, 0x08000, 0x399fc5f5 )
	ROM_LOAD( "mpr12384.a11", 0x10000, 0x20000, 0x6218d8e7 )
ROM_END


ROM_START( goldnabl )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
// protected code
	ROM_LOAD_EVEN( "ga6.a22", 0x00000, 0x10000, 0xf95b459f )
	ROM_LOAD_ODD ( "ga4.a20", 0x00000, 0x10000, 0x83eabdf5 )
	ROM_LOAD_EVEN( "ga11.a27",0x20000, 0x10000, 0xf4ef9349 )
	ROM_LOAD_ODD ( "ga8.a24", 0x20000, 0x10000, 0x37a65839 )
	/* emtpy 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "epr12521.a8", 0x80000, 0x20000, 0x5001d713 )
	ROM_LOAD_ODD ( "epr12519.a6", 0x80000, 0x20000, 0x4438ca8e )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ga33.b16", 0x00000, 0x10000, 0x84587263 )
	ROM_LOAD( "ga32.b15", 0x10000, 0x10000, 0x63d72388 )
	ROM_LOAD( "ga31.b14", 0x20000, 0x10000, 0xf8b6ae4f )
	ROM_LOAD( "ga30.b13", 0x30000, 0x10000, 0xe29baf4f )
	ROM_LOAD( "ga29.b12", 0x40000, 0x10000, 0x22f0667e )
	ROM_LOAD( "ga28.b11", 0x50000, 0x10000, 0xafb1a7e4 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ga34.b17", 0x000000, 0x10000, 0x28ba70c8 )
	ROM_LOAD( "ga35.b18", 0x010000, 0x10000, 0x2ed96a26 )
	ROM_LOAD( "ga23.a14", 0x020000, 0x10000, 0x84dccc5b )
	ROM_LOAD( "ga18.a9",  0x030000, 0x10000, 0xde346006 )
	ROM_LOAD( "mpr12379.b4", 0x040000, 0x40000, 0x1a0e8c57 )
	ROM_LOAD( "ga36.b19", 0x080000, 0x10000, 0x101d2fff )
	ROM_LOAD( "ga37.b20", 0x090000, 0x10000, 0x677e64a6 )
	ROM_LOAD( "ga19.a10", 0x0a0000, 0x10000, 0x11794d05 )
	ROM_LOAD( "ga20.a11", 0x0b0000, 0x10000, 0xad1c1c90 )
	ROM_LOAD( "mpr12381.b5", 0x0c0000, 0x40000, 0x81ba6ecc )
	ROM_LOAD( "mpr12382.b3", 0x100000, 0x40000, 0x81601c6f )
	ROM_LOAD( "mpr12383.b6", 0x140000, 0x40000, 0x5dbacf7a )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12390",     0x00000, 0x08000, 0x399fc5f5 )
	ROM_LOAD( "mpr12384.a11", 0x10000, 0x20000, 0x6218d8e7 )
ROM_END


/***************************************************************************/

static READ_HANDLER( goldnaxe_skip_r )
{
	if (cpu_get_pc()==0x3cb0) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x2c1c]);
}

static READ_HANDLER( ga_io_players_r ) {return (io_player1_r(offset) << 8) | io_player2_r(offset);}
static READ_HANDLER( ga_io_service_r )
{
	return (io_service_r(offset) << 8) | (READ_WORD(&sys16_workingram[0x2c96]) & 0x00ff);
}

static struct MemoryReadAddress goldnaxe_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },

	{ 0x100000, 0x10ffff, MRA_TILERAM },
	{ 0x110000, 0x110fff, MRA_TEXTRAM },
	{ 0x140000, 0x140fff, MRA_PALETTERAM },
	{ 0x1f0000, 0x1f0003, MRA_EXTRAM },
	{ 0x200000, 0x200fff, MRA_SPRITERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc40fff, MRA_EXTRAM2 },
	{ 0xffecd0, 0xffecd1, ga_io_players_r },
	{ 0xffec96, 0xffec97, ga_io_service_r },
	{ 0xffec1c, 0xffec1d, goldnaxe_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static WRITE_HANDLER( ga_sound_command_w )
{
	if( (data&0xff000000)==0 )
		sound_command_w(offset,data>>8);
	COMBINE_WORD_MEM(&sys16_workingram[0x2cfc],data);
}

static struct MemoryWriteAddress goldnaxe_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x100000, 0x10ffff, MWA_TILERAM },
	{ 0x110000, 0x110fff, MWA_TEXTRAM },
	{ 0x140000, 0x140fff, MWA_PALETTERAM },
	{ 0x1f0000, 0x1f0003, MWA_EXTRAM },
	{ 0x200000, 0x200fff, MWA_SPRITERAM },
	{ 0xc40000, 0xc40fff, MWA_EXTRAM2 },
	{ 0xc43000, 0xc43001, MWA_NOP },
	{ 0xffecfc, 0xffecfd, ga_sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void goldnaxe_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );
	set_tile_bank( READ_WORD( &sys16_workingram[0x2c94] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void goldnaxe_init_machine( void ){
	static int bank[16] = { 0,2,8,10,16,18,0,0,4,6,12,14,20,22,0,0 };

	sys16_obj_bank = bank;

	patch_code( 0x3CB2, 0x60 );
	patch_code( 0x3CB3, 0x1e );

	sys16_sprxoffset = -0xb8;
	sys16_update_proc = goldnaxe_update_proc;
}

static void init_goldnaxe( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 3,0x80000 );
}

static void init_goldnabl( void )
{
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x60000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;
	sys16_sprite_decode( 3,0x80000 );
}

/***************************************************************************/

INPUT_PORTS_START( goldnaxe )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Credits needed" )
	PORT_DIPSETTING(    0x01, "1 to start, 1 to continue")
	PORT_DIPSETTING(    0x00, "2 to start, 1 to continue")
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, "Energy Meter" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_goldnaxe, \
	goldnaxe_readmem,goldnaxe_writemem,goldnaxe_init_machine, gfx2,upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( goldnaxa )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr12545.a2", 0x00000, 0x40000, 0xa97c4e4d )
	ROM_LOAD_ODD ( "epr12544.a1", 0x00000, 0x40000, 0x5e38f668 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12385", 0x00000, 0x20000, 0xb8a4e7e0 )
	ROM_LOAD( "epr12386", 0x20000, 0x20000, 0x25d7d779 )
	ROM_LOAD( "epr12387", 0x40000, 0x20000, 0xc7fcadf3 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr12378.b1", 0x000000, 0x40000, 0x119e5a82 )
	ROM_LOAD( "mpr12379.b4", 0x040000, 0x40000, 0x1a0e8c57 )
	ROM_LOAD( "mpr12380.b2", 0x080000, 0x40000, 0xbb2c0853 )
	ROM_LOAD( "mpr12381.b5", 0x0c0000, 0x40000, 0x81ba6ecc )
	ROM_LOAD( "mpr12382.b3", 0x100000, 0x40000, 0x81601c6f )
	ROM_LOAD( "mpr12383.b6", 0x140000, 0x40000, 0x5dbacf7a )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12390",     0x00000, 0x08000, 0x399fc5f5 )
	ROM_LOAD( "mpr12384.a11", 0x10000, 0x20000, 0x6218d8e7 )
ROM_END

ROM_START( goldnaxb )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
// Custom 68000 ver 317-0110
	ROM_LOAD_EVEN( "epr12389.a2", 0x00000, 0x40000, 0x35d5fa77 )
	ROM_LOAD_ODD ( "epr12388.a1", 0x00000, 0x40000, 0x72952a93 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12385", 0x00000, 0x20000, 0xb8a4e7e0 )
	ROM_LOAD( "epr12386", 0x20000, 0x20000, 0x25d7d779 )
	ROM_LOAD( "epr12387", 0x40000, 0x20000, 0xc7fcadf3 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr12378.b1", 0x000000, 0x40000, 0x119e5a82 )
	ROM_LOAD( "mpr12379.b4", 0x040000, 0x40000, 0x1a0e8c57 )
	ROM_LOAD( "mpr12380.b2", 0x080000, 0x40000, 0xbb2c0853 )
	ROM_LOAD( "mpr12381.b5", 0x0c0000, 0x40000, 0x81ba6ecc )
	ROM_LOAD( "mpr12382.b3", 0x100000, 0x40000, 0x81601c6f )
	ROM_LOAD( "mpr12383.b6", 0x140000, 0x40000, 0x5dbacf7a )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12390",     0x00000, 0x08000, 0x399fc5f5 )
	ROM_LOAD( "mpr12384.a11", 0x10000, 0x20000, 0x6218d8e7 )
ROM_END

ROM_START( goldnaxc )
	ROM_REGION( 0x0c0000, REGION_CPU1 ) /* 68000 code */
// Custom 68000 ver 317-0122
	ROM_LOAD_EVEN( "epr12543.a2", 0x00000, 0x40000, 0xb0df9ca4 )
	ROM_LOAD_ODD ( "epr12542.a1", 0x00000, 0x40000, 0xb7994d3c )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12385", 0x00000, 0x20000, 0xb8a4e7e0 )
	ROM_LOAD( "epr12386", 0x20000, 0x20000, 0x25d7d779 )
	ROM_LOAD( "epr12387", 0x40000, 0x20000, 0xc7fcadf3 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr12378.b1", 0x000000, 0x40000, 0x119e5a82 )
	ROM_LOAD( "mpr12379.b4", 0x040000, 0x40000, 0x1a0e8c57 )
	ROM_LOAD( "mpr12380.b2", 0x080000, 0x40000, 0xbb2c0853 )
	ROM_LOAD( "mpr12381.b5", 0x0c0000, 0x40000, 0x81ba6ecc )
	ROM_LOAD( "mpr12382.b3", 0x100000, 0x40000, 0x81601c6f )
	ROM_LOAD( "mpr12383.b6", 0x140000, 0x40000, 0x5dbacf7a )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12390",     0x00000, 0x08000, 0x399fc5f5 )
	ROM_LOAD( "mpr12384.a11", 0x10000, 0x20000, 0x6218d8e7 )
ROM_END


/***************************************************************************/

static READ_HANDLER( goldnaxa_skip_r )
{
	if (cpu_get_pc()==0x3ca0) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x2c1c]);
}

// This version has somekind of hardware comparitor for collision detection,
// and a hardware multiplier.
static int ga_hardware_collision_data[5];
static WRITE_HANDLER( ga_hardware_collision_w )
{
	static int bit=1;
	ga_hardware_collision_data[offset/2]=data;
	if(offset==4)
	{
		if(ga_hardware_collision_data[2] <= ga_hardware_collision_data[0] &&
			ga_hardware_collision_data[2] >= ga_hardware_collision_data[1])
		{
			ga_hardware_collision_data[4] |=bit;
		}
		bit=bit<<1;
	}
	if(offset==8) bit=1;
}

static READ_HANDLER( ga_hardware_collision_r )
{
	return ga_hardware_collision_data[4];
}

static int ga_hardware_multiplier_data[4];
static WRITE_HANDLER( ga_hardware_multiplier_w )
{
	ga_hardware_multiplier_data[offset/2]=data;
}

static READ_HANDLER( ga_hardware_multiplier_r )
{
	if(offset==6)
		return ga_hardware_multiplier_data[0] * ga_hardware_multiplier_data[1];
	else
		return ga_hardware_multiplier_data[offset/2];
}

static struct MemoryReadAddress goldnaxa_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },

	{ 0x100000, 0x10ffff, MRA_TILERAM },
	{ 0x110000, 0x110fff, MRA_TEXTRAM },
	{ 0x140000, 0x140fff, MRA_PALETTERAM },
	{ 0x1e0008, 0x1e0009, ga_hardware_collision_r },
	{ 0x1f0000, 0x1f0007, ga_hardware_multiplier_r },
	{ 0x1f1008, 0x1f1009, ga_hardware_collision_r },
	{ 0x1f2000, 0x1f2003, MRA_EXTRAM },
	{ 0x200000, 0x200fff, MRA_SPRITERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc40fff, MRA_EXTRAM2 },
	{ 0xffecd0, 0xffecd1, ga_io_players_r },
	{ 0xffec96, 0xffec97, ga_io_service_r },
	{ 0xffec1c, 0xffec1d, goldnaxa_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress goldnaxa_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x10ffff, MWA_TILERAM },
	{ 0x110000, 0x110fff, MWA_TEXTRAM },
	{ 0x140000, 0x140fff, MWA_PALETTERAM },
	{ 0x1e0000, 0x1e0009, ga_hardware_collision_w },
	{ 0x1f0000, 0x1f0003, ga_hardware_multiplier_w },
	{ 0x1f1000, 0x1f1009, ga_hardware_collision_w },
	{ 0x1f2000, 0x1f2003, MWA_EXTRAM },
	{ 0x200000, 0x200fff, MWA_SPRITERAM },
	{ 0xc40000, 0xc40fff, MWA_EXTRAM2 },
	{ 0xffecfc, 0xffecfd, ga_sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void goldnaxa_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );
	set_tile_bank( READ_WORD( &sys16_workingram[0x2c94] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void goldnaxa_init_machine( void ){
	static int bank[16] = { 0,2,8,10,16,18,0,0,4,6,12,14,20,22,0,0 };

	sys16_obj_bank = bank;

	patch_code( 0x3CA2, 0x60 );
	patch_code( 0x3CA3, 0x1e );

	sys16_sprxoffset = -0xb8;
	sys16_update_proc = goldnaxa_update_proc;
}

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_goldnaxa, \
	goldnaxa_readmem,goldnaxa_writemem,goldnaxa_init_machine, gfx2,upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( hwchamp )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "rom0-e.bin", 0x000000, 0x20000, 0xe5abfed7 )
	ROM_LOAD_ODD ( "rom0-o.bin", 0x000000, 0x20000, 0x25180124 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "scr01.bin", 0x00000, 0x20000, 0xfc586a86 )
	ROM_LOAD( "scr11.bin", 0x20000, 0x20000, 0xaeaaa9d8 )
	ROM_LOAD( "scr02.bin", 0x40000, 0x20000, 0x7715a742 )
	ROM_LOAD( "scr12.bin", 0x60000, 0x20000, 0x63a82afa )
	ROM_LOAD( "scr03.bin", 0x80000, 0x20000, 0xf30cd5fd )
	ROM_LOAD( "scr13.bin", 0xA0000, 0x20000, 0x5b8494a8 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "obj0-o.bin", 0x000000, 0x020000, 0xfc098a13 )
	ROM_LOAD( "obj0-e.bin", 0x020000, 0x020000, 0x5db934a8 )
	ROM_LOAD( "obj1-o.bin", 0x040000, 0x020000, 0x1f27ee74 )
	ROM_LOAD( "obj1-e.bin", 0x060000, 0x020000, 0x8a6a5cf1 )
	ROM_LOAD( "obj2-o.bin", 0x080000, 0x020000, 0xc0b2ba82 )
	ROM_LOAD( "obj2-e.bin", 0x0a0000, 0x020000, 0xd6c7917b )
	ROM_LOAD( "obj3-o.bin", 0x0c0000, 0x020000, 0x52fa3a49 )
	ROM_LOAD( "obj3-e.bin", 0x0e0000, 0x020000, 0x57e8f9d2 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "s-prog.bin", 0x0000, 0x8000, 0x96a12d9d )

	ROM_LOAD( "speech0.bin", 0x10000, 0x20000, 0x4191c03d )
	ROM_LOAD( "speech1.bin", 0x30000, 0x20000, 0xa4d53f7b )
ROM_END

/***************************************************************************/

static int hwc_handles_shifts[3];

static WRITE_HANDLER( hwc_io_handles_w )
{
	hwc_handles_shifts[offset/2]=7;
}

static READ_HANDLER( hwc_io_handles_r )
{
	static int dodge_toggle=0;
	int data=0,ret;
	if(offset==0)
	{
		// monitor
		data=input_port_0_r( offset );
		if(input_port_1_r( offset ) & 4)
		{
			if(dodge_toggle)
				data=0x38;
			else
				data=0x60;
		}
		if(input_port_1_r( offset ) & 8)
		{
			if(dodge_toggle)
				data=0xc8;
			else
				data=0xa0;
		}
		if(input_port_1_r( offset ) & 0x10)
		{
			if(dodge_toggle)
				data=0xff;
			else
				data=0xe0;
		}
		if(input_port_1_r( offset ) & 0x20)
		{
			if(dodge_toggle)
				data=0x0;
			else
				data=0x20;
		}
		if(hwc_handles_shifts[offset/2]==0)
			dodge_toggle^=1;
	}
	else if(offset==2)
	{
		// left handle
		if(input_port_1_r( offset ) & 1)
			data=0xff;
	}
	else
	{
		// right handle
		if(input_port_1_r( offset ) & 2)
			data=0xff;
	}
	ret=data>>hwc_handles_shifts[offset/2];
	hwc_handles_shifts[offset/2]--;
	return ret;
}

static struct MemoryReadAddress hwchamp_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x3f0000, 0x3fffff, MRA_EXTRAM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc43020, 0xc43025, hwc_io_handles_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc43fff, MRA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress hwchamp_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x3f0000, 0x3fffff, MWA_EXTRAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc43020, 0xc43025, hwc_io_handles_w },
	{ 0xc40000, 0xc43fff, MWA_EXTRAM2 },
	{ 0xfe0006, 0xfe0007, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void hwchamp_update_proc( void ){
	int leds;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys16_tile_bank0 = READ_WORD( &sys16_extraram[0x0000] )&0xf;
	sys16_tile_bank1 = READ_WORD( &sys16_extraram[0x0002] )&0xf;

	set_refresh( READ_WORD( &sys16_extraram2[0] ) );

	leds=READ_WORD( &sys16_extraram2[0x3034] );
	if(leds & 0x20)
		osd_led_w(0,1);
	else
		osd_led_w(0,0);
	if(leds & 0x80)
		osd_led_w(1,1);
	else
		osd_led_w(1,0);
	if(leds & 0x40)
		osd_led_w(2,1);
	else
		osd_led_w(2,0);

}

static void hwchamp_init_machine( void ){
	static int bank[16] = {0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30};

	sys16_obj_bank = bank;
	sys16_spritelist_end=0xc000;

	sys16_update_proc = hwchamp_update_proc;
}

static void init_hwchamp( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x040000 );
}
/***************************************************************************/

INPUT_PORTS_START( hwchamp )

PORT_START	/* Monitor */
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE  , 70, 4, 0x0, 0xff )

PORT_START	/* Handles (Fake) */
	PORT_BITX(0x01, 0, IPT_BUTTON1, IP_NAME_DEFAULT, KEYCODE_F, IP_JOY_NONE ) // right hit
	PORT_BITX(0x02, 0, IPT_BUTTON2, IP_NAME_DEFAULT, KEYCODE_D, IP_JOY_NONE ) // left hit
	PORT_BITX(0x04, 0, IPT_BUTTON3, IP_NAME_DEFAULT, KEYCODE_B, IP_JOY_NONE ) // right dodge
	PORT_BITX(0x08, 0, IPT_BUTTON4, IP_NAME_DEFAULT, KEYCODE_Z, IP_JOY_NONE ) // left dodge
	PORT_BITX(0x10, 0, IPT_BUTTON5, IP_NAME_DEFAULT, KEYCODE_V, IP_JOY_NONE ) // right sway
	PORT_BITX(0x20, 0, IPT_BUTTON6, IP_NAME_DEFAULT, KEYCODE_X, IP_JOY_NONE ) // left swat
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )	// Not Used
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Start Level Select" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Continue Mode" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, "Time Adjust"  )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_hwchamp, \
	hwchamp_readmem,hwchamp_writemem,hwchamp_init_machine, gfx4 ,upd7759_interface)

/***************************************************************************/
// pre16
ROM_START( mjleague )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr-7404.09b", 0x000000, 0x8000, 0xec1655b5 )
	ROM_LOAD_ODD ( "epr-7401.06b", 0x000000, 0x8000, 0x2befa5e0 )
	ROM_LOAD_EVEN( "epr-7405.10b", 0x010000, 0x8000, 0x7a4f4e38 )
	ROM_LOAD_ODD ( "epr-7402.07b", 0x010000, 0x8000, 0xb7bef762 )
	ROM_LOAD_EVEN( "epra7406.11b", 0x020000, 0x8000, 0xbb743639 )
	ROM_LOAD_ODD ( "epra7403.08b", 0x020000, 0x8000, 0xd86250cf )	// Fails memory test. Bad rom?

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr-7051.09a", 0x00000, 0x08000, 0x10ca255a )
	ROM_RELOAD(               0x08000, 0x08000 )
	ROM_LOAD( "epr-7052.10a", 0x10000, 0x08000, 0x2550db0e )
	ROM_RELOAD(               0x18000, 0x08000 )
	ROM_LOAD( "epr-7053.11a", 0x20000, 0x08000, 0x5bfea038 )
	ROM_RELOAD(               0x28000, 0x08000 )

	ROM_REGION( 0x050000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr-7055.05a", 0x000000, 0x008000, 0x1fb860bd )
	ROM_LOAD( "epr-7059.02b", 0x008000, 0x008000, 0x3d14091d )
	ROM_LOAD( "epr-7056.06a", 0x010000, 0x008000, 0xb35dd968 )
	ROM_LOAD( "epr-7060.03b", 0x018000, 0x008000, 0x61bb3757 )
	ROM_LOAD( "epr-7057.07a", 0x020000, 0x008000, 0x3e5a2b6f )
	ROM_LOAD( "epr-7061.04b", 0x028000, 0x008000, 0xc808dad5 )
	ROM_LOAD( "epr-7058.08a", 0x030000, 0x008000, 0xb543675f )
	ROM_LOAD( "epr-7062.05b", 0x038000, 0x008000, 0x9168eb47 )
//	ROM_LOAD( "epr-7055.05a", 0x040000, 0x008000, 0x1fb860bd ) loaded twice??
//	ROM_LOAD( "epr-7059.02b", 0x048000, 0x008000, 0x3d14091d ) loaded twice??

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "eprc7054.01b", 0x00000, 0x8000, 0x4443b744 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr-7063.01a", 0x00000, 0x8000, 0x45d8908a )
	ROM_LOAD( "epr-7065.02a", 0x08000, 0x8000, 0x8c8f8cff )
	ROM_LOAD( "epr-7064.03a", 0x10000, 0x8000, 0x159f6636 )
	ROM_LOAD( "epr-7066.04a", 0x18000, 0x8000, 0xf5cfa91f )
ROM_END

/***************************************************************************/

static READ_HANDLER( mjl_io_player1_r )
{
	int data=input_port_0_r( offset ) & 0x80;

	if(READ_WORD( &sys16_extraram2[2] ) & 0x4)
		data|=(input_port_5_r( offset ) & 0x3f) << 1;
	else
		data|=(input_port_6_r( offset ) & 0x3f) << 1;

	return data;
}

static READ_HANDLER( mjl_io_service_r )
{
	int data=input_port_2_r( offset ) & 0x3f;

	if(READ_WORD( &sys16_extraram2[2] ) & 0x4)
	{
		data|=(input_port_5_r( offset ) & 0x40);
		data|=(input_port_7_r( offset ) & 0x40) << 1;
	}
	else
	{
		data|=(input_port_6_r( offset ) & 0x40);
		data|=(input_port_8_r( offset ) & 0x40) << 1;
	}

	return data;
}

static READ_HANDLER( mjl_io_player2_r )
{
	int data=input_port_1_r( offset ) & 0x80;
	if(READ_WORD( &sys16_extraram2[2] ) & 0x4)
		data|=(input_port_7_r( offset ) & 0x3f) << 1;
	else
		data|=(input_port_8_r( offset ) & 0x3f) << 1;
	return data;
}

static READ_HANDLER( mjl_io_bat_r )
{
	int data1=input_port_0_r( offset );
	int data2=input_port_1_r( offset );
	int ret=0;

	// Hitting has 8 values, but for easy of playing, I've only added 3

	if(data1 &1) ret=0x00;
	else if(data1 &2) ret=0x03;
	else if(data1 &4) ret=0x07;
	else ret=0x0f;

	if(data2 &1) ret|=0x00;
	else if(data2 &2) ret|=0x30;
	else if(data2 &4) ret|=0x70;
	else ret|=0xf0;

	return ret;

}

static struct MemoryReadAddress mjleague_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },

	{ 0xc40002, 0xc40007, MRA_EXTRAM2},
	{ 0xc41000, 0xc41001, mjl_io_service_r },
	{ 0xc41002, 0xc41003, mjl_io_player1_r },
	{ 0xc41006, 0xc41007, mjl_io_player2_r },
	{ 0xc41004, 0xc41005, mjl_io_bat_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc60000, 0xc60001, MRA_NOP }, /* What is this? Watchdog? */

	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress mjleague_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40002, 0xc40007, MWA_EXTRAM2},
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void mjleague_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	set_fg_page1( READ_WORD( &sys16_textram[0x0e8e] ) );
	set_bg_page1( READ_WORD( &sys16_textram[0x0e8c] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram2[0] ) );
}

static void mjleague_init_machine( void ){
	static int bank[16] = { 00,01,02,03,00,01,02,03,00,01,02,03,00,01,02,03};

	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 2;
	sys16_sprxoffset = -0xbd;
	sys16_fgxoffset = sys16_bgxoffset = 7;

	// remove memory test because it fails.
	patch_code( 0xBD42, 0x66 );

	sys16_update_proc = mjleague_update_proc;
}

static void init_mjleague( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 5,0x010000 );
}

/***************************************************************************/

INPUT_PORTS_START( mjleague )

PORT_START /* player 1 button fake */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 )

PORT_START /* player 1 button fake */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_PLAYER2 )

PORT_START  /* Service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, "Starting Points" )
	PORT_DIPSETTING(    0x0c, "2000" )
	PORT_DIPSETTING(    0x08, "3000" )
	PORT_DIPSETTING(    0x04, "5000" )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPNAME( 0x10, 0x10, "Team Select" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )	//??? something to do with cocktail mode?
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

PORT_START	/* IN5 */
	PORT_ANALOG( 0x7f, 0x40, IPT_TRACKBALL_Y, 70, 30, 0, 127 )

PORT_START	/* IN6 */
	PORT_ANALOG( 0x7f, 0x40, IPT_TRACKBALL_X /*| IPF_REVERSE*/, 50, 30, 0, 127 )

PORT_START	/* IN7 */
	PORT_ANALOG( 0x7f, 0x40, IPT_TRACKBALL_Y | IPF_PLAYER2, 70, 30, 0, 127 )

PORT_START	/* IN8 */
	PORT_ANALOG( 0x7f, 0x40, IPT_TRACKBALL_X | IPF_PLAYER2 | IPF_REVERSE, 50, 30, 0, 127 )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7751( machine_driver_mjleague, \
	mjleague_readmem,mjleague_writemem,mjleague_init_machine, gfx1)

/***************************************************************************/
// sys18
ROM_START( moonwalk )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0159
	ROM_LOAD_EVEN( "epr13235.a6", 0x000000, 0x40000, 0x6983e129 )
	ROM_LOAD_ODD ( "epr13234.a5", 0x000000, 0x40000, 0xc9fd20f2 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, 0x862d2c03 )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, 0x7d1ac3ec )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, 0x56d3393c )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13224.b11", 0x000000, 0x40000, 0xc59f107b )
	ROM_LOAD( "mpr13231.a11", 0x040000, 0x40000, 0xa5e96346 )
	ROM_LOAD( "mpr13223.b10", 0x080000, 0x40000, 0x364f60ff )
	ROM_LOAD( "mpr13230.a10", 0x0c0000, 0x40000, 0x9550091f )
	ROM_LOAD( "mpr13222.b9",  0x100000, 0x40000, 0x523df3ed )
	ROM_LOAD( "mpr13229.a9",  0x140000, 0x40000, 0xf40dc45d )
	ROM_LOAD( "epr13221.b8",  0x180000, 0x40000, 0x9ae7546a )
	ROM_LOAD( "epr13228.a8",  0x1c0000, 0x40000, 0xde3786be )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x10000, 0x20000, 0x56c2e82b )
	ROM_LOAD( "mpr13219.b4", 0x30000, 0x40000, 0x19e2061f )
	ROM_LOAD( "mpr13220.b5", 0x70000, 0x40000, 0x58d4d9ce )
	ROM_LOAD( "mpr13249.b6", 0xb0000, 0x40000, 0x623edc5d )
ROM_END

ROM_START( moonwlka )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0158
	ROM_LOAD_EVEN( "epr13233", 0x000000, 0x40000, 0xf3dac671 )
	ROM_LOAD_ODD ( "epr13232", 0x000000, 0x40000, 0x541d8bdf )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, 0x862d2c03 )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, 0x7d1ac3ec )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, 0x56d3393c )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13224.b11", 0x000000, 0x40000, 0xc59f107b )
	ROM_LOAD( "mpr13231.a11", 0x040000, 0x40000, 0xa5e96346 )
	ROM_LOAD( "mpr13223.b10", 0x080000, 0x40000, 0x364f60ff )
	ROM_LOAD( "mpr13230.a10", 0x0c0000, 0x40000, 0x9550091f )
	ROM_LOAD( "mpr13222.b9",  0x100000, 0x40000, 0x523df3ed )
	ROM_LOAD( "mpr13229.a9",  0x140000, 0x40000, 0xf40dc45d )
	ROM_LOAD( "epr13221.b8",  0x180000, 0x40000, 0x9ae7546a )
	ROM_LOAD( "epr13228.a8",  0x1c0000, 0x40000, 0xde3786be )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x10000, 0x20000, 0x56c2e82b )
	ROM_LOAD( "mpr13219.b4", 0x30000, 0x40000, 0x19e2061f )
	ROM_LOAD( "mpr13220.b5", 0x70000, 0x40000, 0x58d4d9ce )
	ROM_LOAD( "mpr13249.b6", 0xb0000, 0x40000, 0x623edc5d )
ROM_END

// sys18
ROM_START( moonwlkb )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "moonwlkb.01", 0x000000, 0x10000, 0xf49cdb16 )
	ROM_LOAD_ODD ( "moonwlkb.05", 0x000000, 0x10000, 0xc483f29f )
	ROM_LOAD_EVEN( "moonwlkb.02", 0x020000, 0x10000, 0x0bde1896 )
	ROM_LOAD_ODD ( "moonwlkb.06", 0x020000, 0x10000, 0x5b9fc688 )
	ROM_LOAD_EVEN( "moonwlkb.03", 0x040000, 0x10000, 0x0c5fe15c )
	ROM_LOAD_ODD ( "moonwlkb.07", 0x040000, 0x10000, 0x9e600704 )
	ROM_LOAD_EVEN( "moonwlkb.04", 0x060000, 0x10000, 0x64692f79 )
	ROM_LOAD_ODD ( "moonwlkb.08", 0x060000, 0x10000, 0x546ca530 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, 0x862d2c03 )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, 0x7d1ac3ec )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, 0x56d3393c )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13224.b11", 0x000000, 0x40000, 0xc59f107b )
	ROM_LOAD( "mpr13231.a11", 0x040000, 0x40000, 0xa5e96346 )
	ROM_LOAD( "mpr13223.b10", 0x080000, 0x40000, 0x364f60ff )
	ROM_LOAD( "mpr13230.a10", 0x0c0000, 0x40000, 0x9550091f )
	ROM_LOAD( "mpr13222.b9",  0x100000, 0x40000, 0x523df3ed )
	ROM_LOAD( "mpr13229.a9",  0x140000, 0x40000, 0xf40dc45d )
	ROM_LOAD( "epr13221.b8",  0x180000, 0x40000, 0x9ae7546a )
	ROM_LOAD( "epr13228.a8",  0x1c0000, 0x40000, 0xde3786be )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x10000, 0x20000, 0x56c2e82b )
	ROM_LOAD( "mpr13219.b4", 0x30000, 0x40000, 0x19e2061f )
	ROM_LOAD( "mpr13220.b5", 0x70000, 0x40000, 0x58d4d9ce )
	ROM_LOAD( "mpr13249.b6", 0xb0000, 0x40000, 0x623edc5d )
ROM_END

/***************************************************************************/

static READ_HANDLER( moonwlkb_skip_r )
{
	if (cpu_get_pc()==0x308a) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x202c]);
}

static struct MemoryReadAddress moonwalk_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },

	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc00000, 0xc0ffff, MRA_EXTRAM },
	{ 0xc40000, 0xc40001, io_dip1_r },
	{ 0xc40002, 0xc40003, io_dip2_r },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41004, 0xc41005, io_player2_r },
	{ 0xc41006, 0xc41007, io_player3_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc40000, 0xc4ffff, MRA_EXTRAM3 },
	{ 0xe40000, 0xe4ffff, MRA_EXTRAM2 },
	{ 0xfe0000, 0xfeffff, MRA_EXTRAM4 },
	{ 0xffe02c, 0xffe02d, moonwlkb_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress moonwalk_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc00000, 0xc0ffff, MWA_EXTRAM },
	{ 0xc40006, 0xc40007, sound_command_nmi_w },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM3 },
	{ 0xe40000, 0xe4ffff, MWA_EXTRAM2 },
	{ 0xfe0000, 0xfeffff, MWA_EXTRAM4 },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void moonwalk_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys16_fg2_scrollx = READ_WORD( &sys16_textram[0x0e9c] );
	sys16_bg2_scrollx = READ_WORD( &sys16_textram[0x0e9e] );
	sys16_fg2_scrolly = READ_WORD( &sys16_textram[0x0e94] );
	sys16_bg2_scrolly = READ_WORD( &sys16_textram[0x0e96] );

	set_fg2_page( READ_WORD( &sys16_textram[0x0e84] ) );
	set_bg2_page( READ_WORD( &sys16_textram[0x0e86] ) );

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | READ_WORD( &sys16_textram[0x0e84] ))
		sys18_fg2_active=1;
	else
		sys18_fg2_active=0;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | READ_WORD( &sys16_textram[0x0e86] ))
		sys18_bg2_active=1;
	else
		sys18_bg2_active=0;

	set_tile_bank18( READ_WORD( &sys16_extraram3[0x6800] ) );
	set_refresh_18( READ_WORD( &sys16_extraram3[0x6600] ) ); // 0xc46601
}

static void moonwalk_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};
	sys16_obj_bank = bank;
	sys16_bg_priority_value=0x1000;
	sys16_sprxoffset = -0x238;
	sys16_spritelist_end=0x8000;

	patch_code( 0x70116, 0x4e);
	patch_code( 0x70117, 0x71);

	patch_code( 0x314a, 0x46);
	patch_code( 0x314b, 0x42);

	patch_code( 0x311b, 0x3f);

	patch_code( 0x70103, 0x00);
	patch_code( 0x70109, 0x00);
	patch_code( 0x07727, 0x00);
	patch_code( 0x07729, 0x00);
	patch_code( 0x0780d, 0x00);
	patch_code( 0x0780f, 0x00);
	patch_code( 0x07861, 0x00);
	patch_code( 0x07863, 0x00);
	patch_code( 0x07d47, 0x00);
	patch_code( 0x07863, 0x00);
	patch_code( 0x08533, 0x00);
	patch_code( 0x08535, 0x00);
	patch_code( 0x085bd, 0x00);
	patch_code( 0x085bf, 0x00);
	patch_code( 0x09a4b, 0x00);
	patch_code( 0x09a4d, 0x00);
	patch_code( 0x09b2f, 0x00);
	patch_code( 0x09b31, 0x00);
	patch_code( 0x0a05b, 0x00);
	patch_code( 0x0a05d, 0x00);
	patch_code( 0x0a23f, 0x00);
	patch_code( 0x0a241, 0x00);
	patch_code( 0x10159, 0x00);
	patch_code( 0x1015b, 0x00);
	patch_code( 0x109fb, 0x00);
	patch_code( 0x109fd, 0x00);

	// * SEGA mark
	patch_code( 0x70212, 0x4e);
	patch_code( 0x70213, 0x71);

	sys16_update_proc = moonwalk_update_proc;
}

static void init_moonwalk( void ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	sys16_onetime_init_machine();
	sys18_splittab_fg_x=&sys16_textram[0x0f80];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0];

	memcpy(RAM,&RAM[0x10000],0xa000);

	sys16_sprite_decode( 4,0x080000 );
}

/***************************************************************************/

INPUT_PORTS_START( moonwalk )

PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

PORT_START /* service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_TILT, "Test", KEYCODE_T, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x08, "Player Vitality" )
	PORT_DIPSETTING(    0x08, "Low" )
	PORT_DIPSETTING(    0x00, "High" )
	PORT_DIPNAME( 0x10, 0x00, "Play Mode" )
	PORT_DIPSETTING(    0x10, "2 Players" )
	PORT_DIPSETTING(    0x00, "3 Players" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Mode" )
	PORT_DIPSETTING(    0x20, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

PORT_START /* player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
//	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_18( machine_driver_moonwalk, \
	moonwalk_readmem,moonwalk_writemem,moonwalk_init_machine, gfx4 )

/***************************************************************************/
// sys16B
ROM_START( passsht )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr11871.a4", 0x000000, 0x10000, 0x0f9ccea5 )
	ROM_LOAD_ODD ( "epr11870.a1", 0x000000, 0x10000, 0xdf43ebcf )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr11854.b9",  0x00000, 0x10000, 0xd31c0b6c )
	ROM_LOAD( "opr11855.b10", 0x10000, 0x10000, 0xb78762b4 )
	ROM_LOAD( "opr11856.b11", 0x20000, 0x10000, 0xea49f666 )

	ROM_REGION( 0x60000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "opr11862.b1",  0x00000, 0x10000, 0xb6e94727 )
	ROM_LOAD( "opr11865.b5",  0x10000, 0x10000, 0x17e8d5d5 )
	ROM_LOAD( "opr11863.b2",  0x20000, 0x10000, 0x3e670098 )
	ROM_LOAD( "opr11866.b6",  0x30000, 0x10000, 0x50eb71cc )
	ROM_LOAD( "opr11864.b3",  0x40000, 0x10000, 0x05733ca8 )
	ROM_LOAD( "opr11867.b7",  0x50000, 0x10000, 0x81e49697 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr11857.a7",  0x00000, 0x08000, 0x789edc06 )
	ROM_LOAD( "epr11858.a8",  0x10000, 0x08000, 0x08ab0018 )
	ROM_LOAD( "epr11859.a9",  0x18000, 0x08000, 0x8673e01b )
	ROM_LOAD( "epr11860.a10", 0x20000, 0x08000, 0x10263746 )
	ROM_LOAD( "epr11861.a11", 0x28000, 0x08000, 0x38b54a71 )
ROM_END

ROM_START( passht4b )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "pas4p.3", 0x000000, 0x10000, 0x2d8bc946 )
	ROM_LOAD_ODD ( "pas4p.4", 0x000000, 0x10000, 0xe759e831 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "pas4p.11",  0x00000, 0x10000, 0xda20fbc9 )
	ROM_LOAD( "pas4p.12", 0x10000, 0x10000, 0xbebb9211 )
	ROM_LOAD( "pas4p.13", 0x20000, 0x10000, 0xe37506c3 )

	ROM_REGION( 0x60000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "opr11862.b1",  0x00000, 0x10000, 0xb6e94727 )
	ROM_LOAD( "opr11865.b5",  0x10000, 0x10000, 0x17e8d5d5 )
	ROM_LOAD( "opr11863.b2",  0x20000, 0x10000, 0x3e670098 )
	ROM_LOAD( "opr11866.b6",  0x30000, 0x10000, 0x50eb71cc )
	ROM_LOAD( "opr11864.b3",  0x40000, 0x10000, 0x05733ca8 )
	ROM_LOAD( "opr11867.b7",  0x50000, 0x10000, 0x81e49697 )

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "pas4p.1",  0x00000, 0x08000, 0xe60fb017 )
	ROM_LOAD( "pas4p.2",  0x10000, 0x10000, 0x092e016e )
ROM_END

ROM_START( passshtb )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "pass3_2p.bin", 0x000000, 0x10000, 0x26bb9299 )
	ROM_LOAD_ODD ( "pass4_2p.bin", 0x000000, 0x10000, 0x06ac6d5d )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr11854.b9",  0x00000, 0x10000, 0xd31c0b6c )
	ROM_LOAD( "opr11855.b10", 0x10000, 0x10000, 0xb78762b4 )
	ROM_LOAD( "opr11856.b11", 0x20000, 0x10000, 0xea49f666 )

	ROM_REGION( 0x60000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "opr11862.b1",  0x00000, 0x10000, 0xb6e94727 )
	ROM_LOAD( "opr11865.b5",  0x10000, 0x10000, 0x17e8d5d5 )
	ROM_LOAD( "opr11863.b2",  0x20000, 0x10000, 0x3e670098 )
	ROM_LOAD( "opr11866.b6",  0x30000, 0x10000, 0x50eb71cc )
	ROM_LOAD( "opr11864.b3",  0x40000, 0x10000, 0x05733ca8 )
	ROM_LOAD( "opr11867.b7",  0x50000, 0x10000, 0x81e49697 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr11857.a7",  0x00000, 0x08000, 0x789edc06 )
	ROM_LOAD( "epr11858.a8",  0x10000, 0x08000, 0x08ab0018 )
	ROM_LOAD( "epr11859.a9",  0x18000, 0x08000, 0x8673e01b )
	ROM_LOAD( "epr11860.a10", 0x20000, 0x08000, 0x10263746 )
	ROM_LOAD( "epr11861.a11", 0x28000, 0x08000, 0x38b54a71 )
ROM_END
/***************************************************************************/

static struct MemoryReadAddress passsht_readmem[] =
{
	{ 0x000000, 0x01ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41004, 0xc41005, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc40fff, MRA_EXTRAM },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress passsht_writemem[] =
{
	{ 0x000000, 0x01ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc42006, 0xc42007, sound_command_w },
	{ 0xc40000, 0xc40fff, MWA_EXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

static int passht4b_io1_val;
static int passht4b_io2_val;
static int passht4b_io3_val;

static READ_HANDLER( passht4b_service_r )
{
	int val=io_service_r(offset);

	if(!(io_player1_r(offset) & 0x40)) val&=0xef;
	if(!(io_player2_r(offset) & 0x40)) val&=0xdf;
	if(!(io_player3_r(offset) & 0x40)) val&=0xbf;
	if(!(io_player4_r(offset) & 0x40)) val&=0x7f;

	passht4b_io3_val=(io_player1_r(offset)<<4) | (io_player3_r(offset)&0xf);
	passht4b_io2_val=(io_player2_r(offset)<<4) | (io_player4_r(offset)&0xf);

	passht4b_io1_val=0xff;

	// player 1 buttons
	if(!(io_player1_r(offset) & 0x10)) passht4b_io1_val &=0xfe;
	if(!(io_player1_r(offset) & 0x20)) passht4b_io1_val &=0xfd;
	if(!(io_player1_r(offset) & 0x80)) passht4b_io1_val &=0xfc;

	// player 2 buttons
	if(!(io_player2_r(offset) & 0x10)) passht4b_io1_val &=0xfb;
	if(!(io_player2_r(offset) & 0x20)) passht4b_io1_val &=0xf7;
	if(!(io_player2_r(offset) & 0x80)) passht4b_io1_val &=0xf3;

	// player 3 buttons
	if(!(io_player3_r(offset) & 0x10)) passht4b_io1_val &=0xef;
	if(!(io_player3_r(offset) & 0x20)) passht4b_io1_val &=0xdf;
	if(!(io_player3_r(offset) & 0x80)) passht4b_io1_val &=0xcf;

	// player 4 buttons
	if(!(io_player4_r(offset) & 0x10)) passht4b_io1_val &=0xbf;
	if(!(io_player4_r(offset) & 0x20)) passht4b_io1_val &=0x7f;
	if(!(io_player4_r(offset) & 0x80)) passht4b_io1_val &=0x3f;

	return val;
}

static READ_HANDLER( passht4b_io1_r ) {	return passht4b_io1_val;}
static READ_HANDLER( passht4b_io2_r ) {	return passht4b_io2_val;}
static READ_HANDLER( passht4b_io3_r ) {	return passht4b_io3_val;}

static struct MemoryReadAddress passht4b_readmem[] =
{
	{ 0x000000, 0x01ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41000, 0xc41001, passht4b_service_r },

	{ 0xc41002, 0xc41003, passht4b_io1_r },
	{ 0xc41004, 0xc41005, passht4b_io2_r },
	{ 0xc41006, 0xc41007, passht4b_io3_r },

	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc43000, 0xc43001, io_player1_r },		// test mode only
	{ 0xc43002, 0xc43003, io_player2_r },
	{ 0xc43004, 0xc43005, io_player3_r },
	{ 0xc43006, 0xc43007, io_player4_r },
	{ 0xc4600a, 0xc4600b, MRA_EXTRAM },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress passht4b_writemem[] =
{
	{ 0x000000, 0x01ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc42006, 0xc42007, sound_command_w },
	{ 0xc4600a, 0xc4600b, MWA_EXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void passsht_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_workingram[0x34be] );
	sys16_bg_scrollx = READ_WORD( &sys16_workingram[0x34c2] );
	sys16_fg_scrolly = READ_WORD( &sys16_workingram[0x34bc] );
	sys16_bg_scrolly = READ_WORD( &sys16_workingram[0x34c0] );

	set_fg_page( READ_WORD( &sys16_textram[0x0ff6] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0ff4] ) );
	set_refresh( READ_WORD( &sys16_extraram[0] ) );
}

static void passht4b_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_workingram[0x34ce] );
	sys16_bg_scrollx = READ_WORD( &sys16_workingram[0x34d2] );
	sys16_fg_scrolly = READ_WORD( &sys16_workingram[0x34cc] );
	sys16_bg_scrolly = READ_WORD( &sys16_workingram[0x34d0] );

	set_fg_page( READ_WORD( &sys16_textram[0x0ff6] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0ff4] ) );
	set_refresh( READ_WORD( &sys16_extraram[0] ) );

}

static void passsht_init_machine( void ){
	static int bank[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,3 };
	sys16_obj_bank = bank;

	sys16_sprxoffset = -0x48;
	sys16_spritesystem = 0;

	// fix name entry
	patch_code( 0x13a8,0xc0);

	sys16_update_proc = passsht_update_proc;
}

static void passht4b_init_machine( void ){
	static int bank[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,3 };
	sys16_obj_bank = bank;

	sys16_sprxoffset = -0xb8;
	sys16_spritesystem = 8;

	// fix name entry
	patch_code( 0x138a,0xc0);

	sys16_update_proc = passht4b_update_proc;
}

static void init_passsht( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 3,0x20000 );
}

static void init_passht4b( void ){
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x30000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	sys16_sprite_decode( 3,0x20000 );
}

/***************************************************************************/

INPUT_PORTS_START( passsht )
PORT_START /* joy 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

PORT_START /* joy 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0e, 0x0e, "Initial Point" )
	PORT_DIPSETTING(    0x06, "2000" )
	PORT_DIPSETTING(    0x0a, "3000" )
	PORT_DIPSETTING(    0x0c, "4000" )
	PORT_DIPSETTING(    0x0e, "5000" )
	PORT_DIPSETTING(    0x08, "6000" )
	PORT_DIPSETTING(    0x04, "7000" )
	PORT_DIPSETTING(    0x02, "8000" )
	PORT_DIPSETTING(    0x00, "9000" )
	PORT_DIPNAME( 0x30, 0x30, "Point Table" )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( passht4b )
PORT_START /* joy 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 )

PORT_START /* joy 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )

PORT_START /* service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0e, 0x0e, "Initial Point" )
	PORT_DIPSETTING(    0x06, "2000" )
	PORT_DIPSETTING(    0x0a, "3000" )
	PORT_DIPSETTING(    0x0c, "4000" )
	PORT_DIPSETTING(    0x0e, "5000" )
	PORT_DIPSETTING(    0x08, "6000" )
	PORT_DIPSETTING(    0x04, "7000" )
	PORT_DIPSETTING(    0x02, "8000" )
	PORT_DIPSETTING(    0x00, "9000" )
	PORT_DIPNAME( 0x30, 0x30, "Point Table" )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

PORT_START /* joy 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )

PORT_START /* joy 4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER4 )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_passsht, \
	passsht_readmem,passsht_writemem,passsht_init_machine, gfx1 ,upd7759_interface)

MACHINE_DRIVER_7759( machine_driver_passht4b, \
	passht4b_readmem,passht4b_writemem,passht4b_init_machine, gfx1 ,upd7759_interface)

/***************************************************************************/
// pre16
ROM_START( quartet )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr7458a.9b",  0x000000, 0x8000, 0x42e7b23e )
	ROM_LOAD_ODD ( "epr7455a.6b",  0x000000, 0x8000, 0x01631ab2 )
	ROM_LOAD_EVEN( "epr7459a.10b", 0x010000, 0x8000, 0x6b540637 )
	ROM_LOAD_ODD ( "epr7456a.7b",  0x010000, 0x8000, 0x31ca583e )
	ROM_LOAD_EVEN( "epr7460.11b",  0x020000, 0x8000, 0xa444ea13 )
	ROM_LOAD_ODD ( "epr7457.8b",   0x020000, 0x8000, 0x3b282c23 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr7461.9c",  0x00000, 0x08000, 0xf6af07f2 )
	ROM_LOAD( "epr7462.10c", 0x08000, 0x08000, 0x7914af28 )
	ROM_LOAD( "epr7463.11c", 0x10000, 0x08000, 0x827c5603 )

	ROM_REGION( 0x050000*2, REGION_GFX2 ) /* sprites  - the same as quartet 2 */
	ROM_LOAD( "epr7465.5c",  0x000000, 0x008000, 0x8a1ab7d7 )
	ROM_RELOAD(              0x040000, 0x008000 ) //twice? - fixes a sprite glitch
	ROM_LOAD( "epr-7469.2b", 0x008000, 0x008000, 0xcb65ae4f )
	ROM_RELOAD(              0x048000, 0x008000 ) //twice?
	ROM_LOAD( "epr7466.6c",  0x010000, 0x008000, 0xb2d3f4f3 )
	ROM_LOAD( "epr-7470.3b", 0x018000, 0x008000, 0x16fc67b1 )
	ROM_LOAD( "epr7467.7c",  0x020000, 0x008000, 0x0af68de2 )
	ROM_LOAD( "epr-7471.4b", 0x028000, 0x008000, 0x13fad5ac )
	ROM_LOAD( "epr7468.8c",  0x030000, 0x008000, 0xddfd40c0 )
	ROM_LOAD( "epr-7472.5b", 0x038000, 0x008000, 0x8e2762ec )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr-7464.1b", 0x0000, 0x8000, 0x9f291306 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr7473.1c", 0x00000, 0x8000, 0x06ec75fa )
	ROM_LOAD( "epr7475.2c", 0x08000, 0x8000, 0x7abd1206 )
	ROM_LOAD( "epr7474.3c", 0x10000, 0x8000, 0xdbf853b8 )
	ROM_LOAD( "epr7476.4c", 0x18000, 0x8000, 0x5eba655a )
ROM_END

ROM_START( quartetj )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr-7458.43",  0x000000, 0x8000, 0x0096499f )
	ROM_LOAD_ODD ( "epr-7455.26",  0x000000, 0x8000, 0xda934390 )
	ROM_LOAD_EVEN( "epr-7459.42",  0x010000, 0x8000, 0xd130cf61 )
	ROM_LOAD_ODD ( "epr-7456.25",  0x010000, 0x8000, 0x7847149f )
	ROM_LOAD_EVEN( "epr7460.11b",  0x020000, 0x8000, 0xa444ea13 )
	ROM_LOAD_ODD ( "epr7457.8b",   0x020000, 0x8000, 0x3b282c23 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr7461.9c",  0x00000, 0x08000, 0xf6af07f2 )
	ROM_LOAD( "epr7462.10c", 0x08000, 0x08000, 0x7914af28 )
	ROM_LOAD( "epr7463.11c", 0x10000, 0x08000, 0x827c5603 )

	ROM_REGION( 0x050000*2, REGION_GFX2 ) /* sprites  - the same as quartet 2 */
	ROM_LOAD( "epr7465.5c",  0x000000, 0x008000, 0x8a1ab7d7 )
	ROM_RELOAD(              0x040000, 0x008000 ) //twice? - fixes a sprite glitch
	ROM_LOAD( "epr-7469.2b", 0x008000, 0x008000, 0xcb65ae4f )
	ROM_RELOAD(              0x048000, 0x008000 ) //twice?
	ROM_LOAD( "epr7466.6c",  0x010000, 0x008000, 0xb2d3f4f3 )
	ROM_LOAD( "epr-7470.3b", 0x018000, 0x008000, 0x16fc67b1 )
	ROM_LOAD( "epr7467.7c",  0x020000, 0x008000, 0x0af68de2 )
	ROM_LOAD( "epr-7471.4b", 0x028000, 0x008000, 0x13fad5ac )
	ROM_LOAD( "epr7468.8c",  0x030000, 0x008000, 0xddfd40c0 )
	ROM_LOAD( "epr-7472.5b", 0x038000, 0x008000, 0x8e2762ec )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr-7464.1b", 0x0000, 0x8000, 0x9f291306 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr7473.1c", 0x00000, 0x8000, 0x06ec75fa )
	ROM_LOAD( "epr7475.2c", 0x08000, 0x8000, 0x7abd1206 )
	ROM_LOAD( "epr7474.3c", 0x10000, 0x8000, 0xdbf853b8 )
	ROM_LOAD( "epr7476.4c", 0x18000, 0x8000, 0x5eba655a )
ROM_END


/***************************************************************************/

static READ_HANDLER( quartet_skip_r )
{
	if (cpu_get_pc()==0x89b2) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0800]);
}

static READ_HANDLER( io_quartet_p1_r ) {return input_port_0_r( offset );}
static READ_HANDLER( io_quartet_p2_r ) {return input_port_1_r( offset );}
static READ_HANDLER( io_quartet_p3_r ) {return input_port_2_r( offset );}
static READ_HANDLER( io_quartet_p4_r ) {return input_port_3_r( offset );}
static READ_HANDLER( io_quartet_dip1_r ) {return input_port_4_r( offset );}
static READ_HANDLER( io_quartet_dip2_r ) {return input_port_5_r( offset );}

static struct MemoryReadAddress quartet_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41000, 0xc41001, io_quartet_p1_r },
	{ 0xc41002, 0xc41003, io_quartet_p2_r },
	{ 0xc41004, 0xc41005, io_quartet_p3_r },
	{ 0xc41006, 0xc41007, io_quartet_p4_r },
	{ 0xc42000, 0xc42001, io_quartet_dip1_r },
	{ 0xc42002, 0xc42003, io_quartet_dip2_r },
	{ 0xc40000, 0xc4ffff, MRA_EXTRAM },
	{ 0xffc800, 0xffc801, quartet_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress quartet_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void quartet_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_workingram[0x0d14] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_workingram[0x0d18] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	if((READ_WORD(&sys16_extraram[4]) & 0xff) == 1)
		sys16_quartet_title_kludge=1;
	else
		sys16_quartet_title_kludge=0;

	set_fg_page1( READ_WORD( &sys16_workingram[0x0d1c] ) );
	set_bg_page1( READ_WORD( &sys16_workingram[0x0d1e] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram[2] ) );
}

static void quartet_init_machine( void ){
	static int bank[16] = { 00,01,02,03,00,01,02,03,00,01,02,03,00,01,02,03};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 2;
	sys16_sprxoffset = -0xbc;
	sys16_fgxoffset = sys16_bgxoffset = 7;

	sys16_update_proc = quartet_update_proc;
}

static void init_quartet( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 5,0x010000 );
}
/***************************************************************************/

INPUT_PORTS_START( quartet )
	// Player 1
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY  )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP  | IPF_8WAY  )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* player 1 coin 2 really */
	// Player 2
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY  | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP  | IPF_8WAY  | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* player 2 coin 2 really */
	// Player 3
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3  )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP  | IPF_8WAY  | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* player 3 coin 2 really */
	// Player 4
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY  | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP  | IPF_8WAY  | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER4)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* player 4 coin 2 really */

	SYS16_COINAGE

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, "Credit Power" )
	PORT_DIPSETTING(    0x04, "500" )
	PORT_DIPSETTING(    0x06, "1000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x00, "9000" )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, "Coin During Game" )
	PORT_DIPSETTING(    0x20, "Power" )
	PORT_DIPSETTING(    0x00, "Credit" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7751( machine_driver_quartet, \
	quartet_readmem,quartet_writemem,quartet_init_machine, gfx8 )

/***************************************************************************/
// pre16
ROM_START( quartet2 )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "quartet2.b9",  0x000000, 0x8000, 0x67177cd8 )
	ROM_LOAD_ODD ( "quartet2.b6",  0x000000, 0x8000, 0x50f50b08 )
	ROM_LOAD_EVEN( "quartet2.b10", 0x010000, 0x8000, 0x4273c3b7 )
	ROM_LOAD_ODD ( "quartet2.b7",  0x010000, 0x8000, 0x0aa337bb )
	ROM_LOAD_EVEN( "quartet2.b11", 0x020000, 0x8000, 0x3a6a375d )
	ROM_LOAD_ODD ( "quartet2.b8",  0x020000, 0x8000, 0xd87b2ca2 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "quartet2.c9",  0x00000, 0x08000, 0x547a6058 )
	ROM_LOAD( "quartet2.c10", 0x08000, 0x08000, 0x77ec901d )
	ROM_LOAD( "quartet2.c11", 0x10000, 0x08000, 0x7e348cce )

	ROM_REGION( 0x050000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr7465.5c",  0x000000, 0x008000, 0x8a1ab7d7 )
	ROM_RELOAD(              0x040000, 0x008000 ) //twice? - fixes a sprite glitch
	ROM_LOAD( "epr-7469.2b", 0x008000, 0x008000, 0xcb65ae4f )
	ROM_RELOAD(              0x048000, 0x008000 ) //twice?
	ROM_LOAD( "epr7466.6c",  0x010000, 0x008000, 0xb2d3f4f3 )
	ROM_LOAD( "epr-7470.3b", 0x018000, 0x008000, 0x16fc67b1 )
	ROM_LOAD( "epr7467.7c",  0x020000, 0x008000, 0x0af68de2 )
	ROM_LOAD( "epr-7471.4b", 0x028000, 0x008000, 0x13fad5ac )
	ROM_LOAD( "epr7468.8c",  0x030000, 0x008000, 0xddfd40c0 )
	ROM_LOAD( "epr-7472.5b", 0x038000, 0x008000, 0x8e2762ec )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr-7464.1b", 0x0000, 0x8000, 0x9f291306 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr7473.1c", 0x00000, 0x8000, 0x06ec75fa )
	ROM_LOAD( "epr7475.2c", 0x08000, 0x8000, 0x7abd1206 )
	ROM_LOAD( "epr7474.3c", 0x10000, 0x8000, 0xdbf853b8 )
	ROM_LOAD( "epr7476.4c", 0x18000, 0x8000, 0x5eba655a )
ROM_END

/***************************************************************************/

static READ_HANDLER( quartet2_skip_r )
{
	if (cpu_get_pc()==0x8f6c) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0800]);
}

static struct MemoryReadAddress quartet2_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc40000, 0xc4ffff, MRA_EXTRAM },
	{ 0xffc800, 0xffc801, quartet2_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress quartet2_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40000, 0xc4ffff, MWA_EXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void quartet2_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_workingram[0x0d14] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_workingram[0x0d18] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	if((READ_WORD(&sys16_extraram[4]) & 0xff) == 1)
		sys16_quartet_title_kludge=1;
	else
		sys16_quartet_title_kludge=0;

	set_fg_page1( READ_WORD( &sys16_workingram[0x0d1c] ) );
	set_bg_page1( READ_WORD( &sys16_workingram[0x0d1e] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram[2] ) );
}

static void quartet2_init_machine( void ){
	static int bank[16] = { 00,01,02,03,00,01,02,03,00,01,02,03,00,01,02,03};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 2;
	sys16_sprxoffset = -0xbc;
	sys16_fgxoffset = sys16_bgxoffset = 7;

	sys16_update_proc = quartet2_update_proc;
}

static void init_quartet2( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 5,0x010000 );
}
/***************************************************************************/

INPUT_PORTS_START( quartet2 )
	SYS16_JOY1_SWAPPEDBUTTONS
	SYS16_JOY2_SWAPPEDBUTTONS
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, "Credit Power" )
	PORT_DIPSETTING(    0x04, "500" )
	PORT_DIPSETTING(    0x06, "1000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x00, "9000" )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7751( machine_driver_quartet2, \
	quartet2_readmem,quartet2_writemem,quartet2_init_machine, gfx8 )

/***************************************************************************

   Riot City

***************************************************************************/
// sys16B
ROM_START( riotcity )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr14612.bin", 0x000000, 0x20000, 0xa1b331ec )
	ROM_LOAD_ODD ( "epr14610.bin", 0x000000, 0x20000, 0xcd4f2c50 )
	/* empty 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "epr14613.bin", 0x080000, 0x20000, 0x0659df4c )
	ROM_LOAD_ODD ( "epr14611.bin", 0x080000, 0x20000, 0xd9e6f80b )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr14616.bin", 0x00000, 0x20000, 0x46d30368 ) /* plane 1 */
	ROM_LOAD( "epr14625.bin", 0x20000, 0x20000, 0xabfb80fe )
	ROM_LOAD( "epr14617.bin", 0x40000, 0x20000, 0x884e40f9 ) /* plane 2 */
	ROM_LOAD( "epr14626.bin", 0x60000, 0x20000, 0x4ef55846 )
	ROM_LOAD( "epr14618.bin", 0x80000, 0x20000, 0x00eb260e ) /* plane 3 */
	ROM_LOAD( "epr14627.bin", 0xa0000, 0x20000, 0x961e5f82 )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr14619.bin",  0x000000, 0x040000, 0x6f2b5ef7 )
	ROM_LOAD( "epr14622.bin",  0x040000, 0x040000, 0x7ca7e40d )
	ROM_LOAD( "epr14620.bin",  0x080000, 0x040000, 0x66183333 )
	ROM_LOAD( "epr14623.bin",  0x0c0000, 0x040000, 0x98630049 )
	ROM_LOAD( "epr14621.bin",  0x100000, 0x040000, 0xc0f2820e )
	ROM_LOAD( "epr14624.bin",  0x140000, 0x040000, 0xd1a68448 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr14614.bin", 0x00000, 0x10000, 0xc65cc69a )
	ROM_LOAD( "epr14615.bin", 0x10000, 0x20000, 0x46653db1 )
ROM_END

/***************************************************************************/

static READ_HANDLER( riotcity_skip_r )
{
	if (cpu_get_pc()==0x3ce) {cpu_spinuntil_int(); return 0;}

	return READ_WORD(&sys16_workingram[0x2cde]);
}

static struct MemoryReadAddress riotcity_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x3f0000, 0x3fffff, MRA_EXTRAM },
	{ 0xf20000, 0xf20fff, MRA_EXTRAM3 },
	{ 0xf40000, 0xf40fff, MRA_SPRITERAM },
	{ 0xf60000, 0xf60fff, MRA_PALETTERAM },
	{ 0xf81002, 0xf81003, io_player1_r },
	{ 0xf81006, 0xf81007, io_player2_r },
	{ 0xf81000, 0xf81001, io_service_r },
	{ 0xf82002, 0xf82003, io_dip1_r },
	{ 0xf82000, 0xf82001, io_dip2_r },
	{ 0xf80000, 0xf8ffff, MRA_EXTRAM2 },
	{ 0xfa0000, 0xfaffff, MRA_TILERAM },
	{ 0xfb0000, 0xfb0fff, MRA_TEXTRAM },
	{ 0xffecde, 0xffecdf, riotcity_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress riotcity_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x3f0000, 0x3fffff, MWA_EXTRAM },
	{ 0xf00006, 0xf00007, sound_command_w },
	{ 0xf20000, 0xf20fff, MWA_EXTRAM3 },
	{ 0xf40000, 0xf40fff, MWA_SPRITERAM },
	{ 0xf60000, 0xf60fff, MWA_PALETTERAM },
	{ 0xf80000, 0xf8ffff, MWA_EXTRAM2 },
	{ 0xfa0000, 0xfaffff, MWA_TILERAM },
	{ 0xfb0000, 0xfb0fff, MWA_TEXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void riotcity_update_proc (void)
{
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys16_tile_bank1 = READ_WORD( &sys16_extraram3[0x0002] ) & 0xf;
	sys16_tile_bank0 = READ_WORD( &sys16_extraram3[0x0000] ) & 0xf;

	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void riotcity_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x08,0x0A,0x10,0x12,0x00,0x00,0x04,0x06,0x0C,0x0E,0x14,0x16,0x00,0x00};

	sys16_obj_bank = bank;
	sys16_spritesystem = 4;
	sys16_spritelist_end=0x8000;
	sys16_bg_priority_mode=1;

	sys16_update_proc = riotcity_update_proc;
}

static void init_riotcity(void)
{
	sys16_onetime_init_machine();
	sys16_sprite_decode (3,0x80000);
}

/***************************************************************************/

INPUT_PORTS_START( riotcity )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, "Attack Button to Start" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_riotcity, \
	riotcity_readmem,riotcity_writemem,riotcity_init_machine, gfx4,upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( sdi )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "a4.rom", 0x000000, 0x8000, 0xf2c41dd6 )
	ROM_LOAD_ODD ( "a1.rom", 0x000000, 0x8000, 0xa9f816ef )
	ROM_LOAD_EVEN( "a5.rom", 0x010000, 0x8000, 0x7952e27e )
	ROM_LOAD_ODD ( "a2.rom", 0x010000, 0x8000, 0x369af326 )
	ROM_LOAD_EVEN( "a6.rom", 0x020000, 0x8000, 0x8ee2c287 )
	ROM_LOAD_ODD ( "a3.rom", 0x020000, 0x8000, 0x193e4231 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "b9.rom",  0x00000, 0x10000, 0x182b6301 )
	ROM_LOAD( "b10.rom", 0x10000, 0x10000, 0x8f7129a2 )
	ROM_LOAD( "b11.rom", 0x20000, 0x10000, 0x4409411f )

	ROM_REGION( 0x060000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "b1.rom", 0x000000, 0x010000, 0x30e2c50a )
	ROM_LOAD( "b5.rom", 0x010000, 0x010000, 0x794e3e8b )
	ROM_LOAD( "b2.rom", 0x020000, 0x010000, 0x6a8b3fd0 )
	ROM_LOAD( "b6.rom", 0x030000, 0x010000, 0x602da5d5 )
	ROM_LOAD( "b3.rom", 0x040000, 0x010000, 0xb9de3aeb )
	ROM_LOAD( "b7.rom", 0x050000, 0x010000, 0x0a73a057 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "a7.rom", 0x0000, 0x8000, 0x793f9f7f )
ROM_END

// sys16A
ROM_START( sdioj )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
// Custom cpu 317-0027
	ROM_LOAD_EVEN( "epr10970.43", 0x000000, 0x8000, 0xb8fa4a2c )
	ROM_LOAD_ODD ( "epr10968.26", 0x000000, 0x8000, 0xa3f97793 )
	ROM_LOAD_EVEN( "epr10971.42", 0x010000, 0x8000, 0xc44a0328 )
	ROM_LOAD_ODD ( "epr10969.25", 0x010000, 0x8000, 0x455d15bd )
	ROM_LOAD_EVEN( "epr10755.41", 0x020000, 0x8000, 0x405e3969 )
	ROM_LOAD_ODD ( "epr10752.24", 0x020000, 0x8000, 0x77453740 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr10756.95", 0x00000, 0x10000, 0x44d8a506 )
	ROM_LOAD( "epr10757.94", 0x10000, 0x10000, 0x497e1740 )
	ROM_LOAD( "epr10758.93", 0x20000, 0x10000, 0x61d61486 )

	ROM_REGION( 0x060000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "b1.rom", 0x000000, 0x010000, 0x30e2c50a )
	ROM_LOAD( "b5.rom", 0x010000, 0x010000, 0x794e3e8b )
	ROM_LOAD( "b2.rom", 0x020000, 0x010000, 0x6a8b3fd0 )
	ROM_LOAD( "b6.rom", 0x030000, 0x010000, 0x602da5d5 )
	ROM_LOAD( "b3.rom", 0x040000, 0x010000, 0xb9de3aeb )
	ROM_LOAD( "b7.rom", 0x050000, 0x010000, 0x0a73a057 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr10759.12", 0x0000, 0x8000, 0xd7f9649f )
ROM_END


/***************************************************************************/

static READ_HANDLER( io_p1mousex_r ){ return 0xff-input_port_5_r( offset ); }
static READ_HANDLER( io_p1mousey_r ){ return input_port_6_r( offset ); }

static READ_HANDLER( io_p2mousex_r ){ return input_port_7_r( offset ); }
static READ_HANDLER( io_p2mousey_r ){ return input_port_8_r( offset ); }

static READ_HANDLER( sdi_skip_r )
{
	if (cpu_get_pc()==0x5326) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0400]);
}

static struct MemoryReadAddress sdi_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc40000, 0xc40001, MRA_EXTRAM },
	{ 0xc41004, 0xc41005, io_player1_r },
	{ 0xc41002, 0xc41003, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42004, 0xc42005, io_dip2_r },
	{ 0xc43000, 0xc43001, io_p1mousex_r },
	{ 0xc43004, 0xc43005, io_p1mousey_r },
	{ 0xc43008, 0xc43009, io_p2mousex_r },
	{ 0xc4300c, 0xc4300d, io_p2mousey_r },
//	{ 0xc42000, 0xc42001, MRA_NOP }, /* What is this? */
	{ 0xc60000, 0xc60001, MRA_NOP }, /* What is this? */
	{ 0xffc400, 0xffc401, sdi_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress sdi_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x123406, 0x123407, sound_command_w },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void sdi_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram[0] ) );
}

static void sdi_init_machine( void ){
	static int bank[16] = {00,00,00,00,00,00,00,0x06,00,00,00,0x04,00,0x02,00,00};

	sys16_obj_bank = bank;

	// ??
	patch_code( 0x102f2, 0x00 );
	patch_code( 0x102f3, 0x02 );

	sys16_update_proc = sdi_update_proc;
}

static void sdi_sprite_decode( void ){
	sys16_sprite_decode( 3,0x020000 );
}

static void init_sdi( void ){
	sys16_onetime_init_machine();
	sys18_splittab_bg_x=&sys16_textram[0x0fc0];
	sys16_rowscroll_scroll=0xff00;

	sdi_sprite_decode();
}

/***************************************************************************/

INPUT_PORTS_START( sdi )
PORT_START	/* DSW1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY | IPF_PLAYER2)

	SYS16_JOY2

PORT_START /* Service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "240?", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x80, "Every 50000" )
	PORT_DIPSETTING(    0xc0, "50000" )
	PORT_DIPSETTING(    0x40, "100000" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START				/* fake analog X */
	PORT_ANALOG( 0xff, 0x80, IPT_TRACKBALL_X, 75, 1, 0, 255 )

	PORT_START				/* fake analog Y */
	PORT_ANALOG( 0xff, 0x80, IPT_TRACKBALL_Y, 75, 1, 0, 255 )

	PORT_START				/* fake analog X */
	PORT_ANALOG( 0xff, 0x80, IPT_TRACKBALL_X | IPF_PLAYER2 , 75, 1, 0, 255 )

	PORT_START				/* fake analog Y */
	PORT_ANALOG( 0xff, 0x80, IPT_TRACKBALL_Y | IPF_PLAYER2, 75, 1, 0, 255 )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER( machine_driver_sdi, \
	sdi_readmem,sdi_writemem,sdi_init_machine, gfx1)

/***************************************************************************/
// sys18
ROM_START( shdancer )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "shdancer.a6", 0x000000, 0x40000, 0x3d5b3fa9 )
	ROM_LOAD_ODD ( "shdancer.a5", 0x000000, 0x40000, 0x2596004e )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "sd12712.bin", 0x00000, 0x40000, 0x9bdabe3d )
	ROM_LOAD( "sd12713.bin", 0x40000, 0x40000, 0x852d2b1c )
	ROM_LOAD( "sd12714.bin", 0x80000, 0x40000, 0x448226ce )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "sd12719.bin",  0x000000, 0x40000, 0xd6888534 )
	ROM_LOAD( "sd12726.bin",  0x040000, 0x40000, 0xff344945 )
	ROM_LOAD( "sd12718.bin",  0x080000, 0x40000, 0xba2efc0c )
	ROM_LOAD( "sd12725.bin",  0x0c0000, 0x40000, 0x268a0c17 )
	ROM_LOAD( "sd12717.bin",  0x100000, 0x40000, 0xc81cc4f8 )
	ROM_LOAD( "sd12724.bin",  0x140000, 0x40000, 0x0f4903dc )
	ROM_LOAD( "sd12716.bin",  0x180000, 0x40000, 0xa870e629 )
	ROM_LOAD( "sd12723.bin",  0x1c0000, 0x40000, 0xc606cf90 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "sd12720.bin", 0x10000, 0x20000, 0x7a0d8de1 )
	ROM_LOAD( "sd12715.bin", 0x30000, 0x40000, 0x07051a52 )
ROM_END

/***************************************************************************/

static READ_HANDLER( shdancer_skip_r )
{
	if (cpu_get_pc()==0x2f76) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0000]);
}


static struct MemoryReadAddress shdancer_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },

	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc00000, 0xc00007, MRA_EXTRAM },
	{ 0xe4000a, 0xe4000b, io_dip1_r },
	{ 0xe4000c, 0xe4000d, io_dip2_r },
	{ 0xe40000, 0xe40001, io_player1_r },
	{ 0xe40002, 0xe40003, io_player2_r },
	{ 0xe40008, 0xe40009, io_service_r },
	{ 0xe40000, 0xe4001f, MRA_EXTRAM2 },
	{ 0xe43034, 0xe43035, MRA_NOP },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress shdancer_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc00000, 0xc00007, MWA_EXTRAM },
	{ 0xe40000, 0xe4001f, MWA_EXTRAM2 },
	{ 0xe43034, 0xe43035, MWA_NOP },
	{ 0xfe0006, 0xfe0007, sound_command_nmi_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void shdancer_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys16_fg2_scrollx = READ_WORD( &sys16_textram[0x0e9c] );
	sys16_bg2_scrollx = READ_WORD( &sys16_textram[0x0e9e] );
	sys16_fg2_scrolly = READ_WORD( &sys16_textram[0x0e94] );
	sys16_bg2_scrolly = READ_WORD( &sys16_textram[0x0e96] );

	set_fg2_page( READ_WORD( &sys16_textram[0x0e84] ) );
	set_bg2_page( READ_WORD( &sys16_textram[0x0e86] ) );

	sys18_bg2_active=0;
	sys18_fg2_active=0;

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | READ_WORD( &sys16_textram[0x0e84] ))
		sys18_fg2_active=1;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | READ_WORD( &sys16_textram[0x0e86] ))
		sys18_bg2_active=1;

	set_tile_bank18( READ_WORD( &sys16_extraram[0] ) );
	set_refresh_18( READ_WORD( &sys16_extraram2[0x1c] ) );
}

static void shdancer_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};
	sys16_obj_bank = bank;
	sys16_spritelist_end=0x8000;

	sys16_update_proc = shdancer_update_proc;
}

static void init_shdancer( void ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	sys16_onetime_init_machine();
	sys18_splittab_fg_x=&sys16_textram[0x0f80];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0];
	install_mem_read_handler(0, 0xffc000, 0xffc001, shdancer_skip_r );
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=0;		// doesn't seem to use transparent shadows
#endif

	memcpy(RAM,&RAM[0x10000],0xa000);

	sys16_sprite_decode( 4,0x080000 );
}

/***************************************************************************/

INPUT_PORTS_START( shdancer )
PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, "Time Adjust" )
	PORT_DIPSETTING(    0x00, "2.20" )
	PORT_DIPSETTING(    0x40, "2.40" )
	PORT_DIPSETTING(    0xc0, "3.00" )
	PORT_DIPSETTING(    0x80, "3.30" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_18( machine_driver_shdancer, \
	shdancer_readmem,shdancer_writemem,shdancer_init_machine, gfx4 )

/***************************************************************************/

ROM_START( shdancbl )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ic39", 0x000000, 0x10000, 0xadc1781c )
	ROM_LOAD_ODD ( "ic53", 0x000000, 0x10000, 0x1c1ac463 )
	ROM_LOAD_EVEN( "ic38", 0x020000, 0x10000, 0xcd6e155b )
	ROM_LOAD_ODD ( "ic52", 0x020000, 0x10000, 0xbb3c49a4 )
	ROM_LOAD_EVEN( "ic37", 0x040000, 0x10000, 0x1bd8d5c3 )
	ROM_LOAD_ODD ( "ic51", 0x040000, 0x10000, 0xce2e71b4 )
	ROM_LOAD_EVEN( "ic36", 0x060000, 0x10000, 0xbb861290 )
	ROM_LOAD_ODD ( "ic50", 0x060000, 0x10000, 0x7f7b82b1 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ic4",  0x00000, 0x20000, 0xf0a016fe )
	ROM_LOAD( "ic18", 0x20000, 0x20000, 0xf6bee053 )
	ROM_LOAD( "ic3",  0x40000, 0x20000, 0xe07e6b5d )
	ROM_LOAD( "ic17", 0x60000, 0x20000, 0xf59deba1 )
	ROM_LOAD( "ic2",  0x80000, 0x20000, 0x60095070 )
	ROM_LOAD( "ic16", 0xa0000, 0x20000, 0x0f0d5dd3 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ic73", 0x000000, 0x10000, 0x59e77c96 )
	ROM_LOAD( "ic74", 0x010000, 0x10000, 0x90ea5407 )
	ROM_LOAD( "ic75", 0x020000, 0x10000, 0x27d2fa61 )
	ROM_LOAD( "ic76", 0x030000, 0x10000, 0xf36db688 )
	ROM_LOAD( "ic58", 0x040000, 0x10000, 0x9cd5c8c7 )
	ROM_LOAD( "ic59", 0x050000, 0x10000, 0xff40e872 )
	ROM_LOAD( "ic60", 0x060000, 0x10000, 0x826d7245 )
	ROM_LOAD( "ic61", 0x070000, 0x10000, 0xdcf8068b )
	ROM_LOAD( "ic77", 0x080000, 0x10000, 0xf93470b7 )
	ROM_LOAD( "ic78", 0x090000, 0x10000, 0x4d523ea3 )
	ROM_LOAD( "ic95", 0x0a0000, 0x10000, 0x828b8294 )
	ROM_LOAD( "ic94", 0x0b0000, 0x10000, 0x542b2d1e )
	ROM_LOAD( "ic62", 0x0c0000, 0x10000, 0x50ca8065 )
	ROM_LOAD( "ic63", 0x0d0000, 0x10000, 0xd1866aa9 )
	ROM_LOAD( "ic90", 0x0e0000, 0x10000, 0x3602b758 )
	ROM_LOAD( "ic89", 0x0f0000, 0x10000, 0x1ba4be93 )
	ROM_LOAD( "ic79", 0x100000, 0x10000, 0xf22548ee )
	ROM_LOAD( "ic80", 0x110000, 0x10000, 0x6209f7f9 )
	ROM_LOAD( "ic81", 0x120000, 0x10000, 0x34692f23 )
	ROM_LOAD( "ic82", 0x130000, 0x10000, 0x7ae40237 )
	ROM_LOAD( "ic64", 0x140000, 0x10000, 0x7a8b7bcc )
	ROM_LOAD( "ic65", 0x150000, 0x10000, 0x90ffca14 )
	ROM_LOAD( "ic66", 0x160000, 0x10000, 0x5d655517 )
	ROM_LOAD( "ic67", 0x170000, 0x10000, 0x0e5d0855 )
	ROM_LOAD( "ic83", 0x180000, 0x10000, 0xa9040a32 )
	ROM_LOAD( "ic84", 0x190000, 0x10000, 0xd6810031 )
	ROM_LOAD( "ic92", 0x1a0000, 0x10000, 0xb57d5cb5 )
	ROM_LOAD( "ic91", 0x1b0000, 0x10000, 0x49def6c8 )
	ROM_LOAD( "ic68", 0x1c0000, 0x10000, 0x8d684e53 )
	ROM_LOAD( "ic69", 0x1d0000, 0x10000, 0xc47d32e2 )
	ROM_LOAD( "ic88", 0x1e0000, 0x10000, 0x9de140e1 )
	ROM_LOAD( "ic87", 0x1f0000, 0x10000, 0x8172a991 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ic45", 0x10000, 0x10000, 0x576b3a81 )
	ROM_LOAD( "ic46", 0x20000, 0x10000, 0xc84e8c84 )
ROM_END

/***************************************************************************/

/*
static READ_HANDLER( shdancer_skip_r )
{
	if (cpu_get_pc()==0x2f76) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0x0000]);
}
*/

static struct MemoryReadAddress shdancbl_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },

	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc00000, 0xc00007, MRA_EXTRAM },
	{ 0xc40000, 0xc40001, io_dip1_r },
	{ 0xc40002, 0xc40003, io_dip2_r },

	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41004, 0xc41005, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
//	{ 0xc40000, 0xc4ffff, MRA_EXTRAM3 },
	{ 0xe40000, 0xe4001f, MRA_EXTRAM2 },
	{ 0xe43034, 0xe43035, MRA_NOP },
//	{ 0xffc000, 0xffc001, shdancer_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress shdancbl_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc00000, 0xc00007, MWA_EXTRAM },
//	{ 0xc40000, 0xc4ffff, MWA_EXTRAM3 },
	{ 0xe40000, 0xe4001f, MWA_EXTRAM2 },
	{ 0xe43034, 0xe43035, MWA_NOP },
	{ 0xfe0006, 0xfe0007, sound_command_nmi_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void shdancbl_update_proc( void ){
// this is all wrong and needs re-doing.

	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys16_fg2_scrollx = READ_WORD( &sys16_textram[0x0e9c] );
	sys16_bg2_scrollx = READ_WORD( &sys16_textram[0x0e9e] );
	sys16_fg2_scrolly = READ_WORD( &sys16_textram[0x0e94] );
	sys16_bg2_scrolly = READ_WORD( &sys16_textram[0x0e96] );

	set_fg2_page( READ_WORD( &sys16_textram[0x0e84] ) );
	set_bg2_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys18_bg2_active=0;
	sys18_fg2_active=0;

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | READ_WORD( &sys16_textram[0x0e84] ))
		sys18_fg2_active=1;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | READ_WORD( &sys16_textram[0x0e86] ))
		sys18_bg2_active=1;

	set_tile_bank18( READ_WORD( &sys16_extraram[0] ) );
	set_refresh_18( READ_WORD( &sys16_extraram2[0x1c] ) );
}


static void shdancbl_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};
	sys16_obj_bank = bank;
	sys16_spritelist_end=0x8000;
	sys16_sprxoffset = -0xbc+0x77;

	sys16_update_proc = shdancbl_update_proc;
}

static void init_shdancbl( void ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	int i;

	sys16_onetime_init_machine();
	sys18_splittab_fg_x=&sys16_textram[0x0f80];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0];
	install_mem_read_handler(0, 0xffc000, 0xffc001, shdancer_skip_r );
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=0;		// doesn't seem to use transparent shadows
#endif

	memcpy(RAM,&RAM[0x10000],0xa000);

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0xc0000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;
	sys16_sprite_decode( 4,0x080000 );
}
/***************************************************************************/

MACHINE_DRIVER_18( machine_driver_shdancbl, \
	shdancbl_readmem,shdancbl_writemem,shdancbl_init_machine, gfx4 )

/***************************************************************************/
// sys18
ROM_START( shdancrj )
	ROM_REGION( 0x080000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "sd12722b.bin", 0x000000, 0x40000, 0xc00552a2 )
	ROM_LOAD_ODD ( "sd12721b.bin", 0x000000, 0x40000, 0x653d351a )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "sd12712.bin",  0x00000, 0x40000, 0x9bdabe3d )
	ROM_LOAD( "sd12713.bin",  0x40000, 0x40000, 0x852d2b1c )
	ROM_LOAD( "sd12714.bin",  0x80000, 0x40000, 0x448226ce )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "sd12719.bin",  0x000000, 0x40000, 0xd6888534 )
	ROM_LOAD( "sd12726.bin",  0x040000, 0x40000, 0xff344945 )
	ROM_LOAD( "sd12718.bin",  0x080000, 0x40000, 0xba2efc0c )
	ROM_LOAD( "sd12725.bin",  0x0c0000, 0x40000, 0x268a0c17 )
	ROM_LOAD( "sd12717.bin",  0x100000, 0x40000, 0xc81cc4f8 )
	ROM_LOAD( "sd12724.bin",  0x140000, 0x40000, 0x0f4903dc )
	ROM_LOAD( "sd12716.bin",  0x180000, 0x40000, 0xa870e629 )
	ROM_LOAD( "sd12723.bin",  0x1c0000, 0x40000, 0xc606cf90 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "sd12720.bin", 0x10000, 0x20000, 0x7a0d8de1 )
	ROM_LOAD( "sd12715.bin", 0x30000, 0x40000, 0x07051a52 )
ROM_END

/***************************************************************************/
static READ_HANDLER( shdancrj_skip_r )
{
	if (cpu_get_pc()==0x2f70) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&sys16_workingram[0xc000]);
}

static void shdancrj_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};
	sys16_obj_bank = bank;
	sys16_spritelist_end=0x8000;

	patch_code(0x6821, 0xdf);
	sys16_update_proc = shdancer_update_proc;
}

static void init_shdancrj( void ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	sys16_onetime_init_machine();
	sys18_splittab_fg_x=&sys16_textram[0x0f80];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0];
	install_mem_read_handler(0, 0xffc000, 0xffc001, shdancrj_skip_r );

	memcpy(RAM,&RAM[0x10000],0xa000);

	sys16_sprite_decode( 4,0x080000 );
}

/***************************************************************************/

MACHINE_DRIVER_18( machine_driver_shdancrj, \
	shdancer_readmem,shdancer_writemem,shdancrj_init_machine, gfx4 )

/***************************************************************************/
// sys16B
ROM_START( shinobi )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "shinobi.a4", 0x000000, 0x10000, 0xb930399d )
	ROM_LOAD_ODD ( "shinobi.a1", 0x000000, 0x10000, 0x343f4c46 )
	ROM_LOAD_EVEN( "epr11283",   0x020000, 0x10000, 0x9d46e707 )
	ROM_LOAD_ODD ( "epr11281",   0x020000, 0x10000, 0x7961d07e )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "shinobi.b9",  0x00000, 0x10000, 0x5f62e163 )
	ROM_LOAD( "shinobi.b10", 0x10000, 0x10000, 0x75f8fbc9 )
	ROM_LOAD( "shinobi.b11", 0x20000, 0x10000, 0x06508bb9 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11290.10", 0x00000, 0x10000, 0x611f413a )
	ROM_LOAD( "epr11294.11", 0x10000, 0x10000, 0x5eb00fc1 )
	ROM_LOAD( "epr11291.17", 0x20000, 0x10000, 0x3c0797c0 )
	ROM_LOAD( "epr11295.18", 0x30000, 0x10000, 0x25307ef8 )
	ROM_LOAD( "epr11292.23", 0x40000, 0x10000, 0xc29ac34e )
	ROM_LOAD( "epr11296.24", 0x50000, 0x10000, 0x04a437f8 )
	ROM_LOAD( "epr11293.29", 0x60000, 0x10000, 0x41f41063 )
	ROM_LOAD( "epr11297.30", 0x70000, 0x10000, 0xb6e1fd72 )

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "shinobi.a7", 0x0000, 0x8000, 0x2457a7cf )
	ROM_LOAD( "shinobi.a8", 0x10000, 0x8000, 0xc8df8460 )
	ROM_LOAD( "shinobi.a9", 0x18000, 0x8000, 0xe5a4cf30 )

ROM_END

ROM_START( shinobib )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
// Custom cpu 317-0049
	ROM_LOAD_EVEN( "epr11282", 0x000000, 0x10000, 0x5f2e5524 )
	ROM_LOAD_ODD ( "epr11280", 0x000000, 0x10000, 0xbdfe5c38 )
	ROM_LOAD_EVEN( "epr11283", 0x020000, 0x10000, 0x9d46e707 )
	ROM_LOAD_ODD ( "epr11281", 0x020000, 0x10000, 0x7961d07e )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "shinobi.b9",  0x00000, 0x10000, 0x5f62e163 )
	ROM_LOAD( "shinobi.b10", 0x10000, 0x10000, 0x75f8fbc9 )
	ROM_LOAD( "shinobi.b11", 0x20000, 0x10000, 0x06508bb9 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11290.10", 0x00000, 0x10000, 0x611f413a )
	ROM_LOAD( "epr11294.11", 0x10000, 0x10000, 0x5eb00fc1 )
	ROM_LOAD( "epr11291.17", 0x20000, 0x10000, 0x3c0797c0 )
	ROM_LOAD( "epr11295.18", 0x30000, 0x10000, 0x25307ef8 )
	ROM_LOAD( "epr11292.23", 0x40000, 0x10000, 0xc29ac34e )
	ROM_LOAD( "epr11296.24", 0x50000, 0x10000, 0x04a437f8 )
	ROM_LOAD( "epr11293.29", 0x60000, 0x10000, 0x41f41063 )
	ROM_LOAD( "epr11297.30", 0x70000, 0x10000, 0xb6e1fd72 )

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "shinobi.a7", 0x0000, 0x8000, 0x2457a7cf )
	ROM_LOAD( "shinobi.a8", 0x10000, 0x8000, 0xc8df8460 )
	ROM_LOAD( "shinobi.a9", 0x18000, 0x8000, 0xe5a4cf30 )

ROM_END

/***************************************************************************/

static READ_HANDLER( shinobi_skip_r )
{
	if (cpu_get_pc()==0x32e0) {cpu_spinuntil_int(); return 1<<8;}

	return READ_WORD(&sys16_workingram[0x301c]);
}

static struct MemoryReadAddress shinobi_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc40000, 0xc40001, MRA_EXTRAM2 },
	{ 0xc43000, 0xc43001, MRA_NOP },
	{ 0xfff01c, 0xfff01d, shinobi_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress shinobi_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM2 },
	{ 0xc43000, 0xc43001, MWA_NOP },
	{ 0xfe0006, 0xfe0007, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void shinobi_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void shinobi_init_machine( void ){
	static int bank[16] = { 0,0,0,0,0,0,0,6,0,0,0,4,0,2,0,0 };
	sys16_obj_bank = bank;
	sys16_dactype = 1;
	sys16_update_proc = shinobi_update_proc;
}

static void init_shinobi( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x20000 );
}

/***************************************************************************/

INPUT_PORTS_START( shinobi )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "240", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, "Enemy's Bullet Speed" )
	PORT_DIPSETTING(    0x40, "Slow" )
	PORT_DIPSETTING(    0x00, "Fast" )
	PORT_DIPNAME( 0x80, 0x80, "Language" )
	PORT_DIPSETTING(    0x80, "Japanese" )
	PORT_DIPSETTING(    0x00, "English" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_shinobi, \
	shinobi_readmem,shinobi_writemem,shinobi_init_machine, gfx1,upd7759_interface )

/***************************************************************************/
// sys16A
ROM_START( shinobia )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0050
	ROM_LOAD_EVEN( "epr11262.42", 0x000000, 0x10000, 0xd4b8df12 )
	ROM_LOAD_ODD ( "epr11260.27", 0x000000, 0x10000, 0x2835c95d )
	ROM_LOAD_EVEN( "epr11263.43", 0x020000, 0x10000, 0xa2a620bd )
	ROM_LOAD_ODD ( "epr11261.25", 0x020000, 0x10000, 0xa3ceda52 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr11264.95", 0x00000, 0x10000, 0x46627e7d )
	ROM_LOAD( "epr11265.94", 0x10000, 0x10000, 0x87d0f321 )
	ROM_LOAD( "epr11266.93", 0x20000, 0x10000, 0xefb4af87 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11290.10", 0x00000, 0x10000, 0x611f413a )
	ROM_LOAD( "epr11294.11", 0x10000, 0x10000, 0x5eb00fc1 )
	ROM_LOAD( "epr11291.17", 0x20000, 0x10000, 0x3c0797c0 )
	ROM_LOAD( "epr11295.18", 0x30000, 0x10000, 0x25307ef8 )
	ROM_LOAD( "epr11292.23", 0x40000, 0x10000, 0xc29ac34e )
	ROM_LOAD( "epr11296.24", 0x50000, 0x10000, 0x04a437f8 )
	ROM_LOAD( "epr11293.29", 0x60000, 0x10000, 0x41f41063 )
	ROM_LOAD( "epr11297.30", 0x70000, 0x10000, 0xb6e1fd72 )

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr11267.12", 0x0000, 0x8000, 0xdd50b745 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x08000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr11268.1", 0x0000, 0x8000, 0x6d7966da )
ROM_END


ROM_START( shinobl )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
// Star Bootleg
	ROM_LOAD_EVEN( "b3",          0x000000, 0x10000, 0x38e59646 )
	ROM_LOAD_ODD ( "b1",          0x000000, 0x10000, 0x8529d192 )
	ROM_LOAD_EVEN( "epr11263.43", 0x020000, 0x10000, 0xa2a620bd )
	ROM_LOAD_ODD ( "epr11261.25", 0x020000, 0x10000, 0xa3ceda52 )

// Beta Bootleg
//	ROM_LOAD_EVEN( "4",           0x000000, 0x10000, 0xc178a39c )
//	ROM_LOAD_ODD ( "2",           0x000000, 0x10000, 0x5ad8ebf2 )
//	ROM_LOAD_EVEN( "epr11263.43", 0x020000, 0x10000, 0xa2a620bd )
//	ROM_LOAD_ODD ( "epr11261.25", 0x020000, 0x10000, 0xa3ceda52 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr11264.95", 0x00000, 0x10000, 0x46627e7d )
	ROM_LOAD( "epr11265.94", 0x10000, 0x10000, 0x87d0f321 )
	ROM_LOAD( "epr11266.93", 0x20000, 0x10000, 0xefb4af87 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11290.10", 0x00000, 0x10000, 0x611f413a )
	ROM_LOAD( "epr11294.11", 0x10000, 0x10000, 0x5eb00fc1 )
	ROM_LOAD( "epr11291.17", 0x20000, 0x10000, 0x3c0797c0 )
	ROM_LOAD( "epr11295.18", 0x30000, 0x10000, 0x25307ef8 )
	ROM_LOAD( "epr11292.23", 0x40000, 0x10000, 0xc29ac34e )
	ROM_LOAD( "epr11296.24", 0x50000, 0x10000, 0x04a437f8 )
	ROM_LOAD( "epr11293.29", 0x60000, 0x10000, 0x41f41063 )
//	ROM_LOAD( "epr11297.30", 0x70000, 0x10000, 0xb6e1fd72 )
	ROM_LOAD( "b17",         0x70000, 0x10000, 0x0315cf42 )	// Beta bootleg uses the rom above.

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr11267.12", 0x0000, 0x8000, 0xdd50b745 )

	ROM_REGION( 0x1000, REGION_CPU3 )      /* 4k for 7751 onboard ROM */
	ROM_LOAD( "7751.bin",     0x0000, 0x0400, 0x6a9534fc ) /* 7751 - U34 */

	ROM_REGION( 0x08000, REGION_SOUND1 ) /* 7751 sound data */
	ROM_LOAD( "epr11268.1", 0x0000, 0x8000, 0x6d7966da )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress shinobl_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42000, 0xc42001, io_dip1_r },
	{ 0xc42002, 0xc42003, io_dip2_r },
	{ 0xc40000, 0xc40fff, MRA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress shinobl_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, sound_command_nmi_w },
	{ 0xc40000, 0xc40fff, MWA_EXTRAM2 },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void shinobl_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	set_fg_page( READ_WORD( &sys16_textram[0x0e9e] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e9c] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram2[2] ) );

}

static void shinobl_init_machine( void ){
	static int bank[16] = {0,2,4,6,1,3,5,7,0,0,0,0,0,0,0,0};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 2;
	sys16_sprxoffset = -0xbc;
	sys16_fgxoffset = sys16_bgxoffset = 7;
	sys16_tilebank_switch=0x2000;

	sys16_dactype = 1;
	sys16_update_proc = shinobl_update_proc;
}



/***************************************************************************/

MACHINE_DRIVER_7751( machine_driver_shinobl, \
	shinobl_readmem,shinobl_writemem,shinobl_init_machine, gfx1)

/***************************************************************************/

// sys16A custom
ROM_START( tetris )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr12201.rom", 0x000000, 0x8000, 0x338e9b51 )
	ROM_LOAD_ODD ( "epr12200.rom", 0x000000, 0x8000, 0xfb058779 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12202.rom", 0x00000, 0x10000, 0x2f7da741 )
	ROM_LOAD( "epr12203.rom", 0x10000, 0x10000, 0xa6e58ec5 )
	ROM_LOAD( "epr12204.rom", 0x20000, 0x10000, 0x0ae98e23 )

	ROM_REGION( 0x010000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr12169.rom", 0x0000, 0x8000, 0xdacc6165 )
	ROM_LOAD( "epr12170.rom", 0x8000, 0x8000, 0x87354e42 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12205.rom", 0x0000, 0x8000, 0x6695dc99 )
ROM_END

// sys16B
ROM_START( tetrisbl )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "rom2.bin", 0x000000, 0x10000, 0x4d165c38 )
	ROM_LOAD_ODD ( "rom1.bin", 0x000000, 0x10000, 0x1e912131 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "scr01.rom", 0x00000, 0x10000, 0x62640221 )
	ROM_LOAD( "scr02.rom", 0x10000, 0x10000, 0x9abd183b )
	ROM_LOAD( "scr03.rom", 0x20000, 0x10000, 0x2495fd4e )

	ROM_REGION( 0x020000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "obj0-o.rom", 0x00000, 0x10000, 0x2fb38880 )
	ROM_LOAD( "obj0-e.rom", 0x10000, 0x10000, 0xd6a02cba )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "s-prog.rom", 0x0000, 0x8000, 0xbd9ba01b )
ROM_END

// sys16B
ROM_START( tetrisa )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
// Custom Cpu 317-0092
	ROM_LOAD_EVEN( "tetris.a7", 0x000000, 0x10000, 0x9ce15ac9 )
	ROM_LOAD_ODD ( "tetris.a5", 0x000000, 0x10000, 0x98d590ca )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "scr01.rom", 0x00000, 0x10000, 0x62640221 )
	ROM_LOAD( "scr02.rom", 0x10000, 0x10000, 0x9abd183b )
	ROM_LOAD( "scr03.rom", 0x20000, 0x10000, 0x2495fd4e )

	ROM_REGION( 0x020000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "obj0-o.rom", 0x00000, 0x10000, 0x2fb38880 )
	ROM_LOAD( "obj0-e.rom", 0x10000, 0x10000, 0xd6a02cba )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "s-prog.rom", 0x0000, 0x8000, 0xbd9ba01b )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress tetris_readmem[] =
{
	{ 0x000000, 0x01ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x418000, 0x41803f, MRA_EXTRAM2 },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc40000, 0xc40001, MRA_EXTRAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc80000, 0xc80001, MRA_NOP },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress tetris_writemem[] =
{
	{ 0x000000, 0x01ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x418000, 0x41803f, MWA_EXTRAM2 },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM },
	{ 0xc42006, 0xc42007, sound_command_w },
	{ 0xc43034, 0xc43035, MWA_NOP },
	{ 0xc80000, 0xc80001, MWA_NOP },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void tetris_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_extraram2[0x38] ) );
	set_bg_page( READ_WORD( &sys16_extraram2[0x28] ) );

	set_refresh( READ_WORD( &sys16_extraram[0x0] ) );
}

static void tetris_init_machine( void ){
	static int bank[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	sys16_obj_bank = bank;

	patch_code( 0xba6, 0x4e );
	patch_code( 0xba7, 0x71 );

	sys16_sprxoffset = -0x40;
	sys16_update_proc = tetris_update_proc;
}

static void init_tetris( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 1,0x10000 );
}

static void init_tetrisbl( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 1,0x20000 );
}
/***************************************************************************/

INPUT_PORTS_START( tetris )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE /* unconfirmed */

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Unknown ) )	// from the code it looks like some kind of difficulty
	PORT_DIPSETTING(    0x0c, "A" )					// level, but all 4 levels points to the same place
	PORT_DIPSETTING(    0x08, "B" )					// so it doesn't actually change anything!!
	PORT_DIPSETTING(    0x04, "C" )
	PORT_DIPSETTING(    0x00, "D" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER( machine_driver_tetris, \
	tetris_readmem,tetris_writemem,tetris_init_machine, gfx1 )

/***************************************************************************/
// sys16B
ROM_START( timscanr )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ts10853.bin", 0x00000, 0x8000, 0x24d7c5fb )
	ROM_LOAD_ODD ( "ts10850.bin", 0x00000, 0x8000, 0xf1575732 )
	ROM_LOAD_EVEN( "ts10854.bin", 0x10000, 0x8000, 0x82d0b237 )
	ROM_LOAD_ODD ( "ts10851.bin", 0x10000, 0x8000, 0xf5ce271b )
	ROM_LOAD_EVEN( "ts10855.bin", 0x20000, 0x8000, 0x63e95a53 )
	ROM_LOAD_ODD ( "ts10852.bin", 0x20000, 0x8000, 0x7cd1382b )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "timscanr.b9",  0x00000, 0x8000, 0x07dccc37 )
	ROM_LOAD( "timscanr.b10", 0x08000, 0x8000, 0x84fb9a3a )
	ROM_LOAD( "timscanr.b11", 0x10000, 0x8000, 0xc8694bc0 )

	ROM_REGION( 0x40000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ts10548.bin", 0x00000, 0x8000, 0xaa150735 )
	ROM_LOAD( "ts10552.bin", 0x08000, 0x8000, 0x6fcbb9f7 )
	ROM_LOAD( "ts10549.bin", 0x10000, 0x8000, 0x2f59f067 )
	ROM_LOAD( "ts10553.bin", 0x18000, 0x8000, 0x8a220a9f )
	ROM_LOAD( "ts10550.bin", 0x20000, 0x8000, 0xf05069ff )
	ROM_LOAD( "ts10554.bin", 0x28000, 0x8000, 0xdc64f809 )
	ROM_LOAD( "ts10551.bin", 0x30000, 0x8000, 0x435d811f )
	ROM_LOAD( "ts10555.bin", 0x38000, 0x8000, 0x2143c471 )

	ROM_REGION( 0x18000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ts10562.bin", 0x0000, 0x8000, 0x3f5028bf )
	ROM_LOAD( "ts10563.bin", 0x10000, 0x8000, 0x9db7eddf )
ROM_END

/***************************************************************************/

static READ_HANDLER( timscanr_skip_r )
{
	if (cpu_get_pc()==0x1044c) {cpu_spinuntil_int(); return 0;}

	return READ_WORD(&sys16_workingram[0x000c]);
}


static struct MemoryReadAddress timscanr_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc41004, 0xc41005, io_dip3_r },
	{ 0xffc00c, 0xffc00d, timscanr_skip_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress timscanr_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM },
	{ 0xfe0006, 0xfe0007, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void timscanr_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram[0x0] ) );
}

static void timscanr_init_machine( void ){
	static int bank[16] = {00,00,00,00,00,00,00,0x03,00,00,00,0x02,00,0x01,00,00};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_update_proc = timscanr_update_proc;
}

static void init_timscanr( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x10000 );
}
/***************************************************************************/

INPUT_PORTS_START( timscanr )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )		//??
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x1e, 0x14, "Bonus" )
	PORT_DIPSETTING(    0x16, "Replay 1000000/2000000" )
	PORT_DIPSETTING(    0x14, "Replay 1200000/2500000" )
	PORT_DIPSETTING(    0x12, "Replay 1500000/3000000" )
	PORT_DIPSETTING(    0x10, "Replay 2000000/4000000" )
	PORT_DIPSETTING(    0x1c, "Replay 1000000" )
	PORT_DIPSETTING(    0x1e, "Replay 1200000" )
	PORT_DIPSETTING(    0x1a, "Replay 1500000" )
	PORT_DIPSETTING(    0x18, "Replay 1800000" )
	PORT_DIPSETTING(    0x0e, "ExtraBall 100000" )
	PORT_DIPSETTING(    0x0c, "ExtraBall 200000" )
	PORT_DIPSETTING(    0x0a, "ExtraBall 300000" )
	PORT_DIPSETTING(    0x08, "ExtraBall 400000" )
	PORT_DIPSETTING(    0x06, "ExtraBall 500000" )
	PORT_DIPSETTING(    0x04, "ExtraBall 600000" )
	PORT_DIPSETTING(    0x02, "ExtraBall 700000" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_DIPNAME( 0x20, 0x20, "Match" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x00, "5" )

PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )		//??
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_timscanr, \
	timscanr_readmem,timscanr_writemem,timscanr_init_machine, gfx8,upd7759_interface )

/***************************************************************************/

// sys16B
ROM_START( toryumon )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "17689",  0x00000, 0x20000, 0x4f0dee19 )
	ROM_LOAD_ODD ( "17688",  0x00000, 0x20000, 0x717d81c7 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "17700", 0x00000, 0x40000, 0x8f288b37 )
	ROM_LOAD( "17701", 0x40000, 0x40000, 0x6dfb025b )
	ROM_LOAD( "17702", 0x80000, 0x40000, 0xae0b7eab )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "17692", 0x00000, 0x20000, 0x543c4327 )
	ROM_LOAD( "17695", 0x20000, 0x20000, 0xee60f244 )
	ROM_LOAD( "17693", 0x40000, 0x20000, 0x4a350b3e )
	ROM_LOAD( "17696", 0x60000, 0x20000, 0x6edb54f1 )
	ROM_LOAD( "17694", 0x80000, 0x20000, 0xb296d71d )
	ROM_LOAD( "17697", 0xa0000, 0x20000, 0x6ccb7b28 )
	ROM_LOAD( "17698", 0xc0000, 0x20000, 0xcd4dfb82 )
	ROM_LOAD( "17699", 0xe0000, 0x20000, 0x2694ecce )


	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "17691", 0x00000,  0x08000, 0x14205388 )
	ROM_LOAD( "17690", 0x10000,  0x40000, 0x4f9ba4e4 )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress toryumon_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x3e2000, 0x3e2003, MRA_EXTRAM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },

	{ 0xe40000, 0xe40001, MRA_EXTRAM2 },

	{ 0xe41002, 0xe41003, io_player1_r },
	{ 0xe41004, 0xe41005, MRA_NOP },
	{ 0xe41006, 0xe41007, io_player2_r },
	{ 0xe41000, 0xe41001, io_service_r },
	{ 0xe42002, 0xe42003, io_dip1_r },
	{ 0xe42000, 0xe42001, io_dip2_r },

	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress toryumon_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x3e2000, 0x3e2003, MWA_EXTRAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xe40000, 0xe40001, MWA_EXTRAM2 },
	{ 0xfe0006, 0xfe0007, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void toryumon_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	sys16_tile_bank0 = READ_WORD( &sys16_extraram[0x0000] )&0xf;
	sys16_tile_bank1 = READ_WORD( &sys16_extraram[0x0002] )&0xf;

	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void toryumon_init_machine( void ){
	static int bank[16] = {00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,00,00,00,00,00,00,00,00};
	sys16_obj_bank = bank;

	sys16_update_proc = toryumon_update_proc;
}

static void init_toryumon(void)
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x40000 );
}
/***************************************************************************/

INPUT_PORTS_START( toryumon )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "VS-Mode Battle" )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0xe0, "Normal" )
	PORT_DIPSETTING(    0xa0, "Hard" )
	PORT_DIPSETTING(    0x80, "Hard+1" )
	PORT_DIPSETTING(    0x60, "Hard+2" )
	PORT_DIPSETTING(    0x40, "Hard+3" )
	PORT_DIPSETTING(    0x20, "Hard+4" )
	PORT_DIPSETTING(    0x00, "Hard+5" )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_toryumon, \
	toryumon_readmem,toryumon_writemem,toryumon_init_machine, gfx4,upd7759_interface )

/***************************************************************************/

// sys16B
ROM_START( tturf )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "12327.7a",  0x00000, 0x20000, 0x0376c593 )
	ROM_LOAD_ODD ( "12326.5a",  0x00000, 0x20000, 0xf998862b )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "12268.14a", 0x00000, 0x10000, 0xe0dac07f )
	ROM_LOAD( "12269.15a", 0x10000, 0x10000, 0x457a8790 )
	ROM_LOAD( "12270.16a", 0x20000, 0x10000, 0x69fc025b )

	ROM_REGION( 0x80000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "12279.1b", 0x00000, 0x10000, 0x7a169fb1 )
	ROM_LOAD( "12283.5b", 0x10000, 0x10000, 0xae0fa085 )
	ROM_LOAD( "12278.2b", 0x20000, 0x10000, 0x961d06b7 )
	ROM_LOAD( "12282.6b", 0x30000, 0x10000, 0xe8671ee1 )
	ROM_LOAD( "12277.3b", 0x40000, 0x10000, 0xf16b6ba2 )
	ROM_LOAD( "12281.7b", 0x50000, 0x10000, 0x1ef1077f )
	ROM_LOAD( "12276.4b", 0x60000, 0x10000, 0x838bd71f )
	ROM_LOAD( "12280.8b", 0x70000, 0x10000, 0x639a57cb )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "12328.10a", 0x0000, 0x8000, 0x00000000 )
	ROM_LOAD( "12329.11a", 0x10000, 0x10000, 0xed9a686d )		// speech
	ROM_LOAD( "12330.12a", 0x20000, 0x10000, 0xfb762bca )

ROM_END

// sys16B
ROM_START( tturfu )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr12266.bin",  0x00000, 0x10000, 0xf549def8 )
	ROM_LOAD_ODD ( "epr12264.bin",  0x00000, 0x10000, 0xf7cdb289 )
	ROM_LOAD_EVEN( "epr12267.bin",  0x20000, 0x10000, 0x3c3ce191 )
	ROM_LOAD_ODD ( "epr12265.bin",  0x20000, 0x10000, 0x8cdadd9a )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "12268.14a", 0x00000, 0x10000, 0xe0dac07f )
	ROM_LOAD( "12269.15a", 0x10000, 0x10000, 0x457a8790 )
	ROM_LOAD( "12270.16a", 0x20000, 0x10000, 0x69fc025b )

	ROM_REGION( 0x80000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "12279.1b", 0x00000, 0x10000, 0x7a169fb1 )
	ROM_LOAD( "12283.5b", 0x10000, 0x10000, 0xae0fa085 )
	ROM_LOAD( "12278.2b", 0x20000, 0x10000, 0x961d06b7 )
	ROM_LOAD( "12282.6b", 0x30000, 0x10000, 0xe8671ee1 )
	ROM_LOAD( "12277.3b", 0x40000, 0x10000, 0xf16b6ba2 )
	ROM_LOAD( "12281.7b", 0x50000, 0x10000, 0x1ef1077f )
	ROM_LOAD( "12276.4b", 0x60000, 0x10000, 0x838bd71f )
	ROM_LOAD( "12280.8b", 0x70000, 0x10000, 0x639a57cb )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12271.bin", 0x0000,  0x8000, 0x99671e52 )
	ROM_LOAD( "epr12272.bin", 0x10000, 0x8000, 0x7cf7e69f )
	ROM_LOAD( "epr12273.bin", 0x18000, 0x8000, 0x28f0bb8b )
	ROM_LOAD( "epr12274.bin", 0x20000, 0x8000, 0x8207f0c4 )
	ROM_LOAD( "epr12275.bin", 0x28000, 0x8000, 0x182f3c3d )

ROM_END

/***************************************************************************/
static READ_HANDLER( tt_io_player1_r ){ return input_port_0_r( offset ) << 8; }
static READ_HANDLER( tt_io_player2_r ){ return input_port_1_r( offset ) << 8; }
static READ_HANDLER( tt_io_service_r ){ return input_port_2_r( offset ) << 8; }

static struct MemoryReadAddress tturf_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x2001e6, 0x2001e7, tt_io_service_r },
	{ 0x2001e8, 0x2001e9, tt_io_player1_r },
	{ 0x2001ea, 0x2001eb, tt_io_player2_r },
	{ 0x200000, 0x203fff, MRA_EXTRAM },
	{ 0x300000, 0x300fff, MRA_SPRITERAM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x500000, 0x500fff, MRA_PALETTERAM },

	{ 0x602002, 0x602003, io_dip1_r },
	{ 0x602000, 0x602001, io_dip2_r },
	{-1}
};

static struct MemoryWriteAddress tturf_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x200000, 0x203fff, MWA_EXTRAM },
	{ 0x300000, 0x300fff, MWA_SPRITERAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x500000, 0x500fff, MWA_PALETTERAM },
	{ 0x600000, 0x600005, MWA_EXTRAM2 },
//	{ 0x600006, 0x600007, sound_command_w },
	{-1}
};
/***************************************************************************/
static void tturf_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void tturf_init_machine( void ){
	static int bank[16] = {00,00,0x02,00,0x04,00,0x06,00,00,00,00,00,00,00,00,00};
	sys16_obj_bank = bank;
	sys16_spritelist_end=0xc000;

	sys16_update_proc = tturf_update_proc;
}

static void tturfu_init_machine( void ){
	static int bank[16] = {00,00,00,00,00,00,00,00,00,00,00,02,00,04,06,00};
	sys16_obj_bank = bank;
	sys16_spritelist_end=0xc000;

	sys16_update_proc = tturf_update_proc;
}

static void init_tturf(void)
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x20000 );
}
/***************************************************************************/

INPUT_PORTS_START( tturf )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, "Continues" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "Unlimited" )
	PORT_DIPSETTING(    0x03, "Unlimited" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x20, "Starting Energy" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "6" )
	PORT_DIPSETTING(    0x30, "8" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Bonus Energy" )
	PORT_DIPSETTING(    0x80, "1" )
	PORT_DIPSETTING(    0x00, "2" )
INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_tturf, \
	tturf_readmem,tturf_writemem,tturf_init_machine, gfx1,upd7759_interface )

MACHINE_DRIVER_7759( machine_driver_tturfu, \
	tturf_readmem,tturf_writemem,tturfu_init_machine, gfx1,upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( tturfbl )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "tt042197.rom", 0x00000, 0x10000, 0xdeee5af1 )
	ROM_LOAD_ODD ( "tt06c794.rom", 0x00000, 0x10000, 0x90e6a95a )
	ROM_LOAD_EVEN( "tt030be3.rom", 0x20000, 0x10000, 0x100264a2 )
	ROM_LOAD_ODD ( "tt05ef8a.rom", 0x20000, 0x10000, 0xf787a948 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "tt1574b3.rom", 0x00000, 0x10000, 0xe9e630da )
	ROM_LOAD( "tt16cf44.rom", 0x10000, 0x10000, 0x4c467735 )
	ROM_LOAD( "tt17d59e.rom", 0x20000, 0x10000, 0x60c0f2fe )

	ROM_REGION( 0x80000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "12279.1b", 0x00000, 0x10000, 0x7a169fb1 )
	ROM_LOAD( "12283.5b", 0x10000, 0x10000, 0xae0fa085 )
	ROM_LOAD( "12278.2b", 0x20000, 0x10000, 0x961d06b7 )
	ROM_LOAD( "12282.6b", 0x30000, 0x10000, 0xe8671ee1 )
	ROM_LOAD( "12277.3b", 0x40000, 0x10000, 0xf16b6ba2 )
	ROM_LOAD( "12281.7b", 0x50000, 0x10000, 0x1ef1077f )
	ROM_LOAD( "12276.4b", 0x60000, 0x10000, 0x838bd71f )
	ROM_LOAD( "12280.8b", 0x70000, 0x10000, 0x639a57cb )

	ROM_REGION( 0x28000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "tt014d68.rom", 0x00000, 0x08000, 0xd4aab1d9 )
	ROM_CONTINUE(             0x10000, 0x08000 )
	ROM_LOAD( "tt0246ff.rom", 0x18000, 0x10000, 0xbb4bba8f )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress tturfbl_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x2001e6, 0x2001e7, tt_io_service_r },
	{ 0x2001e8, 0x2001e9, tt_io_player1_r },
	{ 0x2001ea, 0x2001eb, tt_io_player2_r },
	{ 0x200000, 0x203fff, MRA_EXTRAM },
	{ 0x300000, 0x300fff, MRA_SPRITERAM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x500000, 0x500fff, MRA_PALETTERAM },
	{ 0x600002, 0x600003, io_dip1_r },
	{ 0x600000, 0x600001, io_dip2_r },
	{ 0x601002, 0x601003, io_player1_r },
	{ 0x601004, 0x601005, io_player2_r },
	{ 0x601000, 0x601001, io_service_r },
	{ 0x602002, 0x602003, io_dip1_r },
	{ 0x602000, 0x602001, io_dip2_r },
	{ 0xc46000, 0xc4601f, MRA_EXTRAM3 },
	{-1}
};

static struct MemoryWriteAddress tturfbl_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x200000, 0x203fff, MWA_EXTRAM },
	{ 0x300000, 0x300fff, MWA_SPRITERAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x500000, 0x500fff, MWA_PALETTERAM },
	{ 0x600000, 0x600005, MWA_EXTRAM2 },
	{ 0x600006, 0x600007, sound_command_w },
	{ 0xc44000, 0xc44001, MWA_NOP },
	{ 0xc46000, 0xc4601f, MWA_EXTRAM3 },
	{-1}
};

/***************************************************************************/

static void tturfbl_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );


	{
		int data1,data2;

		data1 = READ_WORD( &sys16_textram[0x0e80] );
		data2 = READ_WORD( &sys16_textram[0x0e82] );

		sys16_fg_page[3] = data1>>12;
		sys16_bg_page[3] = (data1>>8)&0xf;
		sys16_fg_page[1] = (data1>>4)&0xf;
		sys16_bg_page[1] = data1&0xf;

		sys16_fg_page[2] = data2>>12;
		sys16_bg_page[2] = (data2>>8)&0xf;
		sys16_fg_page[0] = (data2>>4)&0xf;
		sys16_bg_page[0] = data2&0xf;
	}


	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void tturfbl_init_machine( void ){
	static int bank[16] = {00,00,00,00,00,00,00,0x06,00,00,00,0x04,00,0x02,00,00};
	sys16_obj_bank = bank;
	sys16_sprxoffset = -0x48;
	sys16_spritelist_end=0xc000;

	sys16_update_proc = tturfbl_update_proc;
}

static void init_tturfbl(void)
{
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x30000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	sys16_sprite_decode( 4,0x20000 );
}
/***************************************************************************/
// sound ??
MACHINE_DRIVER_7759( machine_driver_tturfbl, \
	tturfbl_readmem,tturfbl_writemem,tturfbl_init_machine, gfx1,upd7759_interface )

/***************************************************************************/
// sys16B
ROM_START( wb3 )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr12259.a7", 0x000000, 0x20000, 0x54927c7e )
	ROM_LOAD_ODD ( "epr12258.a5", 0x000000, 0x20000, 0x01f5898c )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12124.a14", 0x00000, 0x10000, 0xdacefb6f )
	ROM_LOAD( "epr12125.a15", 0x10000, 0x10000, 0x9fc36df7 )
	ROM_LOAD( "epr12126.a16", 0x20000, 0x10000, 0xa693fd94 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr12093.b4", 0x000000, 0x010000, 0x4891e7bb )
	ROM_LOAD( "epr12097.b8", 0x010000, 0x010000, 0xe645902c )
	ROM_LOAD( "epr12091.b2", 0x020000, 0x010000, 0x8409a243 )
	ROM_LOAD( "epr12095.b6", 0x030000, 0x010000, 0xe774ec2c )
	ROM_LOAD( "epr12090.b1", 0x040000, 0x010000, 0xaeeecfca )
	ROM_LOAD( "epr12094.b5", 0x050000, 0x010000, 0x615e4927 )
	ROM_LOAD( "epr12092.b3", 0x060000, 0x010000, 0x5c2f0d90 )
	ROM_LOAD( "epr12096.b7", 0x070000, 0x010000, 0x0cd59d6e )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12127.a10", 0x0000, 0x8000, 0x0bb901bb )
ROM_END

ROM_START( wb3a )
	ROM_REGION( 0x40000, REGION_CPU1 ) /* 68000 code */
// Custom CPU 317-0089
	ROM_LOAD_EVEN( "epr12137.a7", 0x000000, 0x20000, 0x6f81238e )
	ROM_LOAD_ODD ( "epr12136.a5", 0x000000, 0x20000, 0x4cf05003 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12124.a14", 0x00000, 0x10000, 0xdacefb6f )
	ROM_LOAD( "epr12125.a15", 0x10000, 0x10000, 0x9fc36df7 )
	ROM_LOAD( "epr12126.a16", 0x20000, 0x10000, 0xa693fd94 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr12093.b4", 0x000000, 0x010000, 0x4891e7bb )
	ROM_LOAD( "epr12097.b8", 0x010000, 0x010000, 0xe645902c )
	ROM_LOAD( "epr12091.b2", 0x020000, 0x010000, 0x8409a243 )
	ROM_LOAD( "epr12095.b6", 0x030000, 0x010000, 0xe774ec2c )
	ROM_LOAD( "epr12090.b1", 0x040000, 0x010000, 0xaeeecfca )
	ROM_LOAD( "epr12094.b5", 0x050000, 0x010000, 0x615e4927 )
	ROM_LOAD( "epr12092.b3", 0x060000, 0x010000, 0x5c2f0d90 )
	ROM_LOAD( "epr12096.b7", 0x070000, 0x010000, 0x0cd59d6e )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12127.a10", 0x0000, 0x8000, 0x0bb901bb )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress wb3_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static WRITE_HANDLER( wb3_sound_command_w )
{
	if( (data&0xff000000)==0 )
		sound_command_w(offset,data>>8);
}

static struct MemoryWriteAddress wb3_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x3f0000, 0x3f0003, MWA_NOP },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM2 },
	{ 0xffc008, 0xffc009, wb3_sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void wb3_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}


static void wb3_init_machine( void ){
	static int bank[16] = {4,0,2,0,6,0,0,0x06,0,0,0,0x04,0,0x02,0,0};

	sys16_obj_bank = bank;

	sys16_update_proc = wb3_update_proc;
}

static void init_wb3(void)
{
	sys16_onetime_init_machine();
	sys16_sprite_decode( 4,0x20000 );
}

/***************************************************************************/

INPUT_PORTS_START( wb3 )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )		//??
	PORT_DIPSETTING(    0x10, "5000/10000/18000/30000" )
	PORT_DIPSETTING(    0x00, "5000/15000/30000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Round Select" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )			// no collision though
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER( machine_driver_wb3, \
	wb3_readmem,wb3_writemem,wb3_init_machine, gfx1 )

/***************************************************************************/
// sys16B
ROM_START( wb3bl )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "wb3_03", 0x000000, 0x10000, 0x0019ab3b )
	ROM_LOAD_ODD ( "wb3_05", 0x000000, 0x10000, 0x196e17ee )
	ROM_LOAD_EVEN( "wb3_02", 0x020000, 0x10000, 0xc87350cb )
	ROM_LOAD_ODD ( "wb3_04", 0x020000, 0x10000, 0x565d5035 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "wb3_14", 0x00000, 0x10000, 0xd3f20bca )
	ROM_LOAD( "wb3_15", 0x10000, 0x10000, 0x96ff9d52 )
	ROM_LOAD( "wb3_16", 0x20000, 0x10000, 0xafaf0d31 )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr12093.b4", 0x000000, 0x010000, 0x4891e7bb )
	ROM_LOAD( "epr12097.b8", 0x010000, 0x010000, 0xe645902c )
	ROM_LOAD( "epr12091.b2", 0x020000, 0x010000, 0x8409a243 )
	ROM_LOAD( "epr12095.b6", 0x030000, 0x010000, 0xe774ec2c )
	ROM_LOAD( "epr12090.b1", 0x040000, 0x010000, 0xaeeecfca )
	ROM_LOAD( "epr12094.b5", 0x050000, 0x010000, 0x615e4927 )
	ROM_LOAD( "epr12092.b3", 0x060000, 0x010000, 0x5c2f0d90 )
	ROM_LOAD( "epr12096.b7", 0x070000, 0x010000, 0x0cd59d6e )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12127.a10", 0x0000, 0x8000, 0x0bb901bb )
ROM_END

/***************************************************************************/

static struct MemoryReadAddress wb3bl_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x440000, 0x440fff, MRA_SPRITERAM },
	{ 0x840000, 0x840fff, MRA_PALETTERAM },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41004, 0xc41005, io_player2_r },
	{ 0xc41000, 0xc41001, io_service_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xc46000, 0xc4601f, MRA_EXTRAM3 },
	{ 0xff0000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress wb3bl_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x3f0000, 0x3f0003, MWA_NOP },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x440000, 0x440fff, MWA_SPRITERAM },
	{ 0x840000, 0x840fff, MWA_PALETTERAM },
	{ 0xc42006, 0xc42007, sound_command_w },
	{ 0xc40000, 0xc40001, MWA_EXTRAM2 },
	{ 0xc44000, 0xc44001, MWA_NOP },
	{ 0xc46000, 0xc4601f, MWA_EXTRAM3 },
	{ 0xff0000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};

/***************************************************************************/

static void wb3bl_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_workingram[0xc030] );
	sys16_bg_scrollx = READ_WORD( &sys16_workingram[0xc038] );
	sys16_fg_scrolly = READ_WORD( &sys16_workingram[0xc032] );
	sys16_bg_scrolly = READ_WORD( &sys16_workingram[0xc03c] );

	set_fg_page( READ_WORD( &sys16_textram[0x0ff6] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0ff4] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void wb3bl_init_machine( void ){
	static int bank[16] = {4,0,2,0,6,0,0,0x06,0,0,0,0x04,0,0x02,0,0};

	sys16_obj_bank = bank;

	patch_code( 0x17058, 0x4e );
	patch_code( 0x17059, 0xb9 );
	patch_code( 0x1705a, 0x00 );
	patch_code( 0x1705b, 0x00 );
	patch_code( 0x1705c, 0x09 );
	patch_code( 0x1705d, 0xdc );
	patch_code( 0x1705e, 0x4e );
	patch_code( 0x1705f, 0xf9 );
	patch_code( 0x17060, 0x00 );
	patch_code( 0x17061, 0x01 );
	patch_code( 0x17062, 0x70 );
	patch_code( 0x17063, 0xe0 );
	patch_code( 0x1a3a, 0x31 );
	patch_code( 0x1a3b, 0x7c );
	patch_code( 0x1a3c, 0x80 );
	patch_code( 0x1a3d, 0x00 );
	patch_code( 0x23df8, 0x14 );
	patch_code( 0x23df9, 0x41 );
	patch_code( 0x23dfa, 0x10 );
	patch_code( 0x23dfd, 0x14 );
	patch_code( 0x23dff, 0x1c );

	sys16_update_proc = wb3bl_update_proc;
}

static void init_wb3bl(void)
{
	int i;

	sys16_onetime_init_machine();

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0x30000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	sys16_sprite_decode( 4,0x20000 );
}

/***************************************************************************/

MACHINE_DRIVER( machine_driver_wb3bl, \
	wb3bl_readmem,wb3bl_writemem,wb3bl_init_machine, gfx1 )

/***************************************************************************/
// sys16B
ROM_START( wrestwar )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ww.a7", 0x00000, 0x20000, 0xeeaba126 )
	ROM_LOAD_ODD ( "ww.a5", 0x00000, 0x20000, 0x6714600a )
	/* empty 0x40000 - 0x80000 */
	ROM_LOAD_EVEN( "ww.a8", 0x80000, 0x20000, 0xb77ba665 )
	ROM_LOAD_ODD ( "ww.a6", 0x80000, 0x20000, 0xddf075cb )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ww.a14", 0x00000, 0x20000, 0x6a821ab9 )
	ROM_LOAD( "ww.a15", 0x20000, 0x20000, 0x2b1a0751 )
	ROM_LOAD( "ww.a16", 0x40000, 0x20000, 0xf6e190fe )

	ROM_REGION( 0x180000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ww.b1",  0x000000, 0x20000, 0xffa7d368 )
	ROM_LOAD( "ww.b5",  0x020000, 0x20000, 0x8d7794c1 )
	ROM_LOAD( "ww.b2",  0x040000, 0x20000, 0x0ed343f2 )
	ROM_LOAD( "ww.b6",  0x060000, 0x20000, 0x99458d58 )
	ROM_LOAD( "ww.b3",  0x080000, 0x20000, 0x3087104d )
	ROM_LOAD( "ww.b7",  0x0a0000, 0x20000, 0xabcf9bed )
	ROM_LOAD( "ww.b4",  0x0c0000, 0x20000, 0x41b6068b )
	ROM_LOAD( "ww.b8",  0x0e0000, 0x20000, 0x97eac164 )
	ROM_LOAD( "ww.a1",  0x100000, 0x20000, 0x260311c5 )
	ROM_LOAD( "ww.b10", 0x120000, 0x20000, 0x35a4b1b1 )
	ROM_LOAD( "ww.a2",  0x140000, 0x10000, 0x12e38a5c )
	ROM_LOAD( "ww.b11", 0x160000, 0x10000, 0xfa06fd24 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ww.a10", 0x0000, 0x08000, 0xc3609607 )
	ROM_LOAD( "ww.a11", 0x10000, 0x20000, 0xfb9a7f29 )
	ROM_LOAD( "ww.a12", 0x30000, 0x20000, 0xd6617b19 )
ROM_END

/***************************************************************************/

static READ_HANDLER( ww_io_service_r )
{
	return io_service_r(offset) | (READ_WORD(&sys16_workingram[0x2082]) & 0xff00);
}

static struct MemoryReadAddress wrestwar_readmem[] =
{

	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x100000, 0x10ffff, MRA_TILERAM },
	{ 0x110000, 0x110fff, MRA_TEXTRAM },
	{ 0x200000, 0x200fff, MRA_SPRITERAM },
	{ 0x300000, 0x300fff, MRA_PALETTERAM },
	{ 0x400000, 0x400003, MRA_EXTRAM },
	{ 0xc40000, 0xc40001, MRA_EXTRAM2 },
	{ 0xc41002, 0xc41003, io_player1_r },
	{ 0xc41006, 0xc41007, io_player2_r },
	{ 0xc42002, 0xc42003, io_dip1_r },
	{ 0xc42000, 0xc42001, io_dip2_r },
	{ 0xffe082, 0xffe083, ww_io_service_r },
	{ 0xffc000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress wrestwar_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x100000, 0x10ffff, MWA_TILERAM },
	{ 0x110000, 0x110fff, MWA_TEXTRAM },
	{ 0x200000, 0x200fff, MWA_SPRITERAM },
	{ 0x300000, 0x300fff, MWA_PALETTERAM },
	{ 0x400000, 0x400003, MWA_EXTRAM },
	{ 0xc40000, 0xc40001, MWA_EXTRAM2 },
	{ 0xc43034, 0xc43035, MWA_NOP },
	{ 0xffe08e, 0xffe08f, sound_command_w },
	{ 0xffc000, 0xffffff, MWA_WORKINGRAM },
	{-1}
};
/***************************************************************************/

static void wrestwar_update_proc( void ){
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );

	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );
	set_tile_bank( READ_WORD( &sys16_extraram[2] ) );
	set_refresh( READ_WORD( &sys16_extraram2[0] ) );
}

static void wrestwar_init_machine( void ){
	static int bank[16] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};

	sys16_obj_bank = bank;
	sys16_bg_priority_mode=2;
	sys16_bg_priority_value=0x0a00;

	sys16_update_proc = wrestwar_update_proc;
}

static void init_wrestwar( void ){
	sys16_onetime_init_machine();
	sys16_bg1_trans=1;
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=16;
#endif
	sys18_splittab_bg_y=&sys16_textram[0x0f40];
	sys18_splittab_fg_y=&sys16_textram[0x0f00];
	sys16_rowscroll_scroll=0x8000;

	sys16_sprite_decode( 6,0x40000 );
}
/***************************************************************************/

INPUT_PORTS_START( wrestwar )
	SYS16_JOY1
	SYS16_JOY2
	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Round Time" )
	PORT_DIPSETTING(    0x00, "100" )
	PORT_DIPSETTING(    0x0c, "110" )
	PORT_DIPSETTING(    0x08, "120" )
	PORT_DIPSETTING(    0x04, "130" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Continuation" )
	PORT_DIPSETTING(    0x20, "Continue" )
	PORT_DIPSETTING(    0x00, "No Continue" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

INPUT_PORTS_END

/***************************************************************************/

MACHINE_DRIVER_7759( machine_driver_wrestwar, \
	wrestwar_readmem,wrestwar_writemem,wrestwar_init_machine, gfx2,upd7759_interface )


/***************************************************************************/
/***************************************************************************/

/* hang-on's accel/brake are really both analog controls, but I've added them
as digital as well to see what works better */
#define HANGON_DIGITAL_CONTROLS

// hangon hardware
ROM_START( hangon )
	ROM_REGION( 0x020000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "6918.rom", 0x000000, 0x8000, 0x20b1c2b0 )
	ROM_LOAD_ODD ( "6916.rom", 0x000000, 0x8000, 0x7d9db1bf )
	ROM_LOAD_EVEN( "6917.rom", 0x010000, 0x8000, 0xfea12367 )
	ROM_LOAD_ODD ( "6915.rom", 0x010000, 0x8000, 0xac883240 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "6841.rom", 0x00000, 0x08000, 0x54d295dc )
	ROM_LOAD( "6842.rom", 0x08000, 0x08000, 0xf677b568 )
	ROM_LOAD( "6843.rom", 0x10000, 0x08000, 0xa257f0da )

	ROM_REGION( 0x080000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "6819.rom", 0x000000, 0x008000, 0x469dad07 )
	ROM_RELOAD(           0x070000, 0x008000 )	/* again? */
	ROM_LOAD( "6820.rom", 0x008000, 0x008000, 0x87cbc6de )
	ROM_RELOAD(           0x078000, 0x008000 )	/* again? */
	ROM_LOAD( "6821.rom", 0x010000, 0x008000, 0x15792969 )
	ROM_LOAD( "6822.rom", 0x018000, 0x008000, 0xe9718de5 )
	ROM_LOAD( "6823.rom", 0x020000, 0x008000, 0x49422691 )
	ROM_LOAD( "6824.rom", 0x028000, 0x008000, 0x701deaa4 )
	ROM_LOAD( "6825.rom", 0x030000, 0x008000, 0x6e23c8b4 )
	ROM_LOAD( "6826.rom", 0x038000, 0x008000, 0x77d0de2c )
	ROM_LOAD( "6827.rom", 0x040000, 0x008000, 0x7fa1bfb6 )
	ROM_LOAD( "6828.rom", 0x048000, 0x008000, 0x8e880c93 )
	ROM_LOAD( "6829.rom", 0x050000, 0x008000, 0x7ca0952d )
	ROM_LOAD( "6830.rom", 0x058000, 0x008000, 0xb1a63aef )
	ROM_LOAD( "6845.rom", 0x060000, 0x008000, 0xba08c9b8 )
	ROM_LOAD( "6846.rom", 0x068000, 0x008000, 0xf21e57a3 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "6833.rom", 0x00000, 0x4000, 0x3b942f5f )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "6831.rom", 0x00000, 0x8000, 0xcfef5481 )
	ROM_LOAD( "6832.rom", 0x08000, 0x8000, 0x4165aea5 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("6920.rom", 0x0000, 0x8000, 0x1c95013e )
	ROM_LOAD_ODD( "6919.rom", 0x0000, 0x8000, 0x6ca30d69 )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "6840.rom", 0x0000, 0x8000, 0x581230e3 )
ROM_END

/***************************************************************************/


static READ_HANDLER( ho_io_x_r ){ return input_port_0_r( offset ); }
#ifdef HANGON_DIGITAL_CONTROLS
static READ_HANDLER( ho_io_y_r ){
	int data = input_port_1_r( offset );

	switch(data & 3)
	{
		case 3:	return 0xffff;	// both
		case 2:	return 0x00ff;  // brake
		case 1:	return 0xff00;  // accel
		case 0:	return 0x0000;  // neither
	}
	return 0x0000;
}
#else
static READ_HANDLER( ho_io_y_r ){ return (input_port_1_r( offset ) << 8) + input_port_5_r( offset ); }
#endif

static READ_HANDLER( ho_io_highscoreentry_r )
{
	int mode=READ_WORD(&sys16_extraram4[0x3000]);

	if(mode&4)
	{	// brake
		if(ho_io_y_r(0) & 0x00ff) return 0xffff;
	}
	else if(mode&8)
	{
		// button
		if(ho_io_y_r(0) & 0xff00) return 0xffff;
	}
	return 0;
}

static READ_HANDLER( hangon1_skip_r )
{
	if (cpu_get_pc()==0x17e6) {cpu_spinuntil_int(); return 0xffff;}

//	return READ_WORD(&sys16_extraram[0xc400]);
	return READ_WORD(&sys16_extraram[0x0400]);
}


static struct MemoryReadAddress hangon_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x20c400, 0x20c401, hangon1_skip_r },
	{ 0x20c000, 0x20ffff, MRA_EXTRAM },
	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x600000, 0x600fff, MRA_SPRITERAM },
	{ 0xa00000, 0xa00fff, MRA_PALETTERAM },
	{ 0xc68000, 0xc68fff, MRA_EXTRAM2 },
	{ 0xc7e000, 0xc7ffff, MRA_EXTRAM3 },
	{ 0xe01000, 0xe01001, io_service_r },
	{ 0xe0100c, 0xe0100d, io_dip2_r },
	{ 0xe0100a, 0xe0100b, io_dip1_r },
	{ 0xe03020, 0xe03021, ho_io_highscoreentry_r },
	{ 0xe03028, 0xe03029, ho_io_x_r },
	{ 0xe0302a, 0xe0302b, ho_io_y_r },
	{ 0xe00000, 0xe03fff, MRA_EXTRAM4 },
	{-1}
};

static struct MemoryWriteAddress hangon_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x20c000, 0x20ffff, MWA_EXTRAM },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x600000, 0x600fff, MWA_SPRITERAM },
	{ 0xa00000, 0xa00fff, MWA_PALETTERAM },
	{ 0xc68000, 0xc68fff, MWA_EXTRAM2 },
	{ 0xc7e000, 0xc7ffff, MWA_EXTRAM3 },
	{ 0xe00000, 0xe00001, sound_command_nmi_w },
	{ 0xe00000, 0xe03fff, MWA_EXTRAM4 },
	{-1}
};

static READ_HANDLER( hangon2_skip_r )
{
	if (cpu_get_pc()==0xf66) {cpu_spinuntil_int(); return 0xffff;}

//	return READ_WORD(&sys16_extraram2[0x3f000]);
	return READ_WORD(&sys16_extraram3[0x01000]);
}

static struct MemoryReadAddress hangon_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xc7f000, 0xc7f001, hangon2_skip_r },
	{ 0xc68000, 0xc68fff, MRA_EXTRAM2 },
	{ 0xc7e000, 0xc7ffff, MRA_EXTRAM3 },
	{-1}
};

static struct MemoryWriteAddress hangon_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xc68000, 0xc68fff, MWA_EXTRAM2 },
	{ 0xc7e000, 0xc7ffff, MWA_EXTRAM3 },
	{-1}
};


static struct MemoryReadAddress hangon_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd000, YM2203_status_port_0_r },
	{ 0xe000, 0xe7ff, SegaPCM_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress hangon_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xd000, 0xd000, YM2203_control_port_0_w },
	{ 0xd001, 0xd001, YM2203_write_port_0_w },
	{ 0xe000, 0xe7ff, SegaPCM_w },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort hangon_sound_readport[] =
{
	{ 0x40, 0x40, soundlatch_r },
	{ -1 }	/* end of table */
};


static struct IOWritePort hangon_sound_writeport[] =
{
	{ -1 }
};

/***************************************************************************/

static void hangon_update_proc( void ){
	int leds;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	set_fg_page1( READ_WORD( &sys16_textram[0x0e9e] ) );
	set_bg_page1( READ_WORD( &sys16_textram[0x0e9c] ) );
	set_refresh_3d( READ_WORD( &sys16_extraram4[0x2] ) );

	leds=READ_WORD( &sys16_extraram4[0x2] );
	if(leds & 4)
	{
		osd_led_w(0,1);
		osd_led_w(1,1);
		osd_led_w(2,1);
	}
	else
	{
		osd_led_w(0,0);
		osd_led_w(1,0);
		osd_led_w(2,0);
	}

}

static void hangon_init_machine( void ){
	static int bank[16] = { 00,01,02,03,04,05,06,00,01,02,03,04,05,06,00,06};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 5;
	sys16_sprxoffset = -0xc0;
	sys16_fgxoffset = 8;
	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;

	patch_code( 0x83bd, 0x29);
	patch_code( 0x8495, 0x2a);
	patch_code( 0x84f9, 0x2b);

	sys16_update_proc = hangon_update_proc;

	gr_ver = &sys16_extraram2[0x0];
	gr_hor = gr_ver+0x200;
	gr_pal = gr_ver+0x400;
	gr_flip= gr_ver+0x600;
	gr_palette= 0xf80 / 2;
	gr_palette_default = 0x70 /2;
	gr_colorflip[0][0]=0x08 / 2;
	gr_colorflip[0][1]=0x04 / 2;
	gr_colorflip[0][2]=0x00 / 2;
	gr_colorflip[0][3]=0x06 / 2;
	gr_colorflip[1][0]=0x0a / 2;
	gr_colorflip[1][1]=0x04 / 2;
	gr_colorflip[1][2]=0x02 / 2;
	gr_colorflip[1][3]=0x02 / 2;
}



static void init_hangon( void )
{
	sys16_onetime_init_machine();

	sys16_sprite_decode( 8,0x010000 );
	generate_gr_screen(512,1024,8,0,4,0x8000);
}
/***************************************************************************/

INPUT_PORTS_START( hangon )
PORT_START	/* Steering */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_REVERSE | IPF_CENTER , 100, 3, 0x48, 0xb7 )

#ifdef HANGON_DIGITAL_CONTROLS

PORT_START	/* Buttons */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )

#else

PORT_START	/* Accel / Decel */
	PORT_ANALOG( 0xff, 0x1, IPT_AD_STICK_Y | IPF_CENTER | IPF_REVERSE, 100, 16, 0, 0xa2 )

#endif

	SYS16_SERVICE
	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x06, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x18, 0x18, "Time Adj." )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Medium" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, "Play Music" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )

#ifndef HANGON_DIGITAL_CONTROLS

PORT_START	/* Brake */
	PORT_ANALOG( 0xff, 0x1, IPT_AD_STICK_Y | IPF_PLAYER2 | IPF_CENTER | IPF_REVERSE, 100, 16, 0, 0xa2 )

#endif
INPUT_PORTS_END

/***************************************************************************/

static struct MachineDriver machine_driver_hangon =
{
	{
		{
			CPU_M68000,
			10000000,
			hangon_readmem,hangon_writemem,0,0,
			sys16_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4096000,
			hangon_sound_readmem,hangon_sound_writemem,hangon_sound_readport,hangon_sound_writeport,
//			ignore_interrupt,1
			interrupt,4
		},
		{
			CPU_M68000,
			10000000,
			hangon_readmem2,hangon_writemem2,0,0,
			sys16_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	hangon_init_machine,
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	gfx8,
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sys16_ho_vh_start,
	sys16_vh_stop,
	sys16_ho_vh_screenrefresh,
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{			// wrong sound chip??
			SOUND_SEGAPCM,
			&segapcm_interface_32k,
		}
	}
};


/***************************************************************************/
// space harrier / enduro racer hardware
ROM_START( sharrier )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ic97.bin", 0x000000, 0x8000, 0x7c30a036 )
	ROM_LOAD_ODD ( "ic84.bin", 0x000000, 0x8000, 0x16deaeb1 )
	ROM_LOAD_EVEN( "ic98.bin", 0x010000, 0x8000, 0x40b1309f )
	ROM_LOAD_ODD ( "ic85.bin", 0x010000, 0x8000, 0xce78045c )
	ROM_LOAD_EVEN( "ic99.bin", 0x020000, 0x8000, 0xf6391091 )
	ROM_LOAD_ODD ( "ic86.bin", 0x020000, 0x8000, 0x79b367d7 )
	ROM_LOAD_EVEN( "ic100.bin", 0x030000, 0x8000, 0x6171e9d3 )
	ROM_LOAD_ODD ( "ic87.bin", 0x030000, 0x8000, 0x70cb72ef )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "sic31.bin", 0x00000, 0x08000, 0x347fa325 )
	ROM_LOAD( "sic46.bin", 0x08000, 0x08000, 0x39d98bd1 )
	ROM_LOAD( "sic60.bin", 0x10000, 0x08000, 0x3da3ea6b )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ic36.bin", 0x000000, 0x008000, 0x93e2d264 )
	ROM_LOAD( "ic28.bin", 0x008000, 0x008000, 0xedbf5fc3 )
	ROM_LOAD( "ic118.bin",0x010000, 0x008000, 0xe8c537d8 )
	ROM_LOAD( "ic8.bin",  0x018000, 0x008000, 0x22844fa4 )

	ROM_LOAD( "ic35.bin", 0x020000, 0x008000, 0xcd6e7500 )
	ROM_LOAD( "ic27.bin", 0x028000, 0x008000, 0x41f25a9c )
	ROM_LOAD( "ic17.bin", 0x030000, 0x008000, 0x5bb09a67 )
	ROM_LOAD( "ic7.bin",  0x038000, 0x008000, 0xdcaa2ebf )

	ROM_LOAD( "ic34.bin", 0x040000, 0x008000, 0xd5e15e66 )
	ROM_LOAD( "ic26.bin", 0x048000, 0x008000, 0xac62ae2e )
	ROM_LOAD( "ic16.bin", 0x050000, 0x008000, 0x9c782295 )
	ROM_LOAD( "ic6.bin",  0x058000, 0x008000, 0x3711105c )

	ROM_LOAD( "ic33.bin", 0x060000, 0x008000, 0x60d7c1bb )
	ROM_LOAD( "ic25.bin", 0x068000, 0x008000, 0xf6330038 )
	ROM_LOAD( "ic15.bin", 0x070000, 0x008000, 0x60737b98 )
	ROM_LOAD( "ic5.bin",  0x078000, 0x008000, 0x70fb5ebb )

	ROM_LOAD( "ic32.bin", 0x080000, 0x008000, 0x6d7b5c97 )
	ROM_LOAD( "ic24.bin", 0x088000, 0x008000, 0xcebf797c )
	ROM_LOAD( "ic14.bin", 0x090000, 0x008000, 0x24596a8b )
	ROM_LOAD( "ic4.bin",  0x098000, 0x008000, 0xb537d082 )

	ROM_LOAD( "ic31.bin", 0x0a0000, 0x008000, 0x5e784271 )
	ROM_LOAD( "ic23.bin", 0x0a8000, 0x008000, 0x510e5e10 )
	ROM_LOAD( "ic13.bin", 0x0b0000, 0x008000, 0x7a2dad15 )
	ROM_LOAD( "ic3.bin",  0x0b8000, 0x008000, 0xf5ba4e08 )

	ROM_LOAD( "ic30.bin", 0x0c0000, 0x008000, 0xec42c9ef )
	ROM_LOAD( "ic22.bin", 0x0c8000, 0x008000, 0x6d4a7d7a )
	ROM_LOAD( "ic12.bin", 0x0d0000, 0x008000, 0x0f732717 )
	ROM_LOAD( "ic2.bin",  0x0d8000, 0x008000, 0xfc3bf8f3 )

	ROM_LOAD( "ic29.bin", 0x0e0000, 0x008000, 0xed51fdc4 )
	ROM_LOAD( "ic21.bin", 0x0e8000, 0x008000, 0xdfe75f3d )
	ROM_LOAD( "ic11.bin", 0x0f0000, 0x008000, 0xa2c07741 )
	ROM_LOAD( "ic1.bin",  0x0f8000, 0x008000, 0xb191e22f )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ic73.bin", 0x00000, 0x004000, 0xd6397933 )
	ROM_LOAD( "ic72.bin", 0x04000, 0x004000, 0x504e76d9 )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "snd7231.256", 0x00000, 0x008000, 0x871c6b14 )
	ROM_LOAD( "snd7232.256", 0x08000, 0x008000, 0x4b59340c )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("ic54.bin", 0x0000, 0x8000, 0xd7c535b6 )
	ROM_LOAD_ODD( "ic67.bin", 0x0000, 0x8000, 0xa6153af8 )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "pic2.bin", 0x0000, 0x8000, 0xb4740419 )
ROM_END

/***************************************************************************/

static READ_HANDLER( sh_io_joy_r ){ return (input_port_5_r( offset ) << 8) + input_port_6_r( offset ); }

static unsigned char *shared_ram;
static READ_HANDLER( shared_ram_r ) { return READ_WORD(&shared_ram[offset]); }
static WRITE_HANDLER( shared_ram_w ) { COMBINE_WORD_MEM(&shared_ram[offset], data); }

static READ_HANDLER( sh_motor_status_r ) { return 0x0; }

static struct MemoryReadAddress harrier_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x040000, 0x043fff, MRA_EXTRAM },
	{ 0x100000, 0x107fff, MRA_TILERAM },
	{ 0x108000, 0x108fff, MRA_TEXTRAM },
	{ 0x110000, 0x110fff, MRA_PALETTERAM },
	{ 0x124000, 0x127fff, shared_ram_r },
	{ 0x130000, 0x130fff, MRA_SPRITERAM },
	{ 0x140010, 0x140011, io_service_r },
	{ 0x140014, 0x140015, io_dip1_r },
	{ 0x140016, 0x140017, io_dip2_r },
	{ 0x140024, 0x140027, sh_motor_status_r },
	{ 0x140000, 0x140027, MRA_EXTRAM3 },		//io
	{ 0xc68000, 0xc68fff, MRA_EXTRAM2 },

	{-1}
};

static struct MemoryWriteAddress harrier_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x040000, 0x043fff, MWA_EXTRAM },
	{ 0x100000, 0x107fff, MWA_TILERAM },
	{ 0x108000, 0x108fff, MWA_TEXTRAM },
	{ 0x110000, 0x110fff, MWA_PALETTERAM },
	{ 0x124000, 0x127fff, shared_ram_w, &shared_ram },
	{ 0x130000, 0x130fff, MWA_SPRITERAM },
	{ 0x140000, 0x140001, sound_command_nmi_w },
	{ 0x140000, 0x140027, MWA_EXTRAM3 },		//io
	{ 0xc68000, 0xc68fff, MWA_EXTRAM2 },

	{-1}
};

static struct MemoryReadAddress harrier_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xc68000, 0xc68fff, MRA_EXTRAM2 },
	{ 0xc7c000, 0xc7ffff, shared_ram_r },
	{-1}
};

static struct MemoryWriteAddress harrier_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xc68000, 0xc68fff, MWA_EXTRAM2 },
	{ 0xc7c000, 0xc7ffff, shared_ram_w, &shared_ram },
	{-1}
};

static struct MemoryReadAddress harrier_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xd000, 0xd000, YM2203_status_port_0_r },
	{ 0xe000, 0xe0ff, SegaPCM_r },
	{ 0x8000, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress harrier_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xd000, 0xd000, YM2203_control_port_0_w },
	{ 0xd001, 0xd001, YM2203_write_port_0_w },
	{ 0xe000, 0xe0ff, SegaPCM_w },
	{ 0x8000, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort harrier_sound_readport[] =
{
	{ 0x40, 0x40, soundlatch_r },
	{ -1 }	/* end of table */
};


static struct IOWritePort harrier_sound_writeport[] =
{
	{ -1 }
};

/***************************************************************************/

static void harrier_update_proc( void ){
	int data;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x01ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	data = READ_WORD( &sys16_textram[0x0e9e] );

	sys16_fg_page[0] = data>>12;
	sys16_fg_page[1] = (data>>8)&0xf;
	sys16_fg_page[3] = (data>>4)&0xf;
	sys16_fg_page[2] = data&0xf;

	data = READ_WORD( &sys16_textram[0x0e9c] );
	sys16_bg_page[0] = data>>12;
	sys16_bg_page[1] = (data>>8)&0xf;
	sys16_bg_page[3] = (data>>4)&0xf;
	sys16_bg_page[2] = data&0xf;

	WRITE_WORD(&sys16_extraram[0x492],sh_io_joy_r(0));

	data=READ_WORD( &sys16_extraram3[2] );
	set_refresh_3d( data );

	if(data & 8)
	{
		osd_led_w(0,1);
		osd_led_w(2,1);
	}
	else
	{
		osd_led_w(0,0);
		osd_led_w(2,0);
	}
	if(data & 4)
		osd_led_w(1,1);
	else
		osd_led_w(1,0);

}

static void harrier_init_machine( void ){
	static int bank[16] = { 00,01,02,03,04,05,06,07,00,00,00,00,00,00,00,00};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 6;
	sys16_sprxoffset = -0xc0;
	sys16_fgxoffset = 8;
	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;


//*disable illegal rom writes
	patch_code( 0x8112, 0x4a);
	patch_code( 0x83d2, 0x4a);
	patch_code( 0x83d6, 0x4a);
	patch_code( 0x82c4, 0x4a);
	patch_code( 0x82c8, 0x4a);
	patch_code( 0x84d0, 0x4a);
	patch_code( 0x84d4, 0x4a);
	patch_code( 0x85de, 0x4a);
	patch_code( 0x85e2, 0x4a);

	sys16_update_proc = harrier_update_proc;

	gr_ver = &sys16_extraram2[0x0];
	gr_hor = gr_ver+0x200;
	gr_pal = gr_ver+0x400;
	gr_flip= gr_ver+0x600;
	gr_palette= 0xf80 / 2;
	gr_palette_default = 0x70 /2;
	gr_colorflip[0][0]=0x00 / 2;
	gr_colorflip[0][1]=0x02 / 2;
	gr_colorflip[0][2]=0x04 / 2;
	gr_colorflip[0][3]=0x00 / 2;
	gr_colorflip[1][0]=0x00 / 2;
	gr_colorflip[1][1]=0x00 / 2;
	gr_colorflip[1][2]=0x06 / 2;
	gr_colorflip[1][3]=0x00 / 2;

#ifdef TRANSPARENT_SHADOWS
	sys16_sh_shadowpal=0;
#endif
}

static void init_sharrier( void )
{
	sys16_onetime_init_machine();
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=NumOfShadowColors / 2;
#endif

#ifdef SPACEHARRIER_OFFSETS
	spaceharrier_patternoffsets=malloc(65536);

	memset(spaceharrier_patternoffsets,0x7f,65535);
	spaceharrier_patternoffsets[0x2124] = 0; // small shadow
	spaceharrier_patternoffsets[0x2429] = 4; // ice berg of round 7
	spaceharrier_patternoffsets[0x211b] = 1; // small flying rock
	spaceharrier_patternoffsets[0x515b] = 0; // small flying ball
	spaceharrier_patternoffsets[0x611f] = 0; // small ceiling ball
	spaceharrier_patternoffsets[0x624a] = 1; // small ceiling ball
	spaceharrier_patternoffsets[0x5785] = 1; // 3 poses of the Harrier on the title screen
	spaceharrier_patternoffsets[0x5771] = 1; // these are the only patterns which do not need
	spaceharrier_patternoffsets[0x579a] = 1; // position compensations
	spaceharrier_patternoffsets[0x06f3] = 0; // missiles
	spaceharrier_patternoffsets[0x0735] = 0;
#endif

	sys16_sprite_decode2( 8,0x020000 ,1);
	generate_gr_screen(512,512,0,0,4,0x8000);
}
/***************************************************************************/

INPUT_PORTS_START( sharrier )
	SYS16_JOY1
	SYS16_JOY2

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, "Moving" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x10, 0x10, "Add Player Score" )
	PORT_DIPSETTING(    0x10, "5000000" )
	PORT_DIPSETTING(    0x00, "7000000" )
	PORT_DIPNAME( 0x20, 0x20, "Trial Time" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )


PORT_START	/* X */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X |  IPF_REVERSE, 100, 4, 0x20, 0xdf )

PORT_START	/* Y */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y |  IPF_REVERSE, 100, 4, 0x60, 0x9f )

INPUT_PORTS_END

/***************************************************************************/

static struct MachineDriver machine_driver_sharrier =
{
	{
		{
			CPU_M68000,
			10000000,
			harrier_readmem,harrier_writemem,0,0,
			sys16_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4096000,
			harrier_sound_readmem,harrier_sound_writemem,harrier_sound_readport,harrier_sound_writeport,
//			ignore_interrupt,1
			interrupt,4
		},
		{
			CPU_M68000,
			10000000,
			harrier_readmem2,harrier_writemem2,0,0,
			sys16_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	harrier_init_machine,
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	gfx8,
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sys16_ho_vh_start,
	sys16_vh_stop,
	sys16_ho_vh_screenrefresh,
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_SEGAPCM,
			&segapcm_interface_32k,
		}
	}
};

/***************************************************************************/

/* hang-on's accel/brake are really both analog controls, but I've added them
as digital as well to see what works better */

// hangon hardware
ROM_START( shangon )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code - protected */
	ROM_LOAD_EVEN( "ic133", 0x000000, 0x10000, 0xe52721fe )
	ROM_LOAD_ODD ( "ic118", 0x000000, 0x10000, 0x5fee09f6 )
	ROM_LOAD_EVEN( "ic132", 0x020000, 0x10000, 0x5d55d65f )
	ROM_LOAD_ODD ( "ic117", 0x020000, 0x10000, 0xb967e8c3 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ic54",        0x00000, 0x08000, 0x260286f9 )
	ROM_LOAD( "ic55",        0x08000, 0x08000, 0xc609ee7b )
	ROM_LOAD( "ic56",        0x10000, 0x08000, 0xb236a403 )

	ROM_REGION( 0x0120000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ic8",         0x000000, 0x010000, 0xd6ac012b )
	ROM_RELOAD(              0x100000, 0x010000 )	// twice?
	ROM_LOAD( "ic16",        0x010000, 0x010000, 0xd9d83250 )
	ROM_RELOAD(              0x110000, 0x010000 )	// twice?
	ROM_LOAD( "ic7",         0x020000, 0x010000, 0x25ebf2c5 )
	ROM_RELOAD(              0x0e0000, 0x010000 )	// twice?
	ROM_LOAD( "ic15",        0x030000, 0x010000, 0x6365d2e9 )
	ROM_RELOAD(              0x0f0000, 0x010000 )	// twice?
	ROM_LOAD( "ic6",         0x040000, 0x010000, 0x8a57b8d6 )
	ROM_LOAD( "ic14",        0x050000, 0x010000, 0x3aff8910 )
	ROM_LOAD( "ic5",         0x060000, 0x010000, 0xaf473098 )
	ROM_LOAD( "ic13",        0x070000, 0x010000, 0x80bafeef )
	ROM_LOAD( "ic4",         0x080000, 0x010000, 0x03bc4878 )
	ROM_LOAD( "ic12",        0x090000, 0x010000, 0x274b734e )
	ROM_LOAD( "ic3",         0x0a0000, 0x010000, 0x9f0677ed )
	ROM_LOAD( "ic11",        0x0b0000, 0x010000, 0x508a4701 )
	ROM_LOAD( "ic2",         0x0c0000, 0x010000, 0xb176ea72 )
	ROM_LOAD( "ic10",        0x0d0000, 0x010000, 0x42fcd51d )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "ic88", 0x0000, 0x08000, 0x1254efa6 )

	ROM_LOAD( "ic66", 0x10000, 0x08000, 0x06f55364 )
	ROM_LOAD( "ic67", 0x18000, 0x08000, 0x731f5cf8 )
	ROM_LOAD( "ic68", 0x20000, 0x08000, 0xa60dabff )
	ROM_LOAD( "ic69", 0x28000, 0x08000, 0x473cc411 )

	ROM_REGION( 0x40000, REGION_CPU3 ) /* second 68000 CPU  - protected */
	ROM_LOAD_EVEN( "ic76", 0x0000, 0x10000, 0x02be68db )
	ROM_LOAD_ODD ( "ic58", 0x0000, 0x10000, 0xf13e8bee )
	ROM_LOAD_EVEN( "ic75", 0x20000, 0x10000, 0x1627c224 )
	ROM_LOAD_ODD ( "ic57", 0x20000, 0x10000, 0x8cdbcde8 )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "ic47", 0x0000, 0x8000, 0x7836bcc3 )
ROM_END

ROM_START( shangonb )
	ROM_REGION( 0x030000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "s-hangon.30", 0x000000, 0x10000, 0xd95e82fc )
	ROM_LOAD_ODD ( "s-hangon.32", 0x000000, 0x10000, 0x2ee4b4fb )
	ROM_LOAD_EVEN( "s-hangon.29", 0x020000, 0x8000, 0x12ee8716 )
	ROM_LOAD_ODD ( "s-hangon.31", 0x020000, 0x8000, 0x155e0cfd )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "ic54",        0x00000, 0x08000, 0x260286f9 )
	ROM_LOAD( "ic55",        0x08000, 0x08000, 0xc609ee7b )
	ROM_LOAD( "ic56",        0x10000, 0x08000, 0xb236a403 )

	ROM_REGION( 0x0120000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "ic8",         0x000000, 0x010000, 0xd6ac012b )
	ROM_RELOAD(              0x100000, 0x010000 )	// twice?
	ROM_LOAD( "ic16",        0x010000, 0x010000, 0xd9d83250 )
	ROM_RELOAD(              0x110000, 0x010000 )	// twice?
	ROM_LOAD( "s-hangon.20", 0x020000, 0x010000, 0xeef23b3d )
	ROM_RELOAD(              0x0e0000, 0x010000 )	// twice?
	ROM_LOAD( "s-hangon.14", 0x030000, 0x010000, 0x0f26d131 )
	ROM_RELOAD(              0x0f0000, 0x010000 )	// twice?
	ROM_LOAD( "ic6",         0x040000, 0x010000, 0x8a57b8d6 )
	ROM_LOAD( "ic14",        0x050000, 0x010000, 0x3aff8910 )
	ROM_LOAD( "ic5",         0x060000, 0x010000, 0xaf473098 )
	ROM_LOAD( "ic13",        0x070000, 0x010000, 0x80bafeef )
	ROM_LOAD( "ic4",         0x080000, 0x010000, 0x03bc4878 )
	ROM_LOAD( "ic12",        0x090000, 0x010000, 0x274b734e )
	ROM_LOAD( "ic3",         0x0a0000, 0x010000, 0x9f0677ed )
	ROM_LOAD( "ic11",        0x0b0000, 0x010000, 0x508a4701 )
	ROM_LOAD( "ic2",         0x0c0000, 0x010000, 0xb176ea72 )
	ROM_LOAD( "ic10",        0x0d0000, 0x010000, 0x42fcd51d )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "s-hangon.03", 0x0000, 0x08000, 0x83347dc0 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "s-hangon.02", 0x00000, 0x10000, 0xda08ca2b )
	ROM_LOAD( "s-hangon.01", 0x10000, 0x10000, 0x8b10e601 )

	ROM_REGION( 0x40000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("s-hangon.09", 0x0000, 0x10000, 0x070c8059 )
	ROM_LOAD_ODD( "s-hangon.05", 0x0000, 0x10000, 0x9916c54b )
	ROM_LOAD_EVEN("s-hangon.08", 0x20000, 0x10000, 0x000ad595 )
	ROM_LOAD_ODD( "s-hangon.04", 0x20000, 0x10000, 0x8f8f4af0 )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "s-hangon.26", 0x0000, 0x8000, 0x1bbe4fc8 )
ROM_END


/***************************************************************************/

static unsigned char *shared_ram2;
static READ_HANDLER( shared_ram2_r ) { return READ_WORD(&shared_ram2[offset]); }
static WRITE_HANDLER( shared_ram2_w ) { COMBINE_WORD_MEM(&shared_ram2[offset], data); }

static struct MemoryReadAddress shangon_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x20c640, 0x20c647, sound_shared_ram_r },
	{ 0x20c000, 0x20ffff, MRA_EXTRAM5 },

	{ 0x400000, 0x40ffff, MRA_TILERAM },
	{ 0x410000, 0x410fff, MRA_TEXTRAM },
	{ 0x600000, 0x600fff, MRA_SPRITERAM },
	{ 0xa00000, 0xa00fff, MRA_PALETTERAM },
	{ 0xc68000, 0xc68fff, shared_ram_r },
	{ 0xc7c000, 0xc7ffff, shared_ram2_r },
	{ 0xe01000, 0xe01001, io_service_r },
	{ 0xe0100c, 0xe0100d, io_dip2_r },
	{ 0xe0100a, 0xe0100b, io_dip1_r },
	{ 0xe030f8, 0xe030f9, ho_io_x_r },
	{ 0xe030fa, 0xe030fb, ho_io_y_r },
	{ 0xe00000, 0xe03fff, MRA_EXTRAM4 },	// io
	{-1}
};

static struct MemoryWriteAddress shangon_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x20c640, 0x20c647, sound_shared_ram_w },
	{ 0x20c000, 0x20ffff, MWA_EXTRAM5 },
	{ 0x400000, 0x40ffff, MWA_TILERAM },
	{ 0x410000, 0x410fff, MWA_TEXTRAM },
	{ 0x600000, 0x600fff, MWA_SPRITERAM },
	{ 0xa00000, 0xa00fff, MWA_PALETTERAM },
	{ 0xc68000, 0xc68fff, shared_ram_w, &shared_ram },
	{ 0xc7c000, 0xc7ffff, shared_ram2_w, &shared_ram2 },
	{ 0xe00000, 0xe03fff, MWA_EXTRAM4 },	// io
	{-1}
};

static struct MemoryReadAddress shangon_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x454000, 0x45401f, MRA_EXTRAM3 },
	{ 0x7e8000, 0x7e8fff, shared_ram_r },
	{ 0x7fc000, 0x7ffbff, shared_ram2_r },
	{ 0x7ffc00, 0x7fffff, MRA_EXTRAM },
	{-1}
};

static struct MemoryWriteAddress shangon_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x454000, 0x45401f, MWA_EXTRAM3 },
	{ 0x7e8000, 0x7e8fff, shared_ram_w },
	{ 0x7fc000, 0x7ffbff, shared_ram2_w },
	{ 0x7ffc00, 0x7fffff, MWA_EXTRAM },
	{-1}
};

static struct MemoryReadAddress shangon_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xf7ff, SegaPCM_r },
	{ 0xf800, 0xf807, sound2_shared_ram_r },
	{ 0xf808, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress shangon_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xf000, 0xf7ff, SegaPCM_w },
	{ 0xf800, 0xf807, sound2_shared_ram_w,&sound_shared_ram },
	{ 0xf808, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

/***************************************************************************/

static void shangon_update_proc( void ){
	int leds;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x00ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	set_fg_page1( READ_WORD( &sys16_textram[0x0e9e] ) );
	set_bg_page1( READ_WORD( &sys16_textram[0x0e9c] ) );

	set_refresh_3d( READ_WORD( &sys16_extraram4[2] ) );

	leds=READ_WORD( &sys16_extraram4[0x2] );

	if(leds & 4)
	{
		osd_led_w(0,1);
		osd_led_w(1,1);
		osd_led_w(2,1);
	}
	else
	{
		osd_led_w(0,0);
		osd_led_w(1,0);
		osd_led_w(2,0);
	}

}

static void shangon_init_machine( void ){
	static int bank[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 5;
	sys16_sprxoffset = -0xc0;
	sys16_fgxoffset = 8;
	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;

	patch_code( 0x65bd, 0xf9);
	patch_code( 0x6677, 0xfa);
	patch_code( 0x66d5, 0xfb);
	patch_code( 0x9621, 0xfb);

	sys16_update_proc = shangon_update_proc;

	gr_ver = &shared_ram[0x0];
	gr_hor = gr_ver+0x200;
	gr_pal = gr_ver+0x400;
	gr_flip= gr_ver+0x600;
	gr_palette= 0xf80 / 2;
	gr_palette_default = 0x70 /2;
	gr_colorflip[0][0]=0x08 / 2;
	gr_colorflip[0][1]=0x04 / 2;
	gr_colorflip[0][2]=0x00 / 2;
	gr_colorflip[0][3]=0x06 / 2;
	gr_colorflip[1][0]=0x0a / 2;
	gr_colorflip[1][1]=0x04 / 2;
	gr_colorflip[1][2]=0x02 / 2;
	gr_colorflip[1][3]=0x02 / 2;
}



static void init_shangon( void )
{
	sys16_onetime_init_machine();

	sys16_sprite_decode( 9,0x020000 );
	generate_gr_screen(512,1024,0,0,4,0x8000);
	//??
	patch_z80code( 0x1087, 0x20);
	patch_z80code( 0x1088, 0x01);
}

static void init_shangonb( void )
{
	sys16_onetime_init_machine();

	sys16_sprite_decode( 9,0x020000 );
	generate_gr_screen(512,1024,8,0,4,0x8000);
}
/***************************************************************************/

INPUT_PORTS_START( shangon )
PORT_START	/* Steering */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_REVERSE | IPF_CENTER , 100, 3, 0x42, 0xbd )

#ifdef HANGON_DIGITAL_CONTROLS

PORT_START	/* Buttons */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )

#else

PORT_START	/* Accel / Decel */
	PORT_ANALOG( 0xff, 0x1, IPT_AD_STICK_Y | IPF_CENTER | IPF_REVERSE, 100, 16, 1, 0xa2 )

#endif

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x06, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x18, 0x18, "Time Adj." )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, "Play Music" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )


#ifndef HANGON_DIGITAL_CONTROLS

PORT_START	/* Brake */
	PORT_ANALOG( 0xff, 0x1, IPT_AD_STICK_Y | IPF_PLAYER2 | IPF_CENTER | IPF_REVERSE, 100, 16, 1, 0xa2 )

#endif
INPUT_PORTS_END

/***************************************************************************/
static struct MachineDriver machine_driver_shangon =
{
	{
		{
			CPU_M68000,
			10000000,
			shangon_readmem,shangon_writemem,0,0,
			sys16_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4096000,
			shangon_sound_readmem,shangon_sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1
		},
		{
			CPU_M68000,
			10000000,
			shangon_readmem2,shangon_writemem2,0,0,
			sys16_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	shangon_init_machine,
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	gfx8,
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sys16_ho_vh_start,
	sys16_vh_stop,
	sys16_ho_vh_screenrefresh,
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_SEGAPCM,
			&segapcm_interface_15k_512,
		}
	}
};

/***************************************************************************/
// Outrun hardware
ROM_START( outrun )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "10380a", 0x000000, 0x10000, 0x434fadbc )
	ROM_LOAD_ODD ( "10382a", 0x000000, 0x10000, 0x1ddcc04e )
	ROM_LOAD_EVEN( "10381a", 0x020000, 0x10000, 0xbe8c412b )
	ROM_LOAD_ODD ( "10383a", 0x020000, 0x10000, 0xdcc586e7 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10268", 0x00000, 0x08000, 0x95344b04 )
	ROM_LOAD( "10232", 0x08000, 0x08000, 0x776ba1eb )
	ROM_LOAD( "10267", 0x10000, 0x08000, 0xa85bb823 )
	ROM_LOAD( "10231", 0x18000, 0x08000, 0x8908bcbf )
	ROM_LOAD( "10266", 0x20000, 0x08000, 0x9f6f1a74 )
	ROM_LOAD( "10230", 0x28000, 0x08000, 0x686f5e50 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10371", 0x000000, 0x010000, 0x0a1c98de )
	ROM_CONTINUE(      0x080000, 0x010000 )
	ROM_LOAD( "10373", 0x010000, 0x010000, 0x339f8e64 )
	ROM_CONTINUE(      0x090000, 0x010000 )
	ROM_LOAD( "10375", 0x020000, 0x010000, 0x62a472bd )
	ROM_CONTINUE(      0x0a0000, 0x010000 )
	ROM_LOAD( "10377", 0x030000, 0x010000, 0xc86daecb )
	ROM_CONTINUE(      0x0b0000, 0x010000 )

	ROM_LOAD( "10372", 0x040000, 0x010000, 0x1640ad1f )
	ROM_CONTINUE(      0x0c0000, 0x010000 )
	ROM_LOAD( "10374", 0x050000, 0x010000, 0x22744340 )
	ROM_CONTINUE(      0x0d0000, 0x010000 )
	ROM_LOAD( "10376", 0x060000, 0x010000, 0x8337ace7 )
	ROM_CONTINUE(      0x0e0000, 0x010000 )
	ROM_LOAD( "10378", 0x070000, 0x010000, 0x544068fd )
	ROM_CONTINUE(      0x0f0000, 0x010000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10187",       0x00000, 0x008000, 0xa10abaa9 )

	ROM_REGION( 0x38000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "10193",       0x00000, 0x008000, 0xbcd10dde )
	ROM_RELOAD(              0x30000, 0x008000 ) // twice??
	ROM_LOAD( "10192",       0x08000, 0x008000, 0x770f1270 )
	ROM_LOAD( "10191",       0x10000, 0x008000, 0x20a284ab )
	ROM_LOAD( "10190",       0x18000, 0x008000, 0x7cab70e2 )
	ROM_LOAD( "10189",       0x20000, 0x008000, 0x01366b54 )
	ROM_LOAD( "10188",       0x28000, 0x008000, 0xbad30ad9 )

	ROM_REGION( 0x40000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("10327a", 0x00000, 0x10000, 0xe28a5baf )
	ROM_LOAD_ODD( "10329a", 0x00000, 0x10000, 0xda131c81 )
	ROM_LOAD_EVEN("10328a", 0x20000, 0x10000, 0xd5ec5e5d )
	ROM_LOAD_ODD( "10330a", 0x20000, 0x10000, 0xba9ec82a )

	ROM_REGION( 0x80000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "10185", 0x0000, 0x8000, 0x22794426 )
ROM_END

ROM_START( outruna )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "10380b", 0x000000, 0x10000, 0x1f6cadad )
	ROM_LOAD_ODD ( "10382b", 0x000000, 0x10000, 0xc4c3fa1a )
	ROM_LOAD_EVEN( "10381a", 0x020000, 0x10000, 0xbe8c412b )
	ROM_LOAD_ODD ( "10383b", 0x020000, 0x10000, 0x10a2014a )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10268", 0x00000, 0x08000, 0x95344b04 )
	ROM_LOAD( "10232", 0x08000, 0x08000, 0x776ba1eb )
	ROM_LOAD( "10267", 0x10000, 0x08000, 0xa85bb823 )
	ROM_LOAD( "10231", 0x18000, 0x08000, 0x8908bcbf )
	ROM_LOAD( "10266", 0x20000, 0x08000, 0x9f6f1a74 )
	ROM_LOAD( "10230", 0x28000, 0x08000, 0x686f5e50 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10371", 0x000000, 0x010000, 0x0a1c98de )
	ROM_CONTINUE(      0x080000, 0x010000 )
	ROM_LOAD( "10373", 0x010000, 0x010000, 0x339f8e64 )
	ROM_CONTINUE(      0x090000, 0x010000 )
	ROM_LOAD( "10375", 0x020000, 0x010000, 0x62a472bd )
	ROM_CONTINUE(      0x0a0000, 0x010000 )
	ROM_LOAD( "10377", 0x030000, 0x010000, 0xc86daecb )
	ROM_CONTINUE(      0x0b0000, 0x010000 )

	ROM_LOAD( "10372", 0x040000, 0x010000, 0x1640ad1f )
	ROM_CONTINUE(      0x0c0000, 0x010000 )
	ROM_LOAD( "10374", 0x050000, 0x010000, 0x22744340 )
	ROM_CONTINUE(      0x0d0000, 0x010000 )
	ROM_LOAD( "10376", 0x060000, 0x010000, 0x8337ace7 )
	ROM_CONTINUE(      0x0e0000, 0x010000 )
	ROM_LOAD( "10378", 0x070000, 0x010000, 0x544068fd )
	ROM_CONTINUE(      0x0f0000, 0x010000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10187",       0x00000, 0x008000, 0xa10abaa9 )

	ROM_REGION( 0x38000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "10193",       0x00000, 0x008000, 0xbcd10dde )
	ROM_RELOAD(              0x30000, 0x008000 ) // twice??
	ROM_LOAD( "10192",       0x08000, 0x008000, 0x770f1270 )
	ROM_LOAD( "10191",       0x10000, 0x008000, 0x20a284ab )
	ROM_LOAD( "10190",       0x18000, 0x008000, 0x7cab70e2 )
	ROM_LOAD( "10189",       0x20000, 0x008000, 0x01366b54 )
	ROM_LOAD( "10188",       0x28000, 0x008000, 0xbad30ad9 )

	ROM_REGION( 0x40000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("10327a", 0x00000, 0x10000, 0xe28a5baf )
	ROM_LOAD_ODD( "10329a", 0x00000, 0x10000, 0xda131c81 )
	ROM_LOAD_EVEN("10328a", 0x20000, 0x10000, 0xd5ec5e5d )
	ROM_LOAD_ODD( "10330a", 0x20000, 0x10000, 0xba9ec82a )

	ROM_REGION( 0x80000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "10185", 0x0000, 0x8000, 0x22794426 )
ROM_END


ROM_START( outrunb )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "orun_mn.rom", 0x000000, 0x10000, 0xcddceea2 )
	ROM_LOAD_ODD ( "orun_ml.rom", 0x000000, 0x10000, 0x9cfc07d5 )
	ROM_LOAD_EVEN( "orun_mm.rom", 0x020000, 0x10000, 0x3092d857 )
	ROM_LOAD_ODD ( "orun_mk.rom", 0x020000, 0x10000, 0x30a1c496 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10268", 0x00000, 0x08000, 0x95344b04 )
	ROM_LOAD( "10232", 0x08000, 0x08000, 0x776ba1eb )
	ROM_LOAD( "10267", 0x10000, 0x08000, 0xa85bb823 )
	ROM_LOAD( "10231", 0x18000, 0x08000, 0x8908bcbf )
	ROM_LOAD( "10266", 0x20000, 0x08000, 0x9f6f1a74 )
	ROM_LOAD( "10230", 0x28000, 0x08000, 0x686f5e50 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "orun_1.rom", 0x000000, 0x010000, 0x77377e00 )
	ROM_LOAD( "orun_3.rom", 0x010000, 0x010000, 0x69ecc975 )
	ROM_LOAD( "orun_5.rom", 0x020000, 0x010000, 0xb6a8d0e2 )
	ROM_LOAD( "orun_7.rom", 0x030000, 0x010000, 0xd632d8a2 )

	ROM_LOAD( "orun_2.rom", 0x040000, 0x010000, 0x2c0e7277 )
	ROM_LOAD( "orun_4.rom", 0x050000, 0x010000, 0x54761e57 )
	ROM_LOAD( "orun_6.rom", 0x060000, 0x010000, 0xa00d0676 )
	ROM_LOAD( "orun_8.rom", 0x070000, 0x010000, 0xda398368 )

	ROM_LOAD( "orun_17.rom", 0x080000, 0x010000, 0x4f784236 )
	ROM_LOAD( "orun_19.rom", 0x090000, 0x010000, 0xee4f7154 )
	ROM_LOAD( "orun_21.rom", 0x0a0000, 0x010000, 0xe9880aa3 )
	ROM_LOAD( "orun_23.rom", 0x0b0000, 0x010000, 0xdc286dc2 )

	ROM_LOAD( "orun_18.rom", 0x0c0000, 0x010000, 0x8d459356 )
	ROM_LOAD( "orun_20.rom", 0x0d0000, 0x010000, 0xc2825654 )
	ROM_LOAD( "orun_22.rom", 0x0e0000, 0x010000, 0xef7d06fe )
	ROM_LOAD( "orun_24.rom", 0x0f0000, 0x010000, 0x1222af9f )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "orun_ma.rom", 0x00000, 0x008000, 0xa3ff797a )

	ROM_REGION( 0x38000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "10193",       0x00000, 0x008000, 0xbcd10dde )
	ROM_RELOAD(              0x30000, 0x008000 ) // twice??
	ROM_LOAD( "10192",       0x08000, 0x008000, 0x770f1270 )
	ROM_LOAD( "10191",       0x10000, 0x008000, 0x20a284ab )
	ROM_LOAD( "10190",       0x18000, 0x008000, 0x7cab70e2 )
	ROM_LOAD( "10189",       0x20000, 0x008000, 0x01366b54 )
	ROM_LOAD( "10188",       0x28000, 0x008000, 0xbad30ad9 )

	ROM_REGION( 0x40000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("orun_mj.rom", 0x00000, 0x10000, 0xd7f5aae0 )
	ROM_LOAD_ODD( "orun_mh.rom", 0x00000, 0x10000, 0x88c2e78f )
	ROM_LOAD_EVEN("10328a",      0x20000, 0x10000, 0xd5ec5e5d )
	ROM_LOAD_ODD( "orun_mg.rom", 0x20000, 0x10000, 0x74c5fbec )

	ROM_REGION( 0x80000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "orun_me.rom", 0x0000, 0x8000, 0x666fe754 )

//	ROM_LOAD( "orun_mf.rom", 0x0000, 0x8000, 0xed5bda9c )	//??
ROM_END

/***************************************************************************/

//static READ_HANDLER( or_io_joy_r ){ return (input_port_5_r( offset ) << 8) + input_port_6_r( offset ); }

#ifdef HANGON_DIGITAL_CONTROLS
static READ_HANDLER( or_io_brake_r ){
	int data = input_port_1_r( offset );

	switch(data & 3)
	{
		case 3:	return 0xff00;	// both
		case 1:	return 0xff00;  // brake
		case 2:	return 0x0000;  // accel
		case 0:	return 0x0000;  // neither
	}
	return 0x0000;
}

static READ_HANDLER( or_io_acc_steer_r ){
	int data = input_port_1_r( offset );
	int ret = input_port_0_r( offset ) << 8;

	switch(data & 3)
	{
		case 3:	return 0x00 | ret;	// both
		case 1:	return 0x00 | ret;  // brake
		case 2:	return 0xff | ret;  // accel
		case 0:	return 0x00 | ret ;  // neither
	}
	return 0x00 | ret;
}
#else
static READ_HANDLER( or_io_acc_steer_r ){ return (input_port_0_r( offset ) << 8) + input_port_1_r( offset ); }
static READ_HANDLER( or_io_brake_r ){ return input_port_5_r( offset ) << 8; }
#endif

static int or_gear=0;

static READ_HANDLER( or_io_service_r )
{
	int ret=input_port_2_r( offset );
	int data=input_port_1_r( offset );
	if(data & 4) or_gear=0;
	else if(data & 8) or_gear=1;

	if(or_gear) ret|=0x10;
	else ret&=0xef;

	return ret;
}

static READ_HANDLER( or_reset2_r )
{
	cpu_set_reset_line(2,PULSE_LINE);
	return 0;
}


static struct MemoryReadAddress outrun_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x060892, 0x060893, or_io_acc_steer_r },
	{ 0x060894, 0x060895, or_io_brake_r },
	{ 0x060900, 0x060907, sound_shared_ram_r },		//???
	{ 0x060000, 0x067fff, MRA_EXTRAM5 },

	{ 0x100000, 0x10ffff, MRA_TILERAM },
	{ 0x110000, 0x110fff, MRA_TEXTRAM },

	{ 0x130000, 0x130fff, MRA_SPRITERAM },
	{ 0x120000, 0x121fff, MRA_PALETTERAM },

	{ 0x140010, 0x140011, or_io_service_r },
	{ 0x140014, 0x140015, io_dip1_r },
	{ 0x140016, 0x140017, io_dip2_r },

	{ 0x140000, 0x140071, MRA_EXTRAM3 },		//io
	{ 0x200000, 0x23ffff, MRA_BANK8 },
	{ 0x260000, 0x267fff, shared_ram_r },
	{ 0xe00000, 0xe00001, or_reset2_r },

	{-1}
};

static WRITE_HANDLER( outrun_sound_write_w )
{
	sound_shared_ram[0]=data&0xff;
}

static struct MemoryWriteAddress outrun_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x060900, 0x060907, sound_shared_ram_w },		//???
	{ 0x060000, 0x067fff, MWA_EXTRAM5 },

	{ 0x100000, 0x10ffff, MWA_TILERAM },
	{ 0x110000, 0x110fff, MWA_TEXTRAM },

	{ 0x130000, 0x130fff, MWA_SPRITERAM },
	{ 0x120000, 0x121fff, MWA_PALETTERAM },

	{ 0x140000, 0x140071, MWA_EXTRAM3 },		//io
	{ 0x200000, 0x23ffff, MWA_BANK8 },
	{ 0x260000, 0x267fff, shared_ram_w, &shared_ram },
	{ 0xffff06, 0xffff07, outrun_sound_write_w },

	{-1}
};

static struct MemoryReadAddress outrun_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x060000, 0x067fff, shared_ram_r },
	{ 0x080000, 0x09ffff, MRA_EXTRAM },		// gr

	{-1}
};

static struct MemoryWriteAddress outrun_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x060000, 0x067fff, shared_ram_w },
	{ 0x080000, 0x09ffff, MWA_EXTRAM },		// gr

	{-1}
};

// Outrun

static struct MemoryReadAddress outrun_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xf0ff, SegaPCM_r },
	{ 0xf100, 0xf7ff, MRA_NOP },
	{ 0xf800, 0xf807, sound2_shared_ram_r },
	{ 0xf808, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress outrun_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xf000, 0xf0ff, SegaPCM_w },
	{ 0xf100, 0xf7ff, MWA_NOP },
	{ 0xf800, 0xf807, sound2_shared_ram_w,&sound_shared_ram },
	{ 0xf808, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

/***************************************************************************/

static void outrun_update_proc( void ){
	int data;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0e98] );
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0e9a] );
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0e90] );
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0e92] );
	set_fg_page( READ_WORD( &sys16_textram[0x0e80] ) );
	set_bg_page( READ_WORD( &sys16_textram[0x0e82] ) );

	set_refresh( READ_WORD( &sys16_extraram5[0xb6e] ) );
	data=READ_WORD( &sys16_extraram5[0xb6c] );

	if(data & 0x2)
	{
		osd_led_w(0,1);
		osd_led_w(2,1);
	}
	else
	{
		osd_led_w(0,0);
		osd_led_w(2,0);
	}

	if(data & 0x4)
		osd_led_w(1,1);
	else
		osd_led_w(1,0);
}

static void outrun_init_machine( void ){
	static int bank[16] = { 07,00,01,04,05,02,03,06,00,00,00,00,00,00,00,00};
	sys16_obj_bank = bank;
	sys16_spritesystem = 7;
	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;
	sys16_sprxoffset = -0xc0;

// cpu 0 reset opcode resets cpu 2?
	patch_code(0x7d44,0x4a);
	patch_code(0x7d45,0x79);
	patch_code(0x7d46,0x00);
	patch_code(0x7d47,0xe0);
	patch_code(0x7d48,0x00);
	patch_code(0x7d49,0x00);

// *forced sound cmd
	patch_code( 0x55ed, 0x00);

// rogue tile on music selection screen
//	patch_code( 0x38545, 0x80);

// *freeze time
//	patch_code( 0xb6b6, 0x4e);
//	patch_code( 0xb6b7, 0x71);

	cpu_setbank(8, memory_region(REGION_CPU3));

	sys16_update_proc = outrun_update_proc;

	gr_ver = &sys16_extraram[0];
	gr_hor = gr_ver+0x400;
	gr_flip= gr_ver+0xc00;
	gr_palette= 0xf00 / 2;
	gr_palette_default = 0x800 /2;
	gr_colorflip[0][0]=0x08 / 2;
	gr_colorflip[0][1]=0x04 / 2;
	gr_colorflip[0][2]=0x00 / 2;
	gr_colorflip[0][3]=0x00 / 2;
	gr_colorflip[1][0]=0x0a / 2;
	gr_colorflip[1][1]=0x06 / 2;
	gr_colorflip[1][2]=0x02 / 2;
	gr_colorflip[1][3]=0x00 / 2;

	gr_second_road = &sys16_extraram[0x10000];

}

static void outruna_init_machine( void ){
	static int bank[16] = { 07,00,01,04,05,02,03,06,00,00,00,00,00,00,00,00};
	sys16_obj_bank = bank;
	sys16_spritesystem = 7;
	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;

// cpu 0 reset opcode resets cpu 2?
	patch_code(0x7db8,0x4a);
	patch_code(0x7db9,0x79);
	patch_code(0x7dba,0x00);
	patch_code(0x7dbb,0xe0);
	patch_code(0x7dbc,0x00);
	patch_code(0x7dbd,0x00);

// *forced sound cmd
	patch_code( 0x5661, 0x00);

// rogue tile on music selection screen
//	patch_code( 0x38455, 0x80);

// *freeze time
//	patch_code( 0xb6b6, 0x4e);
//	patch_code( 0xb6b7, 0x71);

	cpu_setbank(8, memory_region(REGION_CPU3));

	sys16_update_proc = outrun_update_proc;

	gr_ver = &sys16_extraram[0];
	gr_hor = gr_ver+0x400;
	gr_flip= gr_ver+0xc00;
	gr_palette= 0xf00 / 2;
	gr_palette_default = 0x800 /2;
	gr_colorflip[0][0]=0x08 / 2;
	gr_colorflip[0][1]=0x04 / 2;
	gr_colorflip[0][2]=0x00 / 2;
	gr_colorflip[0][3]=0x00 / 2;
	gr_colorflip[1][0]=0x0a / 2;
	gr_colorflip[1][1]=0x06 / 2;
	gr_colorflip[1][2]=0x02 / 2;
	gr_colorflip[1][3]=0x00 / 2;

	gr_second_road = &sys16_extraram[0x10000];

}

static void init_outrun( void )
{
	sys16_onetime_init_machine();
	sys16_sprite_decode2( 4,0x040000, 0 );
	generate_gr_screen(512,2048,0,0,3,0x8000);
}

static void init_outrunb( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int i;
	int odd,even,word;
	sys16_onetime_init_machine();
/*
  Main Processor
	Comparing the bootleg with the custom bootleg, it seems that:-

  if even bytes &0x28 == 0x20 or 0x08 then they are xored with 0x28
  if odd bytes &0xc0 == 0x40 or 0x80 then they are xored with 0xc0

  ie. data lines are switched.
*/

	for(i=0;i<0x40000;i+=2)
	{
		word=READ_WORD(&RAM[i]);
		even=word>>8;
		odd=word&0xff;
		// even byte
		if((even&0x28) == 0x20 || (even&0x28) == 0x08)
			even^=0x28;
		// odd byte
		if((odd&0xc0) == 0x80 || (odd&0xc0) == 0x40)
			odd^=0xc0;
		word=(even<<8)+odd;
		WRITE_WORD(&RAM[i],word);
	}

/*
  Second Processor

  if even bytes &0xc0 == 0x40 or 0x80 then they are xored with 0xc0
  if odd bytes &0x0c == 0x04 or 0x08 then they are xored with 0x0c
*/
	RAM = memory_region(REGION_CPU3);
	for(i=0;i<0x40000;i+=2)
	{
		word=READ_WORD(&RAM[i]);
		even=word>>8;
		odd=word&0xff;
		// even byte
		if((even&0xc0) == 0x80 || (even&0xc0) == 0x40)
			even^=0xc0;
		// odd byte
		if((odd&0x0c) == 0x08 || (odd&0x0c) == 0x04)
			odd^=0x0c;
		word=(even<<8)+odd;
		WRITE_WORD(&RAM[i],word);
	}
/*
  Road GFX

	rom orun_me.rom
	if bytes &0x60 == 0x40 or 0x20 then they are xored with 0x60

	rom orun_mf.rom
	if bytes &0xc0 == 0x40 or 0x80 then they are xored with 0xc0

  I don't know why there's 2 road roms, but I'm using orun_me.rom
*/
	RAM = memory_region(REGION_GFX3);
	for(i=0;i<0x8000;i++)
	{
		if((RAM[i]&0x60) == 0x20 || (RAM[i]&0x60) == 0x40)
			RAM[i]^=0x60;
	}

	generate_gr_screen(512,2048,0,0,3,0x8000);

/*
  Z80 Code
	rom orun_ma.rom
	if bytes &0x60 == 0x40 or 0x20 then they are xored with 0x60

*/
	RAM = memory_region(REGION_CPU2);
	for(i=0;i<0x8000;i++)
	{
		if((RAM[i]&0x60) == 0x20 || (RAM[i]&0x60) == 0x40)
			RAM[i]^=0x60;
	}

	sys16_sprite_decode2( 4,0x040000,0  );
}

/***************************************************************************/

INPUT_PORTS_START( outrun )
PORT_START	/* Steering */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_CENTER, 100, 3, 0x48, 0xb8 )
//	PORT_ANALOG( 0xff, 0x7f, IPT_PADDLE , 70, 3, 0x48, 0xb8 )

#ifdef HANGON_DIGITAL_CONTROLS

PORT_START	/* Buttons */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 )

#else

PORT_START	/* Accel / Decel */
	PORT_ANALOG( 0xff, 0x30, IPT_AD_STICK_Y | IPF_CENTER | IPF_REVERSE, 100, 16, 0x30, 0x90 )

#endif

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
//	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, "Up Cockpit" )
	PORT_DIPSETTING(    0x01, "Mini Up" )
	PORT_DIPSETTING(    0x03, "Moving" )
//	PORT_DIPSETTING(    0x00, "No Use" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Time" )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, "Enemies" )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )


#ifndef HANGON_DIGITAL_CONTROLS

PORT_START	/* Brake */
	PORT_ANALOG( 0xff, 0x30, IPT_AD_STICK_Y | IPF_PLAYER2 | IPF_CENTER | IPF_REVERSE, 100, 16, 0x30, 0x90 )

#endif

INPUT_PORTS_END

/***************************************************************************/
static int or_interrupt( void ){
	int intleft=cpu_getiloops();
	if(intleft!=0) return 2;
	else return 4;
}


#define MACHINE_DRIVER_OUTRUN( GAMENAME,INITMACHINE) \
static struct MachineDriver GAMENAME = \
{ \
	{ \
		{ \
			CPU_M68000, \
			12000000, \
			outrun_readmem,outrun_writemem,0,0, \
			or_interrupt,2 \
		}, \
		{ \
			CPU_Z80 | CPU_AUDIO_CPU, \
			4096000, \
			outrun_sound_readmem,outrun_sound_writemem,sound_readport,sound_writeport, \
			ignore_interrupt,1 \
		}, \
		{ \
			CPU_M68000, \
			12000000, \
			outrun_readmem2,outrun_writemem2,0,0, \
			sys16_interrupt,2 \
		}, \
	}, \
	60, 100 /*DEFAULT_60HZ_VBLANK_DURATION*/, \
	4, /* needed to sync processors */ \
	INITMACHINE, \
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 }, \
	gfx1, \
	4096*ShadowColorsMultiplier,4096*ShadowColorsMultiplier, \
	0, \
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK, \
	0, \
	sys16_or_vh_start, \
	sys16_vh_stop, \
	sys16_or_vh_screenrefresh, \
	SOUND_SUPPORTS_STEREO,0,0,0, \
	{ \
		{ \
			SOUND_YM2151, \
			&ym2151_interface \
		}, \
		{ \
			SOUND_SEGAPCM, \
			&segapcm_interface_15k, \
		} \
	} \
};

MACHINE_DRIVER_OUTRUN(machine_driver_outrun,outrun_init_machine)
MACHINE_DRIVER_OUTRUN(machine_driver_outruna,outruna_init_machine)

/*****************************************************************************/
// Enduro Racer

ROM_START( enduror )
	ROM_REGION( 0x040000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "7640a.rom",0x00000, 0x8000, 0x1d1dc5d4 )
	ROM_LOAD_ODD ( "7636a.rom",0x00000, 0x8000, 0x84131639 )

	ROM_LOAD_EVEN( "7641.rom", 0x10000, 0x8000, 0x2503ae7c )
	ROM_LOAD_ODD ( "7637.rom", 0x10000, 0x8000, 0x82a27a8c )
	ROM_LOAD_EVEN( "7642.rom", 0x20000, 0x8000, 0x1c453bea )	// enduro.a06 / .a09
	ROM_LOAD_ODD ( "7638.rom", 0x20000, 0x8000, 0x70544779 )	// looks like encrypted versions of

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "7644.rom", 0x00000, 0x08000, 0xe7a4ff90 )
	ROM_LOAD( "7645.rom", 0x08000, 0x08000, 0x4caa0095 )
	ROM_LOAD( "7646.rom", 0x10000, 0x08000, 0x7e432683 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */

	ROM_LOAD( "7678.rom", 0x000000, 0x008000, 0x9fb5e656 )
	ROM_LOAD( "7670.rom", 0x008000, 0x008000, 0xdbbe2f6e )
	ROM_LOAD( "7662.rom", 0x010000, 0x008000, 0xcb0c13c5 )
	ROM_LOAD( "7654.rom", 0x018000, 0x008000, 0x2db6520d )

	ROM_LOAD( "7677.rom", 0x020000, 0x008000, 0x7764765b )
	ROM_LOAD( "7669.rom", 0x028000, 0x008000, 0xf9525faa )
	ROM_LOAD( "7661.rom", 0x030000, 0x008000, 0xfe93a79b )
	ROM_LOAD( "7653.rom", 0x038000, 0x008000, 0x46a52114 )

	ROM_LOAD( "7676.rom", 0x040000, 0x008000, 0x2e42e0d4 )
	ROM_LOAD( "7668.rom", 0x048000, 0x008000, 0xe115ce33 )
	ROM_LOAD( "7660.rom", 0x050000, 0x008000, 0x86dfbb68 )
	ROM_LOAD( "7652.rom", 0x058000, 0x008000, 0x2880cfdb )

	ROM_LOAD( "7675.rom", 0x060000, 0x008000, 0x05cd2d61 )
	ROM_LOAD( "7667.rom", 0x068000, 0x008000, 0x923bde9d )
	ROM_LOAD( "7659.rom", 0x070000, 0x008000, 0x629dc8ce )
	ROM_LOAD( "7651.rom", 0x078000, 0x008000, 0xd7902bad )

	ROM_LOAD( "7674.rom", 0x080000, 0x008000, 0x1a129acf )
	ROM_LOAD( "7666.rom", 0x088000, 0x008000, 0x23697257 )
	ROM_LOAD( "7658.rom", 0x090000, 0x008000, 0x1677f24f )
	ROM_LOAD( "7650.rom", 0x098000, 0x008000, 0x642635ec )

	ROM_LOAD( "7673.rom", 0x0a0000, 0x008000, 0x82602394 )
	ROM_LOAD( "7665.rom", 0x0a8000, 0x008000, 0x12d77607 )
	ROM_LOAD( "7657.rom", 0x0b0000, 0x008000, 0x8158839c )
	ROM_LOAD( "7649.rom", 0x0b8000, 0x008000, 0x4edba14c )

	ROM_LOAD( "7672.rom", 0x0c0000, 0x008000, 0xd11452f7 )
	ROM_LOAD( "7664.rom", 0x0c8000, 0x008000, 0x0df2cfad )
	ROM_LOAD( "7656.rom", 0x0d0000, 0x008000, 0x6c741272 )
	ROM_LOAD( "7648.rom", 0x0d8000, 0x008000, 0x983ea830 )

	ROM_LOAD( "7671.rom", 0x0e0000, 0x008000, 0xb0c7fdc6 )
	ROM_LOAD( "7663.rom", 0x0e8000, 0x008000, 0x2b0b8f08 )
	ROM_LOAD( "7655.rom", 0x0f0000, 0x008000, 0x3433fe7b )
	ROM_LOAD( "7647.rom", 0x0f8000, 0x008000, 0x2e7fbec0 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "7682.rom", 0x00000, 0x008000, 0xc4efbf48 )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "7681.rom", 0x00000, 0x008000, 0xbc0c4d12 )
	ROM_LOAD( "7680.rom", 0x08000, 0x008000, 0x627b3c8c )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("7634.rom", 0x0000, 0x8000, 0x3e07fd32 )
	ROM_LOAD_ODD ("7635.rom", 0x0000, 0x8000, 0x22f762ab )
	// alternate version??
//	ROM_LOAD_EVEN("7634a.rom", 0x0000, 0x8000, 0xaec83731 )
//	ROM_LOAD_ODD ("7635a.rom", 0x0000, 0x8000, 0xb2fce96f )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "7633.rom", 0x0000, 0x8000, 0x6f146210 )
ROM_END



ROM_START( endurobl )
	ROM_REGION( 0x040000+0x010000+0x040000, REGION_CPU1 ) /* 68000 code + space for RAM + space for decrypted opcodes */
	ROM_LOAD_EVEN( "7.13j", 0x030000, 0x08000, 0xf1d6b4b7 )
	ROM_CONTINUE (          0x000000, 0x08000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_ODD ( "4.13h", 0x030000, 0x08000, 0x43bff873 )						// rom de-coded
	ROM_CONTINUE (          0x000001, 0x08000 | ROMFLAG_ALTERNATE )		// data de-coded

	ROM_LOAD_EVEN( "8.14j", 0x010000, 0x08000, 0x2153154a )
	ROM_LOAD_ODD ( "5.14h", 0x010000, 0x08000, 0x0a97992c )
	ROM_LOAD_EVEN( "9.15j", 0x020000, 0x08000, 0xdb3bff1c )	// one byte difference from
	ROM_LOAD_ODD ( "6.15h", 0x020000, 0x08000, 0x54b1885a )	// enduro.a06 / enduro.a09

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "7644.rom", 0x00000, 0x08000, 0xe7a4ff90 )
	ROM_LOAD( "7645.rom", 0x08000, 0x08000, 0x4caa0095 )
	ROM_LOAD( "7646.rom", 0x10000, 0x08000, 0x7e432683 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "7678.rom", 0x000000, 0x008000, 0x9fb5e656 )
	ROM_LOAD( "7670.rom", 0x008000, 0x008000, 0xdbbe2f6e )
	ROM_LOAD( "7662.rom", 0x010000, 0x008000, 0xcb0c13c5 )
	ROM_LOAD( "7654.rom", 0x018000, 0x008000, 0x2db6520d )

	ROM_LOAD( "7677.rom", 0x020000, 0x008000, 0x7764765b )
	ROM_LOAD( "7669.rom", 0x028000, 0x008000, 0xf9525faa )
	ROM_LOAD( "7661.rom", 0x030000, 0x008000, 0xfe93a79b )
	ROM_LOAD( "7653.rom", 0x038000, 0x008000, 0x46a52114 )

	ROM_LOAD( "7676.rom", 0x040000, 0x008000, 0x2e42e0d4 )
	ROM_LOAD( "7668.rom", 0x048000, 0x008000, 0xe115ce33 )
	ROM_LOAD( "7660.rom", 0x050000, 0x008000, 0x86dfbb68 )
	ROM_LOAD( "7652.rom", 0x058000, 0x008000, 0x2880cfdb )

	ROM_LOAD( "7675.rom", 0x060000, 0x008000, 0x05cd2d61 )
	ROM_LOAD( "7667.rom", 0x068000, 0x008000, 0x923bde9d )
	ROM_LOAD( "7659.rom", 0x070000, 0x008000, 0x629dc8ce )
	ROM_LOAD( "7651.rom", 0x078000, 0x008000, 0xd7902bad )

	ROM_LOAD( "7674.rom", 0x080000, 0x008000, 0x1a129acf )
	ROM_LOAD( "7666.rom", 0x088000, 0x008000, 0x23697257 )
	ROM_LOAD( "7658.rom", 0x090000, 0x008000, 0x1677f24f )
	ROM_LOAD( "7650.rom", 0x098000, 0x008000, 0x642635ec )

	ROM_LOAD( "7673.rom", 0x0a0000, 0x008000, 0x82602394 )
	ROM_LOAD( "7665.rom", 0x0a8000, 0x008000, 0x12d77607 )
	ROM_LOAD( "7657.rom", 0x0b0000, 0x008000, 0x8158839c )
	ROM_LOAD( "7649.rom", 0x0b8000, 0x008000, 0x4edba14c )

	ROM_LOAD( "7672.rom", 0x0c0000, 0x008000, 0xd11452f7 )
	ROM_LOAD( "7664.rom", 0x0c8000, 0x008000, 0x0df2cfad )
	ROM_LOAD( "7656.rom", 0x0d0000, 0x008000, 0x6c741272 )
	ROM_LOAD( "7648.rom", 0x0d8000, 0x008000, 0x983ea830 )

	ROM_LOAD( "7671.rom", 0x0e0000, 0x008000, 0xb0c7fdc6 )
	ROM_LOAD( "7663.rom", 0x0e8000, 0x008000, 0x2b0b8f08 )
	ROM_LOAD( "7655.rom", 0x0f0000, 0x008000, 0x3433fe7b )
	ROM_LOAD( "7647.rom", 0x0f8000, 0x008000, 0x2e7fbec0 )


	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "13.16d", 0x00000, 0x004000, 0x81c82fc9 )
	ROM_LOAD( "12.16e", 0x04000, 0x004000, 0x755bfdad )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "7681.rom", 0x00000, 0x008000, 0xbc0c4d12 )
	ROM_LOAD( "7680.rom", 0x08000, 0x008000, 0x627b3c8c )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("7634.rom", 0x0000, 0x8000, 0x3e07fd32 )
	ROM_LOAD_ODD ("7635.rom", 0x0000, 0x8000, 0x22f762ab )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "7633.rom", 0x0000, 0x8000, 0x6f146210 )
ROM_END

ROM_START( endurob2 )
	ROM_REGION( 0x040000+0x010000+0x040000, REGION_CPU1 ) /* 68000 code + space for RAM + space for decrypted opcodes */
	ROM_LOAD_EVEN( "enduro.a07", 0x000000, 0x08000, 0x259069bc )
	ROM_LOAD_ODD ( "enduro.a04", 0x000000, 0x08000, 0xf584fbd9 )
	ROM_LOAD_EVEN( "enduro.a08", 0x010000, 0x08000, 0xd234918c )
	ROM_LOAD_ODD ( "enduro.a05", 0x010000, 0x08000, 0xa525dd57 )
	ROM_LOAD_EVEN( "enduro.a09", 0x020000, 0x08000, 0xf6391091 )
	ROM_LOAD_ODD ( "enduro.a06", 0x020000, 0x08000, 0x79b367d7 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "7644.rom", 0x00000, 0x08000, 0xe7a4ff90 )
	ROM_LOAD( "7645.rom", 0x08000, 0x08000, 0x4caa0095 )
	ROM_LOAD( "7646.rom", 0x10000, 0x08000, 0x7e432683 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */

	ROM_LOAD( "7678.rom", 0x000000, 0x008000, 0x9fb5e656 )
	ROM_LOAD( "7670.rom", 0x008000, 0x008000, 0xdbbe2f6e )
	ROM_LOAD( "7662.rom", 0x010000, 0x008000, 0xcb0c13c5 )
	ROM_LOAD( "7654.rom", 0x018000, 0x008000, 0x2db6520d )

	ROM_LOAD( "7677.rom", 0x020000, 0x008000, 0x7764765b )
	ROM_LOAD( "7669.rom", 0x028000, 0x008000, 0xf9525faa )
	ROM_LOAD( "enduro.a34", 0x030000, 0x008000, 0x296454d8 )
	ROM_LOAD( "7653.rom", 0x038000, 0x008000, 0x46a52114 )

	ROM_LOAD( "7676.rom", 0x040000, 0x008000, 0x2e42e0d4 )
	ROM_LOAD( "7668.rom", 0x048000, 0x008000, 0xe115ce33 )
	ROM_LOAD( "enduro.a35", 0x050000, 0x008000, 0x1ebe76df )
	ROM_LOAD( "7652.rom", 0x058000, 0x008000, 0x2880cfdb )

	ROM_LOAD( "enduro.a20", 0x060000, 0x008000, 0x7c280bc8 )
	ROM_LOAD( "enduro.a28", 0x068000, 0x008000, 0x321f034b )
	ROM_LOAD( "enduro.a36", 0x070000, 0x008000, 0x243e34e5 )
	ROM_LOAD( "enduro.a44", 0x078000, 0x008000, 0x84bb12a1 )

	ROM_LOAD( "7674.rom", 0x080000, 0x008000, 0x1a129acf )
	ROM_LOAD( "7666.rom", 0x088000, 0x008000, 0x23697257 )
	ROM_LOAD( "7658.rom", 0x090000, 0x008000, 0x1677f24f )
	ROM_LOAD( "7650.rom", 0x098000, 0x008000, 0x642635ec )

	ROM_LOAD( "7673.rom", 0x0a0000, 0x008000, 0x82602394 )
	ROM_LOAD( "7665.rom", 0x0a8000, 0x008000, 0x12d77607 )
	ROM_LOAD( "7657.rom", 0x0b0000, 0x008000, 0x8158839c )
	ROM_LOAD( "7649.rom", 0x0b8000, 0x008000, 0x4edba14c )

	ROM_LOAD( "7672.rom", 0x0c0000, 0x008000, 0xd11452f7 )
	ROM_LOAD( "7664.rom", 0x0c8000, 0x008000, 0x0df2cfad )
	ROM_LOAD( "enduro.a39", 0x0d0000, 0x008000, 0x1ff3a5e2 )
	ROM_LOAD( "7648.rom", 0x0d8000, 0x008000, 0x983ea830 )

	ROM_LOAD( "7671.rom", 0x0e0000, 0x008000, 0xb0c7fdc6 )
	ROM_LOAD( "7663.rom", 0x0e8000, 0x008000, 0x2b0b8f08 )
	ROM_LOAD( "7655.rom", 0x0f0000, 0x008000, 0x3433fe7b )
	ROM_LOAD( "7647.rom", 0x0f8000, 0x008000, 0x2e7fbec0 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "enduro.a16", 0x00000, 0x008000, 0xd2cb6eb5 )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* Sega PCM sound data */
	ROM_LOAD( "7681.rom", 0x00000, 0x008000, 0xbc0c4d12 )
	ROM_LOAD( "7680.rom", 0x08000, 0x008000, 0x627b3c8c )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* second 68000 CPU */
	ROM_LOAD_EVEN("7634.rom", 0x0000, 0x8000, 0x3e07fd32 )
	ROM_LOAD_ODD ("7635.rom", 0x0000, 0x8000, 0x22f762ab )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* Road Graphics  (region size should be gr_bitmapwidth*256 )*/
	ROM_LOAD( "7633.rom", 0x0000, 0x8000, 0x6f146210 )
ROM_END

/***************************************************************************/

static READ_HANDLER( er_io_analog_r )
{
	switch(READ_WORD(&sys16_extraram3[0x30]))
	{
		case 0:		// accel
			if(input_port_1_r( offset ) & 1)
				return 0xff;
			else
				return 0;
		case 4:		// brake
			if(input_port_1_r( offset ) & 2)
				return 0xff;
			else
				return 0;
		case 8:		// bank up down?
			if(input_port_1_r( offset ) & 4)
				return 0xff;
			else
				return 0;
		case 12:	// handle
			return input_port_0_r( offset );

	}
	return 0;
}

static READ_HANDLER( er_reset2_r )
{
	cpu_set_reset_line(2,PULSE_LINE);
	return 0;
}

static struct MemoryReadAddress enduror_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x040000, 0x043fff, MRA_EXTRAM },
	{ 0x100000, 0x107fff, MRA_TILERAM },
	{ 0x108000, 0x108fff, MRA_TEXTRAM },
	{ 0x110000, 0x110fff, MRA_PALETTERAM },

	{ 0x124000, 0x127fff, shared_ram_r },

	{ 0x130000, 0x130fff, MRA_SPRITERAM },

	{ 0x140010, 0x140011, io_service_r },
	{ 0x140014, 0x140015, io_dip1_r },
	{ 0x140016, 0x140017, io_dip2_r },

	{ 0x140030, 0x140031, er_io_analog_r },

	{ 0x140000, 0x1400ff, MRA_EXTRAM3 },		//io
	{ 0xe00000, 0xe00001, er_reset2_r },
	{-1}
};

static struct MemoryWriteAddress enduror_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x040000, 0x043fff, MWA_EXTRAM },
	{ 0x100000, 0x107fff, MWA_TILERAM },
	{ 0x108000, 0x108fff, MWA_TEXTRAM },
	{ 0x110000, 0x110fff, MWA_PALETTERAM },
	{ 0x124000, 0x127fff, shared_ram_w, &shared_ram },
	{ 0x130000, 0x130fff, MWA_SPRITERAM },
	{ 0x140000, 0x140001, sound_command_nmi_w },
	{ 0x140000, 0x1400ff, MWA_EXTRAM3 },		//io
	{-1}
};


static READ_HANDLER( enduro_p2_skip_r )
{
	if (cpu_get_pc()==0x4ba) {cpu_spinuntil_int(); return 0xffff;}

	return READ_WORD(&shared_ram[0x2000]);
}

static struct MemoryReadAddress enduror_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xc68000, 0xc68fff, MRA_EXTRAM2 },
	{ 0xc7e000, 0xc7e001, enduro_p2_skip_r },
	{ 0xc7c000, 0xc7ffff, shared_ram_r },
	{-1}
};

static struct MemoryWriteAddress enduror_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xc68000, 0xc68fff, MWA_EXTRAM2 },
	{ 0xc7c000, 0xc7ffff, shared_ram_w, &shared_ram },
	{-1}
};

static struct MemoryReadAddress enduror_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd000, YM2203_status_port_0_r },
	{ 0xe000, 0xe7ff, SegaPCM_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress enduror_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xd000, 0xd000, YM2203_control_port_0_w },
	{ 0xd001, 0xd001, YM2203_write_port_0_w },
	{ 0xe000, 0xe7ff, SegaPCM_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort enduror_sound_readport[] =
{
	{ 0x40, 0x40, soundlatch_r },
	{ -1 }	/* end of table */
};


static struct IOWritePort enduror_sound_writeport[] =
{
	{ -1 }
};

static struct MemoryReadAddress enduror_b2_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
//	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xf000, 0xf7ff, SegaPCM_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress enduror_b2_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
//	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xf000, 0xf7ff, SegaPCM_w },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort enduror_b2_sound_readport[] =
{
	{ 0x00, 0x00, YM2203_status_port_0_r },
	{ 0x80, 0x80, YM2203_status_port_1_r },
	{ 0xc0, 0xc0, YM2203_status_port_2_r },
	{ 0x40, 0x40, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort enduror_b2_sound_writeport[] =
{
	{ 0x00, 0x00, YM2203_control_port_0_w },
	{ 0x01, 0x01, YM2203_write_port_0_w },
	{ 0x80, 0x80, YM2203_control_port_1_w },
	{ 0x81, 0x81, YM2203_write_port_1_w },
	{ 0xc0, 0xc0, YM2203_control_port_2_w },
	{ 0xc1, 0xc1, YM2203_write_port_2_w },
	{ -1 }
};

/***************************************************************************/
static void enduror_update_proc( void ){
	int data;
	sys16_fg_scrollx = READ_WORD( &sys16_textram[0x0ff8] ) & 0x01ff;
	sys16_bg_scrollx = READ_WORD( &sys16_textram[0x0ffa] ) & 0x01ff;
	sys16_fg_scrolly = READ_WORD( &sys16_textram[0x0f24] ) & 0x01ff;
	sys16_bg_scrolly = READ_WORD( &sys16_textram[0x0f26] ) & 0x01ff;

	data = READ_WORD( &sys16_textram[0x0e9e] );

	sys16_fg_page[0] = data>>12;
	sys16_fg_page[1] = (data>>8)&0xf;
	sys16_fg_page[3] = (data>>4)&0xf;
	sys16_fg_page[2] = data&0xf;

	data = READ_WORD( &sys16_textram[0x0e9c] );
	sys16_bg_page[0] = data>>12;
	sys16_bg_page[1] = (data>>8)&0xf;
	sys16_bg_page[3] = (data>>4)&0xf;
	sys16_bg_page[2] = data&0xf;

	data = READ_WORD( &sys16_extraram3[2] );
	set_refresh_3d( data );

	if(data & 4)
	{
		osd_led_w(0,1);
		osd_led_w(1,1);
		osd_led_w(2,1);
	}
	else
	{
		osd_led_w(0,0);
		osd_led_w(1,0);
		osd_led_w(2,0);
	}
}


static void enduror_init_machine( void ){
	static int bank[16] = { 00,01,02,03,04,05,06,07,00,00,00,00,00,00,00,00};
	sys16_obj_bank = bank;
	sys16_textmode=1;
	sys16_spritesystem = 6;
	sys16_sprxoffset = -0xc0;
	sys16_fgxoffset = 13;
//	sys16_sprxoffset = -0xbb;
//	sys16_fgxoffset = 8;
	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;

	sys16_update_proc = enduror_update_proc;

	gr_ver = &sys16_extraram2[0x0];
	gr_hor = gr_ver+0x200;
	gr_pal = gr_ver+0x400;
	gr_flip= gr_ver+0x600;
	gr_palette= 0xf80 / 2;
	gr_palette_default = 0x70 /2;
	gr_colorflip[0][0]=0x00 / 2;
	gr_colorflip[0][1]=0x02 / 2;
	gr_colorflip[0][2]=0x04 / 2;
	gr_colorflip[0][3]=0x00 / 2;
	gr_colorflip[1][0]=0x00 / 2;
	gr_colorflip[1][1]=0x00 / 2;
	gr_colorflip[1][2]=0x06 / 2;
	gr_colorflip[1][3]=0x00 / 2;

#ifdef TRANSPARENT_SHADOWS
	sys16_sh_shadowpal=0xff;
#endif
}



static void enduror_sprite_decode( void ){
	unsigned char *RAM = memory_region(REGION_CPU1);
	sys16_sprite_decode2( 8,0x020000 ,1);
	generate_gr_screen(512,1024,8,0,4,0x8000);

//	enduror_decode_data (RAM,RAM,0x10000);	// no decrypt info.
	enduror_decode_data (RAM+0x10000,RAM+0x10000,0x10000);
	enduror_decode_data2(RAM+0x20000,RAM+0x20000,0x10000);
}

static void endurob_sprite_decode( void ){
	sys16_sprite_decode2( 8,0x020000 ,1);
	generate_gr_screen(512,1024,8,0,4,0x8000);
}

static void endurora_opcode_decode( void )
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = 0x50000;	/* place decrypted opcodes in a hole after RAM */


	memory_set_opcode_base(0,rom+diff);

	memcpy(rom+diff+0x10000,rom+0x10000,0x20000);
	memcpy(rom+diff,rom+0x30000,0x10000);

	// patch code to force a reset on cpu2 when starting a new game.
	// Undoubtly wrong, but something like it is needed for the game to work
	WRITE_WORD(&rom[0x1866 + diff],0x4a79);
	WRITE_WORD(&rom[0x1868 + diff],0x00e0);
	WRITE_WORD(&rom[0x186a + diff],0x0000);
}


static void endurob2_opcode_decode( void )
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = 0x50000;	/* place decrypted opcodes in a hole after RAM */


	memory_set_opcode_base(0,rom+diff);

	memcpy(rom+diff,rom,0x30000);

	endurob2_decode_data (rom,rom+diff,0x10000);
	endurob2_decode_data2(rom+0x10000,rom+diff+0x10000,0x10000);

	// patch code to force a reset on cpu2 when starting a new game.
	// Undoubtly wrong, but something like it is needed for the game to work
	WRITE_WORD(&rom[0x1866 + diff],0x4a79);
	WRITE_WORD(&rom[0x1868 + diff],0x00e0);
	WRITE_WORD(&rom[0x186a + diff],0x0000);
}

static void init_enduror( void )
{
	sys16_onetime_init_machine();
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=NumOfShadowColors / 2;
#endif
//	sys16_MaxShadowColors=0;

	enduror_sprite_decode();
}

static void init_endurobl( void )
{
	sys16_onetime_init_machine();
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=NumOfShadowColors / 2;
#endif
//	sys16_MaxShadowColors=0;

	endurob_sprite_decode();
	endurora_opcode_decode();
}

static void init_endurob2( void )
{
	sys16_onetime_init_machine();
#ifdef TRANSPARENT_SHADOWS
	sys16_MaxShadowColors=NumOfShadowColors / 2;
#endif
//	sys16_MaxShadowColors=0;

	endurob_sprite_decode();
	endurob2_opcode_decode();
}


/***************************************************************************/

INPUT_PORTS_START( enduror )
PORT_START	/* handle right left */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_REVERSE | IPF_CENTER, 100, 4, 0x0, 0xff )

PORT_START	/* Fake Buttons */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )	// accel
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )	// brake
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )	// wheelie

PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE

PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, "Moving" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x06, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x18, 0x18, "Time Adjust" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x60, 0x60, "Time Control" )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

//PORT_START	/* Y */
//	PORT_ANALOG( 0xff, 0x0, IPT_AD_STICK_Y | IPF_CENTER , 100, 8, 0x0, 0xff )

INPUT_PORTS_END

/***************************************************************************/

static struct MachineDriver machine_driver_enduror =
{
	{
		{
			CPU_M68000,
			10000000,
			enduror_readmem,enduror_writemem,0,0,
			sys16_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4096000,
			enduror_sound_readmem,enduror_sound_writemem,enduror_sound_readport,enduror_sound_writeport,
			interrupt,4
		},
		{
			CPU_M68000,
			10000000,
			enduror_readmem2,enduror_writemem2,0,0,
			sys16_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	enduror_init_machine,
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	gfx8,
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sys16_ho_vh_start,
	sys16_vh_stop,
	sys16_ho_vh_screenrefresh,
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_SEGAPCM,
			&segapcm_interface_32k,
		}
	}
};

static struct MachineDriver machine_driver_endurob2 =
{
	{
		{
			CPU_M68000,
			10000000,
			enduror_readmem,enduror_writemem,0,0,
			sys16_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4096000,
			enduror_b2_sound_readmem,enduror_b2_sound_writemem,enduror_b2_sound_readport,enduror_b2_sound_writeport,
			ignore_interrupt,1
		},
		{
			CPU_M68000,
			10000000,
			enduror_readmem2,enduror_writemem2,0,0,
			sys16_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	2,
	enduror_init_machine,
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	gfx8,
	2048*ShadowColorsMultiplier,2048*ShadowColorsMultiplier,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sys16_ho_vh_start,
	sys16_vh_stop,
	sys16_ho_vh_screenrefresh,
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface2
		},
		{
			SOUND_SEGAPCM,
			&segapcm_interface_15k,
		}
	}
};

/*****************************************************************************/
/* Dummy drivers for games that don't have a working clone and are protected */
/*****************************************************************************/

static struct MemoryReadAddress sys16_dummy_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0xff0000, 0xffffff, MRA_WORKINGRAM },
	{-1}
};

static struct MemoryWriteAddress sys16_dummy_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },
	{ 0xff0000, 0xffffff, MWA_WORKINGRAM },

	{-1}
};

static void sys16_dummy_init_machine( void ){
	static int bank[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	sys16_obj_bank = bank;
}

static void init_s16dummy( void )
{
	sys16_onetime_init_machine();
//	sys16_sprite_decode( 4,0x040000 );
}

INPUT_PORTS_START( s16dummy )
INPUT_PORTS_END

MACHINE_DRIVER( machine_driver_s16dummy, \
	sys16_dummy_readmem,sys16_dummy_writemem,sys16_dummy_init_machine, gfx8)

/*****************************************************************************/
// Ace Attacker

// sys18
ROM_START( aceattac )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "11491.4a", 0x000000, 0x10000, 0x77b820f1 )
	ROM_LOAD_ODD ( "11489.1a", 0x000000, 0x10000, 0xbbe623c5 )
	ROM_LOAD_EVEN( "11492.5a", 0x020000, 0x10000, 0xd8bd3139 )
	ROM_LOAD_ODD ( "11490.2a", 0x020000, 0x10000, 0x38cb3a41 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11493.9b",  0x00000, 0x10000, 0x654485d9 )
	ROM_LOAD( "11494.10b", 0x10000, 0x10000, 0xb67971ab )
	ROM_LOAD( "11495.11b", 0x20000, 0x10000, 0xb687ab61 )

	ROM_REGION( 0x80000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "11501.1b", 0x00000, 0x10000, 0x09179ead )
	ROM_LOAD( "11502.2b", 0x10000, 0x10000, 0xa3ee36b8 )
	ROM_LOAD( "11503.3b", 0x20000, 0x10000, 0x344c0692 )
	ROM_LOAD( "11504.4b", 0x30000, 0x10000, 0x7cae7920 )
	ROM_LOAD( "11505.5b", 0x40000, 0x10000, 0xb67f1ecf )
	ROM_LOAD( "11506.6b", 0x50000, 0x10000, 0xb0104def )
	ROM_LOAD( "11507.7b", 0x60000, 0x10000, 0xa2af710a )
	ROM_LOAD( "11508.8b", 0x70000, 0x10000, 0x5cbb833c )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "11496.7a",	 0x00000, 0x08000, 0x82cb40a9 )
	ROM_LOAD( "11497.8a",    0x10000, 0x08000, 0xb04f62cc )
	ROM_LOAD( "11498.9a",    0x18000, 0x08000, 0x97baf52b )
	ROM_LOAD( "11499.10a",   0x20000, 0x08000, 0xea332866 )
	ROM_LOAD( "11500.11a",   0x28000, 0x08000, 0x2ddf1c31 )
ROM_END

/*****************************************************************************/
/*****************************************************************************/
// After Burner

ROM_START( aburner )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD( "epr10949.bin",0x000000,0x20000, 0xd8437d92 )
	ROM_LOAD( "epr10948.bin",0x000000,0x20000, 0x64284761 )
	ROM_LOAD( "epr10947.bin",0x000000,0x20000, 0x08838392 )
	ROM_LOAD( "epr10946.bin",0x000000,0x20000, 0xd7d485f4 )
	ROM_LOAD( "epr10945.bin",0x000000,0x20000, 0xdf4d4c4f )
	ROM_LOAD( "epr10944.bin",0x000000,0x20000, 0x17be8f67 )
	ROM_LOAD( "epr10943.bin",0x000000,0x20000, 0xb98294dc )
	ROM_LOAD( "epr10942.bin",0x000000,0x20000, 0x5ce10b8c )
	ROM_LOAD( "epr10941.bin",0x000000,0x20000, 0x136ea264 )
	ROM_LOAD( "epr10940.bin",0x000000,0x20000, 0x4d132c4e )
	ROM_LOAD( "epr10928.bin",0x000000,0x20000, 0x7c01d40b )
	ROM_LOAD( "epr10927.bin",0x000000,0x20000, 0x66d36757 )
	ROM_LOAD( "epr10926.bin",0x000000,0x10000, 0xed8bd632 )
	ROM_LOAD( "epr10925.bin",0x000000,0x10000, 0x4ef048cc )
	ROM_LOAD( "epr10924.bin",0x000000,0x10000, 0x50c15a6d )
	ROM_LOAD( "epr10923.bin",0x000000,0x10000, 0x6888eb8f )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 2nd 68000 code */

	ROM_REGION( 0x40000, REGION_GFX3 ) /* gr */
ROM_END


/*****************************************************************************/
// After Burner II

ROM_START( aburner2 )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "11107.58",  0x000000, 0x20000, 0x6d87bab7 )
	ROM_LOAD_ODD ( "11108.104", 0x000000, 0x20000, 0x202a3e1d )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11115.154", 0x00000, 0x10000, 0xe8e32921 )
	ROM_LOAD( "11114.153", 0x10000, 0x10000, 0x2e97f633 )
	ROM_LOAD( "11113.152", 0x20000, 0x10000, 0x36058c8c )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10932.125", 0x000000, 0x20000, 0xcc0821d6 )
	ROM_LOAD( "10933.126", 0x020000, 0x20000, 0xc8efb2c3 )
	ROM_LOAD( "10934.129", 0x040000, 0x20000, 0x4a51b1fa )
	ROM_LOAD( "10935.130", 0x060000, 0x20000, 0xc1e23521 )

	ROM_LOAD( "10936.133", 0x080000, 0x20000, 0xada70d64 )
	ROM_LOAD( "10937.134", 0x0a0000, 0x20000, 0x00a6144f )
	ROM_LOAD( "10938.102", 0x0c0000, 0x20000, 0xe7675baf )
	ROM_LOAD( "10939.103", 0x0e0000, 0x20000, 0xa0d49480 )

	ROM_LOAD( "11103.127", 0x100000, 0x20000, 0xbdd60da2 )
	ROM_LOAD( "11104.131", 0x120000, 0x20000, 0x06a35fce )
	ROM_LOAD( "11105.135", 0x140000, 0x20000, 0x027b0689 )
	ROM_LOAD( "11106.104", 0x160000, 0x20000, 0x9e1fec09 )

	ROM_LOAD( "11116.128", 0x180000, 0x20000, 0x49b4c1ba )
	ROM_LOAD( "11117.132", 0x1a0000, 0x20000, 0x821fbb71 )
	ROM_LOAD( "11118.136", 0x1c0000, 0x20000, 0x8f38540b )
	ROM_LOAD( "11119.105", 0x1e0000, 0x20000, 0xd0343a8e )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "11112.17",    0x00000, 0x10000, 0xd777fc6d )
	ROM_LOAD( "11102.13",    0x10000, 0x20000, 0x6c07c78d )
	ROM_LOAD( "10931.11",    0x30000, 0x20000, 0x9209068f )
	ROM_LOAD( "10930.12",    0x30000, 0x20000, 0x6493368b )

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 2nd 68000 code */
	ROM_LOAD_EVEN( "11109.20", 0x000000, 0x20000, 0x85a0fe07 )
	ROM_LOAD_ODD ( "11110.29", 0x000000, 0x20000, 0xf3d6797c )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* gr */
	ROM_LOAD_ODD ( "10922.40", 0x000000, 0x10000, 0xb49183d4 )
ROM_END


/*****************************************************************************/
// Bloxeed

// sys18
ROM_START( bloxeed )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "rom-e.rom", 0x000000, 0x20000, 0xa481581a )
	ROM_LOAD_ODD ( "rom-o.rom", 0x000000, 0x20000, 0xdd1bc3bf )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "scr0.rom", 0x00000, 0x10000, 0xe024aa33 )
	ROM_LOAD( "scr1.rom", 0x10000, 0x10000, 0x8041b814 )
	ROM_LOAD( "scr2.rom", 0x20000, 0x10000, 0xde32285e )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "obj0-e.rom", 0x00000, 0x10000, 0x90d31a8c )
	ROM_LOAD( "obj0-o.rom", 0x10000, 0x10000, 0xf0c0f49d )

	ROM_REGION( 0x20000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "sound0.rom",	 0x00000, 0x20000, 0x6f2fc63c )
ROM_END


/*****************************************************************************/
// Clutch Hitter
// sys18
ROM_START( cltchitr )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr13795.6a", 0x000000, 0x40000, 0xb0b60b67 )
	ROM_LOAD_ODD ( "epr13751.4a", 0x000000, 0x40000, 0xc8d80233 )
	ROM_LOAD_EVEN( "epr13786.7a", 0x080000, 0x40000, 0x3095dac0 )
	ROM_LOAD_ODD ( "epr13784.5a", 0x080000, 0x40000, 0x80c8180d )

	ROM_REGION( 0x180000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13787.10a", 0x000000, 0x80000, 0xf05c68c6 )
	ROM_LOAD( "mpr13788.11a", 0x080000, 0x80000, 0x0106fea6 )
	ROM_LOAD( "mpr13789.12a", 0x100000, 0x80000, 0x09ba8835 )

	ROM_REGION( 0x300000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "mpr13773.1c",  0x000000, 0x80000, 0x3fc600e5 )
	ROM_LOAD( "mpr13774.2c",  0x080000, 0x80000, 0x2411a824 )
	ROM_LOAD( "mpr13775.3c",  0x100000, 0x80000, 0xcf527bf6 )
	ROM_LOAD( "mpr13779.10c", 0x180000, 0x80000, 0xc707f416 )
	ROM_LOAD( "mpr13780.11c", 0x200000, 0x80000, 0xa4c341e0 )
	ROM_LOAD( "mpr13781.12c", 0x280000, 0x80000, 0xf33b13af )

	ROM_REGION( 0x180000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr13793.7c",    0x000000, 0x80000, 0xa3d31944 )
	ROM_LOAD( "epr13791.5c",	0x080000, 0x80000, 0x35c16d80 )
	ROM_LOAD( "epr13792.6c",    0x100000, 0x80000, 0x808f9695 )
ROM_END


/*****************************************************************************/
// Cotton

ROM_START( cotton )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-?????
	ROM_LOAD_EVEN( "epr13858.a7", 0x000000, 0x20000, 0x276f42fe )
	ROM_LOAD_ODD ( "epr13856.a5", 0x000000, 0x20000, 0x14e6b5e7 )
	ROM_LOAD_EVEN( "epr13859.a8", 0x040000, 0x20000, 0x4703ef9d )
	ROM_LOAD_ODD ( "epr13857.a6", 0x040000, 0x20000, 0xde37e527 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "scr01.rom", 0x00000, 0x20000, 0xa47354b6 )
	ROM_LOAD( "scr11.rom", 0x20000, 0x20000, 0xd38424b5 )
	ROM_LOAD( "scr02.rom", 0x40000, 0x20000, 0x8c990026 )
	ROM_LOAD( "scr12.rom", 0x60000, 0x20000, 0x21c15b8a )
	ROM_LOAD( "scr03.rom", 0x80000, 0x20000, 0xd2b175bf )
	ROM_LOAD( "scr13.rom", 0xa0000, 0x20000, 0xb9d62531 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "obj0-e.rom", 0x000000, 0x20000, 0xab4b3468 )
	ROM_LOAD( "obj0-o.rom", 0x020000, 0x20000, 0x7024f404 )
	ROM_LOAD( "obj1-e.rom", 0x040000, 0x20000, 0x69b41ac3 )
	ROM_LOAD( "obj1-o.rom", 0x060000, 0x20000, 0x6169bba4 )
	ROM_LOAD( "obj2-e.rom", 0x080000, 0x20000, 0x0801cf02 )
	ROM_LOAD( "obj2-o.rom", 0x0a0000, 0x20000, 0xb014f02d )
	ROM_LOAD( "obj3-e.rom", 0x0c0000, 0x20000, 0xf066f315 )
	ROM_LOAD( "obj3-o.rom", 0x0e0000, 0x20000, 0xe62a7cd6 )
	ROM_LOAD( "obj4-e.rom", 0x100000, 0x20000, 0x1bd145f3 )
	ROM_LOAD( "obj4-o.rom", 0x120000, 0x20000, 0x943aba8b )
	ROM_LOAD( "obj5-e.rom", 0x140000, 0x20000, 0x4fd59bff )
	ROM_LOAD( "obj5-o.rom", 0x160000, 0x20000, 0x7ea93200 )
	ROM_LOAD( "obj6-e.rom", 0x180000, 0x20000, 0x6a66868d )
	ROM_LOAD( "obj6-o.rom", 0x1a0000, 0x20000, 0x1c942190 )
	ROM_LOAD( "obj7-e.rom", 0x1c0000, 0x20000, 0x1c5ffad8 )
	ROM_LOAD( "obj7-o.rom", 0x1e0000, 0x20000, 0x856f3ee2 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "s-prog.rom",	 0x00000, 0x08000, 0x6a57b027 )
	ROM_LOAD( "speech0.rom", 0x10000, 0x20000, 0x4d21153f )
ROM_END


ROM_START( cottona )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0181a
	ROM_LOAD_EVEN( "ep13921a.a7", 0x000000, 0x20000, 0xf047a037 )
	ROM_LOAD_ODD ( "ep13919a.a5", 0x000000, 0x20000, 0x651108b1 )
	ROM_LOAD_EVEN( "ep13922a.a8", 0x040000, 0x20000, 0x1ca248c5 )
	ROM_LOAD_ODD ( "ep13920a.a6", 0x040000, 0x20000, 0xfa3610f9 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "scr01.rom", 0x00000, 0x20000, 0xa47354b6 )
	ROM_LOAD( "scr11.rom", 0x20000, 0x20000, 0xd38424b5 )
	ROM_LOAD( "scr02.rom", 0x40000, 0x20000, 0x8c990026 )
	ROM_LOAD( "scr12.rom", 0x60000, 0x20000, 0x21c15b8a )
	ROM_LOAD( "scr03.rom", 0x80000, 0x20000, 0xd2b175bf )
	ROM_LOAD( "scr13.rom", 0xa0000, 0x20000, 0xb9d62531 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "obj0-e.rom", 0x000000, 0x20000, 0xab4b3468 )
	ROM_LOAD( "obj0-o.rom", 0x020000, 0x20000, 0x7024f404 )
	ROM_LOAD( "obj1-e.rom", 0x040000, 0x20000, 0x69b41ac3 )
	ROM_LOAD( "obj1-o.rom", 0x060000, 0x20000, 0x6169bba4 )
	ROM_LOAD( "obj2-e.rom", 0x080000, 0x20000, 0x0801cf02 )
	ROM_LOAD( "obj2-o.rom", 0x0a0000, 0x20000, 0xb014f02d )
	ROM_LOAD( "obj3-e.rom", 0x0c0000, 0x20000, 0xf066f315 )
	ROM_LOAD( "obj3-o.rom", 0x0e0000, 0x20000, 0xe62a7cd6 )
	ROM_LOAD( "obj4-e.rom", 0x100000, 0x20000, 0x1bd145f3 )
	ROM_LOAD( "obj4-o.rom", 0x120000, 0x20000, 0x943aba8b )
	ROM_LOAD( "obj5-e.rom", 0x140000, 0x20000, 0x4fd59bff )
	ROM_LOAD( "obj5-o.rom", 0x160000, 0x20000, 0x7ea93200 )
	ROM_LOAD( "obj6-e.rom", 0x180000, 0x20000, 0x6a66868d )
	ROM_LOAD( "obj6-o.rom", 0x1a0000, 0x20000, 0x1c942190 )
	ROM_LOAD( "obj7-e.rom", 0x1c0000, 0x20000, 0x1c5ffad8 )
	ROM_LOAD( "obj7-o.rom", 0x1e0000, 0x20000, 0x856f3ee2 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "s-prog.rom",	 0x00000, 0x08000, 0x6a57b027 )
	ROM_LOAD( "speech0.rom", 0x10000, 0x20000, 0x4d21153f )
ROM_END


/*****************************************************************************/
// DD Crew

ROM_START( ddcrew )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "14153.6a", 0x000000, 0x40000, 0xe01fae0c )
	ROM_LOAD_ODD ( "14152.4a", 0x000000, 0x40000, 0x69c7b571 )
	ROM_LOAD_EVEN( "14141.7a", 0x080000, 0x40000, 0x080a494b )
	ROM_LOAD_ODD ( "14139.5a", 0x080000, 0x40000, 0x06c31531 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "14127.1c", 0x00000, 0x40000, 0x2228cd88 )
	ROM_LOAD( "14128.2c", 0x40000, 0x40000, 0xedba8e10 )
	ROM_LOAD( "14129.3c", 0x80000, 0x40000, 0xe8ecc305 )

	ROM_REGION( 0x400000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "14134.10c", 0x000000, 0x80000, 0x4fda6a4b )
	ROM_LOAD( "14142.10a", 0x080000, 0x80000, 0x3cbf1f2a )
	ROM_LOAD( "14135.11c", 0x100000, 0x80000, 0xe9c74876 )
	ROM_LOAD( "14143.11a", 0x180000, 0x80000, 0x59022c31 )
	ROM_LOAD( "14136.12c", 0x200000, 0x80000, 0x720d9858 )
	ROM_LOAD( "14144.12a", 0x280000, 0x80000, 0x7775fdd4 )
	ROM_LOAD( "14137.13c", 0x300000, 0x80000, 0x846c4265 )
	ROM_LOAD( "14145.13a", 0x380000, 0x80000, 0x0e76c797 )

	ROM_REGION( 0x1a0000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "14133.7c",	 0x000000, 0x20000, 0xcff96665 )
	ROM_LOAD( "14130.4c",    0x020000, 0x80000, 0x948f34a1 )
	ROM_LOAD( "14131.5c",    0x0a0000, 0x80000, 0xbe5a7d0b )
	ROM_LOAD( "14132.6c",    0x120000, 0x80000, 0x1fae0220 )
ROM_END


/*****************************************************************************/
// Dunk Shot

ROM_START( dunkshot )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "10468.bin", 0x000000, 0x8000, 0xe2d5f97a )
	ROM_LOAD_ODD ( "10467.bin", 0x000000, 0x8000, 0x29774114 )
	ROM_LOAD_EVEN( "10470.bin", 0x010000, 0x8000, 0x8c60761f )
	ROM_LOAD_ODD ( "10469.bin", 0x010000, 0x8000, 0xaa442b81 )
	ROM_LOAD_EVEN( "10472.bin", 0x020000, 0x8000, 0x206027a6 )
	ROM_LOAD_ODD ( "10471.bin", 0x020000, 0x8000, 0x22777314 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10485.bin", 0x00000, 0x8000, 0xf16dda29 )
	ROM_LOAD( "10486.bin", 0x08000, 0x8000, 0x311d973c )
	ROM_LOAD( "10487.bin", 0x10000, 0x8000, 0xa8fb179f )

	ROM_REGION( 0x40000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10481.bin", 0x00000, 0x8000, 0xfeb04bc9 )
	ROM_LOAD( "10477.bin", 0x08000, 0x8000, 0xf9d3b2cb )
	ROM_LOAD( "10482.bin", 0x10000, 0x8000, 0x5bc07618 )
	ROM_LOAD( "10478.bin", 0x18000, 0x8000, 0x5b5c5c92 )
	ROM_LOAD( "10483.bin", 0x20000, 0x8000, 0x7cab4f9e )
	ROM_LOAD( "10479.bin", 0x28000, 0x8000, 0xe84190a0 )
	ROM_LOAD( "10484.bin", 0x30000, 0x8000, 0xbcb5fcc9 )
	ROM_LOAD( "10480.bin", 0x38000, 0x8000, 0x5dffd9dd )

	ROM_REGION( 0x28000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10473.bin",	 0x00000, 0x08000, 0x7f1f5a27 )
	ROM_LOAD( "10474.bin",   0x10000, 0x08000, 0x419a656e )
	ROM_LOAD( "10475.bin",   0x18000, 0x08000, 0x17d55e85 )
	ROM_LOAD( "10476.bin",   0x20000, 0x08000, 0xa6be0956 )
ROM_END



/*****************************************************************************/
// Laser Ghost

// sys18
ROM_START( lghost )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "13429", 0x000000, 0x20000, 0x0e0ccf26 )
	ROM_LOAD_ODD ( "13437", 0x000000, 0x20000, 0x38b4dc2f )
	ROM_LOAD_EVEN( "13411", 0x040000, 0x20000, 0xc3aeae07 )
	ROM_LOAD_ODD ( "13413", 0x040000, 0x20000, 0x75f43e21 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "13414", 0x00000, 0x20000, 0x82025f3b )
	ROM_LOAD( "13415", 0x20000, 0x20000, 0xa76852e9 )
	ROM_LOAD( "13416", 0x40000, 0x20000, 0xe88db149 )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "13603", 0x00000, 0x20000, 0x2e3cc07b )
	ROM_LOAD( "13604", 0x20000, 0x20000, 0x576388af )
	ROM_LOAD( "13421", 0x40000, 0x20000, 0xabee8771 )
	ROM_LOAD( "13424", 0x60000, 0x20000, 0x260ab077 )
	ROM_LOAD( "13422", 0x80000, 0x20000, 0x36cef12c )
	ROM_LOAD( "13425", 0xa0000, 0x20000, 0xe0ff8807 )
	ROM_LOAD( "13423", 0xc0000, 0x20000, 0x5b8e0053 )
	ROM_LOAD( "13426", 0xe0000, 0x20000, 0xc689853b )

	ROM_REGION( 0x80000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "13417",	 0x00000, 0x20000, 0xcd7beb49 )
	ROM_LOAD( "13420",   0x20000, 0x20000, 0x03199cbb )
	ROM_LOAD( "13419",   0x40000, 0x20000, 0xa918ef68 )
	ROM_LOAD( "13418",   0x60000, 0x20000, 0x4006c9f1 )
ROM_END

/*****************************************************************************/
// Line of Fire

ROM_START( loffire )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr12850.rom", 0x000000, 0x20000, 0x14598f2a )
	ROM_LOAD_ODD ( "epr12849.rom", 0x000000, 0x20000, 0x61cfd2fe )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr12791.rom", 0x00000, 0x10000, 0xacfa69ba )
	ROM_LOAD( "opr12792.rom", 0x10000, 0x10000, 0xe506723c )
	ROM_LOAD( "opr12793.rom", 0x20000, 0x10000, 0x0ce8cce3 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr12775.rom", 0x000000, 0x20000, 0x693056ec )
	ROM_LOAD( "epr12776.rom", 0x020000, 0x20000, 0x61efbdfd )
	ROM_LOAD( "epr12777.rom", 0x040000, 0x20000, 0x29d5b953 )
	ROM_LOAD( "epr12778.rom", 0x060000, 0x20000, 0x2fb68e07 )

	ROM_LOAD( "epr12779.rom", 0x080000, 0x20000, 0xae58af7c )
	ROM_LOAD( "epr12780.rom", 0x0a0000, 0x20000, 0xee670c1e )
	ROM_LOAD( "epr12781.rom", 0x0c0000, 0x20000, 0x538f6bc5 )
	ROM_LOAD( "epr12782.rom", 0x0e0000, 0x20000, 0x5acc34f7 )

	ROM_LOAD( "epr12783.rom", 0x100000, 0x20000, 0xc13feea9 )
	ROM_LOAD( "epr12784.rom", 0x120000, 0x20000, 0x39b94c65 )
	ROM_LOAD( "epr12785.rom", 0x140000, 0x20000, 0x05ed0059 )
	ROM_LOAD( "epr12786.rom", 0x160000, 0x20000, 0xa4123165 )

	ROM_LOAD( "epr12787.rom", 0x180000, 0x20000, 0x6431a3a6 )
	ROM_LOAD( "epr12788.rom", 0x1a0000, 0x20000, 0x1982a0ce )
	ROM_LOAD( "epr12789.rom", 0x1c0000, 0x20000, 0x97d03274 )
	ROM_LOAD( "epr12790.rom", 0x1e0000, 0x20000, 0x816e76e6 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12798.rom",	 0x00000, 0x10000, 0x0587738d )
	ROM_LOAD( "epr12799.rom",    0x10000, 0x20000, 0xbc60181c )
	ROM_LOAD( "epr12800.rom",    0x30000, 0x20000, 0x1158c1a3 )
	ROM_LOAD( "epr12801.rom",    0x50000, 0x20000, 0x2d6567c4 )

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 2nd 68000 code */
	ROM_LOAD_EVEN( "epr12803.rom", 0x000000, 0x20000, 0xc1d9e751 )
	ROM_LOAD_ODD ( "epr12802.rom", 0x000000, 0x20000, 0xd746bb39 )
	ROM_LOAD_EVEN( "epr12805.rom", 0x040000, 0x20000, 0x4a7200c3 )
	ROM_LOAD_ODD ( "epr12804.rom", 0x040000, 0x20000, 0xb853480e )
ROM_END


/*****************************************************************************/
// MVP

ROM_START( mvp )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "13000.rom", 0x000000, 0x40000, 0x2e0e21ec )
	ROM_LOAD_ODD ( "12999.rom", 0x000000, 0x40000, 0xfd213d28 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "13011.rom", 0x00000, 0x40000, 0x1cb871fc )
	ROM_LOAD( "13012.rom", 0x40000, 0x40000, 0xb75e6821 )
	ROM_LOAD( "13013.rom", 0x80000, 0x40000, 0xf1944a3c )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "13010.rom", 0x000000, 0x40000, 0xdf37c567 )
	ROM_LOAD( "13009.rom", 0x040000, 0x40000, 0x126d2e37 )
	ROM_LOAD( "13006.rom", 0x080000, 0x40000, 0x2e9afd2f )
	ROM_LOAD( "13003.rom", 0x0c0000, 0x40000, 0x21424151 )
	ROM_LOAD( "13007.rom", 0x100000, 0x40000, 0x55c8605b )
	ROM_LOAD( "13004.rom", 0x140000, 0x40000, 0x0aa09dd3 )
	ROM_LOAD( "13008.rom", 0x180000, 0x40000, 0xb3d46dfc )
	ROM_LOAD( "13005.rom", 0x1c0000, 0x40000, 0xc899c810 )

	ROM_REGION( 0x50000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "13002.rom",	 0x00000, 0x08000, 0x1b6e1515 )
	ROM_LOAD( "13001.rom",   0x10000, 0x40000, 0xe8cace8c )
ROM_END


/*****************************************************************************/
// Thunder Blade

// after burner hardware
ROM_START( thndrbld )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "thnbld.58", 0x000000, 0x20000, 0xe057dd5a )
	ROM_LOAD_ODD ( "thnbld.63", 0x000000, 0x20000, 0xc6b994b8 )
	ROM_LOAD_EVEN( "11306.epr", 0x040000, 0x20000, 0x4b95f2b4 )
	ROM_LOAD_ODD ( "11307.epr", 0x040000, 0x20000, 0x2d6833e4 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11316.epr", 0x00000, 0x10000, 0x84290dff )
	ROM_LOAD( "11315.epr", 0x10000, 0x10000, 0x35813088 )
	ROM_LOAD( "11314.epr", 0x20000, 0x10000, 0xd4f954a9 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "thnbld.105",0x000000, 0x20000, 0xb4a382f7 )
	ROM_LOAD( "thnbld.101",0x020000, 0x20000, 0x525e2e1d )
	ROM_LOAD( "thnbld.97", 0x040000, 0x20000, 0x5f2783be )
	ROM_LOAD( "thnbld.93", 0x060000, 0x20000, 0x90775579 )

	ROM_LOAD( "11328.epr", 0x080000, 0x20000, 0xda39e89c )
	ROM_LOAD( "11329.epr", 0x0a0000, 0x20000, 0x31b20257 )
	ROM_LOAD( "11330.epr", 0x0c0000, 0x20000, 0xaa7c70c5 )
	ROM_LOAD( "11331.epr", 0x0e0000, 0x20000, 0x3a2c042e )

	ROM_LOAD( "11324.epr", 0x100000, 0x20000, 0x9742b552 )
	ROM_LOAD( "11325.epr", 0x120000, 0x20000, 0xb9e98ae9 )
	ROM_LOAD( "11326.epr", 0x140000, 0x20000, 0x29198403 )
	ROM_LOAD( "11327.epr", 0x160000, 0x20000, 0xdeae90f1 )

	ROM_LOAD( "11320.epr", 0x180000, 0x20000, 0xa95c76b8 )
//	ROM_LOAD( "11321.epr", 0x1a0000, 0x20000, 0x8e738f58 )
	ROM_LOAD( "thnbld.98", 0x1a0000, 0x10000, 0xeb4b9e57 )
	ROM_LOAD( "11322.epr", 0x1c0000, 0x20000, 0x10364d74 )
	ROM_LOAD( "11323.epr", 0x1e0000, 0x20000, 0x27e40735 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "thnbld.17",	 0x00000, 0x10000, 0xd37b54a4 )
	ROM_LOAD( "11317.epr",   0x10000, 0x20000, 0xd4e7ac1f )
	ROM_LOAD( "11318.epr",   0x30000, 0x20000, 0x70d3f02c )
	ROM_LOAD( "11319.epr",   0x50000, 0x20000, 0x50d9242e )

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 2nd 68000 code */
	ROM_LOAD_EVEN( "thnbld.20", 0x000000, 0x20000, 0xed988fdb )
	ROM_LOAD_ODD ( "thnbld.29", 0x000000, 0x20000, 0x12523bc1 )
	ROM_LOAD_EVEN( "11310.epr", 0x040000, 0x20000, 0x5d9fa02c )
	ROM_LOAD_ODD ( "11311.epr", 0x040000, 0x20000, 0x483de21b )

	ROM_REGION( 0x10000, REGION_GFX3 ) /* ???? */
	ROM_LOAD( "11313.epr",	 0x00000, 0x10000, 0x6a56c4c3 )
ROM_END


// Thunder Blade Japan

// after burner hardware
ROM_START( thndrbdj )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "11304.epr", 0x000000, 0x20000, 0xa90630ef )
	ROM_LOAD_ODD ( "11305.epr", 0x000000, 0x20000, 0x9ba3ef61 )
	ROM_LOAD_EVEN( "11306.epr", 0x040000, 0x20000, 0x4b95f2b4 )
	ROM_LOAD_ODD ( "11307.epr", 0x040000, 0x20000, 0x2d6833e4 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "11316.epr", 0x00000, 0x10000, 0x84290dff )
	ROM_LOAD( "11315.epr", 0x10000, 0x10000, 0x35813088 )
	ROM_LOAD( "11314.epr", 0x20000, 0x10000, 0xd4f954a9 )

	ROM_REGION( 0x200000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "11332.epr", 0x000000, 0x20000, 0xdc089ec6 )
	ROM_LOAD( "11333.epr", 0x020000, 0x20000, 0x05a2333f )
	ROM_LOAD( "11334.epr", 0x040000, 0x20000, 0x348f91c7 )
	ROM_LOAD( "11335.epr", 0x060000, 0x20000, 0xf19b3e86 )

	ROM_LOAD( "11328.epr", 0x080000, 0x20000, 0xda39e89c )
	ROM_LOAD( "11329.epr", 0x0a0000, 0x20000, 0x31b20257 )
	ROM_LOAD( "11330.epr", 0x0c0000, 0x20000, 0xaa7c70c5 )
	ROM_LOAD( "11331.epr", 0x0e0000, 0x20000, 0x3a2c042e )

	ROM_LOAD( "11324.epr", 0x100000, 0x20000, 0x9742b552 )
	ROM_LOAD( "11325.epr", 0x120000, 0x20000, 0xb9e98ae9 )
	ROM_LOAD( "11326.epr", 0x140000, 0x20000, 0x29198403 )
	ROM_LOAD( "11327.epr", 0x160000, 0x20000, 0xdeae90f1 )

	ROM_LOAD( "11320.epr", 0x180000, 0x20000, 0xa95c76b8 )
	ROM_LOAD( "11321.epr", 0x1a0000, 0x20000, 0x8e738f58 )
	ROM_LOAD( "11322.epr", 0x1c0000, 0x20000, 0x10364d74 )
	ROM_LOAD( "11323.epr", 0x1e0000, 0x20000, 0x27e40735 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "11312.epr",   0x00000, 0x10000, 0x3b974ed2 )
	ROM_LOAD( "11317.epr",   0x10000, 0x20000, 0xd4e7ac1f )
	ROM_LOAD( "11318.epr",   0x30000, 0x20000, 0x70d3f02c )
	ROM_LOAD( "11319.epr",   0x50000, 0x20000, 0x50d9242e )

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 2nd 68000 code */
	ROM_LOAD_EVEN( "11308.epr", 0x000000, 0x20000, 0x7956c238 )
	ROM_LOAD_ODD ( "11309.epr", 0x000000, 0x20000, 0xc887f620 )
	ROM_LOAD_EVEN( "11310.epr", 0x040000, 0x20000, 0x5d9fa02c )
	ROM_LOAD_ODD ( "11311.epr", 0x040000, 0x20000, 0x483de21b )

	ROM_REGION( 0x10000, REGION_GFX3 ) /* ???? */
	ROM_LOAD( "11313.epr",	 0x00000, 0x10000, 0x6a56c4c3 )
ROM_END


/*****************************************************************************/
// Turbo Outrun

ROM_START( toutrun )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0106
	ROM_LOAD_EVEN( "epr12397.133", 0x000000, 0x10000, 0xe4b57d7d )
	ROM_LOAD_ODD ( "epr12396.118", 0x000000, 0x10000, 0x5e7115cb )
	ROM_LOAD_EVEN( "epr12399.132", 0x020000, 0x10000, 0x62c77b1b )
	ROM_LOAD_ODD ( "epr12398.117", 0x020000, 0x10000, 0x18e34520 )
	ROM_LOAD_EVEN( "epr12293.131", 0x040000, 0x10000, 0xf4321eea )
	ROM_LOAD_ODD ( "epr12292.116", 0x040000, 0x10000, 0x51d98af0 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr12323.102", 0x00000, 0x10000, 0x4de43a6f )
	ROM_LOAD( "opr12324.103", 0x10000, 0x10000, 0x24607a55 )
	ROM_LOAD( "opr12325.104", 0x20000, 0x10000, 0x1405137a )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "opr12307.9",  0x00000, 0x10000, 0x437dcf09 )
	ROM_LOAD( "opr12308.10", 0x10000, 0x10000, 0x0de70cc2 )
	ROM_LOAD( "opr12309.11", 0x20000, 0x10000, 0xdeb8c242 )
	ROM_LOAD( "opr12310.12", 0x30000, 0x10000, 0x45cf157e )

	ROM_LOAD( "opr12311.13", 0x40000, 0x10000, 0xae2bd639 )
	ROM_LOAD( "opr12312.14", 0x50000, 0x10000, 0x626000e7 )
	ROM_LOAD( "opr12313.15", 0x60000, 0x10000, 0x52870c37 )
	ROM_LOAD( "opr12314.16", 0x70000, 0x10000, 0x40c461ea )

	ROM_LOAD( "opr12315.17", 0x80000, 0x10000, 0x3ff9a3a3 )
	ROM_LOAD( "opr12316.18", 0x90000, 0x10000, 0x8a1e6dc8 )
	ROM_LOAD( "opr12317.19", 0xa0000, 0x10000, 0x77e382d4 )
	ROM_LOAD( "opr12318.20", 0xb0000, 0x10000, 0xd1afdea9 )

	ROM_LOAD( "opr12320.22", 0xc0000, 0x10000, 0x7931e446 )
	ROM_LOAD( "opr12321.23", 0xd0000, 0x10000, 0x830bacd4 )
	ROM_LOAD( "opr12322.24", 0xe0000, 0x10000, 0x8b812492 )
	ROM_LOAD( "opr12319.25", 0xf0000, 0x10000, 0xdf23baf9 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12300.88",	0x00000, 0x10000, 0xe8ff7011 )
	ROM_LOAD( "opr12301.66",    0x10000, 0x10000, 0x6e78ad15 )
	ROM_LOAD( "opr12302.67",    0x20000, 0x10000, 0xe72928af )
	ROM_LOAD( "opr12303.68",    0x30000, 0x10000, 0x8384205c )
	ROM_LOAD( "opr12304.69",    0x40000, 0x10000, 0xe1762ac3 )
	ROM_LOAD( "opr12305.70",    0x50000, 0x10000, 0xba9ce677 )
	ROM_LOAD( "opr12306.71",    0x60000, 0x10000, 0xe49249fd )

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 68000 code */
	ROM_LOAD_EVEN( "opr12295.76", 0x000000, 0x10000, 0xd43a3a84 )
	ROM_LOAD_ODD ( "opr12294.58", 0x000000, 0x10000, 0x27cdcfd3 )
	ROM_LOAD_EVEN( "opr12297.75", 0x020000, 0x10000, 0x1d9b5677 )
	ROM_LOAD_ODD ( "opr12296.57", 0x020000, 0x10000, 0x0a513671 )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* road */
	ROM_LOAD_ODD ( "epr12298.11", 0x000000, 0x08000, 0xfc9bc41b )
ROM_END


ROM_START( toutruna )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
// custom cpu 317-0106
	ROM_LOAD_EVEN( "epr12410.133", 0x000000, 0x10000, 0xaa74f3e9 )
	ROM_LOAD_ODD ( "epr12409.118", 0x000000, 0x10000, 0xc11c8ef7 )
	ROM_LOAD_EVEN( "epr12412.132", 0x020000, 0x10000, 0xb0534647 )
	ROM_LOAD_ODD ( "epr12411.117", 0x020000, 0x10000, 0x12bb0d83 )
	ROM_LOAD_EVEN( "epr12293.131", 0x040000, 0x10000, 0xf4321eea )
	ROM_LOAD_ODD ( "epr12292.116", 0x040000, 0x10000, 0x51d98af0 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "opr12323.102", 0x00000, 0x10000, 0x4de43a6f )
	ROM_LOAD( "opr12324.103", 0x10000, 0x10000, 0x24607a55 )
	ROM_LOAD( "opr12325.104", 0x20000, 0x10000, 0x1405137a )

	ROM_REGION( 0x100000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "opr12307.9",  0x00000, 0x10000, 0x437dcf09 )
	ROM_LOAD( "opr12308.10", 0x10000, 0x10000, 0x0de70cc2 )
	ROM_LOAD( "opr12309.11", 0x20000, 0x10000, 0xdeb8c242 )
	ROM_LOAD( "opr12310.12", 0x30000, 0x10000, 0x45cf157e )

	ROM_LOAD( "opr12311.13", 0x40000, 0x10000, 0xae2bd639 )
	ROM_LOAD( "opr12312.14", 0x50000, 0x10000, 0x626000e7 )
	ROM_LOAD( "opr12313.15", 0x60000, 0x10000, 0x52870c37 )
	ROM_LOAD( "opr12314.16", 0x70000, 0x10000, 0x40c461ea )

	ROM_LOAD( "opr12315.17", 0x80000, 0x10000, 0x3ff9a3a3 )
	ROM_LOAD( "opr12316.18", 0x90000, 0x10000, 0x8a1e6dc8 )
	ROM_LOAD( "opr12317.19", 0xa0000, 0x10000, 0x77e382d4 )
	ROM_LOAD( "opr12318.20", 0xb0000, 0x10000, 0xd1afdea9 )

	ROM_LOAD( "opr12320.22", 0xc0000, 0x10000, 0x7931e446 )
	ROM_LOAD( "opr12321.23", 0xd0000, 0x10000, 0x830bacd4 )
	ROM_LOAD( "opr12322.24", 0xe0000, 0x10000, 0x8b812492 )
	ROM_LOAD( "opr12319.25", 0xf0000, 0x10000, 0xdf23baf9 )

	ROM_REGION( 0x70000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr12300.88",	0x00000, 0x10000, 0xe8ff7011 )
	ROM_LOAD( "opr12301.66",    0x10000, 0x10000, 0x6e78ad15 )
	ROM_LOAD( "opr12302.67",    0x20000, 0x10000, 0xe72928af )
	ROM_LOAD( "opr12303.68",    0x30000, 0x10000, 0x8384205c )
	ROM_LOAD( "opr12304.69",    0x40000, 0x10000, 0xe1762ac3 )
	ROM_LOAD( "opr12305.70",    0x50000, 0x10000, 0xba9ce677 )
	ROM_LOAD( "opr12306.71",    0x60000, 0x10000, 0xe49249fd )

	ROM_REGION( 0x100000, REGION_CPU3 ) /* 68000 code */
	ROM_LOAD_EVEN( "opr12295.76", 0x000000, 0x10000, 0xd43a3a84 )
	ROM_LOAD_ODD ( "opr12294.58", 0x000000, 0x10000, 0x27cdcfd3 )
	ROM_LOAD_EVEN( "opr12297.75", 0x020000, 0x10000, 0x1d9b5677 )
	ROM_LOAD_ODD ( "opr12296.57", 0x020000, 0x10000, 0x0a513671 )

	ROM_REGION( 0x40000, REGION_GFX3 ) /* road */
	ROM_LOAD_ODD ( "epr12298.11", 0x000000, 0x08000, 0xfc9bc41b )
ROM_END


/*****************************************************************************/
// Excite League

ROM_START( exctleag )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr11937.a02",0x00000,0x10000, 0x4ebda367 )
	ROM_LOAD_ODD ( "epr11936.a01",0x00000,0x10000, 0x0863de60 )
	ROM_LOAD_EVEN( "epr11939.a04",0x20000,0x10000, 0x117dd98f )
	ROM_LOAD_ODD ( "epr11938.a03",0x20000,0x10000, 0x07c08d47 )
	ROM_LOAD_EVEN( "epr11941.a06",0x40000,0x10000, 0x4df2d451 )
	ROM_LOAD_ODD ( "epr11940.a05",0x40000,0x10000, 0xdec83274 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr11942.b09",0x00000,0x10000, 0xeb70e827 )
	ROM_LOAD( "epr11943.b10",0x10000,0x10000, 0xd97c8982 )
	ROM_LOAD( "epr11944.b11",0x20000,0x10000, 0xa75cae80 )

	ROM_REGION( 0x80000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11950.b01",0x00000,0x10000, 0xaf497849 )
	ROM_LOAD( "epr11951.b02",0x10000,0x10000, 0xc04fa974 )
	ROM_LOAD( "epr11952.b03",0x20000,0x10000, 0xe64a9761 )
	ROM_LOAD( "epr11953.b04",0x30000,0x10000, 0x4cae3999 )
	ROM_LOAD( "epr11954.b05",0x40000,0x10000, 0x5fa2106c )
	ROM_LOAD( "epr11955.b06",0x50000,0x10000, 0x86a0c368 )
	ROM_LOAD( "epr11956.b07",0x60000,0x10000, 0xaff5c2fa )
	ROM_LOAD( "epr11957.b08",0x70000,0x10000, 0x218f835b )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr11945.a07",0x00000,0x8000, 0xc2a83012 )
	ROM_LOAD( "epr11140.a08",0x10000,0x8000, 0xb297371b )
	ROM_LOAD( "epr11141.a09",0x18000,0x8000, 0x19756aa6 )
	ROM_LOAD( "epr11142.a10",0x20000,0x8000, 0x25d26c66 )
	ROM_LOAD( "epr11143.a11",0x28000,0x8000, 0x848b7b77 )

ROM_END



/*****************************************************************************/
// Super League

ROM_START( suprleag )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "epr11131.a02",0x00000,0x10000, 0x9b78c2cc )
	ROM_LOAD_ODD ( "epr11130.a01",0x00000,0x10000, 0xe2451676 )
	ROM_LOAD_EVEN( "epr11133.a04",0x20000,0x10000, 0xeed72f37 )
	ROM_LOAD_ODD ( "epr11132.a03",0x20000,0x10000, 0xff199325 )
	ROM_LOAD_EVEN( "epr11135.a06",0x40000,0x10000, 0x3735e0e1 )
	ROM_LOAD_ODD ( "epr11134.a05",0x40000,0x10000, 0xccd857f5 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "epr11136.b09",0x00000,0x10000, 0xc3860ce4 )
	ROM_LOAD( "epr11137.b10",0x10000,0x10000, 0x92d96187 )
	ROM_LOAD( "epr11138.b11",0x20000,0x10000, 0xc01dc773 )

	ROM_REGION( 0x80000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "epr11144.b01",0x00000,0x10000, 0xb31de51c )
	ROM_LOAD( "epr11145.b02",0x10000,0x10000, 0x4223d2c3 )
	ROM_LOAD( "epr11146.b03",0x20000,0x10000, 0xbf0359b6 )
	ROM_LOAD( "epr11147.b04",0x30000,0x10000, 0x3e592772 )
	ROM_LOAD( "epr11148.b05",0x40000,0x10000, 0x126e1309 )
	ROM_LOAD( "epr11149.b06",0x50000,0x10000, 0x694d3765 )
	ROM_LOAD( "epr11150.b07",0x60000,0x10000, 0x9fc0aded )
	ROM_LOAD( "epr11151.b08",0x70000,0x10000, 0x9de95169 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "epr11139.a07",0x00000,0x08000, 0x9cbd99da )
	ROM_LOAD( "epr11140.a08",0x10000,0x08000, 0xb297371b )
	ROM_LOAD( "epr11141.a09",0x18000,0x08000, 0x19756aa6 )
	ROM_LOAD( "epr11142.a10",0x20000,0x08000, 0x25d26c66 )
	ROM_LOAD( "epr11143.a11",0x28000,0x08000, 0x848b7b77 )

ROM_END

/*****************************************************************************/
// Action Fighter

ROM_START( afighter )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
// cpu 317-0018
	ROM_LOAD_EVEN( "10348",0x00000,0x08000, 0xe51e3012 )
	ROM_LOAD_ODD ( "10349",0x00000,0x08000, 0x4b434c37 )
	ROM_LOAD_EVEN( "10350",0x20000,0x08000, 0xf2cd6b3f )
	ROM_LOAD_ODD ( "10351",0x20000,0x08000, 0xede21d8d )
	ROM_LOAD_EVEN( "10352",0x40000,0x08000, 0xf8abb143 )
	ROM_LOAD_ODD ( "10353",0x40000,0x08000, 0x5a757dc9 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "10281",0x00000,0x10000, 0x30e92cda )
	ROM_LOAD( "10282",0x10000,0x10000, 0xb67b8910 )
	ROM_LOAD( "10283",0x20000,0x10000, 0xe7dbfd2d )

	ROM_REGION( 0x40000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "10285",0x00000,0x08000, 0x98aa3d04 )
	ROM_LOAD( "10286",0x08000,0x08000, 0x8da050cf )
	ROM_LOAD( "10287",0x10000,0x08000, 0x7989b74a )
	ROM_LOAD( "10288",0x18000,0x08000, 0xd3ce551a )
	ROM_LOAD( "10289",0x20000,0x08000, 0xc59d1b98 )
	ROM_LOAD( "10290",0x28000,0x08000, 0x39354223 )
	ROM_LOAD( "10291",0x30000,0x08000, 0x6e4b245c )
	ROM_LOAD( "10292",0x38000,0x08000, 0xcef289a3 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "10284",0x00000,0x8000, 0x8ff09116 )

ROM_END

/*****************************************************************************/
// Ryukyu

ROM_START( ryukyu )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
// cpu 317-5023
	ROM_LOAD_EVEN( "13347",0x00000,0x10000, 0x398031fa )
	ROM_LOAD_ODD ( "13348",0x00000,0x10000, 0x5f0e0c86 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "13351",0x00000,0x20000, 0xa68a4e6d )
	ROM_LOAD( "13352",0x20000,0x20000, 0x5e5531e4 )
	ROM_LOAD( "13353",0x40000,0x20000, 0x6d23dfd8 )

	ROM_REGION( 0x40000*2, REGION_GFX2 ) /* sprites */
	ROM_LOAD( "13354",0x00000,0x20000, 0xf07aad99 )
	ROM_LOAD( "13355",0x20000,0x20000, 0x67890019 )
	ROM_LOAD( "13356",0x30000,0x20000, 0x5498290b )
	ROM_LOAD( "13357",0x40000,0x20000, 0xf9e7cf03 )

	ROM_REGION( 0x30000, REGION_CPU2 ) /* sound CPU */
	ROM_LOAD( "13349",0x00000,0x08000, 0xb83183f8 )
	ROM_LOAD( "13350",0x10000,0x20000, 0x3c59a658 )

ROM_END



/***************************************************************************/

GAMEX(1986, alexkidd, 0,        alexkidd, alexkidd, alexkidd, ROT0,         "Sega",    "Alex Kidd (set 1)", GAME_NOT_WORKING)
GAME( 1986, alexkida, alexkidd, alexkidd, alexkidd, alexkidd, ROT0,         "Sega",    "Alex Kidd (set 2)")
GAME( 1987, aliensyn, 0,        aliensyn, aliensyn, aliensyn, ROT0,         "Sega",    "Alien Syndrome (set 1)")
GAMEX(1987, aliensya, aliensyn, aliensyn, aliensyn, aliensyn, ROT0,         "Sega",    "Alien Syndrome (set 2)", GAME_NOT_WORKING)
GAMEX(1987, aliensyj, aliensyn, aliensyn, aliensyn, aliensyn, ROT0,         "Sega",    "Alien Syndrome (Japan)", GAME_NOT_WORKING)
GAMEX(1987, aliensyb, aliensyn, aliensyn, aliensyn, aliensyn, ROT0,         "Sega",    "Alien Syndrome (set 3)", GAME_NOT_WORKING)
GAME( 1988, altbeast, 0,        altbeast, altbeast, altbeast, ROT0,         "Sega",    "Altered Beast (Version 1)")
GAMEX(1988, jyuohki,  altbeast, altbeast, altbeast, altbeast, ROT0,         "Sega",    "Jyuohki (Japan)",           GAME_NOT_WORKING)
GAMEX(1988, altbeas2, altbeast, altbeas2, altbeast, altbeast, ROT0,         "Sega",    "Altered Beast (Version 2)", GAME_NO_SOUND)
GAMEX(1990, astorm,   0,        astorm,   astorm,   astorm,   ROT0_16BIT,   "Sega",    "Alien Storm", GAME_NOT_WORKING)
GAMEX(1990, astorm2p, astorm,   astorm,   astorm,   astorm,   ROT0_16BIT,   "Sega",    "Alien Storm (2 Player)", GAME_NOT_WORKING)
GAME( 1990, astormbl, astorm,   astorm,   astorm,   astorm,   ROT0_16BIT,   "bootleg", "Alien Storm (bootleg)")
GAMEX(1990, atomicp,  0,        atomicp,  atomicp,  atomicp,  ROT0,         "Philko",  "Atomic Point", GAME_NO_SOUND)
GAME( 1990, aurail,   0,        aurail,   aurail,   aurail,   ROT0,         "Sega / Westone", "Aurail (set 1)")
GAME( 1990, auraila,  aurail,   aurail,   aurail,   auraila,  ROT0,         "Sega / Westone", "Aurail (set 2)")
GAME( 1989, bayroute, 0,        bayroute, bayroute, bayroute, ROT0,         "Sunsoft / Sega", "Bay Route (set 1)")
GAMEX(1989, bayrouta, bayroute, bayroute, bayroute, bayrouta, ROT0,         "Sunsoft / Sega", "Bay Route (set 2)", GAME_NOT_WORKING)
GAMEX(1989, bayrtbl1, bayroute, bayroute, bayroute, bayrtbl1, ROT0,         "bootleg", "Bay Route (bootleg set 1)", GAME_NOT_WORKING)
GAMEX(1989, bayrtbl2, bayroute, bayroute, bayroute, bayrtbl1, ROT0,         "bootleg", "Bay Route (bootleg set 2)", GAME_NOT_WORKING)
GAME( 1986, bodyslam, 0,        bodyslam, bodyslam, bodyslam, ROT0,         "Sega",    "Body Slam")
GAME( 1986, dumpmtmt, bodyslam, bodyslam, bodyslam, bodyslam, ROT0,         "Sega",    "Dump Matsumoto (Japan)")
GAME( 1989, dduxbl,   0,        dduxbl,   dduxbl,   dduxbl,   ROT0,         "bootleg", "Dynamite Dux (bootleg)")
GAMEX(1989, eswat,    0,        eswat,    eswat,    eswat,    ROT0,         "Sega",    "E-Swat", GAME_NOT_WORKING)
GAME( 1989, eswatbl,  eswat,    eswat,    eswat,    eswat,    ROT0,         "bootleg", "E-Swat (bootleg)")
GAME( 1986, fantzone, 0,        fantzone, fantzone, fantzone, ROT0,         "Sega",    "Fantasy Zone (Japan New Ver.)")
GAME( 1986, fantzono, fantzone, fantzono, fantzone, fantzone, ROT0,         "Sega",    "Fantasy Zone (Old Ver.)")
GAMEX(1989, fpoint,   0,        fpoint,   fpoint,   fpoint,   ROT0,         "Sega",    "Flash Point", GAME_NOT_WORKING)
GAME( 1989, fpointbl, fpoint,   fpoint,   fpoint,   fpointbl, ROT0,         "bootleg", "Flash Point (bootleg)")
GAME( 1989, goldnaxe, 0,        goldnaxe, goldnaxe, goldnaxe, ROT0,         "Sega",    "Golden Axe (Version 1)")
GAMEX(1989, goldnaxj, goldnaxe, goldnaxe, goldnaxe, goldnaxe, ROT0,         "Sega",    "Golden Axe (Version 1, Japan)", GAME_NOT_WORKING)
GAMEX(1989, goldnabl, goldnaxe, goldnaxe, goldnaxe, goldnabl, ROT0,         "bootleg", "Golden Axe (bootleg)", GAME_NOT_WORKING)
GAME( 1989, goldnaxa, goldnaxe, goldnaxa, goldnaxe, goldnaxe, ROT0,         "Sega",    "Golden Axe (Version 2)")
GAMEX(1989, goldnaxb, goldnaxe, goldnaxa, goldnaxe, goldnaxe, ROT0,         "Sega",    "Golden Axe (Version 2 317-0110)", GAME_NOT_WORKING)
GAMEX(1989, goldnaxc, goldnaxe, goldnaxa, goldnaxe, goldnaxe, ROT0,         "Sega",    "Golden Axe (Version 2 317-0122)", GAME_NOT_WORKING)
GAME( 1987, hwchamp,  0,        hwchamp,  hwchamp,  hwchamp,  ROT0,         "Sega",    "Heavyweight Champ")
GAME( 1985, mjleague, 0,        mjleague, mjleague, mjleague, ROT270,       "Sega",    "Major League")
GAMEX(1990, moonwalk, 0,        moonwalk, moonwalk, moonwalk, ROT0,         "Sega",    "Moon Walker (Set 1)", GAME_NOT_WORKING)
GAMEX(1990, moonwlka, moonwalk, moonwalk, moonwalk, moonwalk, ROT0,         "Sega",    "Moon Walker (Set 2)", GAME_NOT_WORKING)
GAME( 1990, moonwlkb, moonwalk, moonwalk, moonwalk, moonwalk, ROT0,         "bootleg", "Moon Walker (bootleg)")
GAMEX(????, passsht,  0,        passsht,  passsht,  passsht,  ROT270,       "Sega",    "Passing Shot (2 Players)", GAME_NOT_WORKING)
GAME( ????, passshtb, passsht,  passsht,  passsht,  passsht,  ROT270,       "bootleg", "Passing Shot (2 Players) (bootleg)")
GAMEX(????, passht4b, passsht,  passht4b, passht4b, passht4b, ROT270,       "bootleg", "Passing Shot (4 Players) (bootleg)", GAME_NO_SOUND)
GAME( 1986, quartet,  0,        quartet,  quartet,  quartet,  ROT0,         "Sega",    "Quartet")
GAME( 1986, quartetj, quartet,  quartet,  quartet,  quartet,  ROT0,         "Sega",    "Quartet (Japan)")
GAME( 1986, quartet2, quartet,  quartet2, quartet2, quartet2, ROT0,         "Sega",    "Quartet II")
GAME( 1991, riotcity, 0,        riotcity, riotcity, riotcity, ROT0,         "Sega / Westone", "Riot City")
GAME( 1987, sdi,      0,        sdi,      sdi,      sdi,      ROT0,         "Sega",    "SDI - Strategic Defense Initiative")
GAMEX(1987, sdioj,    sdi,      sdi,      sdi,      sdi,      ROT0,         "Sega",    "SDI - Strategic Defense Initiative (Japan)", GAME_NOT_WORKING)
GAME( 1989, shdancer, 0,        shdancer, shdancer, shdancer, ROT0,         "Sega",    "Shadow Dancer (US)")
GAMEX(1989, shdancbl, shdancer, shdancbl, shdancer, shdancbl, ROT0,         "bootleg", "Shadow Dancer (bootleg)", GAME_NOT_WORKING)
GAME( 1989, shdancrj, shdancer, shdancrj, shdancer, shdancrj, ROT0,         "Sega",    "Shadow Dancer (Japan)")
GAME( 1987, shinobi,  0,        shinobi,  shinobi,  shinobi,  ROT0,         "Sega",    "Shinobi (set 1)")
GAMEX(1987, shinobib, shinobi,  shinobi,  shinobi,  shinobi,  ROT0,         "Sega",    "Shinobi (set 3)", GAME_NOT_WORKING)
GAMEX(1987, shinobia, shinobi,  shinobl,  shinobi,  shinobi,  ROT0,         "Sega",    "Shinobi (set 2)", GAME_NOT_WORKING)
GAME( 1987, shinobl,  shinobi,  shinobl,  shinobi,  shinobi,  ROT0,         "bootleg", "Shinobi (bootleg)")
GAMEX(1988, tetris,   0,        tetris,   tetris,   tetris,   ROT0,         "Sega",    "Tetris (Sega Set 1)", GAME_NOT_WORKING)
GAME( 1988, tetrisbl, tetris,   tetris,   tetris,   tetrisbl, ROT0,         "bootleg", "Tetris (Sega bootleg)")
GAMEX(1988, tetrisa,  tetris,   tetris,   tetris,   tetrisbl, ROT0,         "Sega",    "Tetris (Sega Set 2)", GAME_NOT_WORKING)
GAME( 1987, timscanr, 0,        timscanr, timscanr, timscanr, ROT270,       "Sega",    "Time Scanner")
GAME (1994, toryumon, 0,        toryumon, toryumon, toryumon, ROT0,         "Sega",    "Toryumon")
GAMEX(1989, tturf,    0,        tturf,    tturf,    tturf,    ROT0_16BIT,   "Sega / Sunsoft", "Tough Turf (Japan)", GAME_NO_SOUND)
GAMEX(1989, tturfu,   tturf,    tturfu,   tturf,    tturf,    ROT0_16BIT,   "Sega / Sunsoft", "Tough Turf (US)", GAME_NO_SOUND)
GAMEX(1989, tturfbl,  tturf,    tturfbl,  tturf,    tturfbl,  ROT0_16BIT,   "bootleg", "Tough Turf (bootleg)", GAME_IMPERFECT_SOUND)
GAME( 1988, wb3,      0,        wb3,      wb3,      wb3,      ROT0,         "Sega / Westone", "Wonder Boy III - Monster Lair (set 1)")
GAMEX(1988, wb3a,     wb3,      wb3,      wb3,      wb3,      ROT0,         "Sega / Westone", "Wonder Boy III - Monster Lair (set 2)", GAME_NOT_WORKING)
GAME( 1988, wb3bl,    wb3,      wb3bl,    wb3,      wb3bl,    ROT0,         "bootleg", "Wonder Boy III - Monster Lair (bootleg)")
GAME( 1989, wrestwar, 0,        wrestwar, wrestwar, wrestwar, ROT270_16BIT, "Sega",    "Wrestle War")

GAME( 1985, hangon,   0,        hangon,   hangon,   hangon,   ROT0,         "Sega",    "Hang-On")
GAME( 1985, sharrier, 0,        sharrier, sharrier, sharrier, ROT0_16BIT,   "Sega",    "Space Harrier")
GAMEX(1992, shangon,  0,        shangon,  shangon,  shangon,  ROT0,         "Sega",    "Super Hang-On", GAME_NOT_WORKING)
GAME( 1992, shangonb, shangon,  shangon,  shangon,  shangonb, ROT0,         "bootleg", "Super Hang-On (bootleg)")
GAME( 1986, outrun,   0,        outrun,   outrun,   outrun,   ROT0,         "Sega",    "Out Run (set 1)")
GAME( 1986, outruna,  outrun,   outruna,  outrun,   outrun,   ROT0,         "Sega",    "Out Run (set 2)")
GAME( 1986, outrunb,  outrun,   outruna,  outrun,   outrunb,  ROT0,         "Sega",    "Out Run (set 3)")
GAMEX(1985, enduror,  0,        enduror,  enduror,  enduror,  ROT0,         "Sega",    "Enduro Racer", GAME_NOT_WORKING)
GAME( 1985, endurobl, enduror,  enduror,  enduror,  endurobl, ROT0,         "bootleg", "Enduro Racer (bootleg set 1)")
GAME( 1985, endurob2, enduror,  endurob2, enduror,  endurob2, ROT0,         "bootleg", "Enduro Racer (bootleg set 2)")



GAMEX(????, aceattac, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Ace Attacker", GAME_NOT_WORKING)
GAMEX(????, aburner,  0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "After Burner (Japan)", GAME_NOT_WORKING)
GAMEX(????, aburner2, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "After Burner II", GAME_NOT_WORKING)
GAMEX(????, bloxeed,  0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Bloxeed", GAME_NOT_WORKING)
GAMEX(????, cltchitr, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Clutch Hitter", GAME_NOT_WORKING)
GAMEX(????, cotton,   0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Cotton (Japan)", GAME_NOT_WORKING)
GAMEX(????, cottona,  cotton,   s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Cotton", GAME_NOT_WORKING)
GAMEX(????, ddcrew,   0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "DD Crew", GAME_NOT_WORKING)
GAMEX(????, dunkshot, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Dunk Shot", GAME_NOT_WORKING)
GAMEX(????, lghost,   0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Laser Ghost", GAME_NOT_WORKING)
GAMEX(????, loffire,  0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Line of Fire", GAME_NOT_WORKING)
GAMEX(????, mvp,      0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "MVP", GAME_NOT_WORKING)
GAMEX(????, thndrbld, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Thunder Blade", GAME_NOT_WORKING)
GAMEX(????, thndrbdj, thndrbld, s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Thunder Blade (Japan)", GAME_NOT_WORKING)
GAMEX(????, toutrun,  0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Turbo Outrun (set 1)", GAME_NOT_WORKING)
GAMEX(????, toutruna, toutrun,  s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Turbo Outrun (set 2)", GAME_NOT_WORKING)
GAMEX(????, exctleag, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Excite League", GAME_NOT_WORKING)
GAMEX(????, suprleag, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Super League", GAME_NOT_WORKING)
GAMEX(????, afighter, 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Action Fighter", GAME_NOT_WORKING)
GAMEX(????, ryukyu  , 0,        s16dummy, s16dummy, s16dummy, ROT0,         "Sega", "Ryukyu", GAME_NOT_WORKING)


