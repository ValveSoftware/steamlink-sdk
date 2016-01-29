#include "../vidhrdw/relief.cpp"

/***************************************************************************

	Relief Pitcher

    driver by Aaron Giles

****************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( relief_playfieldram_w );
WRITE_HANDLER( relief_playfield2ram_w );
WRITE_HANDLER( relief_colorram_w );
WRITE_HANDLER( relief_video_control_w );

int relief_vh_start(void);
void relief_vh_stop(void);
void relief_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

void relief_scanline_update(int scanline);


static UINT8 ym2413_volume;
static UINT8 overall_volume;
static UINT32 adpcm_bank_base;



/*************************************
 *
 *	Interrupt handling
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_scanline_int_state)
		newstate = 4;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_video_control_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(relief_scanline_update, 8);

	OKIM6295_set_bank_base(0, ALL_VOICES, 0);
	ym2413_volume = 15;
	overall_volume = 127;
	adpcm_bank_base = 0;
}



/*************************************
 *
 *	I/O handling
 *
 *************************************/

static READ_HANDLER( special_port2_r )
{
	int result = input_port_2_r(offset);
	if (atarigen_cpu_to_sound_ready) result ^= 0x0020;
	if (!(result & 0x0080) || atarigen_get_hblank()) result ^= 0x0001;
	return result;
}



/*************************************
 *
 *	Audio control I/O
 *
 *************************************/

static WRITE_HANDLER( audio_control_w )
{
	(void)offset;
	if (!(data & 0x00ff0000))
	{
		ym2413_volume = (data >> 1) & 15;
		atarigen_set_ym2413_vol((ym2413_volume * overall_volume * 100) / (127 * 15));
		adpcm_bank_base = (0x040000 * ((data >> 6) & 3)) | (adpcm_bank_base & 0x100000);
	}
	if (!(data & 0xff000000))
		adpcm_bank_base = (0x100000 * ((data >> 8) & 1)) | (adpcm_bank_base & 0x0c0000);

	OKIM6295_set_bank_base(0, ALL_VOICES, adpcm_bank_base);
}


static WRITE_HANDLER( audio_volume_w )
{
	(void)offset;
	if (!(data & 0x00ff0000))
	{
		overall_volume = data & 127;
		atarigen_set_ym2413_vol((ym2413_volume * overall_volume * 100) / (127 * 15));
		atarigen_set_oki6295_vol(overall_volume * 100 / 127);
	}
}



/*************************************
 *
 *	MSM5295 I/O
 *
 *************************************/

static READ_HANDLER( adpcm_r )
{
	return OKIM6295_status_0_r(offset) | 0xff00;
}


static WRITE_HANDLER( adpcm_w )
{
	if (!(data & 0x00ff0000))
		OKIM6295_data_0_w(offset, data & 0xff);
}



/*************************************
 *
 *	YM2413 I/O
 *
 *************************************/

static READ_HANDLER( ym2413_r )
{
	(void)offset;
	return YM2413_status_port_0_r(0) | 0xff00;
}


