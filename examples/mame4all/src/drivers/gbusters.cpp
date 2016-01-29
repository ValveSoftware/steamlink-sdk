#include "../vidhrdw/gbusters.cpp"

/***************************************************************************

Gangbusters(GX878) (c) 1988 Konami

Preliminary driver by:
	Manuel Abadia <manu@teleline.es>

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "vidhrdw/konamiic.h"

/* prototypes */
static void gbusters_init_machine( void );
static void gbusters_banking( int lines );


extern int gbusters_priority;

void gbusters_vh_stop( void );
int gbusters_vh_start( void );
void gbusters_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static int palette_selected;
static unsigned char *ram;

static int gbusters_interrupt( void )
{
	if (K052109_is_IRQ_enabled())
		return KONAMI_INT_IRQ;
	else
		return ignore_interrupt();
}

static READ_HANDLER( bankedram_r )
{
	if (palette_selected)
		return paletteram_r(offset);
	else
		return ram[offset];
}

static WRITE_HANDLER( bankedram_w )
{
	if (palette_selected)
		paletteram_xBBBBBGGGGGRRRRR_swap_w(offset,data);
	else
		ram[offset] = data;
}

static WRITE_HANDLER( gbusters_1f98_w )
{

	/* bit 0 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line((data & 0x01) ? ASSERT_LINE : CLEAR_LINE);

	/* bit 7 used (during gfx rom tests), but unknown */

	/* other bits unused/unknown */
	//if (data & 0xfe){
		//logerror("%04x: (1f98) write %02x\n",cpu_get_pc(), data);
		//usrintf_showmessage("$1f98 = %02x", data);
	//}
}

static WRITE_HANDLER( gbusters_coin_counter_w )
{
	/* bit 0 select palette RAM  or work RAM at 5800-5fff */
	palette_selected = ~data & 0x01;

	/* bits 1 & 2 = coin counters */
	coin_counter_w(0,data & 0x02);
	coin_counter_w(1,data & 0x04);

	/* bits 3 selects tilemap priority */
	gbusters_priority = data & 0x08;

	/* bit 7 is used but unknown */

	/* other bits unused/unknown */
	//if (data & 0xf8)
	//{
	//	char baf[40];
	//	logerror("%04x: (ccount) write %02x\n",cpu_get_pc(), data);
	//	sprintf(baf,"ccnt = %02x", data);
//		usrintf_showmessage(baf);
	//}
}

static WRITE_HANDLER( gbusters_unknown_w )
{
	//logerror("%04x: ??? write %02x\n",cpu_get_pc(), data);

//{
//char baf[40];
//	sprintf(baf,"??? = %02x", data);
//	usrintf_showmessage(baf);
//}
}

WRITE_HANDLER( gbusters_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,0xff);
}

static WRITE_HANDLER( gbusters_snd_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_SOUND1);

	int bank_B = 0x20000*((data >> 2) & 0x01);	/* ?? */
	int bank_A = 0x20000*((data) & 0x01);		/* ?? */

	K007232_bankswitch(0,RAM + bank_A,RAM + bank_B);

#if 0
	{
		char baf[40];
		sprintf(baf,"snd_bankswitch = %02x", data);
		usrintf_showmessage(baf);
	}
#endif
}

