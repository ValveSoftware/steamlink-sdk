/*
	Hal21 (sound not working, missing color proms, possibly bad tile gfx ROMs)
	ASO (seems fine)
	Alpha Mission ('p3.6d' is a bad dump)

	todo:
	- hal21 sound (2xAY8192)
	- hal21 gfx
	- hal21 colors
	- sound cpu status needs hooked up in both games
	- virtualize palette (background palette is bank selected) for further speedup
*/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


extern void tnk3_draw_text( struct osd_bitmap *bitmap, int bank, unsigned char *source );
extern void tnk3_draw_status( struct osd_bitmap *bitmap, int bank, unsigned char *source );

static int scrollx_base; /* this is the only difference in video hardware found so far */

static int common_vh_start( void ){
	dirtybuffer = (unsigned char*)malloc( 64*64 );
	if( dirtybuffer ){
		tmpbitmap = bitmap_alloc( 512, 512 );
		if( tmpbitmap ){
			memset( dirtybuffer, 1, 64*64  );
			return 0;
		}
		free( dirtybuffer );
	}
	return 1;
}

int aso_vh_start( void ){
	scrollx_base = -16;
	return common_vh_start();
}

int hal21_vh_start( void ){
	scrollx_base = 240;
	return common_vh_start();
}

void aso_vh_stop( void ){
	bitmap_free( tmpbitmap );
	free( dirtybuffer );
}


void aso_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom){
	int i;
	int num_colors = 1024;
/* palette format is RRRG GGBB B??? the three unknown bits are used but */
/* I'm not sure how, I'm currently using them as least significant bit but */
/* that's most likely wrong. */
	for( i=0; i<num_colors; i++ ){
		int bit0=0,bit1,bit2,bit3;

		colortable[i] = i;

		bit0 = (color_prom[2*num_colors] >> 2) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*palette++ = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[2*num_colors] >> 1) & 0x01;
		bit1 = (color_prom[num_colors] >> 2) & 0x01;
		bit2 = (color_prom[num_colors] >> 3) & 0x01;
		bit3 = (color_prom[0] >> 0) & 0x01;
		*palette++ = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[2*num_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*num_colors] >> 3) & 0x01;
		bit2 = (color_prom[num_colors] >> 0) & 0x01;
		bit3 = (color_prom[num_colors] >> 1) & 0x01;
		*palette++ = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}
}

static void aso_draw_background(
		struct osd_bitmap *bitmap,
		int scrollx, int scrolly,
		int bank, int color,
		const struct GfxElement *gfx )
{
	const struct rectangle *clip = &Machine->visible_area;
	int offs;

	static int old_bank, old_color;

	if( color!=old_color || bank!=old_bank ){
		memset( dirtybuffer, 1, 64*64  );
		old_bank = bank;
		old_color = color;
	}

	for( offs=0; offs<64*64; offs++ ){
		if( dirtybuffer[offs] ){
			int tile_number = videoram[offs]+bank*256;
			int sy = (offs%64)*8;
			int sx = (offs/64)*8;

			drawgfx( tmpbitmap,gfx,
				tile_number,
				color,
				0,0, /* no flip */
				sx,sy,
				0,TRANSPARENCY_NONE,0);

			dirtybuffer[offs] = 0;
		}
	}

	copyscrollbitmap(bitmap,tmpbitmap,
		1,&scrollx,1,&scrolly,
		clip,
		TRANSPARENCY_NONE,0);
}

void aso_draw_sprites(
		struct osd_bitmap *bitmap,
		int xscroll, int yscroll,
		const struct GfxElement *gfx
){
	const unsigned char *source = spriteram;
	const unsigned char *finish = source+60*4;

	struct rectangle clip = Machine->visible_area;

	while( source<finish ){
		int attributes = source[3]; /* YBBX.CCCC */
		int tile_number = source[1];
		int sy = source[0] + ((attributes&0x10)?256:0) - yscroll;
		int sx = source[2] + ((attributes&0x80)?256:0) - xscroll;
		int color = attributes&0xf;

		if( !(attributes&0x20) ) tile_number += 512;
		if( attributes&0x40 ) tile_number += 256;

		drawgfx(bitmap,gfx,
			tile_number,
			color,
			0,0,
			(256-sx)&0x1ff,sy&0x1ff,
			&clip,TRANSPARENCY_PEN,7);

		source+=4;
	}
}

