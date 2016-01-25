#include "../vidhrdw/hydra.cpp"

/***************************************************************************

	Hydra/Pit Fighter

****************************************************************************/


#include "driver.h"
#include "sound/adpcm.h"
#include "machine/atarigen.h"
#include "sndhrdw/atarijsa.h"
#include "vidhrdw/generic.h"


/* Note: if this is set to 1, it must also be set in the vidhrdw module */
#define HIGH_RES 0


extern struct rectangle hydra_mo_area;
extern UINT32 hydra_mo_priority_offset;
extern INT32 hydra_pf_xoffset;


static UINT8 which_input;


WRITE_HANDLER( hydra_playfieldram_w );

int hydra_vh_start(void);
void hydra_vh_stop(void);
void hydra_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

void hydra_scanline_update(int param);
WRITE_HANDLER( hydra_mo_control_w );



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_video_int_state)
		newstate = 1;
	if (atarigen_sound_int_state)
		newstate = 2;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_slapstic_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(hydra_scanline_update, 8);
	atarijsa_reset();
}



/*************************************
 *
 *	I/O read dispatch.
 *
 *************************************/

static READ_HANDLER( special_port0_r )
{
	int temp = input_port_0_r(offset);
	if (atarigen_cpu_to_sound_ready) temp ^= 0x1000;
	temp ^= 0x2000;		/* A2DOK always high for now */
	return temp;
}


static WRITE_HANDLER( a2d_select_w )
{
	(void)data;
	which_input = (offset / 2);
}


static READ_HANDLER( a2d_data_r )
{
	/* Pit Fighter has no A2D, just another input port */
	if (hydra_mo_area.min_x != 0)
		return input_port_1_r(offset);

	/* otherwise, assume it's hydra */
	if (which_input < 3)
		return readinputport(1 + which_input) << 8;

	return 0;
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0xfc0000, 0xfc0001, special_port0_r },
	{ 0xfc8000, 0xfc8001, a2d_data_r },
	{ 0xfd0000, 0xfd0001, atarigen_sound_upper_r },
	{ 0xfd8000, 0xfdffff, atarigen_eeprom_r },
