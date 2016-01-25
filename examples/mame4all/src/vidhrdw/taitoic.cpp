/***************************************************************************

TC0100SCN
---------
Tilemap generator, has three 64x64 tilemaps with 8x8 tiles.
The front one fetches gfx data from RAM, the others use ROMs as usual.

0000-3fff BG0
4000-5fff FG0
6000-6fff gfx data for FG0
7000-7fff unknown/unused?
8000-bfff BG1
c000-c1ff BG0 rowscroll (gunfront)
c200-c3ff unknown/unused? (FG0 rowscroll?)
c400-c5ff BG1 rowscroll (gunfront)
c600-ffff unknown/unused?

control registers:
000-001 BG0 scroll X
002-003 BG1 scroll X
004-005 FG0 scroll X
006-007 BG0 scroll Y
008-009 BG1 scroll Y
00a-00b FG0 scroll Y
00c-00d ---------------x BG0 disable
        --------------x- BG1 disable
        -------------x-- FG0 disable
        ------------x--- change priority order from BG0-BG1-FG0 to BG1-BG0-FG0
        -----------x---- unknown (cameltru, NOT cameltry)
00e-00f ---------------x flip screen
        --x------------- unknown (thunderfox)


TC0280GRD
TC0430GRW
---------
These generate a zooming/rotating tilemap. The TC0280GRD has to be used in
pairs, while the TC0430GRW is a newer, single-chip solution.
Regardless of the hardware differences, the two are functionally identical
except for incxx and incxy, which need to be multiplied by 2 in the TC0280GRD
to bring them to the same scale of the other parameters.

control registers:
000-003 start x
004-005 incxx
006-007 incyx
008-00b start y
00c-00d incxy
00e-00f incyy


TC0480SCP
---------
Tilemap generator, has four zoomable 32x32 tilemaps with 16x16 tiles.
It also has a front 64x64 tilemap with 8x8 tiles which fetches gfx data
from RAM.

0000-0fff BG0
1000-1fff BG1
2000-2fff BG2
3000-3fff BG3
4000-41ff BG0 rowscroll ?
4400-45ff BG1 rowscroll ?
4800-49ff BG2 rowscroll ?
4c00-4dff BG3 rowscroll ?
6000-61ff BG0 colscroll ??
6400-65ff BG1 colscroll ??
6800-69ff BG2 colscroll ??
6c00-6dff BG3 colscroll ??
[gaps above seem to be unused]
7000-bfff unknown/unused?
c000-dfff FG0
e000-ffff gfx data for FG0 (4bpp)

Each tile in bg layers is two data words:

+0x00   %yx..bbbb cccccccc      b=control bits(?) c=colour .=unused(?)
+0x02   tilenum                 (range 0 to $1fff)

[y=yflip x=xflip I think. No idea what 4 lsbs do.]

control registers:
000-001 BG0 x scroll     (layer priority order definable)
002-003 BG1 x scroll
004-005 BG2 x scroll
006-007 BG3 x scroll
008-009 BG0 y scroll
00a-00b BG1 y scroll
00c-00d BG2 y scroll
00e-00f BG3 y scroll
010-011 BG0 zoom  <0x7f = shrunk e.g. 0x1a in Footchmp hiscore; 0x7f = normal;
012-013 BG1 zoom   0xfefe = max(?) zoom e.g. start of game in Metalb
014-015 BG2 zoom
016-017 BG3 zoom
018-019 Text layer x scroll
01a-01b Text layer y scroll
01c-01d Unused (not written)
01e-01f Layer Control register
		x-------	(probably unused)
		-x------	Flip screen
		--x-----	unknown

				Set in Metalb init by whether a byte in prg ROM $7fffe is zero.
				Subsequently Metalb changes it for some screen layer layouts.
				Footchmp clears it, Hthero sets it [then both leave it alone].
				Deadconx code at $10e2 is interesting, with possible values of:
				0x0, 0x20, 0x40, 0x60 poked in (via ram buffer) to control reg,
				dependent on byte in prg ROM $7fffd and whether screen is flipped.

		---xxxxx	BG layer priority order (?)

				In Metalb this takes various values [see Raine lookup table in
				vidhrdw\taito_f2.c]. Other games leave it at zero.

020-021 BG0 dx?	(these four change when x-scrolling layer)
022-023 BG1 dx?
024-025 BG2 dx?
026-027 BG3 dx?
028-029 BG0 dy?	(these four change when y-scrolling layer)
02a-02b BG1 dy?
02c-02d BG2 dy?
02e-02f BG3 dy?


TC0110PCR
---------
Interface to palette RAM, and simple tilemap/sprite priority handler. The
priority order seems to be fixed.
The data bus is 16 bits wide.

000  W selects palette RAM address
002 RW read/write palette RAM
004  W unknown, often written to


TC0220IOC
---------
A simple I/O interface with integrated watchdog.
It has four address inputs, which would suggest 16 bytes of addressing space,
but only the first 8 seem to be used.

000 R  IN00-07 (DSA)
000  W watchdog reset
001 R  IN08-15 (DSB)
002 R  IN16-23 (1P)
002  W unknown. Usually written on startup: initialize?
003 R  IN24-31 (2P)
004 RW coin counters and lockout
005  W unknown
006  W unknown
007 R  INB0-7 (coin)


TC0510NIO
---------
Newer version of the I/O chip

000 R  DSWA
000  W watchdog reset
001 R  DSWB
001  W unknown (ssi)
002 R  1P
003 R  2P
003  W unknown (yuyugogo, qzquest and qzchikyu use it a lot)
004 RW coin counters and lockout
005  W unknown
006  W unknown (koshien and pulirula use it a lot)
007 R  coin

***************************************************************************/

#include "driver.h"
#include "taitoic.h"



static unsigned char taitof2_scrbank;
WRITE_HANDLER( taitof2_scrbank_w )   /* Mjnquest banks its 2 sets of scr tiles */
{
    taitof2_scrbank = (data & 0x1);
}

#define TC0100SCN_RAM_SIZE 0x10000
#define TC0100SCN_TOTAL_CHARS 256
#define TC0100SCN_MAX_CHIPS 2
static int TC0100SCN_chips;
static UINT8 TC0100SCN_ctrl[TC0100SCN_MAX_CHIPS][16];
static UINT8 *TC0100SCN_ram[TC0100SCN_MAX_CHIPS],*TC0100SCN_bg_ram[TC0100SCN_MAX_CHIPS],
		*TC0100SCN_fg_ram[TC0100SCN_MAX_CHIPS],*TC0100SCN_tx_ram[TC0100SCN_MAX_CHIPS],
		*TC0100SCN_char_ram[TC0100SCN_MAX_CHIPS],
		*TC0100SCN_bgscroll_ram[TC0100SCN_MAX_CHIPS],*TC0100SCN_fgscroll_ram[TC0100SCN_MAX_CHIPS];
