#include "../vidhrdw/skullxbo.cpp"

/***************************************************************************

Skull & Crossbones Memory Map
-----------------------------

driver by Aaron Giles

SKULL & CROSSBONES 68000 MEMORY MAP

Function                           Address        R/W  DATA
-------------------------------------------------------------
Program ROM                        000000-07FFFF  R    D0-D15

Switch 1 (Player 1)                FC0000         R    D0-D7
Action                                            R    D5
Step (Development Only)                           R    D4
Joystick Left                                     R    D3
Joystick Right                                    R    D2
Joystick Down                                     R    D1
Joystick Up                                       R    D0

Switch 2 (Player 2)                FC2000         R    D0-D7
Action                                            R    D5
Step (Development Only)                           R    D4
Joystick Left                                     R    D3
Joystick Right                                    R    D2
Joystick Down                                     R    D1
Joystick Up                                       R    D0

Self-Test (Active Low)             FC4000         R    D7
Vertical Blank                                    R    D6
Audio Busy Flag (Active Low)                      R    D5

Audio Receive Port                 FC6000         R    D8-D15

EEPROM                             FC8000-FC8FFE  R/W  D0-D7

Color RAM                          FCA000-FCAFFE  R/W  D0-D15

Unlock EEPROM                      FD0000         W    xx
Sound Processor Reset              FD2000         W    xx
Watchdog reset                     FD4000         W    xx
IRQ Acknowledge                    FD6000         W    xx
Audio Send Port                    FD8000         W    D8-D15

Playfield RAM                      FF0000-FF1FFF  R/W  D0-D15
Alpha RAM                          FF2000-FF2FFF  R/W  D0-D15
Motion Object RAM                  FF3000-FF3FFF  R/W  D0-D15
RAM                                FF4000-FFFFFF  R/W

****************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "sndhrdw/atarijsa.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( skullxbo_playfieldram_w );
WRITE_HANDLER( skullxbo_playfieldlatch_w );
WRITE_HANDLER( skullxbo_hscroll_w );
WRITE_HANDLER( skullxbo_vscroll_w );
WRITE_HANDLER( skullxbo_mobmsb_w );

int skullxbo_vh_start(void);
void skullxbo_vh_stop(void);
void skullxbo_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

void skullxbo_scanline_update(int param);




/*************************************
 *
 *	Initialization & interrupts
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_scanline_int_state)
		newstate = 1;
	if (atarigen_video_int_state)
		newstate = 2;
	if (atarigen_sound_int_state)
		newstate = 4;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void irq_gen(int param)
{
	(void)param;
	atarigen_scanline_int_gen();
}


static void alpha_row_update(int scanline)
{
	UINT16 *check = (UINT16 *)&atarigen_alpharam[((scanline / 8) * 64 + 42) * 2];

	/* check for interrupts in the alpha ram */
	/* the interrupt occurs on the HBLANK of the 6th scanline following */
	if ((UINT8 *)check < &atarigen_alpharam[atarigen_alpharam_size] && (*check & 0x8000))
		timer_set(cpu_getscanlineperiod() * 6.9, 0, irq_gen);

	/* update the playfield and motion objects */
	skullxbo_scanline_update(scanline);
}


static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(alpha_row_update, 8);
	atarijsa_reset();
}



/*************************************
 *
 *	I/O read dispatch.
 *
 *************************************/

static READ_HANDLER( special_port1_r )
{
	int temp = input_port_1_r(offset);
	if (atarigen_cpu_to_sound_ready) temp ^= 0x0040;
	if (atarigen_get_hblank()) temp ^= 0x0010;
	return temp;
}



/*************************************
 *
 *	Who knows what this is?
 *
 *************************************/