int hal21_vreg[6];

WRITE_HANDLER( hal21_vreg0_w ){ hal21_vreg[0] = data; }
WRITE_HANDLER( hal21_vreg1_w ){ hal21_vreg[1] = data; }
WRITE_HANDLER( hal21_vreg2_w ){ hal21_vreg[2] = data; }
WRITE_HANDLER( hal21_vreg3_w ){ hal21_vreg[3] = data; }
WRITE_HANDLER( hal21_vreg4_w ){ hal21_vreg[4] = data; }
WRITE_HANDLER( hal21_vreg5_w ){ hal21_vreg[5] = data; }

void aso_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh ){
	unsigned char *ram = memory_region(REGION_CPU1);
	int attributes = hal21_vreg[1];
	{
		unsigned char bg_attrs = hal21_vreg[0];
		int scrolly = -8+hal21_vreg[4]+((attributes&0x10)?256:0);
		int scrollx = scrollx_base + hal21_vreg[5]+((attributes&0x02)?0:256);

		aso_draw_background( bitmap, -scrollx, -scrolly,
			bg_attrs>>4, /* tile bank */
			bg_attrs&0xf, /* color bank */
			Machine->gfx[1]
		);
	}

	{
		int scrollx = 0x1e + hal21_vreg[3] + ((attributes&0x01)?256:0);
		int scrolly = -8+0x11+hal21_vreg[2] + ((attributes&0x08)?256:0);
		aso_draw_sprites( bitmap, scrollx, scrolly, Machine->gfx[2] );
	}

	{
		int bank = (attributes&0x40)?1:0;
		tnk3_draw_text( bitmap, bank, &ram[0xf800] );
		tnk3_draw_status( bitmap, bank, &ram[0xfc00] );
	}
/*
	{
		int i;
		for( i=0; i<6; i++ ){
			int data = hal21_vreg[i];
			drawgfx( bitmap, Machine->uifont,
				"0123456789abcdef"[data>>4],0,0,0,
				0,i*16,
				&Machine->visible_area,
				TRANSPARENCY_NONE,0 );
			drawgfx( bitmap, Machine->uifont,
				"0123456789abcdef"[data&0xf],0,0,0,
				8,i*16,
				&Machine->visible_area,
				TRANSPARENCY_NONE,0 );
		}
	}
*/
}


INPUT_PORTS_START( hal21 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,	IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,	IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,	IPT_UNKNOWN ) /* sound CPU status */
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START /* P1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* P2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) ) /* unused */
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) ) /* ? */
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0xc0, "20000 60000" )
	PORT_DIPSETTING(    0x80, "40000 90000" )
	PORT_DIPSETTING(	0x40, "50000 120000" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, "Bonus Type" )
	PORT_DIPSETTING(    0x00, "Every Bonus Set" )
	PORT_DIPSETTING(    0x01, "Second Bonus Set" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x06, "4" )
	PORT_DIPNAME( 0x18, 0x18, "Special" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, DEF_STR( Demo_Sounds) )
	PORT_DIPSETTING(    0x08, "Infinite Lives" )
	PORT_DIPSETTING(    0x00, "Freeze" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) ) // 0x20 -> fe65
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) ) // unused
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) ) /* ? */
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/**************************************************************************/

