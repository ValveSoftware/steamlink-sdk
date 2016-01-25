/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "common.h"
#include "vidhrdw/generic.h"



unsigned char *gottlieb_characterram;
#define MAX_CHARS 256
static unsigned char *dirtycharacter;
static int background_priority=0;
static unsigned char hflip=0;
static unsigned char vflip=0;
static int spritebank;



/***************************************************************************

  Gottlieb games dosn't have a color PROM. They use 32 bytes of RAM to
  dynamically create the palette. Each couple of bytes defines one
  color (4 bits per pixel; the high 4 bits of the second byte are unused).

  The RAM is conected to the RGB output this way:

  bit 7 -- 240 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 2  kohm resistor  -- GREEN
        -- 240 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
        -- 1  kohm resistor  -- RED
  bit 0 -- 2  kohm resistor  -- RED

  bit 7 -- unused
        -- unused
        -- unused
        -- unused
        -- 240 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 1  kohm resistor  -- BLUE
  bit 0 -- 2  kohm resistor  -- BLUE

***************************************************************************/
WRITE_HANDLER( gottlieb_paletteram_w )
{
	int bit0,bit1,bit2,bit3;
	int r,g,b,val;


	paletteram[offset] = data;

	/* red component */
	val = paletteram[offset | 1];
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	r = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	/* green component */
	val = paletteram[offset & ~1];
	bit0 = (val >> 4) & 0x01;
	bit1 = (val >> 5) & 0x01;
	bit2 = (val >> 6) & 0x01;
	bit3 = (val >> 7) & 0x01;
	g = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	/* blue component */
	val = paletteram[offset & ~1];
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	b = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	palette_change_color(offset / 2,r,g,b);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int gottlieb_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((dirtycharacter = (unsigned char*)malloc(MAX_CHARS)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	/* Some games have character gfx data in ROM, some others in RAM. We don't */
	/* want to recalculate chars if data is in ROM, so let's start with the array */
	/* initialized to 0. */
	memset(dirtycharacter,0,MAX_CHARS);

	return 0;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void gottlieb_vh_stop(void)
{
	free(dirtycharacter);
	generic_vh_stop();
}



WRITE_HANDLER( gottlieb_video_outputs_w )
{
	extern void gottlieb_knocker(void);
	static int last = 0;


	background_priority = data & 1;

	hflip = data & 2;
	vflip = data & 4;
	if ((data & 6) != (last & 6))
		memset(dirtybuffer,1,videoram_size);

	/* in Q*Bert Qubes only, bit 4 controls the sprite bank */
	spritebank = (data & 0x10) >> 4;

	if ((last&0x20) && !(data&0x20)) gottlieb_knocker();

	last = data;
}

WRITE_HANDLER( usvsthem_video_outputs_w )
{
	background_priority = data & 1;

	/* in most games, bits 1 and 2 flip screen, however in the laser */
	/* disc games they are different. */

	/* bit 1 controls the sprite bank. */
	spritebank = (data & 0x02) >> 1;

	/* bit 2 video enable (0 = black screen) */

	/* bit 3 genlock control (1 = show laserdisc image) */
}



WRITE_HANDLER( gottlieb_characterram_w )
{
	if (gottlieb_characterram[offset] != data)
	{
		dirtycharacter[offset / 32] = 1;
		gottlieb_characterram[offset] = data;
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void gottlieb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
    int offs;


	/* update palette */
	if (palette_recalc())
		memset(dirtybuffer, 1, videoram_size);

    /* recompute character graphics */
    for (offs = 0;offs < Machine->drv->gfxdecodeinfo[0].gfxlayout->total;offs++)
	{
		if (dirtycharacter[offs])
			decodechar(Machine->gfx[0],offs,gottlieb_characterram,Machine->drv->gfxdecodeinfo[0].gfxlayout);
	}


    /* for every character in the Video RAM, check if it has been modified */
    /* since last time and update it accordingly. */
    for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs] || dirtycharacter[videoram[offs]])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			if (hflip) sx = 31 - sx;
			if (vflip) sy = 29 - sy;

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					0,
					hflip,vflip,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
    }

	memset(dirtycharacter,0,MAX_CHARS);


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
    for (offs = 0;offs < spriteram_size - 8;offs += 4)     /* it seems there's something strange with sprites #62 and #63 */
	{
	    int sx,sy;


		/* coordinates hand tuned to make the position correct in Q*Bert Qubes start */
		/* of level animation. */
		sx = (spriteram[offs + 1]) - 4;
		if (hflip) sx = 233 - sx;
		sy = (spriteram[offs]) - 13;
		if (vflip) sy = 228 - sy;

		if (spriteram[offs] || spriteram[offs + 1])	/* needed to avoid garbage on screen */
			drawgfx(bitmap,Machine->gfx[1],
					(255 ^ spriteram[offs + 2]) + 256 * spritebank,
					0,
					hflip,vflip,
					sx,sy,
					&Machine->visible_area,
					background_priority ? TRANSPARENCY_THROUGH : TRANSPARENCY_PEN,
					background_priority ? Machine->pens[0]     : 0);
	}
}
