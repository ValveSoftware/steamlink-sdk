/***************************************************************************


This driver is dedicated to my loving wife Natalia Wiebelt
                                      and my daughter Lara Anna Maria
Summer 1997 Bernd Wiebelt

Many thanks to Al Kossow for the original sources and the solid documentation.
Without him, I could never had completed this driver.


--------

Most of the info here comes from the wiretap archive at:
http://www.spies.com/arcade/simulation/gameHardware/


Omega Race Memory Map
Version 1.1 (Jul 24,1997)
---------------------

0000 - 3fff	PROM
4000 - 4bff	RAM (3k)
5c00 - 5cff	NVRAM (256 x 4bits)
8000 - 8fff	Vec RAM (4k)
9000 - 9fff	Vec ROM (4k)

15 14 13 12 11 10
--+--+--+--+--+--
0  0  0  0                       M8 - 2732  (4k)
0  0  0  1                       L8 - 2732
0  0  1  0                       K8 - 2732
0  0  1  1                       J8 - 2732

0  1  -  0  0  0                 RAM (3k)
0  1  -  0  0  1
0  1  -  0  1  0

0  1  -  1  1  1                 4 Bit BB RAM (d0-d3)

1  -  -  0  0                    Vec RAM (4k)
1  -  -  0  1
1  -  -  1  0			 Vec ROM (2k) E1
1  -  -  1  1                    Vec ROM (2k) F1

I/O Ports

8	Start/ (VG start)
9	WDOG/  (Reset watchdog)
A	SEQRES/ (VG stop/reset?)
B	RDSTOP/ d7 = stop (VG running if 0)

10 I	DIP SW C4 (game ship settings)

	6 5  4 3  2 1
                      1st bonus ship at
        | |  | |  0 0  40,000
        | |  | |  0 1  50,000
        | |  | |  1 0  70,000
        | |  | |  1 1 100,000
        | |  | |      2nd and  3rd bonus ships
        | |  0 0      150,000   250,000
        | |  0 1      250,000   500,000
        | |  1 0      500,000   750,000
        | |  1 1      750,000 1,500,000
        | |           ships per credit
        0 0           1 credit = 2 ships / 2 credits = 4 ships
        0 1           1 credit = 2 ships / 2 credits = 5 ships
        1 0           1 credit = 3 ships / 2 credits = 6 ships
        1 1           1 credit = 3 ships / 2 credits = 7 ships

11 I	7 = Test
	6 = P1 Fire
	5 = P1 Thrust
	4 = Tilt

	1 = Coin 2
	0 = Coin 1

12 I	7 = 1P1CR
	6 = 1P2CR

	3 = 2P2CR -+
	2 = 2P1CR  |
	1 = P2Fire |
	0 = P2Thr -+ cocktail only

13 O   7 =
        6 = screen reverse
        5 = 2 player 2 credit start LED
        4 = 2 player 1 credit start LED
        3 = 1 player 1 credit start LED
        2 = 1 player 1 credit start LED
        1 = coin meter 2
        0 = coin meter 1

14 O	sound command (interrupts sound Z80)

15 I	encoder 1 (d7-d2)

	The encoder is a 64 position Grey Code encoder, or a
	pot and A to D converter.

	Unlike the quadrature inputs on Atari and Sega games,
        Omega Race's controller is an absolute angle.

	0x00, 0x04, 0x14, 0x10, 0x18, 0x1c, 0x5c, 0x58,
	0x50, 0x54, 0x44, 0x40, 0x48, 0x4c, 0x6c, 0x68,
	0x60, 0x64, 0x74, 0x70, 0x78, 0x7c, 0xfc, 0xf8,
	0xf0, 0xf4, 0xe4, 0xe0, 0xe8, 0xec, 0xcc, 0xc8,
	0xc0, 0xc4, 0xd4, 0xd0, 0xd8, 0xdc, 0x9c, 0x98,
	0x90, 0x94, 0x84, 0x80, 0x88, 0x8c, 0xac, 0xa8,
	0xa0, 0xa4, 0xb4, 0xb0, 0xb8, 0xbc, 0x3c, 0x38,
	0x30, 0x34, 0x24, 0x20, 0x28, 0x2c, 0x0c, 0x08

16 I	encoder 2 (d5-d0)

	The inputs aren't scrambled as they are on the 1 player
        encoder

17 I	DIP SW C6 (coin/cocktail settings)

        8  7  6 5 4  3 2 1
                             coin switch 1
        |  |  | | |  0 0 0   1 coin  2 credits
        |  |  | | |  0 0 1   1 coin  3 credits
        |  |  | | |  0 1 0   1 coin  5 credits
        |  |  | | |  0 1 1   4 coins 5 credits
        |  |  | | |  1 0 0   3 coins 4 credits
        |  |  | | |  1 0 1   2 coins 3 credits
        |  |  | | |  1 1 0   2 coins 1 credit
        |  |  | | |  1 1 1   1 coin  1 credit
        |  |  | | |
        |  |  | | |          coin switch 2
        |  |  0 0 0          1 coin  2 credits
        |  |  0 0 1          1 coin  3 credits
        |  |  0 1 0          1 coin  5 credits
        |  |  0 1 1          4 coins 5 credits
        |  |  1 0 0          3 coins 4 credits
        |  |  1 0 1          2 coins 3 credits
        |  |  1 1 0          2 coins 1 credit
        |  |  1 1 1          1 coin  1 credit
        |  |
        |  0                 coin play
        |  1                 free play
        |
        0                    normal
        1                    cocktail

display list format: (4 byte opcodes)

+------+------+------+------+------+------+------+------+
|DY07   DY06   DY05   DY04   DY03   DY02   DY01   DY00  | 0
+------+------+------+------+------+------+------+------+
|OPCD3  OPCD2  OPCD1  OPCD0  DY11   DY10   DY09   DY08  | 1 OPCD 1111 = ABBREV/
+------+------+------+------+------+------+------+------+
|DX07   DX06   DX05   DX04   DX03   DX02   DX01   DX00  | 2
+------+------+------+------+------+------+------+------+
|INTEN3 INTEN2 INTEN1 INTEN0 DX11   DX10   DX09   DX08  | 3
+------+------+------+------+------+------+------+------+

    Draw relative vector       0x80      1000YYYY YYYYYYYY IIIIXXXX XXXXXXXX

    Draw relative vector
    and load scale             0x90      1001YYYY YYYYYYYY SSSSXXXX XXXXXXXX

    Beam to absolute
    screen position            0xA0      1010YYYY YYYYYYYY ----XXXX XXXXXXXX

    Halt                       0xB0      1011---- --------

    Jump to subroutine         0xC0      1100AAAA AAAAAAAA

    Return from subroutine     0xD0      1101---- --------

    Jump to new address        0xE0      1110AAAA AAAAAAAA

    Short vector draw          0xF0      1111YYYY IIIIXXXX


Sound Z80 Memory Map

0000 ROM
1000 RAM

15 14 13 12 11 10
            0           2k prom (K5)
            1           2k prom (J5)
         1              1k RAM  (K4,J4)

I/O (write-only)

0,1 			8912 (K3)
2,3			8912 (J3)


I/O (read-only)

0                       input port from main CPU.
                        main CPU writing port generated INT
Sound Commands:

0 - reset sound CPU
***************************************************************************/

