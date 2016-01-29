#include "../vidhrdw/chqflag.cpp"

/***************************************************************************

Chequered Flag / Checkered Flag (GX717) (c) Konami 1988

Notes:
	* Some background tiles are missing. Hopefully this is because of the bad ROMS.
	* The enemies appear and dissapear randomly because of the K051733 protection.
	* The sound is not working properly.

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "cpu/konami/konami.h"
#include "vidhrdw/konamiic.h"

static int K051316_readroms;

static WRITE_HANDLER( k007232_extvolume_w );

/* from vidhrdw/chqflag.c */
int chqflag_vh_start( void );
void chqflag_vh_stop( void );
void chqflag_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh );

static int chqflag_interrupt( void )
{
	if (cpu_getiloops() == 0){
		if (K051960_is_IRQ_enabled()) return KONAMI_INT_IRQ;
	}
	else if (cpu_getiloops() % 2){
		if (K051960_is_NMI_enabled()) return nmi_interrupt();
	}
	return ignore_interrupt();
}

static WRITE_HANDLER( chqflag_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* bits 0-4 = ROM bank # (0x00-0x11) */
	bankaddress = 0x10000 + (data & 0x1f)*0x4000;
	cpu_setbank(4,&RAM[bankaddress]);

	/* bit 5 = memory bank select */
	if (data & 0x20){
		cpu_setbankhandler_r (2, paletteram_r);							/* palette */
		cpu_setbankhandler_w (2, paletteram_xBBBBBGGGGGRRRRR_swap_w);	/* palette */
		if (K051316_readroms){
			cpu_setbankhandler_r (1, K051316_rom_0_r);	/* 051316 #1 (ROM test) */
			cpu_setbankhandler_w (1, K051316_0_w);		/* 051316 #1 */
		}
		else{
			cpu_setbankhandler_r (1, K051316_0_r);		/* 051316 #1 */
			cpu_setbankhandler_w (1, K051316_0_w);		/* 051316 #1 */
		}
	}
	else{
		cpu_setbankhandler_r (1, MRA_RAM);				/* RAM */
		cpu_setbankhandler_w (1, MWA_RAM);				/* RAM */
		cpu_setbankhandler_r (2, MRA_RAM);				/* RAM */
		cpu_setbankhandler_w (2, MWA_RAM);				/* RAM */
	}

	/* other bits unknown/unused */
}

static WRITE_HANDLER( chqflag_vreg_w )
{
	/* bits 0 & 1 = coin counters */
	coin_counter_w(1,data & 0x01);
	coin_counter_w(0,data & 0x02);

	/* bit 4 = enable rom reading thru K051316 #1 & #2 */
	if ((K051316_readroms = (data & 0x10))){
		cpu_setbankhandler_r (3, K051316_rom_1_r);	/* 051316 (ROM test) */
	}
	else{
		cpu_setbankhandler_r (3, K051316_1_r);		/* 051316 */
	}

	/* other bits unknown/unused */
}

static int analog_ctrl;

static WRITE_HANDLER( select_analog_ctrl_w )
{
	analog_ctrl = data;
}

static READ_HANDLER( analog_read_r )
{
	static int accel, wheel;

	switch (analog_ctrl & 0x03){
		case 0x00: return (accel = readinputport(5));	/* accelerator */
		case 0x01: return (wheel = readinputport(6));	/* steering */
		case 0x02: return accel;						/* accelerator (previous?) */
		case 0x03: return wheel;						/* steering (previous?) */
	}

	return 0xff;
}

WRITE_HANDLER( chqflag_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,Z80_IRQ_INT);
}


/****************************************************************************/

