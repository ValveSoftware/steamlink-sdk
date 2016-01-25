#include "../vidhrdw/mitchell.cpp"

/***************************************************************************

"Mitchell hardware". Actually used mostly by Capcom.

All games run on the same hardware except mgakuen, which runs on an
earlier version, without RAM banking, not encrypted (standard Z80)
and without EEPROM.

Other games that might run on this hardware:
"Chi-toitsu"(YUGA 1988)-Another version of"Mahjong Gakuen"
"MIRAGE -Youjyu mahjong den-"(MITCHELL 1994)

Notes:
- Super Pang has a protection which involves copying code stored in the
  EEPROM to RAM and execute it from there. The first time the game is run,
  you have to keep the player 1 start button pressed until the title screen
  appears to force the game to initialize the EEPROM, otherwise it will not
  work.
  This is simultaed with a kluge in input_r.

TODO:
- understand what bits 0 and 3 of input port 0x05 are
- ball speed is erratic in Block Block. It was not like this at one point.
  This is probably related to interrupts and maybe to the above bits.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"

/* in machine/kabuki.c */
void mgakuen2_decode(void);
void pang_decode(void);
void cworld_decode(void);
void hatena_decode(void);
void spang_decode(void);
void sbbros_decode(void);
void marukin_decode(void);
void qtono1_decode(void);
void qsangoku_decode(void);
void block_decode(void);


int  pang_vh_start(void);
void pang_vh_stop(void);
void pang_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( mgakuen_paletteram_w );
READ_HANDLER( mgakuen_paletteram_r );
WRITE_HANDLER( mgakuen_videoram_w );
READ_HANDLER( mgakuen_videoram_r );
WRITE_HANDLER( mgakuen_objram_w );
READ_HANDLER( mgakuen_objram_r );

WRITE_HANDLER( pang_video_bank_w );
WRITE_HANDLER( pang_videoram_w );
READ_HANDLER( pang_videoram_r );
WRITE_HANDLER( pang_colorram_w );
READ_HANDLER( pang_colorram_r );
WRITE_HANDLER( pang_gfxctrl_w );
WRITE_HANDLER( pang_paletteram_w );
READ_HANDLER( pang_paletteram_r );

extern unsigned char *pang_videoram;
extern unsigned char *pang_colorram;

extern size_t pang_videoram_size;



static WRITE_HANDLER( pang_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + (data & 0x0f) * 0x4000;

	cpu_setbank(1,&RAM[bankaddress]);
}



/***************************************************************************

  EEPROM

***************************************************************************/

static struct EEPROM_interface eeprom_interface =
{
	6,		/* address bits */
	16,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111"	/* erase command */
};

static unsigned char *nvram;
static size_t nvram_size;
static int init_eeprom_count;

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
	{
		EEPROM_save(file);					/* EEPROM */
		if (nvram_size)	/* Super Pang, Block Block */
			osd_fwrite(file,nvram,nvram_size);	/* NVRAM */
	}
	else
	{
		EEPROM_init(&eeprom_interface);

		if (file)
		{
			init_eeprom_count = 0;
			EEPROM_load(file);					/* EEPROM */
			if (nvram_size)	/* Super Pang, Block Block */
			osd_fread(file,nvram,nvram_size);	/* NVRAM */
		}
		else
			init_eeprom_count = 1000;	/* for Super Pang */
	}
}

static READ_HANDLER( pang_port5_r )
{
	int bit;
	extern struct GameDriver driver_mgakuen2;

	bit = EEPROM_read_bit() << 7;

	/* bits 0 and (sometimes) 3 are checked in the interrupt handler. */
	/* Maybe they are vblank related, but I'm not sure. */
	/* bit 3 is checked before updating the palette so it really seems to be vblank. */
	/* Many games require two interrupts per frame and for these bits to toggle, */
	/* otherwise music doesn't work. */
	if (cpu_getiloops() & 1) bit |= 0x01;
	else bit |= 0x08;
if (Machine->gamedrv == &driver_mgakuen2)	/* hack... music doesn't work otherwise */
	bit ^= 0x08;

	return (input_port_0_r(0) & 0x76) | bit;
}

static WRITE_HANDLER( eeprom_cs_w )
{
	EEPROM_set_cs_line(data ? CLEAR_LINE : ASSERT_LINE);
}

static WRITE_HANDLER( eeprom_clock_w )
{
	EEPROM_set_clock_line(data ? CLEAR_LINE : ASSERT_LINE);
}

static WRITE_HANDLER( eeprom_serial_w )
{
	EEPROM_write_bit(data);
}



/***************************************************************************

  Input handling

***************************************************************************/

static int dial[2],dial_selected;

static READ_HANDLER( block_input_r )
{
	static int dir[2];

	if (dial_selected)
	{
		int delta;

		delta = (readinputport(4 + offset) - dial[offset]) & 0xff;
		if (delta & 0x80)
		{
			delta = (-delta) & 0xff;
			if (dir[offset])
			{
			/* don't report movement on a direction change, otherwise it will stutter */
				dir[offset] = 0;
				delta = 0;
			}
		}
		else if (delta > 0)
		{
			if (dir[offset] == 0)
			{
			/* don't report movement on a direction change, otherwise it will stutter */
				dir[offset] = 1;
				delta = 0;
			}
		}
		if (delta > 0x3f) delta = 0x3f;
		return delta << 2;
	}
	else
	{
		int res;

		res = readinputport(2 + offset) & 0xf7;
		if (dir[offset]) res |= 0x08;

		return res;
	}
}

static WRITE_HANDLER( block_dial_control_w )
{
	if (data == 0x08)
	{
		/* reset the dial counters */
		dial[0] = readinputport(4);
		dial[1] = readinputport(5);
	}
	else if (data == 0x80)
		dial_selected = 0;
	else
		dial_selected = 1;
}


