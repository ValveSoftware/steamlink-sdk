/*

  Tatty little tile viewer for CPS2 games

*/



#include "osdepend.h"


static int cps2_start;
static int cps2_debug;
static int cps2_width;

int cps2_gfx_start(void)
{
	UINT32 dwval;
    int size=memory_region_length(REGION_GFX1);
    unsigned char *data = memory_region(REGION_GFX1);
	int i,j,nchar,penusage,gfxsize;

    gfxsize=size/4;

	/* Set up maximum values */
    cps1_max_char  =(gfxsize/2)/8;
    cps1_max_tile16=(gfxsize/4)/8;
    cps1_max_tile32=(gfxsize/16)/8;

	cps1_gfx=(UINT32*)malloc(gfxsize*sizeof(UINT32));
	if (!cps1_gfx)
	{
		return -1;
	}

	cps1_char_pen_usage=(int*)malloc(cps1_max_char*sizeof(int));
	if (!cps1_char_pen_usage)
	{
		return -1;
	}
	memset(cps1_char_pen_usage, 0, cps1_max_char*sizeof(int));

	cps1_tile16_pen_usage=(int*)malloc(cps1_max_tile16*sizeof(int));
	if (!cps1_tile16_pen_usage)
		return -1;
	memset(cps1_tile16_pen_usage, 0, cps1_max_tile16*sizeof(int));

	cps1_tile32_pen_usage=(int*)malloc(cps1_max_tile32*sizeof(int));
	if (!cps1_tile32_pen_usage)
	{
		return -1;
	}
	memset(cps1_tile32_pen_usage, 0, cps1_max_tile32*sizeof(int));

	{
        for (i=0; i<gfxsize/4; i++)
		{
			nchar=i/8;  /* 8x8 char number */
            dwval=0;
            for (j=0; j<8; j++)
            {
				int n,mask;
				n=0;
				mask=0x80>>j;
				if (*(data+size/4)&mask)	   n|=1;
				if (*(data+size/4+1)&mask)	 n|=2;
				if (*(data+size/2+size/4)&mask)    n|=4;
				if (*(data+size/2+size/4+1)&mask)  n|=8;
				dwval|=n<<(28-j*4);
				penusage=1<<n;
                penusage=0xffff;
				cps1_char_pen_usage[nchar]|=penusage;
				cps1_tile16_pen_usage[nchar/2]|=penusage;
				cps1_tile32_pen_usage[nchar/8]|=penusage;
		   }
		   cps1_gfx[2*i]=dwval;
		   dwval=0;
		   for (j=0; j<8; j++)
		   {
				int n,mask;
				n=0;
				mask=0x80>>j;
				if (*(data)&mask)	  n|=1;
				if (*(data+1)&mask)	n|=2;
				if (*(data+size/2)&mask)   n|=4;
				if (*(data+size/2+1)&mask) n|=8;
				dwval|=n<<(28-j*4);
				penusage=1<<n;
				cps1_char_pen_usage[nchar]|=penusage;
				cps1_tile16_pen_usage[nchar/2]|=penusage;
				cps1_tile32_pen_usage[nchar/8]|=penusage;
		   }
		   cps1_gfx[2*i+1]=dwval;
           data+=4;
		}

        data = memory_region(REGION_GFX1)+2;
        for (i=0; i<gfxsize/4; i++)
		{
		   nchar=i/8+(gfxsize/4)/8;  /* 8x8 char number */
		   dwval=0;
		   for (j=0; j<8; j++)
		   {
				int n,mask;
				n=0;
				mask=0x80>>j;
				if (*(data+size/4)&mask)	   n|=1;
				if (*(data+size/4+1)&mask)	 n|=2;
				if (*(data+size/2+size/4)&mask)    n|=4;
				if (*(data+size/2+size/4+1)&mask)  n|=8;
				dwval|=n<<(28-j*4);
				penusage=1<<n;
				cps1_char_pen_usage[nchar]|=penusage;
				cps1_tile16_pen_usage[nchar/2]|=penusage;
				cps1_tile32_pen_usage[nchar/8]|=penusage;
		   }
           cps1_gfx[2*(i+gfxsize/4)]=dwval;
		   dwval=0;
		   for (j=0; j<8; j++)
		   {
				int n,mask;
				n=0;
				mask=0x80>>j;
				if (*(data)&mask)	  n|=1;
				if (*(data+1)&mask)	n|=2;
				if (*(data+size/2)&mask)   n|=4;
				if (*(data+size/2+1)&mask) n|=8;
				dwval|=n<<(28-j*4);
				penusage=1<<n;
				cps1_char_pen_usage[nchar]|=penusage;
				cps1_tile16_pen_usage[nchar/2]|=penusage;
				cps1_tile32_pen_usage[nchar/8]|=penusage;
		   }
           cps1_gfx[2*(i+gfxsize/4)+1]=dwval;
           data+=4;
		}

	}

    return 0;
}


int cps2_vh_start(void)
{
    if (cps1_vh_start())
    {
        return -1;
    }
    cps1_gfx_stop();
    cps2_gfx_start();

	cps2_start=0;
	cps2_debug=1;	/* Scroll 1 display */
	cps2_width=48;	/* 48 characters wide */

	return 0;
}