static struct MemoryReadAddress chqflag_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },					/* RAM */
	{ 0x1000, 0x17ff, MRA_BANK1 },					/* banked RAM (RAM/051316 (chip 1)) */
	{ 0x1800, 0x1fff, MRA_BANK2 },					/* palette + RAM */
	{ 0x2000, 0x2007, K051937_r },					/* Sprite control registers */
	{ 0x2400, 0x27ff, K051960_r },					/* Sprite RAM */
	{ 0x2800, 0x2fff, MRA_BANK3 },					/* 051316 zoom/rotation (chip 2) */
	{ 0x3100, 0x3100, input_port_0_r },				/* DIPSW #1  */
	{ 0x3200, 0x3200, input_port_3_r },				/* COINSW, STARTSW, test mode */
	{ 0x3201, 0x3201, input_port_2_r },				/* DIPSW #3, SW 4 */
	{ 0x3203, 0x3203, input_port_1_r },				/* DIPSW #2 */
	{ 0x3400, 0x341f, K051733_r },					/* 051733 (protection) */
	{ 0x3701, 0x3701, input_port_4_r },				/* Brake + Shift + ? */
	{ 0x3702, 0x3702, analog_read_r },				/* accelerator/wheel */
	{ 0x4000, 0x7fff, MRA_BANK4 },					/* banked ROM */
	{ 0x8000, 0xffff, MRA_ROM },					/* ROM */
	{ -1 }
};

static struct MemoryWriteAddress chqflag_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },					/* RAM */
	{ 0x1000, 0x17ff, MWA_BANK1 },					/* banked RAM (RAM/051316 (chip 1)) */
	{ 0x1800, 0x1fff, MWA_BANK2 },					/* palette + RAM */
	{ 0x2000, 0x2007, K051937_w },					/* Sprite control registers */
	{ 0x2400, 0x27ff, K051960_w },					/* Sprite RAM */
	{ 0x2800, 0x2fff, K051316_1_w },				/* 051316 zoom/rotation (chip 2) */
	{ 0x3000, 0x3000, soundlatch_w },				/* sound code # */
	{ 0x3001, 0x3001, chqflag_sh_irqtrigger_w },	/* cause interrupt on audio CPU */
	{ 0x3002, 0x3002, chqflag_bankswitch_w },		/* bankswitch control */
	{ 0x3003, 0x3003, chqflag_vreg_w },				/* enable K051316 ROM reading */
	{ 0x3300, 0x3300, watchdog_reset_w },			/* watchdog timer */
	{ 0x3400, 0x341f, K051733_w },					/* 051733 (protection) */
	{ 0x3500, 0x350f, K051316_ctrl_0_w },			/* 051316 control registers (chip 1) */
	{ 0x3600, 0x360f, K051316_ctrl_1_w },			/* 051316 control registers (chip 2) */
	{ 0x3700, 0x3700, select_analog_ctrl_w },		/* select accelerator/wheel */
	{ 0x3702, 0x3702, select_analog_ctrl_w },		/* select accelerator/wheel (mirror?) */
	{ 0x4000, 0x7fff, MWA_ROM },					/* banked ROM */
	{ 0x8000, 0xffff, MWA_ROM },					/* ROM */
	{ -1 }
};

static struct MemoryReadAddress chqflag_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },				/* ROM */
	{ 0x8000, 0x87ff, MRA_RAM },				/* RAM */
	{ 0xa000, 0xa00d, K007232_read_port_0_r },	/* 007232 (chip 1) */
	{ 0xb000, 0xb00d, K007232_read_port_1_r },	/* 007232 (chip 2) */
	{ 0xc001, 0xc001, YM2151_status_port_0_r },	/* YM2151 */
	{ 0xd000, 0xd000, soundlatch_r },			/* soundlatch_r */
	//{ 0xe000, 0xe000, MRA_NOP },				/* ??? */
	{ -1 }
};

static WRITE_HANDLER( k007232_bankswitch_w )
{
	unsigned char *RAM;
	int bank_A, bank_B;

	/* banks # for the 007232 (chip 1) */
	RAM = memory_region(REGION_SOUND1);
	bank_A = 0x20000*((data >> 4) & 0x03);
	bank_B = 0x20000*((data >> 6) & 0x03);
	K007232_bankswitch(0,RAM + bank_A,RAM + bank_B);

	/* banks # for the 007232 (chip 2) */
	RAM = memory_region(REGION_SOUND2);
	bank_A = 0x20000*((data >> 0) & 0x03);
	bank_B = 0x20000*((data >> 2) & 0x03);
	K007232_bankswitch(1,RAM + bank_A,RAM + bank_B);

}

