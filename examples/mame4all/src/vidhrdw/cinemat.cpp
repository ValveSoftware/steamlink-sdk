/***************************************************************************

  vidhrdw.c

  Generic functions used by the Cinematronics Vector games

***************************************************************************/

#include "driver.h"
#include "vector.h"
#include "cpu/ccpu/ccpu.h"

#define VEC_SHIFT 16

#define RED   0x04
#define GREEN 0x02
#define BLUE  0x01
#define WHITE RED|GREEN|BLUE

static int cinemat_monitor_type;
static int cinemat_overlay_req;
static int cinemat_backdrop_req;
static int cinemat_screenh;
static struct artwork_element *cinemat_simple_overlay;

static int color_display;
static struct artwork *spacewar_panel;
static struct artwork *spacewar_pressed_panel;

struct artwork_element starcas_overlay[]=
{
	{{ 0, 400-1, 0, 300-1},0x00,  61,  0xff, OVERLAY_DEFAULT_OPACITY},
	{{ 200, 49, 150, -2},  0xff, 0x20, 0x20, OVERLAY_DEFAULT_OPACITY},
	{{ 200, 29, 150, -2},  0xff, 0xff, 0xff, OVERLAY_DEFAULT_OPACITY}, /* punch hole in outer circle */
	{{ 200, 38, 150, -1},  0xe0, 0xff, 0x00, OVERLAY_DEFAULT_OPACITY},
	{{-1,-1,-1,-1},0,0,0,0}
};

struct artwork_element tailg_overlay[]=
{
	{{0, 400-1, 0, 300-1}, 0, 64, 64, OVERLAY_DEFAULT_OPACITY},
	{{-1,-1,-1,-1},0,0,0,0}
};

struct artwork_element sundance_overlay[]=
{
	{{0, 400-1, 0, 300-1}, 0xff, 0xff, 0x20, OVERLAY_DEFAULT_OPACITY},
	{{-1,-1,-1,-1},0,0,0,0}
};

struct artwork_element solarq_overlay[]=
{
	{{0, 400-1, 30, 300-1},0x20, 0x20, 0xff, OVERLAY_DEFAULT_OPACITY},
	{{ 0,  399, 0,    29}, 0xff, 0x20, 0x20, OVERLAY_DEFAULT_OPACITY},
	{{ 200, 12, 150,  -2}, 0xff, 0xff, 0x20, OVERLAY_DEFAULT_OPACITY},
	{{-1,-1,-1,-1},0,0,0,0}
};


void CinemaVectorData (int fromx, int fromy, int tox, int toy, int color)
{
    static int lastx, lasty;

	fromy = cinemat_screenh - fromy;
	toy = cinemat_screenh - toy;

	if (fromx != lastx || fromx != lasty)
		vector_add_point (fromx << VEC_SHIFT, fromy << VEC_SHIFT, 0, 0);

    if (color_display)
        vector_add_point (tox << VEC_SHIFT, toy << VEC_SHIFT, color & 0x07, color & 0x08 ? 0x80: 0x40);
    else
        vector_add_point (tox << VEC_SHIFT, toy << VEC_SHIFT, WHITE, color * 12);

	lastx = tox;
	lasty = toy;
}

/* This is called by the game specific init function and sets the local
 * parameters for the generic function cinemat_init_colors() */
void cinemat_select_artwork (int monitor_type, int overlay_req, int backdrop_req, struct artwork_element *simple_overlay)
{
	cinemat_monitor_type = monitor_type;
	cinemat_overlay_req = overlay_req;
	cinemat_backdrop_req = backdrop_req;
	cinemat_simple_overlay = simple_overlay;
}

static void shade_fill (unsigned char *palette, int rgb, int start_index, int end_index, int start_inten, int end_inten)
{
	int i, inten, index_range, inten_range;

	index_range = end_index-start_index;
	inten_range = end_inten-start_inten;
	for (i = start_index; i <= end_index; i++)
	{
		inten = start_inten + (inten_range) * (i-start_index) / (index_range);
		palette[3*i  ] = (rgb & RED  )? inten : 0;
		palette[3*i+1] = (rgb & GREEN)? inten : 0;
		palette[3*i+2] = (rgb & BLUE )? inten : 0;
	}
}


