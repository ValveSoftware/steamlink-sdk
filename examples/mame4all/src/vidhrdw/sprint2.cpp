/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* machine/sprint2.c */
extern int sprint2_collision1_data;
extern int sprint2_collision2_data;
extern int sprint2_gear1;
extern int sprint2_gear2;

unsigned char *sprint2_horiz_ram;
unsigned char *sprint2_vert_car_ram;

static struct osd_bitmap *back_vid;
static struct osd_bitmap *grey_cars_vid;
static struct osd_bitmap *black_car_vid;
static struct osd_bitmap *white_car_vid;

#define WHITE_CAR   0
#define BLACK_CAR   1
#define GREY_CAR1   2
#define GREY_CAR2   3

/***************************************************************************
***************************************************************************/

int sprint2_vh_start(void)
{
	if (generic_vh_start()!=0)
		return 1;

	if ((back_vid = bitmap_alloc(16,8)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	if ((grey_cars_vid = bitmap_alloc(16,8)) == 0)
	{
		bitmap_free(back_vid);
		generic_vh_stop();
		return 1;
	}

	if ((black_car_vid = bitmap_alloc(16,8)) == 0)
	{
		bitmap_free(back_vid);
		bitmap_free(grey_cars_vid);
		generic_vh_stop();
		return 1;
	}

	if ((white_car_vid = bitmap_alloc(16,8)) == 0)
	{
		bitmap_free(back_vid);
		bitmap_free(grey_cars_vid);
		bitmap_free(black_car_vid);
		generic_vh_stop();
		return 1;
	}

	return 0;
}

/***************************************************************************
***************************************************************************/

void sprint2_vh_stop(void)
{
	bitmap_free(back_vid);
	bitmap_free(grey_cars_vid);
	bitmap_free(black_car_vid);
	bitmap_free(white_car_vid);
	generic_vh_stop();
}

/***************************************************************************
sprint2_check_collision

It might seem strange to put the collision-checking routine in vidhrdw.
However, the way Sprint2 hardware collision-checking works is by sending
the video signals for the grey cars, white car, black car, black background,
and white background through a series of logic gates.  This effectively checks
for collisions at a pixel-by-pixel basis.  So we'll do the same thing, but
with a little bit of smarts - there can only be collisions where the black
car and white car are located, so we'll base our checks on these two locations.

We can't just check the color of the main bitmap at a given location, because one
of our video signals might have overdrawn another one.  So here's what we do:
1)  Redraw the background, grey cars, black car, and white car into separate
bitmaps, but clip to where the white car is located.
2)  Scan through the bitmaps, apply the logic from the logic gates and look
for a collision (Collision1).
3)  Redraw the background, grey cars, black car, and white car into separate
bitmaps, but clip to where the black car is located.
4)  Scan through the bitmaps, apply the logic from the logic gates, and look
for a collision (Collision2).
***************************************************************************/

void sprint2_check_collision1(struct osd_bitmap *bitmap)
{
    int sx,sy,org_x,org_y;
    struct rectangle clip;
    int offs;

    clip.min_x=0;
    clip.max_x=15;
    clip.min_y=0;
    clip.max_y=7;

    /* Clip in relation to the white car. */

    org_x=30*8-sprint2_horiz_ram[WHITE_CAR];
    org_y=31*8-sprint2_vert_car_ram[WHITE_CAR*2];

    fillbitmap(back_vid,Machine->pens[1],&clip);
    fillbitmap(grey_cars_vid,Machine->pens[1],&clip);
    fillbitmap(white_car_vid,Machine->pens[1],&clip);
    fillbitmap(black_car_vid,Machine->pens[1],&clip);

    /* Draw the background - a car can overlap up to 6 background squares. */
    /* This could be optimized by not drawing all 6 every time. */

    offs=((org_y/8)*32) + ((org_x/8)%32);

    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=((org_y/8)*32) + (((org_x+8)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=((org_y/8)*32) + (((org_x+16)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=(((org_y+8)/8)*32) + ((org_x/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=(((org_y+8)/8)*32) + (((org_x+8)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=(((org_y+8)/8)*32) + (((org_x+16)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);


    /* Grey car 1 */
    sx=30*8-sprint2_horiz_ram[GREY_CAR1];
    sy=31*8-sprint2_vert_car_ram[GREY_CAR1*2];
    sx=sx-org_x;
    sy=sy-org_y;

    drawgfx(grey_cars_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[GREY_CAR1*2+1]>>3), GREY_CAR1,
            0,0,sx,sy,&clip,TRANSPARENCY_NONE,0);

    /* Grey car 2 */
    sx=30*8-sprint2_horiz_ram[GREY_CAR2];
    sy=31*8-sprint2_vert_car_ram[GREY_CAR2*2];
    sx=sx-org_x;
    sy=sy-org_y;

    drawgfx(grey_cars_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[GREY_CAR2*2+1]>>3), GREY_CAR2,
            0,0,sx,sy,&clip,TRANSPARENCY_COLOR,1);


    /* Black car */
    sx=30*8-sprint2_horiz_ram[BLACK_CAR];
    sy=31*8-sprint2_vert_car_ram[BLACK_CAR*2];
    sx=sx-org_x;
    sy=sy-org_y;

    drawgfx(black_car_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[BLACK_CAR*2+1]>>3), BLACK_CAR,
            0,0,sx,sy,&clip,TRANSPARENCY_NONE,0);

    /* White car */
    drawgfx(white_car_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[WHITE_CAR*2+1]>>3), WHITE_CAR,
            0,0,0,0,&clip,TRANSPARENCY_NONE,0);

    /* Now check for Collision1 */
    for (sy=0;sy<8;sy++)
    {
        for (sx=0;sx<16;sx++)
        {
                if (read_pixel(white_car_vid, sx, sy)==Machine->pens[3])
                {
					int back_pixel;

                    /* Condition 1 - white car = black car */
                    if (read_pixel(black_car_vid, sx, sy)==Machine->pens[0])
                        sprint2_collision1_data|=0x40;

                    /* Condition 2 - white car = grey cars */
                    if (read_pixel(grey_cars_vid, sx, sy)==Machine->pens[2])
                        sprint2_collision1_data|=0x40;

                    back_pixel = read_pixel(back_vid, sx, sy);

                    /* Condition 3 - white car = black playfield (oil) */
                    if (back_pixel==Machine->pens[0])
                        sprint2_collision1_data|=0x40;

                    /* Condition 4 - white car = white playfield (track) */
                    if (back_pixel==Machine->pens[3])
                        sprint2_collision1_data|=0x80;
               }
        }
    }

}

void sprint2_check_collision2(struct osd_bitmap *bitmap)
{

    int sx,sy,org_x,org_y;
    struct rectangle clip;
    int offs;

    clip.min_x=0;
    clip.max_x=15;
    clip.min_y=0;
    clip.max_y=7;

    /* Clip in relation to the black car. */

    org_x=30*8-sprint2_horiz_ram[BLACK_CAR];
    org_y=31*8-sprint2_vert_car_ram[BLACK_CAR*2];

    fillbitmap(back_vid,Machine->pens[1],&clip);
    fillbitmap(grey_cars_vid,Machine->pens[1],&clip);
    fillbitmap(white_car_vid,Machine->pens[1],&clip);
    fillbitmap(black_car_vid,Machine->pens[1],&clip);

    /* Draw the background - a car can overlap up to 6 background squares. */
    /* This could be optimized by not drawing all 6 every time. */

    offs=((org_y/8)*32) + ((org_x/8)%32);

    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=((org_y/8)*32) + (((org_x+8)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=((org_y/8)*32) + (((org_x+16)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=(((org_y+8)/8)*32) + ((org_x/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=(((org_y+8)/8)*32) + (((org_x+8)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);

    offs=(((org_y+8)/8)*32) + (((org_x+16)/8)%32);
    sx = 8 * (offs % 32)-org_x;
    sy = 8 * (offs / 32)-org_y;

    drawgfx(back_vid,Machine->gfx[0],
            videoram[offs] & 0x3F, (videoram[offs] & 0x80)>>7,
			0,0,sx,sy, &clip,TRANSPARENCY_NONE,0);




    /* Grey car 1 */
    sx=30*8-sprint2_horiz_ram[GREY_CAR1];
    sy=31*8-sprint2_vert_car_ram[GREY_CAR1*2];
    sx=sx-org_x;
    sy=sy-org_y;

    drawgfx(grey_cars_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[GREY_CAR1*2+1]>>3), GREY_CAR1,
            0,0,sx,sy,&clip,TRANSPARENCY_NONE,0);

    /* Grey car 2 */
    sx=30*8-sprint2_horiz_ram[GREY_CAR2];
    sy=31*8-sprint2_vert_car_ram[GREY_CAR2*2];
    sx=sx-org_x;
    sy=sy-org_y;

    drawgfx(grey_cars_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[GREY_CAR2*2+1]>>3), GREY_CAR2,
            0,0,sx,sy,&clip,TRANSPARENCY_COLOR,1);


    /* White car */
    sx=30*8-sprint2_horiz_ram[WHITE_CAR];
    sy=31*8-sprint2_vert_car_ram[WHITE_CAR*2];
    sx=sx-org_x;
    sy=sy-org_y;

    drawgfx(white_car_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[WHITE_CAR*2+1]>>3), WHITE_CAR,
            0,0,sx,sy,&clip,TRANSPARENCY_NONE,0);

    /* Black car */
    drawgfx(black_car_vid,Machine->gfx[1],
            (sprint2_vert_car_ram[BLACK_CAR*2+1]>>3), BLACK_CAR,
            0,0,0,0,&clip,TRANSPARENCY_NONE,0);

    /* Now check for Collision2 */
    for (sy=0;sy<8;sy++)
    {
        for (sx=0;sx<16;sx++)
        {
                if (read_pixel(black_car_vid, sx, sy)==Machine->pens[0])
                {
					int back_pixel;

                    /* Condition 1 - black car = white car */
                    if (read_pixel(white_car_vid, sx, sy)==Machine->pens[3])
                        sprint2_collision2_data|=0x40;

                    /* Condition 2 - black car = grey cars */
                    if (read_pixel(grey_cars_vid, sx, sy)==Machine->pens[2])
                        sprint2_collision2_data|=0x40;

                    back_pixel = read_pixel(back_vid, sx, sy);

                    /* Condition 3 - black car = black playfield (oil) */
                    if (back_pixel==Machine->pens[0])
                        sprint2_collision2_data|=0x40;

                    /* Condition 4 - black car = white playfield (track) */
                    if (back_pixel==Machine->pens[3])
                        sprint2_collision2_data|=0x80;
               }
        }
    }
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
static void sprint_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,car;

    /* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int charcode;
			int sx,sy;

			dirtybuffer[offs]=0;

			charcode = videoram[offs] & 0x3f;

			sx = 8 * (offs % 32);
			sy = 8 * (offs / 32);
			drawgfx(tmpbitmap,Machine->gfx[0],
					charcode, (videoram[offs] & 0x80)>>7,
					0,0,sx,sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Draw each one of our four cars */
	for (car=3;car>=0;car--)
	{
		int sx,sy;

		sx=30*8-sprint2_horiz_ram[car];
		sy=31*8-sprint2_vert_car_ram[car*2];

		drawgfx(bitmap,Machine->gfx[1],
				(sprint2_vert_car_ram[car*2+1]>>3), car,
				0,0,sx,sy,
				&Machine->visible_area,TRANSPARENCY_COLOR,1);
	}

	/* Refresh our collision detection buffers */
	sprint2_check_collision1(bitmap);
	sprint2_check_collision2(bitmap);
}


static void draw_gear_indicator(int gear, struct osd_bitmap *bitmap, int x, int color)
{
	/* gear shift indicators - not a part of the original game!!! */

	char gear_buf[6] = {0x07,0x05,0x01,0x12,0x00,0x00}; /* "GEAR  " */
	int offs;

	gear_buf[5] = 0x30 + gear;
	for (offs = 0; offs < 6; offs++)
		drawgfx(bitmap,Machine->gfx[0],
				gear_buf[offs],color,
				0,0,(x+offs)*8,30*8,
				&Machine->visible_area,TRANSPARENCY_NONE,0);
}


void sprint2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	sprint_vh_screenrefresh(bitmap,full_refresh);

	draw_gear_indicator(sprint2_gear1, bitmap, 25, 1);
	draw_gear_indicator(sprint2_gear2, bitmap, 1 , 0);
}

void sprint1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	sprint_vh_screenrefresh(bitmap,full_refresh);

	draw_gear_indicator(sprint2_gear1, bitmap, 12, 1);
}