static struct MemoryWriteAddress chqflag_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },					/* ROM */
	{ 0x8000, 0x87ff, MWA_RAM },					/* RAM */
	{ 0x9000, 0x9000, k007232_bankswitch_w },		/* 007232 bankswitch */
	{ 0xa000, 0xa00d, K007232_write_port_0_w },		/* 007232 (chip 1) */
	{ 0xa01c, 0xa01c, k007232_extvolume_w },/* extra volume, goes to the 007232 w/ A11 */
											/* selecting a different latch for the external port */
	{ 0xb000, 0xb00d, K007232_write_port_1_w },		/* 007232 (chip 2) */
	{ 0xc000, 0xc000, YM2151_register_port_0_w },	/* YM2151 */
	{ 0xc001, 0xc001, YM2151_data_port_0_w },		/* YM2151 */
	{ 0xf000, 0xf000, MWA_NOP },					/* ??? */
	{ -1 }
};


INPUT_PORTS_START( chqflag )
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
//	PORT_DIPSETTING(    0x00, "Coin Slot 2 Invalidity" )

	PORT_START	/* DSW #2 (according to the manual SW1 thru SW5 are not used) */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x60, "Easy" )
	PORT_DIPSETTING(	0x40, "Normal" )
	PORT_DIPSETTING(	0x20, "Difficult" )
	PORT_DIPSETTING(	0x00, "Very difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )	/* DIPSW #3 - SW4 */
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START
	/* COINSW + STARTSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	/* DIPSW #3 */
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Title" )
	PORT_DIPSETTING(	0x40, "Chequered Flag" )
	PORT_DIPSETTING(	0x00, "Checkered Flag" )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Brake, Shift + ??? */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_TOGGLE | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_TOGGLE | IPF_PLAYER1 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* if this is set, it goes directly to test mode */
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* if bit 7 == 0, the game resets */

	PORT_START	/* Accelerator */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 50, 5, 0, 0xff)

	PORT_START	/* Driving wheel */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_CENTER, 80, 8, 0, 0xff)

INPUT_PORTS_END

static void chqflag_ym2151_irq_w(int data)
{
	cpu_cause_interrupt(1,Z80_NMI_INT);
}


static struct YM2151interface ym2151_interface =
{
	1,
	3579545,	/* 3.579545 MHz? */
	{ YM3012_VOL(80,MIXER_PAN_LEFT,80,MIXER_PAN_RIGHT) },
	{ chqflag_ym2151_irq_w },
	{ 0 }
};

static void volume_callback0(int v)
{
	K007232_set_volume(0,0,(v >> 4)*0x11,0);
	K007232_set_volume(0,1,0,(v & 0x0f)*0x11);
}

static WRITE_HANDLER( k007232_extvolume_w )
{
	K007232_set_volume(0,0,(data >> 4)*0x11/2,(data & 0x0f)*0x11/2);
}

static void volume_callback1(int v)
{
	//K007232_set_volume(1,0,(v & 0x0f)*0x11/2,(v & 0x0f)*0x11/2);
	K007232_set_volume(1,1,(v & 0x0f)*0x11/2,(v >> 4)*0x11/2);
}

static struct K007232_interface k007232_interface =
{
	2,															/* number of chips */
	{ REGION_SOUND1, REGION_SOUND2 },							/* memory regions */
	{ K007232_VOL(20,MIXER_PAN_CENTER,20,MIXER_PAN_CENTER),		/* volume */
		K007232_VOL(20,MIXER_PAN_LEFT,20,MIXER_PAN_RIGHT) },
	{ volume_callback0,  volume_callback1 }						/* external port callback */
};