void cps2_vh_stop(void)
{
    cps1_vh_stop();
}


void cps1_debug_tiles_f(struct osd_bitmap *bitmap, int layer, int width)
{
    int maxy=width/2;
    int x,y;
	int n=cps2_start;

	/* Blank screen */
    fillbitmap(bitmap, palette_transparent_pen, NULL);

    for (y=0; y<maxy; y++)
    {
        for (x=0;x<width;x++)
        {
            switch (layer)
            {
                case 1:
                    cps1_draw_scroll1(bitmap, n, 0, 0, 0, 32+x*8, 32+y*8, 0xffff);
                    break;
                case 2:
                    cps1_draw_tile16(bitmap, Machine->gfx[2], n, 0, 0, 0, 32+x*16, 32+y*16, 0xffff);
                    break;
                case 3:
                    cps1_draw_tile32(bitmap, Machine->gfx[3], n, 0, 0, 0, 32+x*32, 32+y*32, 0xffff);
                    break;
            }
            n++;
        }
     }

     if (keyboard_pressed(KEYCODE_PGDN))
     {
        cps2_start+=width*maxy;
     }
     if (keyboard_pressed(KEYCODE_PGUP))
     {
        cps2_start-=width*maxy;
     }
     if (cps2_start < 0)
     {
		cps2_start=0;
     }
}


void cps1_debug_tiles(struct osd_bitmap *bitmap)
{

    if (keyboard_pressed(KEYCODE_1))
    {
       cps2_debug=1;
       cps2_start=0;
       cps2_width=48;
    }
    if (keyboard_pressed(KEYCODE_2))
    {
       cps2_debug=2;
       cps2_start=0;
       cps2_width=24;
    }
    if (keyboard_pressed(KEYCODE_3))
    {
       cps2_debug=3;
       cps2_start=0;
       cps2_width=12;
    }


    if (cps2_debug)
    {
        cps1_debug_tiles_f(bitmap, cps2_debug, cps2_width);
    }
}

WRITE_HANDLER( cps2_qsound_sharedram_w );

void cps2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
    static int qcode;
    int stop=0;
    int oldq=qcode;
    int i,offset;

    if (cps1_palette)
    {
        for (i=0; i<cps1_palette_size; i+=2)
        {
            int color=0x0fff+((i&0x0f)<<(8+4));
            WRITE_WORD(&cps1_palette[i],color);
        }
    }

	/* Get video memory base registers */
	cps1_get_video_base();

    cps1_build_palette();
   	for (i = offset = 0; i < cps1_palette_entries; i++)
	{
        int j;
        for (j = 0; j < 15; j++)
        {
           palette_used_colors[offset++] = PALETTE_COLOR_USED;
        }
        palette_used_colors[offset++] = PALETTE_COLOR_TRANSPARENT;
	}

    palette_recalc ();

    cps1_debug_tiles(bitmap);
    if (keyboard_pressed_memory(KEYCODE_UP))
        qcode++;

    if (keyboard_pressed_memory(KEYCODE_DOWN))
        qcode--;

    qcode &= 0xffff;

    if (keyboard_pressed_memory(KEYCODE_ENTER))
        stop=0xff;


    if (qcode != oldq)
    {
        int mode=0;
        cps2_qsound_sharedram_w(0x1ffa, 0x0088);
        cps2_qsound_sharedram_w(0x1ffe, 0xffff);

        cps2_qsound_sharedram_w(0x00, 0x0000);
        cps2_qsound_sharedram_w(0x02, qcode);
        cps2_qsound_sharedram_w(0x06, 0x0000);
        cps2_qsound_sharedram_w(0x08, 0x0000);
        cps2_qsound_sharedram_w(0x0c, mode);
        cps2_qsound_sharedram_w(0x0e, 0x0010);
        cps2_qsound_sharedram_w(0x10, 0x0000);
        cps2_qsound_sharedram_w(0x12, 0x0000);
        cps2_qsound_sharedram_w(0x14, 0x0000);
        cps2_qsound_sharedram_w(0x16, 0x0000);
        cps2_qsound_sharedram_w(0x18, 0x0000);
        cps2_qsound_sharedram_w(0x1e, 0x0000);
    }
    {
    struct DisplayText dt[3];
    char *instructions="PRESS: PGUP/PGDN=CODE  1=8x8  2=16x16  3=32x32  UP/DN=QCODE";
    char text1[256];
    sprintf(text1, "GFX CODE=%06x  :  QSOUND CODE=%04x", cps2_start, qcode );
    dt[0].text = text1;
    dt[0].color = UI_COLOR_INVERSE;
    dt[0].x = (Machine->uiwidth - Machine->uifontwidth * strlen(text1)) / 2;
    dt[0].y = 8*23;
    dt[1].text = instructions;
    dt[1].color = UI_COLOR_NORMAL;
    dt[1].x = (Machine->uiwidth - Machine->uifontwidth * strlen(instructions)) / 2;
    dt[1].y = dt[0].y+2*Machine->uifontheight;

    dt[2].text = 0; /* terminate array */
	displaytext(Machine->scrbitmap,dt,0,0);
    }
}