#include "driver.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"



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
			memset(nvram,0,nvram_size);
	}
}


static void omegrace_init_machine(void)
{
	/* Omega Race expects the vector processor to be ready. */
	avgdvg_reset_w (0, 0);
}

static READ_HANDLER( omegrace_vg_go_r )
{
	avgdvg_go_w(0,0);
	return 0;
}

static READ_HANDLER( omegrace_watchdog_r )
{
	return 0;
}

static READ_HANDLER( omegrace_vg_status_r )
{
	if (avgdvg_done())
		return 0;
	else
		return 0x80;
}

/*
 * Encoder bit mappings
 * The encoder is a 64 way switch, with the inputs scrambled
 * on the input port (and shifted 2 bits to the left for the
 * 1 player encoder
 *
 * 3 6 5 4 7 2 for encoder 1 (shifted two bits left..)
 *
 *
 * 5 4 3 2 1 0 for encoder 2 (not shifted..)
 */

static unsigned char spinnerTable[64] = {
	0x00, 0x04, 0x14, 0x10, 0x18, 0x1c, 0x5c, 0x58,
	0x50, 0x54, 0x44, 0x40, 0x48, 0x4c, 0x6c, 0x68,
	0x60, 0x64, 0x74, 0x70, 0x78, 0x7c, 0xfc, 0xf8,
	0xf0, 0xf4, 0xe4, 0xe0, 0xe8, 0xec, 0xcc, 0xc8,
	0xc0, 0xc4, 0xd4, 0xd0, 0xd8, 0xdc, 0x9c, 0x98,
	0x90, 0x94, 0x84, 0x80, 0x88, 0x8c, 0xac, 0xa8,
	0xa0, 0xa4, 0xb4, 0xb0, 0xb8, 0xbc, 0x3c, 0x38,
	0x30, 0x34, 0x24, 0x20, 0x28, 0x2c, 0x0c, 0x08 };


