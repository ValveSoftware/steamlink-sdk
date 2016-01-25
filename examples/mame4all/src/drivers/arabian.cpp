#include "../vidhrdw/arabian.cpp"

/***************************************************************************

Arabian memory map (preliminary)

0000-1fff ROM 0
2000-3fff ROM 1
4000-5fff ROM 2
6000-7fff ROM 3
8000-bfff VIDEO RAM

d000-dfff RAM
e000-e100 CRT controller  ? blitter ?

memory mapped ports:
read:
d7f0 - IN 0
d7f1 - IN 1
d7f2 - IN 2
d7f3 - IN 3
d7f4 - IN 4
d7f5 - IN 5
d7f6 - clock HI  ?
d7f7 - clock set ?
d7f8 - clock LO  ?

c000 - DSW 0
c200 - DSW 1

DSW 0
bit 7 = ?
bit 6 = ?
bit 5 = ?
bit 4 = ?
bit 3 = ?
bit 2 = 1 - test mode
bit 1 = COIN 2
bit 0 = COIN 1

DSW 1
bit 7 - \
bit 6 - - coins per credit    (ALL=1 free play)
bit 5 - -
bit 4 - /
bit 3 - carry bowls / don't carry bowls to next level (0 DON'T CARRY)
bit 2 - PIC NORMAL or FLIPPED (0 NORMAL)
bit 1 - COCKTAIL or UPRIGHT   (0 COCKTAIL)
bit 0 - LIVES 0=3 lives 1=5 lives

write:
c800 - AY-3-8910 control
ca00 - AY-3-8910 write

ON AY PORTS there are two things :

port 0x0e (write only) to do ...

port 0x0f (write only) controls (active LO):
BIT 0 -	coin counter 1
BIT 1 - coin counter 2
BIT 2 -
BIT 3 -
BIT 4 - 0-read RAM  1-read switches(ports)
BIT 5 -
BIT 6 -
BIT 7 -

interrupts:
standart IM 1 interrupt mode (rst #38 every vblank)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



void arabian_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int arabian_vh_start(void);
void arabian_vh_stop(void);
void arabian_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( arabian_blitter_w );
WRITE_HANDLER( arabian_videoram_w );


static UINT8 arabian_clock=0;
static UINT8 portB=0;


static int arabian_interrupt(void)
{
	arabian_clock++;
	return interrupt();
}


static WRITE_HANDLER( arabian_portB_w )
{
	int pc;
	static int last;

	portB = data;

	pc = cpu_get_pc();
	if ((pc == 0x0a7a) || (pc == 0x002a)) pc = cpu_geturnpc();
	//if ((data & 0xec) != last) logerror("Port B written  %02X  PC=%04X\n",data,pc);
	last = data & 0xec;

	coin_counter_w(0, ~data & 0x01);
	coin_counter_w(1, ~data & 0x02);
}

static READ_HANDLER( arabian_input_port_r )
{
	int pom;


	if (portB & 0x10)  /* if 1 read the switches */
	{
		switch(offset)
		{
		case 0:  pom = readinputport(2); break;
		case 1:  pom = readinputport(3); break;
		case 2:  pom = readinputport(4); break;
		case 3:  pom = readinputport(5); break;
		case 4:  pom = readinputport(6); break;
		case 5:  pom = readinputport(7); break;
		case 6:  pom = arabian_clock >> 4; break;
		case 8:  pom = arabian_clock & 0x0f; break;
		default:
			//logerror("Input Port %04X read.  PC=%04X\n", offset+0xd7f0, cpu_get_pc());
			pom = 0;
			break;
		}
	}
	else  /* if bit 4 of AY port 0f==0 then read RAM memory instead of switches */
	{
		unsigned char *RAM = memory_region(REGION_CPU1);
		pom = RAM[ 0xd7f0 + offset ];
	}

	return pom;
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc000, input_port_0_r },
	{ 0xc200, 0xc200, input_port_1_r },
	{ 0xd000, 0xd7ef, MRA_RAM },
	{ 0xd7f0, 0xd7ff, arabian_input_port_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xbfff, arabian_videoram_w, &videoram },
	{ 0xd000, 0xd7ff, MWA_RAM },
	{ 0xe000, 0xe07f, arabian_blitter_w, &spriteram },
	{ -1 }  /* end of table */
};



