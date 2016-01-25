#include "../vidhrdw/xexex.cpp"

/***************************************************************************

Xexex

***************************************************************************/

#include "driver.h"
#include "cpuintrf.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"
#include "cpu/z80/z80.h"
#include "machine/eeprom.h"

int xexex_vh_start(void);
void xexex_vh_stop(void);
void xexex_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( xexex_palette_w );

WRITE_HANDLER( K053157_ram_w );
WRITE_HANDLER( K053157_w );
READ_HANDLER( K053157_r );

static int cur_rombank;
static int cur_back_select, cur_back_ctrla;
static int cur_control2;

unsigned char *xexex_palette_ram;

static int init_eeprom_count;

static struct EEPROM_interface eeprom_interface =
{
	7,				/* address bits */
	8,				/* data bits */
	"011000",		/*  read command */
	"011100",		/* write command */
	"0100100000000",/* erase command */
	"0100000000000",/* lock command */
	"0100110000000" /* unlock command */
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
			init_eeprom_count = 0;
			EEPROM_load(file);
		}
		else
			init_eeprom_count = 10;
	}
}

static void gfx_init(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_4(REGION_GFX2);
}

static void init_xexex(void)
{
	cur_rombank = 0;

	gfx_init();
}

static READ_HANDLER( control0_r )
{
	return input_port_0_r(0);
}

static READ_HANDLER( control1_r )
{
	int res;

	/* bit 0 is EEPROM data */
	/* bit 1 is EEPROM ready */
	/* bit 3 is service button */
	res = EEPROM_read_bit() | input_port_1_r(0);

	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xf7;
	}

	return res;
}

static READ_HANDLER( control2_r )
{
	return cur_control2;
}

static WRITE_HANDLER( control2_w )
{
	/* bit 0  is data */
	/* bit 1  is cs (active low) */
	/* bit 2  is clock (active high) */
	/* bit 11 is watchdog */

	EEPROM_write_bit(data & 0x01);
	EEPROM_set_cs_line((data & 0x02) ? CLEAR_LINE : ASSERT_LINE);
	EEPROM_set_clock_line((data & 0x04) ? ASSERT_LINE : CLEAR_LINE);
	cur_control2 = data;

	/* bit 8 = enable sprite ROM reading */
	K053246_set_OBJCHA_line((data & 0x0100) ? ASSERT_LINE : CLEAR_LINE);
}

static int xexex_interrupt(void)
{
	switch (cpu_getiloops())
	{
		case 0:
			if (K053247_is_IRQ_enabled()) return 4;	/* ??? */
			break;

		case 1:
			if (K053247_is_IRQ_enabled()) return 5;	/* ??? */
			break;

		case 2:
//			if (K053247_is_IRQ_enabled()) return 6;	/* ??? */
			break;
	}
	return ignore_interrupt();
}

static int sound_status = 0, sound_cmd = 0;

static WRITE_HANDLER( sound_cmd_w )
{
	//logerror("Sound command : %d\n", data & 0xff);
	sound_cmd = data & 0xff;
	//	cpu_set_irq_line(1, 0, HOLD_LINE);
	if(sound_cmd == 0xfe)
	  sound_status = 0x7f;
}

static READ_HANDLER( sound_status_r )
{
	return sound_status;
}

static READ_HANDLER( back_ctrla_r )
{
	return cur_back_ctrla;
}

static WRITE_HANDLER( back_ctrla_w )
{
	data &= 0xff;
	if(data != cur_back_ctrla) {
		//logerror("Back: ctrla = %02x (%08x)\n", data, cpu_get_pc());
		cur_back_ctrla = data;
	}
}

static READ_HANDLER( back_select_r )
{
	return cur_back_select;
}

static WRITE_HANDLER( back_select_w )
{
	data &= 0xff;
	if(data != cur_back_select) {
		//logerror("Back: select = %02x (%08x)\n", data, cpu_get_pc());
		cur_back_select = data;
	}
}

