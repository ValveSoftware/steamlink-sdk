#include "../machine/polepos.cpp"
#include "../vidhrdw/polepos.cpp"
#include "../sndhrdw/polepos.cpp"

/***************************************************************************
  Pole Position memory map (preliminary)

driver by Ernesto Corvi, Juergen Buchmueller, Alex Pasadyn, Aaron Giles


Z80
----------------------------------------
0000-2fff (R) ROM
3000-37ff (R/W) Battery Backup RAM
4000-43ff (R/W) Motion Object memory
	(4380-43ff Vertical and Horizontal position)
4400-47ff (R/W) Motion Object memory
	(4780-47ff Vertical and Horizontal position)
4800-4bff (R/W) Road Memory
	(4800-49ff Character)
	(4b80-4bff Horizontal Scroll)
4c00-57ff (R/W) Alphanumeric Memory
	(4c00-4fff) Alphanumeric
	(5000-53ff) View character
8000-83ff (R/W) Sound Memory
	(83c0-83ff Sound)
9000-90ff (R/W) 4 bit CPU data
9100-9100 (R/W) 4 bit CPU controller
a000-a000 (R/W) Input/Output
		  on WRITE: IRQ Enable ( 1 = enable, 0 = disable )
		  on READ: bit0 = Not Used, bit1 = 128V, bit2 = Power-Line Sense, bit3 = ADC End Flag
a001-a001 (W) 4 bit CPU Enable
a002-a002 (W) Sound enable
a003-a003 (W) ADC Input Select
a004-a004 (W) CPU 1 Enable
a005-a005 (W) CPU 2 Enable
a006-a006 (W) Start Switch
a007-a007 (W) Color Enable
a100-a100 (W) Watchdog reset
a200-a200 (W) Car Sound ( Lower Nibble )
a300-a300 (W) Car Sound ( Upper Nibble )

Z8002 #1 & #2 (they share the ram)
----------------------------------------
0000-3fff ROM
6000-6003 NMI-Enable
	(6000-6001 CPU1 NMI enable)
	(6002-6003 CPU2 NMI enable)
8000-8fff Motion Object Memory
	(8700-87ff Horizontal and Vertical position)
	(8f00-8fff Character, Color, Vertical size, Horizontal size)
9000-97ff Road Memory
	(9000-93ff Character)
	(9700-97ff Horizontal scroll)
9800-9fff Alphanumeric Memory (video RAM #1)
a000-afff View character memory (I think it refers to 'View' as the background)
c000-c000 View horizontal position
c100-c100 Road vertical position

NOTES:
- Pole Position II reports 'Manual Start' on the Test Mode. This is ok,
because they had to accomodate the hardware from Pole Position I to allow
track selection.

***************************************************************************/

#include "driver.h"
#include "cpu/z8000/z8000.h"
#include "vidhrdw/generic.h"


/* from machine */
void polepos_init_machine(void);
WRITE_HANDLER( polepos_z80_irq_enable_w );
WRITE_HANDLER( polepos_z8002_nvi_enable_w );
int polepos_z8002_1_interrupt(void);
int polepos_z8002_2_interrupt(void);
WRITE_HANDLER( polepos_z8002_enable_w );
WRITE_HANDLER( polepos_adc_select_w );
READ_HANDLER( polepos_adc_r );
READ_HANDLER( polepos_io_r );
WRITE_HANDLER( polepos_mcu_enable_w );
READ_HANDLER( polepos_mcu_control_r );
WRITE_HANDLER( polepos_mcu_control_w );
READ_HANDLER( polepos_mcu_data_r );
WRITE_HANDLER( polepos_mcu_data_w );
WRITE_HANDLER( polepos_start_w );
READ_HANDLER( polepos2_ic25_r );

/* from sndhrdw */
int polepos_sh_start(const struct MachineSound *msound);
void polepos_sh_stop(void);
void polepos_sh_update(void);
WRITE_HANDLER( polepos_engine_sound_lsb_w );
WRITE_HANDLER( polepos_engine_sound_msb_w );

/* from vidhrdw */
extern UINT8 *polepos_view_memory;
extern UINT8 *polepos_road_memory;
extern UINT8 *polepos_alpha_memory;
extern UINT8 *polepos_sprite_memory;

int polepos_vh_start(void);
void polepos_vh_stop(void);
void polepos_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void polepos_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( polepos_view_w );
WRITE_HANDLER( polepos_road_w );
WRITE_HANDLER( polepos_alpha_w );
WRITE_HANDLER( polepos_sprite_w );
WRITE_HANDLER( polepos_z80_view_w );
WRITE_HANDLER( polepos_z80_road_w );
WRITE_HANDLER( polepos_z80_alpha_w );
WRITE_HANDLER( polepos_z80_sprite_w );

READ_HANDLER( polepos_view_r );
READ_HANDLER( polepos_road_r );
READ_HANDLER( polepos_alpha_r );
READ_HANDLER( polepos_sprite_r );
READ_HANDLER( polepos_z80_view_r );
READ_HANDLER( polepos_z80_road_r );
READ_HANDLER( polepos_z80_alpha_r );
READ_HANDLER( polepos_z80_sprite_r );

