/*
 * Aztarac vector generator emulation
 *
 * Jul 25 1999 by Mathis Rosenhauer
 *
 */

#include "driver.h"
#include "vidhrdw/vector.h"

#define VEC_SHIFT 16

UINT8 *aztarac_vectorram;

static int xcenter, ycenter;

INLINE void read_vectorram (int addr, int *x, int *y, int *c)
{
    addr <<= 1;
    *c = READ_WORD (&aztarac_vectorram[addr]) & 0xffff;
    *x = READ_WORD (&aztarac_vectorram[addr + 0x1000]) & 0x03ff;
    *y = READ_WORD (&aztarac_vectorram[addr + 0x2000]) & 0x03ff;
    if (*x & 0x200) *x |= 0xfffffc00;
    if (*y & 0x200) *y |= 0xfffffc00;
}

INLINE void aztarac_vector (int x, int y, int color, int intensity)
{
    if (translucency) intensity *= 0.8;
    vector_add_point (xcenter + (x << VEC_SHIFT), ycenter - (y << VEC_SHIFT), color, intensity);
}

WRITE_HANDLER( aztarac_ubr_w )
{
    int x, y, c, intensity, xoffset, yoffset, color;
    int defaddr, objaddr=0, ndefs;

    if (data & 1)
    {
        vector_clear_list();

        while (1)
        {
            read_vectorram (objaddr, &xoffset, &yoffset, &c);
            objaddr++;

            if (c & 0x4000)
                break;

            if ((c & 0x2000) == 0)
            {
                defaddr = (c >> 1) & 0x7ff;
                aztarac_vector (xoffset, yoffset, 0, 0);

                read_vectorram (defaddr, &x, &ndefs, &c);
                ndefs++;

                if (c)
                {
                    /* latch color only once */
                    intensity = c >> 8;
                    color = c & 0x3f;
                    while (ndefs--)
                    {
                        defaddr++;
                        read_vectorram (defaddr, &x, &y, &c);
                        if ((c & 0xff00) == 0)
                            aztarac_vector (x + xoffset, y + yoffset, 0, 0);
                        else
                            aztarac_vector (x + xoffset, y + yoffset, color, intensity);
                    }
                }
                else
                {
                    /* latch color for every definition */
                    while (ndefs--)
                    {
                        defaddr++;
                        read_vectorram (defaddr, &x, &y, &c);
                        aztarac_vector (x + xoffset, y + yoffset, c & 0x3f, c >> 8);
                    }
                }
            }
        }
    }
}

int aztarac_vg_interrupt(void)
{
    return 4;
}

void aztarac_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
    int r, g, b, i;

    for (i = 4; i > 0; i--)
        for (r = 0; r < 4; r++)
            for (g = 0; g < 4; g++)
                for (b = 0; b < 4; b++)
                {
                    *palette++ = (255 * r * i)/ 12;
                    *palette++ = (255 * g * i)/ 12;
                    *palette++ = (255 * b * i)/ 12;
                }
}

int aztarac_vh_start (void)
{
    int xmin, xmax, ymin, ymax;


	xmin = Machine->visible_area.min_x;
	ymin = Machine->visible_area.min_y;
	xmax = Machine->visible_area.max_x;
	ymax = Machine->visible_area.max_y;

	xcenter=((xmax + xmin) / 2) << VEC_SHIFT;
	ycenter=((ymax + ymin) / 2) << VEC_SHIFT;

	vector_set_shift (VEC_SHIFT);
	return vector_vh_start();
}
