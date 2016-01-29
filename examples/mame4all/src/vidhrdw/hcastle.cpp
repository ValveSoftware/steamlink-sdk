/***************************************************************************

	Haunted Castle video emulation

***************************************************************************/

#include "driver.h"
#include "vidhrdw/konamiic.h"
#include "vidhrdw/generic.h"

static struct osd_bitmap *pf1_bitmap;
static struct osd_bitmap *pf2_bitmap;
static unsigned char *dirty_pf1,*dirty_pf2;
unsigned char *hcastle_pf1_videoram,*hcastle_pf2_videoram;
static int gfx_bank;



void hcastle_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,chip,pal,clut;

	for (chip = 0;chip < 2;chip++)
	{
		for (pal = 0;pal < 8;pal++)
		{
			clut = (pal & 1) + 2 * chip;
			for (i = 0;i < 256;i++)
			{
				if ((pal & 1) == 0)	/* sprites */
				{
					if (color_prom[256 * clut + i] == 0)
						*(colortable++) = 0;
					else
						*(colortable++) = 16 * pal + color_prom[256 * clut + i];
				}
				else
					*(colortable++) = 16 * pal + color_prom[256 * clut + i];
			}
		}
	}
}



/*****************************************************************************/

int hcastle_vh_start(void)
{
 	if ((pf1_bitmap = bitmap_alloc(64*8,32*8)) == 0)
		return 1;
	if ((pf2_bitmap = bitmap_alloc(64*8,32*8)) == 0)
		return 1;

	dirty_pf1=(unsigned char*)malloc(0x1000);
	dirty_pf2=(unsigned char*)malloc(0x1000);
	memset(dirty_pf1,1,0x1000);
	memset(dirty_pf2,1,0x1000);

	return 0;
}

void hcastle_vh_stop(void)
{
	bitmap_free(pf1_bitmap);
	bitmap_free(pf2_bitmap);
	free(dirty_pf1);
	free(dirty_pf2);
}



WRITE_HANDLER( hcastle_pf1_video_w )
{
	hcastle_pf1_videoram[offset]=data;
	dirty_pf1[offset]=1;
}

WRITE_HANDLER( hcastle_pf2_video_w )
{
	hcastle_pf2_videoram[offset]=data;
	dirty_pf2[offset]=1;
}

WRITE_HANDLER( hcastle_gfxbank_w )
{
	gfx_bank = data;
}

READ_HANDLER( hcastle_gfxbank_r )
{
	return gfx_bank;
}

WRITE_HANDLER( hcastle_pf1_control_w )
{
	if (offset==3)
	{
		if ((data&0x8)==0)
			buffer_spriteram(spriteram+0x800,0x800);
		else
			buffer_spriteram(spriteram,0x800);
	}
	K007121_ctrl_0_w(offset,data);
}

WRITE_HANDLER( hcastle_pf2_control_w )
{
	if (offset==3)
	{
		if ((data&0x8)==0)
			buffer_spriteram_2(spriteram_2+0x800,0x800);
		else
			buffer_spriteram_2(spriteram_2,0x800);
	}
	K007121_ctrl_1_w(offset,data);
}

/*****************************************************************************/

static void draw_sprites( struct osd_bitmap *bitmap, unsigned char *sbank, int bank )
{
	int bank_base = (bank == 0) ? 0x4000 * (gfx_bank & 1) : 0;
	K007121_sprites_draw(bank,bitmap,sbank,(K007121_ctrlram[bank][6]&0x30)*2,0,bank_base,-1);
}

/*****************************************************************************/