INPUT_PORTS_START( aso )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_VBLANK )  /* ? */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Allow Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C) )
	PORT_DIPSETTING(    0x28, DEF_STR( 3C_1C) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_1C) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_4C) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0xc0, "50k 100k" )
	PORT_DIPSETTING(    0x80, "60k 120k" )
	PORT_DIPSETTING(    0x40, "100k 200k" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Bonus Occurrence" )
	PORT_DIPSETTING(    0x01, "1st & every 2nd" )
	PORT_DIPSETTING(    0x00, "1st & 2nd only" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x06, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x10,    0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Cheat of some kind", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Start Area" )
	PORT_DIPSETTING(    0xc0, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x00, "4" )
INPUT_PORTS_END


/**************************************************************************/

static struct GfxLayout char256 = {
	8,8,
	0x100,
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	256
};

static struct GfxLayout char1024 = {
	8,8,
	0x400,
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	256
};

static struct GfxLayout sprite1024 = {
	16,16,
	0x400,
	3,
	{ 2*1024*256,1*1024*256,0*1024*256 },
	{
		7,6,5,4,3,2,1,0,
		15,14,13,12,11,10,9,8
	},
	{
		0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16
	},
	256
};

static struct GfxDecodeInfo aso_gfxdecodeinfo[] =
{
	/* colors 512-1023 are currently unused, I think they are a second bank */
	{ REGION_GFX1, 0, &char256,    128*3,  8 },	/* colors 384..511 */
	{ REGION_GFX2, 0, &char1024,   128*1, 16 },	/* colors 128..383 */
	{ REGION_GFX3, 0, &sprite1024, 128*0, 16 },	/* colors   0..127 */
	{ -1 }
};

/**************************************************************************/

#define SNK_NMI_ENABLE	1
#define SNK_NMI_PENDING	2

static int snk_soundcommand = 0;
static unsigned char *shared_ram, *shared_auxram;

static READ_HANDLER( shared_auxram_r ){ return shared_auxram[offset]; }
static WRITE_HANDLER( shared_auxram_w ){ shared_auxram[offset] = data; }

static READ_HANDLER( shared_ram_r ){ return shared_ram[offset]; }
static WRITE_HANDLER( shared_ram_w ){ shared_ram[offset] = data; }

static int CPUA_latch = 0;
static int CPUB_latch = 0;

static WRITE_HANDLER( CPUA_int_enable_w ){
	if( CPUA_latch & SNK_NMI_PENDING ){
		cpu_cause_interrupt( 0, Z80_NMI_INT );
		CPUA_latch = 0;
	}
	else {
		CPUA_latch |= SNK_NMI_ENABLE;
	}
}

static READ_HANDLER( CPUA_int_trigger_r ){
	if( CPUA_latch&SNK_NMI_ENABLE ){
		cpu_cause_interrupt( 0, Z80_NMI_INT );
		CPUA_latch = 0;
	}
	else {
		CPUA_latch |= SNK_NMI_PENDING;
	}
	return 0xff;
}

static WRITE_HANDLER( CPUB_int_enable_w ){
	if( CPUB_latch & SNK_NMI_PENDING ){
		cpu_cause_interrupt( 1, Z80_NMI_INT );
		CPUB_latch = 0;
	}
	else {
		CPUB_latch |= SNK_NMI_ENABLE;
	}
}

static READ_HANDLER( CPUB_int_trigger_r ){
	if( CPUB_latch&SNK_NMI_ENABLE ){
		cpu_cause_interrupt( 1, Z80_NMI_INT );
		CPUB_latch = 0;
	}
	else {
		CPUB_latch |= SNK_NMI_PENDING;
	}
	return 0xff;
}

static WRITE_HANDLER( snk_soundcommand_w ){
	snk_soundcommand = data;
	cpu_cause_interrupt( 2, Z80_IRQ_INT );
//	cpu_cause_interrupt(2, 0xff); old ASO
}

static READ_HANDLER( snk_soundcommand_r )
{
	int val = snk_soundcommand;
	snk_soundcommand = 0;
	return val;
}

/**************************************************************************/

static struct YM3526interface ym3526_interface ={
	1,			/* number of chips */
	4000000,	/* 4 MHz? (hand tuned) */
	{ 50 }		/* (not supported) */
};

static struct MemoryReadAddress aso_readmem_sound[] ={
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd000, snk_soundcommand_r },
	{ 0xf000, 0xf000, YM3526_status_port_0_r },
	{ -1 }
};

static struct MemoryWriteAddress aso_writemem_sound[] ={
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xf000, 0xf000, YM3526_control_port_0_w }, /* YM3526 #1 control port? */
	{ 0xf001, 0xf001, YM3526_write_port_0_w },   /* YM3526 #1 write port?  */
	{ -1 }
};

/**************************************************************************/

static struct AY8910interface ay8910_interface = {
	2, /* number of chips */
	2000000, /* 2 MHz */
	{ 35,35 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MemoryReadAddress hal21_readmem_sound[] = {
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, snk_soundcommand_r },
//	{ 0xc000, 0xc000, ack },
	{ -1 }
};

static struct MemoryWriteAddress hal21_writemem_sound[] = {
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xe000, 0xe000, AY8910_control_port_0_w },
	{ 0xe001, 0xe001, AY8910_write_port_0_w },
	{ 0xe008, 0xe008, AY8910_control_port_1_w },
	{ 0xe009, 0xe009, AY8910_write_port_1_w },
	{ -1 }
};

