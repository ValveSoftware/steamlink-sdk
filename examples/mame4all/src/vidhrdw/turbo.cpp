/*************************************************************************

	 Turbo - Sega - 1981

	 Video Hardware

*************************************************************************/

#ifndef DRAW_CORE_INCLUDE

#include "driver.h"
#include "vidhrdw/generic.h"

/* constants */
#define VIEW_WIDTH					(32*8)
#define VIEW_HEIGHT					(28*8)

/* external definitions */
UINT8 *turbo_sprite_position;
UINT8 turbo_collision;

/* from machine */
extern UINT8 turbo_opa, turbo_opb, turbo_opc;
extern UINT8 turbo_ipa, turbo_ipb, turbo_ipc;
extern UINT8 turbo_fbpla, turbo_fbcol;
extern UINT8 turbo_segment_data[32];
extern UINT8 turbo_speed;

/* internal data */
static UINT8 *sprite_gfxdata, *sprite_priority;
static UINT8 *road_gfxdata, *road_palette, *road_enable_collide;
static UINT8 *back_gfxdata, *back_palette;
static UINT8 *overall_priority, *collision_map;

/* sprite tracking */
struct sprite_params_data
{
	UINT32 *base;
	int offset, rowbytes;
	int yscale, miny, maxy;
	int xscale, xoffs;
};
static struct sprite_params_data sprite_params[16];
static UINT32 *sprite_expanded_data;

/* orientation */
static const struct rectangle game_clip = { 0, VIEW_WIDTH - 1, 64, 64 + VIEW_HEIGHT - 1 };
static struct rectangle adjusted_clip;
static int startx, starty, deltax, deltay;

/* misc other stuff */
static UINT16 *back_expanded_data;
static UINT16 *road_expanded_palette;
static UINT8 drew_frame;

/* prototypes */
static void draw_everything_core_8(struct osd_bitmap *bitmap);
static void draw_everything_core_16(struct osd_bitmap *bitmap);
static void draw_minimal(struct osd_bitmap *bitmap);


/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/

void turbo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom)
{
	int i;

	for (i = 0; i < 512; i++, color_prom++)
	{
		int bit0, bit1, bit2;

		/* bits 4,5,6 of the index are inverted before being used as addresses */
		/* to save ourselves lots of trouble, we will undo the inversion when */
		/* generating the palette */
		int adjusted_index = i ^ 0x70;

		/* red component */
		bit0 = (*color_prom >> 0) & 1;
		bit1 = (*color_prom >> 1) & 1;
		bit2 = (*color_prom >> 2) & 1;
		palette[adjusted_index * 3 + 0] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (*color_prom >> 3) & 1;
		bit1 = (*color_prom >> 4) & 1;
		bit2 = (*color_prom >> 5) & 1;
		palette[adjusted_index * 3 + 1] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (*color_prom >> 6) & 1;
		bit2 = (*color_prom >> 7) & 1;
		palette[adjusted_index * 3 + 2] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}

	/* LED segments colors: black and red */
	palette[512 * 3 + 0] = 0x00;
	palette[512 * 3 + 1] = 0x00;
	palette[512 * 3 + 2] = 0x00;
	palette[513 * 3 + 0] = 0xff;
	palette[513 * 3 + 1] = 0x00;
	palette[513 * 3 + 2] = 0x00;
	/* Tachometer colors: Led colors + yellow and green */
	palette[514 * 3 + 0] = 0x00;
	palette[514 * 3 + 1] = 0x00;
	palette[514 * 3 + 2] = 0x00;
	palette[515 * 3 + 0] = 0xff;
	palette[515 * 3 + 1] = 0xff;
	palette[515 * 3 + 2] = 0x00;
	palette[516 * 3 + 0] = 0x00;
	palette[516 * 3 + 1] = 0x00;
	palette[516 * 3 + 2] = 0x00;
	palette[517 * 3 + 0] = 0x00;
	palette[517 * 3 + 1] = 0xff;
	palette[517 * 3 + 2] = 0x00;
}


/***************************************************************************

  Video startup/shutdown

***************************************************************************/