static int keymatrix;

static READ_HANDLER( mahjong_input_r )
{
	int i;

	for (i = 0;i < 5;i++)
		if (keymatrix & (0x80 >> i)) return readinputport(2 + 5 * offset + i);

	return 0xff;
}

static WRITE_HANDLER( mahjong_input_select_w )
{
	keymatrix = data;
}


static int input_type;

static READ_HANDLER( input_r )
{
	switch (input_type)
	{
		case 0:
		default:
			return readinputport(1 + offset);
			break;
		case 1:	/* Mahjong games */
			if (offset) return mahjong_input_r(offset-1);
			else return readinputport(1);
			break;
		case 2:	/* Block Block - dial control */
			if (offset) return block_input_r(offset-1);
			else return readinputport(1);
			break;
		case 3:	/* Super Pang - simulate START 1 press to initialize EEPROM */
			if (offset || init_eeprom_count == 0) return readinputport(1 + offset);
			else
			{
				init_eeprom_count--;
				return readinputport(1) & ~0x08;
			}
			break;
	}
}

static WRITE_HANDLER( input_w )
{
	switch (input_type)
	{
		case 0:
		default:
//logerror("PC %04x: write %02x to port 01\n",cpu_get_pc(),data);
			break;
		case 1:
			mahjong_input_select_w(offset,data);
			break;
		case 2:
			block_dial_control_w(offset,data);
			break;
	}
}



/***************************************************************************

  Memory handlers

***************************************************************************/