/**************************** ASO/Alpha Mission *************************/

static struct MemoryReadAddress aso_readmem_cpuA[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc000, input_port_0_r },	/* coin, start */
	{ 0xc100, 0xc100, input_port_1_r },	/* P1 */
	{ 0xc200, 0xc200, input_port_2_r },	/* P2 */
	{ 0xc500, 0xc500, input_port_3_r },	/* DSW1 */
	{ 0xc600, 0xc600, input_port_4_r },	/* DSW2 */
	{ 0xc700, 0xc700, CPUB_int_trigger_r },
	{ 0xd000, 0xffff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress aso_writemem_cpuA[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc400, 0xc400, snk_soundcommand_w },
	{ 0xc700, 0xc700, CPUA_int_enable_w },
	{ 0xc800, 0xc800, hal21_vreg1_w },
	{ 0xc900, 0xc900, hal21_vreg2_w },
	{ 0xca00, 0xca00, hal21_vreg3_w },
	{ 0xcb00, 0xcb00, hal21_vreg4_w },
	{ 0xcc00, 0xcc00, hal21_vreg5_w },
	{ 0xcf00, 0xcf00, hal21_vreg0_w },
	{ 0xd800, 0xdfff, MWA_RAM, &shared_auxram },
	{ 0xe000, 0xe7ff, MWA_RAM, &spriteram },
	{ 0xe800, 0xf7ff, videoram_w, &videoram },
	{ 0xf800, 0xffff, MWA_RAM, &shared_ram },
	{ -1 }
};

static struct MemoryReadAddress aso_readmem_cpuB[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc000, CPUA_int_trigger_r },
	{ 0xc800, 0xe7ff, shared_auxram_r },
	{ 0xe800, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xffff, shared_ram_r },
	{ -1 }
};
static struct MemoryWriteAddress aso_writemem_cpuB[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc000, CPUB_int_enable_w },
	{ 0xc800, 0xd7ff, shared_auxram_w },
	{ 0xd800, 0xe7ff, videoram_w },
	{ 0xe800, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xffff, shared_ram_w },
	{ -1 }
};

/**************************** HAL21 *************************/

static struct MemoryReadAddress hal21_readmem_CPUA[] = {
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc000, input_port_0_r },	/* coin, start */
	{ 0xc100, 0xc100, input_port_1_r },	/* P1 */
	{ 0xc200, 0xc200, input_port_2_r },	/* P2 */
	{ 0xc400, 0xc400, input_port_3_r },	/* DSW1 */
	{ 0xc500, 0xc500, input_port_4_r },	/* DSW2 */
	{ 0xc700, 0xc700, CPUB_int_trigger_r },
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf000, 0xffff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress hal21_writemem_CPUA[] = {
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc300, 0xc300, snk_soundcommand_w },
	{ 0xc600, 0xc600, hal21_vreg0_w },
	{ 0xc700, 0xc700, CPUA_int_enable_w },
	{ 0xd300, 0xd300, hal21_vreg1_w },
	{ 0xd400, 0xd400, hal21_vreg2_w },
	{ 0xd500, 0xd500, hal21_vreg3_w },
	{ 0xd600, 0xd600, hal21_vreg4_w },
	{ 0xd700, 0xd700, hal21_vreg5_w },
	{ 0xe000, 0xefff, MWA_RAM, &spriteram },
	{ 0xf000, 0xffff, MWA_RAM, &shared_ram },
	{ -1 }
};

READ_HANDLER( hal21_spriteram_r ){
	return spriteram[offset];
}
WRITE_HANDLER( hal21_spriteram_w ){
	spriteram[offset] = data;
}

static struct MemoryReadAddress hal21_readmem_CPUB[] = {
	{ 0x0000, 0x9fff, MRA_ROM },
	{ 0xc000, 0xcfff, hal21_spriteram_r },
	{ 0xd000, 0xdfff, MRA_RAM }, /* background */
	{ 0xe000, 0xefff, shared_ram_r },
	{ -1 }
};

static struct MemoryWriteAddress hal21_writemem_CPUB[] = {
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xa000, CPUB_int_enable_w },
	{ 0xc000, 0xcfff, hal21_spriteram_w },
	{ 0xd000, 0xdfff, videoram_w, &videoram },
	{ 0xe000, 0xefff, shared_ram_w },
	{ -1 }
};