int turbo_vh_start(void)
{
	int i, j, sprite_length, sprite_bank_size, back_length;
	UINT32 sprite_expand[16];
	UINT32 *dst;
	UINT16 *bdst;
	UINT8 *src;

	/* allocate the expanded sprite data */
	sprite_length = memory_region_length(REGION_GFX1);
	sprite_bank_size = sprite_length / 8;
	sprite_expanded_data = (UINT32*)malloc(sprite_length * 2 * sizeof(UINT32));
	if (!sprite_expanded_data)
		return 1;

	/* allocate the expanded background data */
	back_length = memory_region_length(REGION_GFX3);
	back_expanded_data = (UINT16*)malloc(back_length);
	if (!back_expanded_data)
	{
		free(sprite_expanded_data);
		return 1;
	}

	/* allocate the expanded road palette */
	road_expanded_palette = (UINT16*)malloc(0x40 * sizeof(UINT16));
	if (!road_expanded_palette)
	{
		free(back_expanded_data);
		free(sprite_expanded_data);
		return 1;
	}

	/* determine ROM/PROM addresses */
	sprite_gfxdata = memory_region(REGION_GFX1);
	sprite_priority = memory_region(REGION_PROMS) + 0x0200;

	road_gfxdata = memory_region(REGION_GFX2);
	road_palette = memory_region(REGION_PROMS) + 0x0b00;
	road_enable_collide = memory_region(REGION_PROMS) + 0x0b40;

	back_gfxdata = memory_region(REGION_GFX3);
	back_palette = memory_region(REGION_PROMS) + 0x0a00;

	overall_priority = memory_region(REGION_PROMS) + 0x0600;
	collision_map = memory_region(REGION_PROMS) + 0x0b60;

	/* compute the sprite expansion array */
	for (i = 0; i < 16; i++)
	{
		UINT32 value = 0;
		if (i & 1) value |= 0x00000001;
		if (i & 2) value |= 0x00000100;
		if (i & 4) value |= 0x00010000;
		if (i & 8) value |= 0x01000000;

		/* special value for the end-of-row */
		if ((i & 0x0c) == 0x04) value = 0x12345678;

		sprite_expand[i] = value;
	}

	/* expand the sprite ROMs */
	src = sprite_gfxdata;
	dst = sprite_expanded_data;
	for (i = 0; i < 8; i++)
	{
		/* expand this bank */
		for (j = 0; j < sprite_bank_size; j++)
		{
			*dst++ = sprite_expand[*src >> 4];
			*dst++ = sprite_expand[*src++ & 15];
		}

		/* shift for the next bank */
		for (j = 0; j < 16; j++)
			if (sprite_expand[j] != 0x12345678) sprite_expand[j] <<= 1;
	}

	/* expand the background ROMs */
	src = back_gfxdata;
	bdst = back_expanded_data;
	for (i = 0; i < back_length / 2; i++, src++)
	{
		int bits1 = src[0];
		int bits2 = src[back_length / 2];
		int newbits = 0;

		for (j = 0; j < 8; j++)
		{
			newbits |= ((bits1 >> (j ^ 7)) & 1) << (j * 2);
			newbits |= ((bits2 >> (j ^ 7)) & 1) << (j * 2 + 1);
		}
		*bdst++ = newbits;
	}

	/* expand the road palette */
	src = road_palette;
	bdst = road_expanded_palette;
	for (i = 0; i < 0x20; i++, src++)
		*bdst++ = src[0] | (src[0x20] << 8);

	/* set the default drawing parameters */
	startx = game_clip.min_x;
	starty = game_clip.min_y;
	deltax = deltay = 1;
	adjusted_clip = game_clip;

	/* adjust our parameters for the specified orientation */
	if (Machine->orientation)
	{
		int temp;

		if (Machine->orientation & ORIENTATION_SWAP_XY)
		{
			temp = startx; startx = starty; starty = temp;
			temp = adjusted_clip.min_x; adjusted_clip.min_x = adjusted_clip.min_y; adjusted_clip.min_y = temp;
			temp = adjusted_clip.max_x; adjusted_clip.max_x = adjusted_clip.max_y; adjusted_clip.max_y = temp;
		}
		if (Machine->orientation & ORIENTATION_FLIP_X)
		{
			startx = adjusted_clip.max_x;
			if (!(Machine->orientation & ORIENTATION_SWAP_XY)) deltax = -deltax;
			else deltay = -deltay;
		}
		if (Machine->orientation & ORIENTATION_FLIP_Y)
		{
			starty = adjusted_clip.max_y;
			if (!(Machine->orientation & ORIENTATION_SWAP_XY)) deltay = -deltay;
			else deltax = -deltax;
		}
	}

	/* other stuff */
	drew_frame = 0;

	/* return success */
	return 0;
}


void turbo_vh_stop(void)
{
	free(sprite_expanded_data);
	free(back_expanded_data);
	free(road_expanded_palette);
}


/***************************************************************************

  Sprite information

***************************************************************************/

