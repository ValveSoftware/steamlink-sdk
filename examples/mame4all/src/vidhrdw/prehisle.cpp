/***************************************************************************

	Prehistoric Isle video routines

	Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static struct osd_bitmap *pf1_bitmap,*pf2_bitmap;
unsigned char *prehisle_video;
static int vid_control[32],dirty_back,dirty_front;

/******************************************************************************/

void prehisle_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int offs,mx,my,color,tile,i;
	int colmask[0x80],code,pal_base,tile_base;
	int scrollx,scrolly;
	unsigned char *tilemap = memory_region(REGION_GFX5);
	static int old_base=0xfffff,old_front=0xfffff;

	/* Build the dynamic palette */
	palette_init_used_colors();

	/* Text layer */
	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs <0x800;offs += 2)
	{
		code = READ_WORD(&videoram[offs]);
		color=code>>12;
		if (code==0xff20) continue;
		colmask[color] |= Machine->gfx[0]->pen_usage[code&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	/* Tiles - bottom layer */
	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (offs=0; offs<256; offs++)
		palette_used_colors[pal_base + offs] = PALETTE_COLOR_USED;

	/* Tiles - top layer */
	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0x0000;offs <0x4000;offs += 2 )
	{
		code = READ_WORD(&prehisle_video[offs]);
		color= code>>12;
		colmask[color] |= Machine->gfx[2]->pen_usage[code&0x7ff];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}

///* kludge */
palette_used_colors[pal_base + 16 * color +15] = PALETTE_COLOR_TRANSPARENT;
palette_change_color(pal_base + 16 * color +15 ,0,0,0);

	}

	/* Sprites */
	pal_base = Machine->drv->gfxdecodeinfo[3].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs <0x400;offs += 8 )
	{
		code = READ_WORD(&spriteram[offs+4])&0x1fff;
		color= READ_WORD(&spriteram[offs+6])>>12;
		if (code>0x13ff) code=0x13ff;
		colmask[color] |= Machine->gfx[3]->pen_usage[code];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc()) {
		dirty_back=1;
		dirty_front=1;
	}

	/* Calculate tilebase for background, 64 bytes per column */
	tile_base=((READ_WORD(&vid_control[6])&0x3ff0)>>4)*64;
	if (old_base!=tile_base) dirty_back=1; /* Redraw */
	old_base=tile_base;

	/* Back layer, taken from tilemap rom */
	if (dirty_back)
	{
		tile_base&=0xfffe; /* Safety */
		dirty_back=0;

		for (mx=0; mx<17;mx++)
		{
			for (my=0; my<32;my++)
			{
				tile = tilemap[1+tile_base] + (tilemap[tile_base]<<8);
				color = tile>>12;
				drawgfx(pf1_bitmap,Machine->gfx[1],
		                    (tile & 0x7ff) | 0x800,
		                    color,
		                    tile&0x800,0,
		                    16*mx,16*my,
		                    0,TRANSPARENCY_NONE,0);
				tile_base+=2;
				if (tile_base==0x10000) tile_base=0; /* Wraparound */
			}
		}
	}

	scrollx=-(READ_WORD(&vid_control[6])&0xf);
	scrolly=-READ_WORD(&vid_control[4]);
	copyscrollbitmap(bitmap,pf1_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Calculate tilebase for background, 64 bytes per column */
	tile_base=((READ_WORD(&vid_control[2])&0xff0)>>4)*64;
	if (old_front!=tile_base) dirty_front=1; /* Redraw */
	old_front=tile_base;

	/* Back layer, taken from tilemap rom */
	if (dirty_front)
	{
		tile_base&=0x3ffe; /* Safety */
		dirty_front=0;

		for (mx=0; mx<17;mx++)
		{
			for (my=0; my<32;my++)
			{
				tile = READ_WORD(&prehisle_video[tile_base]);
				color = tile>>12;
				drawgfx(pf2_bitmap,Machine->gfx[2],
		                    tile & 0x7ff,
		                    color,
		                    0,tile&0x800,
		                    16*mx,16*my,
		                    0,TRANSPARENCY_NONE,0);
				tile_base+=2;
				if (tile_base==0x4000) tile_base=0; /* Wraparound */
			}
		}
	}

	scrollx=-(READ_WORD(&vid_control[2])&0xf);
	scrolly=-READ_WORD(&vid_control[0]);
	copyscrollbitmap(bitmap,pf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);

	/* Sprites */
	for (offs = 0;offs <0x800 ;offs += 8) {
		int x,y,sprite,colour,fx,fy;

	    y=READ_WORD (&spriteram[offs+0]);
		if (y>254) continue; /* Speedup */
	    x=READ_WORD (&spriteram[offs+2]);
		if (x&0x200) x=-(0xff-(x&0xff));
		if (x>256) continue; /* Speedup */

	    sprite=READ_WORD (&spriteram[offs+4]);
	    colour=READ_WORD (&spriteram[offs+6])>>12;

		fy=sprite&0x8000;
		fx=sprite&0x4000;

	    sprite=sprite&0x1fff;

		if (sprite>0x13ff) sprite=0x13ff;

		drawgfx(bitmap,Machine->gfx[3],
				sprite,
				colour,fx,fy,x,y,
				&Machine->visible_area,TRANSPARENCY_PEN,15);
	}

	/* Text layer */
	mx = -1;
	my = 0;
	for (offs = 0x000; offs < 0x800;offs += 2)
	{
		mx++;
		if (mx == 32)
		{
			mx = 0;
			my++;
		}
		tile = READ_WORD(&videoram[offs]);
		color = tile>>12;
		if ((tile&0xff)!=0x20)
			drawgfx(bitmap,Machine->gfx[0],
					tile & 0xfff,
					color,
					0,0,
					8*mx,8*my,
					&Machine->visible_area,TRANSPARENCY_PEN,15);
    }
}

