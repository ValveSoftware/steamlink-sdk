#include "../vidhrdw/hyperspt.cpp"

/***************************************************************************

Based on drivers from Juno First emulator by Chris Hardy (chrish@kcbbs.gen.nz)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"


void konami1_decode(void);

extern unsigned char *hyperspt_scroll;

WRITE_HANDLER( hyperspt_flipscreen_w );
void hyperspt_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int hyperspt_vh_start(void);
void hyperspt_vh_stop(void);
void hyperspt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void roadf_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( konami_sh_irqtrigger_w );
READ_HANDLER( hyperspt_sh_timer_r );
WRITE_HANDLER( hyperspt_sound_w );

/* these routines lurk in sndhrdw/trackfld.c */
extern struct VLM5030interface konami_vlm5030_interface;
extern struct SN76496interface konami_sn76496_interface;
extern struct DACinterface konami_dac_interface;
WRITE_HANDLER( konami_SN76496_latch_w );
WRITE_HANDLER( konami_SN76496_0_w );


/* handle fake button for speed cheat */
static READ_HANDLER( konami_IN1_r )
{
	int res;
	static int cheat = 0;
	static int bits[] = { 0xee, 0xff, 0xbb, 0xaa };

	res = readinputport(1);

	if ((res & 0x80) == 0)
	{
		res |= 0x55;
		res &= bits[cheat];
		cheat = (++cheat)%4;
	}
	return res;
}



/*
 Track'n'Field has 1k of battery backed RAM which can be erased by setting a dipswitch
*/
static unsigned char *nvram;
static size_t nvram_size;
static int we_flipped_the_switch;

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
	{
		osd_fwrite(file,nvram,nvram_size);

		if (we_flipped_the_switch)
		{
			struct InputPort *in;


			/* find the dip switch which resets the high score table, and set it */
			/* back to off. */
			in = Machine->input_ports;

			while (in->type != IPT_END)
			{
				if (in->name != NULL && in->name != IP_NAME_DEFAULT &&
						strcmp(in->name,"World Records") == 0)
				{
					if (in->default_value == 0)
						in->default_value = in->mask;
					break;
				}

				in++;
			}

			we_flipped_the_switch = 0;
		}
	}
	else
	{
		if (file)
		{
			osd_fread(file,nvram,nvram_size);
			we_flipped_the_switch = 0;
		}
		else
		{
			struct InputPort *in;


			/* find the dip switch which resets the high score table, and set it on */
			in = Machine->input_ports;

			while (in->type != IPT_END)
			{
				if (in->name != NULL && in->name != IP_NAME_DEFAULT &&
						strcmp(in->name,"World Records") == 0)
				{
					if (in->default_value == in->mask)
					{
						in->default_value = 0;
						we_flipped_the_switch = 1;
					}
					break;
				}

				in++;
			}
		}
	}
}



