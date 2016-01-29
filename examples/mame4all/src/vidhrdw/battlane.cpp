/***************************************************************************

	Battlelane

    TODO: Properly support flip screen
          Tidy / Optimize and add dirty layer support

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static struct osd_bitmap *screen_bitmap;

size_t battlane_bitmap_size;
unsigned char *battlane_bitmap;
static int battlane_video_ctrl;

const int battlane_spriteram_size=0x100;
static unsigned char battlane_spriteram[0x100];

const int battlane_tileram_size=0x800;
static unsigned char battlane_tileram[0x800];


static int flipscreen;
static int battlane_scrolly;
static int battlane_scrollx;

extern int battlane_cpu_control;

static struct osd_bitmap *bkgnd_bitmap;  /* scroll bitmap */


WRITE_HANDLER( battlane_video_ctrl_w )
{
	/*
    Video control register

        0x80    = ????
        0x0e    = Bitmap plane (bank?) select  (0-7)
        0x01    = Scroll MSB
	*/

	battlane_video_ctrl=data;
}

READ_HANDLER( battlane_video_ctrl_r )
{
	return battlane_video_ctrl;
}

void battlane_set_video_flip(int flip)
{

    if (flip != flipscreen)
    {
        // Invalidate any cached data
    }

    flipscreen=flip;

    /*
    Don't flip the screen. The render function doesn't support
    it properly yet.
    */
    flipscreen=0;

}

WRITE_HANDLER( battlane_scrollx_w )
{
    battlane_scrollx=data;
}

WRITE_HANDLER( battlane_scrolly_w )
{
    battlane_scrolly=data;
}

WRITE_HANDLER( battlane_tileram_w )
{
    battlane_tileram[offset]=data;
}

READ_HANDLER( battlane_tileram_r )
{
    return battlane_tileram[offset];
}

WRITE_HANDLER( battlane_spriteram_w )
{
    battlane_spriteram[offset]=data;
}

READ_HANDLER( battlane_spriteram_r )
{
    return battlane_spriteram[offset];
}


WRITE_HANDLER( battlane_bitmap_w )
{
	int i, orval;

    orval=(~battlane_video_ctrl>>1)&0x07;

	if (orval==0)
		orval=7;

	for (i=0; i<8; i++)
	{
		if (data & 1<<i)
		{
		    screen_bitmap->line[(offset / 0x100) * 8+i][(0x2000-offset) % 0x100]|=orval;
		}
		else
		{
		    screen_bitmap->line[(offset / 0x100) * 8+i][(0x2000-offset) % 0x100]&=~orval;
		}
	}
	battlane_bitmap[offset]=data;
}