/**************************************************************************/

static struct MachineDriver machine_driver_aso =
{
	{
		{
			CPU_Z80,
			4000000, /* ? */
			aso_readmem_cpuA,aso_writemem_cpuA,0,0,
			interrupt,1
		},
		{
			CPU_Z80,
			4000000, /* ? */
			aso_readmem_cpuB,aso_writemem_cpuB,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* 4 Mhz (?) */
			aso_readmem_sound,aso_writemem_sound,0,0,
			interrupt,1
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100,	/* CPU slices per frame */
	0, /* init machine */

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 1*8, 28*8-1 },

	aso_gfxdecodeinfo,
	1024,1024,
	aso_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	aso_vh_start,
	aso_vh_stop,
	aso_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
	    {
	       SOUND_YM3526,
	       &ym3526_interface
	    }
	}
};

static struct MachineDriver machine_driver_hal21 = {
	{
		{
			CPU_Z80,
			3360000,	/* 3.336 Mhz? */
			hal21_readmem_CPUA,hal21_writemem_CPUA,0,0,
			interrupt,1
		},
		{
			CPU_Z80,
			3360000,	/* 3.336 Mhz? */
			hal21_readmem_CPUB,hal21_writemem_CPUB,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* 4 Mhz (?) */
			hal21_readmem_sound,hal21_writemem_sound,0,0,
			interrupt,1
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100,	/* CPU slices per frame */
	0, /* init_machine */

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 1*8, 28*8-1 },
	aso_gfxdecodeinfo,
	1024,1024,
	aso_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	hal21_vh_start,
	aso_vh_stop,
	aso_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
	    {
	       SOUND_AY8910,
	       &ay8910_interface
	    }
	}
};

/**************************************************************************/

