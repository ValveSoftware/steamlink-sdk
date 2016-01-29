/***************************************************************************

  vidhrdw/llander.c

  Functions to emulate the blinking control panel in lunar lander.
  Added 11/6/98, by Chris Kirmse (ckirmse@ricochet.net)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"

#define NUM_LIGHTS 5

static struct artwork *llander_panel;
static struct artwork *llander_lit_panel;

static struct rectangle light_areas[NUM_LIGHTS] =
{
	{  0, 205, 0, 127 },
	{206, 343, 0, 127 },
	{344, 481, 0, 127 },
	{482, 616, 0, 127 },
	{617, 799, 0, 127 },
};

/* current status of each light */
static int lights[NUM_LIGHTS];
/* whether or not each light needs to be redrawn*/
static int lights_changed[NUM_LIGHTS];
/***************************************************************************

  Lunar Lander video routines

***************************************************************************/

void llander_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int width, height, i, nextcol;

	avg_init_palette_white(palette,colortable,color_prom);

	llander_lit_panel = NULL;
	width = Machine->scrbitmap->width;
	height = 0.16 * width;

	nextcol = 24;

	artwork_load_size(&llander_panel, "llander.png", nextcol, Machine->drv->total_colors-nextcol, width, height);
	if (llander_panel != NULL)
	{
		if (Machine->scrbitmap->depth == 8)
			nextcol += llander_panel->num_pens_used;

		artwork_load_size(&llander_lit_panel, "llander1.png", nextcol, Machine->drv->total_colors-nextcol, width, height);
		if (llander_lit_panel == NULL)
		{
			artwork_free(&llander_panel);
			return ;
		}
	}
	else
		return;

	for (i = 0; i < 16; i++)
		palette[3*(i+8)]=palette[3*(i+8)+1]=palette[3*(i+8)+2]= (255*i)/15;

	memcpy (palette+3*llander_panel->start_pen, llander_panel->orig_palette,
			3*llander_panel->num_pens_used);
	memcpy (palette+3*llander_lit_panel->start_pen, llander_lit_panel->orig_palette,
			3*llander_lit_panel->num_pens_used);
}

int llander_start(void)
{
	int i;

	if (dvg_start())
		return 1;

	if (llander_panel == NULL)
		return 0;

	for (i=0;i<NUM_LIGHTS;i++)
	{
		lights[i] = 0;
		lights_changed[i] = 1;
	}
	if (llander_panel) backdrop_refresh(llander_panel);
	if (llander_lit_panel) backdrop_refresh(llander_lit_panel);
	return 0;
}

void llander_stop(void)
{
	dvg_stop();

	if (llander_panel != NULL)
		artwork_free(&llander_panel);

	if (llander_lit_panel != NULL)
		artwork_free(&llander_lit_panel);

}

void llander_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i, pwidth, pheight;
	float scale;
	struct osd_bitmap vector_bitmap;
	struct rectangle rect;

	if (llander_panel == NULL)
	{
		vector_vh_screenrefresh(bitmap,full_refresh);
		return;
	}

	pwidth = llander_panel->artwork->width;
	pheight = llander_panel->artwork->height;

	vector_bitmap.width = bitmap->width;
	vector_bitmap.height = bitmap->height - pheight;
	vector_bitmap._private = bitmap->_private;
	vector_bitmap.line = bitmap->line;

	vector_vh_screenrefresh(&vector_bitmap,full_refresh);

	if (full_refresh)
	{
		rect.min_x = 0;
		rect.max_x = pwidth-1;
		rect.min_y = bitmap->height - pheight;
		rect.max_y = bitmap->height - 1;

		copybitmap(bitmap,llander_panel->artwork,0,0,
				   0,bitmap->height - pheight,&rect,TRANSPARENCY_NONE,0);
		osd_mark_dirty (rect.min_x,rect.min_y,rect.max_x,rect.max_y,0);
	}

	scale = pwidth/800.0;

	for (i=0;i<NUM_LIGHTS;i++)
	{
		if (lights_changed[i] || full_refresh)
		{
			rect.min_x = scale * light_areas[i].min_x;
			rect.max_x = scale * light_areas[i].max_x;
			rect.min_y = bitmap->height - pheight + scale * light_areas[i].min_y;
			rect.max_y = bitmap->height - pheight + scale * light_areas[i].max_y;

			if (lights[i])
				copybitmap(bitmap,llander_lit_panel->artwork,0,0,
						   0,bitmap->height - pheight,&rect,TRANSPARENCY_NONE,0);
			else
				copybitmap(bitmap,llander_panel->artwork,0,0,
						   0,bitmap->height - pheight,&rect,TRANSPARENCY_NONE,0);

			osd_mark_dirty (rect.min_x,rect.min_y,rect.max_x,rect.max_y,0);

			lights_changed[i] = 0;
		}
	}
}

/* Lunar lander LED port seems to be mapped thus:

   NNxxxxxx - Apparently unused
   xxNxxxxx - Unknown gives 4 high pulses of variable duration when coin put in ?
   xxxNxxxx - Start    Lamp ON/OFF == 0/1
   xxxxNxxx - Training Lamp ON/OFF == 1/0
   xxxxxNxx - Cadet    Lamp ON/OFF
   xxxxxxNx - Prime    Lamp ON/OFF
   xxxxxxxN - Command  Lamp ON/OFF

   Selection lamps seem to all be driver 50/50 on/off during attract mode ?

*/

WRITE_HANDLER( llander_led_w )
{
	/*      logerror("LANDER LED: %02x\n",data); */

    int i;

    for (i=0;i<5;i++)
    {
		int new_light = (data & (1 << (4-i))) != 0;
		if (lights[i] != new_light)
		{
			lights[i] = new_light;
			lights_changed[i] = 1;
		}
    }



}

