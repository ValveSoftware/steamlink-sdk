#include "../vidhrdw/popeye.cpp"

/***************************************************************************

Popeye memory map (preliminary)

driver by Marc Lafontaine

0000-7fff  ROM

8000-87ff  RAM
8c00       background x position
8c01       background y position
8c02       ?
8c03       bit 3: background palette bank
           bit 0-2: sprite palette bank
8c04-8e7f  sprites
8f00-8fff  RAM (stack)

a000-a3ff  Text video ram
a400-a7ff  Text Attribute

c000-cfff  Background bitmap. Accessed as nibbles: bit 7 selects which of
           the two nibbles should be written to.


I/O 0  ;AY-3-8910 Control Reg.
I/O 1  ;AY-3-8910 Data Write Reg.
        write to port B: select bit of DSW2 to read in bit 7 of port A (0-2-4-6-8-a-c-e)
I/O 3  ;AY-3-8910 Data Read Reg.
        read from port A: bit 0-5 = DSW1  bit 7 = bit of DSW1 selected by port B

        DSW1
		bit 0-3 = coins per play (0 = free play)
		bit 4-5 = ?

		DSW2
		bit 0-1 = lives
		bit 2-3 = difficulty
		bit 4-5 = bonus
		bit 6 = demo sounds
		bit 7 = cocktail/upright (0 = upright)

I/O 2  ;bit 0 Coin in 1
        bit 1 Coin in 2
        bit 2 Coin in 3 = 5 credits
        bit 3
        bit 4 Start 2 player game
        bit 5 Start 1 player game
        bit 6
        bit 7

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *popeye_videoram;
extern size_t popeye_videoram_size;
extern unsigned char *popeye_background_pos,*popeye_palette_bank;
WRITE_HANDLER( popeye_backgroundram_w );
WRITE_HANDLER( popeye_videoram_w );
WRITE_HANDLER( popeye_colorram_w );
WRITE_HANDLER( popeye_palettebank_w );
void popeye_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void popeyebl_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void popeye_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int  popeye_vh_start(void);
void popeye_vh_stop(void);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8c00, 0x8e7f, MRA_RAM },
	{ 0x8f00, 0x8fff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8c04, 0x8e7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x8f00, 0x8fff, MWA_RAM },
	{ 0xa000, 0xa3ff, videoram_w, &videoram, &videoram_size },
	{ 0xa400, 0xa7ff, colorram_w, &colorram },
	{ 0xc000, 0xcfff, popeye_videoram_w, &popeye_videoram, &popeye_videoram_size },
	{ 0x8c00, 0x8c01, MWA_RAM, &popeye_background_pos },
	{ 0x8c03, 0x8c03, popeye_palettebank_w, &popeye_palette_bank },
	{ -1 }	/* end of table */
};



static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ 0x02, 0x02, input_port_2_r },
	{ 0x03, 0x03, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( popeye )

	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
/*  0x0e = 2 Coins/1 Credit
    0x07, 0x0c = 1 Coin/1 Credit
    0x01, 0x0b, 0x0d = 1 Coin/2 Credits */
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 7 scans DSW1 one bit at a time */

	PORT_START	/* DSW1 (FAKE - appears as bit 7 of DSW0, see code below) */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "40000" )
	PORT_DIPSETTING(    0x20, "60000" )
	PORT_DIPSETTING(    0x10, "80000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	16,16,	/* 16*16 characters (8*8 doubled) */
	256,	/* 256 characters */
	1,	/* 1 bit per pixel (there are two bitplanes in the ROM, but only one is used) */
	{ 0 },
	{ 7,7, 6,6, 5,5, 4,4, 3,3, 2,2, 1,1, 0,0 },	/* pretty straightforward layout */
	{ 0*8,0*8, 1*8,1*8, 2*8,2*8, 3*8,3*8, 4*8,4*8, 5*8,5*8, 6*8,6*8, 7*8,7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	512,	/* 512 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 0x4000*8 },	/* the two bitplanes are separated in different files */
	{7+(0x2000*8),6+(0x2000*8),5+(0x2000*8),4+(0x2000*8),
     3+(0x2000*8),2+(0x2000*8),1+(0x2000*8),0+(0x2000*8),
   7,6,5,4,3,2,1,0 },
	{ 15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8,
    7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8, },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,      0, 16 },	/* chars */
	{ REGION_GFX2, 0x0000, &spritelayout, 16*2, 64 },	/* sprites */
	{ -1 } /* end of array */
};



static int dswbit;

static WRITE_HANDLER( popeye_portB_w )
{
	/* bit 0 does something - RV in the schematics */

	/* bits 1-3 select DSW1 bit to read */
	dswbit = (data & 0x0e) >> 1;
}

static READ_HANDLER( popeye_portA_r )
{
	int res;


	res = input_port_3_r(offset);
	res |= (input_port_4_r(offset) << (7-dswbit)) & 0x80;

	return res;
}

static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	2000000,	/* 2 MHz */
	{ 40 },
	{ popeye_portA_r },
	{ 0 },
	{ 0 },
	{ popeye_portB_w }
};



static struct MachineDriver machine_driver_popeyebl =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz */
			readmem,writemem,readport,writeport,
			nmi_interrupt,2
		}
	},
	30, DEFAULT_30HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*16, 30*16, { 0*16, 32*16-1, 1*16, 29*16-1 },
	gfxdecodeinfo,
	32+16+256, 16*2+64*4,
	popeyebl_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	popeye_vh_start,
	popeye_vh_stop,
	popeye_vh_screenrefresh,

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