static WRITE_HANDLER( ym2413_w )
{
	if (!(data & 0x00ff0000))
	{
		if (offset & 2)
			YM2413_data_port_0_w(0, data & 0xff);
		else
			YM2413_register_port_0_w(0, data & 0xff);
	}
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x140000, 0x140003, ym2413_r },
	{ 0x140010, 0x140011, adpcm_r },
	{ 0x180000, 0x180fff, atarigen_eeprom_upper_r },
	{ 0x260000, 0x260001, input_port_0_r },
	{ 0x260002, 0x260003, input_port_1_r },
	{ 0x260010, 0x260011, special_port2_r },
	{ 0x260012, 0x260013, input_port_3_r },
	{ 0x3effc0, 0x3effff, atarigen_video_control_r },
	{ 0xfe0000, 0xfe0fff, MRA_BANK1 },
	{ 0xfeffc0, 0xfeffff, atarigen_video_control_r },
	{ 0xff0000, 0xff5fff, MRA_BANK3 },
	{ 0xff6000, 0xff7fff, MRA_BANK4 },
	{ 0xff8000, 0xff8fff, MRA_BANK5 },
	{ 0xff9000, 0xffffff, MRA_BANK6 },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x140000, 0x140003, ym2413_w },
	{ 0x140010, 0x140011, adpcm_w },
	{ 0x140020, 0x140021, audio_volume_w },
	{ 0x140030, 0x140031, audio_control_w },
	{ 0x180000, 0x180fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0x1c0030, 0x1c0031, atarigen_eeprom_enable_w },
	{ 0x2a0000, 0x2a0001, watchdog_reset_w },
	{ 0x3effc0, 0x3effff, atarigen_video_control_w, &atarigen_video_control_data },
	{ 0xfe0000, 0xfe0fff, atarigen_666_paletteram_w, &paletteram },
	{ 0xfeffc0, 0xfeffff, atarigen_video_control_w },
	{ 0xff0000, 0xff1fff, relief_playfield2ram_w, &atarigen_playfield2ram, &atarigen_playfield2ram_size },
	{ 0xff2000, 0xff3fff, relief_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0xff4000, 0xff5fff, relief_colorram_w, &atarigen_playfieldram_color },
	{ 0xff6000, 0xff7fff, MWA_BANK4, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0xff8000, 0xff8fff, MWA_BANK5, &atarigen_alpharam, &atarigen_alpharam_size },
	{ 0xff9000, 0xffffff, MWA_BANK6 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( relief )
	PORT_START	/* 260000 */
	PORT_BIT(  0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )

	PORT_START	/* 260002 */
	PORT_BIT(  0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )

	PORT_START	/* 260010 */
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNUSED )	/* tested before writing to 260040 */
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT(  0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 260012 */
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_BIT(  0xffdc, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout pfanmolayout =
{
	8,8,	/* 8*8 sprites */
	32768,	/* 32768 of them */
	4,		/* 4 bits per pixel */
	{ 0x80000*3*8, 0x80000*2*8, 0x80000*1*8, 0x80000*0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x000000, &pfanmolayout,   0, 64 },		/* alpha & playfield */
	{ REGION_GFX1, 0x000001, &pfanmolayout, 256, 16 },		/* sprites */
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct OKIM6295interface okim6295_interface =
{
	1,					/* 1 chip */
	{ ATARI_CLOCK_14MHz/4/3/165 },
	{ REGION_SOUND1 },
	{ 75 }
};


static struct YM2413interface ym2413_interface =
{
	1,					/* 1 chip */
	ATARI_CLOCK_14MHz/4,
	{ 75 },
	{ 0 }
};


/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_relief =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			readmem,writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	relief_vh_start,
	relief_vh_stop,
	relief_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		},
		{
			SOUND_YM2413,
			&ym2413_interface
		}
	},

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM decoding
 *
 *************************************/

static void rom_decode(void)
{
	UINT8 *base = memory_region(REGION_SOUND1);
	int i;

	/* invert the graphics bits */
	for (i = 0; i < memory_region_length(REGION_GFX1); i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;

	/* expand the ADPCM data to avoid lots of memcpy's during gameplay */
	/* the upper 128k is fixed, the lower 128k is bankswitched */
	memcpy(&base[0x000000], &base[0x100000], 0x20000);
	memcpy(&base[0x040000], &base[0x100000], 0x20000);
	memcpy(&base[0x080000], &base[0x140000], 0x20000);
	memcpy(&base[0x0c0000], &base[0x160000], 0x20000);
	memcpy(&base[0x100000], &base[0x180000], 0x20000);
	memcpy(&base[0x140000], &base[0x1a0000], 0x20000);
	memcpy(&base[0x180000], &base[0x1c0000], 0x20000);
	memcpy(&base[0x1c0000], &base[0x1e0000], 0x20000);

	memcpy(&base[0x020000], &base[0x120000], 0x20000);
	memcpy(&base[0x060000], &base[0x120000], 0x20000);
	memcpy(&base[0x0a0000], &base[0x120000], 0x20000);
	memcpy(&base[0x0e0000], &base[0x120000], 0x20000);
	memcpy(&base[0x120000], &base[0x120000], 0x20000);
	memcpy(&base[0x160000], &base[0x120000], 0x20000);
	memcpy(&base[0x1a0000], &base[0x120000], 0x20000);
	memcpy(&base[0x1e0000], &base[0x120000], 0x20000);
}



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_relief(void)
{
	static const UINT16 default_eeprom[] =
	{
		0x0001,0x0166,0x0128,0x01E6,0x0100,0x012C,0x0300,0x0144,
		0x0700,0x01C0,0x2F00,0x01EC,0x0B00,0x0148,0x0140,0x0100,
		0x0124,0x0188,0x0120,0x0600,0x0196,0x013C,0x0192,0x0150,
		0x0166,0x0128,0x01E6,0x0100,0x012C,0x0300,0x0144,0x0700,
		0x01C0,0x2F00,0x01EC,0x0B00,0x0148,0x0140,0x0100,0x0124,
		0x0188,0x0120,0x0600,0x0196,0x013C,0x0192,0x0150,0xFF00,
		0x9500,0x0000
	};

	atarigen_eeprom_default = default_eeprom;

	rom_decode();
}


static void init_relief2(void)
{
	static const UINT16 default_eeprom[] =
	{
		0x0001,0x01FD,0x019F,0x015E,0x01FF,0x019E,0x03FF,0x015F,
		0x07FF,0x01FD,0x12FF,0x01FC,0x01FB,0x07FF,0x01F7,0x01FF,
		0x01DF,0x02FF,0x017F,0x03FF,0x0300,0x0110,0x0300,0x0140,
		0x0300,0x018E,0x0400,0x0180,0x0101,0x0300,0x0180,0x0204,
		0x0120,0x0182,0x0100,0x0102,0x0600,0x01D5,0x0138,0x0192,
		0x0150,0x01FD,0x019F,0x015E,0x01FF,0x019E,0x03FF,0x015F,
		0x07FF,0x01FD,0x12FF,0x01FC,0x01FB,0x07FF,0x01F7,0x01FF,
		0x01DF,0x02FF,0x017F,0x03FF,0x0300,0x0110,0x0300,0x0140,
		0x0300,0x018E,0x0400,0x0180,0x0101,0x0300,0x0180,0x0204,
		0x0120,0x0182,0x0100,0x0102,0x0600,0x01D5,0x0138,0x0192,
		0x0150,0xE600,0x01C3,0x019D,0x0131,0x0100,0x0116,0x0100,
		0x010A,0x0190,0x010E,0x014A,0x0200,0x010B,0x018D,0x0121,
		0x0100,0x0145,0x0100,0x0109,0x0184,0x012C,0x0200,0x0107,
		0x01AA,0x0149,0x60FF,0x3300,0x0000
	};

	atarigen_eeprom_default = default_eeprom;

	rom_decode();
}



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( relief )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "0011d.19e", 0x00000, 0x20000, 0xcb3f73ad )
	ROM_LOAD_ODD ( "0012d.19j", 0x00000, 0x20000, 0x90655721 )
	ROM_LOAD_EVEN( "093-0013.17e", 0x40000, 0x20000, 0x1e1e82e5 )
	ROM_LOAD_ODD ( "093-0014.17j", 0x40000, 0x20000, 0x19e5decd )

	ROM_REGION( 0x240000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "093-0025.14s", 0x000000, 0x80000, 0x1b9e5ef2 )
	ROM_LOAD( "093-0026.8d",  0x080000, 0x80000, 0x09b25d93 )
	ROM_LOAD( "093-0027.18s", 0x100000, 0x80000, 0x5bc1c37b )
	ROM_LOAD( "093-0028.10d", 0x180000, 0x80000, 0x55fb9111 )
	ROM_LOAD( "093-0029.4d",  0x200000, 0x40000, 0xe4593ff4 )

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* 2MB for ADPCM data */
	ROM_LOAD( "093-0030.9b",  0x100000, 0x80000, 0xf4c567f5 )
	ROM_LOAD( "093-0031.10b", 0x180000, 0x80000, 0xba908d73 )
ROM_END

ROM_START( relief2 )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "093-0011.19e", 0x00000, 0x20000, 0x794cea33 )
	ROM_LOAD_ODD ( "093-0012.19j", 0x00000, 0x20000, 0x577495f8 )
	ROM_LOAD_EVEN( "093-0013.17e", 0x40000, 0x20000, 0x1e1e82e5 )
	ROM_LOAD_ODD ( "093-0014.17j", 0x40000, 0x20000, 0x19e5decd )

	ROM_REGION( 0x240000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "093-0025.14s", 0x000000, 0x80000, 0x1b9e5ef2 )
	ROM_LOAD( "093-0026.8d",  0x080000, 0x80000, 0x09b25d93 )
	ROM_LOAD( "093-0027.18s", 0x100000, 0x80000, 0x5bc1c37b )
	ROM_LOAD( "093-0028.10d", 0x180000, 0x80000, 0x55fb9111 )
	ROM_LOAD( "093-0029.4d",  0x200000, 0x40000, 0xe4593ff4 )

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* 2MB for ADPCM data */
	ROM_LOAD( "093-0030.9b",  0x100000, 0x80000, 0xf4c567f5 )
	ROM_LOAD( "093-0031.10b", 0x180000, 0x80000, 0xba908d73 )
ROM_END



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1992, relief,  0,      relief, relief, relief,  ROT0, "Atari Games", "Relief Pitcher (set 1)" )
GAME( 1992, relief2, relief, relief, relief, relief2, ROT0, "Atari Games", "Relief Pitcher (set 2)" )