static WRITE_HANDLER( skullxbo_mobwr_w )
{
	//logerror("MOBWR[%02X] = %04X\n", offset, data);
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0xff2000, 0xff2fff, MRA_BANK1 },
	{ 0xff5000, 0xff5001, atarigen_sound_r },
	{ 0xff5800, 0xff5801, input_port_0_r },
	{ 0xff5802, 0xff5803, special_port1_r },
	{ 0xff6000, 0xff6fff, atarigen_eeprom_r },
	{ 0xff8000, 0xffbfff, MRA_BANK2 },
	{ 0xffc000, 0xffcfff, MRA_BANK3 },
	{ 0xffd000, 0xffdfff, MRA_BANK4 },
	{ 0xffe000, 0xffffff, MRA_BANK5 },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x000000, 0x08ffff, MWA_ROM },
	{ 0xff0000, 0xff07ff, skullxbo_mobmsb_w },
	{ 0xff0800, 0xff0bff, atarigen_halt_until_hblank_0_w },
	{ 0xff0c00, 0xff0fff, atarigen_eeprom_enable_w },
	{ 0xff1000, 0xff13ff, atarigen_video_int_ack_w },
	{ 0xff1400, 0xff17ff, atarigen_sound_w },
	{ 0xff1800, 0xff1bff, atarigen_sound_reset_w },
	{ 0xff1c00, 0xff1c7f, skullxbo_playfieldlatch_w },
	{ 0xff1c80, 0xff1cff, skullxbo_hscroll_w },
	{ 0xff1d00, 0xff1d7f, atarigen_scanline_int_ack_w },
	{ 0xff1d80, 0xff1dff, watchdog_reset_w },
	{ 0xff1e00, 0xff1e7f, skullxbo_playfieldlatch_w },
	{ 0xff1e80, 0xff1eff, skullxbo_hscroll_w },
	{ 0xff1f00, 0xff1f7f, atarigen_scanline_int_ack_w },
	{ 0xff1f80, 0xff1fff, watchdog_reset_w },
	{ 0xff2000, 0xff2fff, atarigen_666_paletteram_w, &paletteram },
	{ 0xff4000, 0xff47ff, skullxbo_vscroll_w },
	{ 0xff4800, 0xff4fff, skullxbo_mobwr_w },
	{ 0xff6000, 0xff6fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0xff8000, 0xffbfff, skullxbo_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0xffc000, 0xffcfff, MWA_BANK3, &atarigen_alpharam, &atarigen_alpharam_size },
	{ 0xffd000, 0xffdfff, MWA_BANK4, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0xffe000, 0xffffff, MWA_BANK5 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( skullxbo )
	PORT_START      /* ff5800 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )

	PORT_START      /* ff5802 */
	PORT_BIT( 0x000f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_UNUSED )	/* HBLANK */
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )	/* /AUDBUSY */
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )

	JSA_II_PORT		/* audio board port */
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout pflayout =
{
	16,8,	/* 8*8 chars */
	20480,	/* 20480 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0x50000*8+0,0x50000*8+0, 0x50000*8+4,0x50000*8+4, 0,0, 4,4,
	  0x50000*8+8,0x50000*8+8, 0x50000*8+12,0x50000*8+12, 8,8, 12,12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every char takes 16 consecutive bytes */
};


static struct GfxLayout anlayout =
{
	16,8,	/* 8*8 chars */
	2048,	/* 2048 chars */
	2,		/* 2 bits per pixel */
	{ 0, 1 },
	{ 0,0, 2,2, 4,4, 6,6, 8,8, 10,10, 12,12, 14,14 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16	/* every char takes 16 consecutive bytes */
};


static struct GfxLayout molayout =
{
	16,8,	/* 8*8 chars */
	20480,	/* 20480 chars */
	5,		/* 5 bits per pixel */
	{ 4*0x50000*8, 3*0x50000*8, 2*0x50000*8, 1*0x50000*8, 0*0x50000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every char takes 16 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &molayout, 0x000, 32 },
	{ REGION_GFX2, 0, &pflayout, 0x200, 16 },
	{ REGION_GFX3, 0, &anlayout, 0x300, 16 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_skullxbo =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			main_readmem,main_writemem,0,0,
			atarigen_video_int_gen,1
		},
		JSA_II_CPU
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	init_machine,

