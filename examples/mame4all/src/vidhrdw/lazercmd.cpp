/*************************************************************/
/*                                                           */
/* Lazer Command video handler                               */
/*                                                           */
/*************************************************************/

#include "driver.h"
#include "artwork.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/lazercmd.h"

extern int marker_x, marker_y;

static int video_inverted = 0;


#define JADE	0x20,0xb0,0x20,OVERLAY_DEFAULT_OPACITY
#define MUSTARD 0xb0,0x80,0x20,OVERLAY_DEFAULT_OPACITY

#define	END  {{ -1, -1, -1, -1}, 0,0,0,0}

static const struct artwork_element overlay[]=
{
	{{  0*HORZ_CHR, 16*HORZ_CHR-1,  0*VERT_CHR, 1*VERT_CHR-1 }, MUSTARD },
	{{ 16*HORZ_CHR, 32*HORZ_CHR-1,  0*VERT_CHR, 1*VERT_CHR-1 }, JADE    },
	{{  0*HORZ_CHR, 16*HORZ_CHR-1,  1*VERT_CHR,22*VERT_CHR-1 }, JADE    },
	{{ 16*HORZ_CHR, 32*HORZ_CHR-1,  1*VERT_CHR,22*VERT_CHR-1 }, MUSTARD },
	{{  0*HORZ_CHR, 16*HORZ_CHR-1, 22*VERT_CHR,23*VERT_CHR-1 }, MUSTARD },
	{{ 16*HORZ_CHR, 32*HORZ_CHR-1, 22*VERT_CHR,23*VERT_CHR-1 }, JADE    },
	END
};

/* scale a markers vertical position */
/* the following table shows how the markers */
/* vertical position worked in hardware  */
/*  marker_y  lines    marker_y  lines   */
/*     0      0 + 1       8      10 + 11 */
/*     1      2 + 3       9      12 + 13 */
/*     2      4 + 5      10      14 + 15 */
/*     3      6 + 7      11      16 + 17 */
/*     4      8 + 9      12      18 + 19 */
static int vert_scale(int data)
{
	return ((data & 0x07)<<1) + ((data & 0xf8)>>3) * VERT_CHR;
}

/* mark the character occupied by the marker dirty */
void lazercmd_marker_dirty(int marker)
{
	int x, y;

	x = marker_x - 1;             /* normal video lags marker by 1 pixel */
	y = vert_scale(marker_y) - VERT_CHR; /* first line used as scratch pad */

	if (x < 0 || x >= HORZ_RES * HORZ_CHR)
		return;

	if (y < 0 || y >= (VERT_RES - 1) * VERT_CHR)
        return;

	/* mark all occupied character positions dirty */
    dirtybuffer[(y+0)/VERT_CHR * HORZ_RES + (x+0)/HORZ_CHR] = 1;
	dirtybuffer[(y+3)/VERT_CHR * HORZ_RES + (x+0)/HORZ_CHR] = 1;
	dirtybuffer[(y+0)/VERT_CHR * HORZ_RES + (x+3)/HORZ_CHR] = 1;
	dirtybuffer[(y+3)/VERT_CHR * HORZ_RES + (x+3)/HORZ_CHR] = 1;
}


/* plot a bitmap marker */
/* hardware has 2 marker sizes 2x2 and 4x2 selected by jumper */
/* meadows lanes normaly use 2x2 pixels and lazer command uses either */
static void plot_pattern(struct osd_bitmap *bitmap, int x, int y)
{
	int xbit, ybit, size;

    size = 2;
	if (input_port_2_r(0) & 0x40)
    {
		size = 4;
    }

	for (ybit = 0; ybit < 2; ybit++)
	{
	    if (y+ybit < 0 || y+ybit >= VERT_RES * VERT_CHR)
		    return;

	    for (xbit = 0; xbit < size; xbit++)
		{
			if (x+xbit < 0 || x+xbit >= HORZ_RES * HORZ_CHR)
				continue;

			plot_pixel(bitmap, x+xbit, y+ybit, Machine->pens[2]);
		}
	}
}


int lazercmd_vh_start(void)
{
	if( generic_vh_start() )
		return 1;

	/* is overlay enabled? */

	if (input_port_2_r(0) & 0x80)
	{
		overlay_create(overlay, 3, Machine->drv->total_colors-3);
	}

	return 0;
}


void lazercmd_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i,x,y;

	if (video_inverted != (input_port_2_r(0) & 0x20))
	{
		video_inverted = input_port_2_r(0) & 0x20;
		memset(dirtybuffer, 1, videoram_size);
	}

	if (palette_recalc() || full_refresh)
        memset(dirtybuffer, 1, videoram_size);

	/* The first row of characters are invisible */
	for (i = 0; i < (VERT_RES - 1) * HORZ_RES; i++)
	{
		if (dirtybuffer[i])
		{
			int sx,sy;

			dirtybuffer[i] = 0;

			sx = i % HORZ_RES;
			sy = i / HORZ_RES;

			sx *= HORZ_CHR;
			sy *= VERT_CHR;

			drawgfx(bitmap, Machine->gfx[0],
					videoram[i], video_inverted ? 1 : 0,
					0,0,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	x = marker_x - 1;             /* normal video lags marker by 1 pixel */
	y = vert_scale(marker_y) - VERT_CHR; /* first line used as scratch pad */
	plot_pattern(bitmap,x,y);
}
