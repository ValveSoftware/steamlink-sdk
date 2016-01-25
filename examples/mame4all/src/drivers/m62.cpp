#include "../vidhrdw/m62.cpp"

/****************************************************************************

Irem "M62" system

There's two crystals on Kid Kiki. 24.00 MHz and 3.579545 MHz for sound

TODO:
- Kid Niki is missing the drums

**************************************************************************/

#include "driver.h"
#include "sndhrdw/irem.h"
#include "vidhrdw/generic.h"


void irem_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void battroad_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void spelunk2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int ldrun_vh_start( void );
int kidniki_vh_start( void );
int spelunkr_vh_start( void );
WRITE_HANDLER( irem_flipscreen_w );
WRITE_HANDLER( kungfum_scroll_low_w );
WRITE_HANDLER( kungfum_scroll_high_w );
WRITE_HANDLER( ldrun3_vscroll_w );
WRITE_HANDLER( ldrun4_hscroll_w );
WRITE_HANDLER( irem_background_hscroll_w );
WRITE_HANDLER( irem_background_vscroll_w );
WRITE_HANDLER( battroad_scroll_w );
WRITE_HANDLER( kidniki_text_vscroll_w );
WRITE_HANDLER( kidniki_background_bank_w );
WRITE_HANDLER( spelunkr_palbank_w );
WRITE_HANDLER( spelunk2_gfxport_w );
void kungfum_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);
void battroad_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);
void ldrun_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void ldrun4_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void lotlot_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void kidniki_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);
void spelunkr_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);
void spelunk2_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);

extern unsigned char *irem_textram;
extern size_t irem_textram_size;



/* Lode Runner 2 seems to have a simple protection on the bank switching */
/* circuitry. It writes data to ports 0x80 and 0x81, then reads port 0x80 */
/* a variable number of times (discarding the result) and finally retrieves */
/* data from the bankswitched ROM area. */
/* Since the data written to 0x80 is always the level number, I just use */
/* that to select the ROM. The only exception I make is a special case used in */
/* service mode to test the ROMs. */
static int ldrun2_bankswap;

READ_HANDLER( ldrun2_bankswitch_r )
{
	if (ldrun2_bankswap)
	{
		unsigned char *RAM = memory_region(REGION_CPU1);


		ldrun2_bankswap--;

		/* swap to bank #1 on second read */
		if (ldrun2_bankswap == 0)
			cpu_setbank(1,&RAM[0x12000]);
	}
	return 0;
}

WRITE_HANDLER( ldrun2_bankswitch_w )
{
	int bankaddress;
	static int bankcontrol[2];
	int banks[30] =
	{
		0,0,0,0,0,1,0,1,0,0,
		0,1,1,1,1,1,0,0,0,0,
		1,0,1,1,1,1,1,1,1,1
	};
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankcontrol[offset] = data;
	if (offset == 0)
	{
		if (data < 1 || data > 30)
		{
//logerror("unknown bank select %02x\n",data);
			return;
		}
		bankaddress = 0x10000 + (banks[data-1] * 0x2000);
		cpu_setbank(1,&RAM[bankaddress]);
	}
	else
	{
		if (bankcontrol[0] == 0x01 && data == 0x0d)
		/* special case for service mode */
			ldrun2_bankswap = 2;
		else ldrun2_bankswap = 0;
	}
}


/* Lode Runner 3 has, it seems, a poor man's protection consisting of a PAL */
/* (I think; it's included in the ROM set) which is read at certain times, */
/* and the game crashes if ti doesn't match the expected values. */
READ_HANDLER( ldrun3_prot_5_r )
{
	return 5;
}

READ_HANDLER( ldrun3_prot_7_r )
{
	return 7;
}


WRITE_HANDLER( ldrun4_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + ((data & 0x01) * 0x4000);
	cpu_setbank(1,&RAM[bankaddress]);
}

static WRITE_HANDLER( kidniki_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + (data & 0x0f) * 0x2000;
	cpu_setbank(1,&RAM[bankaddress]);
}

#define battroad_bankswitch_w kidniki_bankswitch_w

static WRITE_HANDLER( spelunkr_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + (data & 0x03) * 0x2000;
	cpu_setbank(1,&RAM[bankaddress]);
}

WRITE_HANDLER( spelunk2_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);


	cpu_setbank(1,&RAM[0x20000 + 0x1000 * ((data & 0xc0)>>6)]);
	cpu_setbank(2,&RAM[0x10000 + 0x0400 *  (data & 0x3c)]);
}




static struct MemoryWriteAddress kungfum_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xa000, 0xa000, kungfum_scroll_low_w },
	{ 0xb000, 0xb000, kungfum_scroll_high_w },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	/* Kung Fu Master is the only game in this driver to have separated (but */
	/* contiguous) videoram and colorram. They are interleaved in all the others. */
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress battroad_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xa000, 0xbfff, MRA_BANK1 },
	{ 0xc800, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress battroad_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc800, 0xcfff, MWA_RAM, &irem_textram, &irem_textram_size },
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress ldrun_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xd000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ldrun_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress ldrun2_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK1 },
	{ 0xd000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ldrun2_writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress ldrun3_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc800, 0xc800, ldrun3_prot_5_r },
	{ 0xcc00, 0xcc00, ldrun3_prot_7_r },
	{ 0xcfff, 0xcfff, ldrun3_prot_7_r },
	{ 0xd000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ldrun3_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress ldrun4_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xd000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ldrun4_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc800, 0xc800, ldrun4_bankswitch_w },
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress lotlot_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xa000, 0xafff, MRA_RAM },
	{ 0xd000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress lotlot_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xa000, 0xafff, MWA_RAM, &irem_textram, &irem_textram_size },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xdfff, videoram_w, &videoram, &videoram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress kidniki_readmem[] = {
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK1 },
	{ 0xa000, 0xafff, MRA_RAM },
	{ 0xd000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress kidniki_writemem[] = {
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xafff, videoram_w, &videoram, &videoram_size },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xdfff, MWA_RAM, &irem_textram, &irem_textram_size },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress spelunkr_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK1 },
	{ 0xa000, 0xbfff, MRA_RAM },
	{ 0xc800, 0xcfff, MRA_RAM },
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress spelunkr_writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xbfff, videoram_w, &videoram, &videoram_size },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc800, 0xcfff, MWA_RAM, &irem_textram, &irem_textram_size },
	{ 0xd000, 0xd001, irem_background_vscroll_w },
	{ 0xd002, 0xd003, irem_background_hscroll_w },
	{ 0xd004, 0xd004, spelunkr_bankswitch_w },
	{ 0xd005, 0xd005, spelunkr_palbank_w },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress spelunk2_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_BANK1 },
	{ 0x9000, 0x9fff, MRA_BANK2 },
	{ 0xa000, 0xbfff, MRA_RAM },
	{ 0xc800, 0xcfff, MRA_RAM },
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress spelunk2_writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xbfff, videoram_w, &videoram, &videoram_size },
	{ 0xc000, 0xc0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc800, 0xcfff, MWA_RAM, &irem_textram, &irem_textram_size },
	{ 0xd000, 0xd002, spelunk2_gfxport_w },
	{ 0xd003, 0xd003, spelunk2_bankswitch_w },
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};


static struct IOReadPort ldrun_readport[] =
{
	{ 0x00, 0x00, input_port_0_r },   /* coin */
	{ 0x01, 0x01, input_port_1_r },   /* player 1 control */
	{ 0x02, 0x02, input_port_2_r },   /* player 2 control */
	{ 0x03, 0x03, input_port_3_r },   /* DSW 1 */
	{ 0x04, 0x04, input_port_4_r },   /* DSW 2 */
	{ -1 }	/* end of table */
};

static struct IOWritePort battroad_writeport[] =
{
	{ 0x00, 0x00, irem_sound_cmd_w },
	{ 0x01, 0x01, irem_flipscreen_w },	/* + coin counters */
	{ 0x80, 0x82, battroad_scroll_w },
	{ 0x83, 0x83, battroad_bankswitch_w },
	{ -1 }	/* end of table */
};

static struct IOWritePort ldrun_writeport[] =
{
	{ 0x00, 0x00, irem_sound_cmd_w },
	{ 0x01, 0x01, irem_flipscreen_w },	/* + coin counters */
	{ -1 }	/* end of table */
};