static int TC0100SCN_bgscrollx[TC0100SCN_MAX_CHIPS],TC0100SCN_bgscrolly[TC0100SCN_MAX_CHIPS],
		TC0100SCN_fgscrollx[TC0100SCN_MAX_CHIPS],TC0100SCN_fgscrolly[TC0100SCN_MAX_CHIPS];
static struct tilemap *TC0100SCN_tilemap[TC0100SCN_MAX_CHIPS][3];
static char *TC0100SCN_char_dirty[TC0100SCN_MAX_CHIPS];
static int TC0100SCN_chars_dirty[TC0100SCN_MAX_CHIPS];
static int TC0100SCN_bg_gfx[TC0100SCN_MAX_CHIPS],TC0100SCN_tx_gfx[TC0100SCN_MAX_CHIPS];




INLINE void common_get_bg_tile_info(UINT8 *ram,int gfxnum,int tile_index)
{
	int code = (READ_WORD(&ram[4*tile_index + 2]) & 0x7fff) + (taitof2_scrbank << 15);
	int attr = READ_WORD(&ram[4*tile_index]);
	SET_TILE_INFO(gfxnum,code,attr & 0xff);
	tile_info.flags = TILE_FLIPYX((attr & 0xc000) >> 14);
}

INLINE void common_get_tx_tile_info(UINT8 *ram,int gfxnum,int tile_index)
{
	int attr = READ_WORD(&ram[2*tile_index]);
	SET_TILE_INFO(gfxnum,attr & 0xff,(attr & 0x3f00) >> 6);
	tile_info.flags = TILE_FLIPYX((attr & 0xc000) >> 14);
}

static void TC0100SCN_get_bg_tile_info_0(int tile_index)
{
	common_get_bg_tile_info(TC0100SCN_bg_ram[0],TC0100SCN_bg_gfx[0],tile_index);
}

static void TC0100SCN_get_fg_tile_info_0(int tile_index)
{
	common_get_bg_tile_info(TC0100SCN_fg_ram[0],TC0100SCN_bg_gfx[0],tile_index);
}

static void TC0100SCN_get_tx_tile_info_0(int tile_index)
{
	common_get_tx_tile_info(TC0100SCN_tx_ram[0],TC0100SCN_tx_gfx[0],tile_index);
}

static void TC0100SCN_get_bg_tile_info_1(int tile_index)
{
	common_get_bg_tile_info(TC0100SCN_bg_ram[1],TC0100SCN_bg_gfx[1],tile_index);
}

static void TC0100SCN_get_fg_tile_info_1(int tile_index)
{
	common_get_bg_tile_info(TC0100SCN_fg_ram[1],TC0100SCN_bg_gfx[1],tile_index);
}

static void TC0100SCN_get_tx_tile_info_1(int tile_index)
{
	common_get_tx_tile_info(TC0100SCN_tx_ram[1],TC0100SCN_tx_gfx[1],tile_index);
}


void (*get_tile_info[TC0100SCN_MAX_CHIPS][3])(int tile_index) =
{
	{ TC0100SCN_get_bg_tile_info_0, TC0100SCN_get_fg_tile_info_0 ,TC0100SCN_get_tx_tile_info_0 },
	{ TC0100SCN_get_bg_tile_info_1, TC0100SCN_get_fg_tile_info_1 ,TC0100SCN_get_tx_tile_info_1 }
};


static struct GfxLayout TC0100SCN_charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


