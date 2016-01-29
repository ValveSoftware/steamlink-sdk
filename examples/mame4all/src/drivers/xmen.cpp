#include "../vidhrdw/xmen.cpp"

/***************************************************************************

X-Men

driver by Nicola Salmoria

***************************************************************************/
#include "driver.h"
#include "vidhrdw/konamiic.h"
#include "machine/eeprom.h"
#include "cpu/z80/z80.h"


int xmen_vh_start(void);
void xmen_vh_stop(void);
void xmen_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static READ_HANDLER( K052109_halfword_r )
{
	return K052109_r(offset >> 1);
}

static WRITE_HANDLER( K052109_halfword_w )
{
	if ((data & 0x00ff0000) == 0)
		K052109_w(offset >> 1,data & 0xff);
}

static WRITE_HANDLER( K053251_halfword_w )
{
	if ((data & 0x00ff0000) == 0)
		K053251_w(offset >> 1,data & 0xff);
}



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
			init_eeprom_count = 10;
	}
}

static READ_HANDLER( eeprom_r )
{
	int res;

//logerror("%06x eeprom_r\n",cpu_get_pc());
	/* bit 6 is EEPROM data */
	/* bit 7 is EEPROM ready */
	/* bit 14 is service button */
	res = (EEPROM_read_bit() << 6) | input_port_2_r(0);
	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xbfff;
	}
	return res;
}

static WRITE_HANDLER( eeprom_w )
{
//logerror("%06x: write %04x to 108000\n",cpu_get_pc(),data);
	if ((data & 0x00ff0000) == 0)
	{
		/* bit 0 = coin counter */
		coin_counter_w(0,data & 0x01);

		/* bit 2 is data */
		/* bit 3 is clock (active high) */
		/* bit 4 is cs (active low) */
		EEPROM_write_bit(data & 0x04);
		EEPROM_set_cs_line((data & 0x10) ? CLEAR_LINE : ASSERT_LINE);
		EEPROM_set_clock_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);
	}
	else
	{
		/* bit 8 = enable sprite ROM reading */
		K053246_set_OBJCHA_line((data & 0x0100) ? ASSERT_LINE : CLEAR_LINE);
		/* bit 9 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x0200) ? ASSERT_LINE : CLEAR_LINE);
	}
}

static READ_HANDLER( sound_status_r )
{
	return soundlatch2_r(0);
}

static WRITE_HANDLER( sound_cmd_w )
{
	if (offset == 0)
	{
		data &= 0xff;
		soundlatch_w(0, data);
		if(!Machine->sample_rate)
			if(data == 0xfc || data == 0xfe)
				soundlatch2_w(0, 0x7f);
	}
}

static WRITE_HANDLER( sound_irq_w )
{
	cpu_set_irq_line(1, Z80_IRQ_INT, HOLD_LINE);
}

static WRITE_HANDLER( xmen_18fa00_w )
{
	if (offset == 0)
	{
		/* bit 2 is interrupt enable */
		interrupt_enable_w(0,data & 0x04);
	}
}