	/* video hardware */
	42*16, 30*8, { 0*16, 42*16-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK |
			VIDEO_PIXEL_ASPECT_RATIO_1_2,
	0,
	skullxbo_vh_start,
	skullxbo_vh_stop,
	skullxbo_vh_screenrefresh,

	/* sound hardware */
	JSA_II_MONO(REGION_SOUND1),

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( skullxbo )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "5150", 0x00000, 0x10000, 0x9546d88b )
	ROM_LOAD_ODD ( "5151", 0x00000, 0x10000, 0xb9ed8bd4 )
	ROM_LOAD_EVEN( "5152", 0x20000, 0x10000, 0xc07e44fc )
	ROM_LOAD_ODD ( "5153", 0x20000, 0x10000, 0xfef8297f )
	ROM_LOAD_EVEN( "5154", 0x40000, 0x10000, 0xde4101a3 )
	ROM_LOAD_ODD ( "5155", 0x40000, 0x10000, 0x78c0f6ad )
	ROM_LOAD_EVEN( "5156", 0x70000, 0x08000, 0xcde16b55 )
	ROM_LOAD_ODD ( "5157", 0x70000, 0x08000, 0x31c77376 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "1149",      0x10000, 0x4000, 0x8d730e7a )
	ROM_CONTINUE(          0x04000, 0xc000 )

