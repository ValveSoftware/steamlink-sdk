#include "driver.h"


extern unsigned char *videoram;
extern size_t videoram_size;
extern unsigned char *colorram;
extern unsigned char *spriteram;
extern size_t spriteram_size;
extern unsigned char *spriteram_2;
extern size_t spriteram_2_size;
extern unsigned char *spriteram_3;
extern size_t spriteram_3_size;
extern unsigned char *buffered_spriteram;
extern unsigned char *buffered_spriteram_2;
extern unsigned char *dirtybuffer;
extern struct osd_bitmap *tmpbitmap;

int generic_vh_start(void);
int generic_bitmapped_vh_start(void);
void generic_vh_stop(void);
void generic_bitmapped_vh_stop(void);
void generic_bitmapped_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
READ_HANDLER( videoram_r );
READ_HANDLER( colorram_r );
WRITE_HANDLER( videoram_w );
WRITE_HANDLER( colorram_w );
READ_HANDLER( spriteram_r );
WRITE_HANDLER( spriteram_w );
READ_HANDLER( spriteram_2_r );
WRITE_HANDLER( spriteram_2_w );
WRITE_HANDLER( buffer_spriteram_w );
WRITE_HANDLER( buffer_spriteram_2_w );
void buffer_spriteram(unsigned char *ptr,int length);
void buffer_spriteram_2(unsigned char *ptr,int length);