WRITE_HANDLER( polepos_view_hscroll_w );
WRITE_HANDLER( polepos_road_vscroll_w );



static unsigned char *nvram;
static size_t nvram_size;

static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0xff,nvram_size);
	}
}


/*********************************************************************
 * CPU memory structures
 *********************************************************************/

static struct MemoryReadAddress z80_readmem[] =
{
	{ 0x0000, 0x2fff, MRA_ROM },				/* ROM */
	{ 0x3000, 0x37ff, MRA_RAM },				/* Battery Backup */
	{ 0x4000, 0x47ff, polepos_z80_sprite_r },	/* Motion Object */
	{ 0x4800, 0x4bff, polepos_z80_road_r },		/* Road Memory */
	{ 0x4c00, 0x4fff, polepos_z80_alpha_r },	/* Alphanumeric (char ram) */
	{ 0x5000, 0x57ff, polepos_z80_view_r }, 	/* Background Memory */
	{ 0x8000, 0x83ff, MRA_RAM }, 				/* Sound Memory */
	{ 0x9000, 0x90ff, polepos_mcu_data_r },		/* 4 bit CPU data */
	{ 0x9100, 0x9100, polepos_mcu_control_r },	/* 4 bit CPU control */
	{ 0xa000, 0xa000, polepos_io_r },			/* IO */
	{ -1 }					 					/* end of table */
};

static struct MemoryWriteAddress z80_writemem[] =
{
	{ 0x0000, 0x2fff, MWA_ROM }, 						/* ROM */
	{ 0x3000, 0x37ff, MWA_RAM, &nvram, &nvram_size },	/* Battery Backup */
	{ 0x4000, 0x47ff, polepos_z80_sprite_w },			/* Motion Object */
	{ 0x4800, 0x4bff, polepos_z80_road_w }, 			/* Road Memory */
	{ 0x4c00, 0x4fff, polepos_z80_alpha_w }, 			/* Alphanumeric (char ram) */
	{ 0x5000, 0x57ff, polepos_z80_view_w }, 			/* Background Memory */
	{ 0x8000, 0x83bf, MWA_RAM }, 						/* Sound Memory */
	{ 0x83c0, 0x83ff, polepos_sound_w, &polepos_soundregs },/* Sound data */
	{ 0x9000, 0x90ff, polepos_mcu_data_w },				/* 4 bit CPU data */
	{ 0x9100, 0x9100, polepos_mcu_control_w }, 			/* 4 bit CPU control */
	{ 0xa000, 0xa000, polepos_z80_irq_enable_w },		/* NMI enable */
	{ 0xa001, 0xa001, polepos_mcu_enable_w },			/* 4 bit CPU enable */
	{ 0xa002, 0xa002, MWA_NOP }, 						/* Sound Enable */
	{ 0xa003, 0xa003, polepos_adc_select_w },			/* ADC Input select */
	{ 0xa004, 0xa005, polepos_z8002_enable_w },			/* CPU 1/2 enable */
	{ 0xa006, 0xa006, polepos_start_w },				/* Start Switch */
	{ 0xa007, 0xa007, MWA_NOP }, 						/* Color Enable */
	{ 0xa100, 0xa100, watchdog_reset_w }, 				/* Watchdog */
	{ 0xa200, 0xa200, polepos_engine_sound_lsb_w },		/* Car Sound ( Lower Nibble ) */
	{ 0xa300, 0xa300, polepos_engine_sound_msb_w }, 	/* Car Sound ( Upper Nibble ) */
	{ -1 }					 							/* end of table */
};