static void update_sprite_info(void)
{
	struct sprite_params_data *data = sprite_params;
	int i;

	/* first loop over all sprites and update those whose scanlines intersect ours */
	for (i = 0; i < 16; i++, data++)
	{
		UINT8 *sprite_base = spriteram + 16 * i;

		/* snarf all the data */
		data->base = sprite_expanded_data + (i & 7) * 0x8000;
		data->offset = (sprite_base[6] + 256 * sprite_base[7]) & 0x7fff;
		data->rowbytes = (INT16)(sprite_base[4] + 256 * sprite_base[5]);
		data->miny = sprite_base[0];
		data->maxy = sprite_base[1];
		data->xscale = ((5 * 256 - 4 * sprite_base[2]) << 16) / (5 * 256);
		data->yscale = (4 << 16) / (sprite_base[3] + 4);
		data->xoffs = -1;
	}

	/* now find the X positions */
	for (i = 0; i < 0x200; i++)
	{
		int value = turbo_sprite_position[i];
		if (value)
		{
			int base = (i & 0x100) >> 5;
			int which;
			for (which = 0; which < 8; which++)
				if (value & (1 << which))
					sprite_params[base + which].xoffs = i & 0xff;
		}
	}
}


/***************************************************************************

  Internal draw routines

***************************************************************************/

static void draw_one_sprite(const struct sprite_params_data *data, UINT32 *dest, int xclip, int scanline)
{
	int xstep = data->xscale;
	int xoffs = data->xoffs;
	int xcurr, offset;
	UINT32 *src;

	/* xoffs of -1 means don't draw */
	if (xoffs == -1) return;

	/* clip to the road */
	xcurr = 0;
	if (xoffs < xclip)
	{
		/* the pixel clock starts on xoffs regardless of clipping; take this into account */
		xcurr = ((xclip - xoffs) * xstep) & 0xffff;
		xoffs = xclip;
	}

	/* compute the current data offset */
	scanline = ((scanline - data->miny) * data->yscale) >> 16;
	offset = data->offset + (scanline + 1) * data->rowbytes;

	/* determine the bitmap location */
	src = &data->base[offset & 0x7fff];

	/* loop over columns */
	while (xoffs < VIEW_WIDTH)
	{
		UINT32 srcval = src[xcurr >> 16];

		/* stop on the end-of-row signal */
		if (srcval == 0x12345678) break;
		dest[xoffs++] |= srcval;
		xcurr += xstep;
	}
}


static void draw_road_sprites(UINT32 *dest, int scanline)
{
	static const struct sprite_params_data *param_list[6] =
	{
		&sprite_params[0], &sprite_params[8],
		&sprite_params[1], &sprite_params[9],
		&sprite_params[2], &sprite_params[10]
	};
	int i;

	/* loop over the road sprites */
	for (i = 0; i < 6; i++)
	{
		const struct sprite_params_data *data = param_list[i];

		/* if the sprite intersects this scanline, draw it */
		if (scanline >= data->miny && scanline < data->maxy)
			draw_one_sprite(data, dest, 0, scanline);
	}
}


static void draw_offroad_sprites(UINT32 *dest, int road_column, int scanline)
{
	static const struct sprite_params_data *param_list[10] =
	{
		&sprite_params[3], &sprite_params[11],
		&sprite_params[4], &sprite_params[12],
		&sprite_params[5], &sprite_params[13],
		&sprite_params[6], &sprite_params[14],
		&sprite_params[7], &sprite_params[15]
	};
	int i;

	/* loop over the offroad sprites */
	for (i = 0; i < 10; i++)
	{
		const struct sprite_params_data *data = param_list[i];

		/* if the sprite intersects this scanline, draw it */
		if (scanline >= data->miny && scanline < data->maxy)
			draw_one_sprite(data, dest, road_column, scanline);
	}
}