static struct MemoryReadAddress mgakuen_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc7ff, mgakuen_paletteram_r },	/* palette RAM */
	{ 0xc800, 0xcfff, pang_colorram_r },	/* Attribute RAM */
	{ 0xd000, 0xdfff, mgakuen_videoram_r },	/* char RAM */
	{ 0xe000, 0xefff, MRA_RAM },	/* Work RAM */
	{ 0xf000, 0xffff, mgakuen_objram_r },	/* OBJ RAM */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress mgakuen_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, mgakuen_paletteram_w },
	{ 0xc800, 0xcfff, pang_colorram_w, &pang_colorram },
	{ 0xd000, 0xdfff, mgakuen_videoram_w, &pang_videoram, &pang_videoram_size },
	{ 0xe000, 0xefff, MWA_RAMROM },
	{ 0xf000, 0xffff, mgakuen_objram_w },	/* OBJ RAM */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc7ff, pang_paletteram_r },	/* Banked palette RAM */
	{ 0xc800, 0xcfff, pang_colorram_r },	/* Attribute RAM */
	{ 0xd000, 0xdfff, pang_videoram_r },	/* Banked char / OBJ RAM */
	{ 0xe000, 0xffff, MRA_RAM },	/* Work RAM */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, pang_paletteram_w },
	{ 0xc800, 0xcfff, pang_colorram_w, &pang_colorram },
	{ 0xd000, 0xdfff, pang_videoram_w, &pang_videoram, &pang_videoram_size },
	{ 0xe000, 0xffff, MWA_RAMROM },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x02, input_r },	/* Super Pang needs a kludge to initialize EEPROM;
						the Mahjong games and Block Block need special input treatment */
	{ 0x03, 0x03, input_port_12_r },	/* mgakuen only */
	{ 0x04, 0x04, input_port_13_r },	/* mgakuen only */
	{ 0x05, 0x05, pang_port5_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, pang_gfxctrl_w },    /* Palette bank, layer enable, coin counters, more */
	{ 0x01, 0x01, input_w },
	{ 0x02, 0x02, pang_bankswitch_w },      /* Code bank register */
	{ 0x03, 0x03, YM2413_data_port_0_w },
	{ 0x04, 0x04, YM2413_register_port_0_w },
	{ 0x05, 0x05, OKIM6295_data_0_w },
	{ 0x06, 0x06, MWA_NOP },	/* watchdog? irq ack? */
	{ 0x07, 0x07, pang_video_bank_w },      /* Video RAM bank register */
	{ 0x08, 0x08, eeprom_cs_w },
	{ 0x10, 0x10, eeprom_clock_w },
	{ 0x18, 0x18, eeprom_serial_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( mgakuen )
	PORT_START      /* DSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* data from EEPROM */

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Kan", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 M", KEYCODE_M, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 I", KEYCODE_I, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 A", KEYCODE_A, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Reach", KEYCODE_LSHIFT, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 N", KEYCODE_N, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 J", KEYCODE_J, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 F", KEYCODE_F, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 B", KEYCODE_B, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Ron", KEYCODE_Z, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Chi", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 K", KEYCODE_K, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 G", KEYCODE_G, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 C", KEYCODE_C, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Pon", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 L", KEYCODE_L, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 H", KEYCODE_H, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 D", KEYCODE_D, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Flip", KEYCODE_X, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P2 Kan", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 M", KEYCODE_M, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 I", KEYCODE_I, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 A", KEYCODE_A, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P2 Reach", KEYCODE_LSHIFT, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 N", KEYCODE_N, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 J", KEYCODE_J, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 F", KEYCODE_F, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 B", KEYCODE_B, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P2 Ron", KEYCODE_Z, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 Chi", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 K", KEYCODE_K, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 G", KEYCODE_G, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 C", KEYCODE_C, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 Pon", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 L", KEYCODE_L, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 H", KEYCODE_H, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 D", KEYCODE_D, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 Flip", KEYCODE_X, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x08, 0x08, "Rules" )
	PORT_DIPSETTING(    0x08, "Kantou" )
	PORT_DIPSETTING(    0x00, "Kansai" )
	PORT_DIPNAME( 0x10, 0x00, "Harness Type" )
	PORT_DIPSETTING(    0x10, "Generic" )
	PORT_DIPSETTING(    0x00, "Royal Mahjong" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, "Player 1 Skill" )
	PORT_DIPSETTING(    0x03, "Weak" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x01, "Strong" )
	PORT_DIPSETTING(    0x00, "Very Strong" )
	PORT_DIPNAME( 0x0c, 0x0c, "Player 1 Skill" )
	PORT_DIPSETTING(    0x0c, "Weak" )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x04, "Strong" )
	PORT_DIPSETTING(    0x00, "Very Strong" )
	PORT_DIPNAME( 0x10, 0x00, "Music" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Help Mode" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( marukin )
	PORT_START      /* DSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BITX(0x02, 0x02, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* data from EEPROM */

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode farther down */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Kan", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 M", KEYCODE_M, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 I", KEYCODE_I, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 A", KEYCODE_A, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Reach", KEYCODE_LSHIFT, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 N", KEYCODE_N, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 J", KEYCODE_J, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 F", KEYCODE_F, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 B", KEYCODE_B, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Ron", KEYCODE_Z, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Chi", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 K", KEYCODE_K, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 G", KEYCODE_G, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 C", KEYCODE_C, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Pon", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 L", KEYCODE_L, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 H", KEYCODE_H, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 D", KEYCODE_D, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Flip", KEYCODE_X, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P2 Kan", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 M", KEYCODE_M, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 I", KEYCODE_I, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 A", KEYCODE_A, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P2 Reach", KEYCODE_LSHIFT, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 N", KEYCODE_N, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 J", KEYCODE_J, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 F", KEYCODE_F, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 B", KEYCODE_B, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P2 Ron", KEYCODE_Z, IP_JOY_NONE )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 Chi", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 K", KEYCODE_K, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 G", KEYCODE_G, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 C", KEYCODE_C, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 Pon", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 L", KEYCODE_L, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 H", KEYCODE_H, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 D", KEYCODE_D, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P2 Flip", KEYCODE_X, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( pkladies )
	PORT_START      /* DSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BITX(0x02, 0x02, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* data from EEPROM */

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode farther down */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 Deal", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P1 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 A", KEYCODE_A, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 Cancel", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 B", KEYCODE_B, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 Flip", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 C", KEYCODE_C, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P1 D", KEYCODE_D, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 Deal", KEYCODE_LCONTROL, IP_JOY_NONE )
	PORT_BITX(0x40, IP_ACTIVE_LOW, 0, "P2 E", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 A", KEYCODE_A, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 Cancel", KEYCODE_LALT, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 B", KEYCODE_B, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P2 Flip", KEYCODE_SPACE, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 C", KEYCODE_C, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, 0, "P2 D", KEYCODE_D, IP_JOY_NONE )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( pang )
	PORT_START      /* DSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BITX(0x02, 0x02, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* data from EEPROM */

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( qtono1 )
	PORT_START      /* DSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BITX(0x02, 0x02, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* data from EEPROM */

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode farther down */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( block )
	PORT_START      /* DSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BITX(0x02, 0x02, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* USED - handled in port5_r */
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* data from EEPROM */

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* dial direction */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* dial direction */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )

	PORT_START      /* DIAL1 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 50, 20, 0, 0)

	PORT_START      /* DIAL2 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER2, 50, 20, 0, 0)
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	32768,	/* 32768 characters */
	4,		/* 4 bits per pixel */
	{ 32768*16*8+4, 32768*16*8+0,4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout marukin_charlayout =
{
	8,8,	/* 8*8 characters */
	65536,	/* 65536 characters */
	4,		/* 4 bits per pixel */
	{ 3*4, 2*4, 1*4, 0*4 },
	{ 0, 1, 2, 3, 16+0, 16+1, 16+2, 16+3 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8    /* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	2048,   /* 2048 sprites */
	4,      /* 4 bits per pixel */
	{ 2048*64*8+4, 2048*64*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8    /* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo mgakuen_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &marukin_charlayout, 0,  64 }, /* colors 0-1023 */
	{ REGION_GFX2, 0, &spritelayout,       0,  16 }, /* colors 0- 255 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo marukin_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &marukin_charlayout, 0, 128 }, /* colors 0-2047 */
	{ REGION_GFX2, 0, &spritelayout,       0,  16 }, /* colors 0- 255 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 128 }, /* colors 0-2047 */
	{ REGION_GFX2, 0, &spritelayout,   0,  16 }, /* colors 0- 255 */
	{ -1 } /* end of array */
};



static struct YM2413interface ym2413_interface=
{
	1,	/* 1 chip */
	8000000,	/* 8MHz ??? (hand tuned) */
	{ 50 },	/* Volume */
};

static struct OKIM6295interface okim6295_interface =
{
	1,			/* 1 chip */
	{ 8000 },	/* 8000Hz ??? */
	{ REGION_SOUND1 },		/* memory region 2 */
	{ 50 }
};



static struct MachineDriver machine_driver_mgakuen =
{
	{
		{
			CPU_Z80,
			6000000,	/* ??? */
			mgakuen_readmem,mgakuen_writemem,readport,writeport,
			interrupt,2	/* ??? one extra irq seems to be needed for music (see input5_r) */
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	64*8, 32*8, { 8*8, (64-8)*8-1, 1*8, 31*8-1 },
	mgakuen_gfxdecodeinfo,
	1024, 1024,	/* less colors than the others */
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	pang_vh_start,
	pang_vh_stop,
	pang_vh_screenrefresh,
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		},
		{
			SOUND_YM2413,
			&ym2413_interface
		},
	}

	/* no EEPROM */
};

static struct MachineDriver machine_driver_pang =
{
	{
		{
			CPU_Z80,
			8000000,	/* Super Pang says 8MHZ ORIGINAL BOARD */
			readmem,writemem,readport,writeport,
			interrupt,2	/* ??? one extra irq seems to be needed for music (see input5_r) */
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	64*8, 32*8, { 8*8, (64-8)*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	pang_vh_start,
	pang_vh_stop,
	pang_vh_screenrefresh,
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		},
		{
			SOUND_YM2413,
			&ym2413_interface
		},
	},

	nvram_handler
};

static struct MachineDriver machine_driver_marukin =
{
	{
		{
			CPU_Z80,
			8000000,	/* Super Pang says 8MHZ ORIGINAL BOARD */
			readmem,writemem,readport,writeport,
			interrupt,2	/* ??? one extra irq seems to be needed for music (see input5_r) */
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	64*8, 32*8, { 8*8, (64-8)*8-1, 1*8, 31*8-1 },
	marukin_gfxdecodeinfo,
	2048, 2048,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	pang_vh_start,
	pang_vh_stop,
	pang_vh_screenrefresh,
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		},
		{
			SOUND_YM2413,
			&ym2413_interface
		},
	},

	nvram_handler
};



ROM_START( mgakuen )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 192k for code */
	ROM_LOAD( "mg-1.1j",      0x00000, 0x08000, 0xbf02ea6b )
	ROM_LOAD( "mg-2.1l",      0x10000, 0x20000, 0x64141b0c )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mg-1.13h",     0x000000, 0x80000, 0xfd6a0805 )	/* chars */
	ROM_LOAD( "mg-2.14h",     0x080000, 0x80000, 0xe26e871e )
	ROM_LOAD( "mg-3.16h",     0x100000, 0x80000, 0xdd781d9a )
	ROM_LOAD( "mg-4.17h",     0x180000, 0x80000, 0x97afcc79 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mg-6.4l",      0x000000, 0x20000, 0x34594e62 )	/* sprites */
	ROM_LOAD( "mg-7.6l",      0x020000, 0x20000, 0xf304c806 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "mg-5.1c",      0x00000, 0x80000, 0x170332f1 )	/* banked */
ROM_END

ROM_START( mgakuen2 )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "mg2-xf.1j",    0x00000, 0x08000, 0xc8165d2d )
	ROM_LOAD( "mg2-y.1l",     0x10000, 0x20000, 0x75bbcc14 )
	ROM_LOAD( "mg2-z.3l",     0x30000, 0x20000, 0xbfdba961 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mg2-a.13h",    0x000000, 0x80000, 0x31a0c55e )	/* chars */
	ROM_LOAD( "mg2-b.14h",    0x080000, 0x80000, 0xc18488fa )
	ROM_LOAD( "mg2-c.16h",    0x100000, 0x80000, 0x9425b364 )
	ROM_LOAD( "mg2-d.17h",    0x180000, 0x80000, 0x6cc9eeba )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mg2-f.4l",     0x000000, 0x20000, 0x3172c9fe )	/* sprites */
	ROM_LOAD( "mg2-g.6l",     0x020000, 0x20000, 0x19b8b61c )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "mg2-e.1c",     0x00000, 0x80000, 0x70fd0809 )	/* banked */
ROM_END

ROM_START( pkladies )
	ROM_REGION( 2*0x20000, REGION_CPU1 )	/* 128k for code + 128k for decrypted opcodes */
	ROM_LOAD( "pko-prg1.14f", 0x00000, 0x08000, 0x86585a94 )
	ROM_LOAD( "pko-prg2.15f", 0x10000, 0x10000, 0x86cbe82d )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "pko-001.8h",   0x000000, 0x80000, 0x1ead5d9b )	/* chars */
	ROM_LOAD_GFX_ODD ( "pko-003.8j",   0x000000, 0x80000, 0x339ab4e6 )
	ROM_LOAD_GFX_EVEN( "pko-002.9h",   0x100000, 0x80000, 0x1cf02586 )
	ROM_LOAD_GFX_ODD ( "pko-004.9j",   0x100000, 0x80000, 0x09ccb442 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "pko-chr1.2j",  0x000000, 0x20000, 0x31ce33cd )	/* sprites */
	ROM_LOAD( "pko-chr2.3j",  0x020000, 0x20000, 0xad7e055f )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "pko-voi1.2d",  0x00000, 0x20000, 0x07e0f531 )
	ROM_LOAD( "pko-voi2.3d",  0x20000, 0x20000, 0x18398bf6 )
ROM_END

ROM_START( dokaben )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "db06.11h",     0x00000, 0x08000, 0x413e0886 )
	ROM_LOAD( "db07.13h",     0x10000, 0x20000, 0x8bdcf49e )
	ROM_LOAD( "db08.14h",     0x30000, 0x20000, 0x1643bdd9 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "db02.1e",      0x000000, 0x20000, 0x9aa8470c )	/* chars */
	ROM_LOAD( "db03.2e",      0x020000, 0x20000, 0x3324e43d )
	/* 40000-7ffff empty */
	ROM_LOAD( "db04.1g",      0x080000, 0x20000, 0xc0c5b6c2 )
	ROM_LOAD( "db05.2g",      0x0a0000, 0x20000, 0xd2ab25f2 )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "db10.2k",      0x000000, 0x20000, 0x9e70f7ae )	/* sprites */
	ROM_LOAD( "db09.1k",      0x020000, 0x20000, 0x2d9263f7 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "db01.1d",      0x00000, 0x20000, 0x62fa6b81 )
