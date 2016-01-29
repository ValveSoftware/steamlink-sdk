#include "../vidhrdw/vendetta.cpp"

/***************************************************************************

Vendetta (GX081) (c) 1991 Konami

Preliminary driver by:
Ernesto Corvi
someone@secureshell.com

Notes:
- collision detection is handled by a protection chip. Its emulation might
  not be 100% accurate.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "machine/eeprom.h"

/* prototypes */
static void vendetta_init_machine( void );
static void vendetta_banking( int lines );
static void vendetta_video_banking( int select );

int vendetta_vh_start(void);
void vendetta_vh_stop(void);
void vendetta_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


/***************************************************************************

  EEPROM

***************************************************************************/

static int init_eeprom_count;


static struct EEPROM_interface eeprom_interface =
{
	7,				/* address bits */
	8,				/* data bits */
	"011000",		/*  read command */
	"011100",		/* write command */
	0,				/* erase command */
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
			init_eeprom_count = 1000;
	}
}

static READ_HANDLER( vendetta_eeprom_r )
{
	int res;

	res = EEPROM_read_bit();

	res |= 0x02;//konami_eeprom_ack() << 5; /* add the ack */

	res |= readinputport( 3 ) & 0x0c; /* test switch */

	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xfb;
	}
	return res;
}

static int irq_enabled;

static WRITE_HANDLER( vendetta_eeprom_w )
{
	/* bit 0 - VOC0 - Video banking related */
	/* bit 1 - VOC1 - Video banking related */
	/* bit 2 - MSCHNG - Mono Sound select (Amp) */
	/* bit 3 - EEPCS - Eeprom CS */
	/* bit 4 - EEPCLK - Eeprom CLK */
	/* bit 5 - EEPDI - Eeprom data */
	/* bit 6 - IRQ enable */
	/* bit 7 - Unused */

	if ( data == 0xff ) /* this is a bug in the eeprom write code */
		return;

	/* EEPROM */
	EEPROM_write_bit(data & 0x20);
	EEPROM_set_clock_line((data & 0x10) ? ASSERT_LINE : CLEAR_LINE);
	EEPROM_set_cs_line((data & 0x08) ? CLEAR_LINE : ASSERT_LINE);

	irq_enabled = ( data >> 6 ) & 1;

	vendetta_video_banking( data & 1 );
}

/********************************************/

static READ_HANDLER( vendetta_K052109_r ) { return K052109_r( offset + 0x2000 ); }
static WRITE_HANDLER( vendetta_K052109_w ) { K052109_w( offset + 0x2000, data ); }

static void vendetta_video_banking( int select )
{
	if ( select & 1 )
	{
		cpu_setbankhandler_r( 2, paletteram_r );
		cpu_setbankhandler_w( 2, paletteram_xBBBBBGGGGGRRRRR_swap_w );
		cpu_setbankhandler_r( 3, K053247_r );
		cpu_setbankhandler_w( 3, K053247_w );
	}
	else
	{
		cpu_setbankhandler_r( 2, vendetta_K052109_r );
		cpu_setbankhandler_w( 2, vendetta_K052109_w );
		cpu_setbankhandler_r( 3, K052109_r );
		cpu_setbankhandler_w( 3, K052109_w );
	}
}

static WRITE_HANDLER( vendetta_5fe0_w )
{
//char baf[40];
//sprintf(baf,"5fe0 = %02x",data);
//usrintf_showmessage(baf);

	/* bit 0,1 coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bit 2 = BRAMBK ?? */

	/* bit 3 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);

	/* bit 4 = INIT ?? */

	/* bit 5 = enable sprite ROM reading */
	K053246_set_OBJCHA_line((data & 0x20) ? ASSERT_LINE : CLEAR_LINE);
}

static READ_HANDLER( speedup_r )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	int data = ( RAM[0x28d2] << 8 ) | RAM[0x28d3];

	if ( data < memory_region_length(REGION_CPU1) )
	{
		data = ( RAM[data] << 8 ) | RAM[data + 1];

		if ( data == 0xffff )
			cpu_spinuntil_int();
	}

	return RAM[0x28d2];
}

static void z80_nmi_callback( int param )
{
	cpu_set_nmi_line( 1, ASSERT_LINE );
}

static WRITE_HANDLER( z80_arm_nmi_w )
{
	cpu_set_nmi_line( 1, CLEAR_LINE );

	timer_set( TIME_IN_USEC( 50 ), 0, z80_nmi_callback );
}

static WRITE_HANDLER( z80_irq_w )
{
	cpu_cause_interrupt( 1, 0xff );
}

READ_HANDLER( vendetta_sound_interrupt_r )
{
	cpu_cause_interrupt( 1, 0xff );
	return 0x00;
}

READ_HANDLER( vendetta_sound_r )
{
	/* If the sound CPU is running, read the status, otherwise
	   just make it pass the test */
	if (Machine->sample_rate != 0) 	return K053260_r(2 + offset);
	else
	{
		static int res = 0x00;

		res = ((res + 1) & 0x07);
		return offset ? res : 0x00;
	}
}

