/***************************************************************************

  vidhrdw.c

  Written by Kenneth Lin (kenneth_lin@ai.vancouver.bc.ca)

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *jackal_scrollram,*jackal_videoctrl;



void jackal_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		COLOR(1,i) = (*color_prom & 0x0f);
		color_prom++;
	}
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		COLOR(2,i) = (*color_prom & 0x0f) + 16;
		color_prom++;
	}
}



int jackal_vh_start(void)
{
	videoram_size = 0x400;

	dirtybuffer = 0;
	tmpbitmap = 0;

	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
	{
		return 1;
	}
	memset(dirtybuffer,1,videoram_size);
	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}
	return 0;
}


void jackal_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);

	dirtybuffer = 0;
	tmpbitmap = 0;
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void jackal_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	unsigned char *sr, *ss;
	int offs,i;
	unsigned char *RAM = (memory_region(REGION_CPU1));


	if (palette_recalc())
	{
		memset(dirtybuffer,1,videoram_size);
	}

	jackal_scrollram = &RAM[0x0020];
	colorram = &RAM[0x2000];
	videoram = &RAM[0x2400];

	spriteram_size = 0x500;

	if (jackal_videoctrl[0x03] & 0x08)
	{
		sr = &RAM[0x03800];	// Sprite 2
		ss = &RAM[0x13800];	// Additional Sprite 2
	}
	else
	{
		sr = &RAM[0x03000];	// Sprite 1
		ss = &RAM[0x13000];	// Additional Sprite 1
	}

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;

			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(tmpbitmap,Machine->gfx[0],
				videoram[offs] + ((colorram[offs] & 0xc0) << 2) + ((colorram[offs] & 0x30) << 6),
				0,//colorram[offs] & 0x0f, there must be a PROM like in Contra
				colorram[offs] & 0x10,colorram[offs] & 0x20,
				8*sx,8*sy,
				0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int h_scroll_num = 0, v_scroll_num = 0;
		int h_scroll[32], v_scroll[32];

		if (jackal_videoctrl[2] & 0x08)
		{
			h_scroll_num = 32;
			for (i = 0;i < 32;i++)
				h_scroll[i] = -(jackal_scrollram[i]);
		}

		if (jackal_videoctrl[2] & 0x04)
		{
			v_scroll_num = 32;
			for (i = 0;i < 32;i++)
				v_scroll[i] = -(jackal_scrollram[i]);
		}

		if (jackal_videoctrl[0] != 0)
		{
			v_scroll_num = 1;
			v_scroll[0] = -(jackal_videoctrl[0]);
		}

		if (jackal_videoctrl[1] != 0)
		{
			h_scroll_num = 1;
			h_scroll[0] = -(jackal_videoctrl[1]);
		}

		if ((h_scroll_num == 0) && (v_scroll_num == 0))
			copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
		else
			copyscrollbitmap(bitmap,tmpbitmap,h_scroll_num,h_scroll,v_scroll_num,v_scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	/* Draw the sprites. */
	{
		unsigned char sr1, sr2, sr3, sr4, sr5;
		int spritenum, sx, sy, color;
		unsigned char sn1, sn2, sp, flipx, flipy;

		for ( offs = 0; offs < 0x0F5; /* offs += 5 */ )
		{
			sn1 = ss[offs++]; // offs+0
			sn2 = ss[offs++]; // offs+1
			sy  = ss[offs++]; // offs+2
			sx  = ss[offs++]; // offs+3
			sp  = ss[offs++]; // offs+4

			flipx = sp & 0x20;
			flipy = sp & 0x40;
			color = ((sn2 & 0xf0)>>4);

			if ( !(sp & 0xC) )
			{
				spritenum = sn1 + ((sn2 & 0x3) << 8);

				if (sy > 0xF0) sy = sy - 256;
				if (sp & 0x01) sx = sx - 256;

				if (sp & 0x10)
				{
					if ( (sx > -16) || (sx < 0xF0) )
					{
						drawgfx(bitmap,Machine->gfx[2],
							spritenum,
							color,
							flipx,flipy,
							flipx?sx+16:sx, flipy?sy+16:sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[2],
							spritenum+1,
							color,
							flipx,flipy,
							flipx?sx:sx+16, flipy?sy+16:sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[2],
							spritenum+2,
							color,
							flipx,flipy,
							flipx?sx+16:sx, flipy?sy:sy+16,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[2],
							spritenum+3,
							color,
							flipx,flipy,
							flipx?sx:sx+16, flipy?sy:sy+16,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
				}
				else
				{
					if ( (sx > -8) || (sx < 0xF0) )
					{
						drawgfx(bitmap,Machine->gfx[2],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
				}
			}
			else if ( (sx < 0xF0) && !(sp & 0x01) )
			{
				spritenum = sn1*4 + ((sn2 & (8+4)) >> 2) + ((sn2 & (2+1)) << 10);

				if ((sp & 0x0C) == 0x0C)
				{
					drawgfx(bitmap,Machine->gfx[4],
						spritenum,
						color,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
				if ((sp & 0x0C) == 0x08)
				{
					drawgfx(bitmap,Machine->gfx[4],
						spritenum,
						color,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
					drawgfx(bitmap,Machine->gfx[4],
						spritenum - 2,
						color,
						flipx,flipy,
						sx,sy+8,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
				if ((sp & 0x0C) == 0x04)
				{
					drawgfx(bitmap,Machine->gfx[4],
						spritenum,
						color,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
					drawgfx(bitmap,Machine->gfx[4],
						spritenum + 1,
						color,
						flipx,flipy,
						sx+8,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
			}
		}

		for (offs = 0; offs < 0x11D; offs += 5)
		{
			if ( (sr[offs+2] < 0xF0) && !(sr[offs+4] & 0x01) )
			{
				sr1 = sr[offs];
				sr2 = sr[offs+1];
				sr3 = sr[offs+2];
				sr4 = sr[offs+3];
				sr5 = sr[offs+4];

				sy = sr3;
				sx = sr4;

				flipx = sr5 & 0x20;
				flipy = sr5 & 0x40;
				color = ((sr2 & 0xf0)>>4);

				spritenum = sr1 + ((sr2 & 0x3) << 8);

				if (sr5 & 0xC)    /* half sized sprite */
				{

					spritenum = sr1*4 + ((sr2 & (8+4)) >> 2) + ((sr2 & (2+1)) << 10);

					if ((sr5 & 0x0C) == 0x0C)
					{
						drawgfx(bitmap,Machine->gfx[3],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
					if ((sr5 & 0x0C) == 0x08)
					{
						drawgfx(bitmap,Machine->gfx[3],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[3],
							spritenum - 2,
							color,
							flipx,flipy,
							sx,sy+8,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
					if ((sr5 & 0x0C) == 0x04)
					{
						drawgfx(bitmap,Machine->gfx[3],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[3],
							spritenum + 1,
							color,
							flipx,flipy,
							sx+8,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}

				}
				else
				{
					if (sr5 & 0x10)
					{
						drawgfx(bitmap,Machine->gfx[1],
							spritenum,
							color,
							flipx,flipy,
							flipx?sx+16:sx, flipy?sy+16:sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[1],
							spritenum+1,
							color,
							flipx,flipy,
							flipx?sx:sx+16, flipy?sy+16:sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[1],
							spritenum+2,
							color,
							flipx,flipy,
							flipx?sx+16:sx, flipy?sy:sy+16,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[1],
							spritenum+3,
							color,
							flipx,flipy,
							flipx?sx:sx+16, flipy?sy:sy+16,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
					else
						drawgfx(bitmap,Machine->gfx[1],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
			}
		}

		for (offs = 0x4F1; offs >= 0x11D; offs -= 5)
		{
			if ( (sr[offs+2] < 0xF0) && !(sr[offs+4] & 0x01) )
			{
				sr1 = sr[offs];
				sr2 = sr[offs+1];
				sr3 = sr[offs+2];
				sr4 = sr[offs+3];
				sr5 = sr[offs+4];

				sy = sr3;
				sx = sr4;

				flipx = sr5 & 0x20;
				flipy = sr5 & 0x40;
				color = ((sr2 & 0xf0)>>4);

				if (sr[offs+4] & 0xC)    /* half sized sprite */
				{

					spritenum = sr1*4 + ((sr2 & (8+4)) >> 2) + ((sr2 & (2+1)) << 10);

					if ((sr5 & 0x0C) == 0x0C)
					{
						drawgfx(bitmap,Machine->gfx[3],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
					if ((sr5 & 0x0C) == 0x08)
					{
						drawgfx(bitmap,Machine->gfx[3],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[3],
							spritenum - 2,
							color,
							flipx,flipy,
							sx,sy+8,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
					if ((sr5 & 0x0C) == 0x04)
					{
						drawgfx(bitmap,Machine->gfx[3],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[3],
							spritenum + 1,
							color,
							flipx,flipy,
							sx+8,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}

				}
				else
				{
					spritenum = sr1 + ((sr2 & 0x3) << 8);

					if (sr5 & 0x10)
					{
						drawgfx(bitmap,Machine->gfx[1],
							spritenum,
							color,
							flipx,flipy,
							flipx?sx+16:sx, flipy?sy+16:sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[1],
							spritenum+1,
							color,
							flipx,flipy,
							flipx?sx:sx+16, flipy?sy+16:sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[1],
							spritenum+2,
							color,
							flipx,flipy,
							flipx?sx+16:sx, flipy?sy:sy+16,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
						drawgfx(bitmap,Machine->gfx[1],
							spritenum+3,
							color,
							flipx,flipy,
							flipx?sx:sx+16, flipy?sy:sy+16,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
					else
						drawgfx(bitmap,Machine->gfx[1],
							spritenum,
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
			}
		}
	}
}