ROM_END

ROM_START( pang )
	ROM_REGION( 2*0x30000, REGION_CPU1 )	/* 192k for code + 192k for decrypted opcodes */
	ROM_LOAD( "pang6.bin",    0x00000, 0x08000, 0x68be52cd )
	ROM_LOAD( "pang7.bin",    0x10000, 0x20000, 0x4a2e70f6 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "pang_09.bin",  0x000000, 0x20000, 0x3a5883f5 )	/* chars */
	ROM_LOAD( "bb3.bin",      0x020000, 0x20000, 0x79a8ed08 )
	/* 40000-7ffff empty */
	ROM_LOAD( "pang_11.bin",  0x080000, 0x20000, 0x166a16ae )
	ROM_LOAD( "bb5.bin",      0x0a0000, 0x20000, 0x2fb3db6c )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bb10.bin",     0x000000, 0x20000, 0xfdba4f6e )	/* sprites */
	ROM_LOAD( "bb9.bin",      0x020000, 0x20000, 0x39f47a63 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bb1.bin",      0x00000, 0x20000, 0xc52e5b8e )
ROM_END

ROM_START( pangb )
	ROM_REGION( 2*0x30000, REGION_CPU1 )	/* 192k for code + 192k for decrypted opcodes */
	ROM_LOAD( "pang_04.bin",  0x30000, 0x08000, 0xf68f88a5 )   /* Decrypted opcode + data */
	ROM_CONTINUE(             0x00000, 0x08000 )
	ROM_LOAD( "pang_02.bin",  0x40000, 0x20000, 0x3f15bb61 )   /* Decrypted op codes */
	ROM_LOAD( "pang_03.bin",  0x10000, 0x20000, 0x0c8477ae )   /* Decrypted data */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "pang_09.bin",  0x000000, 0x20000, 0x3a5883f5 )	/* chars */
	ROM_LOAD( "bb3.bin",      0x020000, 0x20000, 0x79a8ed08 )
	/* 40000-7ffff empty */
	ROM_LOAD( "pang_11.bin",  0x080000, 0x20000, 0x166a16ae )
	ROM_LOAD( "bb5.bin",      0x0a0000, 0x20000, 0x2fb3db6c )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bb10.bin",     0x000000, 0x20000, 0xfdba4f6e )	/* sprites */
	ROM_LOAD( "bb9.bin",      0x020000, 0x20000, 0x39f47a63 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bb1.bin",      0x00000, 0x20000, 0xc52e5b8e )
ROM_END

ROM_START( bbros )
	ROM_REGION( 2*0x30000, REGION_CPU1 )	/* 192k for code + 192k for decrypted opcodes */
	ROM_LOAD( "bb6.bin",      0x00000, 0x08000, 0xa3041ca4 )
	ROM_LOAD( "bb7.bin",      0x10000, 0x20000, 0x09231c68 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bb2.bin",      0x000000, 0x20000, 0x62f29992 )	/* chars */
	ROM_LOAD( "bb3.bin",      0x020000, 0x20000, 0x79a8ed08 )
	/* 40000-7ffff empty */
	ROM_LOAD( "bb4.bin",      0x080000, 0x20000, 0xf705aa89 )
	ROM_LOAD( "bb5.bin",      0x0a0000, 0x20000, 0x2fb3db6c )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bb10.bin",     0x000000, 0x20000, 0xfdba4f6e )	/* sprites */
	ROM_LOAD( "bb9.bin",      0x020000, 0x20000, 0x39f47a63 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bb1.bin",      0x00000, 0x20000, 0xc52e5b8e )
ROM_END

ROM_START( pompingw )
	ROM_REGION( 2*0x30000, REGION_CPU1 )	/* 192k for code + 192k for decrypted opcodes */
	ROM_LOAD( "pwj_06.11h",   0x00000, 0x08000, 0x4a0a6426 )
	ROM_LOAD( "pwj_07.13h",   0x10000, 0x20000, 0xa9402420 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "pw_02.1e",     0x000000, 0x20000, 0x4b5992e4 )	/* chars */
	ROM_LOAD( "bb3.bin",      0x020000, 0x20000, 0x79a8ed08 )
	/* 40000-7ffff empty */
	ROM_LOAD( "pwj_04.1g",    0x080000, 0x20000, 0x01e49081 )
	ROM_LOAD( "bb5.bin",      0x0a0000, 0x20000, 0x2fb3db6c )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bb10.bin",     0x000000, 0x20000, 0xfdba4f6e )	/* sprites */
	ROM_LOAD( "bb9.bin",      0x020000, 0x20000, 0x39f47a63 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bb1.bin",      0x00000, 0x20000, 0xc52e5b8e )
ROM_END

ROM_START( cworld )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "cw05.bin",     0x00000, 0x08000, 0xd3c1723d )
	ROM_LOAD( "cw06.bin",     0x10000, 0x20000, 0xd71ed4a3 )
	ROM_LOAD( "cw07.bin",     0x30000, 0x20000, 0xd419ce08 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cw08.bin",     0x000000, 0x20000, 0x6c80da3c )	/* chars */
	ROM_LOAD( "cw09.bin",     0x020000, 0x20000, 0x7607da71 )
	ROM_LOAD( "cw10.bin",     0x040000, 0x20000, 0x6f0e639f )
	ROM_LOAD( "cw11.bin",     0x060000, 0x20000, 0x130bd7c0 )
	ROM_LOAD( "cw18.bin",     0x080000, 0x20000, 0xbe6ee0c9 )
	ROM_LOAD( "cw19.bin",     0x0a0000, 0x20000, 0x51fc5532 )
	ROM_LOAD( "cw20.bin",     0x0c0000, 0x20000, 0x58381d58 )
	ROM_LOAD( "cw21.bin",     0x0e0000, 0x20000, 0x910cc753 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cw16.bin",     0x000000, 0x20000, 0xf90217d1 )	/* sprites */
	ROM_LOAD( "cw17.bin",     0x020000, 0x20000, 0xc953c702 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "cw01.bin",     0x00000, 0x20000, 0xf4368f5b )
ROM_END

ROM_START( hatena )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "q2-05.rom",    0x00000, 0x08000, 0x66c9e1da )
	ROM_LOAD( "q2-06.rom",    0x10000, 0x20000, 0x5fc39916 )
	ROM_LOAD( "q2-07.rom",    0x30000, 0x20000, 0xec6d5e5e )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "q2-08.rom",    0x000000, 0x20000, 0x6c80da3c )	/* chars */
	ROM_LOAD( "q2-09.rom",    0x020000, 0x20000, 0xabe3e15c )
	ROM_LOAD( "q2-10.rom",    0x040000, 0x20000, 0x6963450d )
	ROM_LOAD( "q2-11.rom",    0x060000, 0x20000, 0x1e319fa2 )
	ROM_LOAD( "q2-18.rom",    0x080000, 0x20000, 0xbe6ee0c9 )
	ROM_LOAD( "q2-19.rom",    0x0a0000, 0x20000, 0x70300445 )
	ROM_LOAD( "q2-20.rom",    0x0c0000, 0x20000, 0x21a6ff42 )
	ROM_LOAD( "q2-21.rom",    0x0e0000, 0x20000, 0x076280c9 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "q2-16.rom",    0x000000, 0x20000, 0xec19b2f0 )	/* sprites */
	ROM_LOAD( "q2-17.rom",    0x020000, 0x20000, 0xecd69d92 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "q2-01.rom",    0x00000, 0x20000, 0x149e7a89 )
ROM_END

ROM_START( spang )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "spe_06.rom",   0x00000, 0x08000, 0x1af106fb )
	ROM_LOAD( "spe_07.rom",   0x10000, 0x20000, 0x208b5f54 )
	ROM_LOAD( "spe_08.rom",   0x30000, 0x20000, 0x2bc03ade )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "spe_02.rom",   0x000000, 0x20000, 0x63c9dfd2 )	/* chars */
	ROM_LOAD( "03.f2",        0x020000, 0x20000, 0x3ae28bc1 )
	/* 40000-7ffff empty */
	ROM_LOAD( "spe_04.rom",   0x080000, 0x20000, 0x9d7b225b )
	ROM_LOAD( "05.g2",        0x0a0000, 0x20000, 0x4a060884 )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "spe_10.rom",   0x000000, 0x20000, 0xeedd0ade )	/* sprites */
	ROM_LOAD( "spe_09.rom",   0x020000, 0x20000, 0x04b41b75 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "spe_01.rom",   0x00000, 0x20000, 0x2d19c133 )