void cinemat_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,j,k, nextcol;
	char filename[1024];

	int trcl1[] = { 0,0,2,2,1,1 };
	int trcl2[] = { 1,2,0,1,0,2 };
	int trcl3[] = { 2,1,1,0,2,0 };

	/* initialize the first 8 colors with the basic colors */
	for (i = 0; i < 8; i++)
	{
		palette[3*i  ] = (i & RED  ) ? 0xff : 0;
		palette[3*i+1] = (i & GREEN) ? 0xff : 0;
		palette[3*i+2] = (i & BLUE ) ? 0xff : 0;
	}

	shade_fill (palette, WHITE, 8, 23, 0, 255);
    nextcol = 24;

	/* fill the rest of the 256 color entries depending on the game */
	switch (cinemat_monitor_type)
	{
		case  CCPU_MONITOR_BILEV:
		case  CCPU_MONITOR_16LEV:
            color_display = FALSE;
			/* Attempt to load backdrop if requested */
			if (cinemat_backdrop_req)
			{
                sprintf (filename, "%sb.png", Machine->gamedrv->name );
				backdrop_load(filename, nextcol, Machine->drv->total_colors-nextcol);
				if (artwork_backdrop!=NULL)
                {
					memcpy (palette+3*artwork_backdrop->start_pen, artwork_backdrop->orig_palette, 3*artwork_backdrop->num_pens_used);
					if (Machine->scrbitmap->depth == 8)
						nextcol += artwork_backdrop->num_pens_used;
                }
			}
			/* Attempt to load overlay if requested */
			if (cinemat_overlay_req)
			{
				if (cinemat_simple_overlay != NULL)
				{
					/* use simple overlay */
					artwork_elements_scale(cinemat_simple_overlay,
										   Machine->scrbitmap->width,
										   Machine->scrbitmap->height);
					overlay_create(cinemat_simple_overlay, nextcol,Machine->drv->total_colors-nextcol);
				}
				else
				{
					/* load overlay from file */
	                sprintf (filename, "%so.png", Machine->gamedrv->name );
					overlay_load(filename, nextcol, Machine->drv->total_colors-nextcol);
				}

				if ((Machine->scrbitmap->depth == 8) || (artwork_backdrop == 0))
					overlay_set_palette (palette, (Machine->drv->total_colors > 256 ? 256 : Machine->drv->total_colors) - nextcol);
			}
			break;

		case  CCPU_MONITOR_WOWCOL:
            color_display = TRUE;
			/* TODO: support real color */
			/* put in 40 shades for red, blue and magenta */
			shade_fill (palette, RED       ,   8,  47, 10, 250);
			shade_fill (palette, BLUE      ,  48,  87, 10, 250);
			shade_fill (palette, RED|BLUE  ,  88, 127, 10, 250);

			/* put in 20 shades for yellow and green */
			shade_fill (palette, GREEN     , 128, 147, 10, 250);
			shade_fill (palette, RED|GREEN , 148, 167, 10, 250);

			/* and 14 shades for cyan and white */
			shade_fill (palette, BLUE|GREEN, 168, 181, 10, 250);
			shade_fill (palette, WHITE     , 182, 194, 10, 250);

			/* Fill in unused gaps with more anti-aliasing colors. */
			/* There are 60 slots available.           .ac JAN2498 */
			i=195;
			for (j=0; j<6; j++)
			{
				for (k=7; k<=16; k++)
				{
					palette[3*i+trcl1[j]] = ((256*k)/16)-1;
					palette[3*i+trcl2[j]] = ((128*k)/16)-1;
					palette[3*i+trcl3[j]] = 0;
					i++;
				}
			}
			break;
	}
}


void spacewar_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int width, height, i, nextcol;

    color_display = FALSE;

	/* initialize the first 8 colors with the basic colors */
	for (i = 0; i < 8; i++)
	{
		palette[3*i  ] = (i & RED  ) ? 0xff : 0;
		palette[3*i+1] = (i & GREEN) ? 0xff : 0;
		palette[3*i+2] = (i & BLUE ) ? 0xff : 0;
	}

	for (i = 0; i < 16; i++)
		palette[3*(i+8)]=palette[3*(i+8)+1]=palette[3*(i+8)+2]= (255*i)/15;

	spacewar_pressed_panel = NULL;
	width = Machine->scrbitmap->width;
	height = 0.16 * width;

	nextcol = 24;

	artwork_load_size(&spacewar_panel, "spacewr1.png", nextcol, Machine->drv->total_colors - nextcol, width, height);
	if (spacewar_panel != NULL)
	{
		if (Machine->scrbitmap->depth == 8)
			nextcol += spacewar_panel->num_pens_used;

		artwork_load_size(&spacewar_pressed_panel, "spacewr2.png", nextcol, Machine->drv->total_colors - nextcol, width, height);
		if (spacewar_pressed_panel == NULL)
		{
			artwork_free(&spacewar_panel);
			return ;
		}
	}
	else
		return;

	memcpy (palette+3*spacewar_panel->start_pen, spacewar_panel->orig_palette,
			3*spacewar_panel->num_pens_used);

	if (Machine->scrbitmap->depth == 8)
		memcpy (palette+3*spacewar_pressed_panel->start_pen, spacewar_pressed_panel->orig_palette,
				3*spacewar_pressed_panel->num_pens_used);
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int cinemat_vh_start (void)
{
	vector_set_shift (VEC_SHIFT);

	if (artwork_backdrop)
	{
		backdrop_refresh (artwork_backdrop);
		backdrop_refresh_tables (artwork_backdrop);
	}

	cinemat_screenh = Machine->visible_area.max_y - Machine->visible_area.min_y;
	return vector_vh_start();
}

