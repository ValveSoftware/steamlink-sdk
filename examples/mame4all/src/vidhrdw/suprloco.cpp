/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"


extern unsigned char *spriteram;
extern size_t spriteram_size;

unsigned char *suprloco_videoram;

static struct tilemap *bg_tilemap;
static int control;

#define SPR_Y_TOP		0
#define SPR_Y_BOTTOM	1
#define SPR_X			2
#define SPR_COL			3
#define SPR_SKIP_LO		4
#define SPR_SKIP_HI		5
#define SPR_GFXOFS_LO	6
#define SPR_GFXOFS_HI	7


/***************************************************************************

  Convert the color PROMs into a more useable format.

  I'm not sure about the resistor values, I'm using the Galaxian ones.

***************************************************************************/
void suprloco_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < 512;i++)
	{
		int bit0,bit1,bit2;

		/* red component */
		bit0 = (*color_prom >> 0) & 0x01;
		bit1 = (*color_prom >> 1) & 0x01;
		bit2 = (*color_prom >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (*color_prom >> 3) & 0x01;
		bit1 = (*color_prom >> 4) & 0x01;
		bit2 = (*color_prom >> 5) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (*color_prom >> 6) & 0x01;
		bit2 = (*color_prom >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}

	/* generate a second bank of sprite palette with red changed to purple */
	for (i = 0;i < 256;i++)
	{
		*(palette++) = palette[-256*3];
		*(palette++) = palette[-256*3];
		if ((i & 0x0f) == 0x09)
			*(palette++) = 0xff;
		else
			*(palette++) = palette[-256*3];
	}
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_tile_info(int tile_index)
{
	unsigned char attr = suprloco_videoram[2*tile_index+1];
	SET_TILE_INFO(0,suprloco_videoram[2*tile_index] | ((attr & 0x03) << 8),(attr & 0x1c) >> 2)
	tile_info.priority = (attr & 0x20) >> 5;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int suprloco_vh_start(void)
{
	bg_tilemap = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,32,32);

	if (!bg_tilemap)
		return 1;

	tilemap_set_scroll_rows(bg_tilemap,32);

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( suprloco_videoram_w )
{
	if (suprloco_videoram[offset] != data)
	{
		suprloco_videoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}

static int suprloco_scrollram[32];

WRITE_HANDLER( suprloco_scrollram_w )
{
	int adj = flip_screen ? -8 : 8;

	suprloco_scrollram[offset] = data;
	tilemap_set_scrollx(bg_tilemap,offset, data - adj);
}

READ_HANDLER( suprloco_scrollram_r )
{
	return suprloco_scrollram[offset];
}

WRITE_HANDLER( suprloco_control_w )
{
	/* There is probably a palette select in here */

   	/* Bit 0   - coin counter A */
	/* Bit 1   - coin counter B (only used if coinage differs from A) */
	/* Bit 2-3 - probably unused */
	/* Bit 4   - ??? */
	/* Bit 5   - pulsated when loco turns "super" */
	/* Bit 6   - probably unused */
	/* Bit 7   - flip screen */

	if ((control & 0x10) != (data & 0x10))
	{
		/*logerror("Bit 4 = %d\n", (data >> 4) & 1); */
	}

	coin_counter_w(0, data & 0x01);
	coin_counter_w(1, data & 0x02);

	flip_screen_w(0,data & 0x80);
	tilemap_set_scrolly(bg_tilemap,0,flip_screen ? -32 : 0);

	control = data;
}


READ_HANDLER( suprloco_control_r )
{
	return control;
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

INLINE void draw_pixel(struct osd_bitmap *bitmap,int x,int y,int color)
{
	if (flip_screen)
	{
		x = bitmap->width - x - 1;
		y = bitmap->height - y - 1;
	}

	if (x < Machine->visible_area.min_x ||
		x > Machine->visible_area.max_x ||
		y < Machine->visible_area.min_y ||
		y > Machine->visible_area.max_y)
		return;

	plot_pixel(bitmap, x, y, color);
}


static void render_sprite(struct osd_bitmap *bitmap,int spr_number)
{
	int sx,sy,col,row,height,src,adjy,dy;
	unsigned char *spr_reg;
	unsigned short *spr_palette;
	short skip;	/* bytes to skip before drawing each row (can be negative) */


	spr_reg	= spriteram + 0x10 * spr_number;

	src = spr_reg[SPR_GFXOFS_LO] + (spr_reg[SPR_GFXOFS_HI] << 8);
	skip = spr_reg[SPR_SKIP_LO] + (spr_reg[SPR_SKIP_HI] << 8);

	height		= spr_reg[SPR_Y_BOTTOM] - spr_reg[SPR_Y_TOP];
	spr_palette	= Machine->remapped_colortable + 0x100 + 0x10 * spr_reg[SPR_COL] + ((control & 0x20)?0x100:0);
	sx = spr_reg[SPR_X];
	sy = spr_reg[SPR_Y_TOP] + 1;

	if (!flip_screen)
	{
		adjy = sy;
		dy = 1;
	}
	else
	{
		adjy = sy + height + 30;  /* some of the sprites are still off by a pixel */
		dy = -1;
	}

	for (row = 0;row < height;row++,adjy+=dy)
	{
		int color1,color2,flipx;
		UINT8 data;
		UINT8 *gfx;

		src += skip;

		col = 0;

		/* get pointer to packed sprite data */
		gfx = &(memory_region(REGION_GFX2)[src & 0x7fff]);
		flipx = src & 0x8000;   /* flip x */

		while (1)
		{
			if (flipx)	/* flip x */
			{
				data = *gfx--;
				color1 = data & 0x0f;
				color2 = data >> 4;
			}
			else
			{
				data = *gfx++;
				color1 = data >> 4;
				color2 = data & 0x0f;
			}

			if (color1 == 15) break;
			if (color1)
				draw_pixel(bitmap,sx+col,  adjy,spr_palette[color1]);

			if (color2 == 15) break;
			if (color2)
				draw_pixel(bitmap,sx+col+1,adjy,spr_palette[color2]);

			col += 2;
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int spr_number;
	unsigned char *spr_reg;


	for (spr_number = 0;spr_number < (spriteram_size >> 4);spr_number++)
	{
		spr_reg = spriteram + 0x10 * spr_number;
		if (spr_reg[SPR_X] != 0xff)
			render_sprite(bitmap,spr_number);
	}
}

void suprloco_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,0);
	draw_sprites(bitmap);
	tilemap_draw(bitmap,bg_tilemap,1);
}