static struct MemoryReadAddress hyperspt_readmem[] =
{
	{ 0x1000, 0x10ff, MRA_RAM },
	{ 0x1600, 0x1600, input_port_4_r }, /* DIP 2 */
	{ 0x1680, 0x1680, input_port_0_r }, /* IO Coin */
//	{ 0x1681, 0x1681, input_port_1_r }, /* P1 IO */
	{ 0x1681, 0x1681, konami_IN1_r }, /* P1 IO and handle fake button for cheating */
	{ 0x1682, 0x1682, input_port_2_r }, /* P2 IO */
	{ 0x1683, 0x1683, input_port_3_r }, /* DIP 1 */
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress roadf_readmem[] =
{
	{ 0x1000, 0x10ff, MRA_RAM },
	{ 0x1600, 0x1600, input_port_4_r }, /* DIP 2 */
	{ 0x1680, 0x1680, input_port_0_r }, /* IO Coin */
	{ 0x1681, 0x1681, input_port_1_r }, /* P1 IO */
	{ 0x1682, 0x1682, input_port_2_r }, /* P2 IO */
	{ 0x1683, 0x1683, input_port_3_r }, /* DIP 1 */
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x1000, 0x10bf, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x10C0, 0x10ff, MWA_RAM, &hyperspt_scroll },  /* Scroll amount */
	{ 0x1400, 0x1400, watchdog_reset_w },
	{ 0x1480, 0x1480, hyperspt_flipscreen_w },
	{ 0x1481, 0x1481, konami_sh_irqtrigger_w },  /* cause interrupt on audio CPU */
	{ 0x1483, 0x1484, coin_counter_w },
	{ 0x1487, 0x1487, interrupt_enable_w },  /* Interrupt enable */
	{ 0x1500, 0x1500, soundlatch_w },
	{ 0x2000, 0x27ff, videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, colorram_w, &colorram },
	{ 0x3000, 0x37ff, MWA_RAM },
	{ 0x3800, 0x3fff, MWA_RAM, &nvram, &nvram_size },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x4fff, MRA_RAM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ 0x8000, 0x8000, hyperspt_sh_timer_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x4fff, MWA_RAM },
	{ 0xa000, 0xa000, VLM5030_data_w }, /* speech data */
	{ 0xc000, 0xdfff, hyperspt_sound_w },     /* speech and output controll */
	{ 0xe000, 0xe000, DAC_0_data_w },
	{ 0xe001, 0xe001, konami_SN76496_latch_w },  /* Loads the snd command into the snd latch */
	{ 0xe002, 0xe002, konami_SN76496_0_w },      /* This address triggers the SN chip to read the data port. */
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( hyperspt )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
//	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	/* Fake button to press buttons 1 and 3 impossibly fast. Handle via konami_IN1_r */
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_CHEAT | IPF_PLAYER1, "Run Like Hell Cheat", IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 /*| IPF_COCKTAIL*/ )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 /*| IPF_COCKTAIL*/ )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 /*| IPF_COCKTAIL*/ )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 /*| IPF_COCKTAIL*/ )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 /*| IPF_COCKTAIL*/ )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 /*| IPF_COCKTAIL*/ )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
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
	PORT_DIPSETTING(    0x00, "Disabled" )
/* 0x00 disables Coin 2. It still accepts coins and makes the sound, but
   it doesn't give you any credit */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "After Last Event" )
	PORT_DIPSETTING(    0x01, "Game Over" )
	PORT_DIPSETTING(    0x00, "Game Continues" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "World Records" )
	PORT_DIPSETTING(    0x08, "Don't Erase" )
	PORT_DIPSETTING(    0x00, "Erase on Reset" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xf0, "Easy 1" )
	PORT_DIPSETTING(    0xe0, "Easy 2" )
	PORT_DIPSETTING(    0xd0, "Easy 3" )
	PORT_DIPSETTING(    0xc0, "Easy 4" )
	PORT_DIPSETTING(    0xb0, "Normal 1" )
	PORT_DIPSETTING(    0xa0, "Normal 2" )
	PORT_DIPSETTING(    0x90, "Normal 3" )
	PORT_DIPSETTING(    0x80, "Normal 4" )
	PORT_DIPSETTING(    0x70, "Normal 5" )
	PORT_DIPSETTING(    0x60, "Normal 6" )
	PORT_DIPSETTING(    0x50, "Normal 7" )
	PORT_DIPSETTING(    0x40, "Normal 8" )
	PORT_DIPSETTING(    0x30, "Difficult 1" )
	PORT_DIPSETTING(    0x20, "Difficult 2" )
	PORT_DIPSETTING(    0x10, "Difficult 3" )
	PORT_DIPSETTING(    0x00, "Difficult 4" )
INPUT_PORTS_END

INPUT_PORTS_START( roadf )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* the game doesn't boot if this is 1 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
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
	PORT_DIPSETTING(    0x00, "Disabled" )