ROM_END

ROM_START( sbbros )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "06.j12",       0x00000, 0x08000, 0x292eee6a )
	ROM_LOAD( "07.j13",       0x10000, 0x20000, 0xf46b698d )
	ROM_LOAD( "08.j14",       0x30000, 0x20000, 0xa75e7fbe )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "02.f1",        0x000000, 0x20000, 0x0c22ffc6 )	/* chars */
	ROM_LOAD( "03.f2",        0x020000, 0x20000, 0x3ae28bc1 )
	/* 40000-7ffff empty */
	ROM_LOAD( "04.g2",        0x080000, 0x20000, 0xbb3dee5b )
	ROM_LOAD( "05.g2",        0x0a0000, 0x20000, 0x4a060884 )
	/* c0000-fffff empty */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "10.l2",        0x000000, 0x20000, 0xd6675d8f )	/* sprites */
	ROM_LOAD( "09.l1",        0x020000, 0x20000, 0x8f678bc8 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "01.d1",        0x00000, 0x20000, 0xb96ea126 )
ROM_END

ROM_START( marukin )
	ROM_REGION( 2*0x30000, REGION_CPU1 )	/* 192k for code + 192k for decrypted opcodes */
	ROM_LOAD( "mg3-01.9d",    0x00000, 0x08000, 0x04357973 )
	ROM_LOAD( "mg3-02.10d",   0x10000, 0x20000, 0x50d08da0 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mg3-a.3k",     0x000000, 0x80000, 0x420f1de7 )	/* chars */
	ROM_LOAD( "mg3-b.4k",     0x080000, 0x80000, 0xd8de13fa )
	ROM_LOAD( "mg3-c.6k",     0x100000, 0x80000, 0xfbeb66e8 )
	ROM_LOAD( "mg3-d.7k",     0x180000, 0x80000, 0x8f6bd831 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mg3-05.2g",    0x000000, 0x20000, 0x7a738d2d )	/* sprites */
	ROM_LOAD( "mg3-04.1g",    0x020000, 0x20000, 0x56f30515 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "mg3-e.1d",     0x00000, 0x80000, 0x106c2fa9 )	/* banked */
ROM_END

ROM_START( qtono1 )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "q3-05.rom",    0x00000, 0x08000, 0x1dd0a344 )
	ROM_LOAD( "q3-06.rom",    0x10000, 0x20000, 0xbd6a2110 )
	ROM_LOAD( "q3-07.rom",    0x30000, 0x20000, 0x61e53c4f )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "q3-08.rom",    0x000000, 0x20000, 0x1533b978 )	/* chars */
	ROM_LOAD( "q3-09.rom",    0x020000, 0x20000, 0xa32db2f2 )
	ROM_LOAD( "q3-10.rom",    0x040000, 0x20000, 0xed681aa8 )
	ROM_LOAD( "q3-11.rom",    0x060000, 0x20000, 0x38b2fd10 )
	ROM_LOAD( "q3-18.rom",    0x080000, 0x20000, 0x9e4292ac )
	ROM_LOAD( "q3-19.rom",    0x0a0000, 0x20000, 0xb7f6d40f )
	ROM_LOAD( "q3-20.rom",    0x0c0000, 0x20000, 0x6cd7f38d )
	ROM_LOAD( "q3-21.rom",    0x0e0000, 0x20000, 0xb4aa6b4b )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "q3-16.rom",    0x000000, 0x20000, 0x863d6836 )	/* sprites */
	ROM_LOAD( "q3-17.rom",    0x020000, 0x20000, 0x459bf59c )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "q3-01.rom",    0x00000, 0x20000, 0x6c1be591 )