/*	{ 0xfe0000, 0xfe7fff, from_r },*/
	{ 0xfe8000, 0xfe89ff, MRA_BANK1 },
	{ 0xff0000, 0xff3fff, MRA_BANK2 },
	{ 0xff4000, 0xff5fff, MRA_BANK3 },
	{ 0xff6000, 0xff6fff, MRA_BANK4 },
	{ 0xff7000, 0xffffff, MRA_BANK5 },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0xf80000, 0xf80001, watchdog_reset_w },
	{ 0xf88000, 0xf8ffff, atarigen_eeprom_enable_w },
	{ 0xf90000, 0xf90001, atarigen_sound_upper_w },
	{ 0xf98000, 0xf98001, atarigen_sound_reset_w },
	{ 0xfa0000, 0xfa0001, hydra_mo_control_w },
	{ 0xfb0000, 0xfb0001, atarigen_video_int_ack_w },
	{ 0xfc8000, 0xfc8007, a2d_select_w },
	{ 0xfd8000, 0xfdffff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0xfe8000, 0xfe89ff, atarigen_666_paletteram_w, &paletteram },
	{ 0xff0000, 0xff3fff, MWA_BANK2, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0xff4000, 0xff5fff, hydra_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0xff6000, 0xff6fff, MWA_BANK4, &atarigen_alpharam, &atarigen_alpharam_size },
	{ 0xff7000, 0xffffff, MWA_BANK5 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( hydra )
	PORT_START		/* fc0000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON5 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON6 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x0fe0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START		/* ADC 0 @ fc8000 */
	PORT_ANALOG( 0x00ff, 0x0080, IPT_AD_STICK_X, 50, 10, 0, 255 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START		/* ADC 1 @ fc8000 */
	PORT_ANALOG( 0x00ff, 0x0080, IPT_AD_STICK_Y, 70, 10, 0, 255 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* ADC 2 @ fc8000 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL, 100, 16, 0x00, 0xff )

	JSA_II_PORT		/* audio board port */
INPUT_PORTS_END


INPUT_PORTS_START( pitfight )
	PORT_START		/* fc0000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0f80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START      /* fc8000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER3 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER3 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* not used */
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* not used */
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	JSA_II_PORT		/* audio board port */
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout pflayout =
{
	8 << HIGH_RES,8,	/* 8*8 chars */
	16384,	/* 16384 chars */
	5,		/* 5 bits per pixel */
	{ 0, 0, 1, 2, 3 },
#if HIGH_RES
	{ 0x40000*8+0,0x40000*8+0, 0x40000*8+4,0x40000*8+4, 0,0, 4,4,
		0x40000*8+8,0x40000*8+8, 0x40000*8+12,0x40000*8+12, 8,8, 12,12 },
#else
	{ 0x40000*8+0, 0x40000*8+4, 0, 4, 0x40000*8+8, 0x40000*8+12, 8, 12 },
#endif
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout anlayout =
{
	8 << HIGH_RES,8,	/* 8*8 chars */
	4096,	/* 4096 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
#if HIGH_RES
	{ 0,0, 4,4, 8,8, 12,12, 16,16, 20,20, 24,24, 28,28 },
#else
	{ 0, 4, 8, 12, 16, 20, 24, 28 },
#endif
	{ 0*8, 4*8, 8*8, 12*8, 16*8, 20*8, 24*8, 28*8 },
	32*8	/* every char takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pflayout, 0x300, 8 },
	{ REGION_GFX2, 0, &anlayout, 0x100, 16 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_hydra =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz,
			main_readmem,main_writemem,0,0,
			atarigen_video_int_gen,1
		},
		JSA_II_CPU
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	init_machine,

	/* video hardware */
	42*(8 << HIGH_RES), 30*8, { 0*(8 << HIGH_RES), 42*(8 << HIGH_RES)-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	1024,1024,
	0,

#if HIGH_RES
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_PIXEL_ASPECT_RATIO_1_2,
#else
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
#endif
	0,
	hydra_vh_start,
	hydra_vh_stop,
	hydra_vh_screenrefresh,

	/* sound hardware */
	JSA_II_MONO(REGION_SOUND1),

	atarigen_nvram_handler
};



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_hydra(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x078000, 116);

	hydra_mo_area.min_x = 0 << HIGH_RES;
	hydra_mo_area.max_x = 255 << HIGH_RES;
	hydra_mo_area.min_y = 0;
	hydra_mo_area.max_y = 239;
	hydra_mo_priority_offset = 10;
	hydra_pf_xoffset = 0;

	atarijsa_init(1, 4, 0, 0x8000);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4159, 0x4171);

	/* display messages */
	atarigen_show_slapstic_message();
	atarigen_show_sound_message();
}


static void init_hydrap(void)
{
	atarigen_eeprom_default = NULL;

	hydra_mo_area.min_x = 0 << HIGH_RES;
	hydra_mo_area.max_x = 255 << HIGH_RES;
	hydra_mo_area.min_y = 0;
	hydra_mo_area.max_y = 239;
	hydra_mo_priority_offset = 10;
	hydra_pf_xoffset = 0;

	atarijsa_init(1, 4, 0, 0x8000);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4159, 0x4171);

	/* display messages */
	atarigen_show_sound_message();
}


static void init_pitfight(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x038000, 111);

	hydra_mo_area.min_x = 40 << HIGH_RES;
	hydra_mo_area.max_x = (40 + 255) << HIGH_RES;
	hydra_mo_area.min_y = 0;
	hydra_mo_area.max_y = 239;
	hydra_mo_priority_offset = 12;
	hydra_pf_xoffset = 2;

	atarijsa_init(1, 4, 0, 0x8000);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4159, 0x4171);

	/* display messages */
	atarigen_show_slapstic_message();
	atarigen_show_sound_message();
}



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( hydra )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "hydr3028.bin", 0x00000, 0x10000, 0x43475f73 )
	ROM_LOAD_ODD ( "hydr3029.bin", 0x00000, 0x10000, 0x886e1de8 )
	ROM_LOAD_EVEN( "hydr3034.bin", 0x20000, 0x10000, 0x5115aa36 )
	ROM_LOAD_ODD ( "hydr3035.bin", 0x20000, 0x10000, 0xa28ba44b )
	ROM_LOAD_EVEN( "hydr1032.bin", 0x40000, 0x10000, 0xecd1152a )
	ROM_LOAD_ODD ( "hydr1033.bin", 0x40000, 0x10000, 0x2ebe1939 )
	ROM_LOAD_EVEN( "hydr1030.bin", 0x60000, 0x10000, 0xb31fd41f )
	ROM_LOAD_ODD ( "hydr1031.bin", 0x60000, 0x10000, 0x453d076f )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "hydraa0.bin", 0x10000, 0x4000, 0x619d7319 )
	ROM_CONTINUE(            0x04000, 0xc000 )

	ROM_REGION( 0x0a0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hydr1017.bin",  0x000000, 0x10000, 0xbd77b747 ) /* playfield, planes 0-3 odd */
	ROM_LOAD( "hydr1018.bin",  0x010000, 0x10000, 0x7c24e637 )
	ROM_LOAD( "hydr1019.bin",  0x020000, 0x10000, 0xaa2fb07b )
	ROM_LOAD( "hydr1020.bin",  0x030000, 0x10000, 0x906ccd98 )
	ROM_LOAD( "hydr1021.bin",  0x040000, 0x10000, 0xf88cdac2 ) /* playfield, planes 0-3 even */
	ROM_LOAD( "hydr1022.bin",  0x050000, 0x10000, 0xa9c612ff )
	ROM_LOAD( "hydr1023.bin",  0x060000, 0x10000, 0xb706aa6e )
	ROM_LOAD( "hydr1024.bin",  0x070000, 0x10000, 0xc49eac53 )
	ROM_LOAD( "hydr1025.bin",  0x080000, 0x10000, 0x98b5b1a1 ) /* playfield plane 4 */
	ROM_LOAD( "hydr1026.bin",  0x090000, 0x10000, 0xd68d44aa )

	ROM_REGION( 0x020000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hydr1027.bin",  0x000000, 0x20000, 0xf9135b9b ) /* alphanumerics */

	ROM_REGION( 0x100000, REGION_GFX3 )
	ROM_LOAD_ODD ( "hydr1001.bin", 0x00000, 0x10000, 0x3f757a53 )
	ROM_LOAD_EVEN( "hydr1002.bin", 0x00000, 0x10000, 0xa1169469 )
	ROM_LOAD_ODD ( "hydr1003.bin", 0x20000, 0x10000, 0xaa21ec33 )
	ROM_LOAD_EVEN( "hydr1004.bin", 0x20000, 0x10000, 0xc0a2be66 )
	ROM_LOAD_ODD ( "hydr1005.bin", 0x40000, 0x10000, 0x80c285b3 )
	ROM_LOAD_EVEN( "hydr1006.bin", 0x40000, 0x10000, 0xad831c59 )
	ROM_LOAD_ODD ( "hydr1007.bin", 0x60000, 0x10000, 0xe0688cc0 )
	ROM_LOAD_EVEN( "hydr1008.bin", 0x60000, 0x10000, 0xe6827f6b )
	ROM_LOAD_ODD ( "hydr1009.bin", 0x80000, 0x10000, 0x33624d07 )
	ROM_LOAD_EVEN( "hydr1010.bin", 0x80000, 0x10000, 0x9de4c689 )
	ROM_LOAD_ODD ( "hydr1011.bin", 0xa0000, 0x10000, 0xd55c6e49 )
	ROM_LOAD_EVEN( "hydr1012.bin", 0xa0000, 0x10000, 0x43af45d0 )
	ROM_LOAD_ODD ( "hydr1013.bin", 0xc0000, 0x10000, 0x2647a82b )
	ROM_LOAD_EVEN( "hydr1014.bin", 0xc0000, 0x10000, 0x8897d5e9 )
	ROM_LOAD_ODD ( "hydr1015.bin", 0xe0000, 0x10000, 0xcf7f69fd )
	ROM_LOAD_EVEN( "hydr1016.bin", 0xe0000, 0x10000, 0x61aaf14f )

	ROM_REGION( 0x30000, REGION_SOUND1 )	/* 192k for ADPCM samples */
	ROM_LOAD( "hydr1037.bin",  0x00000, 0x10000, 0xb974d3d0 )
	ROM_LOAD( "hydr1038.bin",  0x10000, 0x10000, 0xa2eda15b )
	ROM_LOAD( "hydr1039.bin",  0x20000, 0x10000, 0xeb9eaeb7 )
ROM_END

ROM_START( hydrap )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "hydhi0.bin", 0x00000, 0x10000, 0xdab2e8a2 )
	ROM_LOAD_ODD ( "hydlo0.bin", 0x00000, 0x10000, 0xc18d4f16 )
	ROM_LOAD_EVEN( "hydhi1.bin", 0x20000, 0x10000, 0x50c12bb9 )
	ROM_LOAD_ODD ( "hydlo1.bin", 0x20000, 0x10000, 0x5ee0a846 )
	ROM_LOAD_EVEN( "hydhi2.bin", 0x40000, 0x10000, 0x436a6d81 )
	ROM_LOAD_ODD ( "hydlo2.bin", 0x40000, 0x10000, 0x182bfd6a )
	ROM_LOAD_EVEN( "hydhi3.bin", 0x60000, 0x10000, 0x29e9e03e )
	ROM_LOAD_ODD ( "hydlo3.bin", 0x60000, 0x10000, 0x7b5047f0 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "hydraa0.bin", 0x10000, 0x4000, BADCRC(0x619d7319) )
	ROM_CONTINUE(            0x04000, 0xc000 )

	ROM_REGION( 0x0a0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hydr1017.bin",  0x000000, 0x10000, 0xbd77b747 ) /* playfield, planes 0-3 odd */
	ROM_LOAD( "hydr1018.bin",  0x010000, 0x10000, 0x7c24e637 )
	ROM_LOAD( "hydr1019.bin",  0x020000, 0x10000, 0xaa2fb07b )
	ROM_LOAD( "hydpl03.bin",   0x030000, 0x10000, 0x1f0dfe60 )
	ROM_LOAD( "hydr1021.bin",  0x040000, 0x10000, 0xf88cdac2 ) /* playfield, planes 0-3 even */
	ROM_LOAD( "hydr1022.bin",  0x050000, 0x10000, 0xa9c612ff )
	ROM_LOAD( "hydr1023.bin",  0x060000, 0x10000, 0xb706aa6e )
	ROM_LOAD( "hydphi3.bin",   0x070000, 0x10000, 0x917e250c )
	ROM_LOAD( "hydr1025.bin",  0x080000, 0x10000, 0x98b5b1a1 ) /* playfield plane 4 */
	ROM_LOAD( "hydpl41.bin",   0x090000, 0x10000, 0x85f9afa6 )

	ROM_REGION( 0x020000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hydalph.bin",   0x000000, 0x20000, 0x7dd2b062 ) /* alphanumerics */

	ROM_REGION( 0x100000, REGION_GFX3 )
	ROM_LOAD_ODD ( "hydmhi0.bin", 0x00000, 0x10000, 0x3c83b42d )
	ROM_LOAD_EVEN( "hydmlo0.bin", 0x00000, 0x10000, 0x6d49650c )
	ROM_LOAD_ODD ( "hydmhi1.bin", 0x20000, 0x10000, 0x689b3376 )
	ROM_LOAD_EVEN( "hydmlo1.bin", 0x20000, 0x10000, 0xc81a4e88 )
	ROM_LOAD_ODD ( "hydmhi2.bin", 0x40000, 0x10000, 0x77098e14 )
	ROM_LOAD_EVEN( "hydmlo2.bin", 0x40000, 0x10000, 0x40015d9d )
	ROM_LOAD_ODD ( "hydmhi3.bin", 0x60000, 0x10000, 0xdfebdcbd )
	ROM_LOAD_EVEN( "hydmlo3.bin", 0x60000, 0x10000, 0x213c407c )
	ROM_LOAD_ODD ( "hydmhi4.bin", 0x80000, 0x10000, 0x2897765f )
	ROM_LOAD_EVEN( "hydmlo4.bin", 0x80000, 0x10000, 0x730157f3 )
	ROM_LOAD_ODD ( "hydmhi5.bin", 0xa0000, 0x10000, 0xecd061ae )
	ROM_LOAD_EVEN( "hydmlo5.bin", 0xa0000, 0x10000, 0xa5a08c53 )
	ROM_LOAD_ODD ( "hydmhi6.bin", 0xc0000, 0x10000, 0xaa3f2903 )
	ROM_LOAD_EVEN( "hydmlo6.bin", 0xc0000, 0x10000, 0xdb8ea56f )
	ROM_LOAD_ODD ( "hydmhi7.bin", 0xe0000, 0x10000, 0x71fc3e43 )
	ROM_LOAD_EVEN( "hydmlo7.bin", 0xe0000, 0x10000, 0x7960b0c2 )

	ROM_REGION( 0x30000, REGION_SOUND1 )	/* 192k for ADPCM samples */
	ROM_LOAD( "hydr1037.bin",  0x00000, 0x10000, BADCRC(0xb974d3d0) )
	ROM_LOAD( "hydr1038.bin",  0x10000, 0x10000, BADCRC(0xa2eda15b) )
	ROM_LOAD( "hydr1039.bin",  0x20000, 0x10000, BADCRC(0xeb9eaeb7) )
ROM_END

ROM_START( pitfight )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "4028", 0x00000, 0x10000, 0xf7cb1a4b )
	ROM_LOAD_ODD ( "4029", 0x00000, 0x10000, 0x13ae0d4f )
	ROM_LOAD_EVEN( "3030", 0x20000, 0x10000, 0xb053e779 )
	ROM_LOAD_ODD ( "3031", 0x20000, 0x10000, 0x2b8c4d13 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "1060", 0x10000, 0x4000, 0x231d71d7 )
	ROM_CONTINUE(     0x04000, 0xc000 )

	ROM_REGION( 0x0a0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1017",  0x000000, 0x10000, 0xad3cfea5 ) /* playfield, planes 0-3 odd */
	ROM_LOAD( "1018",  0x010000, 0x10000, 0x1a0f8bcf )
	ROM_LOAD( "1021",  0x040000, 0x10000, 0x777efee3 ) /* playfield, planes 0-3 even */
	ROM_LOAD( "1022",  0x050000, 0x10000, 0x524319d0 )
	ROM_LOAD( "1025",  0x080000, 0x10000, 0xfc41691a ) /* playfield plane 4 */

	ROM_REGION( 0x020000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1027",  0x000000, 0x10000, 0xa59f381d ) /* alphanumerics */

	ROM_REGION( 0x200000, REGION_GFX3 )
	ROM_LOAD_ODD ( "1001", 0x000000, 0x20000, 0x3af31444 )
	ROM_LOAD_EVEN( "1002", 0x000000, 0x20000, 0xf1d76a4c )
	ROM_LOAD_ODD ( "1003", 0x040000, 0x20000, 0x28c41c2a )
	ROM_LOAD_EVEN( "1004", 0x040000, 0x20000, 0x977744da )
	ROM_LOAD_ODD ( "1005", 0x080000, 0x20000, 0xae59aef2 )
	ROM_LOAD_EVEN( "1006", 0x080000, 0x20000, 0xb6ccd77e )
	ROM_LOAD_ODD ( "1007", 0x0c0000, 0x20000, 0xba33b0c0 )
	ROM_LOAD_EVEN( "1008", 0x0c0000, 0x20000, 0x09bd047c )
	ROM_LOAD_ODD ( "1009", 0x100000, 0x20000, 0xab85b00b )
	ROM_LOAD_EVEN( "1010", 0x100000, 0x20000, 0xeca94bdc )
	ROM_LOAD_ODD ( "1011", 0x140000, 0x20000, 0xa86582fd )
	ROM_LOAD_EVEN( "1012", 0x140000, 0x20000, 0xefd1152d )
	ROM_LOAD_ODD ( "1013", 0x180000, 0x20000, 0xa141379e )
	ROM_LOAD_EVEN( "1014", 0x180000, 0x20000, 0x93bfcc15 )
	ROM_LOAD_ODD ( "1015", 0x1c0000, 0x20000, 0x9378ad0b )
	ROM_LOAD_EVEN( "1016", 0x1c0000, 0x20000, 0x19c3fbe0 )

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* 256k for ADPCM samples */
	ROM_LOAD( "1061",  0x00000, 0x10000, 0x5b0468c6 )
	ROM_LOAD( "1062",  0x10000, 0x10000, 0xf73fe3cb )
	ROM_LOAD( "1063",  0x20000, 0x10000, 0xaa93421d )
	ROM_LOAD( "1064",  0x30000, 0x10000, 0x33f045d5 )
ROM_END

ROM_START( pitfigh3 )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "3028", 0x00000, 0x10000, 0x99530da4 )
	ROM_LOAD_ODD ( "3029", 0x00000, 0x10000, 0x78c7afbf )
	ROM_LOAD_EVEN( "3030", 0x20000, 0x10000, 0xb053e779 )
	ROM_LOAD_ODD ( "3031", 0x20000, 0x10000, 0x2b8c4d13 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "1060", 0x10000, 0x4000, 0x231d71d7 )
	ROM_CONTINUE(     0x04000, 0xc000 )

	ROM_REGION( 0x0a0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1017",  0x000000, 0x10000, 0xad3cfea5 ) /* playfield, planes 0-3 odd */
	ROM_LOAD( "1018",  0x010000, 0x10000, 0x1a0f8bcf )
	ROM_LOAD( "1021",  0x040000, 0x10000, 0x777efee3 ) /* playfield, planes 0-3 even */
	ROM_LOAD( "1022",  0x050000, 0x10000, 0x524319d0 )
	ROM_LOAD( "1025",  0x080000, 0x10000, 0xfc41691a ) /* playfield plane 4 */

	ROM_REGION( 0x020000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1027",  0x000000, 0x10000, 0xa59f381d ) /* alphanumerics */

	ROM_REGION( 0x200000, REGION_GFX3 )
	ROM_LOAD_ODD ( "1001", 0x000000, 0x20000, 0x3af31444 )
	ROM_LOAD_EVEN( "1002", 0x000000, 0x20000, 0xf1d76a4c )
	ROM_LOAD_ODD ( "1003", 0x040000, 0x20000, 0x28c41c2a )
	ROM_LOAD_EVEN( "1004", 0x040000, 0x20000, 0x977744da )
	ROM_LOAD_ODD ( "1005", 0x080000, 0x20000, 0xae59aef2 )
	ROM_LOAD_EVEN( "1006", 0x080000, 0x20000, 0xb6ccd77e )
	ROM_LOAD_ODD ( "1007", 0x0c0000, 0x20000, 0xba33b0c0 )
	ROM_LOAD_EVEN( "1008", 0x0c0000, 0x20000, 0x09bd047c )
	ROM_LOAD_ODD ( "1009", 0x100000, 0x20000, 0xab85b00b )
	ROM_LOAD_EVEN( "1010", 0x100000, 0x20000, 0xeca94bdc )
	ROM_LOAD_ODD ( "1011", 0x140000, 0x20000, 0xa86582fd )
	ROM_LOAD_EVEN( "1012", 0x140000, 0x20000, 0xefd1152d )
	ROM_LOAD_ODD ( "1013", 0x180000, 0x20000, 0xa141379e )
	ROM_LOAD_EVEN( "1014", 0x180000, 0x20000, 0x93bfcc15 )
	ROM_LOAD_ODD ( "1015", 0x1c0000, 0x20000, 0x9378ad0b )
	ROM_LOAD_EVEN( "1016", 0x1c0000, 0x20000, 0x19c3fbe0 )

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* 256k for ADPCM samples */
	ROM_LOAD( "1061",  0x00000, 0x10000, 0x5b0468c6 )
	ROM_LOAD( "1062",  0x10000, 0x10000, 0xf73fe3cb )
	ROM_LOAD( "1063",  0x20000, 0x10000, 0xaa93421d )
	ROM_LOAD( "1064",  0x30000, 0x10000, 0x33f045d5 )
ROM_END



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1990, hydra,    0,        hydra, hydra,    hydra,    ROT0, "Atari Games", "Hydra" )
GAME( 1990, hydrap,   hydra,    hydra, hydra,    hydrap,   ROT0, "Atari Games", "Hydra (prototype)" )
GAME( 1990, pitfight, 0,        hydra, pitfight, pitfight, ROT0, "Atari Games", "Pit Fighter (version 4)" )
GAME( 1990, pitfigh3, pitfight, hydra, pitfight, pitfight, ROT0, "Atari Games", "Pit Fighter (version 3)" )