/* 0x00 disables Coin 2. It still accepts coins and makes the sound, but
   it doesn't give you any credit */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x06, 0x06, "Number of Opponents" )
	PORT_DIPSETTING(    0x06, "Easy" )
	PORT_DIPSETTING(    0x04, "Medium" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x08, 0x08, "Speed of Opponents" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x30, 0x30, "Fuel Consumption" )
	PORT_DIPSETTING(    0x30, "Easy" )
	PORT_DIPSETTING(    0x20, "Medium" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout hyperspt_charlayout =
{
	8,8,	/* 8*8 sprites */
	1024,	/* 1024 characters */
	4,	/* 4 bits per pixel */
	{ 0x4000*8+4, 0x4000*8+0, 4, 0  },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout hyperspt_spritelayout =
{
	16,16,	/* 16*16 sprites */
	512,	/* 512 sprites */
	4,	/* 4 bits per pixel */
	{ 0x8000*8+4, 0x8000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,
		32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8, },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo hyperspt_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &hyperspt_charlayout,       0, 16 },
	{ REGION_GFX2, 0, &hyperspt_spritelayout, 16*16, 16 },
	{ -1 } /* end of array */
};


static struct GfxLayout roadf_charlayout =
{
	8,8,	/* 8*8 sprites */
	1536,	/* 1536 characters */
	4,	/* 4 bits per pixel */
	{ 0x6000*8+4, 0x6000*8+0, 4, 0  },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout roadf_spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	4,	/* 4 bits per pixel */
	{ 0x4000*8+4, 0x4000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,
		32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8, },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo roadf_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &roadf_charlayout,       0, 16 },
	{ REGION_GFX2, 0, &roadf_spritelayout, 16*16, 16 },
	{ -1 } /* end of array */
};



/* filename for hyper sports sample files */
static const char *hyperspt_sample_names[] =
{
	"*hyperspt",
	"00.wav","01.wav","02.wav","03.wav","04.wav","05.wav","06.wav","07.wav",
	"08.wav","09.wav","0a.wav","0b.wav","0c.wav","0d.wav","0e.wav","0f.wav",
	"10.wav","11.wav","12.wav","13.wav","14.wav","15.wav","16.wav","17.wav",
	"18.wav","19.wav","1a.wav","1b.wav","1c.wav","1d.wav","1e.wav","1f.wav",
	"20.wav","21.wav","22.wav","23.wav","24.wav","25.wav","26.wav","27.wav",
	"28.wav","29.wav","2a.wav","2b.wav","2c.wav","2d.wav","2e.wav","2f.wav",
	"30.wav","31.wav","32.wav","33.wav","34.wav","35.wav","36.wav","37.wav",
	"38.wav","39.wav","3a.wav","3b.wav","3c.wav","3d.wav","3e.wav","3f.wav",
	"40.wav","41.wav","42.wav","43.wav","44.wav","45.wav","46.wav","47.wav",
	"48.wav","49.wav",
	0
};

struct VLM5030interface hyperspt_vlm5030_interface =
{
	3580000,    /* master clock  */
	255,        /* volume        */
	REGION_SOUND1,	/* memory region  */
	0,         /* memory size    */
	0,         /* VCU            */
	hyperspt_sample_names
};