ROM_START( hal21 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for CPUA code */
	ROM_LOAD( "hal21p1.bin",    0x0000, 0x2000, 0x9d193830 )
	ROM_LOAD( "hal21p2.bin",    0x2000, 0x2000, 0xc1f00350 )
	ROM_LOAD( "hal21p3.bin",    0x4000, 0x2000, 0x881d22a6 )
	ROM_LOAD( "hal21p4.bin",    0x6000, 0x2000, 0xce692534 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for CPUB code */
	ROM_LOAD( "hal21p5.bin",    0x0000, 0x2000, 0x3ce0684a )
	ROM_LOAD( "hal21p6.bin",    0x2000, 0x2000, 0x878ef798 )
	ROM_LOAD( "hal21p7.bin",    0x4000, 0x2000, 0x72ebbe95 )
	ROM_LOAD( "hal21p8.bin",    0x6000, 0x2000, 0x17e22ad3 )
	ROM_LOAD( "hal21p9.bin",    0x8000, 0x2000, 0xb146f891 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for sound code */
	ROM_LOAD( "hal21p10.bin",   0x0000, 0x4000, 0x916f7ba0 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hal21p12.bin", 0x0000, 0x2000, 0x9839a7cd ) /* char */

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE  ) /* background tiles */
	ROM_LOAD( "hal21p11.bin", 0x0000, 0x4000, 0x24abc57e )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE  ) /* 16x16 sprites */
	ROM_LOAD( "hal21p13.bin", 0x00000, 0x4000, 0x052b4f4f )
	ROM_RELOAD(               0x04000, 0x4000 )
	ROM_LOAD( "hal21p14.bin", 0x08000, 0x4000, 0xda0cb670 )
	ROM_RELOAD(               0x0c000, 0x4000 )
	ROM_LOAD( "hal21p15.bin", 0x10000, 0x4000, 0x5c5ea945 )
	ROM_RELOAD(               0x14000, 0x4000 )

	ROM_REGION( 0x0c00, REGION_PROMS )
	ROM_LOAD( "hal21_1.prm",  0x000, 0x400, 0x195768fc )
	ROM_LOAD( "hal21_2.prm",  0x400, 0x400, 0xc5d84225 )
	ROM_LOAD( "hal21_3.prm",  0x800, 0x400, 0x605afff8 )
ROM_END

ROM_START( hal21j )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for CPUA code */
	ROM_LOAD( "hal21p1.bin",    0x0000, 0x2000, 0x9d193830 )
	ROM_LOAD( "hal21p2.bin",    0x2000, 0x2000, 0xc1f00350 )
	ROM_LOAD( "hal21p3.bin",    0x4000, 0x2000, 0x881d22a6 )
	ROM_LOAD( "hal21p4.bin",    0x6000, 0x2000, 0xce692534 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for CPUB code */
	ROM_LOAD( "hal21p5.bin",    0x0000, 0x2000, 0x3ce0684a )
	ROM_LOAD( "hal21p6.bin",    0x2000, 0x2000, 0x878ef798 )
	ROM_LOAD( "hal21p7.bin",    0x4000, 0x2000, 0x72ebbe95 )
	ROM_LOAD( "hal21p8.bin",    0x6000, 0x2000, 0x17e22ad3 )
	ROM_LOAD( "hal21p9.bin",    0x8000, 0x2000, 0xb146f891 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for sound code */
	ROM_LOAD( "hal21-10.bin",   0x0000, 0x4000, 0xa182b3f0 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hal21p12.bin", 0x0000, 0x2000, 0x9839a7cd ) /* char */

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE  ) /* background tiles */
	ROM_LOAD( "hal21p11.bin", 0x0000, 0x4000, 0x24abc57e )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE  ) /* 16x16 sprites */
	ROM_LOAD( "hal21p13.bin", 0x00000, 0x4000, 0x052b4f4f )
	ROM_RELOAD(               0x04000, 0x4000 )
	ROM_LOAD( "hal21p14.bin", 0x08000, 0x4000, 0xda0cb670 )
	ROM_RELOAD(               0x0c000, 0x4000 )
	ROM_LOAD( "hal21p15.bin", 0x10000, 0x4000, 0x5c5ea945 )
	ROM_RELOAD(               0x14000, 0x4000 )

	ROM_REGION( 0x0c00, REGION_PROMS )
	ROM_LOAD( "hal21_1.prm",  0x000, 0x400, 0x195768fc )
	ROM_LOAD( "hal21_2.prm",  0x400, 0x400, 0xc5d84225 )
	ROM_LOAD( "hal21_3.prm",  0x800, 0x400, 0x605afff8 )
ROM_END

ROM_START( aso )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for cpuA code */
	ROM_LOAD( "aso.1",    0x0000, 0x8000, 0x3fc9d5e4 )
	ROM_LOAD( "aso.3",    0x8000, 0x4000, 0x39a666d2 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for cpuB code */
	ROM_LOAD( "aso.4",    0x0000, 0x8000, 0x2429792b )
	ROM_LOAD( "aso.6",    0x8000, 0x4000, 0xc0bfdf1f )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for sound code */
	ROM_LOAD( "aso.7",    0x0000, 0x8000, 0x49258162 )  /* YM3526 */
	ROM_LOAD( "aso.9",    0x8000, 0x4000, 0xaef5a4f4 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* characters */
	ROM_LOAD( "aso.14",   0x0000, 0x2000, 0x8baa2253 )

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE  ) /* background tiles */
	ROM_LOAD( "aso.10",   0x0000, 0x8000, 0x00dff996 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE  ) /* 16x16 sprites */
	ROM_LOAD( "aso.11",   0x00000, 0x8000, 0x7feac86c )
	ROM_LOAD( "aso.12",   0x08000, 0x8000, 0x6895990b )
	ROM_LOAD( "aso.13",   0x10000, 0x8000, 0x87a81ce1 )

	ROM_REGION( 0x0c00, REGION_PROMS )
	ROM_LOAD( "up02_f12.rom",  0x000, 0x00400, 0x5b0a0059 )
	ROM_LOAD( "up02_f13.rom",  0x400, 0x00400, 0x37e28dd8 )
	ROM_LOAD( "up02_f14.rom",  0x800, 0x00400, 0xc3fd1dd3 )
ROM_END



GAMEX( 1985, aso,    0,     aso,   aso,   0, ROT270_16BIT, "SNK", "ASO - Armored Scrum Object", GAME_IMPERFECT_SOUND )
GAMEX( 1985, hal21,  0,     hal21, hal21, 0, ROT270_16BIT, "SNK", "HAL21", GAME_NO_SOUND | GAME_WRONG_COLORS )
GAMEX( 1985, hal21j, hal21, hal21, hal21, 0, ROT270_16BIT, "SNK", "HAL21 (Japan)", GAME_NO_SOUND | GAME_WRONG_COLORS )