static struct IOWritePort writeport[] =
{
	{ 0xc800, 0xc800, AY8910_control_port_0_w },
	{ 0xca00, 0xca00, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( arabian )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_SERVICE( 0x04, IP_ACTIVE_HIGH )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Carry Bowls to Next Life" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xf0, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x10, "A 2/1 B 2/1" )
	PORT_DIPSETTING(    0x20, "A 2/1 B 1/3" )
	PORT_DIPSETTING(    0x00, "A 1/1 B 1/1" )
	PORT_DIPSETTING(    0x30, "A 1/1 B 1/2" )
	PORT_DIPSETTING(    0x40, "A 1/1 B 1/3" )
	PORT_DIPSETTING(    0x50, "A 1/1 B 1/4" )
	PORT_DIPSETTING(    0x60, "A 1/1 B 1/5" )
	PORT_DIPSETTING(    0x70, "A 1/1 B 1/6" )
	PORT_DIPSETTING(    0x80, "A 1/2 B 1/2" )
	PORT_DIPSETTING(    0x90, "A 1/2 B 1/4" )
	PORT_DIPSETTING(    0xa0, "A 1/2 B 1/6" )
	PORT_DIPSETTING(    0xb0, "A 1/2 B 1/10" )
	PORT_DIPSETTING(    0xc0, "A 1/2 B 1/11" )
	PORT_DIPSETTING(    0xd0, "A 1/2 B 1/12" )
	PORT_DIPSETTING(    0xf0, DEF_STR( Free_Play ) )
	/* 0xe0 gives A 1/2 B 1/6 */

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, "Coin Chutes" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "20000" )
	PORT_DIPSETTING(    0x08, "40000" )
	PORT_DIPSETTING(    0x0c, "20000 50000 +100K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END




static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz?????? */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ arabian_portB_w }
};



static struct MachineDriver machine_driver_arabian =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80 | CPU_16BIT_PORT,
			4000000,	/* 4 Mhz */
			readmem,writemem,0,writeport,
			arabian_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	256, 256, { 0, 255, 11, 242 },
	0,
	32,32,
	arabian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	arabian_vh_start,
	arabian_vh_stop,
	arabian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( arabian )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ic1rev2.87",       0x0000, 0x2000, 0x5e1c98b8 )
	ROM_LOAD( "ic2rev2.88",       0x2000, 0x2000, 0x092f587e )
	ROM_LOAD( "ic3rev2.89",       0x4000, 0x2000, 0x15145f23 )
	ROM_LOAD( "ic4rev2.90",       0x6000, 0x2000, 0x32b77b44 )

	ROM_REGION( 0x10000, REGION_GFX1 ) /* graphics roms */
	ROM_LOAD( "ic84.91",      0x0000, 0x2000, 0xc4637822 )	/* because of very rare way */
	ROM_LOAD( "ic85.92",      0x2000, 0x2000, 0xf7c6866d )  /* CRT controller uses these roms */
	ROM_LOAD( "ic86.93",      0x4000, 0x2000, 0x71acd48d )  /* there's no way, but to decode */
	ROM_LOAD( "ic87.94",      0x6000, 0x2000, 0x82160b9a )	/* it at runtime - which is SLOW */
ROM_END

ROM_START( arabiana )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ic1.87",       0x0000, 0x2000, 0x51e9a6b1 )
	ROM_LOAD( "ic2.88",       0x2000, 0x2000, 0x1cdcc1ab )
	ROM_LOAD( "ic3.89",       0x4000, 0x2000, 0xb7b7faa0 )
	ROM_LOAD( "ic4.90",       0x6000, 0x2000, 0xdbded961 )

	ROM_REGION( 0x10000, REGION_GFX1 ) /* graphics roms */
	ROM_LOAD( "ic84.91",      0x0000, 0x2000, 0xc4637822 )	/* because of very rare way */
	ROM_LOAD( "ic85.92",      0x2000, 0x2000, 0xf7c6866d )  /* CRT controller uses these roms */
	ROM_LOAD( "ic86.93",      0x4000, 0x2000, 0x71acd48d )  /* there's no way, but to decode */
	ROM_LOAD( "ic87.94",      0x6000, 0x2000, 0x82160b9a )	/* it at runtime - which is SLOW */
ROM_END



GAMEX( 1983, arabian,  0,       arabian, arabian, 0, ROT270, "Sun Electronics", "Arabian", GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
GAMEX( 1983, arabiana, arabian, arabian, arabian, 0, ROT270, "[Sun Electronics] (Atari license)", "Arabian (Atari)", GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