static struct MachineDriver machine_driver_hyperspt =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2048000,        /* 1.400 Mhz ??? */
			hyperspt_readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318180/4,	/* Z80 Clock is derived from a 14.31818 Mhz crystal */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	hyperspt_gfxdecodeinfo,
	32,16*16+16*16,
	hyperspt_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	hyperspt_vh_start,
	hyperspt_vh_stop,
	hyperspt_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&konami_dac_interface
		},
		{
			SOUND_SN76496,
			&konami_sn76496_interface
		},
		{
			SOUND_VLM5030,
			&hyperspt_vlm5030_interface
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_roadf =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2048000,        /* 1.400 Mhz ??? */
			roadf_readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318180/4,	/* Z80 Clock is derived from a 14.31818 Mhz crystal */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	roadf_gfxdecodeinfo,
	32,16*16+16*16,
	hyperspt_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	hyperspt_vh_start,
	hyperspt_vh_stop,
	roadf_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&konami_dac_interface
		},
		{
			SOUND_SN76496,
			&konami_sn76496_interface
		},
		{
			SOUND_VLM5030,
			&konami_vlm5030_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( hyperspt )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "c01",          0x4000, 0x2000, 0x0c720eeb )
	ROM_LOAD( "c02",          0x6000, 0x2000, 0x560258e0 )
	ROM_LOAD( "c03",          0x8000, 0x2000, 0x9b01c7e6 )
	ROM_LOAD( "c04",          0xa000, 0x2000, 0x10d7e9a2 )
	ROM_LOAD( "c05",          0xc000, 0x2000, 0xb105a8cd )
	ROM_LOAD( "c06",          0xe000, 0x2000, 0x1a34a849 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "c10",          0x0000, 0x2000, 0x3dc1a6ff )
	ROM_LOAD( "c09",          0x2000, 0x2000, 0x9b525c3e )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c26",          0x00000, 0x2000, 0xa6897eac )
	ROM_LOAD( "c24",          0x02000, 0x2000, 0x5fb230c0 )
	ROM_LOAD( "c22",          0x04000, 0x2000, 0xed9271a0 )
	ROM_LOAD( "c20",          0x06000, 0x2000, 0x183f4324 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c14",          0x00000, 0x2000, 0xc72d63be )
	ROM_LOAD( "c13",          0x02000, 0x2000, 0x76565608 )
	ROM_LOAD( "c12",          0x04000, 0x2000, 0x74d2cc69 )
	ROM_LOAD( "c11",          0x06000, 0x2000, 0x66cbcb4d )
	ROM_LOAD( "c18",          0x08000, 0x2000, 0xed25e669 )
	ROM_LOAD( "c17",          0x0a000, 0x2000, 0xb145b39f )
	ROM_LOAD( "c16",          0x0c000, 0x2000, 0xd7ff9f2b )
	ROM_LOAD( "c15",          0x0e000, 0x2000, 0xf3d454e6 )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "c03_c27.bin",  0x0000, 0x0020, 0xbc8a5956 )
	ROM_LOAD( "j12_c28.bin",  0x0020, 0x0100, 0x2c891d59 )
	ROM_LOAD( "a09_c29.bin",  0x0120, 0x0100, 0x811a3f3f )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/*  64k for speech rom    */
	ROM_LOAD( "c08",          0x0000, 0x2000, 0xe8f8ea78 )
ROM_END

ROM_START( hpolym84 )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "c01",          0x4000, 0x2000, 0x0c720eeb )
	ROM_LOAD( "c02",          0x6000, 0x2000, 0x560258e0 )
	ROM_LOAD( "c03",          0x8000, 0x2000, 0x9b01c7e6 )
	ROM_LOAD( "330e04.bin",   0xa000, 0x2000, 0x9c5e2934 )
	ROM_LOAD( "c05",          0xc000, 0x2000, 0xb105a8cd )
	ROM_LOAD( "c06",          0xe000, 0x2000, 0x1a34a849 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "c10",          0x0000, 0x2000, 0x3dc1a6ff )
	ROM_LOAD( "c09",          0x2000, 0x2000, 0x9b525c3e )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c26",          0x00000, 0x2000, 0xa6897eac )
	ROM_LOAD( "330e24.bin",   0x02000, 0x2000, 0xf9bbfe1d )
	ROM_LOAD( "c22",          0x04000, 0x2000, 0xed9271a0 )
	ROM_LOAD( "330e20.bin",   0x06000, 0x2000, 0x29969b92 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c14",          0x00000, 0x2000, 0xc72d63be )
	ROM_LOAD( "c13",          0x02000, 0x2000, 0x76565608 )
	ROM_LOAD( "c12",          0x04000, 0x2000, 0x74d2cc69 )
	ROM_LOAD( "c11",          0x06000, 0x2000, 0x66cbcb4d )
	ROM_LOAD( "c18",          0x08000, 0x2000, 0xed25e669 )
	ROM_LOAD( "c17",          0x0a000, 0x2000, 0xb145b39f )
	ROM_LOAD( "c16",          0x0c000, 0x2000, 0xd7ff9f2b )
	ROM_LOAD( "c15",          0x0e000, 0x2000, 0xf3d454e6 )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "c03_c27.bin",  0x0000, 0x0020, 0xbc8a5956 )
	ROM_LOAD( "j12_c28.bin",  0x0020, 0x0100, 0x2c891d59 )
	ROM_LOAD( "a09_c29.bin",  0x0120, 0x0100, 0x811a3f3f )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/*  64k for speech rom    */
	ROM_LOAD( "c08",          0x0000, 0x2000, 0xe8f8ea78 )