READ_HANDLER( omegrace_spinner1_r )
{
	int res;
	res=readinputport(4);

	return (spinnerTable[res&0x3f]);
}

WRITE_HANDLER( omegrace_leds_w )
{
	/* bits 0 and 1 are coin counters */
	coin_counter_w(0,data & 1);
	coin_counter_w(1,data & 2);

	/* bits 2 to 5 are the start leds (4 and 5 cocktail only, not supported) */
	osd_led_w(0,~data >> 2);
	osd_led_w(1,~data >> 3);

	/* bit 6 flips screen (not supported) */
}

WRITE_HANDLER( omegrace_soundlatch_w )
{
	soundlatch_w (offset, data);
	cpu_cause_interrupt (1, 0xff);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x4bff, MRA_RAM },
	{ 0x5c00, 0x5cff, MRA_RAM }, /* NVRAM */
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9000, 0x9fff, MRA_ROM }, /* vector rom */
	{ -1 }	/* end of table */

	/* 9000-9fff is ROM, hopefully there are no writes to it */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM }, /* Omega Race tries to write there! */
	{ 0x4000, 0x4bff, MWA_RAM },
	{ 0x5c00, 0x5cff, MWA_RAM, &nvram, &nvram_size }, /* NVRAM */
	{ 0x8000, 0x8fff, MWA_RAM, &vectorram, &vectorram_size }, /* vector ram */
	{ 0x9000, 0x9fff, MWA_ROM }, /* vector rom */
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_ROM },
	{ 0x1000, 0x13ff, MRA_RAM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_ROM },
	{ 0x1000, 0x13ff, MWA_RAM },
	{ -1 } /* end of table */
};


