#include "../vidhrdw/macross.cpp"

/********************************************************************

Macross 2

driver by Mirko Buffoni


Memory layout:

000000-07FFFF	68k rom
088000-0885FF	15 bit palette ram (RRRRGGGGBBBBRGBx)
									4321^^^^^^^^0^^
									    4321|||| 0|
									    	4321  0

09C000-09C7FF	VideoRam	(8x8 tiles, 256x256)
0F0000-0FFFFF	Work ram

----

Audio system:  2 x OKI M6295


********************************************************************/


#include "driver.h"

extern unsigned char *macross_workram;
extern unsigned char *macross_spriteram;
extern unsigned char *macross_txvideoram;
extern unsigned char *macross_videocontrol;
extern size_t macross_txvideoram_size;


READ_HANDLER( macross_txvideoram_r );
WRITE_HANDLER( macross_txvideoram_w );
WRITE_HANDLER( macross_paletteram_w );





int  macross_vh_start(void);
void macross_vh_stop(void);
void macross_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);




static int respcount = 0;
static int resp[] = {	0x82, 0xc7, 0x00,
						0x2c, 0x6c, 0x00,
						0x9f, 0xc7, 0x00,
						0x29, 0x69, 0x00,
						0x8b, 0xc7, 0x00 };


READ_HANDLER( macross_protection_r )
{
	int res = resp[ respcount++ ];
	if (respcount >= 15) respcount = 0;

	return res;
}

WRITE_HANDLER( macross_paletteram_w )
{
	int r,g,b;
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);


	WRITE_WORD(&paletteram[offset],newword);

	r = ((newword >> 11) & 0x1e) | ((newword >> 3) & 0x01);
	g = ((newword >>  7) & 0x1e) | ((newword >> 2) & 0x01);
	b = ((newword >>  3) & 0x1e) | ((newword >> 1) & 0x01);

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	palette_change_color(offset / 2,r,g,b);
}


WRITE_HANDLER( macross_oki6295_bankswitch_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		switch (offset)
		{
			case 0x00:	OKIM6295_set_bank_base(0, 0, (data & 0x0f) * 0x10000);	break;
			case 0x02:	OKIM6295_set_bank_base(0, 1, (data & 0x0f) * 0x10000);	break;
			case 0x04:	OKIM6295_set_bank_base(0, 2, (data & 0x0f) * 0x10000);	break;
			case 0x06:	OKIM6295_set_bank_base(0, 3, (data & 0x0f) * 0x10000);	break;
			case 0x08:	OKIM6295_set_bank_base(1, 0, (data & 0x0f) * 0x10000);	break;
			case 0x0a:	OKIM6295_set_bank_base(1, 1, (data & 0x0f) * 0x10000);	break;
			case 0x0c:	OKIM6295_set_bank_base(1, 2, (data & 0x0f) * 0x10000);	break;
			case 0x0e:	OKIM6295_set_bank_base(1, 3, (data & 0x0f) * 0x10000);	break;
		}
	}
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x080000, 0x080001, input_port_0_r },
	{ 0x080002, 0x080003, input_port_1_r },
	{ 0x080008, 0x080009, input_port_2_r },
	{ 0x08000a, 0x08000b, input_port_3_r },
	{ 0x08000e, 0x08000f, macross_protection_r },
	{ 0x088000, 0x0885ff, paletteram_word_r },
	{ 0x090000, 0x093fff, MRA_BANK2 },	/* BG RAM */
	{ 0x09c000, 0x09c7ff, macross_txvideoram_r },
	{ 0x0f0000, 0x0fffff, MRA_BANK1 },	/* Work RAM */
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080014, 0x080015, MWA_NOP },
//	{ 0x08001e, 0x08001f, MWA_NOP },	// Protection chip, not emulated
	{ 0x088000, 0x0885ff, macross_paletteram_w, &paletteram },
	{ 0x08c000, 0x08c00f, macross_oki6295_bankswitch_w },
	{ 0x090000, 0x093fff, MWA_BANK2, &macross_videocontrol },	/* BG RAM */
	{ 0x09c000, 0x09c7ff, macross_txvideoram_w, &macross_txvideoram, &macross_txvideoram_size },
	{ 0x0f0000, 0x0fffff, MWA_BANK1, &macross_workram },	/* Work RAM */
	{ -1 }
};


INPUT_PORTS_START( macross )

	PORT_START		/* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START3 )	/* Service */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* Maybe unused */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* Maybe unused */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* Maybe unused */

	PORT_START      /* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW A */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Language" )
	PORT_DIPSETTING(    0x08, "Japanese" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )

INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	4096,   /* 2048 characters */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8   /* every char takes 32 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	8,8,    /* 8*8 characters */
	65536,  /* 65536 characters */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8   /* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,   /* 16*16 sprites */
	16384,   /* 16384 sprites */
	4,       /* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the bitplanes are packed in one nibble */
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4,
		16*32+2*4, 16*32+3*4, 16*32+0*4, 16*32+1*4, 16*32+6*4, 16*32+7*4, 16*32+4*4, 16*32+5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
		8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	4*32*8   /* every sprites takes 256 consecutive bytes */
};

static struct GfxDecodeInfo macross_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   512, 16 },	/* Chars */
	{ REGION_GFX2, 0, &tilelayout,     0, 16 },	/* Tiles */
	{ REGION_GFX3, 0, &spritelayout, 256, 16 },	/* Sprites */
	{ -1 } /* end of array */
};