static READ_HANDLER( backrom_r )
{
	/*if (!(cur_back_ctrla & 1))
		logerror("Back: Reading rom memory with enable=0\n");*/
	return *(memory_region(REGION_GFX3) + 2048*cur_back_select + (offset>>2));
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x080000, 0x097fff, MRA_BANK1 },			// Main RAM
	{ 0x0c0000, 0x0c003f, K053157_r },
	{ 0x0c4000, 0x0c4001, K053246_word_r },
	{ 0x0c6000, 0x0c6fff, MRA_BANK4 },			// Sprites
	{ 0x0c800a, 0x0c800b, back_ctrla_r },
	{ 0x0c800e, 0x0c800f, back_select_r },
	{ 0x0d6014, 0x0d6015, sound_status_r },
	{ 0x0da000, 0x0da001, input_port_2_r },
	{ 0x0da002, 0x0da003, input_port_3_r },
	{ 0x0dc000, 0x0dc001, control0_r },
	{ 0x0dc002, 0x0dc003, control1_r },
	{ 0x0de000, 0x0de001, control2_r },
	{ 0x100000, 0x17ffff, MRA_ROM },
	{ 0x180000, 0x181fff, MRA_BANK2 },			// Graphic planes
	{ 0x190000, 0x191fff, MRA_BANK6 }, 			// Passthrough to tile roms
	{ 0x1a0000, 0x1a1fff, backrom_r },
	{ 0x1b0000, 0x1b1fff, MRA_BANK5 },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080000, 0x097fff, MWA_BANK1 },
	{ 0x0c0000, 0x0c003f, K053157_w },
	{ 0x0c2000, 0x0c2007, K053246_word_w },
	{ 0x0c6000, 0x0c6fff, MWA_BANK4 },
	{ 0x0c800a, 0x0c800b, back_ctrla_w },
	{ 0x0c800e, 0x0c800f, back_select_w },
	{ 0x0d600c, 0x0d600d, sound_cmd_w },
	{ 0x0de000, 0x0de001, control2_w },
	{ 0x100000, 0x17ffff, MWA_ROM },
	{ 0x180000, 0x181fff, K053157_ram_w },
	{ 0x1b0000, 0x1b1fff, xexex_palette_w, &xexex_palette_ram },
	{ -1 }
};

#if 0
static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK3 },
	{ 0xc000, 0xdf7f, MRA_RAM },
	{ 0xe000, 0xe22f, MRA_RAM },
	{ 0xec01, 0xec01, YM2151_status_port_0_r },
	{ 0xf002, 0xf002, sound_cmd_r },
	{ -1 }
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xdf7f, MWA_RAM },
	{ 0xe000, 0xe22f, MWA_RAM },
	{ 0xec00, 0xec00, YM2151_register_port_0_w },
	{ 0xec01, 0xec01, YM2151_data_port_0_w },
	{ 0xf000, 0xf000, sound_status_w },
	{ 0xf800, 0xf800, sound_bankswitch_w },
	{ -1 }
};
#endif


INPUT_PORTS_START( xexex )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0xcc, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )	// EEPROM data
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_UNUSED )	// EEPROM ready (always 1)
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0xf4, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

INPUT_PORTS_END

static struct GfxLayout char_layout =
{
	8, 8,
	0x200000*8/(8*8*4),
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*8*4, 1*8*4, 2*8*4, 3*8*4, 4*8*4, 5*8*4, 6*8*4, 7*8*4 },
	8*8*4,
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x000000, &char_layout, 0, 128 },
	{ -1 }
};

static struct MachineDriver machine_driver_xexex =
{
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz ? (xtal is 32MHz) */
			readmem, writemem, 0, 0,
			xexex_interrupt, 3	/* ??? */
		},
#if 0
		{
			CPU_Z80,
			2000000,	/* 2 MHz ? (xtal is 32MHz/19.432Mhz) */
			sound_readmem, sound_writemem, 0, 0,
			ignore_interrupt, 1
		},
#endif
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8,
	{ 0*8, 64*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	xexex_vh_start,
	xexex_vh_stop,
	xexex_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{0}
	},

	nvram_handler
};


ROM_START( xexex )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_EVEN( "xex_a01.rom", 0x000000,  0x40000, 0x3ebcb066 )
	ROM_LOAD_ODD ( "xex_a02.rom", 0x000000,  0x40000, 0x36ea7a48 )
	ROM_LOAD_EVEN( "xex_b03.rom", 0x100000,  0x40000, 0x97833086 )
	ROM_LOAD_ODD ( "xex_b04.rom", 0x100000,  0x40000, 0x26ec5dc8 )

	ROM_REGION( 0x30000, REGION_CPU2 )
	ROM_LOAD( "xex_a05.rom", 0x000000, 0x020000, 0x0e33d6ec )
	ROM_RELOAD(              0x010000, 0x020000 )

	ROM_REGION( 0x200000, REGION_GFX1 )
	ROM_LOAD( "xex_b14.rom", 0x000000, 0x100000, 0x02a44bfa )
	ROM_LOAD( "xex_b13.rom", 0x100000, 0x100000, 0x633c8eb5 )

	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD( "xex_b12.rom", 0x000000, 0x100000, 0x08d611b0 )
	ROM_LOAD( "xex_b11.rom", 0x100000, 0x100000, 0xa26f7507 )
	ROM_LOAD( "xex_b10.rom", 0x200000, 0x100000, 0xee31db8d )
	ROM_LOAD( "xex_b09.rom", 0x300000, 0x100000, 0x88f072ef )

	ROM_REGION( 0x80000, REGION_GFX3 )
	ROM_LOAD( "xex_b08.rom", 0x000000, 0x080000, 0xca816b7b )
ROM_END



GAME( 1991, xexex, 0, xexex, xexex, xexex, ORIENTATION_FLIP_Y, "Konami", "Xexex" )
