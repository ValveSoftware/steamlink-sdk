#include "../vidhrdw/seicross.cpp"

/***************************************************************************

Seicross memory map (preliminary)

driver by Nicola Salmoria


0000-77ff ROM
7800-7fff RAM
9000-93ff videoram
9c00-9fff colorram

Read:
A000      Joystick + Players start button
A800      player #2 controls + coin + ?
B000      test switches
B800      watchdog reset

Write:
8820-887f Sprite ram
9800-981f Scroll control
9880-989f ? (always 0?)

I/O ports:
0         8910 control
1         8910 write
4         8910 read


There is a microcontroller on the board. Nichibutsu custom part marked
NSC81050-102  8127 E37 and labeled No. 00363.  It's a 40-pin IC at location 4F
on the (Seicross-) board. Looks like it is linked to the dips (and those are
on a very small daughterboard).

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *seicross_row_scroll;
WRITE_HANDLER( seicross_colorram_w );
void seicross_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void seicross_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static unsigned char *nvram;
static size_t nvram_size;


static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
		{
			/* fill in the default values */
			memset(nvram,0,nvram_size);
			nvram[0x0d] = nvram[0x0f] = nvram[0x11] = nvram[0x13] = nvram[0x15] = nvram[0x19] = 1;
			nvram[0x17] = 3;
		}
	}
}



static void friskyt_init_machine(void)
{
	/* start with the protection mcu halted */
	cpu_set_halt_line(1, ASSERT_LINE);
}



static int portb;

static READ_HANDLER( friskyt_portB_r )
{
	return (portb & 0x9f) | (readinputport(6) & 0x60);
}

static WRITE_HANDLER( friskyt_portB_w )
{
//logerror("PC %04x: 8910 port B = %02x\n",cpu_get_pc(),data);
	/* bit 0 is IRQ enable */
	interrupt_enable_w(0,data & 1);

	/* bit 1 flips screen */

	/* bit 2 resets the microcontroller */
	if (((portb & 4) == 0) && (data & 4))
	{
		/* reset and start the protection mcu */
		cpu_set_reset_line(1, PULSE_LINE);
		cpu_set_halt_line(1, CLEAR_LINE);
	}

	/* other bits unknown */
	portb = data;
}


static unsigned char *sharedram;

static READ_HANDLER( sharedram_r )
{
	return sharedram[offset];
}