static WRITE_HANDLER( sound_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU2);


	bankaddress = 0x10000 + (data & 0x07) * 0x4000;
	cpu_setbank(4,&RAM[bankaddress]);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x080000, 0x0fffff, MRA_ROM },
	{ 0x100000, 0x100fff, K053247_word_r },
	{ 0x101000, 0x101fff, MRA_BANK2 },
	{ 0x104000, 0x104fff, paletteram_word_r },
	{ 0x108054, 0x108055, sound_status_r },
	{ 0x10a000, 0x10a001, input_port_0_r },
	{ 0x10a002, 0x10a003, input_port_1_r },
	{ 0x10a004, 0x10a005, eeprom_r },
	{ 0x10a00c, 0x10a00d, K053246_word_r },
	{ 0x110000, 0x113fff, MRA_BANK1 },	/* main RAM */
	{ 0x18c000, 0x197fff, K052109_halfword_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x080000, 0x0fffff, MWA_ROM },
	{ 0x100000, 0x100fff, K053247_word_w },
	{ 0x101000, 0x101fff, MWA_BANK2 },
	{ 0x104000, 0x104fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x108000, 0x108001, eeprom_w },
	{ 0x108020, 0x108027, K053246_word_w },
	{ 0x10804c, 0x10804d, sound_cmd_w },
	{ 0x10804e, 0x10804f, sound_irq_w },
	{ 0x108060, 0x10807f, K053251_halfword_w },
	{ 0x10a000, 0x10a001, watchdog_reset_w },
	{ 0x110000, 0x113fff, MWA_BANK1 },	/* main RAM */
	{ 0x18fa00, 0x18fa01, xmen_18fa00_w },
	{ 0x18c000, 0x197fff, K052109_halfword_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK4 },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe22f, K054539_0_r },
	{ 0xec01, 0xec01, YM2151_status_port_0_r },
	{ 0xf002, 0xf002, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe22f, K054539_0_w },
	{ 0xe800, 0xe800, YM2151_register_port_0_w },
	{ 0xec01, 0xec01, YM2151_data_port_0_w },
	{ 0xf000, 0xf000, soundlatch2_w },
	{ 0xf800, 0xf800, sound_bankswitch_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( xmen )
	PORT_START	/* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN4 )

	PORT_START	/* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START	/* COIN  EEPROM and service */
	PORT_BIT( 0x003f, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status - always 1 */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x3000, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
INPUT_PORTS_END

INPUT_PORTS_START( xmen2p )
	PORT_START	/* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN2 )
/*
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN4 )
*/

	PORT_START	/* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN1 )
/*
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN3 )
*/

	PORT_START	/* COIN  EEPROM and service */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x003c, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status - always 1 */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x3000, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
INPUT_PORTS_END



static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	4000000,	/* 4 MHz */
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) },
	{ 0 }
};

static struct K054539interface k054539_interface =
{
	1,			/* 1 chip */
	48000,
	{ REGION_SOUND1 },
	{ { 100, 100 } },
	{ 0 }		/* The YM does not seem to be connected to the 539 analog input */
};



static int xmen_interrupt(void)
{
	if (cpu_getiloops() == 0) return m68_level5_irq();
	else return m68_level3_irq();
}

static struct MachineDriver machine_driver_xmen =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* ? */
			readmem,writemem,0,0,
			xmen_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2*3579545,	/* ????? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	xmen_vh_start,
	xmen_vh_stop,
	xmen_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K054539,
			&k054539_interface
		}
	},
	nvram_handler
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( xmen )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "065ubb04.10d",  0x00000, 0x20000, 0xf896c93b )
	ROM_LOAD_ODD ( "065ubb05.10f",  0x00000, 0x20000, 0xe02e5d64 )
	ROM_LOAD_EVEN( "xmen17g.bin",   0x80000, 0x40000, 0xb31dc44c )
	ROM_LOAD_ODD ( "xmen17j.bin",   0x80000, 0x40000, 0x13842fe6 )

	ROM_REGION( 0x30000, REGION_CPU2 )		/* 64k+128k fpr sound cpu */
	ROM_LOAD( "065-a01.6f",   0x00000, 0x20000, 0x147d3a4d )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x200000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "xmen1l.bin",   0x000000, 0x100000, 0x6b649aca )	/* tiles */
	ROM_LOAD( "xmen1h.bin",   0x100000, 0x100000, 0xc5dc8fc4 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "xmen12l.bin",  0x000000, 0x100000, 0xea05d52f )	/* sprites */
	ROM_LOAD( "xmen17l.bin",  0x100000, 0x100000, 0x96b91802 )
	ROM_LOAD( "xmen22h.bin",  0x200000, 0x100000, 0x321ed07a )
	ROM_LOAD( "xmen22l.bin",  0x300000, 0x100000, 0x46da948e )

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* samples for the 054544 */
	ROM_LOAD( "xmenc25.bin",  0x000000, 0x200000, 0x5adbcee0 )