ROM_END

ROM_START( roadf )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "g05_g01.bin",  0x4000, 0x2000, 0xe2492a06 )
	ROM_LOAD( "g07_f02.bin",  0x6000, 0x2000, 0x0bf75165 )
	ROM_LOAD( "g09_g03.bin",  0x8000, 0x2000, 0xdde401f8 )
	ROM_LOAD( "g11_f04.bin",  0xA000, 0x2000, 0xb1283c77 )
	ROM_LOAD( "g13_f05.bin",  0xC000, 0x2000, 0x0ad4d796 )
	ROM_LOAD( "g15_f06.bin",  0xE000, 0x2000, 0xfa42e0ed )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "a17_d10.bin",  0x0000, 0x2000, 0xc33c927e )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a14_e26.bin",  0x00000, 0x4000, 0xf5c738e2 )
	ROM_LOAD( "a12_d24.bin",  0x04000, 0x2000, 0x2d82c930 )
	ROM_LOAD( "c14_e22.bin",  0x06000, 0x4000, 0xfbcfbeb9 )
	ROM_LOAD( "c12_d20.bin",  0x0a000, 0x2000, 0x5e0cf994 )

	ROM_REGION( 0x08000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "j19_e14.bin",  0x00000, 0x4000, 0x16d2bcff )
	ROM_LOAD( "g19_e18.bin",  0x04000, 0x4000, 0x490685ff )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "c03_c27.bin",  0x0000, 0x0020, 0x45d5e352 )
	ROM_LOAD( "j12_c28.bin",  0x0020, 0x0100, 0x2955e01f )
	ROM_LOAD( "a09_c29.bin",  0x0120, 0x0100, 0x5b3b5f2a )
ROM_END

ROM_START( roadf2 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "5g",           0x4000, 0x2000, 0xd8070d30 )
	ROM_LOAD( "6g",           0x6000, 0x2000, 0x8b661672 )
	ROM_LOAD( "8g",           0x8000, 0x2000, 0x714929e8 )
	ROM_LOAD( "11g",          0xA000, 0x2000, 0x0f2c6b94 )
	ROM_LOAD( "g13_f05.bin",  0xC000, 0x2000, 0x0ad4d796 )
	ROM_LOAD( "g15_f06.bin",  0xE000, 0x2000, 0xfa42e0ed )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "a17_d10.bin",  0x0000, 0x2000, 0xc33c927e )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a14_e26.bin",  0x00000, 0x4000, 0xf5c738e2 )
	ROM_LOAD( "a12_d24.bin",  0x04000, 0x2000, 0x2d82c930 )
	ROM_LOAD( "c14_e22.bin",  0x06000, 0x4000, 0xfbcfbeb9 )
	ROM_LOAD( "c12_d20.bin",  0x0a000, 0x2000, 0x5e0cf994 )

	ROM_REGION( 0x08000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "j19_e14.bin",  0x00000, 0x4000, 0x16d2bcff )
	ROM_LOAD( "g19_e18.bin",  0x04000, 0x4000, 0x490685ff )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "c03_c27.bin",  0x0000, 0x0020, 0x45d5e352 )
	ROM_LOAD( "j12_c28.bin",  0x0020, 0x0100, 0x2955e01f )
	ROM_LOAD( "a09_c29.bin",  0x0120, 0x0100, 0x5b3b5f2a )
ROM_END


static void init_hyperspt(void)
{
	konami1_decode();
}


GAME( 1984, hyperspt, 0,        hyperspt, hyperspt, hyperspt, ROT0,  "Konami (Centuri license)", "Hyper Sports" )
GAME( 1984, hpolym84, hyperspt, hyperspt, hyperspt, hyperspt, ROT0,  "Konami", "Hyper Olympics '84" )
GAME( 1984, roadf,    0,        roadf,    roadf,    hyperspt, ROT90, "Konami", "Road Fighter (set 1)" )
GAME( 1984, roadf2,   roadf,    roadf,    roadf,    hyperspt, ROT90, "Konami", "Road Fighter (set 2)" )
