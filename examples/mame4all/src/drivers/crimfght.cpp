#include "../vidhrdw/crimfght.cpp"

/***************************************************************************

Crime Fighters (Konami GX821) (c) 1989 Konami

Preliminary driver by:
	Manuel Abadia <manu@teleline.es>

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "vidhrdw/konamiic.h"


/* prototypes */
static void crimfght_init_machine( void );
static void crimfght_banking( int lines );

void crimfght_vh_stop( void );
int crimfght_vh_start( void );
void crimfght_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static WRITE_HANDLER( crimfght_coin_w )
{
	coin_counter_w(0,data & 1);
	coin_counter_w(1,data & 2);
}

static WRITE_HANDLER( crimfght_sh_irqtrigger_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,0xff);
}

static WRITE_HANDLER( crimfght_snd_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_SOUND1);
	/* b1: bank for channel A */
	/* b0: bank for channel B */

	int bank_A = 0x20000*((data >> 1) & 0x01);
	int bank_B = 0x20000*((data) & 0x01);

	K007232_bankswitch(0,RAM + bank_A,RAM + bank_B);
}


/********************************************/

static READ_HANDLER( speedup_r )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	int data = ( RAM[0x0414] << 8 ) | RAM[0x0415];

	if ( data < memory_region_length(REGION_CPU1) )
	{
		data = ( RAM[data] << 8 ) | RAM[data + 1];

		if ( data == 0xffff )
			cpu_spinuntil_int();
	}

	return RAM[0x0414];
}

static struct MemoryReadAddress crimfght_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_BANK1 },			/* banked RAM */
	{ 0x0414, 0x0414, speedup_r },
	{ 0x0400, 0x1fff, MRA_RAM },			/* RAM */
	{ 0x3f80, 0x3f80, input_port_7_r },		/* Coinsw */
	{ 0x3f81, 0x3f81, input_port_3_r },		/* 1P controls */
	{ 0x3f82, 0x3f82, input_port_4_r },		/* 2P controls */
	{ 0x3f83, 0x3f83, input_port_1_r },		/* DSW #2 */
	{ 0x3f84, 0x3f84, input_port_2_r },		/* DSW #3 */
	{ 0x3f85, 0x3f85, input_port_5_r },		/* 3P controls */
	{ 0x3f86, 0x3f86, input_port_6_r },		/* 4P controls */
	{ 0x3f87, 0x3f87, input_port_0_r },		/* DSW #1 */
	{ 0x3f88, 0x3f88, watchdog_reset_r },	/* watchdog reset */
	{ 0x2000, 0x5fff, K052109_051960_r },	/* video RAM + sprite RAM */
	{ 0x6000, 0x7fff, MRA_BANK2 },			/* banked ROM */
	{ 0x8000, 0xffff, MRA_ROM },			/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress crimfght_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_BANK1 },					/* banked RAM */
	{ 0x0400, 0x1fff, MWA_RAM },					/* RAM */
	{ 0x3f88, 0x3f88, crimfght_coin_w },			/* coin counters */
	{ 0x3f8c, 0x3f8c, crimfght_sh_irqtrigger_w },	/* cause interrupt on audio CPU? */
	{ 0x2000, 0x5fff, K052109_051960_w },			/* video RAM + sprite RAM */
	{ 0x6000, 0x7fff, MWA_ROM },					/* banked ROM */
	{ 0x8000, 0xffff, MWA_ROM },					/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress crimfght_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },				/* ROM 821l01.h4 */
	{ 0x8000, 0x87ff, MRA_RAM },				/* RAM */
	{ 0xa001, 0xa001, YM2151_status_port_0_r },	/* YM2151 */
	{ 0xc000, 0xc000, soundlatch_r },			/* soundlatch_r */
	{ 0xe000, 0xe00d, K007232_read_port_0_r },	/* 007232 registers */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress crimfght_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },					/* ROM 821l01.h4 */
	{ 0x8000, 0x87ff, MWA_RAM },					/* RAM */
	{ 0xa000, 0xa000, YM2151_register_port_0_w },	/* YM2151 */
	{ 0xa001, 0xa001, YM2151_data_port_0_w },		/* YM2151 */
	{ 0xe000, 0xe00d, K007232_write_port_0_w },		/* 007232 registers */
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( crimfght )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
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
	PORT_DIPSETTING(    0x00, "1 Coin/99 Credits" )
/*	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
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
	PORT_DIPSETTING(    0x00, "Invalid" ) */

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very difficult" )
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
	PORT_SERVICE( 0x04, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PLAYER 3 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PLAYER 4 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE4 )
INPUT_PORTS_END

INPUT_PORTS_START( crimfgtj )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
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
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very difficult" )
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
	PORT_SERVICE( 0x04, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* PLAYER 3 INPUTS */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PLAYER 4 INPUTS */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) },
	{ 0 },
	{ crimfght_snd_bankswitch_w }
};

static void volume_callback(int v)
{
	K007232_set_volume(0,0,(v & 0x0f) * 0x11,0);
	K007232_set_volume(0,1,0,(v >> 4) * 0x11);
}