static WRITE_HANDLER( sharedram_w )
{
	sharedram[offset] = data;
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x77ff, MRA_ROM },
	{ 0x7800, 0x7fff, sharedram_r },
	{ 0x8820, 0x887f, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },	/* video RAM */
	{ 0x9800, 0x981f, MRA_RAM },
	{ 0x9c00, 0x9fff, MRA_RAM },	/* color RAM */
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },	/* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },	/* test */
	{ 0xb800, 0xb800, watchdog_reset_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x77ff, MWA_ROM },
	{ 0x7800, 0x7fff, sharedram_w, &sharedram },
	{ 0x8820, 0x887f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0x9800, 0x981f, MWA_RAM, &seicross_row_scroll },
	{ 0x9880, 0x989f, MWA_RAM, &spriteram_2, &spriteram_2_size },
	{ 0x9c00, 0x9fff, seicross_colorram_w, &colorram },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x04, 0x04, AY8910_read_port_0_r },
	{ 0x0c, 0x0c, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x08, 0x08, AY8910_control_port_0_w },
	{ 0x09, 0x09, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress mcu_nvram_readmem[] =
{
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0x1000, 0x10ff, MRA_RAM },
	{ 0x8000, 0xf7ff, MRA_ROM },
	{ 0xf800, 0xffff, sharedram_r },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress mcu_no_nvram_readmem[] =
{
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0x1003, 0x1003, input_port_3_r },	/* DSW1 */
	{ 0x1005, 0x1005, input_port_4_r },	/* DSW2 */
	{ 0x1006, 0x1006, input_port_5_r },	/* DSW3 */
	{ 0x8000, 0xf7ff, MRA_ROM },
	{ 0xf800, 0xffff, sharedram_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress mcu_nvram_writemem[] =
{
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0x1000, 0x10ff, MWA_RAM, &nvram, &nvram_size },
	{ 0x2000, 0x2000, DAC_0_data_w },
	{ 0x8000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, sharedram_w },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress mcu_no_nvram_writemem[] =
{
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0x2000, 0x2000, DAC_0_data_w },
	{ 0x8000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, sharedram_w },
	{ -1 }	/* end of table */
};




INPUT_PORTS_START( friskyt )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_SERVICE( 0x20, IP_ACTIVE_HIGH )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x80, 0x00, "Counter Check" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* Test */
	PORT_DIPNAME( 0x01, 0x00, "Test Mode" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Connection Error" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
INPUT_PORTS_END

INPUT_PORTS_START( radrad )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* Test */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x06, "5" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x05, "5" )
	PORT_DIPSETTING(    0x06, "6" )
	PORT_DIPSETTING(    0x07, "7" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x0f, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 2C_8C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_8C ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( seicross )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* Test */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Connection Error" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_6C ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* Debug */
	PORT_BIT( 0x1f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x20, 0x20, "Debug Mode" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes are packed in one byte */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes are packed in one byte */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 17*8+0, 17*8+1, 17*8+2, 17*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 16 },
	{ REGION_GFX1, 0, &spritelayout, 0, 16 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	1536000,	/* 1.536 MHz ?? */
	{ 25 },
	{ 0 },
	{ friskyt_portB_r },
	{ 0 },
	{ friskyt_portB_w }
};

static struct DACinterface dac_interface =
{
	1,
	{ 25 }
};


#define MACHINE_DRIVER(NAME,NVRAM)														\
static struct MachineDriver machine_driver_##NAME =										\
{																						\
	/* basic machine hardware */														\
	{																					\
		{																				\
			CPU_Z80,																	\
			3072000,	/* 3.072 MHz? */												\
			readmem,writemem,readport,writeport,										\
			interrupt,1																	\
		},																				\
		{																				\
			CPU_NSC8105,																\
			6000000/4,	/* ??? */														\
			mcu_##NAME##_readmem,mcu_##NAME##_writemem,0,0,								\
			ignore_interrupt,0															\
		}																				\
	},																					\
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */	\
	20,	/* 20 CPU slices per frame - an high value to ensure proper */					\
			/* synchronization of the CPUs */											\
	friskyt_init_machine,																\
																						\
	/* video hardware */																\
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },											\
	gfxdecodeinfo,																		\
	64, 64,																				\
	seicross_vh_convert_color_prom,														\
																						\
	VIDEO_TYPE_RASTER,																	\
	0,																					\
	generic_vh_start,																	\
	generic_vh_stop,																	\
	seicross_vh_screenrefresh,															\
																						\
	/* sound hardware */																\
	0,0,0,0,																			\
	{																					\
		{																				\
			SOUND_AY8910,																\
			&ay8910_interface															\
		},																				\
		{																				\
			SOUND_DAC,																	\
			&dac_interface																\
		}																				\
	},																					\
																						\
	NVRAM																				\
};


MACHINE_DRIVER(nvram,nvram_handler)
MACHINE_DRIVER(no_nvram,0)


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( friskyt )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ftom.01",      0x0000, 0x1000, 0xbce5d486 )
	ROM_LOAD( "ftom.02",      0x1000, 0x1000, 0x63157d6e )
	ROM_LOAD( "ftom.03",      0x2000, 0x1000, 0xc8d9ef2c )
	ROM_LOAD( "ftom.04",      0x3000, 0x1000, 0x23a01aac )
	ROM_LOAD( "ftom.05",      0x4000, 0x1000, 0xbfaf702a )
	ROM_LOAD( "ftom.06",      0x5000, 0x1000, 0xbce70b9c )
	ROM_LOAD( "ftom.07",      0x6000, 0x1000, 0xb2ef303a )
	ROM_LOAD( "ft8_8.rom",    0x7000, 0x0800, 0x10461a24 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the protection mcu */
	/* filled in later */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ftom.11",      0x0000, 0x1000, 0x1ec6ff65 )
	ROM_LOAD( "ftom.12",      0x1000, 0x1000, 0x3b8f40b5 )
	ROM_LOAD( "ftom.09",      0x2000, 0x1000, 0x60642f25 )
	ROM_LOAD( "ftom.10",      0x3000, 0x1000, 0x07b9dcfc )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "ft.9c",        0x0000, 0x0020, 0x0032167e )
	ROM_LOAD( "ft.9b",        0x0020, 0x0020, 0x6b364e69 )
ROM_END

ROM_START( radrad )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1.3a",         0x0000, 0x1000, 0xb1e958ca )
	ROM_LOAD( "2.3b",         0x1000, 0x1000, 0x30ba76b3 )
	ROM_LOAD( "3.3c",         0x2000, 0x1000, 0x1c9f397b )
	ROM_LOAD( "4.3d",         0x3000, 0x1000, 0x453966a3 )
	ROM_LOAD( "5.3e",         0x4000, 0x1000, 0xc337c4bd )
	ROM_LOAD( "6.3f",         0x5000, 0x1000, 0x06e15b59 )
	ROM_LOAD( "7.3g",         0x6000, 0x1000, 0x02b1f9c9 )
	ROM_LOAD( "8.3h",         0x7000, 0x0800, 0x911c90e8 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the protection mcu */
	/* filled in later */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11.l7",        0x0000, 0x1000, 0x4ace7afb )
	ROM_LOAD( "12.n7",        0x1000, 0x1000, 0xb19b8473 )
	ROM_LOAD( "9.j7",         0x2000, 0x1000, 0x229939a3 )
	ROM_LOAD( "10.j7",        0x3000, 0x1000, 0x79237913 )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "clr.9c",       0x0000, 0x0020, 0xc9d88422 )
	ROM_LOAD( "clr.9b",       0x0020, 0x0020, 0xee81af16 )
ROM_END

ROM_START( seicross )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "smc1",         0x0000, 0x1000, 0xf6c3aeca )
	ROM_LOAD( "smc2",         0x1000, 0x1000, 0x0ec6c218 )
	ROM_LOAD( "smc3",         0x2000, 0x1000, 0xceb3c8f4 )
	ROM_LOAD( "smc4",         0x3000, 0x1000, 0x3112af59 )
	ROM_LOAD( "smc5",         0x4000, 0x1000, 0xb494a993 )
	ROM_LOAD( "smc6",         0x5000, 0x1000, 0x09d5b9da )
	ROM_LOAD( "smc7",         0x6000, 0x1000, 0x13052b03 )
	ROM_LOAD( "smc8",         0x7000, 0x0800, 0x2093461d )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the protection mcu */
	/* filled in later */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sz11.7k",      0x0000, 0x1000, 0xfbd9b91d )
	ROM_LOAD( "smcd",         0x1000, 0x1000, 0xc3c953c4 )
	ROM_LOAD( "sz9.7j",       0x2000, 0x1000, 0x4819f0cd )
	ROM_LOAD( "sz10.7h",      0x3000, 0x1000, 0x4c268778 )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "sz73.10c",     0x0000, 0x0020, 0x4d218a3c )
	ROM_LOAD( "sz74.10b",     0x0020, 0x0020, 0xc550531c )