	ROM_REGION( 0x190000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1102",     0x000000, 0x10000, 0x90becdfa ) /* MO */
	ROM_LOAD( "1104",     0x010000, 0x10000, 0x33609071 ) /* MO */
	ROM_LOAD( "1106",     0x020000, 0x10000, 0x71962e9f ) /* MO */
	ROM_LOAD( "1101",     0x030000, 0x10000, 0x4d41701e ) /* MO */
	ROM_LOAD( "1103",     0x040000, 0x10000, 0x3011da3b ) /* MO */

	ROM_LOAD( "1108",     0x050000, 0x10000, 0x386c7edc ) /* MO */
	ROM_LOAD( "1110",     0x060000, 0x10000, 0xa54d16e6 ) /* MO */
	ROM_LOAD( "1112",     0x070000, 0x10000, 0x669411f6 ) /* MO */
	ROM_LOAD( "1107",     0x080000, 0x10000, 0xcaaeb57a ) /* MO */
	ROM_LOAD( "1109",     0x090000, 0x10000, 0x61cb4e28 ) /* MO */

	ROM_LOAD( "1114",     0x0a0000, 0x10000, 0xe340d5a1 ) /* MO */
	ROM_LOAD( "1116",     0x0b0000, 0x10000, 0xf25b8aca ) /* MO */
	ROM_LOAD( "1118",     0x0c0000, 0x10000, 0x8cf73585 ) /* MO */
	ROM_LOAD( "1113",     0x0d0000, 0x10000, 0x899b59af ) /* MO */
	ROM_LOAD( "1115",     0x0e0000, 0x10000, 0xcf4fd19a ) /* MO */

	ROM_LOAD( "1120",     0x0f0000, 0x10000, 0xfde7c03d ) /* MO */
	ROM_LOAD( "1122",     0x100000, 0x10000, 0x6ff6a9f2 ) /* MO */
	ROM_LOAD( "1124",     0x110000, 0x10000, 0xf11909f1 ) /* MO */
	ROM_LOAD( "1119",     0x120000, 0x10000, 0x6f8003a1 ) /* MO */
	ROM_LOAD( "1121",     0x130000, 0x10000, 0x8ff0a1ec ) /* MO */

	ROM_LOAD( "1125",     0x140000, 0x10000, 0x3aa7c756 ) /* MO */
	ROM_LOAD( "1126",     0x150000, 0x10000, 0xcb82c9aa ) /* MO */
	ROM_LOAD( "1128",     0x160000, 0x10000, 0xdce32863 ) /* MO */
	/* 170000-18ffff empty */

	ROM_REGION( 0x0a0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2129",     0x000000, 0x10000, 0x36b1a578 ) /* playfield */
	ROM_LOAD( "2131",     0x010000, 0x10000, 0x7b7c04a1 ) /* playfield */
	ROM_LOAD( "2133",     0x020000, 0x10000, 0xe03fe4d9 ) /* playfield */
	ROM_LOAD( "2135",     0x030000, 0x10000, 0x7d497110 ) /* playfield */
	ROM_LOAD( "2137",     0x040000, 0x10000, 0xf91e7872 ) /* playfield */
	ROM_LOAD( "2130",     0x050000, 0x10000, 0xb25368cc ) /* playfield */
	ROM_LOAD( "2132",     0x060000, 0x10000, 0x112f2d20 ) /* playfield */
	ROM_LOAD( "2134",     0x070000, 0x10000, 0x84884ed6 ) /* playfield */
	ROM_LOAD( "2136",     0x080000, 0x10000, 0xbc028690 ) /* playfield */
	ROM_LOAD( "2138",     0x090000, 0x10000, 0x60cec955 ) /* playfield */

	ROM_REGION( 0x008000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2141",     0x000000, 0x08000, 0x60d6d6df ) /* alphanumerics */

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* 256k for ADPCM samples */
	ROM_LOAD( "1145",      0x00000, 0x10000, 0xd9475d58 )
	ROM_LOAD( "1146",      0x10000, 0x10000, 0x133e6aef )
	ROM_LOAD( "1147",      0x20000, 0x10000, 0xba4d556e )
	ROM_LOAD( "1148",      0x30000, 0x10000, 0xc48df49a )
ROM_END


ROM_START( skullxb2 )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "sku0h.bin", 0x00000, 0x10000, 0x47083d59 )
	ROM_LOAD_ODD ( "sku0l.bin", 0x00000, 0x10000, 0x2c03feaf )
	ROM_LOAD_EVEN( "sku1h.bin", 0x20000, 0x10000, 0xaa0471de )
	ROM_LOAD_ODD ( "sku1l.bin", 0x20000, 0x10000, 0xa65386f9 )
	ROM_LOAD_EVEN( "5154",      0x40000, 0x10000, 0xde4101a3 )
	ROM_LOAD_ODD ( "5155",      0x40000, 0x10000, 0x78c0f6ad )
	ROM_LOAD_EVEN( "5156",      0x70000, 0x08000, 0xcde16b55 )
	ROM_LOAD_ODD ( "5157",      0x70000, 0x08000, 0x31c77376 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "1149",      0x10000, 0x4000, 0x8d730e7a )
	ROM_CONTINUE(          0x04000, 0xc000 )

	ROM_REGION( 0x190000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1102",     0x000000, 0x10000, 0x90becdfa ) /* MO */
	ROM_LOAD( "1104",     0x010000, 0x10000, 0x33609071 ) /* MO */
	ROM_LOAD( "1106",     0x020000, 0x10000, 0x71962e9f ) /* MO */
	ROM_LOAD( "1101",     0x030000, 0x10000, 0x4d41701e ) /* MO */
	ROM_LOAD( "1103",     0x040000, 0x10000, 0x3011da3b ) /* MO */

	ROM_LOAD( "1108",     0x050000, 0x10000, 0x386c7edc ) /* MO */
	ROM_LOAD( "1110",     0x060000, 0x10000, 0xa54d16e6 ) /* MO */
	ROM_LOAD( "1112",     0x070000, 0x10000, 0x669411f6 ) /* MO */
	ROM_LOAD( "1107",     0x080000, 0x10000, 0xcaaeb57a ) /* MO */
	ROM_LOAD( "1109",     0x090000, 0x10000, 0x61cb4e28 ) /* MO */

	ROM_LOAD( "1114",     0x0a0000, 0x10000, 0xe340d5a1 ) /* MO */
	ROM_LOAD( "1116",     0x0b0000, 0x10000, 0xf25b8aca ) /* MO */
	ROM_LOAD( "1118",     0x0c0000, 0x10000, 0x8cf73585 ) /* MO */
	ROM_LOAD( "1113",     0x0d0000, 0x10000, 0x899b59af ) /* MO */
	ROM_LOAD( "1115",     0x0e0000, 0x10000, 0xcf4fd19a ) /* MO */

	ROM_LOAD( "1120",     0x0f0000, 0x10000, 0xfde7c03d ) /* MO */
	ROM_LOAD( "1122",     0x100000, 0x10000, 0x6ff6a9f2 ) /* MO */
	ROM_LOAD( "1124",     0x110000, 0x10000, 0xf11909f1 ) /* MO */
	ROM_LOAD( "1119",     0x120000, 0x10000, 0x6f8003a1 ) /* MO */
	ROM_LOAD( "1121",     0x130000, 0x10000, 0x8ff0a1ec ) /* MO */

	ROM_LOAD( "1125",     0x140000, 0x10000, 0x3aa7c756 ) /* MO */
	ROM_LOAD( "1126",     0x150000, 0x10000, 0xcb82c9aa ) /* MO */
	ROM_LOAD( "1128",     0x160000, 0x10000, 0xdce32863 ) /* MO */
	/* 170000-18ffff empty */

	ROM_REGION( 0x0a0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2129",     0x000000, 0x10000, 0x36b1a578 ) /* playfield */
	ROM_LOAD( "2131",     0x010000, 0x10000, 0x7b7c04a1 ) /* playfield */
	ROM_LOAD( "2133",     0x020000, 0x10000, 0xe03fe4d9 ) /* playfield */
	ROM_LOAD( "2135",     0x030000, 0x10000, 0x7d497110 ) /* playfield */
	ROM_LOAD( "2137",     0x040000, 0x10000, 0xf91e7872 ) /* playfield */
	ROM_LOAD( "2130",     0x050000, 0x10000, 0xb25368cc ) /* playfield */
	ROM_LOAD( "2132",     0x060000, 0x10000, 0x112f2d20 ) /* playfield */
	ROM_LOAD( "2134",     0x070000, 0x10000, 0x84884ed6 ) /* playfield */
	ROM_LOAD( "2136",     0x080000, 0x10000, 0xbc028690 ) /* playfield */
	ROM_LOAD( "2138",     0x090000, 0x10000, 0x60cec955 ) /* playfield */

	ROM_REGION( 0x008000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2141",     0x000000, 0x08000, 0x60d6d6df ) /* alphanumerics */

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* 256k for ADPCM samples */
	ROM_LOAD( "1145",      0x00000, 0x10000, 0xd9475d58 )
	ROM_LOAD( "1146",      0x10000, 0x10000, 0x133e6aef )
	ROM_LOAD( "1147",      0x20000, 0x10000, 0xba4d556e )
	ROM_LOAD( "1148",      0x30000, 0x10000, 0xc48df49a )
ROM_END



/*************************************
 *
 *	ROM decoding
 *
 *************************************/

static void rom_decode(void)
{
	int i;
	for (i = 0x170000; i < 0x190000; i++)
		memory_region(REGION_GFX1)[i] = 0;
	for (i = 0; i < memory_region_length(REGION_GFX2); i++)
		memory_region(REGION_GFX2)[i] ^= 0xff;
}



static void init_skullxbo(void)
{
	atarigen_eeprom_default = NULL;

	atarijsa_init(1, 2, 1, 0x0080);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4159, 0x4171);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}



GAME( 1989, skullxbo, 0,        skullxbo, skullxbo, skullxbo, ROT0, "Atari Games", "Skull & Crossbones (set 1)" )
GAME( 1989, skullxb2, skullxbo, skullxbo, skullxbo, skullxbo, ROT0, "Atari Games", "Skull & Crossbones (set 2)" )