static struct K007232_interface k007232_interface =
{
	1,		/* number of chips */
	{ REGION_SOUND1 },	/* memory regions */
	{ K007232_VOL(20,MIXER_PAN_CENTER,20,MIXER_PAN_CENTER) },	/* volume */
	{ volume_callback }	/* external port callback */
};



static struct MachineDriver machine_driver_crimfght =
{
	/* basic machine hardware */
	{
		{
			CPU_KONAMI,
			3000000,		/* ? */
			crimfght_readmem,crimfght_writemem,0,0,
            interrupt,1
        },
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,
			crimfght_readmem_sound, crimfght_writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	crimfght_init_machine,

	/* video hardware */
	64*8, 32*8, { 13*8, (64-13)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	crimfght_vh_start,
	crimfght_vh_stop,
	crimfght_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K007232,
			&k007232_interface,
		}

	}
};

/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( crimfght )
	ROM_REGION( 0x28000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "821l02.f24", 0x10000, 0x18000, 0x588e7da6 )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "821l01.h4",  0x0000, 0x8000, 0x0faca89e )

	ROM_REGION( 0x080000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "821k06.k13", 0x000000, 0x040000, 0xa1eadb24 )	/* characters */
	ROM_LOAD( "821k07.k19", 0x040000, 0x040000, 0x060019fa )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "821k04.k2",  0x000000, 0x080000, 0x00e0291b )	/* sprites */
	ROM_LOAD( "821k05.k8",  0x080000, 0x080000, 0xe09ea05d )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "821a08.i15", 0x0000, 0x0100, 0x7da55800 )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* data for the 007232 */
	ROM_LOAD( "821k03.e5",  0x00000, 0x40000, 0xfef8505a )
ROM_END

ROM_START( crimfgtj )
	ROM_REGION( 0x28000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "821p02.bin", 0x10000, 0x18000, 0xf33fa2e1 )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "821l01.h4",  0x0000, 0x8000, 0x0faca89e )

	ROM_REGION( 0x080000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "821k06.k13", 0x000000, 0x040000, 0xa1eadb24 )	/* characters */
	ROM_LOAD( "821k07.k19", 0x040000, 0x040000, 0x060019fa )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "821k04.k2",  0x000000, 0x080000, 0x00e0291b )	/* sprites */
	ROM_LOAD( "821k05.k8",  0x080000, 0x080000, 0xe09ea05d )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "821a08.i15", 0x0000, 0x0100, 0x7da55800 )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* data for the 007232 */
	ROM_LOAD( "821k03.e5",  0x00000, 0x40000, 0xfef8505a )
ROM_END

ROM_START( crimfgt2 )
ROM_REGION( 0x28000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "crimefb.r02", 0x10000, 0x18000, 0x4ecdd923 )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "821l01.h4",  0x0000, 0x8000, 0x0faca89e )

	ROM_REGION( 0x080000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "821k06.k13", 0x000000, 0x040000, 0xa1eadb24 )	/* characters */
	ROM_LOAD( "821k07.k19", 0x040000, 0x040000, 0x060019fa )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "821k04.k2",  0x000000, 0x080000, 0x00e0291b )	/* sprites */
	ROM_LOAD( "821k05.k8",  0x080000, 0x080000, 0xe09ea05d )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "821a08.i15", 0x0000, 0x0100, 0x7da55800 )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* data for the 007232 */
	ROM_LOAD( "821k03.e5",  0x00000, 0x40000, 0xfef8505a )
ROM_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

static void crimfght_banking( int lines )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs = 0;

	/* bit 5 = select work RAM or palette */
	if (lines & 0x20){
		cpu_setbankhandler_r (1, paletteram_r);							/* palette */
		cpu_setbankhandler_w (1, paletteram_xBBBBBGGGGGRRRRR_swap_w);	/* palette */
	}
	else{
		cpu_setbankhandler_r (1, MRA_RAM);								/* RAM */
		cpu_setbankhandler_w (1, MWA_RAM);								/* RAM */
	}

	/* bit 6 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line((lines & 0x40) ? ASSERT_LINE : CLEAR_LINE);

	offs = 0x10000 + ( ( lines & 0x0f ) * 0x2000 );
	cpu_setbank( 2, &RAM[offs] );
}

static void crimfght_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	konami_cpu_setlines_callback = crimfght_banking;

	/* init the default bank */
	cpu_setbank( 2, &RAM[0x10000] );
}

static void init_crimfght(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}



GAME( 1989, crimfght, 0,        crimfght, crimfght, crimfght, ROT0, "Konami", "Crime Fighters (US 4 players)" )
GAME( 1989, crimfgt2, crimfght, crimfght, crimfgtj, crimfght, ROT0, "Konami", "Crime Fighters (World 2 Players)" )
GAME( 1989, crimfgtj, crimfght, crimfght, crimfgtj, crimfght, ROT0, "Konami", "Crime Fighters (Japan 2 Players)" )