static void draw_scores(struct osd_bitmap *bitmap)
{
	struct rectangle clip;
	int offs, x, y;

	/* current score */
	offs = 31;
	for (y = 0; y < 5; y++, offs--)
		drawgfx(bitmap, Machine->gfx[0],
				turbo_segment_data[offs],
				0,
				0, 0,
				14*8, (2 + y) * 8,
				&Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* high scores */
	for (x = 0; x < 5; x++)
	{
		offs = 6 + x * 5;
		for (y = 0; y < 5; y++, offs--)
			drawgfx(bitmap, Machine->gfx[0],
					turbo_segment_data[offs],
					0,
					0, 0,
					(20 + 2 * x) * 8, (2 + y) * 8,
					&Machine->visible_area, TRANSPARENCY_NONE, 0);
	}

	/* tachometer */
	clip = Machine->visible_area;
	clip.min_x = 5*8;
	clip.max_x = clip.min_x + 1;
	for (y = 0; y < 22; y++)
	{
		static UINT8 led_color[] = { 2, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0 };
		int code = ((y / 2) <= turbo_speed) ? 0 : 1;

		drawgfx(bitmap, Machine->gfx[1],
				code,
				led_color[y / 2],
				0,0,
				5*8, y*2+8,
				&clip, TRANSPARENCY_NONE, 0);
		if (y % 3 == 2)
			clip.max_x++;
	}

	/* shifter status */
	if (readinputport(0) & 0x04)
	{
		drawgfx(bitmap, Machine->gfx[2], 'H', 0, 0,0, 2*8,3*8, &Machine->visible_area, TRANSPARENCY_NONE, 0);
		drawgfx(bitmap, Machine->gfx[2], 'I', 0, 0,0, 2*8,4*8, &Machine->visible_area, TRANSPARENCY_NONE, 0);
	}
	else
	{
		drawgfx(bitmap, Machine->gfx[2], 'L', 0, 0,0, 2*8,3*8, &Machine->visible_area, TRANSPARENCY_NONE, 0);
		drawgfx(bitmap, Machine->gfx[2], 'O', 0, 0,0, 2*8,4*8, &Machine->visible_area, TRANSPARENCY_NONE, 0);
	}
}



/***************************************************************************

  Master refresh routine

***************************************************************************/

void turbo_vh_eof(void)
{
	/* only do collision checking if we didn't draw */
	if (!drew_frame)
	{
		update_sprite_info();
		draw_minimal(Machine->scrbitmap);
	}
	drew_frame = 0;
}

void turbo_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* update the sprite data */
	update_sprite_info();

	/* perform the actual drawing */
	if (bitmap->depth == 8)
		draw_everything_core_8(bitmap);
	else
		draw_everything_core_16(bitmap);

	/* draw the LEDs for the scores */
	draw_scores(bitmap);

	/* indicate that we drew this frame, so that the eof callback doesn't bother doing anything */
	drew_frame = 1;
}


/***************************************************************************

  Road drawing generators

***************************************************************************/

#define DRAW_CORE_INCLUDE

#define FULL_DRAW	1

#define DRAW_FUNC	draw_everything_core_8
#define TYPE		UINT8
#include "turbo.cpp"
#undef TYPE
#undef DRAW_FUNC

#define DRAW_FUNC	draw_everything_core_16
#define TYPE		UINT16
#include "turbo.cpp"
#undef TYPE
#undef DRAW_FUNC

#undef FULL_DRAW
#define FULL_DRAW	0

#define DRAW_FUNC	draw_minimal
#define TYPE		UINT8
#include "turbo.cpp"
#undef TYPE
#undef DRAW_FUNC

#undef FULL_DRAW

#else

/***************************************************************************

  Road drawing routine

***************************************************************************/