static struct MachineDriver machine_driver_chqflag =
{
	{
		{
			CPU_KONAMI,	/* 052001 */
			3000000,	/* ? */
			chqflag_readmem,chqflag_writemem,0,0,
			chqflag_interrupt,16	/* ? */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? */
			chqflag_readmem_sound, chqflag_writemem_sound,0,0,
			ignore_interrupt,0		/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	64*8, 32*8, { 11*8, (63-14)*8, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	1024, 1024,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	chqflag_vh_start,
	chqflag_vh_stop,
	chqflag_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
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

ROM_START( chqflag )
	ROM_REGION( 0x58800, REGION_CPU1 )	/* 052001 code */
	ROM_LOAD( "717h02",		0x050000, 0x008000, 0xf5bd4e78 )	/* banked ROM */
	ROM_CONTINUE(			0x008000, 0x008000 )				/* fixed ROM */
	ROM_LOAD( "717e10",		0x010000, 0x040000, 0x72fc56f6 )	/* banked ROM */
	/* extra memory for banked RAM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the SOUND CPU */
	ROM_LOAD( "717e01",		0x000000, 0x008000, 0x966b8ba8 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "717e04",		0x000000, 0x080000, 0x1a50a1cc )	/* sprites */
	ROM_LOAD( "717e05",		0x080000, 0x080000, 0x46ccb506 )	/* sprites */

	ROM_REGION( 0x020000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "717e06",		0x000000, 0x020000, 0x1ec26c7a )	/* zoom/rotate (N16) */

	ROM_REGION( 0x100000, REGION_GFX3 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "717e07",		0x000000, 0x040000, BADCRC (0xa8d538a8) )	/* BAD DUMP! */
	ROM_LOAD( "717e08",		0x040000, 0x040000, 0xb68a212e )	/* zoom/rotate (L22) */
	ROM_LOAD( "717e11",		0x080000, 0x040000, BADCRC (0x84f8de54) )	/* BAD DUMP! */
	ROM_LOAD( "717e12",		0x0c0000, 0x040000, 0x9269335d )	/* zoom/rotate (N22) */

	ROM_REGION( 0x080000, REGION_SOUND1 )	/* 007232 data (chip 1) */
	ROM_LOAD( "717e03",		0x000000, 0x080000, 0xebe73c22 )

	ROM_REGION( 0x080000, REGION_SOUND2 )	/* 007232 data (chip 2) */
	ROM_LOAD( "717e09",		0x000000, 0x080000, 0xd74e857d )
ROM_END

ROM_START( chqflagj )
	ROM_REGION( 0x58800, REGION_CPU1 )	/* 052001 code */
	ROM_LOAD( "717j02.bin",	0x050000, 0x008000, 0x1 )	/* banked ROM */
	ROM_CONTINUE(			0x008000, 0x008000 )				/* fixed ROM */
	ROM_LOAD( "717e10",		0x010000, 0x040000, 0x72fc56f6 )	/* banked ROM */
	/* extra memory for banked RAM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the SOUND CPU */
	ROM_LOAD( "717e01",		0x000000, 0x008000, 0x966b8ba8 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "717e04",		0x000000, 0x080000, 0x1a50a1cc )	/* sprites */
	ROM_LOAD( "717e05",		0x080000, 0x080000, 0x46ccb506 )	/* sprites */

	ROM_REGION( 0x020000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "717e06",		0x000000, 0x020000, 0x1ec26c7a )	/* zoom/rotate (N16) */

	ROM_REGION( 0x100000, REGION_GFX3 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "717e07",		0x000000, 0x040000, BADCRC (0xa8d538a8) )	/* BAD DUMP! */
	ROM_LOAD( "717e08",		0x040000, 0x040000, 0xb68a212e )	/* zoom/rotate (L22) */
	ROM_LOAD( "717e11",		0x080000, 0x040000, BADCRC (0x84f8de54) )	/* BAD DUMP! */
	ROM_LOAD( "717e12",		0x0c0000, 0x040000, 0x9269335d )	/* zoom/rotate (N22) */

	ROM_REGION( 0x080000, REGION_SOUND1 )	/* 007232 data (chip 1) */
	ROM_LOAD( "717e03",		0x000000, 0x080000, 0xebe73c22 )

	ROM_REGION( 0x080000, REGION_SOUND2 )	/* 007232 data (chip 2) */
	ROM_LOAD( "717e09",		0x000000, 0x080000, 0xd74e857d )
ROM_END



static void init_chqflag(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	konami_rom_deinterleave_2(REGION_GFX1);
	paletteram = &RAM[0x58000];
}

GAMEX( 1988, chqflag,        0, chqflag, chqflag, chqflag, ROT90, "Konami", "Chequered Flag", GAME_NOT_WORKING )
GAMEX( 1988, chqflagj, chqflag, chqflag, chqflag, chqflag, ROT90, "Konami", "Chequered Flag (Japan)", GAME_NOT_WORKING  )