ROM_END

ROM_START( qsangoku )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "q4-05c.rom",   0x00000, 0x08000, 0xe1d010b4 )
	ROM_LOAD( "q4-06.rom",    0x10000, 0x20000, 0xa0301849 )
	ROM_LOAD( "q4-07.rom",    0x30000, 0x20000, 0x2941ef5b )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "q4-08.rom",    0x000000, 0x20000, 0xdc84c6cb )	/* chars */
	ROM_LOAD( "q4-09.rom",    0x020000, 0x20000, 0xcbb6234c )
	ROM_LOAD( "q4-10.rom",    0x040000, 0x20000, 0xc20a27a8 )
	ROM_LOAD( "q4-11.rom",    0x060000, 0x20000, 0x4ff66aed )
	ROM_LOAD( "q4-18.rom",    0x080000, 0x20000, 0xca3acea5 )
	ROM_LOAD( "q4-19.rom",    0x0a0000, 0x20000, 0x1fd92b7d )
	ROM_LOAD( "q4-20.rom",    0x0c0000, 0x20000, 0xb02dc6a1 )
	ROM_LOAD( "q4-21.rom",    0x0e0000, 0x20000, 0x432b1dc1 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "q4-16.rom",    0x000000, 0x20000, 0x77342320 )	/* sprites */
	ROM_LOAD( "q4-17.rom",    0x020000, 0x20000, 0x1275c436 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "q4-01.rom",    0x00000, 0x20000, 0x5d0d07d8 )