READ_HANDLER( battlane_bitmap_r )
{
	return battlane_bitmap[offset];
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int battlane_vh_start(void)
{
	screen_bitmap = bitmap_alloc(0x20*8, 0x20*8);
	if (!screen_bitmap)
	{
		return 1;
	}

	battlane_bitmap=(unsigned char*)malloc(battlane_bitmap_size);
	if (!battlane_bitmap)
	{
		return 1;
	}

	memset(battlane_spriteram, 0, battlane_spriteram_size);
    memset(battlane_tileram,255, battlane_tileram_size);

    bkgnd_bitmap = bitmap_alloc(0x0200, 0x0200);
    if (!bkgnd_bitmap)
	{
		return 1;
	}


	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void battlane_vh_stop(void)
{
	if (screen_bitmap)
	{
		bitmap_free(screen_bitmap);
	}
	if (battlane_bitmap)
	{
		free(battlane_bitmap);
	}
    if (bkgnd_bitmap)
    {
        free(bkgnd_bitmap);
    }
}

/***************************************************************************

  Build palette from palette RAM

***************************************************************************/

INLINE void battlane_build_palette(void)
{
	int offset;
    unsigned char *PALETTE =
        memory_region(REGION_PROMS);

    for (offset = 0; offset < 0x40; offset++)
	{
          int palette = PALETTE[offset];
          int red, green, blue;

          blue   = ((palette>>6)&0x03) * 16*4;
          green  = ((palette>>3)&0x07) * 16*2;
          red    = ((palette>>0)&0x07) * 16*2;

          palette_change_color (offset, red, green, blue);
	}
}

/*

void battlane_vh_convert_color_prom (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{

}
*/

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void battlane_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
    int scrollx,scrolly;
	int x,y, offs;

    /* Scroll registers */
    scrolly=256*(battlane_video_ctrl&0x01)+battlane_scrolly;
    scrollx=256*(battlane_cpu_control&0x01)+battlane_scrollx;


    battlane_build_palette();
	if (palette_recalc ())
    {
         // Mark cached layer as dirty
    }

    /* Draw tile map. TODO: Cache it */
    for (offs=0; offs <0x400;  offs++)
    {
        int sx,sy;
        int code=battlane_tileram[offs];
        int attr=battlane_tileram[0x400+offs];

        sx=(offs&0x0f)+(offs&0x100)/16;
        sy=((offs&0x200)/2+(offs&0x0f0))/16;
        drawgfx(bkgnd_bitmap,Machine->gfx[1+(attr&0x01)],
               code,
               (attr>>1)&0x07,
               !flipscreen,flipscreen,
               sx*16,sy*16,
               NULL,
               TRANSPARENCY_NONE, 0);

    }
    /* copy the background graphics */
    {
		int scrlx, scrly;
        scrlx=-scrollx;
        scrly=-scrolly;
        copyscrollbitmap(bitmap,bkgnd_bitmap,1,&scrly,1,&scrlx,&Machine->visible_area,TRANSPARENCY_NONE,0);
    }
    {
    char baf[256];
    char baf2[40];
    baf[0]=0;

    /* Draw sprites */
    for (offs=0; offs<0x0100; offs+=4)
	{
           /*
           0x80=bank 2
           0x40=
           0x20=bank 1
           0x10=y double
           0x08=Unknown - all vehicles have this bit clear
           0x04=x flip
           0x02=y flip
           0x01=Sprite enable
           */
          int attr=battlane_spriteram[offs+1];
          int code=battlane_spriteram[offs+3];
          code += 256*((attr>>6) & 0x02);
          code += 256*((attr>>5) & 0x01);
          if (offs > 0x00a0)
          {
               sprintf(baf2, "%02x ", attr);
               strcat(baf,baf2);
          }

          if (attr & 0x01)
	      {
               int sx=battlane_spriteram[offs+2];
               int sy=battlane_spriteram[offs];
               int flipx=attr&0x04;
               int flipy=attr&0x02;
               if (!flipscreen)
               {
                    sx=240-sx;
                    sy=240-sy;
                    flipy=!flipy;
                    flipx=!flipx;
               }
               if ( attr & 0x10)  /* Double Y direction */
               {
                   int dy=16;
                   if (flipy)
                   {
                        dy=-16;
                   }
                   drawgfx(bitmap,Machine->gfx[0],
                     code,
                     0,
                     flipx,flipy,
                     sx, sy,
					 &Machine->visible_area,
                     TRANSPARENCY_PEN, 0);

                    drawgfx(bitmap,Machine->gfx[0],
                     code+1,
                     0,
                     flipx,flipy,
                     sx, sy-dy,
					 &Machine->visible_area,
                     TRANSPARENCY_PEN, 0);
                }
                else
                {
                   drawgfx(bitmap,Machine->gfx[0],
					 code,
                     0,
                     flipx,flipy,
                     sx, sy,
					 &Machine->visible_area,
                     TRANSPARENCY_PEN, 0);
                }
          }
	}


//    usrintf_showmessage(baf);
    }
    /* Draw foreground bitmap */
    if (flipscreen)
    {
        for (y=0; y<0x20*8; y++)
        {
            for (x=0; x<0x20*8; x++)
            {
                int data=screen_bitmap->line[y][x];
                if (data)
                {
                    bitmap->line[255-y][255-x]=Machine->pens[data];
                }
            }
        }
    }
    else
    {
        for (y=0; y<0x20*8; y++)
        {
            for (x=0; x<0x20*8; x++)
            {
                int data=screen_bitmap->line[y][x];
                if (data)
                {
                    bitmap->line[y][x]=Machine->pens[data];
                }
            }
        }

    }
}