ROM_END

ROM_START( sectrzon )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "sz1.3a",       0x0000, 0x1000, 0xf0a45cb4 )
	ROM_LOAD( "sz2.3c",       0x1000, 0x1000, 0xfea68ddb )
	ROM_LOAD( "sz3.3d",       0x2000, 0x1000, 0xbaad4294 )
	ROM_LOAD( "sz4.3e",       0x3000, 0x1000, 0x75f2ca75 )
	ROM_LOAD( "sz5.3fg",      0x4000, 0x1000, 0xdc14f2c8 )
	ROM_LOAD( "sz6.3h",       0x5000, 0x1000, 0x397a38c5 )
	ROM_LOAD( "sz7.3i",       0x6000, 0x1000, 0x7b34dc1c )
	ROM_LOAD( "sz8.3j",       0x7000, 0x0800, 0x9933526a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the protection mcu */
	/* filled in later */

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sz11.7k",      0x0000, 0x1000, 0xfbd9b91d )
	ROM_LOAD( "sz12.7m",      0x1000, 0x1000, 0x2bdef9ad )
	ROM_LOAD( "sz9.7j",       0x2000, 0x1000, 0x4819f0cd )
	ROM_LOAD( "sz10.7h",      0x3000, 0x1000, 0x4c268778 )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "sz73.10c",     0x0000, 0x0020, 0x4d218a3c )
	ROM_LOAD( "sz74.10b",     0x0020, 0x0020, 0xc550531c )
ROM_END



static void init_friskyt(void)
{
	int A;
	unsigned char *src,*dest;

	/* the protection mcu shares the main program ROMs and RAM with the main CPU. */

	/* copy over the ROMs */
	src = memory_region(REGION_CPU1);
	dest = memory_region(REGION_CPU2);
	for (A = 0;A < 0x8000;A++)
		 dest[A + 0x8000] = src[A];
}



GAMEX( 1981, friskyt,  0,        nvram,    friskyt,  friskyt, ROT0,  "Nichibutsu", "Frisky Tom", GAME_NO_COCKTAIL )
GAMEX( 1982, radrad,   0,        no_nvram, radrad,   friskyt, ROT0,  "Nichibutsu USA", "Radical Radial", GAME_NO_COCKTAIL )
GAMEX( 1984, seicross, 0,        no_nvram, seicross, friskyt, ROT90, "Nichibutsu + Alice", "Seicross", GAME_NO_COCKTAIL )
GAMEX( 1984, sectrzon, seicross, no_nvram, seicross, friskyt, ROT90, "Nichibutsu + Alice", "Sector Zone", GAME_NO_COCKTAIL )