static void DRAW_FUNC(struct osd_bitmap *bitmap)
{
	TYPE *base = &((TYPE *)bitmap->line[starty])[startx];
	UINT32 sprite_buffer[VIEW_WIDTH + 256];

	UINT8 *overall_priority_base = &overall_priority[(turbo_fbpla & 8) << 6];
	UINT8 *sprite_priority_base = &sprite_priority[(turbo_fbpla & 7) << 7];
	UINT8 *road_gfxdata_base = &road_gfxdata[(turbo_opc << 5) & 0x7e0];
	UINT16 *road_palette_base = &road_expanded_palette[(turbo_fbcol & 1) << 4];

	int dx = deltax, dy = deltay, rowsize = (bitmap->line[1] - bitmap->line[0]) * 8 / bitmap->depth;
	UINT16 *colortable;
	int x, y, i;

	/* expand the appropriate delta */
	if (Machine->orientation & ORIENTATION_SWAP_XY)
		dx *= rowsize;
	else
		dy *= rowsize;

	/* determine the color offset */
	colortable = &Machine->pens[(turbo_fbcol & 6) << 6];

	/* loop over rows */
	for (y = 4; y < VIEW_HEIGHT - 4; y++, base += dy)
	{
		int sel, coch, babit, slipar_acciar, area, area1, area2, area3, area4, area5, road = 0;
		UINT32 *sprite_data = sprite_buffer;
		TYPE *dest = base;

		/* compute the Y sum between opa and the current scanline (p. 141) */
		int va = (y + turbo_opa) & 0xff;

		/* the upper bit of OPC inverts the road */
		if (!(turbo_opc & 0x80)) va ^= 0xff;

		/* clear the sprite buffer and draw the road sprites */
		memset(sprite_buffer, 0, VIEW_WIDTH * sizeof(UINT32));
		draw_road_sprites(sprite_buffer, y);

		/* loop over 8-pixel chunks */
		dest += dx * 8;
		sprite_data += 8;
		for (x = 8; x < VIEW_WIDTH; x += 8)
		{
			int area5_buffer = road_gfxdata_base[0x4000 + (x >> 3)];
			UINT8 back_data = videoram[(y / 8) * 32 + (x / 8) - 33];
			UINT16 backbits_buffer = back_expanded_data[(back_data << 3) | (y & 7)];

			/* loop over columns */
			for (i = 0; i < 8; i++, dest += dx)
			{
				UINT32 sprite = *sprite_data++;

				/* compute the X sum between opb and the current column; only the carry matters (p. 141) */
				int carry = (x + i + turbo_opb) >> 8;

				/* the carry selects which inputs to use (p. 141) */
				if (carry)
				{
					sel	 = turbo_ipb;
					coch = turbo_ipc >> 4;
				}
				else
				{
					sel	 = turbo_ipa;
					coch = turbo_ipc & 15;
				}

				/* at this point we also compute area5 (p. 141) */
				area5 = (area5_buffer >> 3) & 0x10;
				area5_buffer <<= 1;

				/* now look up the rest of the road bits (p. 142) */
				area1 = road_gfxdata[0x0000 | ((sel & 15) << 8) | va];
				area1 = ((area1 + x + i) >> 8) & 0x01;
				area2 = road_gfxdata[0x1000 | ((sel & 15) << 8) | va];
				area2 = ((area2 + x + i) >> 7) & 0x02;
				area3 = road_gfxdata[0x2000 | ((sel >> 4) << 8) | va];
				area3 = ((area3 + x + i) >> 6) & 0x04;
				area4 = road_gfxdata[0x3000 | ((sel >> 4) << 8) | va];
				area4 = ((area4 + x + i) >> 5) & 0x08;

				/* compute the final area value and look it up in IC18/PR1115 (p. 144) */
				area = area5 | area4 | area3 | area2 | area1;
				babit = road_enable_collide[area] & 0x07;

				/* note: SLIPAR is 0 on the road surface only */
				/*		 ACCIAR is 0 on the road surface and the striped edges only */
				slipar_acciar = road_enable_collide[area] & 0x30;
				if (!road && (slipar_acciar & 0x20))
				{
					road = 1;
					draw_offroad_sprites(sprite_buffer, x + i + 2, y);
				}

				/* perform collision detection here */
				turbo_collision |= collision_map[((sprite >> 24) & 7) | (slipar_acciar >> 1)];

				/* we only need to continue if we're actually drawing */
				if (FULL_DRAW)
				{
					int bacol, red, grn, blu, priority, backbits, mx;

					/* also use the coch value to look up color info in IC13/PR1114 and IC21/PR1117 (p. 144) */
					bacol = road_palette_base[coch & 15];

					/* at this point, do the character lookup */
					backbits = backbits_buffer & 3;
					backbits_buffer >>= 2;
					backbits = back_palette[backbits | (back_data & 0xfc)];

					/* look up the sprite priority in IC11/PR1122 */
					priority = sprite_priority_base[sprite >> 25];

					/* use that to look up the overall priority in IC12/PR1123 */
					mx = overall_priority_base[(priority & 7) | ((sprite >> 21) & 8) | ((back_data >> 3) & 0x10) | ((backbits << 2) & 0x20) | (babit << 6)];

					/* the input colors consist of a mix of sprite, road and 1's & 0's */
					red = 0x040000 | ((bacol & 0x001f) << 13) | ((backbits & 1) << 12) | ((sprite <<  4) & 0x0ff0);
					grn = 0x080000 | ((bacol & 0x03e0) <<  9) | ((backbits & 2) << 12) | ((sprite >>  3) & 0x1fe0);
					blu = 0x100000 | ((bacol & 0x7c00) <<  5) | ((backbits & 4) << 12) | ((sprite >> 10) & 0x3fc0);

					/* we then go through a muxer; normally these values are inverted, but */
					/* we've already taken care of that when we generated the palette */
					red = (red >> mx) & 0x10;
					grn = (grn >> mx) & 0x20;
					blu = (blu >> mx) & 0x40;
					*dest = colortable[mx | red | grn | blu];
				}
			}
		}
	}
}

#endif