static struct IOReadPort z80_readport[] =
{
	{ 0x00, 0x00, polepos_adc_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort z80_writeport[] =
{
	{ 0x00, 0x00, IOWP_NOP }, /* ??? */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress z8002_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },			/* ROM */
	{ 0x8000, 0x8fff, polepos_sprite_r },	/* Motion Object */
	{ 0x9000, 0x97ff, polepos_road_r },		/* Road Memory */
	{ 0x9800, 0x9fff, polepos_alpha_r }, 	/* Alphanumeric (char ram) */
	{ 0xa000, 0xafff, polepos_view_r },		/* Background memory */
	{ -1 }						 			/* end of table */
};

static struct MemoryWriteAddress z8002_writemem[] =
{
	{ 0x6000, 0x6003, polepos_z8002_nvi_enable_w },					/* NVI enable */
	{ 0x0000, 0x7fff, MWA_ROM },									/* ROM */
	{ 0x8000, 0x8fff, polepos_sprite_w, &polepos_sprite_memory },	/* Motion Object */
	{ 0x9000, 0x97ff, polepos_road_w, &polepos_road_memory },		/* Road Memory */
	{ 0x9800, 0x9fff, polepos_alpha_w, &polepos_alpha_memory },	  	/* Alphanumeric (char ram) */
	{ 0xa000, 0xafff, polepos_view_w, &polepos_view_memory },		/* Background memory */
	{ 0xc000, 0xc001, polepos_view_hscroll_w },						/* Background horz scroll position */
	{ 0xc100, 0xc101, polepos_road_vscroll_w },						/* Road vertical position */
	{ -1 }													  		/* end of table */
};


/*********************************************************************
 * Input port definitions
 *********************************************************************/

INPUT_PORTS_START( polepos )
	PORT_START  /* IN0 - Mostly Fake - Handled by the MCU */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_TOGGLE, "Gear Change", KEYCODE_SPACE, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x08, 0x08, "Display Shift" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, "Nr. of Laps" )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x01, "4" )
	PORT_DIPNAME( 0x06, 0x06, "Game Time" )
	PORT_DIPSETTING(	0x00, "90 secs." )
	PORT_DIPSETTING(	0x04, "100 secs." )
	PORT_DIPSETTING(	0x02, "110 secs." )
	PORT_DIPSETTING(	0x06, "120 secs." )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( Free_Play ) )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ))
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Speed Unit" )
	PORT_DIPSETTING(	0x02, "MPH" )
	PORT_DIPSETTING(	0x00, "KPH" )
	PORT_DIPNAME( 0x1c, 0x08, "Extended Rank" )
	PORT_DIPSETTING(	0x00, "A" )
	PORT_DIPSETTING(	0x10, "B" )
	PORT_DIPSETTING(	0x08, "C" )
	PORT_DIPSETTING(	0x18, "D" )
	PORT_DIPSETTING(	0x04, "E" )
	PORT_DIPSETTING(	0x14, "F" )
	PORT_DIPSETTING(	0x0c, "G" )
	PORT_DIPSETTING(	0x1c, "H" )
	PORT_DIPNAME( 0xe0, 0x40, "Practice Rank" )
	PORT_DIPSETTING(	0x00, "A" )
	PORT_DIPSETTING(	0x80, "B" )
	PORT_DIPSETTING(	0x40, "C" )
	PORT_DIPSETTING(	0xc0, "D" )
	PORT_DIPSETTING(	0x20, "E" )
	PORT_DIPSETTING(	0xa0, "F" )
	PORT_DIPSETTING(	0x60, "G" )
	PORT_DIPSETTING(	0xe0, "H" )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 100, 50, 0, 0xff, KEYCODE_LALT, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, KEYCODE_LCONTROL, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x80, IPT_DIAL, 60, 1, 0x00, 0xff )
INPUT_PORTS_END


INPUT_PORTS_START( polepos2 )
	PORT_START  /* IN0 - Mostly Fake - Handled by the MCU */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_TOGGLE, "Gear Change", KEYCODE_SPACE, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x08, 0x08, "Display Shift" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED ) /* TEST button */
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )	/* docs say "freeze", but it doesn't seem to work */
	PORT_DIPSETTING(	0x00, DEF_STR( Off ))
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ))
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Speed Unit" )
	PORT_DIPSETTING(	0x04, "MPH" )
	PORT_DIPSETTING(	0x00, "KPH" )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( Free_Play ) )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Speed" )
    PORT_DIPSETTING(    0x00, "Average" )
    PORT_DIPSETTING(    0x01, "High" )
	PORT_DIPNAME( 0x06, 0x00, "Goal" )
	PORT_DIPSETTING(	0x04, "3" )
	PORT_DIPSETTING(	0x00, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_DIPSETTING(	0x06, "6" )
	PORT_DIPNAME( 0x18, 0x08, "Extended Rank" )
	PORT_DIPSETTING(	0x10, "A" )
	PORT_DIPSETTING(	0x00, "B" )
	PORT_DIPSETTING(	0x08, "C" )
	PORT_DIPSETTING(	0x18, "D" )
	PORT_DIPNAME( 0x60, 0x20, "Practice Rank" )
	PORT_DIPSETTING(	0x40, "A" )
	PORT_DIPSETTING(	0x00, "B" )
	PORT_DIPSETTING(	0x20, "C" )
	PORT_DIPSETTING(	0x60, "D" )
	PORT_DIPNAME( 0x80, 0x80, "Game Time" )
	PORT_DIPSETTING(	0x00, "90 secs." )
	PORT_DIPSETTING(	0x80, "120 secs." )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 100, 50, 0, 0xff, KEYCODE_LALT, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, KEYCODE_LCONTROL, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x80, IPT_DIAL, 60, 1, 0x00, 0xff )
INPUT_PORTS_END



/*********************************************************************
 * Graphics layouts
 *********************************************************************/

static struct GfxLayout charlayout_2bpp =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	  /* 2 bits per pixel */
	{ 0, 4 }, /* the two bitplanes are packed */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8*2	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout bigspritelayout =
{
	32, 32, /* 32*32 sprites */
	128, 	/* 128 sprites */
	4,		/* 4 bits per pixel */
	{ 0, 4, 0x8000*8+0, 0x8000*8+4 }, /* each two of the bitplanes are packed */
	{  0,  1,  2,  3,  8,  9, 10, 11,
	  16, 17, 18, 19, 24, 25, 26, 27,
	  32, 33, 34, 35, 40, 41, 42, 43,
	  48, 49, 50, 51, 56, 57, 58, 59},
	{  0*64,  1*64,  2*64,  3*64,  4*64,  5*64,  6*64,  7*64,
		8*64,  9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64,
	  16*64, 17*64, 18*64, 19*64, 20*64, 21*64, 22*64, 23*64,
	  24*64, 25*64, 26*64, 27*64, 28*64, 29*64, 30*64, 31*64 },
	32*32*2  /* each sprite takes 256 consecutive bytes */
};