static struct IOReadPort ldrun2_readport[] =
{
	{ 0x00, 0x00, input_port_0_r },   /* coin */
	{ 0x01, 0x01, input_port_1_r },   /* player 1 control */
	{ 0x02, 0x02, input_port_2_r },   /* player 2 control */
	{ 0x03, 0x03, input_port_3_r },   /* DSW 1 */
	{ 0x04, 0x04, input_port_4_r },   /* DSW 2 */
	{ 0x80, 0x80, ldrun2_bankswitch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort ldrun2_writeport[] =
{
	{ 0x00, 0x00, irem_sound_cmd_w },
	{ 0x01, 0x01, irem_flipscreen_w },	/* + coin counters */
	{ 0x80, 0x81, ldrun2_bankswitch_w },
	{ -1 }	/* end of table */
};

static struct IOWritePort ldrun3_writeport[] =
{
	{ 0x00, 0x00, irem_sound_cmd_w },
	{ 0x01, 0x01, irem_flipscreen_w },	/* + coin counters */
	{ 0x80, 0x80, ldrun3_vscroll_w },
	/* 0x81 used too, don't know what for */
	{ -1 }	/* end of table */
};

static struct IOWritePort ldrun4_writeport[] =
{
	{ 0x00, 0x00, irem_sound_cmd_w },
	{ 0x01, 0x01, irem_flipscreen_w },	/* + coin counters */
	{ 0x82, 0x83, ldrun4_hscroll_w },
	{ -1 }	/* end of table */
};

static struct IOWritePort kidniki_writeport[] =
{
	{ 0x00, 0x00, irem_sound_cmd_w },
	{ 0x01, 0x01, irem_flipscreen_w },	/* + coin counters */
	{ 0x80, 0x81, irem_background_hscroll_w },
	{ 0x82, 0x83, kidniki_text_vscroll_w },
	{ 0x84, 0x84, kidniki_background_bank_w },
	{ 0x85, 0x85, kidniki_bankswitch_w },
	{ -1 }	/* end of table */
};



#define IN0_PORT \
/* Start 1 & 2 also restarts and freezes the game with stop mode on \
   and are used in test mode to enter and esc the various tests */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 ) \
	/* service coin must be active for 19 frames to be consistently recognized */ \
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1, 19 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

#define IN1_PORT \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* probably unused */ \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* probably unused */ \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

#define IN2_PORT \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* probably unused */ \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

#define	COINAGE_DSW \
	/* TODO: support the different settings which happen in Coin Mode 2 */ \
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) ) /* mapped on coin mode 1 */ \
	PORT_DIPSETTING(    0x90, DEF_STR( 7C_1C ) ) \
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) ) \
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_7C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_8C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) ) \
	/* setting 0x80 give 1 Coin/1 Credit */


#define	COINAGE2_DSW \
	/* TODO: support the different settings which happen in Coin Mode 2 */ \
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) ) /* mapped on coin mode 1 */ \
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) ) \
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 8C_3C ) ) \
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 5C_3C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) ) \
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) ) \


INPUT_PORTS_START( kungfum )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x02, 0x02, "Energy Loss" )
	PORT_DIPSETTING(    0x02, "Slow" )
	PORT_DIPSETTING(    0x00, "Fast" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	/* In slowmo mode, press 2 to slow game speed */
	PORT_BITX   ( 0x08, 0x08, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Slow Motion Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In freeze mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Freeze", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In level selection mode, press 1 to select and 2 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Selection Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( battroad )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Fuel Decrease" )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	COINAGE_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( ldrun )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Timer" )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In level selection mode, press 1 to select and 2 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Selection Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( ldrun2 )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Timer" )
	PORT_DIPSETTING(    0x01, "Slow" )
	PORT_DIPSETTING(    0x00, "Fast" )
	PORT_DIPNAME( 0x02, 0x02, "Game Speed" )
	PORT_DIPSETTING(    0x00, "Low" )
	PORT_DIPSETTING(    0x02, "High" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In freeze mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Freeze", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In level selection mode, press 1 to select and 2 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Selection Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( ldrun3 )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Timer" )
	PORT_DIPSETTING(    0x01, "Slow" )
	PORT_DIPSETTING(    0x00, "Fast" )
	PORT_DIPNAME( 0x02, 0x02, "Game Speed" )
	PORT_DIPSETTING(    0x00, "Low" )
	PORT_DIPSETTING(    0x02, "High" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In level selection mode, press 1 to select and 2 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Selection Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( ldrun4 )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Timer" )
	PORT_DIPSETTING(    0x01, "Slow" )
	PORT_DIPSETTING(    0x00, "Fast" )
	PORT_DIPNAME( 0x02, 0x02, "2 Players Game" )
	PORT_DIPSETTING(    0x00, "1 Credit" )
	PORT_DIPSETTING(    0x02, "2 Credits" )
	PORT_DIPNAME( 0x0c, 0x0c, "1 Player Lives" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "2 Players Lives" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x00, "6" )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow 2 Players Game" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In level selection mode, press 1 to select and 2 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Selection Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode (must set 2P game to No)", KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( lotlot )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Speed" )
	PORT_DIPSETTING(    0x03, "Very Slow" )
	PORT_DIPSETTING(    0x02, "Slow" )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Very Fast" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	COINAGE2_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In freeze mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Freeze", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( kidniki )
	PORT_START
	IN0_PORT

	PORT_START
	IN1_PORT

	PORT_START
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "50000" )
	PORT_DIPSETTING(    0x00, "80000" )
	COINAGE2_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, "Game Repeats" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In freeze mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Freeze", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( spelunkr )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Energy Decrease" )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE2_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	/* In teleport mode, keep 1 pressed and press up or down to move the character */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Teleport", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In freeze mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Freeze", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( spelunk2 )
	PORT_START	/* IN0 */
	IN0_PORT

	PORT_START	/* IN1 */
	IN1_PORT

	PORT_START	/* IN2 */
	IN2_PORT

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Energy Decrease" )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	COINAGE2_DSW

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In freeze mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Freeze", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



#define TILELAYOUT(NUM) static struct GfxLayout tilelayout_##NUM =  \
{                                                                   \
	8,8,	/* 8*8 characters */                                    \
	NUM,	/* NUM characters */                                    \
	3,	/* 3 bits per pixel */                                      \
	{ 2*NUM*8*8, NUM*8*8, 0 },                                      \
	{ 0, 1, 2, 3, 4, 5, 6, 7 },                                     \
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },                     \
	8*8	/* every char takes 8 consecutive bytes */                  \
}

TILELAYOUT(1024);
TILELAYOUT(2048);
TILELAYOUT(4096);