static struct MemoryReadAddress gbusters_readmem[] =
{
	{ 0x1f90, 0x1f90, input_port_3_r },		/* coinsw & startsw */
	{ 0x1f91, 0x1f91, input_port_4_r },		/* Player 1 inputs */
	{ 0x1f92, 0x1f92, input_port_5_r },		/* Player 2 inputs */
	{ 0x1f93, 0x1f93, input_port_2_r },		/* DIPSW #3 */
	{ 0x1f94, 0x1f94, input_port_0_r },		/* DIPSW #1 */
	{ 0x1f95, 0x1f95, input_port_1_r },		/* DIPSW #2 */
	{ 0x0000, 0x3fff, K052109_051960_r },	/* tiles + sprites (RAM H21, G21 & H6) */
	{ 0x4000, 0x57ff, MRA_RAM },			/* RAM I12 */
	{ 0x5800, 0x5fff, bankedram_r },		/* palette + work RAM (RAM D16 & C16) */
	{ 0x6000, 0x7fff, MRA_BANK1 },			/* banked ROM */
	{ 0x8000, 0xffff, MRA_ROM },			/* ROM 878n02.rom */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress gbusters_writemem[] =
{
	{ 0x1f80, 0x1f80, gbusters_coin_counter_w },	/* coin counters */
	{ 0x1f84, 0x1f84, soundlatch_w },				/* sound code # */
	{ 0x1f88, 0x1f88, gbusters_sh_irqtrigger_w },	/* cause interrupt on audio CPU */
	{ 0x1f8c, 0x1f8c, watchdog_reset_w },			/* watchdog reset */
	{ 0x1f98, 0x1f98, gbusters_1f98_w },			/* enable gfx ROM read through VRAM */
	{ 0x1f9c, 0x1f9c, gbusters_unknown_w },			/* ??? */
	{ 0x0000, 0x3fff, K052109_051960_w },			/* tiles + sprites (RAM H21, G21 & H6) */
	{ 0x4000, 0x57ff, MWA_RAM },					/* RAM I12 */
	{ 0x5800, 0x5fff, bankedram_w, &ram },			/* palette + work RAM (RAM D16 & C16) */
	{ 0x6000, 0x7fff, MWA_ROM },					/* banked ROM */
	{ 0x8000, 0xffff, MWA_ROM },					/* ROM 878n02.rom */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress gbusters_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },				/* ROM 878h01.rom */
	{ 0x8000, 0x87ff, MRA_RAM },				/* RAM */
	{ 0xa000, 0xa000, soundlatch_r },			/* soundlatch_r */
	{ 0xb000, 0xb00d, K007232_read_port_0_r },	/* 007232 registers */
	{ 0xc001, 0xc001, YM2151_status_port_0_r },	/* YM 2151 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress gbusters_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },					/* ROM 878h01.rom */
	{ 0x8000, 0x87ff, MWA_RAM },					/* RAM */
	{ 0xb000, 0xb00d, K007232_write_port_0_w },		/* 007232 registers */
	{ 0xc000, 0xc000, YM2151_register_port_0_w },	/* YM 2151 */
	{ 0xc001, 0xc001, YM2151_data_port_0_w },		/* YM 2151 */
	{ 0xf000, 0xf000, gbusters_snd_bankswitch_w },	/* 007232 bankswitch? */
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( gbusters )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x04, "Bullets" )
	PORT_DIPSETTING(    0x04, "50" )
	PORT_DIPSETTING(    0x00, "60" )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "50k, 200k & 400k" )
	PORT_DIPSETTING(    0x10, "70k, 250k & 500k" )
	PORT_DIPSETTING(    0x08, "50k" )
	PORT_DIPSETTING(    0x00, "70k" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW #3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

INPUT_PORTS_END


/***************************************************************************

	Machine Driver

***************************************************************************/

static void volume_callback(int v)
{
	K007232_set_volume(0,0,(v >> 4) * 0x11,0);
	K007232_set_volume(0,1,0,(v & 0x0f) * 0x11);
}

static struct K007232_interface k007232_interface =
{
	1,		/* number of chips */
	{ REGION_SOUND1 },	/* memory regions */
	{ K007232_VOL(30,MIXER_PAN_CENTER,30,MIXER_PAN_CENTER) },	/* volume */
	{ volume_callback }	/* external port callback */
};

static struct YM2151interface ym2151_interface =
{
	1, /* 1 chip */
	3579545, /* 3.579545 MHz */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ 0 },
	{ 0 }
};