ROM_END

ROM_START( xmen6p )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "xmenb04.bin",   0x00000, 0x20000, 0x0f09b8e0 )
	ROM_LOAD_ODD ( "xmenb05.bin",   0x00000, 0x20000, 0x867becbf )
	ROM_LOAD_EVEN( "xmen17g.bin",   0x80000, 0x40000, 0xb31dc44c )
	ROM_LOAD_ODD ( "xmen17j.bin",   0x80000, 0x40000, 0x13842fe6 )

	ROM_REGION( 0x30000, REGION_CPU2 )		/* 64k+128k fpr sound cpu */
	ROM_LOAD( "065-a01.6f",   0x00000, 0x20000, 0x147d3a4d )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x200000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "xmen1l.bin",   0x000000, 0x100000, 0x6b649aca )	/* tiles */
	ROM_LOAD( "xmen1h.bin",   0x100000, 0x100000, 0xc5dc8fc4 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "xmen12l.bin",  0x000000, 0x100000, 0xea05d52f )	/* sprites */
	ROM_LOAD( "xmen17l.bin",  0x100000, 0x100000, 0x96b91802 )
	ROM_LOAD( "xmen22h.bin",  0x200000, 0x100000, 0x321ed07a )
	ROM_LOAD( "xmen22l.bin",  0x300000, 0x100000, 0x46da948e )

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* samples for the 054544 */
	ROM_LOAD( "xmenc25.bin",  0x000000, 0x200000, 0x5adbcee0 )
ROM_END

ROM_START( xmen2pj )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "065jaa04.10d",  0x00000, 0x20000, 0x66746339 )
	ROM_LOAD_ODD ( "065jaa05.10f",  0x00000, 0x20000, 0x1215b706 )
	ROM_LOAD_EVEN( "xmen17g.bin",   0x80000, 0x40000, 0xb31dc44c )
	ROM_LOAD_ODD ( "xmen17j.bin",   0x80000, 0x40000, 0x13842fe6 )

	ROM_REGION( 0x30000, REGION_CPU2 )		/* 64k+128k fpr sound cpu */
	ROM_LOAD( "065-a01.6f",   0x00000, 0x20000, 0x147d3a4d )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x200000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "xmen1l.bin",   0x000000, 0x100000, 0x6b649aca )	/* tiles */
	ROM_LOAD( "xmen1h.bin",   0x100000, 0x100000, 0xc5dc8fc4 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "xmen12l.bin",  0x000000, 0x100000, 0xea05d52f )	/* sprites */
	ROM_LOAD( "xmen17l.bin",  0x100000, 0x100000, 0x96b91802 )
	ROM_LOAD( "xmen22h.bin",  0x200000, 0x100000, 0x321ed07a )
	ROM_LOAD( "xmen22l.bin",  0x300000, 0x100000, 0x46da948e )

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* samples for the 054544 */
	ROM_LOAD( "xmenc25.bin",  0x000000, 0x200000, 0x5adbcee0 )
ROM_END



static void init_xmen(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_4(REGION_GFX2);
}

static void init_xmen6p(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	WRITE_WORD(&RAM[0x21a6],0x4e71);
	WRITE_WORD(&RAM[0x21a8],0x4e71);
	WRITE_WORD(&RAM[0x21aa],0x4e71);

	init_xmen();
}



GAME( 1992, xmen,    0,    xmen, xmen,   xmen,   ROT0, "Konami", "X-Men (4 Players)")
GAMEX( 1992, xmen6p,  xmen, xmen, xmen,   xmen6p, ROT0, "Konami", "X-Men (6 Players)", GAME_IMPERFECT_SOUND | GAME_NOT_WORKING )
GAME( 1992, xmen2pj, xmen, xmen, xmen2p, xmen,   ROT0, "Konami", "X-Men (2 Players Japan)")