static struct GfxLayout battroad_charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* number of characters */
	2,	/* 2 bits per pixel */
	{ 0, 1024*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout lotlot_charlayout =
{
	12,10, /* character size */
	256, /* number of characters */
	3, /* bits per pixel */
	{ 0, 256*32*8, 2*256*32*8 },
	{ 0, 1, 2, 3, 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout kidniki_charlayout =
{
	12,8, /* character size */
	1024, /* number of characters */
	3, /* bits per pixel */
	{ 0, 0x4000*8, 2*0x4000*8 },
	{ 0, 1, 2, 3, 64+0,64+1,64+2,64+3,64+4,64+5,64+6,64+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spelunk2_charlayout =
{
	12,8, /* character size */
	512, /* number of characters */
	3, /* bits per pixel */
	{ 0, 0x4000*8, 2*0x4000*8 },
	{
		0,1,2,3,
		0x2000*8+0,0x2000*8+1,0x2000*8+2,0x2000*8+3,
		0x2000*8+4,0x2000*8+5,0x2000*8+6,0x2000*8+7
	},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

#define SPRITELAYOUT(NUM) static struct GfxLayout spritelayout_##NUM =         \
{                                                                              \
	16,16,	/* 16*16 sprites */                                                \
	NUM,	/* NUM sprites */                                                  \
	3,	/* 3 bits per pixel */                                                 \
	{ 2*NUM*32*8, NUM*32*8, 0 },                                               \
	{ 0, 1, 2, 3, 4, 5, 6, 7,                                                  \
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },  \
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,                                  \
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },                    \
	32*8	/* every sprite takes 32 consecutive bytes */                      \
}

SPRITELAYOUT(256);
SPRITELAYOUT(512);
SPRITELAYOUT(1024);
SPRITELAYOUT(2048);


static struct GfxDecodeInfo kungfum_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_1024,       0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_1024,  32*8, 32 },	/* use colors 256-511 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo battroad_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_1024,       0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_512,	256, 32 },	/* use colors 256-511 */
	{ REGION_GFX3, 0, &battroad_charlayout,	512, 32 },	/* use colors 512-543 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ldrun_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_1024,      0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_256,   256, 32 },	/* use colors 256-511 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ldrun2_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_1024,      0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_512,   256, 32 },	/* use colors 256-511 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ldrun3_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_2048,      0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_512,   256, 32 },	/* use colors 256-511 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ldrun4_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_2048,      0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_1024,  256, 32 },	/* use colors 256-511 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo lotlot_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &lotlot_charlayout,    0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_256,   256, 32 },	/* use colors 256-511 */
	{ REGION_GFX3, 0, &lotlot_charlayout,  512, 32 },	/* use colors 512-767 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo kidniki_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_4096,      0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_2048,  256, 32 },	/* use colors 256-511 */
	{ REGION_GFX3, 0, &kidniki_charlayout,   0, 32 },	/* use colors   0-255 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo spelunkr_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_4096,	     0, 32 },	/* use colors   0-255 */
	{ REGION_GFX2, 0, &spritelayout_1024,  256, 32 },	/* use colors 256-511 */
	{ REGION_GFX3, 0, &spelunk2_charlayout,  0, 32 },	/* use colors   0-255 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo spelunk2_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tilelayout_4096,	     0, 64 },	/* use colors   0-511 */
	{ REGION_GFX2, 0, &spritelayout_1024,  512, 32 },	/* use colors 512-767 */
	{ REGION_GFX3, 0, &spelunk2_charlayout,  0, 64 },	/* use colors   0-511 */
	{ -1 } /* end of array */
};



#define MACHINE_DRIVER(GAMENAME,READPORT,VISIBLEMINX,VISIBLEMAXX,COLORS,CONVERTCOLOR)        \
                                                                                             \
static struct MachineDriver machine_driver_##GAMENAME =                                      \
{                                                                                            \
	/* basic machine hardware */                                                             \
	{                                                                                        \
		{                                                                                    \
			CPU_Z80,                                                                         \
			4000000,	/* 4 Mhz (?) */                                                      \
			GAMENAME##_readmem,GAMENAME##_writemem,READPORT##_readport,GAMENAME##_writeport, \
			interrupt,1                                                                      \
		},                                                                                   \
		IREM_AUDIO_CPU                                                                       \
	},                                                                                       \
	55, 1790, /* frames per second and vblank duration from the Lode Runner manual */        \
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */ \
	0,                                                                                       \
                                                                                             \
	/* video hardware */                                                                     \
	64*8, 32*8, { VISIBLEMINX, VISIBLEMAXX, 0*8, 32*8-1 },                                   \
	GAMENAME##_gfxdecodeinfo,                                                                \
	COLORS, COLORS,                                                                          \
	CONVERTCOLOR##_vh_convert_color_prom,                                                    \
                                                                                             \
	VIDEO_TYPE_RASTER,                                                                       \
	0,                                                                                       \
	GAMENAME##_vh_start,                                                                     \
	generic_vh_stop,                                                                         \
	GAMENAME##_vh_screenrefresh,                                                             \
                                                                                             \
	/* sound hardware */                                                                     \
	0,0,0,0,                                                                                 \
	{                                                                                        \
		IREM_AUDIO                                                                           \
	}                                                                                        \
}

#define kungfum_readmem ldrun_readmem
#define kungfum_writeport ldrun_writeport
#define lotlot_writeport ldrun_writeport
#define spelunkr_writeport ldrun_writeport
#define spelunk2_writeport ldrun_writeport
#define	kungfum_vh_start kidniki_vh_start
#define	battroad_vh_start kidniki_vh_start
#define	ldrun2_vh_start ldrun_vh_start
#define	ldrun3_vh_start ldrun_vh_start
#define	ldrun4_vh_start ldrun_vh_start
#define	lotlot_vh_start ldrun_vh_start
#define	spelunk2_vh_start spelunkr_vh_start
#define	ldrun2_vh_screenrefresh ldrun_vh_screenrefresh
#define	ldrun3_vh_screenrefresh ldrun_vh_screenrefresh

MACHINE_DRIVER(kungfum,  ldrun, 16*8, (64-16)*8-1, 512,irem);
MACHINE_DRIVER(battroad, ldrun, 16*8, (64-16)*8-1, 544,battroad);
MACHINE_DRIVER(ldrun,    ldrun,  8*8, (64-8)*8-1,  512,irem);
MACHINE_DRIVER(ldrun2,   ldrun2, 8*8, (64-8)*8-1,  512,irem);
MACHINE_DRIVER(ldrun3,   ldrun,  8*8, (64-8)*8-1,  512,irem);
MACHINE_DRIVER(ldrun4,   ldrun,  8*8, (64-8)*8-1,  512,irem);
MACHINE_DRIVER(lotlot,   ldrun,  8*8, (64-8)*8-1,  768,irem);
MACHINE_DRIVER(kidniki,  ldrun,  8*8, (64-8)*8-1,  512,irem);
MACHINE_DRIVER(spelunkr, ldrun,  8*8, (64-8)*8-1,  512,irem);
MACHINE_DRIVER(spelunk2, ldrun,  8*8, (64-8)*8-1,  768,spelunk2);



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( kungfum )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a-4e-c.bin",   0x0000, 0x4000, 0xb6e2d083 )
	ROM_LOAD( "a-4d-c.bin",   0x4000, 0x4000, 0x7532918e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "a-3e-.bin",    0xa000, 0x2000, 0x58e87ab0 )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3f-.bin",    0xc000, 0x2000, 0xc81e31ea )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3h-.bin",    0xe000, 0x2000, 0xd99fb995 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g-4c-a.bin",   0x00000, 0x2000, 0x6b2cc9c8 )	/* characters */
	ROM_LOAD( "g-4d-a.bin",   0x02000, 0x2000, 0xc648f558 )
	ROM_LOAD( "g-4e-a.bin",   0x04000, 0x2000, 0xfbe9276e )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-4k-.bin",    0x00000, 0x2000, 0x16fb5150 )	/* sprites */
	ROM_LOAD( "b-4f-.bin",    0x02000, 0x2000, 0x67745a33 )
	ROM_LOAD( "b-4l-.bin",    0x04000, 0x2000, 0xbd1c2261 )
	ROM_LOAD( "b-4h-.bin",    0x06000, 0x2000, 0x8ac5ed3a )
	ROM_LOAD( "b-3n-.bin",    0x08000, 0x2000, 0x28a213aa )
	ROM_LOAD( "b-4n-.bin",    0x0a000, 0x2000, 0xd5228df3 )
	ROM_LOAD( "b-4m-.bin",    0x0c000, 0x2000, 0xb16de4f2 )
	ROM_LOAD( "b-3m-.bin",    0x0e000, 0x2000, 0xeba0d66b )
	ROM_LOAD( "b-4c-.bin",    0x10000, 0x2000, 0x01298885 )
	ROM_LOAD( "b-4e-.bin",    0x12000, 0x2000, 0xc77b87d4 )
	ROM_LOAD( "b-4d-.bin",    0x14000, 0x2000, 0x6a70615f )
	ROM_LOAD( "b-4a-.bin",    0x16000, 0x2000, 0x6189d626 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "g-1j-.bin",    0x0000, 0x0100, 0x668e6bca )	/* character palette red component */
	ROM_LOAD( "b-1m-.bin",    0x0100, 0x0100, 0x76c05a9c )	/* sprite palette red component */
	ROM_LOAD( "g-1f-.bin",    0x0200, 0x0100, 0x964b6495 )	/* character palette green component */
	ROM_LOAD( "b-1n-.bin",    0x0300, 0x0100, 0x23f06b99 )	/* sprite palette green component */
	ROM_LOAD( "g-1h-.bin",    0x0400, 0x0100, 0x550563e1 )	/* character palette blue component */
	ROM_LOAD( "b-1l-.bin",    0x0500, 0x0100, 0x35e45021 )	/* sprite palette blue component */
	ROM_LOAD( "b-5f-.bin",    0x0600, 0x0020, 0x7a601c3d )	/* sprite height, one entry per 32 */
															/* sprites. Used at run time! */
	ROM_LOAD( "b-6f-.bin",    0x0620, 0x0100, 0x82c20d12 )	/* video timing? - same as battroad */
ROM_END

ROM_START( kungfud )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a-4e-d",       0x0000, 0x4000, 0xfc330a46 )
	ROM_LOAD( "a-4d-d",       0x4000, 0x4000, 0x1b2fd32f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "a-3e-.bin",    0xa000, 0x2000, 0x58e87ab0 )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3f-.bin",    0xc000, 0x2000, 0xc81e31ea )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3h-.bin",    0xe000, 0x2000, 0xd99fb995 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g-4c-a.bin",   0x00000, 0x2000, 0x6b2cc9c8 )	/* characters */
	ROM_LOAD( "g-4d-a.bin",   0x02000, 0x2000, 0xc648f558 )
	ROM_LOAD( "g-4e-a.bin",   0x04000, 0x2000, 0xfbe9276e )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-4k-.bin",    0x00000, 0x2000, 0x16fb5150 )	/* sprites */
	ROM_LOAD( "b-4f-.bin",    0x02000, 0x2000, 0x67745a33 )
	ROM_LOAD( "b-4l-.bin",    0x04000, 0x2000, 0xbd1c2261 )
	ROM_LOAD( "b-4h-.bin",    0x06000, 0x2000, 0x8ac5ed3a )
	ROM_LOAD( "b-3n-.bin",    0x08000, 0x2000, 0x28a213aa )
	ROM_LOAD( "b-4n-.bin",    0x0a000, 0x2000, 0xd5228df3 )
	ROM_LOAD( "b-4m-.bin",    0x0c000, 0x2000, 0xb16de4f2 )
	ROM_LOAD( "b-3m-.bin",    0x0e000, 0x2000, 0xeba0d66b )
	ROM_LOAD( "b-4c-.bin",    0x10000, 0x2000, 0x01298885 )
	ROM_LOAD( "b-4e-.bin",    0x12000, 0x2000, 0xc77b87d4 )
	ROM_LOAD( "b-4d-.bin",    0x14000, 0x2000, 0x6a70615f )
	ROM_LOAD( "b-4a-.bin",    0x16000, 0x2000, 0x6189d626 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "g-1j-.bin",    0x0000, 0x0100, 0x668e6bca )	/* character palette red component */
	ROM_LOAD( "b-1m-.bin",    0x0100, 0x0100, 0x76c05a9c )	/* sprite palette red component */
	ROM_LOAD( "g-1f-.bin",    0x0200, 0x0100, 0x964b6495 )	/* character palette green component */
	ROM_LOAD( "b-1n-.bin",    0x0300, 0x0100, 0x23f06b99 )	/* sprite palette green component */
	ROM_LOAD( "g-1h-.bin",    0x0400, 0x0100, 0x550563e1 )	/* character palette blue component */
	ROM_LOAD( "b-1l-.bin",    0x0500, 0x0100, 0x35e45021 )	/* sprite palette blue component */
	ROM_LOAD( "b-5f-.bin",    0x0600, 0x0020, 0x7a601c3d )	/* sprite height, one entry per 32 */
															/* sprites. Used at run time! */
	ROM_LOAD( "b-6f-.bin",    0x0620, 0x0100, 0x82c20d12 )	/* video timing? - same as battroad */
ROM_END

ROM_START( spartanx )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a-4e-c-j.bin", 0x0000, 0x4000, 0x32a0a9a6 )
	ROM_LOAD( "a-4d-c-j.bin", 0x4000, 0x4000, 0x3173ea78 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "a-3e-.bin",    0xa000, 0x2000, 0x58e87ab0 )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3f-.bin",    0xc000, 0x2000, 0xc81e31ea )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3h-.bin",    0xe000, 0x2000, 0xd99fb995 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g-4c-a-j.bin", 0x00000, 0x2000, 0x8af9c5a6 )	/* characters */
	ROM_LOAD( "g-4d-a-j.bin", 0x02000, 0x2000, 0xb8300c72 )
	ROM_LOAD( "g-4e-a-j.bin", 0x04000, 0x2000, 0xb50429cd )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-4k-.bin",    0x00000, 0x2000, 0x16fb5150 )	/* sprites */
	ROM_LOAD( "b-4f-.bin",    0x02000, 0x2000, 0x67745a33 )
	ROM_LOAD( "b-4l-.bin",    0x04000, 0x2000, 0xbd1c2261 )
	ROM_LOAD( "b-4h-.bin",    0x06000, 0x2000, 0x8ac5ed3a )
	ROM_LOAD( "b-3n-.bin",    0x08000, 0x2000, 0x28a213aa )
	ROM_LOAD( "b-4n-.bin",    0x0a000, 0x2000, 0xd5228df3 )
	ROM_LOAD( "b-4m-.bin",    0x0c000, 0x2000, 0xb16de4f2 )
	ROM_LOAD( "b-3m-.bin",    0x0e000, 0x2000, 0xeba0d66b )
	ROM_LOAD( "b-4c-.bin",    0x10000, 0x2000, 0x01298885 )
	ROM_LOAD( "b-4e-.bin",    0x12000, 0x2000, 0xc77b87d4 )
	ROM_LOAD( "b-4d-.bin",    0x14000, 0x2000, 0x6a70615f )
	ROM_LOAD( "b-4a-.bin",    0x16000, 0x2000, 0x6189d626 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "g-1j-.bin",    0x0000, 0x0100, 0x668e6bca )	/* character palette red component */
	ROM_LOAD( "b-1m-.bin",    0x0100, 0x0100, 0x76c05a9c )	/* sprite palette red component */
	ROM_LOAD( "g-1f-.bin",    0x0200, 0x0100, 0x964b6495 )	/* character palette green component */
	ROM_LOAD( "b-1n-.bin",    0x0300, 0x0100, 0x23f06b99 )	/* sprite palette green component */
	ROM_LOAD( "g-1h-.bin",    0x0400, 0x0100, 0x550563e1 )	/* character palette blue component */
	ROM_LOAD( "b-1l-.bin",    0x0500, 0x0100, 0x35e45021 )	/* sprite palette blue component */
	ROM_LOAD( "b-5f-.bin",    0x0600, 0x0020, 0x7a601c3d )	/* sprite height, one entry per 32 */
															/* sprites. Used at run time! */
	ROM_LOAD( "b-6f-.bin",    0x0620, 0x0100, 0x82c20d12 )	/* video timing? - same as battroad */
ROM_END

ROM_START( kungfub )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "c5.5h",        0x0000, 0x4000, 0x5d8e791d )
	ROM_LOAD( "c4.5k",        0x4000, 0x4000, 0x4000e2b8 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "a-3e-.bin",    0xa000, 0x2000, 0x58e87ab0 )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3f-.bin",    0xc000, 0x2000, 0xc81e31ea )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3h-.bin",    0xe000, 0x2000, 0xd99fb995 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g-4c-a.bin",   0x00000, 0x2000, 0x6b2cc9c8 )	/* characters */
	ROM_LOAD( "g-4d-a.bin",   0x02000, 0x2000, 0xc648f558 )
	ROM_LOAD( "g-4e-a.bin",   0x04000, 0x2000, 0xfbe9276e )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-4k-.bin",    0x00000, 0x2000, 0x16fb5150 )	/* sprites */
	ROM_LOAD( "b-4f-.bin",    0x02000, 0x2000, 0x67745a33 )
	ROM_LOAD( "b-4l-.bin",    0x04000, 0x2000, 0xbd1c2261 )
	ROM_LOAD( "b-4h-.bin",    0x06000, 0x2000, 0x8ac5ed3a )
	ROM_LOAD( "b-3n-.bin",    0x08000, 0x2000, 0x28a213aa )
	ROM_LOAD( "b-4n-.bin",    0x0a000, 0x2000, 0xd5228df3 )
	ROM_LOAD( "b-4m-.bin",    0x0c000, 0x2000, 0xb16de4f2 )
	ROM_LOAD( "b-3m-.bin",    0x0e000, 0x2000, 0xeba0d66b )
	ROM_LOAD( "b-4c-.bin",    0x10000, 0x2000, 0x01298885 )
	ROM_LOAD( "b-4e-.bin",    0x12000, 0x2000, 0xc77b87d4 )
	ROM_LOAD( "b-4d-.bin",    0x14000, 0x2000, 0x6a70615f )
	ROM_LOAD( "b-4a-.bin",    0x16000, 0x2000, 0x6189d626 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "g-1j-.bin",    0x0000, 0x0100, 0x668e6bca )	/* character palette red component */
	ROM_LOAD( "b-1m-.bin",    0x0100, 0x0100, 0x76c05a9c )	/* sprite palette red component */
	ROM_LOAD( "g-1f-.bin",    0x0200, 0x0100, 0x964b6495 )	/* character palette green component */
	ROM_LOAD( "b-1n-.bin",    0x0300, 0x0100, 0x23f06b99 )	/* sprite palette green component */
	ROM_LOAD( "g-1h-.bin",    0x0400, 0x0100, 0x550563e1 )	/* character palette blue component */
	ROM_LOAD( "b-1l-.bin",    0x0500, 0x0100, 0x35e45021 )	/* sprite palette blue component */
	ROM_LOAD( "b-5f-.bin",    0x0600, 0x0020, 0x7a601c3d )	/* sprite height, one entry per 32 */
															/* sprites. Used at run time! */
	ROM_LOAD( "b-6f-.bin",    0x0620, 0x0100, 0x82c20d12 )	/* video timing? - same as battroad */
ROM_END

ROM_START( kungfub2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "kf4",          0x0000, 0x4000, 0x3f65313f )
	ROM_LOAD( "kf5",          0x4000, 0x4000, 0x9ea325f3 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "a-3e-.bin",    0xa000, 0x2000, 0x58e87ab0 )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3f-.bin",    0xc000, 0x2000, 0xc81e31ea )	/* samples (ADPCM 4-bit) */
	ROM_LOAD( "a-3h-.bin",    0xe000, 0x2000, 0xd99fb995 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g-4c-a.bin",   0x00000, 0x2000, 0x6b2cc9c8 )	/* characters */
	ROM_LOAD( "g-4d-a.bin",   0x02000, 0x2000, 0xc648f558 )
	ROM_LOAD( "g-4e-a.bin",   0x04000, 0x2000, 0xfbe9276e )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-4k-.bin",    0x00000, 0x2000, 0x16fb5150 )	/* sprites */
	ROM_LOAD( "b-4f-.bin",    0x02000, 0x2000, 0x67745a33 )
	ROM_LOAD( "b-4l-.bin",    0x04000, 0x2000, 0xbd1c2261 )
	ROM_LOAD( "b-4h-.bin",    0x06000, 0x2000, 0x8ac5ed3a )
	ROM_LOAD( "b-3n-.bin",    0x08000, 0x2000, 0x28a213aa )
	ROM_LOAD( "b-4n-.bin",    0x0a000, 0x2000, 0xd5228df3 )
	ROM_LOAD( "b-4m-.bin",    0x0c000, 0x2000, 0xb16de4f2 )
	ROM_LOAD( "b-3m-.bin",    0x0e000, 0x2000, 0xeba0d66b )
	ROM_LOAD( "b-4c-.bin",    0x10000, 0x2000, 0x01298885 )
	ROM_LOAD( "b-4e-.bin",    0x12000, 0x2000, 0xc77b87d4 )
	ROM_LOAD( "b-4d-.bin",    0x14000, 0x2000, 0x6a70615f )
	ROM_LOAD( "b-4a-.bin",    0x16000, 0x2000, 0x6189d626 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "g-1j-.bin",    0x0000, 0x0100, 0x668e6bca )	/* character palette red component */
	ROM_LOAD( "b-1m-.bin",    0x0100, 0x0100, 0x76c05a9c )	/* sprite palette red component */
	ROM_LOAD( "g-1f-.bin",    0x0200, 0x0100, 0x964b6495 )	/* character palette green component */
	ROM_LOAD( "b-1n-.bin",    0x0300, 0x0100, 0x23f06b99 )	/* sprite palette green component */
	ROM_LOAD( "g-1h-.bin",    0x0400, 0x0100, 0x550563e1 )	/* character palette blue component */
	ROM_LOAD( "b-1l-.bin",    0x0500, 0x0100, 0x35e45021 )	/* sprite palette blue component */
	ROM_LOAD( "b-5f-.bin",    0x0600, 0x0020, 0x7a601c3d )	/* sprite height, one entry per 32 */
															/* sprites. Used at run time! */
	ROM_LOAD( "b-6f-.bin",    0x0620, 0x0100, 0x82c20d12 )	/* video timing? - same as battroad */
ROM_END

ROM_START( battroad )
	ROM_REGION( 0x1e000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "br-a-4e.b",	0x00000, 0x2000, 0x9bf14768 )
	ROM_LOAD( "br-a-4d.b",	0x02000, 0x2000, 0x39ca1627 )
	ROM_LOAD( "br-a-4b.b",	0x04000, 0x2000, 0x1865bb22 )
	ROM_LOAD( "br-a-4a",	0x06000, 0x2000, 0x65b61c21 )
	ROM_LOAD( "br-c-7c",	0x10000, 0x2000, 0x2e1eca52 )	/* banked at a000-bfff */
	ROM_LOAD( "br-c-7l",	0x12000, 0x2000, 0xf2178578 )
	ROM_LOAD( "br-c-7d",	0x14000, 0x2000, 0x3aa9fa30 )
	ROM_LOAD( "br-c-7b",	0x16000, 0x2000, 0x0b31b90b )
	ROM_LOAD( "br-c-7a",	0x18000, 0x2000, 0xec3b0080 )
	ROM_LOAD( "br-c-7k",	0x1c000, 0x2000, 0xedc75f7f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "br-a-3e",     0xa000, 0x2000, 0xa7140871 )
	ROM_LOAD( "br-a-3f",     0xc000, 0x2000, 0x1bb51b30 )
	ROM_LOAD( "br-a-3h",     0xe000, 0x2000, 0xafb3e083 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "br-c-6h",	0x00000, 0x2000, 0xca50841c )	/* tiles */
	ROM_LOAD( "br-c-6n",	0x02000, 0x2000, 0x7d53163a )
	ROM_LOAD( "br-c-6k",	0x04000, 0x2000, 0x5951e12a )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "br-b-4k.a",	0x00000, 0x2000, 0xd3c5e85b )	/* sprites */
	ROM_LOAD( "br-b-4f.a",	0x02000, 0x2000, 0x4354232a )
	ROM_LOAD( "br-b-3n.a",	0x04000, 0x2000, 0x2668dbef )
	ROM_LOAD( "br-b-4n.a",	0x06000, 0x2000, 0xc719a324 )
	ROM_LOAD( "br-b-4c.a",	0x08000, 0x2000, 0x0b3193bf )
	ROM_LOAD( "br-b-4e.a",	0x0a000, 0x2000, 0x3662e8fb )

	ROM_REGION( 0x04000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "br-c-1b",	0x00000, 0x2000, 0x8088911e )	/* characters */
	ROM_LOAD( "br-c-1c",	0x02000, 0x2000, 0x3d78b653 )

	ROM_REGION( 0x0740, REGION_PROMS )
	ROM_LOAD( "br-c-3j",     0x0000, 0x0100, 0xaceaed79 )	/* tile palette red component */
	ROM_LOAD( "br-b-1m",     0x0100, 0x0100, 0x3bd30c7d )	/* sprite palette red component */
	ROM_LOAD( "br-c-3l",     0x0200, 0x0100, 0x7cf6f380 )	/* tile palette green component */
	ROM_LOAD( "br-b-1n",     0x0300, 0x0100, 0xb7f3dc3b )	/* sprite palette green component */
	ROM_LOAD( "br-c-3k",     0x0400, 0x0100, 0xd90e4a54 )	/* tile palette blue component */
	ROM_LOAD( "br-b-1l",     0x0500, 0x0100, 0x5271c7d8 )	/* sprite palette blue component */
	ROM_LOAD( "br-c-1j",     0x0600, 0x0020, 0x78eb5d77 )	/* character palette */
	ROM_LOAD( "br-b-5p",     0x0620, 0x0020, 0xce746937 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "br-b-6f",     0x0640, 0x0100, 0x82c20d12 )	/* video timing? - same as kungfum */
ROM_END

ROM_START( ldrun )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "lr-a-4e",      0x0000, 0x2000, 0x5d7e2a4d )
	ROM_LOAD( "lr-a-4d",      0x2000, 0x2000, 0x96f20473 )
	ROM_LOAD( "lr-a-4b",      0x4000, 0x2000, 0xb041c4a9 )
	ROM_LOAD( "lr-a-4a",      0x6000, 0x2000, 0x645e42aa )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "lr-a-3f",      0xc000, 0x2000, 0x7a96accd )
	ROM_LOAD( "lr-a-3h",      0xe000, 0x2000, 0x3f7f3939 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr-e-2d",      0x0000, 0x2000, 0x24f9b58d )	/* characters */
	ROM_LOAD( "lr-e-2j",      0x2000, 0x2000, 0x43175e08 )
	ROM_LOAD( "lr-e-2f",      0x4000, 0x2000, 0xe0317124 )

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr-b-4k",      0x0000, 0x2000, 0x8141403e )	/* sprites */
	ROM_LOAD( "lr-b-3n",      0x2000, 0x2000, 0x55154154 )
	ROM_LOAD( "lr-b-4c",      0x4000, 0x2000, 0x924e34d0 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "lr-e-3m",      0x0000, 0x0100, 0x53040416 )	/* character palette red component */
	ROM_LOAD( "lr-b-1m",      0x0100, 0x0100, 0x4bae1c25 )	/* sprite palette red component */
	ROM_LOAD( "lr-e-3l",      0x0200, 0x0100, 0x67786037 )	/* character palette green component */
	ROM_LOAD( "lr-b-1n",      0x0300, 0x0100, 0x9cd3db94 )	/* sprite palette green component */
	ROM_LOAD( "lr-e-3n",      0x0400, 0x0100, 0x5b716837 )	/* character palette blue component */
	ROM_LOAD( "lr-b-1l",      0x0500, 0x0100, 0x08d8cf9a )	/* sprite palette blue component */
	ROM_LOAD( "lr-b-5p",      0x0600, 0x0020, 0xe01f69e2 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "lr-b-6f",      0x0620, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( ldruna )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "roma4c",       0x0000, 0x2000, 0x279421e1 )
	ROM_LOAD( "lr-a-4d",      0x2000, 0x2000, 0x96f20473 )
	ROM_LOAD( "roma4b",       0x4000, 0x2000, 0x3c464bad )
	ROM_LOAD( "roma4a",       0x6000, 0x2000, 0x899df8e0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "lr-a-3f",      0xc000, 0x2000, 0x7a96accd )
	ROM_LOAD( "lr-a-3h",      0xe000, 0x2000, 0x3f7f3939 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr-e-2d",      0x0000, 0x2000, 0x24f9b58d )	/* characters */
	ROM_LOAD( "lr-e-2j",      0x2000, 0x2000, 0x43175e08 )
	ROM_LOAD( "lr-e-2f",      0x4000, 0x2000, 0xe0317124 )

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr-b-4k",      0x0000, 0x2000, 0x8141403e )	/* sprites */
	ROM_LOAD( "lr-b-3n",      0x2000, 0x2000, 0x55154154 )
	ROM_LOAD( "lr-b-4c",      0x4000, 0x2000, 0x924e34d0 )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "lr-e-3m",      0x0000, 0x0100, 0x53040416 )	/* character palette red component */
	ROM_LOAD( "lr-b-1m",      0x0100, 0x0100, 0x4bae1c25 )	/* sprite palette red component */
	ROM_LOAD( "lr-e-3l",      0x0200, 0x0100, 0x67786037 )	/* character palette green component */
	ROM_LOAD( "lr-b-1n",      0x0300, 0x0100, 0x9cd3db94 )	/* sprite palette green component */
	ROM_LOAD( "lr-e-3n",      0x0400, 0x0100, 0x5b716837 )	/* character palette blue component */
	ROM_LOAD( "lr-b-1l",      0x0500, 0x0100, 0x08d8cf9a )	/* sprite palette blue component */
	ROM_LOAD( "lr-b-5p",      0x0600, 0x0020, 0xe01f69e2 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "lr-b-6f",      0x0620, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( ldrun2 )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 64k for code + 16k for banks */
	ROM_LOAD( "lr2-a-4e.a",   0x00000, 0x2000, 0x22313327 )
	ROM_LOAD( "lr2-a-4d",     0x02000, 0x2000, 0xef645179 )
	ROM_LOAD( "lr2-a-4a.a",   0x04000, 0x2000, 0xb11ddf59 )
	ROM_LOAD( "lr2-a-4a",     0x06000, 0x2000, 0x470cc8a1 )
	ROM_LOAD( "lr2-h-1c.a",   0x10000, 0x2000, 0x7ebcadbc )	/* banked at 8000-9fff */
	ROM_LOAD( "lr2-h-1d.a",   0x12000, 0x2000, 0x64cbb7f9 )	/* banked at 8000-9fff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "lr2-a-3e",     0xa000, 0x2000, 0x853f3898 )
	ROM_LOAD( "lr2-a-3f",     0xc000, 0x2000, 0x7a96accd )
	ROM_LOAD( "lr2-a-3h",     0xe000, 0x2000, 0x2a0e83ca )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr2-h-1e",     0x00000, 0x2000, 0x9d63a8ff )	/* characters */
	ROM_LOAD( "lr2-h-1j",     0x02000, 0x2000, 0x40332bbd )
	ROM_LOAD( "lr2-h-1h",     0x04000, 0x2000, 0x9404727d )

	ROM_REGION( 0xc000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr2-b-4k",     0x00000, 0x2000, 0x79909871 )	/* sprites */
	ROM_LOAD( "lr2-b-4f",     0x02000, 0x2000, 0x06ba1ef4 )
	ROM_LOAD( "lr2-b-3n",     0x04000, 0x2000, 0x3cc5893f )
	ROM_LOAD( "lr2-b-4n",     0x06000, 0x2000, 0x49c12f42 )
	ROM_LOAD( "lr2-b-4c",     0x08000, 0x2000, 0xfbe6d24c )
	ROM_LOAD( "lr2-b-4e",     0x0a000, 0x2000, 0x75172d1f )

	ROM_REGION( 0x0720, REGION_PROMS )
	ROM_LOAD( "lr2-h-3m",     0x0000, 0x0100, 0x2c5d834b )	/* character palette red component */
	ROM_LOAD( "lr2-b-1m",     0x0100, 0x0100, 0x4ec9bb3d )	/* sprite palette red component */
	ROM_LOAD( "lr2-h-3l",     0x0200, 0x0100, 0x3ae69aca )	/* character palette green component */
	ROM_LOAD( "lr2-b-1n",     0x0300, 0x0100, 0x1daf1fa4 )	/* sprite palette green component */
	ROM_LOAD( "lr2-h-3n",     0x0400, 0x0100, 0x2b28aec5 )	/* character palette blue component */
	ROM_LOAD( "lr2-b-1l",     0x0500, 0x0100, 0xc8fb708a )	/* sprite palette blue component */
	ROM_LOAD( "lr2-b-5p",     0x0600, 0x0020, 0xe01f69e2 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "lr2-b-6f",     0x0620, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( ldrun3 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "lr3-a-4e",     0x0000, 0x4000, 0x5b334e8e )
	ROM_LOAD( "lr3-a-4d.a",   0x4000, 0x4000, 0xa84bc931 )
	ROM_LOAD( "lr3-a-4b.a",   0x8000, 0x4000, 0xbe09031d )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "lr3-a-3d",     0x8000, 0x4000, 0x28be68cd )
	ROM_LOAD( "lr3-a-3f",     0xc000, 0x4000, 0xcb7186b7 )

	ROM_REGION( 0xc000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr3-n-2a",     0x00000, 0x4000, 0xf9b74dee )	/* characters */
	ROM_LOAD( "lr3-n-2c",     0x04000, 0x4000, 0xfef707ba )
	ROM_LOAD( "lr3-n-2b",     0x08000, 0x4000, 0xaf3d27b9 )

	ROM_REGION( 0xc000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr3-b-4k",     0x00000, 0x4000, 0x63f070c7 )	/* sprites */
	ROM_LOAD( "lr3-b-3n",     0x04000, 0x4000, 0xeab7ad91 )
	ROM_LOAD( "lr3-b-4c",     0x08000, 0x4000, 0x1a460a46 )

	ROM_REGION( 0x0820, REGION_PROMS )
	ROM_LOAD( "lr3-n-2l",     0x0000, 0x0100, 0xe880b86b ) /* character palette red component */
	ROM_LOAD( "lr3-b-1m",     0x0100, 0x0100, 0xf02d7167 ) /* sprite palette red component */
	ROM_LOAD( "lr3-n-2k",     0x0200, 0x0100, 0x047ee051 ) /* character palette green component */
	ROM_LOAD( "lr3-b-1n",     0x0300, 0x0100, 0x9e37f181 ) /* sprite palette green component */
	ROM_LOAD( "lr3-n-2m",     0x0400, 0x0100, 0x69ad8678 ) /* character palette blue component */
	ROM_LOAD( "lr3-b-1l",     0x0500, 0x0100, 0x5b11c41d ) /* sprite palette blue component */
	ROM_LOAD( "lr3-b-5p",     0x0600, 0x0020, 0xe01f69e2 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "lr3-n-4f",     0x0620, 0x0100, 0xdf674be9 )	/* unknown */
	ROM_LOAD( "lr3-b-6f",     0x0720, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( ldrun4 )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* 64k for code + 32k for banked ROM */
	ROM_LOAD( "lr4-a-4e",     0x00000, 0x4000, 0x5383e9bf )
	ROM_LOAD( "lr4-a-4d.c",   0x04000, 0x4000, 0x298afa36 )
	ROM_LOAD( "lr4-v-4k",     0x10000, 0x8000, 0x8b248abd )	/* banked at 8000-bfff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "lr4-a-3d",     0x8000, 0x4000, 0x86c6d445 )
	ROM_LOAD( "lr4-a-3f",     0xc000, 0x4000, 0x097c6c0a )

	ROM_REGION( 0xc000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr4-v-2b",     0x00000, 0x4000, 0x4118e60a )	/* characters */
	ROM_LOAD( "lr4-v-2d",     0x04000, 0x4000, 0x542bb5b5 )
	ROM_LOAD( "lr4-v-2c",     0x08000, 0x4000, 0xc765266c )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lr4-b-4k",     0x00000, 0x4000, 0xe7fe620c )	/* sprites */
	ROM_LOAD( "lr4-b-4f",     0x04000, 0x4000, 0x6f0403db )
	ROM_LOAD( "lr4-b-3n",     0x08000, 0x4000, 0xad1fba1b )
	ROM_LOAD( "lr4-b-4n",     0x0c000, 0x4000, 0x0e568fab )
	ROM_LOAD( "lr4-b-4c",     0x10000, 0x4000, 0x82c53669 )
	ROM_LOAD( "lr4-b-4e",     0x14000, 0x4000, 0x767a1352 )

	ROM_REGION( 0x0820, REGION_PROMS )
	ROM_LOAD( "lr4-v-1m",     0x0000, 0x0100, 0xfe51bf1d ) /* character palette red component */
	ROM_LOAD( "lr4-b-1m",     0x0100, 0x0100, 0x5d8d17d0 ) /* sprite palette red component */
	ROM_LOAD( "lr4-v-1n",     0x0200, 0x0100, 0xda0658e5 ) /* character palette green component */
	ROM_LOAD( "lr4-b-1n",     0x0300, 0x0100, 0xda1129d2 ) /* sprite palette green component */
	ROM_LOAD( "lr4-v-1p",     0x0400, 0x0100, 0x0df23ebe ) /* character palette blue component */
	ROM_LOAD( "lr4-b-1l",     0x0500, 0x0100, 0x0d89b692 ) /* sprite palette blue component */
	ROM_LOAD( "lr4-b-5p",     0x0600, 0x0020, 0xe01f69e2 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "lr4-v-4h",     0x0620, 0x0100, 0xdf674be9 )	/* unknown */
	ROM_LOAD( "lr4-b-6f",     0x0720, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( lotlot )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "lot-a-4e",     0x0000, 0x4000, 0x2913d08f )
	ROM_LOAD( "lot-a-4d",     0x4000, 0x4000, 0x0443095f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU (6803) */
	ROM_LOAD( "lot-a-3h",     0xe000, 0x2000, 0x0781cee7 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lot-k-4a",     0x00000, 0x2000, 0x1b3695f4 )	/* tiles */
	ROM_LOAD( "lot-k-4c",     0x02000, 0x2000, 0xbd2b0730 )
	ROM_LOAD( "lot-k-4b",     0x04000, 0x2000, 0x930ddd55 )

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lot-b-4k",     0x00000, 0x2000, 0xfd27cb90 )	/* sprites */
	ROM_LOAD( "lot-b-3n",     0x02000, 0x2000, 0xbd486fff )
	ROM_LOAD( "lot-b-4c",     0x04000, 0x2000, 0x3026ee6c )

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lot-k-4p",     0x00000, 0x2000, 0x3b7d95ba )	/* chars */
	ROM_LOAD( "lot-k-4l",     0x02000, 0x2000, 0xf98dca1f )
	ROM_LOAD( "lot-k-4n",     0x04000, 0x2000, 0xf0cd76a5 )

	ROM_REGION( 0x0e20, REGION_PROMS )
	ROM_LOAD( "lot-k-2f",     0x0000, 0x0100, 0xb820a05e ) /* tile palette red component */
	ROM_LOAD( "lot-b-1m",     0x0100, 0x0100, 0xc146461d ) /* sprite palette red component */
	ROM_LOAD( "lot-k-2l",     0x0200, 0x0100, 0xac3e230d ) /* character palette red component */
	ROM_LOAD( "lot-k-2e",     0x0300, 0x0100, 0x9b1fa005 ) /* tile palette green component */
	ROM_LOAD( "lot-b-1n",     0x0400, 0x0100, 0x01e07db6 ) /* sprite palette green component */
	ROM_LOAD( "lot-k-2k",     0x0500, 0x0100, 0x1811ad2b ) /* character palette green component */
	ROM_LOAD( "lot-k-2d",     0x0600, 0x0100, 0x315ed9a8 ) /* tile palette blue component */
	ROM_LOAD( "lot-b-1l",     0x0700, 0x0100, 0x8b6fcde3 ) /* sprite palette blue component */
	ROM_LOAD( "lot-k-2j",     0x0800, 0x0100, 0xe791ef2a ) /* character palette blue component */
	ROM_LOAD( "lot-b-5p",     0x0900, 0x0020, 0x110b21fd )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "lot-k-7e",     0x0920, 0x0200, 0x6cef0fbd )	/* unknown */
	ROM_LOAD( "lot-k-7h",     0x0b20, 0x0200, 0x04442bee )	/* unknown */
	ROM_LOAD( "lot-b-6f",     0x0d20, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( kidniki )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* main CPU */
	ROM_LOAD( "dr04.4e",      0x00000, 0x04000, 0x80431858 )
	ROM_LOAD( "dr03.4cd",     0x04000, 0x04000, 0xdba20934 )
	ROM_LOAD( "dr11.8k",      0x10000, 0x08000, 0x04d82d93 )	/* banked at 8000-9fff */
	ROM_LOAD( "dr12.8l",      0x18000, 0x08000, 0xc0b255fd )
	ROM_CONTINUE(             0x28000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD( "dr00.3a",      0x4000, 0x04000, 0x458309f7 )
	ROM_LOAD( "dr01.3cd",     0x8000, 0x04000, 0xe66897bd )
	ROM_LOAD( "dr02.3f",      0xc000, 0x04000, 0xf9e31e26 ) /* 6803 code */

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dr06.2b",      0x00000, 0x8000, 0x4d9a970f )	/* tiles */
	ROM_LOAD( "dr07.2dc",     0x08000, 0x8000, 0xab59a4c4 )
	ROM_LOAD( "dr05.2a",      0x10000, 0x8000, 0x2e6dad0c )

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dr21.4k",      0x00000, 0x4000, 0xa06cea9a )	/* sprites */
	ROM_LOAD( "dr19.4f",      0x04000, 0x4000, 0xb34605ad )
	ROM_LOAD( "dr22.4l",      0x08000, 0x4000, 0x41303de8 )
	ROM_LOAD( "dr20.4jh",     0x0c000, 0x4000, 0x5fbe6f61 )
	ROM_LOAD( "dr14.3p",      0x10000, 0x4000, 0x76cfbcbc )
	ROM_LOAD( "dr24.4p",      0x14000, 0x4000, 0xd51c8db5 )
	ROM_LOAD( "dr23.4nm",     0x18000, 0x4000, 0x03469df8 )
	ROM_LOAD( "dr13.3nm",     0x1c000, 0x4000, 0xd5c3dfe0 )
	ROM_LOAD( "dr16.4cb",     0x20000, 0x4000, 0xf1d1bb93 )
	ROM_LOAD( "dr18.4e",      0x24000, 0x4000, 0xedb7f25b )
	ROM_LOAD( "dr17.4dc",     0x28000, 0x4000, 0x4fb87868 )
	ROM_LOAD( "dr15.4a",      0x2c000, 0x4000, 0xe0b88de5 )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dr08.4l",      0x00000, 0x4000, 0x32d50643 )	/* chars */
	ROM_LOAD( "dr09.4m",      0x04000, 0x4000, 0x17df6f95 )
	ROM_LOAD( "dr10.4n",      0x08000, 0x4000, 0x820ce252 )

	ROM_REGION( 0x0920, REGION_PROMS )
	ROM_LOAD( "dr25.3f",      0x0000, 0x0100, 0x8e91430b )	/* character palette red component */
	ROM_LOAD( "dr30.1m",      0x0100, 0x0100, 0x28c73263 )	/* sprite palette red component */
	ROM_LOAD( "dr26.3h",      0x0200, 0x0100, 0xb563b93f )	/* character palette green component */
	ROM_LOAD( "dr31.1n",      0x0300, 0x0100, 0x3529210e )	/* sprite palette green component */
	ROM_LOAD( "dr27.3j",      0x0400, 0x0100, 0x70d668ef )	/* character palette blue component */
	ROM_LOAD( "dr29.1l",      0x0500, 0x0100, 0x1173a754 )	/* sprite palette blue component */
	ROM_LOAD( "dr32.5p",      0x0600, 0x0020, 0x11cd1f2e )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "dr28.8f",      0x0620, 0x0200, 0x6cef0fbd )	/* unknown */
	ROM_LOAD( "dr33.6f",      0x0820, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END


ROM_START( yanchamr )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* main CPU */
	ROM_LOAD( "ky_a-4e-.bin", 0x00000, 0x04000, 0xc73ad2d6 )
	ROM_LOAD( "ky_a-4d-.bin", 0x04000, 0x04000, 0x401af828 )
	ROM_LOAD( "ky_t-8k-.bin", 0x10000, 0x08000, 0xe967de88 )	/* banked at 8000-9fff */
	ROM_LOAD( "ky_t-8l-.bin", 0x18000, 0x08000, 0xa929110b )
/*	ROM_CONTINUE(             0x28000, 0x08000 ) */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD( "ky_a-3a-.bin", 0x4000, 0x04000, 0xcb365f3b )
	ROM_LOAD( "dr01.3cd",     0x8000, 0x04000, 0xe66897bd )
	ROM_LOAD( "dr02.3f",      0xc000, 0x04000, 0xf9e31e26 ) /* 6803 code */

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ky_t-2c-.bin", 0x00000, 0x8000, 0xcb9761fc )	/* tiles */
	ROM_LOAD( "ky_t-2d-.bin", 0x08000, 0x8000, 0x59732741 )
	ROM_LOAD( "ky_t-2a-.bin", 0x10000, 0x8000, 0x0370fd82 )

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ky_b-4k-.bin", 0x00000, 0x4000, 0x263a9d10 )	/* sprites */
	ROM_LOAD( "ky_b-4f-.bin", 0x04000, 0x4000, 0x86e3d4a8 )
	ROM_LOAD( "ky_b-4l-.bin", 0x08000, 0x4000, 0x19fa7558 )
	ROM_LOAD( "ky_b-4h-.bin", 0x0c000, 0x4000, 0x93e6665c )
	ROM_LOAD( "ky_b-3n-.bin", 0x10000, 0x4000, 0x0287c525 )
	ROM_LOAD( "ky_b-4n-.bin", 0x14000, 0x4000, 0x764946e0 )
	ROM_LOAD( "ky_b-4m-.bin", 0x18000, 0x4000, 0xeced5db9 )
	ROM_LOAD( "ky_b-3m-.bin", 0x1c000, 0x4000, 0xbe6cee44 )
	ROM_LOAD( "ky_b-4c-.bin", 0x20000, 0x4000, 0x84d6b65d )
	ROM_LOAD( "ky_b-4e-.bin", 0x24000, 0x4000, 0xf91f9273 )
	ROM_LOAD( "ky_b-4d-.bin", 0x28000, 0x4000, 0xa2fc15f0 )
	ROM_LOAD( "ky_b-4a-.bin", 0x2c000, 0x4000, 0xff2b9c8a )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ky_t-4l-.bin", 0x00000, 0x4000, 0x1d0a9253 )	/* chars */
	ROM_LOAD( "ky_t-4m-.bin", 0x04000, 0x4000, 0x4075c396 )
	ROM_LOAD( "ky_t-4n-.bin", 0x08000, 0x4000, 0x7564f2ff )

	ROM_REGION( 0x0920, REGION_PROMS )
	ROM_LOAD( "dr25.3f",      0x0000, 0x0100, 0x8e91430b )	/* character palette red component */
	ROM_LOAD( "dr30.1m",      0x0100, 0x0100, 0x28c73263 )	/* sprite palette red component */
	ROM_LOAD( "dr26.3h",      0x0200, 0x0100, 0xb563b93f )	/* character palette green component */
	ROM_LOAD( "dr31.1n",      0x0300, 0x0100, 0x3529210e )	/* sprite palette green component */
	ROM_LOAD( "dr27.3j",      0x0400, 0x0100, 0x70d668ef )	/* character palette blue component */
	ROM_LOAD( "dr29.1l",      0x0500, 0x0100, 0x1173a754 )	/* sprite palette blue component */
	ROM_LOAD( "dr32.5p",      0x0600, 0x0020, 0x11cd1f2e )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "dr28.8f",      0x0620, 0x0200, 0x6cef0fbd )	/* unknown */
	ROM_LOAD( "dr33.6f",      0x0820, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( spelunkr )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* main CPU */
	ROM_LOAD( "spra.4e",      0x00000, 0x4000, 0xcf811201 )
	ROM_LOAD( "spra.4d",      0x04000, 0x4000, 0xbb4faa4f )
	ROM_LOAD( "sprm.7c",      0x10000, 0x4000, 0xfb6197e2 )	/* banked at 8000-9fff */
	ROM_LOAD( "sprm.7b",      0x14000, 0x4000, 0x26bb25a4 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD( "spra.3d",      0x8000, 0x04000, 0x4110363c ) /* adpcm data */
	ROM_LOAD( "spra.3f",      0xc000, 0x04000, 0x67a9d2e6 ) /* 6803 code */

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sprm.1d",      0x00000, 0x4000, 0x4ef7ae89 )	/* tiles */
	ROM_LOAD( "sprm.1e",      0x04000, 0x4000, 0xa3755180 )
	ROM_LOAD( "sprm.3c",      0x08000, 0x4000, 0xb4008e6a )
	ROM_LOAD( "sprm.3b",      0x0c000, 0x4000, 0xf61cf012 )
	ROM_LOAD( "sprm.1c",      0x10000, 0x4000, 0x58b21c76 )
	ROM_LOAD( "sprm.1b",      0x14000, 0x4000, 0xa95cb3e5 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sprb.4k",      0x00000, 0x4000, 0xe7f0e861 )	/* sprites */
	ROM_LOAD( "sprb.4f",      0x04000, 0x4000, 0x32663097 )
	ROM_LOAD( "sprb.3p",      0x08000, 0x4000, 0x8fbaf373 )
	ROM_LOAD( "sprb.4p",      0x0c000, 0x4000, 0x37069b76 )
	ROM_LOAD( "sprb.4c",      0x10000, 0x4000, 0xcfe46a88 )
	ROM_LOAD( "sprb.4e",      0x14000, 0x4000, 0x11c48979 )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sprm.4p",      0x00000, 0x0800, 0x4dfe2e63 )	/* chars */
	ROM_CONTINUE(             0x02000, 0x0800 )			/* first and second half identical, */
	ROM_CONTINUE(             0x00800, 0x0800 )			/* second half not used by the driver */
	ROM_CONTINUE(             0x02800, 0x0800 )
	ROM_CONTINUE(             0x00000, 0x0800 )
	ROM_CONTINUE(             0x03000, 0x0800 )
	ROM_CONTINUE(             0x00800, 0x0800 )
	ROM_CONTINUE(             0x03800, 0x0800 )
	ROM_LOAD( "sprm.4l",      0x04000, 0x0800, 0x239f2cd4 )
	ROM_CONTINUE(             0x06000, 0x0800 )
	ROM_CONTINUE(             0x04800, 0x0800 )
	ROM_CONTINUE(             0x06800, 0x0800 )
	ROM_CONTINUE(             0x05000, 0x0800 )
	ROM_CONTINUE(             0x07000, 0x0800 )
	ROM_CONTINUE(             0x05800, 0x0800 )
	ROM_CONTINUE(             0x07800, 0x0800 )
	ROM_LOAD( "sprm.4m",      0x08000, 0x0800, 0xd6d07d70 )
	ROM_CONTINUE(             0x0a000, 0x0800 )
	ROM_CONTINUE(             0x08800, 0x0800 )
	ROM_CONTINUE(             0x0a800, 0x0800 )
	ROM_CONTINUE(             0x09000, 0x0800 )
	ROM_CONTINUE(             0x0b000, 0x0800 )
	ROM_CONTINUE(             0x09800, 0x0800 )
	ROM_CONTINUE(             0x0b800, 0x0800 )

	ROM_REGION( 0x0920, REGION_PROMS )
	ROM_LOAD( "sprm.2k",      0x0000, 0x0100, 0xfd8fa991 )	/* character palette red component */
	ROM_LOAD( "sprb.1m",      0x0100, 0x0100, 0x8d8cccad )	/* sprite palette red component */
	ROM_LOAD( "sprm.2j",      0x0200, 0x0100, 0x0e3890b4 )	/* character palette blue component */
	ROM_LOAD( "sprb.1n",      0x0300, 0x0100, 0xc40e1cb2 )	/* sprite palette green component */
	ROM_LOAD( "sprm.2h",      0x0400, 0x0100, 0x0478082b )	/* character palette green component */
	ROM_LOAD( "sprb.1l",      0x0500, 0x0100, 0x3ec46248 )	/* sprite palette blue component */
	ROM_LOAD( "sprb.5p",      0x0600, 0x0020, 0x746c6238 )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "sprm.8h",      0x0620, 0x0200, 0x875cc442 )	/* unknown */
	ROM_LOAD( "sprb.6f",      0x0820, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END

ROM_START( spelunk2 )
	ROM_REGION( 0x24000, REGION_CPU1 )	/* main CPU */
	ROM_LOAD( "sp2-a.4e",     0x00000, 0x4000, 0x96c04bbb )
	ROM_LOAD( "sp2-a.4d",     0x04000, 0x4000, 0xcb38c2ff )
	ROM_LOAD( "sp2-r.7d",     0x10000, 0x8000, 0x558837ea )	/* banked at 9000-9fff */
	ROM_LOAD( "sp2-r.7c",     0x18000, 0x8000, 0x4b380162 )	/* banked at 9000-9fff */
	ROM_LOAD( "sp2-r.7b",     0x20000, 0x4000, 0x7709a1fe )	/* banked at 8000-8fff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound CPU */
	ROM_LOAD( "sp2-a.3d",     0x8000, 0x04000, 0x839ec7e2 ) /* adpcm data */
	ROM_LOAD( "sp2-a.3f",     0xc000, 0x04000, 0xad3ce898 ) /* 6803 code */

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sp2-r.1d",     0x00000, 0x8000, 0xc19fa4c9 )	/* tiles */
	ROM_LOAD( "sp2-r.3b",     0x08000, 0x8000, 0x366604af )
	ROM_LOAD( "sp2-r.1b",     0x10000, 0x8000, 0x3a0c4d47 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sp2-b.4k",     0x00000, 0x4000, 0x6cb67a17 )	/* sprites */
	ROM_LOAD( "sp2-b.4f",     0x04000, 0x4000, 0xe4a1166f )
	ROM_LOAD( "sp2-b.3n",     0x08000, 0x4000, 0xf59e8b76 )
	ROM_LOAD( "sp2-b.4n",     0x0c000, 0x4000, 0xfa65bac9 )
	ROM_LOAD( "sp2-b.4c",     0x10000, 0x4000, 0x1caf7013 )
	ROM_LOAD( "sp2-b.4e",     0x14000, 0x4000, 0x780a463b )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sp2-r.4l",     0x00000, 0x0800, 0x6a4b2d8b )	/* chars */
	ROM_CONTINUE(             0x02000, 0x0800 )			/* first and second half identical, */
	ROM_CONTINUE(             0x00800, 0x0800 )			/* second half not used by the driver */
	ROM_CONTINUE(             0x02800, 0x0800 )
	ROM_CONTINUE(             0x00000, 0x0800 )
	ROM_CONTINUE(             0x03000, 0x0800 )
	ROM_CONTINUE(             0x00800, 0x0800 )
	ROM_CONTINUE(             0x03800, 0x0800 )
	ROM_LOAD( "sp2-r.4m",     0x04000, 0x0800, 0xe1368b61 )
	ROM_CONTINUE(             0x06000, 0x0800 )
	ROM_CONTINUE(             0x04800, 0x0800 )
	ROM_CONTINUE(             0x06800, 0x0800 )
	ROM_CONTINUE(             0x05000, 0x0800 )
	ROM_CONTINUE(             0x07000, 0x0800 )
	ROM_CONTINUE(             0x05800, 0x0800 )
	ROM_CONTINUE(             0x07800, 0x0800 )
	ROM_LOAD( "sp2-r.4p",     0x08000, 0x0800, 0xfc138e13 )
	ROM_CONTINUE(             0x0a000, 0x0800 )
	ROM_CONTINUE(             0x08800, 0x0800 )
	ROM_CONTINUE(             0x0a800, 0x0800 )
	ROM_CONTINUE(             0x09000, 0x0800 )
	ROM_CONTINUE(             0x0b000, 0x0800 )
	ROM_CONTINUE(             0x09800, 0x0800 )
	ROM_CONTINUE(             0x0b800, 0x0800 )

	ROM_REGION( 0x0a20, REGION_PROMS )
	ROM_LOAD( "sp2-r.1k",     0x0000, 0x0200, 0x31c1bcdc )	/* chars red and green component */
	ROM_LOAD( "sp2-r.2k",     0x0200, 0x0100, 0x1cf5987e )	/* chars blue component */
	ROM_LOAD( "sp2-r.2j",     0x0300, 0x0100, 0x1acbe2a5 )	/* chars blue component */
	ROM_LOAD( "sp2-b.1m",     0x0400, 0x0100, 0x906104c7 )	/* sprites red component */
	ROM_LOAD( "sp2-b.1n",     0x0500, 0x0100, 0x5a564c06 )	/* sprites green component */
	ROM_LOAD( "sp2-b.1l",     0x0600, 0x0100, 0x8f4a2e3c )	/* sprites blue component */
	ROM_LOAD( "sp2-b.5p",     0x0700, 0x0020, 0xcd126f6a )	/* sprite height, one entry per 32 */
	                                                        /* sprites. Used at run time! */
	ROM_LOAD( "sp2-r.8j",     0x0720, 0x0200, 0x875cc442 )	/* unknown */
	ROM_LOAD( "sp2-b.6f",     0x0920, 0x0100, 0x34d88d3c )	/* video timing? - common to the other games */
ROM_END



GAME( 1984, kungfum,  0,       kungfum,  kungfum,  0, ROT0,  "Irem", "Kung Fu Master" )
GAME( 1984, kungfud,  kungfum, kungfum,  kungfum,  0, ROT0,  "Irem (Data East license)", "Kung Fu Master (Data East)" )
GAME( 1984, spartanx, kungfum, kungfum,  kungfum,  0, ROT0,  "Irem", "Spartan X (Japan)" )
GAME( 1984, kungfub,  kungfum, kungfum,  kungfum,  0, ROT0,  "bootleg", "Kung Fu Master (bootleg set 1)" )
GAME( 1984, kungfub2, kungfum, kungfum,  kungfum,  0, ROT0,  "bootleg", "Kung Fu Master (bootleg set 2)" )
GAME( 1984, battroad, 0,       battroad, battroad, 0, ROT90, "Irem", "The Battle-Road" )
GAME( 1984, ldrun,    0,       ldrun,    ldrun,    0, ROT0,  "Irem (licensed from Broderbund)", "Lode Runner (set 1)" )
GAME( 1984, ldruna,   ldrun,   ldrun,    ldrun,    0, ROT0,  "Irem (licensed from Broderbund)", "Lode Runner (set 2)" )
GAME( 1984, ldrun2,   0,       ldrun2,   ldrun2,   0, ROT0,  "Irem (licensed from Broderbund)", "Lode Runner II - The Bungeling Strikes Back" )	/* Japanese version is called Bangeringu Teikoku No Gyakushuu */
GAME( 1985, ldrun3,   0,       ldrun3,   ldrun3,   0, ROT0,  "Irem (licensed from Broderbund)", "Lode Runner III - Majin No Fukkatsu" )
GAME( 1986, ldrun4,   0,       ldrun4,   ldrun4,   0, ROT0,  "Irem (licensed from Broderbund)", "Lode Runner IV - Teikoku Karano Dasshutsu" )
GAME( 1985, lotlot,   0,       lotlot,   lotlot,   0, ROT0,  "Irem (licensed from Tokuma Shoten)", "Lot Lot" )
GAMEX(1986, kidniki,  0,       kidniki,  kidniki,  0, ROT0,  "Irem (Data East USA license)", "Kid Niki - Radical Ninja (US)", GAME_IMPERFECT_SOUND )
GAMEX(1986, yanchamr, kidniki, kidniki,  kidniki,  0, ROT0,  "Irem", "Kaiketsu Yanchamaru (Japan)", GAME_IMPERFECT_SOUND )
GAME( 1985, spelunkr, 0,       spelunkr, spelunkr, 0, ROT0,  "Irem (licensed from Broderbund)", "Spelunker" )
GAME( 1986, spelunk2, 0,       spelunk2, spelunk2, 0, ROT0,  "Irem (licensed from Broderbund)", "Spelunker II" )