static struct GfxLayout smallspritelayout =
{
	16,32,  /* 16*32 sprites (pixel doubled vertically) */
	128,	/* 128 sprites */
	4,		/* 4 bits per pixel */
	{ 0, 4, 0x2000*8+0, 0x2000*8+4 }, /* each two of the bitplanes are packed */
	{  0,  1,  2,  3,  8,  9, 10, 11,
	  16, 17, 18, 19, 24, 25, 26, 27 },
	{ 0*32,  0*32,  1*32,  1*32,  2*32,  2*32,  3*32,  3*32,
	  4*32,  4*32,  5*32,  5*32,  6*32,  6*32,  7*32,  7*32,
	  8*32,  8*32,  9*32,  9*32, 10*32, 10*32, 11*32, 11*32,
	 12*32, 12*32, 13*32, 13*32, 14*32, 14*32, 15*32, 15*32 },
	16*16*2  /* each sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_2bpp,	  0x0000, 128 },
	{ REGION_GFX2, 0, &charlayout_2bpp,	  0x0200, 128 },
	{ REGION_GFX3, 0, &smallspritelayout, 0x0400, 128 },
	{ REGION_GFX4, 0, &bigspritelayout,	  0x0400, 128 },
	{ -1 } /* end of array */
};


/*********************************************************************
 * Sound interfaces
 *********************************************************************/

static struct namco_interface namco_interface =
{
	3072000/64,		/* sample rate */
	6,				/* number of voices */
	95,				/* playback volume */
	REGION_SOUND1,	/* memory region */
	1				/* stereo */
};

static struct CustomSound_interface custom_interface =
{
	polepos_sh_start,
	polepos_sh_stop,
	polepos_sh_update
};

static const char *polepos_sample_names[] =
{
	"*polepos",
	"pp2_17.wav",
	"pp2_18.wav",
	0	/* end of array */
};

static struct Samplesinterface samples_interface =
{
	2,	/* 2 channels */
	40,	/* volume */
	polepos_sample_names
};



/*********************************************************************
 * Machine driver
 *********************************************************************/

static struct MachineDriver machine_driver_polepos =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz */
			z80_readmem,z80_writemem,z80_readport,z80_writeport,
			ignore_interrupt,1
		},
		{
			CPU_Z8000,
			3072000,	/* 3.072 Mhz */
			z8002_readmem,z8002_writemem,0,0,
			polepos_z8002_1_interrupt,1
		},
		{
			CPU_Z8000,
			3072000,	/* 3.072 Mhz */
			z8002_readmem,z8002_writemem,0,0,
			polepos_z8002_2_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	 /* frames per second, vblank duration */
	100,	/* some interleaving */
	polepos_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	128,0x1400,
	polepos_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	polepos_vh_start,
	polepos_vh_stop,
	polepos_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO, 0, 0, 0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		},
		{
			SOUND_CUSTOM,
			&custom_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	},

	nvram_handler
};


/*********************************************************************
 * ROM definitions
 *********************************************************************/