static struct IOReadPort readport[] =
{
	{ 0x08, 0x08, omegrace_vg_go_r },
	{ 0x09, 0x09, omegrace_watchdog_r },
	{ 0x0b, 0x0b, omegrace_vg_status_r }, /* vg_halt */
	{ 0x10, 0x10, input_port_0_r }, /* DIP SW C4 */
	{ 0x17, 0x17, input_port_1_r }, /* DIP SW C6 */
	{ 0x11, 0x11, input_port_2_r }, /* Player 1 input */
	{ 0x12, 0x12, input_port_3_r }, /* Player 2 input */
	{ 0x15, 0x15, omegrace_spinner1_r }, /* 1st controller */
	{ 0x16, 0x16, input_port_5_r }, /* 2nd controller (cocktail) */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x0a, 0x0a, avgdvg_reset_w },
	{ 0x13, 0x13, omegrace_leds_w }, /* coin counters, leds, flip screen */
	{ 0x14, 0x14, omegrace_soundlatch_w }, /* Sound command */
	{ -1 }	/* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x00, 0x00, soundlatch_r },
	{ -1 }
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x02, 0x02, AY8910_control_port_1_w },
	{ 0x03, 0x03, AY8910_write_port_1_w },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( omegrace )
	PORT_START /* SW0 */
	PORT_DIPNAME( 0x03, 0x03, "1st Bonus Life" )
	PORT_DIPSETTING (   0x00, "40k" )
	PORT_DIPSETTING (   0x01, "50k" )
	PORT_DIPSETTING (   0x02, "70k" )
	PORT_DIPSETTING (   0x03, "100k" )
	PORT_DIPNAME( 0x0c, 0x0c, "2nd & 3rd Bonus Life" )
	PORT_DIPSETTING (   0x00, "150k 250k" )
	PORT_DIPSETTING (   0x04, "250k 500k" )
	PORT_DIPSETTING (   0x08, "500k 750k" )
	PORT_DIPSETTING (   0x0c, "750k 1500k" )
	PORT_DIPNAME( 0x30, 0x30, "Credit(s)/Ships" )
	PORT_DIPSETTING (   0x00, "1C/2S 2C/4S" )
	PORT_DIPSETTING (   0x10, "1C/2S 2C/5S" )
	PORT_DIPSETTING (   0x20, "1C/3S 2C/6S" )
	PORT_DIPSETTING (   0x30, "1C/3S 2C/7S" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x80, DEF_STR( On ) )

	PORT_START /* SW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (   0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x03, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING (   0x04, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING (   0x05, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING (   0x00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING (   0x02, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (   0x30, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x18, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING (   0x20, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING (   0x28, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING (   0x00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING (   0x10, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING (   0x80, DEF_STR( Cocktail ) )

	PORT_START /* IN2 -port 0x11 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START /* IN3 - port 0x12 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_START3 | IPF_COCKTAIL )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_START4 | IPF_COCKTAIL )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* IN4 - port 0x15 - spinner */
	PORT_ANALOG(0x3f, 0x00, IPT_DIAL, 12, 10, 0, 0 )

	PORT_START /* IN5 - port 0x16 - second spinner */
	PORT_ANALOG(0x3f, 0x00, IPT_DIAL | IPF_COCKTAIL, 12, 10, 0, 0 )
INPUT_PORTS_END



static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_omegrace =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3000000,	/* 3.0 MHz */
			readmem,writemem,readport,writeport,
			0,0, /* no vblank interrupt */
			interrupt, 250 /* 250 Hz */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			1500000,	/* 1.5 MHz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			0, 0, /* no vblank interrupt */
			nmi_interrupt, 250 /* 250 Hz */
		}
	},
	40, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1, /* the soundcpu is synchronized by the new timer code */

	omegrace_init_machine,

	/* video hardware */
	400, 300, { 0, 1020, -10, 1010 },
	0,
	256,0,
	avg_init_palette_white,

	VIDEO_TYPE_VECTOR,
	0,
	dvg_start,
	dvg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	},

	nvram_handler
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( omegrace )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "omega.m7",     0x0000, 0x1000, 0x0424d46e )
	ROM_LOAD( "omega.l7",     0x1000, 0x1000, 0xedcd7a7d )
	ROM_LOAD( "omega.k7",     0x2000, 0x1000, 0x6d10f197 )
	ROM_LOAD( "omega.j7",     0x3000, 0x1000, 0x8e8d4b54 )
	ROM_LOAD( "omega.e1",     0x9000, 0x0800, 0x1d0fdf3a )
	ROM_LOAD( "omega.f1",     0x9800, 0x0800, 0xd44c0814 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for audio cpu */
	ROM_LOAD( "sound.k5",     0x0000, 0x0800, 0x7d426017 )
ROM_END



GAME( 1981, omegrace, 0, omegrace, omegrace, 0, ROT0, "Midway", "Omega Race" )