ROM_END

ROM_START( block )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "ble_05.rom",   0x00000, 0x08000, 0xc12e7f4c )
	ROM_LOAD( "ble_06.rom",   0x10000, 0x20000, 0xcdb13d55 )
	ROM_LOAD( "ble_07.rom",   0x30000, 0x20000, 0x1d114f13 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bl_08.rom",    0x000000, 0x20000, 0xaa0f4ff1 )	/* chars */
	ROM_RELOAD(               0x040000, 0x20000 )
	ROM_LOAD( "bl_09.rom",    0x020000, 0x20000, 0x6fa8c186 )
	ROM_RELOAD(               0x060000, 0x20000 )
	ROM_LOAD( "bl_18.rom",    0x080000, 0x20000, 0xc0acafaf )
	ROM_RELOAD(               0x0c0000, 0x20000 )
	ROM_LOAD( "bl_19.rom",    0x0a0000, 0x20000, 0x1ae942f5 )
	ROM_RELOAD(               0x0e0000, 0x20000 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bl_16.rom",    0x000000, 0x20000, 0xfadcaff7 )	/* sprites */
	ROM_LOAD( "bl_17.rom",    0x020000, 0x20000, 0x5f8cab42 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bl_01.rom",    0x00000, 0x20000, 0xc2ec2abb )
ROM_END

ROM_START( blockj )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "blj_05.rom",   0x00000, 0x08000, 0x3b55969a )
	ROM_LOAD( "ble_06.rom",   0x10000, 0x20000, 0xcdb13d55 )
	ROM_LOAD( "blj_07.rom",   0x30000, 0x20000, 0x1723883c )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bl_08.rom",    0x000000, 0x20000, 0xaa0f4ff1 )	/* chars */
	ROM_RELOAD(               0x040000, 0x20000 )
	ROM_LOAD( "bl_09.rom",    0x020000, 0x20000, 0x6fa8c186 )
	ROM_RELOAD(               0x060000, 0x20000 )
	ROM_LOAD( "bl_18.rom",    0x080000, 0x20000, 0xc0acafaf )
	ROM_RELOAD(               0x0c0000, 0x20000 )
	ROM_LOAD( "bl_19.rom",    0x0a0000, 0x20000, 0x1ae942f5 )
	ROM_RELOAD(               0x0e0000, 0x20000 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bl_16.rom",    0x000000, 0x20000, 0xfadcaff7 )	/* sprites */
	ROM_LOAD( "bl_17.rom",    0x020000, 0x20000, 0x5f8cab42 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bl_01.rom",    0x00000, 0x20000, 0xc2ec2abb )
ROM_END

ROM_START( blockbl )
	ROM_REGION( 2*0x50000, REGION_CPU1 )	/* 320k for code + 320k for decrypted opcodes */
	ROM_LOAD( "m7.l6",        0x50000, 0x08000, 0x3b576fd9 )   /* Decrypted opcode + data */
	ROM_CONTINUE(             0x00000, 0x08000 )
	ROM_LOAD( "m5.l3",        0x60000, 0x20000, 0x7c988bb7 )   /* Decrypted opcode + data */
	ROM_CONTINUE(             0x10000, 0x20000 )
	ROM_LOAD( "m6.l5",        0x30000, 0x20000, 0x5768d8eb )   /* Decrypted data */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "m12.o10",      0x000000, 0x20000, 0x963154d9 )	/* chars */
	ROM_RELOAD(               0x040000, 0x20000 )
	ROM_LOAD( "m13.o14",      0x020000, 0x20000, 0x069480bb )
	ROM_RELOAD(               0x060000, 0x20000 )
	ROM_LOAD( "m4.j17",       0x080000, 0x20000, 0x9e3b6f4f )
	ROM_RELOAD(               0x0c0000, 0x20000 )
	ROM_LOAD( "m3.j20",       0x0a0000, 0x20000, 0x629d58fe )
	ROM_RELOAD(               0x0e0000, 0x20000 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "m11.o7",       0x000000, 0x10000, 0x255180a5 )	/* sprites */
	ROM_LOAD( "m10.o5",       0x010000, 0x10000, 0x3201c088 )
	ROM_LOAD( "m9.o3",        0x020000, 0x10000, 0x29357fe4 )
	ROM_LOAD( "m8.o2",        0x030000, 0x10000, 0xabd665d1 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* OKIM */
	ROM_LOAD( "bl_01.rom",    0x00000, 0x20000, 0xc2ec2abb )
ROM_END


static void bootleg_decode(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	memory_set_opcode_base(0,rom+diff);
}



static void init_dokaben(void)
{
	input_type = 0;
	nvram_size = 0;
	mgakuen2_decode();
}
static void init_pang(void)
{
	input_type = 0;
	nvram_size = 0;
	pang_decode();
}
static void init_pangb(void)
{
	input_type = 0;
	nvram_size = 0;
	bootleg_decode();
}
static void init_cworld(void)
{
	input_type = 0;
	nvram_size = 0;
	cworld_decode();
}
static void init_hatena(void)
{
	input_type = 0;
	nvram_size = 0;
	hatena_decode();
}
static void init_spang(void)
{
	input_type = 3;
	nvram_size = 0x80;
	nvram = &memory_region(REGION_CPU1)[0xe000];	/* NVRAM */
	spang_decode();
}
static void init_sbbros(void)
{
	input_type = 3;
	nvram_size = 0x80;
	nvram = &memory_region(REGION_CPU1)[0xe000];	/* NVRAM */
	sbbros_decode();
}
static void init_qtono1(void)
{
	input_type = 0;
	nvram_size = 0;
	qtono1_decode();
}
static void init_qsangoku(void)
{
	input_type = 0;
	nvram_size = 0;
	qsangoku_decode();
}
static void init_mgakuen(void)
{
	input_type = 1;
}
static void init_mgakuen2(void)
{
	input_type = 1;
	nvram_size = 0;
	mgakuen2_decode();
}
static void init_marukin(void)
{
	input_type = 1;
	nvram_size = 0;
	marukin_decode();
}
static void init_block(void)
{
	input_type = 2;
	nvram_size = 0x80;
	nvram = &memory_region(REGION_CPU1)[0xff80];	/* NVRAM */
	block_decode();
}
static void init_blockbl(void)
{
	input_type = 2;
	nvram_size = 0x80;
	nvram = &memory_region(REGION_CPU1)[0xff80];	/* NVRAM */
	bootleg_decode();
}



GAME( 1988, mgakuen,  0,     mgakuen, mgakuen,  mgakuen,  ROT0,   "Yuga", "Mahjong Gakuen" )
GAME( 1989, mgakuen2, 0,     marukin, marukin,  mgakuen2, ROT0,   "Face", "Mahjong Gakuen 2 Gakuen-chou no Fukushuu" )
GAME( 1989, pkladies, 0,     marukin, pkladies, mgakuen2, ROT0,   "Mitchell", "Poker Ladies" )
GAME( 1989, dokaben,  0,     pang,    pang,     dokaben,  ROT0,   "Capcom", "Dokaben (Japan)" )
GAME( 1989, pang,     0,     pang,    pang,     pang,     ROT0,   "Mitchell", "Pang (World)" )
GAME( 1989, pangb,    pang,  pang,    pang,     pangb,    ROT0,   "bootleg", "Pang (bootleg)" )
GAME( 1989, bbros,    pang,  pang,    pang,     pang,     ROT0,   "Capcom", "Buster Bros (US)" )
GAME( 1989, pompingw, pang,  pang,    pang,     pang,     ROT0,   "Mitchell", "Pomping World (Japan)" )
GAME( 1989, cworld,   0,     pang,    qtono1,   cworld,   ROT0,   "Capcom", "Capcom World (Japan)" )
GAME( 1990, hatena,   0,     pang,    qtono1,   hatena,   ROT0,   "Capcom", "Adventure Quiz 2 Hatena Hatena no Dai-Bouken (Japan)" )
GAME( 1990, spang,    0,     pang,    pang,     spang,    ROT0,   "Mitchell", "Super Pang (World)" )
GAME( 1990, sbbros,   spang, pang,    pang,     sbbros,   ROT0,   "Mitchell + Capcom", "Super Buster Bros (US)" )
GAME( 1990, marukin,  0,     marukin, marukin,  marukin,  ROT0,   "Yuga", "Super Marukin-Ban" )
GAME( 1991, qtono1,   0,     pang,    qtono1,   qtono1,   ROT0,   "Capcom", "Quiz Tonosama no Yabou (Japan)" )
GAME( 1991, qsangoku, 0,     pang,    qtono1,   qsangoku, ROT0,   "Capcom", "Quiz Sangokushi (Japan)" )
GAME( 1991, block,    0,     pang,    block,    block,    ROT270, "Capcom", "Block Block (World)" )
GAME( 1991, blockj,   block, pang,    block,    block,    ROT270, "Capcom", "Block Block (Japan)" )
GAME( 1991, blockbl,  block, pang,    block,    blockbl,  ROT270, "bootleg", "Block Block (bootleg)" )