/********************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x1fff, MRA_BANK1	},
	{ 0x28d2, 0x28d2, speedup_r },
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x5f80, 0x5f9f, K054000_r },
	{ 0x5fc0, 0x5fc0, input_port_0_r },
	{ 0x5fc1, 0x5fc1, input_port_1_r },
	{ 0x5fd0, 0x5fd0, vendetta_eeprom_r }, /* vblank, service */
	{ 0x5fd1, 0x5fd1, input_port_2_r },
	{ 0x5fe4, 0x5fe4, vendetta_sound_interrupt_r },
	{ 0x5fe6, 0x5fe7, vendetta_sound_r },
	{ 0x5fe8, 0x5fe9, K053246_r },
	{ 0x5fea, 0x5fea, watchdog_reset_r },
	{ 0x4000, 0x4fff, MRA_BANK3 },
	{ 0x6000, 0x6fff, MRA_BANK2 },
	{ 0x4000, 0x7fff, K052109_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x3fff, MWA_RAM },
	{ 0x5f80, 0x5f9f, K054000_w },
	{ 0x5fa0, 0x5faf, K053251_w },
	{ 0x5fb0, 0x5fb7, K053246_w },
	{ 0x5fe0, 0x5fe0, vendetta_5fe0_w },
	{ 0x5fe2, 0x5fe2, vendetta_eeprom_w },
	{ 0x5fe4, 0x5fe4, z80_irq_w },
	{ 0x5fe6, 0x5fe7, K053260_w },
	{ 0x4000, 0x4fff, MWA_BANK3 },
	{ 0x6000, 0x6fff, MWA_BANK2 },
	{ 0x4000, 0x7fff, K052109_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf801, 0xf801, YM2151_status_port_0_r },
	{ 0xfc00, 0xfc2f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf800, YM2151_register_port_0_w },
	{ 0xf801, 0xf801, YM2151_data_port_0_w },
	{ 0xfa00, 0xfa00, z80_arm_nmi_w },
	{ 0xfc00, 0xfc2f, K053260_w },
	{ -1 }	/* end of table */
};


/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( vendetta )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM ready */
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK ) /* not really vblank, object related. Its timed, otherwise sprites flicker */
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END



/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(35,MIXER_PAN_LEFT,35,MIXER_PAN_RIGHT) },
	{ 0 }
};

static struct K053260_interface k053260_interface =
{
	3579545,
	REGION_SOUND1, /* memory region */
	{ MIXER(75,MIXER_PAN_LEFT), MIXER(75,MIXER_PAN_RIGHT) },
	0
};

static int vendetta_irq( void )
{
	if (irq_enabled)
		return KONAMI_INT_IRQ;
	else
		return ignore_interrupt();
}

static struct MachineDriver machine_driver_vendetta =
{
	/* basic machine hardware */
	{
		{
			CPU_KONAMI,
			3000000,		/* ? */
			readmem,writemem,0,0,
			vendetta_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,
			readmem_sound, writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	vendetta_init_machine,

	/* video hardware */
	64*8, 32*8, { 13*8, (64-13)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	vendetta_vh_start,
	vendetta_vh_stop,
	vendetta_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface
		}
	},

	nvram_handler
};

/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( vendetta )
	ROM_REGION( 0x49000, REGION_CPU1 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081u01", 0x10000, 0x38000, 0xb4d9ade5 )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, 0x4c604d9b )

	ROM_REGION( 0x100000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, 0xb4c777a9 ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, 0x272ac8d9 ) /* characters */

	ROM_REGION( 0x400000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, 0x464b9aa4 ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, 0x4e173759 ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, 0xe9fe6d80 ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, 0x8a22b29a ) /* sprites */

	ROM_REGION( 0x100000, REGION_SOUND1 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, 0x14b6baea )
ROM_END

ROM_START( vendett2 )
	ROM_REGION( 0x49000, REGION_CPU1 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081d01", 0x10000, 0x38000, 0x335da495 )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, 0x4c604d9b )

	ROM_REGION( 0x100000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, 0xb4c777a9 ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, 0x272ac8d9 ) /* characters */

	ROM_REGION( 0x400000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, 0x464b9aa4 ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, 0x4e173759 ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, 0xe9fe6d80 ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, 0x8a22b29a ) /* sprites */

	ROM_REGION( 0x100000, REGION_SOUND1 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, 0x14b6baea )
ROM_END

ROM_START( vendettj )
	ROM_REGION( 0x49000, REGION_CPU1 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081p01", 0x10000, 0x38000, 0x5fe30242 )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, 0x4c604d9b )

	ROM_REGION( 0x100000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, 0xb4c777a9 ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, 0x272ac8d9 ) /* characters */

	ROM_REGION( 0x400000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, 0x464b9aa4 ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, 0x4e173759 ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, 0xe9fe6d80 ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, 0x8a22b29a ) /* sprites */

	ROM_REGION( 0x100000, REGION_SOUND1 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, 0x14b6baea )
ROM_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

static void vendetta_banking( int lines )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	if ( lines >= 0x1c )
	{
		//logerror("PC = %04x : Unknown bank selected %02x\n", cpu_get_pc(), lines );
	}
	else
		cpu_setbank( 1, &RAM[ 0x10000 + ( lines * 0x2000 ) ] );
}

static void vendetta_init_machine( void )
{
	konami_cpu_setlines_callback = vendetta_banking;

	paletteram = &memory_region(REGION_CPU1)[0x48000];
	irq_enabled = 0;

	/* init banks */
	cpu_setbank( 1, &memory_region(REGION_CPU1)[0x10000] );
	vendetta_video_banking( 0 );
}

static void init_vendetta(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_4(REGION_GFX2);
}



GAME( 1991, vendetta, 0,        vendetta, vendetta, vendetta, ROT0, "Konami", "Vendetta (Asia set 1)" )
GAME( 1991, vendett2, vendetta, vendetta, vendetta, vendetta, ROT0, "Konami", "Vendetta (Asia set 2)" )
GAME( 1991, vendettj, vendetta, vendetta, vendetta, vendetta, ROT0, "Konami", "Crime Fighters 2 (Japan)" )