ROM_START( polepos )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "014-105.rom",	0x0000, 0x2000, 0xc918c043 )
	ROM_LOAD	 ( "014-116.rom",	0x2000, 0x1000, 0x7174bcb7 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "pp1_1b",		0x0000, 0x2000, 0x361c56dd )
	ROM_LOAD_EVEN( "pp1_2b",		0x0000, 0x2000, 0x582b530a )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "pp1_5b",		0x0000, 0x2000, 0x5cdf5294 )
	ROM_LOAD_EVEN( "pp1_6b",		0x0000, 0x2000, 0x81696272 )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "pp1_28",		0x0000, 0x1000, 0x5b277daf )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "pp1_29",		0x0000, 0x1000, 0x706e888a )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "pp1_25",		0x0000, 0x2000, 0xac8e28c1 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "pp1_26",		0x2000, 0x2000, 0x94443079 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "014-150.rom",	0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "pp1_19",		0x2000, 0x2000, 0x43ff83e1 )
	ROM_LOAD	 ( "pp1_21",		0x4000, 0x2000, 0x5f958eb4 )
	ROM_LOAD	 ( "014-151.rom",	0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "pp1_20",		0xa000, 0x2000, 0xec18075b )
	ROM_LOAD	 ( "pp1_22",		0xc000, 0x2000, 0x1d2f30b1 )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-137.bpr",	0x0000, 0x0100, 0xf07ff2ad )	/* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",	0x0100, 0x0100, 0xadbde7d7 )	/* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",	0x0200, 0x0100, 0xddac786a )	/* blue palette PROM */
	ROM_LOAD	 ( "014-140.bpr",	0x0300, 0x0100, 0x1e8d0491 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",	0x0400, 0x0100, 0x0e4fe8a0 )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",	0x0800, 0x0400, 0x7afc7cfc )	/* road color PROM */
	ROM_LOAD	 ( "pp1_6.bpr",		0x0c00, 0x0400, 0x2f1079ee )	/* sprite color PROM */
	ROM_LOAD	 ( "131.11n",		0x1000, 0x1000, 0x5921777f )	/* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",	0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "014-159.rom",	0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",	0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "pp1_11",		0x5000, 0x2000, 0x45b9bfeb )	/* voice PROM */
	ROM_LOAD	 ( "pp1_12",		0x7000, 0x2000, 0xa31b4be5 )	/* voice PROM */
	ROM_LOAD	 ( "pp1_13",		0x9000, 0x2000, 0xa4237466 )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( poleposa )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "014-105.rom",	0x0000, 0x2000, 0xc918c043 )
	ROM_LOAD	 ( "014-116.rom",	0x2000, 0x1000, 0x7174bcb7 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "014-101.rom",	0x0000, 0x2000, 0x8c2cf172 )
	ROM_LOAD_EVEN( "014-102.rom",	0x0000, 0x2000, 0x51018857 )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "014-203.rom",	0x0000, 0x2000, 0xeedea6e7 )
	ROM_LOAD_EVEN( "014-204.rom",	0x0000, 0x2000, 0xc52c98ed )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "014-132.rom",	0x0000, 0x1000, 0xa949aa85 )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD	 ( "014-133.rom",	0x0000, 0x1000, 0x3f0eb551 )	/* 2bpp view layer */

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "014-156.rom",	0x0000, 0x2000, 0xe7a09c93 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "014-157.rom",	0x2000, 0x2000, 0xdee7d687 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "014-150.rom",	0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "014-152.rom",	0x2000, 0x2000, 0xa7e3a1c6 )
	ROM_LOAD	 ( "014-154.rom",	0x4000, 0x2000, 0x8992d381 )
	ROM_LOAD	 ( "014-151.rom",	0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "014-153.rom",	0xa000, 0x2000, 0x6c5c6e68 )
	ROM_LOAD	 ( "014-155.rom",	0xc000, 0x2000, 0x111896ad )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-137.bpr",	0x0000, 0x0100, 0xf07ff2ad )	/* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",	0x0100, 0x0100, 0xadbde7d7 )	/* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",	0x0200, 0x0100, 0xddac786a )	/* blue palette PROM */
	ROM_LOAD	 ( "014-140.bpr",	0x0300, 0x0100, 0x1e8d0491 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",	0x0400, 0x0100, 0x0e4fe8a0 )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",	0x0800, 0x0400, 0x7afc7cfc )	/* road color PROM */
	ROM_LOAD	 ( "014-146.bpr",	0x0c00, 0x0400, 0xca4ba741 )	/* sprite color PROM */
	ROM_LOAD	 ( "014-231.rom",	0x1000, 0x1000, 0xa61bff15 )	/* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",	0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "014-159.rom",	0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",	0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",	0x5000, 0x2000, 0x5b4cf05e )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( polepos1 )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "014-105.rom",	0x0000, 0x2000, 0xc918c043 )
	ROM_LOAD	 ( "014-116.rom",	0x2000, 0x1000, 0x7174bcb7 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "014-101.rom",	0x0000, 0x2000, 0x8c2cf172 )
	ROM_LOAD_EVEN( "014-102.rom",	0x0000, 0x2000, 0x51018857 )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "103.3e",		0x0000, 0x2000, 0xaf4fc019 )
	ROM_LOAD_EVEN( "104.4e",		0x0000, 0x2000, 0xba0045f3 )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "014-132.rom",	0x0000, 0x1000, 0xa949aa85 )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "014-133.rom",	0x0000, 0x1000, 0x3f0eb551 )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "014-156.rom",	0x0000, 0x2000, 0xe7a09c93 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "014-157.rom",	0x2000, 0x2000, 0xdee7d687 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "014-150.rom",	0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "014-152.rom",	0x2000, 0x2000, 0xa7e3a1c6 )
	ROM_LOAD	 ( "014-154.rom",	0x4000, 0x2000, 0x8992d381 )
	ROM_LOAD	 ( "014-151.rom",	0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "014-153.rom",	0xa000, 0x2000, 0x6c5c6e68 )
	ROM_LOAD	 ( "014-155.rom",	0xc000, 0x2000, 0x111896ad )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-137.bpr",	0x0000, 0x0100, 0xf07ff2ad )	/* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",	0x0100, 0x0100, 0xadbde7d7 )	/* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",	0x0200, 0x0100, 0xddac786a )	/* blue palette PROM */
	ROM_LOAD	 ( "014-140.bpr",	0x0300, 0x0100, 0x1e8d0491 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",	0x0400, 0x0100, 0x0e4fe8a0 )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",	0x0800, 0x0400, 0x7afc7cfc )	/* road color PROM */
	ROM_LOAD	 ( "014-146.bpr",	0x0c00, 0x0400, 0xca4ba741 )	/* sprite color PROM */
	ROM_LOAD	 ( "131.11n",		0x1000, 0x1000, 0x5921777f )	/* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",	0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "014-159.rom",	0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",	0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",	0x5000, 0x2000, 0x5b4cf05e )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( topracer )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "tr9b.bin",		0x0000, 0x2000, 0x94436b70 )
	ROM_LOAD	 ( "014-116.rom",	0x2000, 0x1000, 0x7174bcb7 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "tr1b.bin",		0x0000, 0x2000, 0x127f0750 )
	ROM_LOAD_EVEN( "tr2b.bin",		0x0000, 0x2000, 0x6bd4ff6b )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "tr5b.bin",		0x0000, 0x2000, 0x4e5f7b9c )
	ROM_LOAD_EVEN( "tr6b.bin",		0x0000, 0x2000, 0x9d038ada )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "tr28.bin",		0x0000, 0x1000, 0xb8217c96 )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "tr29.bin",		0x0000, 0x1000, 0xc6e15c21 )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "trus25.bin",	0x0000, 0x2000, 0x9e1a9c3b )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "trus26.bin",	0x2000, 0x2000, 0x3b39a176 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "pp17.bin",		0x0000, 0x2000, 0x613ab0df )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "tr19.bin",		0x2000, 0x2000, 0xf8e7f551 )
	ROM_LOAD	 ( "tr21.bin",		0x4000, 0x2000, 0x17c798b0 )
	ROM_LOAD	 ( "pp18.bin",		0x8000, 0x2000, 0x5fd933e3 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "tr20.bin",		0xa000, 0x2000, 0x7053e219 )
	ROM_LOAD	 ( "tr22.bin",		0xc000, 0x2000, 0xf48917b2 )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-137.bpr",	0x0000, 0x0100, 0xf07ff2ad )	/* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",	0x0100, 0x0100, 0xadbde7d7 )	/* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",	0x0200, 0x0100, 0xddac786a )	/* blue palette PROM */
	ROM_LOAD	 ( "10p.bin",		0x0300, 0x0100, 0x5af3f710 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",	0x0400, 0x0100, 0x00000000 )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",	0x0800, 0x0400, 0x7afc7cfc )	/* road color PROM */
	ROM_LOAD	 ( "pp1_6.bpr",		0x0c00, 0x0400, 0x2f1079ee )	/* sprite color PROM */
	ROM_LOAD	 ( "014-231.rom",	0x1000, 0x1000, 0xa61bff15 )	/* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",	0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "014-159.rom",	0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",	0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",	0x5000, 0x2000, 0x5b4cf05e )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( polepos2 )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "pp4_9.6h",		0x0000, 0x2000, 0xbcf87004 )
	ROM_LOAD	 ( "183.7f",		0x2000, 0x1000, 0xa9d4c380 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "pp4_1.8m",		0x0000, 0x2000, 0x3f6ac294 )
	ROM_LOAD_EVEN( "pp4_2.8l",		0x0000, 0x2000, 0x51b9a669 )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "pp4_5.4m",		0x0000, 0x2000, 0xc3053cae )
	ROM_LOAD_EVEN( "pp4_6.4l",		0x0000, 0x2000, 0x38d04e0f )
	ROM_LOAD_ODD ( "pp4_7.3m",		0x4000, 0x1000, 0xad1c8994 )
	ROM_LOAD_EVEN( "pp4_8.3l",		0x4000, 0x1000, 0xef25a2ee )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "pp4_28.1f",		0x0000, 0x2000, 0x280dde7d )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",		0x0000, 0x2000, 0xec3ec6e6 )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "pp4_25.1n",		0x0000, 0x2000, 0xfd098e65 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "pp4_26.1m",		0x2000, 0x2000, 0x35ac62b3 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",		0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "pp1_19.4n",		0x2000, 0x2000, 0x43ff83e1 )
	ROM_LOAD	 ( "pp1_21.3n",		0x4000, 0x2000, 0x5f958eb4 )
	ROM_LOAD	 ( "pp4_23.2n",		0x6000, 0x2000, 0x9e056fcd )
	ROM_LOAD	 ( "120.12j",		0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "pp1_20.4m",		0xa000, 0x2000, 0xec18075b )
	ROM_LOAD	 ( "pp1_22.3m",		0xc000, 0x2000, 0x1d2f30b1 )
	ROM_LOAD	 ( "pp4_24.2m",		0xe000, 0x2000, 0x795268cf )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-186.bpr",	0x0000, 0x0100, 0x16d69c31 )	/* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",	0x0100, 0x0100, 0x07340311 )	/* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",	0x0200, 0x0100, 0x1efc84d7 )	/* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",	0x0300, 0x0100, 0x064d51a0 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",	0x0400, 0x0100, 0x7880c5af )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",	0x0800, 0x0400, 0x8b270902 )	/* road color PROM */
	ROM_LOAD	 ( "pp4-6.6m",		0x0c00, 0x0400, 0x647212b5 )	/* sprite color PROM */
	ROM_LOAD	 ( "131.11n",		0x1000, 0x1000, 0x5921777f )	/* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",		0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "128.2m",		0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",		0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "pp1_11.2e",		0x5000, 0x2000, 0x45b9bfeb )	/* voice PROM */
	ROM_LOAD	 ( "pp1_12.2f",		0x7000, 0x2000, 0xa31b4be5 )	/* voice PROM */
	ROM_LOAD	 ( "pp1_13.1e",		0x9000, 0x2000, 0xa4237466 )	/* voice PROM */
	ROM_LOAD	 ( "pp1_14.1f",		0xb000, 0x2000, 0x944580f9 )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( poleps2a )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "180.7h",		0x0000, 0x2000, 0xf85212c4 )
	ROM_LOAD	 ( "183.7f",		0x2000, 0x1000, 0xa9d4c380 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "176.3l",		0x0000, 0x2000, 0x8aeaec98 )
	ROM_LOAD_EVEN( "177.4l",		0x0000, 0x2000, 0x7051df35 )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "178.3e",		0x0000, 0x2000, 0xeac35cfa )
	ROM_LOAD_EVEN( "179.4e",		0x0000, 0x2000, 0x613e917d )
	ROM_LOAD_ODD ( "184.3d",		0x4000, 0x2000, 0xd893c4ed )
	ROM_LOAD_EVEN( "185.4d",		0x4000, 0x2000, 0x899de75e )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "172.7n",		0x0000, 0x2000, 0xfbe5e72f )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",		0x0000, 0x2000, 0xec3ec6e6 )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "170.13n",		0x0000, 0x2000, 0x455d79a0 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "171.12n",		0x2000, 0x2000, 0x78372b81 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",		0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "166.13k",		0x2000, 0x2000, 0x2b0517bd )
	ROM_LOAD	 ( "168.13l",		0x4000, 0x2000, 0x4d7916d9 )
	ROM_LOAD	 ( "175.13m",		0x6000, 0x2000, 0xbd6df480 )
	ROM_LOAD	 ( "120.12j",		0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "167.12k",		0xa000, 0x2000, 0x411e21b5 )
	ROM_LOAD	 ( "169.12l",		0xc000, 0x2000, 0x662ff24b )
	ROM_LOAD	 ( "174.12m",		0xe000, 0x2000, 0xf0c571dc )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-186.bpr",	0x0000, 0x0100, 0x16d69c31 )	/* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",	0x0100, 0x0100, 0x07340311 )	/* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",	0x0200, 0x0100, 0x1efc84d7 )	/* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",	0x0300, 0x0100, 0x064d51a0 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",	0x0400, 0x0100, 0x7880c5af )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",	0x0800, 0x0400, 0x8b270902 )	/* road color PROM */
	ROM_LOAD	 ( "014-192.bpr",	0x0c00, 0x0400, 0xcaddb0b0 )	/* sprite color PROM */
	ROM_LOAD	 ( "131.11n",		0x1000, 0x1000, 0x5921777f )	/* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",		0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "128.2m",		0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",		0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",	0x5000, 0x2000, 0x5b4cf05e )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( poleps2b )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "180.7h",		0x0000, 0x2000, 0xf85212c4 )
	ROM_LOAD	 ( "183.7f",		0x2000, 0x1000, 0xa9d4c380 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "176-v2.3l",		0x0000, 0x2000, 0x848ab742 )
	ROM_LOAD_EVEN( "177-v2.4l",		0x0000, 0x2000, 0x643483f7 )
	ROM_LOAD_EVEN( "rom-v2.4k",		0x4000, 0x1000, 0x2d70dce4 )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "178.3e",		0x0000, 0x2000, 0xeac35cfa )
	ROM_LOAD_EVEN( "179.4e",		0x0000, 0x2000, 0x613e917d )
	ROM_LOAD_ODD ( "184.3d",		0x4000, 0x2000, 0xd893c4ed )
	ROM_LOAD_EVEN( "185.4d",		0x4000, 0x2000, 0x899de75e )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "172.7n",		0x0000, 0x2000, 0xfbe5e72f )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",		0x0000, 0x2000, 0xec3ec6e6 )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "170.13n",		0x0000, 0x2000, 0x455d79a0 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "171.12n",		0x2000, 0x2000, 0x78372b81 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",		0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "166.13k",		0x2000, 0x2000, 0x2b0517bd )
	ROM_LOAD	 ( "168.13l",		0x4000, 0x2000, 0x4d7916d9 )
	ROM_LOAD	 ( "175.13m",		0x6000, 0x2000, 0xbd6df480 )
	ROM_LOAD	 ( "120.12j",		0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "167.12k",		0xa000, 0x2000, 0x411e21b5 )
	ROM_LOAD	 ( "169.12l",		0xc000, 0x2000, 0x662ff24b )
	ROM_LOAD	 ( "174.12m",		0xe000, 0x2000, 0xf0c571dc )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-186.bpr",	0x0000, 0x0100, 0x16d69c31 )	/* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",	0x0100, 0x0100, 0x07340311 )	/* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",	0x0200, 0x0100, 0x1efc84d7 )	/* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",	0x0300, 0x0100, 0x064d51a0 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",	0x0400, 0x0100, 0x7880c5af )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",	0x0800, 0x0400, 0x8b270902 )	/* road color PROM */
	ROM_LOAD	 ( "014-192.bpr",	0x0c00, 0x0400, 0xcaddb0b0 )	/* sprite color PROM */
	ROM_LOAD	 ( "131.11n",		0x1000, 0x1000, 0x5921777f )	/* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",		0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "128.2m",		0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",		0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",	0x5000, 0x2000, 0x5b4cf05e )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


ROM_START( poleps2c )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD	 ( "180.7h",		0x0000, 0x2000, 0xf85212c4 )
	ROM_LOAD	 ( "183.7f",		0x2000, 0x1000, 0xa9d4c380 )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD_ODD ( "3lcpu.rom",		0x0000, 0x2000, 0xcf95a6b7 )
	ROM_LOAD_EVEN( "177-v2.4l",		0x0000, 0x2000, 0x643483f7 )
	ROM_LOAD_EVEN( "cpu-4k.rom",	0x4000, 0x1000, 0x97a496b3 )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD_ODD ( "178.3e",		0x0000, 0x2000, 0xeac35cfa )
	ROM_LOAD_EVEN( "179.4e",		0x0000, 0x2000, 0x613e917d )
	ROM_LOAD_ODD ( "184.3d",		0x4000, 0x2000, 0xd893c4ed )
	ROM_LOAD_EVEN( "185.4d",		0x4000, 0x2000, 0x899de75e )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )		/* 2bpp alpha layer */
	ROM_LOAD	 ( "172.7n",		0x0000, 0x2000, 0xfbe5e72f )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )		/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",		0x0000, 0x2000, 0xec3ec6e6 )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )		/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "170.13n",		0x0000, 0x2000, 0x455d79a0 )	/* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "171.12n",		0x2000, 0x2000, 0x78372b81 )	/* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )		/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",		0x0000, 0x2000, 0x2e134b46 )	/* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "166.13k",		0x2000, 0x2000, 0x2b0517bd )
	ROM_LOAD	 ( "13lvid.rom",	0x4000, 0x2000, 0x9ab89d7f )
	ROM_LOAD	 ( "175.13m",		0x6000, 0x2000, 0xbd6df480 )
	ROM_LOAD	 ( "120.12j",		0x8000, 0x2000, 0x6f9997d2 )	/* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "12kvid.rom",	0xa000, 0x2000, 0xfa131a9b )
	ROM_LOAD	 ( "169.12l",		0xc000, 0x2000, 0x662ff24b )
	ROM_LOAD	 ( "174.12m",		0xe000, 0x2000, 0xf0c571dc )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS )
	ROM_LOAD	 ( "014-186.bpr",	0x0000, 0x0100, 0x16d69c31 )	/* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",	0x0100, 0x0100, 0x07340311 )	/* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",	0x0200, 0x0100, 0x1efc84d7 )	/* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",	0x0300, 0x0100, 0x064d51a0 )	/* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",	0x0400, 0x0100, 0x7880c5af )	/* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",	0x0500, 0x0100, 0x2d502464 )	/* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",	0x0600, 0x0100, 0x027aa62c )	/* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",	0x0700, 0x0100, 0x1f8d0df3 )	/* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",	0x0800, 0x0400, 0x8b270902 )	/* road color PROM */
	ROM_LOAD	 ( "014-192.bpr",	0x0c00, 0x0400, 0xcaddb0b0 )	/* sprite color PROM */
	ROM_LOAD	 ( "131.11n",		0x1000, 0x1000, 0x5921777f )	/* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",		0x2000, 0x2000, 0xee6b3315 )	/* road control PROM */
	ROM_LOAD	 ( "128.2m",		0x4000, 0x2000, 0x6d1e7042 )	/* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",		0x6000, 0x1000, 0x4e97f101 )	/* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1 )
	ROM_LOAD	 ( "014-118.bpr",	0x0000, 0x0100, 0x8568decc )	/* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",	0x1000, 0x2000, 0xb5ad4d5f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",	0x3000, 0x2000, 0x8fdd2f6f )	/* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",	0x5000, 0x2000, 0x5b4cf05e )	/* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1 )
	ROM_LOAD	 ( "014-117.bpr",	0x0000, 0x0100, 0x2401c817 )	/* sync chain */