static struct MachineDriver machine_driver_gbusters =
{
	/* basic machine hardware */
	{
		{
			CPU_KONAMI,		/* Konami custom 052526 */
			3000000,		/* ? */
			gbusters_readmem,gbusters_writemem,0,0,
            gbusters_interrupt,1
        },
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,		/* ? */
			gbusters_readmem_sound, gbusters_writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	gbusters_init_machine,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	gbusters_vh_start,
	gbusters_vh_stop,
	gbusters_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K007232,
			&k007232_interface
		}
	}
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( gbusters )
	ROM_REGION( 0x30800, REGION_CPU1 ) /* code + banked roms + space for banked RAM */
	ROM_LOAD( "878n02.rom", 0x10000, 0x08000, 0x51697aaa )	/* ROM K13 */
	ROM_CONTINUE(           0x08000, 0x08000 )
	ROM_LOAD( "878j03.rom", 0x20000, 0x10000, 0x3943a065 )	/* ROM K15 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "878h01.rom", 0x00000, 0x08000, 0x96feafaa )

	ROM_REGION( 0x80000, REGION_GFX1 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD( "878c07.rom", 0x00000, 0x40000, 0xeeed912c )	/* tiles */
	ROM_LOAD( "878c08.rom", 0x40000, 0x40000, 0x4d14626d )	/* tiles */

	ROM_REGION( 0x80000, REGION_GFX2 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD( "878c05.rom", 0x00000, 0x40000, 0x01f4aea5 )	/* sprites */
	ROM_LOAD( "878c06.rom", 0x40000, 0x40000, 0xedfaaaaf )	/* sprites */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "878a09.rom",   0x0000, 0x0100, 0xe2d09a1b )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* samples for 007232 */
	ROM_LOAD( "878c04.rom",  0x00000, 0x40000, 0x9e982d1c )
ROM_END

ROM_START( crazycop )
	ROM_REGION( 0x30800, REGION_CPU1 ) /* code + banked roms + space for banked RAM */
	ROM_LOAD( "878m02.bin", 0x10000, 0x08000, 0x9c1c9f52 )	/* ROM K13 */
	ROM_CONTINUE(           0x08000, 0x08000 )
	ROM_LOAD( "878j03.rom", 0x20000, 0x10000, 0x3943a065 )	/* ROM K15 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "878h01.rom", 0x00000, 0x08000, 0x96feafaa )

	ROM_REGION( 0x80000, REGION_GFX1 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD( "878c07.rom", 0x00000, 0x40000, 0xeeed912c )	/* tiles */
	ROM_LOAD( "878c08.rom", 0x40000, 0x40000, 0x4d14626d )	/* tiles */

	ROM_REGION( 0x80000, REGION_GFX2 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD( "878c05.rom", 0x00000, 0x40000, 0x01f4aea5 )	/* sprites */
	ROM_LOAD( "878c06.rom", 0x40000, 0x40000, 0xedfaaaaf )	/* sprites */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "878a09.rom",   0x0000, 0x0100, 0xe2d09a1b )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* samples for 007232 */
	ROM_LOAD( "878c04.rom",  0x00000, 0x40000, 0x9e982d1c )
ROM_END


static void gbusters_banking( int lines )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs = 0x10000;

	/* bits 0-3 ROM bank */
	offs += (lines & 0x0f)*0x2000;
	cpu_setbank( 1, &RAM[offs] );

	//if (lines & 0xf0){
		//logerror("%04x: (lines) write %02x\n",cpu_get_pc(), lines);
		//usrintf_showmessage("lines = %02x", lines);
	//}

	/* other bits unknown */
}

static void gbusters_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	konami_cpu_setlines_callback = gbusters_banking;

	/* mirror address for banked ROM */
	memcpy(&RAM[0x18000], &RAM[0x10000], 0x08000 );

	paletteram = &RAM[0x30000];
}


static void init_gbusters(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}



GAME( 1988, gbusters, 0,        gbusters, gbusters, gbusters, ROT90, "Konami", "Gang Busters" )
GAME( 1988, crazycop, gbusters, gbusters, gbusters, gbusters, ROT90, "Konami", "Crazy Cop (Japan)" )