/******************************************************************************/

int prehisle_vh_start (void)
{
	pf1_bitmap=bitmap_alloc(256+16,512);
	pf2_bitmap=bitmap_alloc(256+16,512);
	return 0;
}

void prehisle_vh_stop (void)
{
	bitmap_free(pf1_bitmap);
	bitmap_free(pf2_bitmap);
}

WRITE_HANDLER( prehisle_video_w )
{
	int oldword = READ_WORD(&prehisle_video[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&prehisle_video[offset],newword);
		dirty_front=1; /* Redraw */
	}
}

READ_HANDLER( prehisle_video_r )
{
	return READ_WORD(&prehisle_video[offset]);
}

static int controls_invert;

READ_HANDLER( prehisle_control_r )
{
	switch (offset) {
		case 0x10: /* Player 2 */
			return readinputport(1);

		case 0x20: /* Coins, tilt, service */
			return readinputport(2);

		case 0x40: /* Player 1 */
			return readinputport(0) ^ controls_invert;

		case 0x42: /* Dips */
			return readinputport(3);

		case 0x44: /* Dips + VBL */
			return readinputport(4);

		default:
			//logerror("%06x: read unknown control %02x\n",cpu_get_pc(),offset);
			return 0;
	}
}

WRITE_HANDLER( prehisle_control_w )
{
	switch (offset) {
		case 0:
			WRITE_WORD(&vid_control[0],data);
			break;
		case 0x10:
			WRITE_WORD(&vid_control[2],data);
			break;

		case 0x20:
			WRITE_WORD(&vid_control[4],data);
			break;

		case 0x30:
			WRITE_WORD(&vid_control[6],data);
			break;

		case 0x46:
			controls_invert = data ? 0xff : 0x00;
			break;

		case 0x50:
			WRITE_WORD(&vid_control[8],data);
			break;

		case 0x52:
			WRITE_WORD(&vid_control[10],data);
			break;
		case 0x60:
			WRITE_WORD(&vid_control[12],data);
			break;

		default:
			//logerror("%06x: write unknown control %02x\n",cpu_get_pc(),offset);
			break;
	}
}