ROM_END


/*********************************************************************
 * Initialization routines
 *********************************************************************/

static void init_polepos2(void)
{
	/* note that the bootleg versions don't need this custom IC; they have a hacked ROM in its place */
	install_mem_read_handler(1, 0x4000, 0x5fff, polepos2_ic25_r);
}


/*********************************************************************
 * Game drivers
 *********************************************************************/

GAME( 1982, polepos,  0,        polepos, polepos,  0,        ROT0, "Namco", "Pole Position" )
GAME( 1982, poleposa, polepos,  polepos, polepos,  0,        ROT0, "Namco (Atari license)", "Pole Position (Atari version 2)" )
GAME( 1982, polepos1, polepos,  polepos, polepos,  0,        ROT0, "[Namco] (Atari license)", "Pole Position (Atari version 1)" )
GAME( 1982, topracer, polepos,  polepos, polepos,  0,        ROT0, "bootleg", "Top Racer" )
GAME( 1983, polepos2, 0,        polepos, polepos2, polepos2, ROT0, "Namco", "Pole Position II" )
GAME( 1983, poleps2a, polepos2, polepos, polepos2, polepos2, ROT0, "Namco (Atari license)", "Pole Position II (Atari)" )
GAME( 1983, poleps2b, polepos2, polepos, polepos2, 0,        ROT0, "Namco (Atari license)", "Pole Position II (Atari bootleg 1)" )
GAME( 1983, poleps2c, polepos2, polepos, polepos2, 0,        ROT0, "Namco (Atari license)", "Pole Position II (Atari bootleg 2)" )