ROM_START( popeye )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "c-7a",         0x0000, 0x2000, 0x9af7c821 )
	ROM_LOAD( "c-7b",         0x2000, 0x2000, 0xc3704958 )
	ROM_LOAD( "c-7c",         0x4000, 0x2000, 0x5882ebf9 )
	ROM_LOAD( "c-7e",         0x6000, 0x2000, 0xef8649ca )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "v-5n",         0x0000, 0x0800, 0xcca61ddd )	/* first half is empty */
	ROM_CONTINUE(             0x0000, 0x0800 )

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "v-1e",         0x0000, 0x2000, 0x0f2cd853 )
	ROM_LOAD( "v-1f",         0x2000, 0x2000, 0x888f3474 )
	ROM_LOAD( "v-1j",         0x4000, 0x2000, 0x7e864668 )
	ROM_LOAD( "v-1k",         0x6000, 0x2000, 0x49e1d170 )

	ROM_REGION( 0x0340, REGION_PROMS )
	ROM_LOAD( "prom-cpu.4a",  0x0000, 0x0020, 0x375e1602 ) /* background palette */
	ROM_LOAD( "prom-cpu.3a",  0x0020, 0x0020, 0xe950bea1 ) /* char palette */
	ROM_LOAD( "prom-cpu.5b",  0x0040, 0x0100, 0xc5826883 ) /* sprite palette - low 4 bits */
	ROM_LOAD( "prom-cpu.5a",  0x0140, 0x0100, 0xc576afba ) /* sprite palette - high 4 bits */
	ROM_LOAD( "prom-vid.7j",  0x0240, 0x0100, 0xa4655e2e ) /* timing for the protection ALU */
ROM_END

ROM_START( popeye2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "7a",           0x0000, 0x2000, 0x0bd04389 )
	ROM_LOAD( "7b",           0x2000, 0x2000, 0xefdf02c3 )
	ROM_LOAD( "7c",           0x4000, 0x2000, 0x8eee859e )
	ROM_LOAD( "7e",           0x6000, 0x2000, 0xb64aa314 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "v-5n",         0x0000, 0x0800, 0xcca61ddd )	/* first half is empty */
	ROM_CONTINUE(             0x0000, 0x0800 )

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "v-1e",         0x0000, 0x2000, 0x0f2cd853 )
	ROM_LOAD( "v-1f",         0x2000, 0x2000, 0x888f3474 )
	ROM_LOAD( "v-1j",         0x4000, 0x2000, 0x7e864668 )
	ROM_LOAD( "v-1k",         0x6000, 0x2000, 0x49e1d170 )

	ROM_REGION( 0x0340, REGION_PROMS )
	ROM_LOAD( "prom-cpu.4a",  0x0000, 0x0020, 0x375e1602 ) /* background palette */
	ROM_LOAD( "prom-cpu.3a",  0x0020, 0x0020, 0xe950bea1 ) /* char palette */
	ROM_LOAD( "prom-cpu.5b",  0x0040, 0x0100, 0xc5826883 ) /* sprite palette - low 4 bits */
	ROM_LOAD( "prom-cpu.5a",  0x0140, 0x0100, 0xc576afba ) /* sprite palette - high 4 bits */
	ROM_LOAD( "prom-vid.7j",  0x0240, 0x0100, 0xa4655e2e ) /* timing for the protection ALU */
ROM_END

ROM_START( popeyebl )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "po1",          0x0000, 0x2000, 0xb14a07ca )
	ROM_LOAD( "po2",          0x2000, 0x2000, 0x995475ff )
	ROM_LOAD( "po3",          0x4000, 0x2000, 0x99d6a04a )
	ROM_LOAD( "po4",          0x6000, 0x2000, 0x548a6514 )
	ROM_LOAD( "po_d1-e1.bin", 0xe000, 0x0020, 0x8de22998 )	/* protection PROM */

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "v-5n",         0x0000, 0x0800, 0xcca61ddd )	/* first half is empty */
	ROM_CONTINUE(             0x0000, 0x0800 )

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "v-1e",         0x0000, 0x2000, 0x0f2cd853 )
	ROM_LOAD( "v-1f",         0x2000, 0x2000, 0x888f3474 )
	ROM_LOAD( "v-1j",         0x4000, 0x2000, 0x7e864668 )
	ROM_LOAD( "v-1k",         0x6000, 0x2000, 0x49e1d170 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "popeye.pr1",   0x0000, 0x0020, 0xd138e8a4 ) /* background palette */
	ROM_LOAD( "popeye.pr2",   0x0020, 0x0020, 0x0f364007 ) /* char palette */
	ROM_LOAD( "popeye.pr3",   0x0040, 0x0100, 0xca4d7b6a ) /* sprite palette - low 4 bits */
	ROM_LOAD( "popeye.pr4",   0x0140, 0x0100, 0xcab9bc53 ) /* sprite palette - high 4 bits */
ROM_END



/* The original doesn't work because the ROMs are encrypted. */
/* The encryption is based on a custom ALU and seems to be dynamically evolving */
/* (like Jr. PacMan). I think it decodes 16 bits at a time, bits 0-2 are (or can be) */
/* an opcode for the ALU and the others contain the data. */
GAMEX( 1982?, popeye,   0,      popeyebl, popeye, 0, ROT0, "Nintendo", "Popeye (set 1)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1982?, popeye2,  popeye, popeyebl, popeye, 0, ROT0, "Nintendo", "Popeye (set 2)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1982?, popeyebl, popeye, popeyebl, popeye, 0, ROT0, "bootleg", "Popeye (bootleg)", GAME_NO_COCKTAIL )
