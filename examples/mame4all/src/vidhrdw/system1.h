#ifndef _system1_H_
#define _system1_H_

#include "driver.h"
#include "vidhrdw/generic.h"

#define SPR_Y_TOP		0
#define SPR_Y_BOTTOM	1
#define SPR_X_LO		2
#define SPR_X_HI		3
#define SPR_SKIP_LO		4
#define SPR_SKIP_HI		5
#define SPR_GFXOFS_LO	6
#define SPR_GFXOFS_HI	7

#define system1_SPRITE_PIXEL_MODE1	0	// mode in which coordinates Y of sprites are using for priority checking
						// (pitfall2,upndown,wb deluxe)
#define system1_SPRITE_PIXEL_MODE2	1	// mode in which sprites are always drawing in order (0.1.2...31)
						// (choplifter,wonder boy in monster land)

#define system1_BACKGROUND_MEMORY_SINGLE 0
#define system1_BACKGROUND_MEMORY_BANKED 1

extern unsigned char 	*system1_scroll_y;
extern unsigned char 	*system1_scroll_x;
extern unsigned char 	*system1_videoram;
extern unsigned char 	*system1_backgroundram;
extern unsigned char 	*system1_sprites_collisionram;
extern unsigned char 	*system1_background_collisionram;
extern unsigned char 	*system1_scrollx_ram;
extern size_t system1_videoram_size;
extern size_t system1_backgroundram_size;


int  system1_vh_start(void);
void system1_vh_stop(void);
void system1_define_sprite_pixelmode(int Mode);
void system1_define_background_memory(int Mode);

READ_HANDLER( wbml_bg_bankselect_r );
WRITE_HANDLER( wbml_bg_bankselect_w );
READ_HANDLER( wbml_paged_videoram_r );
WRITE_HANDLER( wbml_paged_videoram_w );
WRITE_HANDLER( system1_background_collisionram_w );
WRITE_HANDLER( system1_sprites_collisionram_w );
WRITE_HANDLER( system1_videoram_w );
WRITE_HANDLER( system1_paletteram_w );
WRITE_HANDLER( system1_backgroundram_w );
void system1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void system1_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( system1_videomode_w );
READ_HANDLER( system1_videomode_r );

WRITE_HANDLER( choplifter_scroll_x_w );
void choplifter_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void wbml_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

#endif
