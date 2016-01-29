/****************************************************************************
 *
 * geebee.c
 *
 * video driver
 * juergen buchmueller <pullmoll@t-online.de>, jan 2000
 *
 * TODO:
 * backdrop support for lamps? (player1, player2 and serve)
 * what is the counter output anyway?
 * add overlay colors for Navalone and Kaitei Takara Sagashi
 *
 ****************************************************************************/

#include "driver.h"
#include "artwork.h"
#include "vidhrdw/generic.h"

/* from machine/geebee.c */
extern int geebee_ball_h;
extern int geebee_ball_v;
extern int geebee_lamp1;
extern int geebee_lamp2;
extern int geebee_lamp3;
extern int geebee_counter;
extern int geebee_lock_out_coil;
extern int geebee_bgw;
extern int geebee_ball_on;
extern int geebee_inv;

static unsigned char palette[] =
{
	0x00,0x00,0x00, /* black */
	0xff,0xff,0xff, /* white */
	0x7f,0x7f,0x7f  /* grey  */
};

static unsigned short geebee_colortable[] =
{
	 0, 1,
	 0, 2,
	 1, 0,
	 2, 0
};

static unsigned short navalone_colortable[] =
{
	 0, 1,
	 0, 2,
	 0, 1,
	 0, 2
};


#define PINK1	0xa0,0x00,0xe0,OVERLAY_DEFAULT_OPACITY
#define PINK2 	0xe0,0x00,0xf0,OVERLAY_DEFAULT_OPACITY
#define ORANGE	0xff,0xd0,0x00,OVERLAY_DEFAULT_OPACITY
#define BLUE	0x00,0x00,0xff,OVERLAY_DEFAULT_OPACITY

#define	END  {{ -1, -1, -1, -1}, 0,0,0,0}

static const struct artwork_element geebee_overlay[]=
{
	{{  1*8,  4*8-1,    0,32*8-1 }, PINK2  },
	{{  4*8,  5*8-1,    0, 6*8-1 }, PINK1  },
	{{  4*8,  5*8-1, 26*8,32*8-1 }, PINK1  },
	{{  4*8,  5*8-1,  6*8,26*8-1 }, ORANGE },
	{{  5*8, 28*8-1,    0, 3*8-1 }, PINK1  },
	{{  5*8, 28*8-1, 29*8,32*8-1 }, PINK1  },
	{{  5*8, 28*8-1,  3*8, 6*8-1 }, BLUE   },
	{{  5*8, 28*8-1, 26*8,29*8-1 }, BLUE   },
	{{ 12*8, 13*8-1, 15*8,17*8-1 }, BLUE   },
	{{ 21*8, 23*8-1, 12*8,14*8-1 }, BLUE   },
	{{ 21*8, 23*8-1, 18*8,20*8-1 }, BLUE   },
	{{ 28*8, 29*8-1,    0,32*8-1 }, PINK2  },
	{{ 29*8, 32*8-1,    0,32*8-1 }, PINK1  },
	END
};

int geebee_vh_start(void)
{
	if( generic_vh_start() )
		return 1;

	/* use an overlay only in upright mode */

	if( (readinputport(2) & 0x01) == 0 )
	{
		overlay_create(geebee_overlay, 3, Machine->drv->total_colors-3);
	}

	return 0;
}

int navalone_vh_start(void)
{
	if( generic_vh_start() )
		return 1;

    /* overlay? */

	return 0;
}

int sos_vh_start(void)
{
	if( generic_vh_start() )
		return 1;

    /* overlay? */

	return 0;
}

int kaitei_vh_start(void)
{
	if( generic_vh_start() )
	return 1;

    /* overlay? */

	return 0;
}

/* Initialise the palette */
void geebee_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable, const unsigned char *color_prom)
{
	memcpy(sys_palette, palette, sizeof (palette));
	memcpy(sys_colortable, geebee_colortable, sizeof (geebee_colortable));
}

/* Initialise the palette */
void navalone_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable, const unsigned char *color_prom)
{
	memcpy(sys_palette, palette, sizeof (palette));
	memcpy(sys_colortable, navalone_colortable, sizeof (navalone_colortable));
}


INLINE void geebee_plot(struct osd_bitmap *bitmap, int x, int y)
{
	struct rectangle r = Machine->visible_area;
	if (x >= r.min_x && x <= r.max_x && y >= r.min_y && y <= r.max_y)
		plot_pixel(bitmap,x,y,Machine->pens[1]);
}

INLINE void mark_dirty(int x, int y)
{
	int cx, cy, offs;
	cy = y / 8;
	cx = x / 8;
    if (geebee_inv)
	{
		offs = (32 - cx) + (31 - cy) * 32;
		dirtybuffer[offs % videoram_size] = 1;
		dirtybuffer[(offs - 1) & (videoram_size - 1)] = 1;
		dirtybuffer[(offs - 32) & (videoram_size - 1)] = 1;
		dirtybuffer[(offs - 32 - 1) & (videoram_size - 1)] = 1;
	}
	else
	{
		offs = (cx - 1) + cy * 32;
		dirtybuffer[offs & (videoram_size - 1)] = 1;
		dirtybuffer[(offs + 1) & (videoram_size - 1)] = 1;
		dirtybuffer[(offs + 32) & (videoram_size - 1)] = 1;
		dirtybuffer[(offs + 32 + 1) & (videoram_size - 1)] = 1;
	}
}

void geebee_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int offs;

	if (palette_recalc() || full_refresh )
        memset(dirtybuffer, 1, videoram_size);

	for( offs = 0; offs < videoram_size; offs++ )
	{
		if( dirtybuffer[offs] )
		{
			int mx,my,sx,sy,code,color;

			dirtybuffer[offs] = 0;

			mx = offs % 32;
			my = offs / 32;

			if (my == 0)
			{
				sx = 8*33;
				sy = 8*mx;
			}
			else if (my == 1)
			{
				sx = 0;
				sy = 8*mx;
			}
			else
			{
				sx = 8*(mx+1);
				sy = 8*my;
			}

			if (geebee_inv)
			{
				sx = 33*8 - sx;
				sy = 31*8 - sy;
			}

			code = videoram[offs];
			color = ((geebee_bgw & 1) << 1) | ((code & 0x80) >> 7);
			drawgfx(bitmap,Machine->gfx[0],
					code,color,
					geebee_inv,geebee_inv,sx,sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	if( geebee_ball_on )
	{
		int x, y;

		mark_dirty(geebee_ball_h+5,geebee_ball_v-2);
		for( y = 0; y < 4; y++ )
			for( x = 0; x < 4; x++ )
				geebee_plot(bitmap,geebee_ball_h+x+5,geebee_ball_v+y-2);
	}
}