int spacewar_vh_start (void)
{
	vector_set_shift (VEC_SHIFT);
	if (spacewar_panel) backdrop_refresh(spacewar_panel);
	if (spacewar_pressed_panel) backdrop_refresh(spacewar_pressed_panel);
	cinemat_screenh = Machine->visible_area.max_y - Machine->visible_area.min_y;
	return vector_vh_start();
}


void cinemat_vh_stop (void)
{
	vector_vh_stop();
}

void spacewar_vh_stop (void)
{
	if (spacewar_panel != NULL)
		artwork_free(&spacewar_panel);

	if (spacewar_pressed_panel != NULL)
		artwork_free(&spacewar_pressed_panel);

	vector_vh_stop();
}

void cinemat_vh_screenrefresh (struct osd_bitmap *bitmap, int full_refresh)
{
    vector_vh_screenrefresh(bitmap, full_refresh);
    vector_clear_list ();
}

void spacewar_vh_screenrefresh (struct osd_bitmap *bitmap, int full_refresh)
{
    int tk[] = {3, 8, 4, 9, 1, 6, 2, 7, 5, 0};
	int i, pwidth, pheight, key, row, col, sw_option;
	float scale;
	struct osd_bitmap vector_bitmap;
	struct rectangle rect;

    static int sw_option_change;

	if (spacewar_panel == NULL)
	{
        vector_vh_screenrefresh(bitmap, full_refresh);
        vector_clear_list ();
		return;
	}

	pwidth = spacewar_panel->artwork->width;
	pheight = spacewar_panel->artwork->height;

	vector_bitmap.width = bitmap->width;
	vector_bitmap.height = bitmap->height - pheight;
	vector_bitmap._private = bitmap->_private;
	vector_bitmap.line = bitmap->line;

	vector_vh_screenrefresh(&vector_bitmap,full_refresh);
    vector_clear_list ();

	if (full_refresh)
		copybitmap(bitmap,spacewar_panel->artwork,0,0,
				   0,bitmap->height - pheight, 0,TRANSPARENCY_NONE,0);

	scale = pwidth/1024.0;

    sw_option = input_port_1_r(0);

    /* move bits 10-11 to position 8-9, so we can just use a simple 'for' loop */
    sw_option = (sw_option & 0xff) | ((sw_option >> 2) & 0x0300);

    sw_option_change ^= sw_option;

	for (i = 0; i < 10; i++)
	{
		if (sw_option_change & (1 << i) || full_refresh)
		{
            key = tk[i];
            col = key % 5;
            row = key / 5;
			rect.min_x = scale * (465 + 20 * col);
			rect.max_x = scale * (465 + 20 * col + 18);
			rect.min_y = scale * (39  + 20 * row) + vector_bitmap.height;
			rect.max_y = scale * (39  + 20 * row + 18) + vector_bitmap.height;

			if (sw_option & (1 << i))
            {
				copybitmap(bitmap,spacewar_panel->artwork,0,0,
						   0, vector_bitmap.height, &rect,TRANSPARENCY_NONE,0);
            }
			else
            {
				copybitmap(bitmap,spacewar_pressed_panel->artwork,0,0,
						   0, vector_bitmap.height, &rect,TRANSPARENCY_NONE,0);
            }

			osd_mark_dirty (rect.min_x, rect.min_y,
                            rect.max_x, rect.max_y, 0);
		}
	}
    sw_option_change = sw_option;
}

int cinemat_clear_list(void)
{
    if (osd_skip_this_frame())
        vector_clear_list ();
    return ignore_interrupt();
}