int TC0100SCN_vh_start(int chips,int gfxnum,int x_offset)
{
	int gfx_index,i;


	if (chips > TC0100SCN_MAX_CHIPS) return 1;

	TC0100SCN_chips = chips;

	for (i = 0;i < chips;i++)
	{
		int xd,yd;

		TC0100SCN_tilemap[i][0] = tilemap_create(get_tile_info[i][0],tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
		TC0100SCN_tilemap[i][1] = tilemap_create(get_tile_info[i][1],tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
		TC0100SCN_tilemap[i][2] = tilemap_create(get_tile_info[i][2],tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

		TC0100SCN_ram[i] = (UINT8*)malloc(TC0100SCN_RAM_SIZE);
		TC0100SCN_char_dirty[i] = (char*)malloc(TC0100SCN_TOTAL_CHARS);

		if (!TC0100SCN_ram[i] || !TC0100SCN_tilemap[i][0] ||
				!TC0100SCN_tilemap[i][1] || !TC0100SCN_tilemap[i][2])
		{
			TC0100SCN_vh_stop();
			return 1;
		}

		TC0100SCN_bg_ram[i]       = TC0100SCN_ram[i] + 0x0000;
		TC0100SCN_tx_ram[i]       = TC0100SCN_ram[i] + 0x4000;
		TC0100SCN_char_ram[i]     = TC0100SCN_ram[i] + 0x6000;
		TC0100SCN_fg_ram[i]       = TC0100SCN_ram[i] + 0x8000;
		TC0100SCN_bgscroll_ram[i] = TC0100SCN_ram[i] + 0xc000;
		TC0100SCN_fgscroll_ram[i] = TC0100SCN_ram[i] + 0xc400;
		memset(TC0100SCN_ram[i],0,TC0100SCN_RAM_SIZE);
		memset(TC0100SCN_char_dirty[i],1,TC0100SCN_TOTAL_CHARS);
		TC0100SCN_chars_dirty[i] = 1;

		/* find first empty slot to decode gfx */
		for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
			if (Machine->gfx[gfx_index] == 0)
				break;
		if (gfx_index == MAX_GFX_ELEMENTS)
		{
			TC0100SCN_vh_stop();
			return 1;
		}

		/* create the char set (gfx will then be updated dynamically from RAM) */
		Machine->gfx[gfx_index] = decodegfx(TC0100SCN_char_ram[i],&TC0100SCN_charlayout);
		if (!Machine->gfx[gfx_index])
			return 1;

		/* set the color information */
		Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
		Machine->gfx[gfx_index]->total_colors = 64;

		TC0100SCN_tx_gfx[i] = gfx_index;

		/* use the given gfx set for bg tiles */
		TC0100SCN_bg_gfx[i] = gfxnum + i;

		TC0100SCN_tilemap[i][0]->transparent_pen = 0;
		TC0100SCN_tilemap[i][1]->transparent_pen = 0;
		TC0100SCN_tilemap[i][2]->transparent_pen = 0;

		/* I'm setting the optional chip #2 7 bits higher and 2 pixels to the left
		   than chip #1 because that's how thundfox wants it. */
		xd = (i == 0) ? -x_offset : (-x_offset-2);
		yd = (i == 0) ? 8 : 1;
		tilemap_set_scrolldx(TC0100SCN_tilemap[i][0],-16 + xd,-16 - xd);
		tilemap_set_scrolldy(TC0100SCN_tilemap[i][0],yd,-yd);
		tilemap_set_scrolldx(TC0100SCN_tilemap[i][1],-16 + xd,-16 - xd);
		tilemap_set_scrolldy(TC0100SCN_tilemap[i][1],yd,-yd);
		tilemap_set_scrolldx(TC0100SCN_tilemap[i][2],-16 + xd,-16 - xd - 7);
		tilemap_set_scrolldy(TC0100SCN_tilemap[i][2],yd,-yd);

		tilemap_set_scroll_rows(TC0100SCN_tilemap[i][0],512);
		tilemap_set_scroll_rows(TC0100SCN_tilemap[i][1],512);
	}

	taitof2_scrbank = 0;

	return 0;
}

void TC0100SCN_vh_stop(void)
{
	int i;

	for (i = 0;i < TC0100SCN_chips;i++)
	{
		free(TC0100SCN_ram[i]);
		TC0100SCN_ram[i] = 0;
		free(TC0100SCN_char_dirty[i]);
		TC0100SCN_char_dirty[i] = 0;
	}
}


static int TC0100SCN_word_r(int chip,offs_t offset)
{
	int res;

	res = READ_WORD(&TC0100SCN_ram[chip][offset]);

	/* for char gfx data, we have to straighten out the 16-bit word into
	   bytes for gfxdecode() to work */
	#ifdef LSB_FIRST
	if (offset >= 0x6000 && offset < 0x7000)
		res = ((res & 0x00ff) << 8) | ((res & 0xff00) >> 8);
	#endif

	return res;
}

READ_HANDLER( TC0100SCN_word_0_r )
{
	return TC0100SCN_word_r(0,offset);
}

READ_HANDLER( TC0100SCN_word_1_r )
{
	return TC0100SCN_word_r(1,offset);
}


static void TC0100SCN_word_w(int chip,offs_t offset,data_t data)
{
	int oldword = READ_WORD(&TC0100SCN_ram[chip][offset]);
	int newword;

	/* for char gfx data, we have to straighten out the 16-bit word into
	   bytes for gfxdecode() to work */
	#ifdef LSB_FIRST
	if (offset >= 0x6000 && offset < 0x7000)
		data = ((data & 0x00ff00ff) << 8) | ((data & 0xff00ff00) >> 8);
	#endif

	newword = COMBINE_WORD(oldword,data);
	if (oldword != newword)
	{
		WRITE_WORD(&TC0100SCN_ram[chip][offset],newword);

		if (offset < 0x4000)
			tilemap_mark_tile_dirty(TC0100SCN_tilemap[chip][0],offset / 4);
		else if (offset < 0x6000)
			tilemap_mark_tile_dirty(TC0100SCN_tilemap[chip][2],(offset & 0x1fff) / 2);
		else if (offset < 0x7000)
		{
			TC0100SCN_char_dirty[chip][(offset - 0x6000) / 16] = 1;
			TC0100SCN_chars_dirty[chip] = 1;
		}
		else if (offset >= 0x8000 && offset < 0xc000)
			tilemap_mark_tile_dirty(TC0100SCN_tilemap[chip][1],(offset & 0x3fff) / 4);
	}
}

WRITE_HANDLER( TC0100SCN_word_0_w )
{
	TC0100SCN_word_w(0,offset,data);
}

WRITE_HANDLER( TC0100SCN_word_1_w )
{
	TC0100SCN_word_w(1,offset,data);
}


READ_HANDLER( TC0100SCN_ctrl_word_0_r )
{
	return READ_WORD(&TC0100SCN_ctrl[0][offset]);
}

READ_HANDLER( TC0100SCN_ctrl_word_1_r )
{
	return READ_WORD(&TC0100SCN_ctrl[1][offset]);
}


static void TC0100SCN_ctrl_word_w(int chip,offs_t offset,data_t data)
{
	COMBINE_WORD_MEM(&TC0100SCN_ctrl[chip][offset],data);

	data = READ_WORD(&TC0100SCN_ctrl[chip][offset]);

	switch (offset)
	{
		case 0x00:
			TC0100SCN_bgscrollx[chip] = -data;
			break;

		case 0x02:
			TC0100SCN_fgscrollx[chip] = -data;
			break;

		case 0x04:
			tilemap_set_scrollx(TC0100SCN_tilemap[chip][2],0,-data);
			break;

		case 0x06:
			TC0100SCN_bgscrolly[chip] = -data;
			break;

		case 0x08:
			TC0100SCN_fgscrolly[chip] = -data;
			break;

		case 0x0a:
			tilemap_set_scrolly(TC0100SCN_tilemap[chip][2],0,-data);
			break;

		case 0x0c:
			break;

		case 0x0e:
		{
			int flip = (data & 0x01) ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0;

			tilemap_set_flip(TC0100SCN_tilemap[chip][0],flip);
			tilemap_set_flip(TC0100SCN_tilemap[chip][1],flip);
			tilemap_set_flip(TC0100SCN_tilemap[chip][2],flip);
			break;
		}
	}
}

WRITE_HANDLER( TC0100SCN_ctrl_word_0_w )
{
	TC0100SCN_ctrl_word_w(0,offset,data);
}

WRITE_HANDLER( TC0100SCN_ctrl_word_1_w )
{
	TC0100SCN_ctrl_word_w(1,offset,data);
}


void TC0100SCN_tilemap_update(void)
{
	int chip,j;

	for (chip = 0;chip < TC0100SCN_chips;chip++)
	{
		tilemap_set_scrolly(TC0100SCN_tilemap[chip][0],0,TC0100SCN_bgscrolly[chip]);
		tilemap_set_scrolly(TC0100SCN_tilemap[chip][1],0,TC0100SCN_fgscrolly[chip]);

		for (j = 0;j < 256;j++)
			tilemap_set_scrollx(TC0100SCN_tilemap[chip][0],
					(j + TC0100SCN_bgscrolly[chip]) & 0x1ff,
					TC0100SCN_bgscrollx[chip] - READ_WORD(&TC0100SCN_bgscroll_ram[chip][2*j]));
		for (j = 0;j < 256;j++)
			tilemap_set_scrollx(TC0100SCN_tilemap[chip][1],
					(j + TC0100SCN_fgscrolly[chip]) & 0x1ff,
					TC0100SCN_fgscrollx[chip] - READ_WORD(&TC0100SCN_fgscroll_ram[chip][2*j]));

		/* Decode any characters that have changed */
		if (TC0100SCN_chars_dirty[chip])
		{
			int tile_index;


			for (tile_index = 0;tile_index < 64*64;tile_index++)
			{
				int attr = READ_WORD(&TC0100SCN_tx_ram[chip][2*tile_index]);
				if (TC0100SCN_char_dirty[chip][attr & 0xff])
					tilemap_mark_tile_dirty(TC0100SCN_tilemap[chip][2],tile_index);
			}

			for (j = 0;j < TC0100SCN_TOTAL_CHARS;j++)
			{
				if (TC0100SCN_char_dirty[chip][j])
					decodechar(Machine->gfx[TC0100SCN_tx_gfx[chip]],j,TC0100SCN_char_ram[chip],&TC0100SCN_charlayout);
				TC0100SCN_char_dirty[chip][j] = 0;
			}
			TC0100SCN_chars_dirty[chip] = 0;
		}

		tilemap_update(TC0100SCN_tilemap[chip][0]);
		tilemap_update(TC0100SCN_tilemap[chip][1]);
		tilemap_update(TC0100SCN_tilemap[chip][2]);
	}
}

void TC0100SCN_tilemap_draw(struct osd_bitmap *bitmap,int chip,int layer,int flags)
{
	int disable = READ_WORD(&TC0100SCN_ctrl[chip][12]) & 0xf7;

	switch (layer)
	{
		case 0:
			if (disable & 0x01) return;
			tilemap_draw(bitmap,TC0100SCN_tilemap[chip][0],flags);
			break;
		case 1:
			if (disable & 0x02) return;
			tilemap_draw(bitmap,TC0100SCN_tilemap[chip][1],flags);
			break;
		case 2:
			if (disable & 0x04) return;
			if (disable & 0x10) return;	/* cameltru */
			tilemap_draw(bitmap,TC0100SCN_tilemap[chip][2],flags);
			break;
	}
}

int TC0100SCN_bottomlayer(int chip)
{
	return (READ_WORD(&TC0100SCN_ctrl[chip][12]) & 0x8) >> 3;
}


/***************************************************************************/

#define TC0280GRD_RAM_SIZE 0x2000
static UINT8 TC0280GRD_ctrl[16];
static UINT8 *TC0280GRD_ram;
static struct tilemap *TC0280GRD_tilemap;
static int TC0280GRD_gfxnum,TC0280GRD_base_color;


static void TC0280GRD_get_tile_info(int tile_index)
{
	int attr = READ_WORD(&TC0280GRD_ram[2*tile_index]);
	SET_TILE_INFO(TC0280GRD_gfxnum,attr & 0x3fff,((attr & 0xc000) >> 14) + TC0280GRD_base_color);
}


int TC0280GRD_vh_start(int gfxnum)
{
	TC0280GRD_ram = (UINT8*)malloc(TC0280GRD_RAM_SIZE);
	TC0280GRD_tilemap = tilemap_create(TC0280GRD_get_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,64,64);

	if (!TC0280GRD_ram || !TC0280GRD_tilemap)
	{
		TC0280GRD_vh_stop();
		return 1;
	}

	tilemap_set_clip(TC0280GRD_tilemap,0);

	TC0280GRD_gfxnum = gfxnum;

	return 0;
}

int TC0430GRW_vh_start(int gfxnum)
{
	return TC0280GRD_vh_start(gfxnum);
}

void TC0280GRD_vh_stop(void)
{
	free(TC0280GRD_ram);
	TC0280GRD_ram = 0;
}

void TC0430GRW_vh_stop(void)
{
	TC0280GRD_vh_stop();
}

READ_HANDLER( TC0280GRD_word_r )
{
	return READ_WORD(&TC0280GRD_ram[offset]);
}

READ_HANDLER( TC0430GRW_word_r )
{
	return TC0280GRD_word_r(offset);
}

WRITE_HANDLER( TC0280GRD_word_w )
{
	int oldword = READ_WORD(&TC0280GRD_ram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD (&TC0280GRD_ram[offset],newword);
		tilemap_mark_tile_dirty(TC0280GRD_tilemap,offset / 2);
	}
}

WRITE_HANDLER( TC0430GRW_word_w )
{
	TC0280GRD_word_w(offset,data);
}

WRITE_HANDLER( TC0280GRD_ctrl_word_w )
{
	COMBINE_WORD_MEM(&TC0280GRD_ctrl[offset],data);
}

WRITE_HANDLER( TC0430GRW_ctrl_word_w )
{
	TC0280GRD_ctrl_word_w(offset,data);
}

void TC0280GRD_tilemap_update(int base_color)
{
	if (TC0280GRD_base_color != base_color)
	{
		TC0280GRD_base_color = base_color;
		tilemap_mark_all_tiles_dirty(TC0280GRD_tilemap);
	}

	tilemap_update(TC0280GRD_tilemap);
}

void TC0430GRW_tilemap_update(int base_color)
{
	TC0280GRD_tilemap_update(base_color);
}

static void zoom_draw(struct osd_bitmap *bitmap,int xoffset,int yoffset,UINT32 priority,int xmultiply)
{
	UINT32 startx,starty;
	int incxx,incxy,incyx,incyy;
	struct osd_bitmap *srcbitmap = TC0280GRD_tilemap->pixmap;

	/* 24-bit signed */
	startx = ((READ_WORD(&TC0280GRD_ctrl[0]) & 0xff) << 16) + READ_WORD(&TC0280GRD_ctrl[2]);
	if (startx & 0x800000) startx -= 0x1000000;
	incxx = (INT16)READ_WORD(&TC0280GRD_ctrl[4]);
	incxx *= xmultiply;
	incyx = (INT16)READ_WORD(&TC0280GRD_ctrl[6]);
	/* 24-bit signed */
	starty = ((READ_WORD(&TC0280GRD_ctrl[8]) & 0xff) << 16) + READ_WORD(&TC0280GRD_ctrl[10]);
	if (starty & 0x800000) starty -= 0x1000000;
	incxy = (INT16)READ_WORD(&TC0280GRD_ctrl[12]);
	incxy *= xmultiply;
	incyy = (INT16)READ_WORD(&TC0280GRD_ctrl[14]);

	startx -= xoffset * incxx + yoffset * incyx;
	starty -= xoffset * incxy + yoffset * incyy;

	copyrozbitmap(bitmap,srcbitmap,startx << 4,starty << 4,
			incxx << 4,incxy << 4,incyx << 4,incyy << 4,
			1,	/* copy with wraparound */
			&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen,priority);
}

void TC0280GRD_zoom_draw(struct osd_bitmap *bitmap,int xoffset,int yoffset,UINT32 priority)
{
	zoom_draw(bitmap,xoffset,yoffset,priority,2);
}

void TC0430GRW_zoom_draw(struct osd_bitmap *bitmap,int xoffset,int yoffset,UINT32 priority)
{
	zoom_draw(bitmap,xoffset,yoffset,priority,1);
}


/***************************************************************************/


#define TC0480SCP_RAM_SIZE 0x10000
#define TC0480SCP_TOTAL_CHARS 256
static UINT8 TC0480SCP_ctrl[48];
static UINT8 *TC0480SCP_ram,
		*TC0480SCP_bg_ram[4],
		*TC0480SCP_tx_ram,
		*TC0480SCP_char_ram,
		*TC0480SCP_bgscroll_ram[4];
static int TC0480SCP_bgscrollx[4];
static int TC0480SCP_bgscrolly[4];
static struct tilemap *TC0480SCP_tilemap[5];
static char *TC0480SCP_char_dirty;
static int TC0480SCP_chars_dirty;
static int TC0480SCP_bg_gfx,TC0480SCP_tx_gfx;
static int TC0480SCP_tile_colbase;
static int TC0480SCP_x_offs;
static int TC0480SCP_y_offs;

int TC0480SCP_pri_reg;   // read externally in vidhrdw\taito_f2.c


INLINE void common_get_tc0480bg_tile_info(UINT8 *ram,int gfxnum,int tile_index)
{
	int code = (READ_WORD(&ram[4*tile_index + 2]) & 0x7fff);
	int attr = READ_WORD(&ram[4*tile_index]);
	SET_TILE_INFO(gfxnum,code,(attr & 0xff) + TC0480SCP_tile_colbase);
	tile_info.flags = TILE_FLIPYX((attr & 0xc000) >> 14);
}

INLINE void common_get_tc0480tx_tile_info(UINT8 *ram,int gfxnum,int tile_index)
{
	int attr = READ_WORD(&ram[2*tile_index]);
	SET_TILE_INFO(gfxnum,attr & 0xff,((attr & 0x3f00) >> 8) + TC0480SCP_tile_colbase);   // >> 8 not 6 as 4bpp
	tile_info.flags = TILE_FLIPYX((attr & 0xc000) >> 14);
}

static void TC0480SCP_get_bg0_tile_info(int tile_index)
{
	common_get_tc0480bg_tile_info(TC0480SCP_bg_ram[0],TC0480SCP_bg_gfx,tile_index);
}

static void TC0480SCP_get_bg1_tile_info(int tile_index)
{
	common_get_tc0480bg_tile_info(TC0480SCP_bg_ram[1],TC0480SCP_bg_gfx,tile_index);
}

static void TC0480SCP_get_bg2_tile_info(int tile_index)
{
	common_get_tc0480bg_tile_info(TC0480SCP_bg_ram[2],TC0480SCP_bg_gfx,tile_index);
}

static void TC0480SCP_get_bg3_tile_info(int tile_index)
{
	common_get_tc0480bg_tile_info(TC0480SCP_bg_ram[3],TC0480SCP_bg_gfx,tile_index);
}

static void TC0480SCP_get_tx_tile_info(int tile_index)
{
	common_get_tc0480tx_tile_info(TC0480SCP_tx_ram,TC0480SCP_tx_gfx,tile_index);
}

void (*tc480_get_tile_info[5])(int tile_index) =
{
	TC0480SCP_get_bg0_tile_info, TC0480SCP_get_bg1_tile_info, TC0480SCP_get_bg2_tile_info, TC0480SCP_get_bg3_tile_info, TC0480SCP_get_tx_tile_info
};


static struct GfxLayout TC0480SCP_charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 3*4, 2*4, 1*4, 0*4, 7*4, 6*4, 5*4, 4*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every sprite takes 32 consecutive bytes */
};


int TC0480SCP_vh_start(int gfxnum,int pixels,int x_offset,int y_offset,int col_base)
{
	int gfx_index;

		int xd,yd;
		TC0480SCP_tile_colbase = col_base;

		TC0480SCP_tilemap[0] = tilemap_create(tc480_get_tile_info[0],tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
		TC0480SCP_tilemap[1] = tilemap_create(tc480_get_tile_info[1],tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
		TC0480SCP_tilemap[2] = tilemap_create(tc480_get_tile_info[2],tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
		TC0480SCP_tilemap[3] = tilemap_create(tc480_get_tile_info[3],tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
		TC0480SCP_tilemap[4] = tilemap_create(tc480_get_tile_info[4],tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

		TC0480SCP_ram = (UINT8*)malloc(TC0480SCP_RAM_SIZE);
		TC0480SCP_char_dirty = (char*)malloc(TC0480SCP_TOTAL_CHARS);

		if (!TC0480SCP_ram || !TC0480SCP_tilemap[0] || !TC0480SCP_tilemap[1] ||
				!TC0480SCP_tilemap[2] || !TC0480SCP_tilemap[3] || !TC0480SCP_tilemap[4])
		{
			TC0480SCP_vh_stop();
			return 1;
		}

		TC0480SCP_bg_ram[0]	  = TC0480SCP_ram + 0x0000;
		TC0480SCP_bg_ram[1]	  = TC0480SCP_ram + 0x1000;
		TC0480SCP_bg_ram[2]	  = TC0480SCP_ram + 0x2000;
		TC0480SCP_bg_ram[3]	  = TC0480SCP_ram + 0x3000;
		TC0480SCP_bgscroll_ram[0] = TC0480SCP_ram + 0x4000;
		TC0480SCP_bgscroll_ram[1] = TC0480SCP_ram + 0x4400;
		TC0480SCP_bgscroll_ram[2] = TC0480SCP_ram + 0x4800;
		TC0480SCP_bgscroll_ram[3] = TC0480SCP_ram + 0x4c00;
		TC0480SCP_tx_ram		  = TC0480SCP_ram + 0xc000;
		TC0480SCP_char_ram	  = TC0480SCP_ram + 0xe000;

		memset(TC0480SCP_ram,0,TC0480SCP_RAM_SIZE);
		memset(TC0480SCP_char_dirty,1,TC0480SCP_TOTAL_CHARS);
		TC0480SCP_chars_dirty = 1;

		/* find first empty slot to decode gfx */
		for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
			if (Machine->gfx[gfx_index] == 0)
				break;
		if (gfx_index == MAX_GFX_ELEMENTS)
		{
			TC0480SCP_vh_stop();
			return 1;
		}

		/* create the char set (gfx will then be updated dynamically from RAM) */
		Machine->gfx[gfx_index] = decodegfx(TC0480SCP_char_ram,&TC0480SCP_charlayout);
		if (!Machine->gfx[gfx_index])
			return 1;

		/* set the color information */
		Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
		Machine->gfx[gfx_index]->total_colors = 64;

		TC0480SCP_tx_gfx = gfx_index;

		/* use the given gfx set for bg tiles */
		TC0480SCP_bg_gfx = gfxnum;

		TC0480SCP_tilemap[0]->transparent_pen = 0;
		TC0480SCP_tilemap[1]->transparent_pen = 0;
		TC0480SCP_tilemap[2]->transparent_pen = 0;
		TC0480SCP_tilemap[3]->transparent_pen = 0;
		TC0480SCP_tilemap[4]->transparent_pen = 0;

		TC0480SCP_x_offs = x_offset + pixels;
		TC0480SCP_y_offs = y_offset;

		xd = -TC0480SCP_x_offs;
		yd =  TC0480SCP_y_offs;

		tilemap_set_scrolldx(TC0480SCP_tilemap[0], xd,   319-xd);   // 40*8 = 320 (screen width)
		tilemap_set_scrolldy(TC0480SCP_tilemap[0], yd,   256-yd);   // 28*8 = 224 (screen height)
		tilemap_set_scrolldx(TC0480SCP_tilemap[1], xd,   319-xd);
		tilemap_set_scrolldy(TC0480SCP_tilemap[1], yd,   256-yd);
		tilemap_set_scrolldx(TC0480SCP_tilemap[2], xd,   319-xd);
		tilemap_set_scrolldy(TC0480SCP_tilemap[2], yd,   256-yd);
		tilemap_set_scrolldx(TC0480SCP_tilemap[3], xd,   319-xd);
		tilemap_set_scrolldy(TC0480SCP_tilemap[3], yd,   256-yd);
		tilemap_set_scrolldx(TC0480SCP_tilemap[4], xd-2, 315-xd);   /* text layer */
		tilemap_set_scrolldy(TC0480SCP_tilemap[4], yd,   256-yd);   /* text layer */

		/* Make bg tilemaps scrollable per pixel row */
		tilemap_set_scroll_rows(TC0480SCP_tilemap[0],512);
		tilemap_set_scroll_rows(TC0480SCP_tilemap[1],512);
		tilemap_set_scroll_rows(TC0480SCP_tilemap[2],512);
		tilemap_set_scroll_rows(TC0480SCP_tilemap[3],512);

	return 0;
}

void TC0480SCP_vh_stop(void)
{
		free(TC0480SCP_ram);
		TC0480SCP_ram = 0;
		free(TC0480SCP_char_dirty);
		TC0480SCP_char_dirty = 0;
}


static int TC0480SCP_word_read(offs_t offset)
{
	int res;

	res = READ_WORD(&TC0480SCP_ram[offset]);

	/* for char gfx data, we have to straighten out the 16-bit word into
	   bytes for gfxdecode() to work */
	#ifdef LSB_FIRST
	if (offset >= 0xe000 && offset <= 0xffff)
		res = ((res & 0x00ff) << 8) | ((res & 0xff00) >> 8);
	#endif

	return res;
}

READ_HANDLER( TC0480SCP_word_r )
{
	return TC0480SCP_word_read(offset);
}

static void TC0480SCP_word_write(offs_t offset,data_t data)
{
	int oldword = READ_WORD(&TC0480SCP_ram[offset]);
	int newword;

	/* for char gfx data, we have to straighten out the 16-bit word into
	   bytes for gfxdecode() to work */
	#ifdef LSB_FIRST
	if (offset >= 0xe000 && offset <= 0xffff)
		data = ((data & 0x00ff00ff) << 8) | ((data & 0xff00ff00) >> 8);
	#endif

	newword = COMBINE_WORD(oldword,data);
	if (oldword != newword)
	{
		WRITE_WORD(&TC0480SCP_ram[offset],newword);

		if (offset < 0x4000)
		{
			tilemap_mark_tile_dirty(TC0480SCP_tilemap[(offset / 0x1000)],((offset % 0x1000) / 4));
		}
		else if (offset < 0xc000)
		{   // do nothing
		}
		else if (offset < 0xe000)
		{
			tilemap_mark_tile_dirty(TC0480SCP_tilemap[4],(offset - 0xc000) / 2);
		}
		else if (offset <= 0xffff)
		{
			TC0480SCP_char_dirty[(offset - 0xe000) / 32] = 1;
			TC0480SCP_chars_dirty = 1;
		}
	}
}

WRITE_HANDLER( TC0480SCP_word_w )
{
	TC0480SCP_word_write(offset,data);
}

READ_HANDLER( TC0480SCP_ctrl_word_r )
{
	return READ_WORD(&TC0480SCP_ctrl[offset]);
}

static void TC0480SCP_ctrl_word_write(offs_t offset,data_t data)
{
	int flip = TC0480SCP_pri_reg & 0x40;

	COMBINE_WORD_MEM(&TC0480SCP_ctrl[offset],data);
	data = READ_WORD(&TC0480SCP_ctrl[offset]);

	switch( offset )
	{
		/* The x offsets of the four bg layers are staggered by intervals of 4 pixels */
/* NOTE:
Metalb does not always stick with this e.g. it wants bg0&1 further right
(see stick man on stairs and blue planet in attract).
There was a bg3 alignment problem with sprites in the round 4 long
spaceship boss: solved by pushing bg3 two pixels right [there may be a
single pixel left because of sprites being a frame "early"].
New offsets make "film" on skyscraper in round 1 slightly wrong.
*/
		case 0x00:   /* bg0 x */
			if (TC0480SCP_tile_colbase) data += 2;   // improves Metalb attract

			if (!flip)  data = -data;
			TC0480SCP_bgscrollx[0] = data;
			break;

		case 0x02:   /* bg1 x */
			if (TC0480SCP_tile_colbase) data += 2;   // improves Metalb attract

			data += 4;
			if (!flip)  data = -data;
			TC0480SCP_bgscrollx[1] = data;
			break;

		case 0x04:   /* bg2 x */
			if (TC0480SCP_tile_colbase) data += 2;   // same as other layers

			data += 8;
			if (!flip)  data = -data;
			TC0480SCP_bgscrollx[2] = data;
			break;

		case 0x06:   /* bg3 x */
			if (TC0480SCP_tile_colbase) data += 2;   // improves Metalb round 4 boss

			data += 12;
			if (!flip)  data = -data;
			TC0480SCP_bgscrollx[3] = data;
			break;

		case 0x08:   /* bg0 y */
			if (flip)  data = -data;
			TC0480SCP_bgscrolly[0] = data;
			break;

		case 0x0a:   /* bg1 y */
			if (flip)  data = -data;
			TC0480SCP_bgscrolly[1] = data;
			break;

		case 0x0c:   /* bg2 y */
			if (flip)  data = -data;
			TC0480SCP_bgscrolly[2] = data;
			break;

		case 0x0e:   /* bg3 y */
			if (flip)  data = -data;
			TC0480SCP_bgscrolly[3] = data;
			break;

		case 0x10:   /* bg0 zoom */
		case 0x12:   /* bg1 zoom */
		case 0x14:   /* bg2 zoom */
		case 0x16:   /* bg3 zoom */
			break;

		case 0x18:   /* fg (text) x */
			tilemap_set_scrollx(TC0480SCP_tilemap[4], 0, -data);
			break;

		case 0x1a:   /* fg (text) y */
			tilemap_set_scrolly(TC0480SCP_tilemap[4], 0, -data);
			break;

		// offset 0x1c is not used

		case 0x1e:   /* control register */
		{
			flip = (data & 0x40) ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0;
			TC0480SCP_pri_reg = data;

			tilemap_set_flip(TC0480SCP_tilemap[0],flip);
			tilemap_set_flip(TC0480SCP_tilemap[1],flip);
			tilemap_set_flip(TC0480SCP_tilemap[2],flip);
			tilemap_set_flip(TC0480SCP_tilemap[3],flip);
			tilemap_set_flip(TC0480SCP_tilemap[4],flip);

			break;
		}

		// Rest are (I think) layer specific delta x and y, used while scrolling that layer
	}
}

WRITE_HANDLER( TC0480SCP_ctrl_word_w )
{
	TC0480SCP_ctrl_word_write(offset,data);
}

void TC0480SCP_tilemap_update(void)
{
	int layer, zoom, i, j;
	int flip = TC0480SCP_pri_reg & 0x40;

	for (layer = 0; layer < 4; layer++)
	{
		tilemap_set_scrolly(TC0480SCP_tilemap[layer],0,TC0480SCP_bgscrolly[layer]);
		zoom = 0x10000 + 0x7f - READ_WORD(&TC0480SCP_ctrl[0x10 + 2*layer]);

		if (zoom != 0x10000)	/* currently we can't use scroll rows when zooming */
		{
			tilemap_set_scrollx(TC0480SCP_tilemap[layer],
					0, TC0480SCP_bgscrollx[layer]);
		}
		else
		{
			for (j = 0;j < 256;j++)
			{
				i = READ_WORD(&TC0480SCP_bgscroll_ram[layer][(2*j)]);

// DG: possible issues: check yellow bg layer when you kill metalb round 1 boss.
// Top part doesn't behave like rest. But these are right for Footchmp+clones.

				if (!flip)
				tilemap_set_scrollx(TC0480SCP_tilemap[layer],
						(j + TC0480SCP_bgscrolly[layer] + (16 - TC0480SCP_y_offs)) & 0x1ff,
						TC0480SCP_bgscrollx[layer] - i);
				if (flip)
				tilemap_set_scrollx(TC0480SCP_tilemap[layer],
						(j + TC0480SCP_bgscrolly[layer] + 0x100 + (16 + TC0480SCP_y_offs)) & 0x1ff,
						TC0480SCP_bgscrollx[layer] + i);
			}
		}
	}

	/* Decode any characters that have changed */
	if (TC0480SCP_chars_dirty)
	{
		int tile_index;

		for (tile_index = 0;tile_index < 64*64;tile_index++)
		{
			int attr = READ_WORD(&TC0480SCP_tx_ram[2*tile_index]);
			if (TC0480SCP_char_dirty[attr & 0xff])
				tilemap_mark_tile_dirty(TC0480SCP_tilemap[4],tile_index);
		}

		for (j = 0;j < TC0480SCP_TOTAL_CHARS;j++)
		{
			if (TC0480SCP_char_dirty[j])
				decodechar(Machine->gfx[TC0480SCP_tx_gfx],j,TC0480SCP_char_ram,&TC0480SCP_charlayout);
			TC0480SCP_char_dirty[j] = 0;
		}
		TC0480SCP_chars_dirty = 0;
	}

	tilemap_update(TC0480SCP_tilemap[0]);
	tilemap_update(TC0480SCP_tilemap[1]);
	tilemap_update(TC0480SCP_tilemap[2]);
	tilemap_update(TC0480SCP_tilemap[3]);
	tilemap_update(TC0480SCP_tilemap[4]);
}


/*********************************************************************
			LAYER ZOOM (still a WIP)

1) bg layers got too far left and down, the greater the magnification.
   Largely fixed by adding offsets (to startx&y) which get bigger as
   we zoom in.

2) Hthero and Footchmp bg layers behaved differently when zoomed.
   Fixed by bringing tc0480scp_x&y_offs into calculations.

3) Metalb "TAITO" text in attract too far to the right.
   Fixed(?) by bringing (layer*4) into offset calculations.
**********************************************************************/

static void zoomtilemap_draw(struct osd_bitmap *bitmap,int layer,int flags)
{
	/* <0x7f = shrunk e.g. 0x1a in Footchmp hiscore; 0x7f = normal;
		0xfefe = max(?) zoom e.g. start of game in Metalb */

	int zoom = 0x10000 + 0x7f - READ_WORD(&TC0480SCP_ctrl[0x10 + 2*layer]);

	if (zoom == 0x10000)	/* no zoom, so we won't need copyrozbitmap */
	{
		tilemap_set_clip(TC0480SCP_tilemap[layer],&Machine->visible_area);	// prevent bad things
		tilemap_draw(bitmap,TC0480SCP_tilemap[layer],flags);
	}
	else	/* zoom */
	{
		UINT32 startx,starty;
		int incxx,incxy,incyx,incyy;
		struct osd_bitmap *srcbitmap = TC0480SCP_tilemap[layer]->pixmap;
		int priority = flags >> 16;
		int flip = TC0480SCP_pri_reg & 0x40;

		tilemap_set_clip(TC0480SCP_tilemap[layer],0);

		if (!flip)
		{
			startx = ((TC0480SCP_bgscrollx[layer] + 16 + layer*4) << 16)
				- ((READ_WORD(&TC0480SCP_ctrl[0x20 + 2*layer]) & 0xff) << 8);	// low order byte

			incxx = zoom;
			incyx = 0;

			starty = (TC0480SCP_bgscrolly[layer] << 16)
				+ ((READ_WORD(&TC0480SCP_ctrl[0x28 + 2*layer]) & 0xff) << 8);	// low order byte
			incxy = 0;
			incyy = zoom;

			startx += (TC0480SCP_x_offs - 16 - layer*4) * incxx;
			starty -= (TC0480SCP_y_offs) * incyy;
		}
		else
		{
			startx = ((-TC0480SCP_bgscrollx[layer] + 16 + layer*4) << 16)
				- ((READ_WORD(&TC0480SCP_ctrl[0x20 + 2*layer]) & 0xff) << 8);	// low order byte

			incxx = zoom;
			incyx = 0;

			starty = (-TC0480SCP_bgscrolly[layer] << 16)
				+ ((READ_WORD(&TC0480SCP_ctrl[0x28 + 2*layer]) & 0xff) << 8);	// low order byte
			incxy = 0;
			incyy = zoom;

			startx += (TC0480SCP_x_offs - 16 - layer*4) * incxx;
			starty -= (TC0480SCP_y_offs) * incyy;
		}

		copyrozbitmap(bitmap,srcbitmap,startx,starty,
				incxx,incxy,incyx,incyy,
				1,	/* copy with wraparound */
				&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen,priority);
	}
}

void TC0480SCP_tilemap_draw(struct osd_bitmap *bitmap,int layer,int flags)
{

// DG: don't know where disable bit(s) are....
//	int disable = READ_WORD(&TC0480SCP_ctrl[0x1e]) & 0x1f;

	switch (layer)
	{
		case 0:
//			if (disable & 0x??) return;
			zoomtilemap_draw(bitmap,0,flags);
			break;
		case 1:
//			if (disable & 0x??) return;
			zoomtilemap_draw(bitmap,1,flags);
			break;
		case 2:
//			if (disable & 0x??) return;
			zoomtilemap_draw(bitmap,2,flags);
			break;
		case 3:
//			if (disable & 0x??) return;
			zoomtilemap_draw(bitmap,3,flags);
			break;
		case 4:
//			if (disable & 0x??) return;
			tilemap_draw(bitmap,TC0480SCP_tilemap[4],flags);
			break;
	}
}


/***************************************************************************/


static int TC0110PCR_addr;
static UINT16 *TC0110PCR_ram;
#define TC0110PCR_RAM_SIZE 0x2000

int TC0110PCR_vh_start(void)
{
	TC0110PCR_ram = (UINT16*)malloc(TC0110PCR_RAM_SIZE * sizeof(*TC0110PCR_ram));

	if (!TC0110PCR_ram) return 1;

	return 0;
}

void TC0110PCR_vh_stop(void)
{
	free(TC0110PCR_ram);
	TC0110PCR_ram = 0;
}

READ_HANDLER( TC0110PCR_word_r )
{
	switch (offset)
	{
		case 2:
			return TC0110PCR_ram[TC0110PCR_addr];

		default:
			return 0xff;
	}
}

WRITE_HANDLER( TC0110PCR_word_w )
{
	switch (offset)
	{
		case 0:
			TC0110PCR_addr = (data >> 1) & 0xfff;   /* In test mode game writes to odd register number so it is (data>>1) */
			break;

		case 2:
		{
			int r,g,b;   /* data = palette BGR value */


			TC0110PCR_ram[TC0110PCR_addr] = data & 0xffff;

			r = (data >>  0) & 0x1f;
			g = (data >>  5) & 0x1f;
			b = (data >> 10) & 0x1f;

			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 3) | (b >> 2);

			palette_change_color(TC0110PCR_addr,r,g,b);
			break;
		}

		default:
			break;
	}
}


/***************************************************************************/


static unsigned char TC0220IOC_regs[16];

READ_HANDLER( TC0220IOC_r )
{
	switch (offset)
	{
		case 0x00:	/* IN00-07 (DSA) */
			return readinputport(0);

		case 0x01:	/* IN08-15 (DSB) */
			return readinputport(1);

		case 0x02:	/* IN16-23 (1P) */
			return readinputport(2);

		case 0x03:	/* IN24-31 (2P) */
			return readinputport(3);

		case 0x04:	/* coin counters and lockout */
			return TC0220IOC_regs[4];

		case 0x07:	/* INB0-7 (coin) */
			return readinputport(4);

		default:
			return 0xff;
	}
}

WRITE_HANDLER( TC0220IOC_w )
{
	TC0220IOC_regs[offset] = data;

	switch (offset)
	{
		case 0x00:
			watchdog_reset_w(offset,data);
			break;

		case 0x04:	/* coin counters and lockout */
			coin_lockout_w(0,~data & 0x01);
			coin_lockout_w(1,~data & 0x02);
			coin_counter_w(0,data & 0x04);
			coin_counter_w(1,data & 0x08);
			break;

		default:
			break;
	}
}


/***************************************************************************/


static unsigned char TC0510NIO_regs[16];

READ_HANDLER( TC0510NIO_r )
{
	switch (offset)
	{
		case 0x00:	/* DSA */
			return readinputport(0);

		case 0x01:	/* DSB */
			return readinputport(1);

		case 0x02:	/* 1P */
			return readinputport(2);

		case 0x03:	/* 2P */
			return readinputport(3);

		case 0x04:	/* coin counters and lockout */
			return TC0510NIO_regs[4];

		case 0x07:	/* coin */
			return readinputport(4);

		default:
			return 0xff;
	}
}

WRITE_HANDLER( TC0510NIO_w )
{
	TC0510NIO_regs[offset] = data;

	switch (offset)
	{
		case 0x00:
			watchdog_reset_w(offset,data);
			break;

		case 0x04:	/* coin counters and lockout */
			coin_lockout_w(0,~data & 0x01);
			coin_lockout_w(1,~data & 0x02);
			coin_counter_w(0,data & 0x04);
			coin_counter_w(1,data & 0x08);
			break;

		default:
			break;
	}
}
