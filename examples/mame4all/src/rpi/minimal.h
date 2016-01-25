/*

  GP2X minimal library v0.A by rlyeh, (c) 2005. emulnation.info@rlyeh (swap it!)

  Thanks to Squidge, Robster, snaff, Reesy and NK, for the help & previous work! :-)

  License
  =======

  Free for non-commercial projects (it would be nice receiving a mail from you).
  Other cases, ask me first.

  GamePark Holdings is not allowed to use this library and/or use parts from it.

*/

#include <fcntl.h>
#include <linux/fb.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>

#ifndef __MINIMAL_H__
#define __MINIMAL_H__

#define gp2x_video_color8(C,R,G,B) gp2x_palette[C]=gp2x_video_color15(R,G,B,0)
#define gp2x_video_color15(R,G,B,A) (((R>>3)&0x1f) << 11) | (((G>>2)&0x3f) << 5 ) | (((B>>3)&0x1f) << 0 );

#define gp2x_video_getr15(C) (((C)>>8)&0xF8)
#define gp2x_video_getg15(C) (((C)>>3)&0xF8)
#define gp2x_video_getb15(C) (((C)<<3)&0xF8)

enum  { GP2X_UP=1<<0,   GP2X_LEFT=1<<1, GP2X_DOWN=1<<2, GP2X_RIGHT=1<<3,
        GP2X_1=1<<4,    GP2X_2=1<<5, 	GP2X_3=1<<6, 	GP2X_4=1<<7,
		GP2X_5=1<<8,	GP2X_6=1<<9, 	GP2X_7=1<<10,	GP2X_8=1<<11, GP2X_9=1<<12,
        GP2X_10=1<<13,	GP2X_11=1<<14, 	GP2X_12=1<<15, 	GP2X_13=1<<16, 
		GP2X_14=1<<17, 	GP2X_15=1<<18, 	GP2X_16=1<<19 };

#define TICKS_PER_SEC 1000000
                                            
extern volatile unsigned short 	gp2x_palette[512];
extern unsigned char 		*gp2x_screen8;
extern unsigned short 		*gp2x_screen15;

extern void gp2x_init(int tickspersecond, int bpp, int rate, int bits, int stereo, int hz, int caller);
extern void gp2x_deinit(void);
extern void gp2x_timer_delay(unsigned long ticks);
extern void gp2x_video_flip(struct osd_bitmap *bitmap);
extern void gp2x_video_flip_single(void);
extern void gp2x_video_setpalette(void);
extern void gp2x_joystick_clear(void);
extern unsigned long gp2x_joystick_read(void);
extern unsigned long gp2x_timer_read(void);

extern void gp2x_set_video_mode(struct osd_bitmap *bitmap, int bpp,int width,int height);
extern void gp2x_set_clock(int mhz);

extern void gp2x_printf(char* fmt, ...);
extern void gp2x_printf_init(void);
extern void gp2x_gamelist_text_out(int x, int y, char *eltexto, int color);
extern void gp2x_gamelist_text_out_fmt(int x, int y, char* fmt, ...);

extern void DisplayScreen(struct osd_bitmap *bitmap);
extern void FE_DisplayScreen(void);

extern void gp2x_frontend_init(void);
extern void gp2x_frontend_deinit(void);

extern void deinit_SDL(void);
extern int init_SDL(void);

#endif
