/*******************************************************************

Rolling Thunder Video Hardware

*******************************************************************/

#include "driver.h"


#define GFX_TILES1	0
#define GFX_TILES2	1
#define GFX_SPRITES	2

unsigned char *rthunder_videoram1,*rthunder_videoram2;
extern unsigned char *spriteram;

static int tilebank;
static int xscroll[4], yscroll[4];	/* scroll + priority */

static struct tilemap *tilemap[4];

static int backcolor;
static int flipscreen;
static const unsigned char *tile_address_prom;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Rolling Thunder has two palette PROMs (512x8 and 512x4) and two 2048x8
  lookup table PROMs.
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 1  kohm resistor  -- BLUE
  bit 0 -- 2.2kohm resistor  -- BLUE

  bit 7 -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 2.2kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
        -- 1  kohm resistor  -- RED
  bit 0 -- 2.2kohm resistor  -- RED

***************************************************************************/

void namcos86_vh_convert_color_prom( unsigned char *palette,unsigned short *colortable,const unsigned char *color_prom )
{
	int i;
	int totcolors,totlookup;


	totcolors = Machine->drv->total_colors;
	totlookup = Machine->drv->color_table_len;

	for (i = 0;i < totcolors;i++)
	{
		int bit0,bit1,bit2,bit3;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[totcolors] >> 0) & 0x01;
		bit1 = (color_prom[totcolors] >> 1) & 0x01;
		bit2 = (color_prom[totcolors] >> 2) & 0x01;
		bit3 = (color_prom[totcolors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += totcolors;
	/* color_prom now points to the beginning of the lookup table */

	/* tiles lookup table */
	for (i = 0;i < totlookup/2;i++)
		*(colortable++) = *color_prom++;

	/* sprites lookup table */
	for (i = 0;i < totlookup/2;i++)
		*(colortable++) = *(color_prom++) + totcolors/2;

	/* color_prom now points to the beginning of the tile address decode PROM */

	tile_address_prom = color_prom;	/* we'll need this at run time */
}




/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static unsigned char *videoram;
static int gfx_num;
static int tile_offs[4];

static void tilemap0_preupdate(void)
{
	videoram = &rthunder_videoram1[0x0000];
	gfx_num = GFX_TILES1;
	tile_offs[0] = ((tile_address_prom[0x00] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	tile_offs[1] = ((tile_address_prom[0x04] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	tile_offs[2] = ((tile_address_prom[0x08] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	tile_offs[3] = ((tile_address_prom[0x0c] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
}

static void tilemap1_preupdate(void)
{
	videoram = &rthunder_videoram1[0x1000];
	gfx_num = GFX_TILES1;
	tile_offs[0] = ((tile_address_prom[0x10] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	tile_offs[1] = ((tile_address_prom[0x14] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	tile_offs[2] = ((tile_address_prom[0x18] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	tile_offs[3] = ((tile_address_prom[0x1c] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
}

static void tilemap2_preupdate(void)
{
	videoram = &rthunder_videoram2[0x0000];
	gfx_num = GFX_TILES2;
	tile_offs[0] = ((tile_address_prom[0x00] & 0xe0) >> 5) * 0x100;
	tile_offs[1] = ((tile_address_prom[0x01] & 0xe0) >> 5) * 0x100;
	tile_offs[2] = ((tile_address_prom[0x02] & 0xe0) >> 5) * 0x100;
	tile_offs[3] = ((tile_address_prom[0x03] & 0xe0) >> 5) * 0x100;
}

static void tilemap3_preupdate(void)
{
	videoram = &rthunder_videoram2[0x1000];
	gfx_num = GFX_TILES2;
	tile_offs[0] = ((tile_address_prom[0x10] & 0xe0) >> 5) * 0x100;
	tile_offs[1] = ((tile_address_prom[0x11] & 0xe0) >> 5) * 0x100;
	tile_offs[2] = ((tile_address_prom[0x12] & 0xe0) >> 5) * 0x100;
	tile_offs[3] = ((tile_address_prom[0x13] & 0xe0) >> 5) * 0x100;
}

static void get_tile_info(int tile_index)
{
	unsigned char attr = videoram[2*tile_index + 1];
	SET_TILE_INFO(gfx_num,videoram[2*tile_index] + tile_offs[attr & 0x03],attr)
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int namcos86_vh_start(void)
{
	tilemap[0] = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);
	tilemap[1] = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);
	tilemap[2] = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);
	tilemap[3] = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	if (!tilemap[0] || !tilemap[1] || !tilemap[2] || !tilemap[3])
		return 1;

	tilemap[0]->transparent_pen = 7;
	tilemap[1]->transparent_pen = 7;
	tilemap[2]->transparent_pen = 7;
	tilemap[3]->transparent_pen = 7;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

READ_HANDLER( rthunder_videoram1_r )
{
	return rthunder_videoram1[offset];
}

WRITE_HANDLER( rthunder_videoram1_w )
{
	if (rthunder_videoram1[offset] != data)
	{
		rthunder_videoram1[offset] = data;
		tilemap_mark_tile_dirty(tilemap[offset/0x1000],(offset & 0xfff)/2);
	}
}

READ_HANDLER( rthunder_videoram2_r )
{
	return rthunder_videoram2[offset];
}

WRITE_HANDLER( rthunder_videoram2_w )
{
	if (rthunder_videoram2[offset] != data)
	{
		rthunder_videoram2[offset] = data;
		tilemap_mark_tile_dirty(tilemap[2+offset/0x1000],(offset & 0xfff)/2);
	}
}

WRITE_HANDLER( rthunder_tilebank_select_0_w )
{
	if (tilebank != 0)
	{
		tilebank = 0;
		tilemap_mark_all_tiles_dirty(tilemap[0]);
		tilemap_mark_all_tiles_dirty(tilemap[1]);
	}
}

WRITE_HANDLER( rthunder_tilebank_select_1_w )
{
	if (tilebank != 1)
	{
		tilebank = 1;
		tilemap_mark_all_tiles_dirty(tilemap[0]);
		tilemap_mark_all_tiles_dirty(tilemap[1]);
	}
}

static void scroll_w(int layer,int offset,int data)
{
	int xdisp[4] = { 36,34,37,35 };
	int ydisp = 9;
	int scrollx,scrolly;


	switch (offset)
	{
		case 0:
			xscroll[layer] = (xscroll[layer]&0xff)|(data<<8);
			break;
		case 1:
			xscroll[layer] = (xscroll[layer]&0xff00)|data;
			break;
		case 2:
			yscroll[layer] = data;
			break;
	}

	scrollx = xscroll[layer]+xdisp[layer];
	scrolly = yscroll[layer]+ydisp;
	if (flipscreen)
	{
		scrollx = -scrollx+256;
		scrolly = -scrolly;
	}
	tilemap_set_scrollx(tilemap[layer],0,scrollx-16);
	tilemap_set_scrolly(tilemap[layer],0,scrolly+16);
}

WRITE_HANDLER( rthunder_scroll0_w )
{
	scroll_w(0,offset,data);
}
WRITE_HANDLER( rthunder_scroll1_w )
{
	scroll_w(1,offset,data);
}
WRITE_HANDLER( rthunder_scroll2_w )
{
	scroll_w(2,offset,data);
}
WRITE_HANDLER( rthunder_scroll3_w )
{
	scroll_w(3,offset,data);
}


WRITE_HANDLER( rthunder_backcolor_w )
{
	backcolor = data;
}


/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites( struct osd_bitmap *bitmap, int sprite_priority )
{
	/* note: sprites don't yet clip at the top of the screen properly */
	const struct rectangle *clip = &Machine->visible_area;

	const unsigned char *source = &spriteram[0x1400];
	const unsigned char *finish = &spriteram[0x1c00-16];	/* the last is NOT a sprite */

	int sprite_xoffs = spriteram[0x1bf5] - 256 * (spriteram[0x1bf4] & 1);
	int sprite_yoffs = spriteram[0x1bf7] - 256 * (spriteram[0x1bf6] & 1);

	while( source<finish )
	{
/*
	source[4]	S-FT -BBB
	source[5]	TTTT TTTT
	source[6]   CCCC CCCX
	source[7]	XXXX XXXX
	source[8]	PPPT -S-F
	source[9]   YYYY YYYY
*/
		unsigned char priority = source[8];
		if( priority>>5 == sprite_priority )
		{
			unsigned char attrs = source[4];
			unsigned char color = source[6];
			int sx = source[7] + (color&1)*256; /* need adjust for left clip */
			int sy = -source[9];
			int flipx = attrs&0x20;
			int flipy = priority & 0x01;
			int tall = (priority&0x04)?1:0;
			int wide = (attrs&0x80)?1:0;
			int sprite_bank = attrs&7;
			int sprite_number = (source[5]&0xff)*4;
			int row,col;

			if ((attrs & 0x10) && !wide) sprite_number += 1;
			if ((priority & 0x10) && !tall) sprite_number += 2;
			color = color>>1;

			if (sx>512-32) sx -= 512;
			if (sy < -209-32) sy += 256;

			if (flipx && !wide) sx-=16;
			if (!tall) sy+=16;
//			if (flipy && !tall) sy+=16;

			sx += sprite_xoffs;
			sy -= sprite_yoffs;

			for( row=0; row<=tall; row++ )
			{
				for( col=0; col<=wide; col++ )
				{
					if (flipscreen)
					{
						drawgfx( bitmap, Machine->gfx[GFX_SPRITES+sprite_bank],
							sprite_number+2*row+col,
							color,
							!flipx,!flipy,
							512-16-67 - (sx+16*(flipx?1-col:col)),
							64-16+209 - (sy+16*(flipy?1-row:row)),
							clip,
							TRANSPARENCY_PEN, 0xf );
					}
					else
					{
						drawgfx( bitmap, Machine->gfx[GFX_SPRITES+sprite_bank],
							sprite_number+2*row+col,
							color,
							flipx,flipy,
							-67 + (sx+16*(flipx?1-col:col)),
							209 + (sy+16*(flipy?1-row:row)),
							clip,
							TRANSPARENCY_PEN, 0xf );
					}
				}
			}
		}
		source+=16;
	}
}



void namcos86_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer;

	/* this is the global sprite Y offset, actually */
	flipscreen = spriteram[0x1bf6] & 1;

	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	tilemap0_preupdate(); tilemap_update(tilemap[0]);
	tilemap1_preupdate(); tilemap_update(tilemap[1]);
	tilemap2_preupdate(); tilemap_update(tilemap[2]);
	tilemap3_preupdate(); tilemap_update(tilemap[3]);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,Machine->gfx[0]->colortable[8*backcolor+7],&Machine->visible_area);

	for (layer = 0;layer < 8;layer++)
	{
		int i;

		for (i = 3;i >= 0;i--)
		{
			if (((xscroll[i] & 0x0e00) >> 9) == layer)
				tilemap_draw(bitmap,tilemap[i],0);
		}

		draw_sprites(bitmap,layer);
	}
#if 0
{
	char buf[80];
int b=keyboard_pressed(KEYCODE_Y)?8:0;
	sprintf(buf,"%02x %02x %02x %02x %02x %02x %02x %02x",
			spriteram[0x1bf0+b],
			spriteram[0x1bf1+b],
			spriteram[0x1bf2+b],
			spriteram[0x1bf3+b],
			spriteram[0x1bf4+b],
			spriteram[0x1bf5+b],
			spriteram[0x1bf6+b],
			spriteram[0x1bf7+b]);
	usrintf_showmessage(buf);
}
#endif
}