static struct OKIM6295interface okim6295_interface =
{
	2,              /* 2 chip */
	{ 22050, 22050 },         /* 22050Hz frequency? */
	{ REGION_SOUND1, REGION_SOUND2 },        /* memory region 2,3 */
	{ 50,50 }
};



static struct MachineDriver machine_driver_macross =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000, /* 10.0 MHz ? */
			readmem,writemem,0,0,
			m68_level4_irq,1,
			m68_level1_irq,102
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	256, 256, { 0*8, 32*8-1, 2*8, 30*8-1 },
	macross_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	macross_vh_start,
	macross_vh_stop,
	macross_vh_screenrefresh,

	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};




ROM_START( macross )
	ROM_REGION( 0x80000, REGION_CPU1 )		/* 68000 code */
	ROM_LOAD( "921a03",  0x00000, 0x80000, 0x33318d55 )	/* 68000 code */

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "921a01",		0x000000, 0x20000, 0xbbd8242d )	/* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "921a04",		0x000000, 0x200000, 0x4002e4bb )	/* 16x16 tiles */

	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "921a07",		0x000000, 0x200000, 0x7d2bf112 )	/* Sprites */

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* 512Kb for ADPCM sounds - sound chip is OKIM6295 */
	ROM_LOAD( "921a05",    0x000000, 0x80000, 0xd5a1eddd )

	ROM_REGION( 0x80000, REGION_SOUND2 )	/* 512Kb for ADPCM sounds - sound chip is OKIM6295 */
	ROM_LOAD( "921a06",    0x000000, 0x80000, 0x89461d0f )
ROM_END






static unsigned char decode_byte(unsigned char src, unsigned char *bitp)
{
	unsigned char ret, i;

	ret = 0;
	for (i=0; i<8; i++)
		ret |= (((src >> bitp[i]) & 1) << (7-i));

	return ret;
}

unsigned long macross_address_map_bg0(unsigned long addr)
{
   return ((addr&0x00004)>> 2) | ((addr&0x00800)>> 10) | ((addr&0x40000)>>16);
}


static unsigned short decode_word(unsigned short src, unsigned char *bitp)
{
	unsigned short ret, i;

	ret=0;
	for (i=0; i<16; i++)
		ret |= (((src >> bitp[i]) & 1) << (15-i));

	return ret;
}


unsigned long macross_address_map_sprites(unsigned long addr)
{
   return ((addr&0x00010)>> 4) | ((addr&0x20000)>>16) | ((addr&0x100000)>>18);
}


void init_macross(void)
{
	/* GFX are scrambled.  We decode them here.  (BIG Thanks to Antiriad for descrambling info) */
	unsigned char *rom;

	static unsigned char decode_data_bg[8][8] =
	{
		{0x3,0x0,0x7,0x2,0x5,0x1,0x4,0x6},
		{0x1,0x2,0x6,0x5,0x4,0x0,0x3,0x7},
		{0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
		{0x7,0x6,0x5,0x0,0x1,0x4,0x3,0x2},
		{0x2,0x0,0x1,0x4,0x3,0x5,0x7,0x6},
		{0x5,0x3,0x7,0x0,0x4,0x6,0x2,0x1},
		{0x2,0x7,0x0,0x6,0x5,0x3,0x1,0x4},
		{0x3,0x4,0x7,0x6,0x2,0x0,0x5,0x1},
	};

	static unsigned char decode_data_sprite[8][16] =
	{
		{0x9,0x3,0x4,0x5,0x7,0x1,0xB,0x8,0x0,0xD,0x2,0xC,0xE,0x6,0xF,0xA},
		{0x1,0x3,0xC,0x4,0x0,0xF,0xB,0xA,0x8,0x5,0xE,0x6,0xD,0x2,0x7,0x9},
		{0xF,0xE,0xD,0xC,0xB,0xA,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
		{0xF,0xE,0xC,0x6,0xA,0xB,0x7,0x8,0x9,0x2,0x3,0x4,0x5,0xD,0x1,0x0},

		{0x1,0x6,0x2,0x5,0xF,0x7,0xB,0x9,0xA,0x3,0xD,0xE,0xC,0x4,0x0,0x8}, /* Haze 20/07/00 */
		{0x7,0x5,0xD,0xE,0xB,0xA,0x0,0x1,0x9,0x6,0xC,0x2,0x3,0x4,0x8,0xF}, /* Haze 20/07/00 */
		{0x0,0x5,0x6,0x3,0x9,0xB,0xA,0x7,0x1,0xD,0x2,0xE,0x4,0xC,0x8,0xF}, /* Antiriad, Corrected by Haze 20/07/00 */
		{0x9,0xC,0x4,0x2,0xF,0x0,0xB,0x8,0xA,0xD,0x3,0x6,0x5,0xE,0x1,0x7}, /* Antiriad, Corrected by Haze 20/07/00 */
	};


	int A;


	/* background */
	rom = memory_region(REGION_GFX2);
	for (A = 0;A < memory_region_length(REGION_GFX2);A++)
	{
		rom[A] = decode_byte( rom[A], decode_data_bg[macross_address_map_bg0(A)]);
	}

	/* sprites */
	rom = memory_region(REGION_GFX3);
	for (A = 0;A < memory_region_length(REGION_GFX3);A += 2)
	{
		unsigned short tmp = decode_word( rom[A]*256 + rom[A+1], decode_data_sprite[macross_address_map_sprites(A)]);
		rom[A] = tmp >> 8;
		rom[A+1] = tmp & 0xff;
	}
}




GAME( 1992, macross, 0, macross, macross, macross, ROT270, "NMK + Big West", "Macross" )