void hcastle_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,tile,color,mx,my,bank,scrollx,scrolly;
	int pf2_bankbase,pf1_bankbase,attr;
	int bit0,bit1,bit2,bit3;
	static int old_pf1,old_pf2;


	palette_init_used_colors();
	memset(palette_used_colors,PALETTE_COLOR_USED,128);
	palette_used_colors[0*16] = PALETTE_COLOR_TRANSPARENT;
	palette_used_colors[1*16] = PALETTE_COLOR_TRANSPARENT;
	palette_used_colors[2*16] = PALETTE_COLOR_TRANSPARENT;
	palette_used_colors[3*16] = PALETTE_COLOR_TRANSPARENT;

	pf1_bankbase = 0x0000;
	pf2_bankbase = 0x4000 * ((gfx_bank & 2) >> 1);

	if (K007121_ctrlram[0][3] & 0x01) pf1_bankbase += 0x2000;
	if (K007121_ctrlram[1][3] & 0x01) pf2_bankbase += 0x2000;

	if (palette_recalc() || pf1_bankbase!=old_pf1 || pf2_bankbase!=old_pf2)
	{
		memset(dirty_pf1,1,0x1000);
		memset(dirty_pf2,1,0x1000);
	}
	old_pf1=pf1_bankbase;
	old_pf2=pf2_bankbase;

	/* Draw foreground */
	bit0 = (K007121_ctrlram[0][0x05] >> 0) & 0x03;
	bit1 = (K007121_ctrlram[0][0x05] >> 2) & 0x03;
	bit2 = (K007121_ctrlram[0][0x05] >> 4) & 0x03;
	bit3 = (K007121_ctrlram[0][0x05] >> 6) & 0x03;
	for (my = 0;my < 32;my++)
	{
		for (mx = 0;mx < 64;mx++)
		{
			if (mx >= 32)
				offs = 0x800 + my*32 + (mx-32);
			else
				offs = my*32 + mx;

			if (!dirty_pf1[offs] && !dirty_pf1[offs+0x400]) continue;
			dirty_pf1[offs]=dirty_pf1[offs+0x400]=0;

			tile = hcastle_pf1_videoram[offs+0x400];
			attr = hcastle_pf1_videoram[offs];
			color = attr & 0x7;
			bank = ((attr & 0x80) >> 7) |
					((attr >> (bit0+2)) & 0x02) |
					((attr >> (bit1+1)) & 0x04) |
					((attr >> (bit2  )) & 0x08) |
					((attr >> (bit3-1)) & 0x10);

			drawgfx(pf1_bitmap,Machine->gfx[0],
					tile+bank*0x100+pf1_bankbase,
					((K007121_ctrlram[0][6]&0x30)*2+16)+color,
					0,0,
					8*mx,8*my,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* Draw background */
	bit0 = (K007121_ctrlram[1][0x05] >> 0) & 0x03;
	bit1 = (K007121_ctrlram[1][0x05] >> 2) & 0x03;
	bit2 = (K007121_ctrlram[1][0x05] >> 4) & 0x03;
	bit3 = (K007121_ctrlram[1][0x05] >> 6) & 0x03;
	for (my = 0;my < 32;my++)
	{
		for (mx = 0;mx < 64;mx++)
		{
			if (mx >= 32)
				offs = 0x800 + my*32 + (mx-32);
			else
				offs = my*32 + mx;

			if (!dirty_pf2[offs] && !dirty_pf2[offs+0x400]) continue;
			dirty_pf2[offs]=dirty_pf2[offs+0x400]=0;

			tile = hcastle_pf2_videoram[offs+0x400];
			attr = hcastle_pf2_videoram[offs];
			color = attr & 0x7;
			bank = ((attr & 0x80) >> 7) |
					((attr >> (bit0+2)) & 0x02) |
					((attr >> (bit1+1)) & 0x04) |
					((attr >> (bit2  )) & 0x08) |
					((attr >> (bit3-1)) & 0x10);

			drawgfx(pf2_bitmap,Machine->gfx[1],
					tile+bank*0x100+pf2_bankbase,
					((K007121_ctrlram[1][6]&0x30)*2+16)+color,
					0,0,
					8*mx,8*my,
					0,TRANSPARENCY_NONE,0);
		}
	}


//	/* Sprite priority */
//	if (K007121_ctrlram[0][3]&0x20)
	if ((gfx_bank & 0x04) == 0)
	{
		scrolly = -K007121_ctrlram[1][2];
		scrollx = -((K007121_ctrlram[1][1]<<8)+K007121_ctrlram[1][0]);
		copyscrollbitmap(bitmap,pf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

		draw_sprites( bitmap, buffered_spriteram, 0 );
		draw_sprites( bitmap, buffered_spriteram_2, 1 );

		scrolly = -K007121_ctrlram[0][2];
		scrollx = -((K007121_ctrlram[0][1]<<8)+K007121_ctrlram[0][0]);
		copyscrollbitmap(bitmap,pf1_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
	}
	else
	{
		scrolly = -K007121_ctrlram[1][2];
		scrollx = -((K007121_ctrlram[1][1]<<8)+K007121_ctrlram[1][0]);
		copyscrollbitmap(bitmap,pf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

		scrolly = -K007121_ctrlram[0][2];
		scrollx = -((K007121_ctrlram[0][1]<<8)+K007121_ctrlram[0][0]);
		copyscrollbitmap(bitmap,pf1_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);

		draw_sprites( bitmap, buffered_spriteram, 0 );
		draw_sprites( bitmap, buffered_spriteram_2, 1 );
	}
}
