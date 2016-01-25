/* video hardware for Namco System II */

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/namcos2.h"

#define ROTATE_TILE_WIDTH   256
#define ROTATE_TILE_HEIGHT  256
#define ROTATE_PIXEL_WIDTH  (ROTATE_TILE_WIDTH*8)
#define ROTATE_PIXEL_HEIGHT (ROTATE_TILE_HEIGHT*8)
#define ROTATE_MASK_WIDTH   (ROTATE_PIXEL_WIDTH-1)
#define ROTATE_MASK_HEIGHT  (ROTATE_PIXEL_HEIGHT-1)
#define get_gfx_pointer(gfxelement,c,line) (gfxelement->gfxdata + (c*gfxelement->height+line) * gfxelement->line_modulo)

#ifdef NAMCOS2_DEBUG_MODE
static int namcos2_used_colour_count=0;
static int namcos2_used_colour_cached=0;
static int namcos2_max_used_colour_count=0;
static int namcos2_max_used_colour_cached=0;
/* extern int palette_max_ran_out; */
/* extern int palette_ran_out; */
#endif

struct tilemap *namcos2_tilemap0=NULL;
struct tilemap *namcos2_tilemap1=NULL;
struct tilemap *namcos2_tilemap2=NULL;
struct tilemap *namcos2_tilemap3=NULL;
struct tilemap *namcos2_tilemap4=NULL;
struct tilemap *namcos2_tilemap5=NULL;

int namcos2_tilemap0_flip=0;
int namcos2_tilemap1_flip=0;
int namcos2_tilemap2_flip=0;
int namcos2_tilemap3_flip=0;
int namcos2_tilemap4_flip=0;
int namcos2_tilemap5_flip=0;

static void namcos2_tilemap0_get_info( int tile_index ){
	int tile,colour;
	tile=READ_WORD(&videoram[0x0000+(tile_index<<1)]);
	/* The tile mask DOESNT use the mangled tile number */
	tile_info.mask_data = memory_region(REGION_GFX4)+(0x08*tile);
	/* The order of bits needs to be corrected to index the right tile  14 15 11 12 13 */
	tile=(tile&0x07ff)|((tile&0xc000)>>3)|((tile&0x3800)<<2);
	colour=namcos2_68k_vram_ctrl_r(0x30)&0x0007;

	SET_TILE_INFO(GFX_CHR,tile,colour)
}

static void namcos2_tilemap1_get_info( int tile_index ){
	int tile,colour;
	tile=READ_WORD(&videoram[0x2000+(tile_index<<1)]);
	/* The tile mask DOESNT use the mangled tile number */
	tile_info.mask_data = memory_region(REGION_GFX4)+0x08*tile;
	/* The order of bits needs to be corrected to index the right tile  14 15 11 12 13 */
	tile=(tile&0x07ff)|((tile&0xc000)>>3)|((tile&0x3800)<<2);
	colour=namcos2_68k_vram_ctrl_r(0x32)&0x0007;
	SET_TILE_INFO(GFX_CHR,tile,colour)
}

static void namcos2_tilemap2_get_info( int tile_index ){
	int tile,colour;
	tile=READ_WORD(&videoram[0x4000+(tile_index<<1)]);
	/* The tile mask DOESNT use the mangled tile number */
	tile_info.mask_data = memory_region(REGION_GFX4)+(0x08*tile);
	/* The order of bits needs to be corrected to index the right tile  14 15 11 12 13 */
	tile=(tile&0x07ff)|((tile&0xc000)>>3)|((tile&0x3800)<<2);
	colour=namcos2_68k_vram_ctrl_r(0x34)&0x0007;
	SET_TILE_INFO(GFX_CHR,tile,colour)
}

static void namcos2_tilemap3_get_info( int tile_index ){
	int tile,colour;
	tile=READ_WORD(&videoram[0x6000+(tile_index<<1)]);
	/* The tile mask DOESNT use the mangled tile number */
	tile_info.mask_data = memory_region(REGION_GFX4)+(0x08*tile);
	/* The order of bits needs to be corrected to index the right tile  14 15 11 12 13 */
	tile=(tile&0x07ff)|((tile&0xc000)>>3)|((tile&0x3800)<<2);
	colour=namcos2_68k_vram_ctrl_r(0x36)&0x0007;
	SET_TILE_INFO(GFX_CHR,tile,colour)
}

static void namcos2_tilemap4_get_info( int tile_index ){
	int tile,colour;
	tile=READ_WORD(&videoram[0x8010+(tile_index<<1)]);
	/* The tile mask DOESNT use the mangled tile number */
	tile_info.mask_data = memory_region(REGION_GFX4)+(0x08*tile);
	/* The order of bits needs to be corrected to index the right tile  14 15 11 12 13 */
	tile=(tile&0x07ff)|((tile&0xc000)>>3)|((tile&0x3800)<<2);
	colour=namcos2_68k_vram_ctrl_r(0x38)&0x0007;
	SET_TILE_INFO(GFX_CHR,tile,colour)
}

static void namcos2_tilemap5_get_info( int tile_index ){
	int tile,colour;
	tile=READ_WORD(&videoram[0x8810+(tile_index<<1)]);
	/* The tile mask DOESNT use the mangled tile number */
	tile_info.mask_data = memory_region(REGION_GFX4)+(0x08*tile);
	/* The order of bits needs to be corrected to index the right tile  14 15 11 12 13 */
	tile=(tile&0x07ff)|((tile&0xc000)>>3)|((tile&0x3800)<<2);
	colour=namcos2_68k_vram_ctrl_r(0x3a)&0x0007;
	SET_TILE_INFO(GFX_CHR,tile,colour)
}

int namcos2_calc_used_pens(int gfx_zone,int tile,char *penused){
	unsigned char* gfxdata=NULL;
	int pix_y=0,pix_x=0;
	int height=0,width=0;
	int pencount=0;

	height=Machine->gfx[gfx_zone]->height;
	width=Machine->gfx[gfx_zone]->width;

	for(pix_y=0;pix_y<height;pix_y++){
		gfxdata=get_gfx_pointer(Machine->gfx[gfx_zone],tile,pix_y);

		for(pix_x=0;pix_x<width;pix_x++){
			penused[(gfxdata[pix_x])>>3]|=1<<(gfxdata[pix_x]&0x07);
		}
	}
//	for(pix_y=0;pix_y<256;pix_y++) if((penused[pix_y>>3])&(1<<(pix_y&0x07))) pencount++;
	return pencount;
}

void namcos2_mark_used_ROZ_colours(void){
	int tile,coloop,colour_code;
	/* Array to mark when a particular tile has had its colours marked  */
	/* so we dont scan it again if its marked in here                   */
	char pen_array[256/8];
	char tile_is_visible_array[0x10000/8];

	/* Rather than scan the whole 256x256 tile array marking used colours */
	/* what we'll do is do a normal rotate of the image and instead of    */
	/* copying any data we'll just use the pixel position to mark the     */
	/* tile as being used and more importantly visible. Then scan the     */
	/* array of visible used tiles and mark the pens of any used tile     */

	/* Blat the used array */
	memset(tile_is_visible_array,0,0x10000/8);

	/* This is a clone of the drawROZ core code */
	{
		int dest_x,dest_x_delta,dest_x_start,dest_x_end,tmp_x;
		int dest_y,dest_y_delta,dest_y_start,dest_y_end,tmp_y;
		int right_dx,right_dy,down_dx,down_dy,start_x,start_y;

		/* These need to be sign extended for arithmetic useage */
		right_dx = namcos2_68k_roz_ctrl_r(0x06);
		right_dy = namcos2_68k_roz_ctrl_r(0x02);
		down_dx  = namcos2_68k_roz_ctrl_r(0x04);
		down_dy  = namcos2_68k_roz_ctrl_r(0x00);
		start_y  = namcos2_68k_roz_ctrl_r(0x0a);
		start_x  = namcos2_68k_roz_ctrl_r(0x08);

		/* Sign extend the deltas */
		if(right_dx&0x8000) right_dx|=0xffff0000;
		if(right_dy&0x8000) right_dy|=0xffff0000;
		if(down_dx &0x8000) down_dx |=0xffff0000;
		if(down_dy &0x8000) down_dy |=0xffff0000;

		/* Correct to 16 bit fixed point from 8/12 bit */
		right_dx <<=8;
		right_dy <<=8;
		down_dx  <<=8;
		down_dy  <<=8;
		start_x  <<=12;
		start_y  <<=12;

		/* Correction factor is needed for the screen offset */
		start_x+=38*right_dx;
		start_y+=38*right_dy;

		if(Machine->orientation & ORIENTATION_SWAP_XY)
		{
			int tmp;
			tmp=right_dx; right_dx=down_dx; down_dx=tmp;
			tmp=right_dy; right_dy=down_dy; down_dy=tmp;
		}

		dest_y_delta=(Machine->orientation & ORIENTATION_FLIP_Y)?-1:1;
		dest_y_start=(Machine->orientation & ORIENTATION_FLIP_Y)?Machine->scrbitmap->height-1:0;
		dest_y_end  =(Machine->orientation & ORIENTATION_FLIP_Y)?-1:Machine->scrbitmap->height;

		dest_x_delta=(Machine->orientation & ORIENTATION_FLIP_X)?-1:1;
		dest_x_start=(Machine->orientation & ORIENTATION_FLIP_X)?Machine->scrbitmap->width-1:0;
		dest_x_end  =(Machine->orientation & ORIENTATION_FLIP_X)?-1:Machine->scrbitmap->width;

		for( dest_y=dest_y_start; dest_y!=dest_y_end; dest_y+=dest_y_delta )
		{
			tmp_x = start_x;
			tmp_y = start_y;

			for( dest_x=dest_x_start; dest_x!=dest_x_end; dest_x+=dest_x_delta )
			{
				int xind=(tmp_x>>16)&ROTATE_MASK_HEIGHT;
				int yind=(tmp_y>>16)&ROTATE_MASK_WIDTH;

				/* First reduce the x/y to tile & x/y subpixels */
				int ram_offset=(((yind>>3)<<8)+(xind>>3))<<1;
				/* Now fetch the tile number from ROZ RAM */
				tile=READ_WORD(&namcos2_68k_roz_ram[ram_offset]);
				/* Mark the tile as being visible */
				tile_is_visible_array[tile>>3]|=1<<(tile&0x07);
				/* Move on a little */
				tmp_x += right_dx;
				tmp_y += right_dy;
			}
			start_x += down_dx;
			start_y += down_dy;
		}
	}
	/* Set the correct colour code */
	colour_code=(namcos2_68k_sprite_bank_r(0)>>8)&0x000f;
	colour_code*=256;

	/* Now we have an array with all visible tiles marked, scan it and mark the used colours */
	for(tile=0;tile<0x10000;tile++)
	{
		/* Check if tile has been done before */
		if( tile_is_visible_array[tile>>3]&(1<<(tile&0x07)) )
		{
			/* Clear the temporary pen usage array */
			memset(pen_array,0,256/8);
			/* Generate pen usage array for this tile on the fly */
			namcos2_calc_used_pens(GFX_ROZ,tile,pen_array);

			/* Process tile used colours */
			for(coloop=0;coloop<256;coloop++)
			{
				/* Is this pen marked by the tile as being used ? */
				if( pen_array[coloop>>3]&(1<<(coloop&0x07)) )
				{
					/* Yes so mark it for the palette manager */
					palette_used_colors[colour_code+coloop] |= PALETTE_COLOR_VISIBLE;
				}
			}
		}
	}
}

void namcos2_mark_used_sprite_colours(void){
	int offset,loop,coloop;
	/* Array to mark when a particular tile has had its colours marked  */
	/* so we dont scan it again if its marked in here                   */
	static char done_array[0x1000/8];
	static char pen_array[256/8];

	/* Blat the used array */
	memset(done_array,0,0x1000/8);

	/* Mark off all of the colour codes used by the sprites */
	offset=(namcos2_68k_sprite_bank_r(0)&0x000f)*(128*8);
	for(loop=0;loop<128;loop++)
	{
		int sizey=(READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+0])>>10)&0x3f;

		/* Sprites are only active if they have a size>0 */
		if(sizey)
		{
			int offset2=READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+2]);
			int offset6=READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+6]);
			int sprn,sprn_done,spr_region,colour_code;

			/* Calulate spr number, region, colour code & the done sprite number */
			sprn=(offset2>>2)&0x7ff;
			sprn_done=sprn;
			sprn_done+=(offset2&0x2000)?0x800:0;
			spr_region=(offset2&0x2000)?GFX_OBJ2:GFX_OBJ1;
			colour_code=256*((offset6>>4)&0x000f);

			if( (done_array[sprn_done>>3]&(1<<(sprn_done&0x07)))==0 )
			{
				/* Clear the temporary pen usage array */
				memset(pen_array,0,256/8);
				/* Generate pen usage array for this tile on the fly */
				namcos2_calc_used_pens(spr_region,sprn,pen_array);

				/* Process tile used colours */
				for(coloop=0;coloop<256;coloop++)
				{
					/* Is this pen marked by the tile as being used ? */
					if( pen_array[coloop>>3]&(1<<(coloop&0x07)) )
					{
						/* Yes so mark it for the palette manager */
						palette_used_colors[colour_code+coloop] |= PALETTE_COLOR_VISIBLE;
					}
				}

				/* Mark the tile as having been done */
				done_array[sprn_done>>3]|=1<<(sprn_done&0x07);
			}
		}
	}
}

int namcos2_vh_start(void){
#ifdef NAMCOS2_DEBUG_MODE
	namcos2_used_colour_count=0;
	namcos2_used_colour_cached=0;
	namcos2_max_used_colour_count=0;
	namcos2_max_used_colour_cached=0;
/*	palette_max_ran_out=0;*/
/*	palette_ran_out=0;*/
#endif
	namcos2_tilemap0 = tilemap_create( namcos2_tilemap0_get_info,tilemap_scan_rows,TILEMAP_BITMASK,8,8,64,64 );
	namcos2_tilemap1 = tilemap_create( namcos2_tilemap1_get_info,tilemap_scan_rows,TILEMAP_BITMASK,8,8,64,64 );
	namcos2_tilemap2 = tilemap_create( namcos2_tilemap2_get_info,tilemap_scan_rows,TILEMAP_BITMASK,8,8,64,64 );
	namcos2_tilemap3 = tilemap_create( namcos2_tilemap3_get_info,tilemap_scan_rows,TILEMAP_BITMASK,8,8,64,64 );
	namcos2_tilemap4 = tilemap_create( namcos2_tilemap4_get_info,tilemap_scan_rows,TILEMAP_BITMASK,8,8,36,28 );
	namcos2_tilemap5 = tilemap_create( namcos2_tilemap5_get_info,tilemap_scan_rows,TILEMAP_BITMASK,8,8,36,28 );

	if( !(namcos2_tilemap0 && namcos2_tilemap1 && namcos2_tilemap2 &&
		  namcos2_tilemap3 && namcos2_tilemap4 && namcos2_tilemap5) )
	return 1; /* insufficient memory */

	/* Setup fixed planes */
	tilemap_set_scrollx( namcos2_tilemap4, 0, 0 );
	tilemap_set_scrolly( namcos2_tilemap4, 0, 0 );
	tilemap_set_scrollx( namcos2_tilemap5, 0, 0 );
	tilemap_set_scrolly( namcos2_tilemap5, 0, 0 );

	namcos2_tilemap0_flip=0;
	namcos2_tilemap1_flip=0;
	namcos2_tilemap2_flip=0;
	namcos2_tilemap3_flip=0;
	namcos2_tilemap4_flip=0;
	namcos2_tilemap5_flip=0;

	tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);

	/* Rotate/Flip the mask ROM */

#ifndef PREROTATE_GFX

//
//  TILEMAP MANAGER SEEMS TO BE ABLE TO COPE OK WITH X/Y FLIPS BUT NOT SWAPXY
//
//	if (Machine->orientation & ORIENTATION_FLIP_Y)
//	{
//		int loopY,tilenum;
//		unsigned char tilecache[8],*tiledata;
//		for(tilenum=0;tilenum<0x10000;tilenum++)
//		{
//			tiledata=memory_region(REGION_GFX4)+(tilenum*0x08);
//			/* Cache tile data */
//			for(loopY=0;loopY<8;loopY++) tilecache[loopY]=tiledata[loopY];
//			/* Flip in Y - write back in reverse */
//			for(loopY=0;loopY<8;loopY++) tiledata[loopY]=tilecache[7-loopY];
//		}
//	}

//	if (Machine->orientation & ORIENTATION_FLIP_X)
//	{
//		int loopX,loopY,tilenum;
//		unsigned char tilecache[8],*tiledata;
//		for(tilenum=0;tilenum<0x10000;tilenum++)
//		{
//			tiledata=memory_region(REGION_GFX4)+(tilenum*0x08);
//			/* Cache tile data */
//			for(loopY=0;loopY<8;loopY++) tilecache[loopY]=tiledata[loopY];
//			/* Wipe source data */
//			for(loopY=0;loopY<8;loopY++) tiledata[loopY]=0;
//			/* Flip in X - do bit reversal */
//			for(loopY=0;loopY<8;loopY++)
//			{
//				for(loopX=0;loopX<8;loopX++)
//				{
//					tiledata[loopY]|=(tilecache[loopY]&(1<<loopX))?(0x80>>loopX):0x00;
//				}
//			}
//		}
//	}

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int loopX,loopY,tilenum;
		unsigned char tilecache[8],*tiledata;

		for(tilenum=0;tilenum<0x10000;tilenum++)
		{
			tiledata=memory_region(REGION_GFX4)+(tilenum*0x08);
			/* Cache tile data */
			for(loopY=0;loopY<8;loopY++) tilecache[loopY]=tiledata[loopY];
			/* Wipe source data */
			for(loopY=0;loopY<8;loopY++) tiledata[loopY]=0;
			/* Swap X/Y data */
			for(loopY=0;loopY<8;loopY++)
			{
				for(loopX=0;loopX<8;loopX++)
				{
					tiledata[loopX]|=(tilecache[loopY]&(0x01<<loopX))?(1<<loopY):0x00;
				}
			}
		}

		/* For some reason the tilemap mgr NEEDS these additional steps */
		/* I've no idea why but it seems to work                        */

		for(tilenum=0;tilenum<0x10000;tilenum++)
		{
			tiledata=memory_region(REGION_GFX4)+(tilenum*0x08);
			/* Cache tile data */
			for(loopY=0;loopY<8;loopY++) tilecache[loopY]=tiledata[loopY];
			/* Flip in Y - write back in reverse */
			for(loopY=0;loopY<8;loopY++) tiledata[loopY]=tilecache[7-loopY];
		}

		for(tilenum=0;tilenum<0x10000;tilenum++)
		{
			tiledata=memory_region(REGION_GFX4)+(tilenum*0x08);
			/* Cache tile data */
			for(loopY=0;loopY<8;loopY++) tilecache[loopY]=tiledata[loopY];
			/* Wipe source data */
			for(loopY=0;loopY<8;loopY++) tiledata[loopY]=0;
			/* Flip in X - do bit reversal */
			for(loopY=0;loopY<8;loopY++)
			{
				for(loopX=0;loopX<8;loopX++)
				{
					tiledata[loopY]|=(tilecache[loopY]&(1<<loopX))?(0x80>>loopX):0x00;
				}
			}
		}
	}

#endif

	return 0;
}


void namcos2_vh_stop(void)
{
	tmpbitmap = 0;
}


void namcos2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	/* Zap the palette to Zero */

	for( i = 0; i < Machine->drv->total_colors; i++ )
	{
		palette[i*3+0] = 0;
		palette[i*3+1] = 0;
		palette[i*3+2] = 0;
	}
}


#ifdef NAMCOS2_DEBUG_MODE

#define TITLE_X         (6*8)
#define RGDISP_X        (5*6)
#define RGDISP_Y        (3*8)
#define RGDISP_Y_SEP    8

static void show_reg(int regbank )
{
	int i,offset;
	char buffer[256];
	int ypos=RGDISP_Y;
	int xpos=RGDISP_X;

	switch (regbank)
	{
		case 1:
			sprintf(buffer,"VRAM Control: %08lx",0x00420000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			for( i=0; i<0x10; i+=2 )
			{
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_vram_ctrl[i]));
				ui_text(buffer,1*RGDISP_X,ypos);

				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_vram_ctrl[0x10+i]));
				ui_text(buffer,2*RGDISP_X,ypos);

				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_vram_ctrl[0x20+i]));
				ui_text(buffer,3*RGDISP_X,ypos);

				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_vram_ctrl[0x30+i]));
				ui_text(buffer,4*RGDISP_X,ypos);

				ypos+=RGDISP_Y_SEP;
			}

			ypos+=RGDISP_Y_SEP;
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"Sprite Bank : %08lx",0x00c40000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"%04x",namcos2_68k_sprite_bank_r(0));
			ui_text(buffer,RGDISP_X,ypos);

			ypos+=RGDISP_Y_SEP;
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"Palette Control: %08lx",0x00443000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			for( i=0; i<0x08; i+=2 )
			{
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_palette_ram[0x3000 + i]));
				ui_text(buffer,1*RGDISP_X,ypos);
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_palette_ram[0x3008 + i]));
				ui_text(buffer,2*RGDISP_X,ypos);
				ypos+=RGDISP_Y_SEP;
			}

			break;
		case 2:
			ypos+=RGDISP_Y_SEP;
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"ROZ Control: %08lx",0x00cc0000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			for( i=0; i<0x08; i+=2 )
			{
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_roz_ctrl[i]));
				ui_text(buffer,1*RGDISP_X,ypos);
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_roz_ctrl[0x08+i]));
				ui_text(buffer,2*RGDISP_X,ypos);
				ypos+=RGDISP_Y_SEP;
			}
			ypos+=RGDISP_Y_SEP;
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"Sprite Bank : %08lx",0x00c40000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"%04x",namcos2_68k_sprite_bank_r(0));
			ui_text(buffer,RGDISP_X,ypos);
			break;
		case 3:
			sprintf(buffer,"Protection Key: %08lx",0x00d00000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			for( i=0; i<0x10; i+=2 )
			{
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_key[i]));
				ui_text(buffer,RGDISP_X,ypos);
				ypos+=RGDISP_Y_SEP;
			}
			break;
		case 4:
			sprintf(buffer,"IRQ Control: %08lx",0x001c0000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			for( i=0; i<0x20; i++ )
			{
				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_master_C148[i]));
				ui_text(buffer,RGDISP_X,ypos);

				sprintf(buffer,"%04x",READ_WORD(&namcos2_68k_slave_C148[i]));
				ui_text(buffer,2*RGDISP_X,ypos);
				ypos+=RGDISP_Y_SEP;
			}
			break;
		case 5:
			sprintf(buffer,"Sprite RAM: %08lx",0x00c00000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"SZ S -Y-  F F B NUM Q  -X-  SZ C P");
			ui_text(buffer,12,ypos);
			ypos+=RGDISP_Y_SEP;
			offset=(namcos2_68k_sprite_bank_r(0)&0x000f)*(128*8);
			for( i=0; i<(0x10*0x08); i+=0x08 )
			{
				xpos=12;

				sprintf(buffer,"%02x %01x %03x  %01x %01x %01x %03x %01x  %03x  %02x %01x %01x",
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x00]))>>0x0a)&0x003f,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x00]))>>0x09)&0x0001,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x00]))>>0x00)&0x01ff,

						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x02]))>>0x0f)&0x0001,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x02]))>>0x0e)&0x0001,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x02]))>>0x0d)&0x0001,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x02]))>>0x02)&0x07ff,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x02]))>>0x00)&0x0003,

						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x04]))>>0x00)&0x03ff,

						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x06]))>>0x0a)&0x003f,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x06]))>>0x04)&0x000f,
						 ((READ_WORD(&namcos2_sprite_ram[offset+i+0x06]))>>0x00)&0x0007);
				ui_text(buffer,xpos,ypos);
				ypos+=RGDISP_Y_SEP;
			}
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"Sprite Bank : %08lx",0x00c40000L);
			ui_text(buffer,TITLE_X,ypos);
			ypos+=RGDISP_Y_SEP;
			sprintf(buffer,"%04x",namcos2_68k_sprite_bank_r(0));
			ui_text(buffer,RGDISP_X,ypos);
			break;
		default:
			break;
	}
}
#endif

INLINE unsigned char fetch_rotated_pixel(int xind, int yind)
{
	unsigned char* gfxdata;
	int ram_offset,pix_x,pix_y,tile;

	/* First reduce the x/y to tile & x/y subpixels */
	ram_offset=(((yind>>3)<<8)+(xind>>3))<<1;
	pix_x=xind&0x07;
	pix_y=yind&0x07;

#ifndef PREROTATE_GFX
	if(Machine->orientation & ORIENTATION_SWAP_XY) { int tmp=pix_x; pix_x=pix_y; pix_y=tmp; }
#endif

	/* Now fetch the tile number from ROZ RAM */
	tile=READ_WORD(&namcos2_68k_roz_ram[ram_offset]);

	/* Now extract the pixel from the tile gfx */
	gfxdata=get_gfx_pointer(Machine->gfx[GFX_ROZ],tile,pix_y);
	return gfxdata[pix_x];
}


static void draw_layerROZ( struct osd_bitmap *dest_bitmap)
{
	const struct GfxElement *rozgfx=Machine->gfx[GFX_ROZ];
	int dest_x,dest_x_delta,dest_x_start,dest_x_end,tmp_x;
	int dest_y,dest_y_delta,dest_y_start,dest_y_end,tmp_y;
	int right_dx,right_dy,down_dx,down_dy,start_x,start_y;
	unsigned short *paldata;
	int colour;

	/* These need to be sign extended for arithmetic useage */
	right_dx = namcos2_68k_roz_ctrl_r(0x06);
	right_dy = namcos2_68k_roz_ctrl_r(0x02);
	down_dx  = namcos2_68k_roz_ctrl_r(0x04);
	down_dy  = namcos2_68k_roz_ctrl_r(0x00);
	start_y  = namcos2_68k_roz_ctrl_r(0x0a);
	start_x  = namcos2_68k_roz_ctrl_r(0x08);

	/* Sign extend the deltas */
	if(right_dx&0x8000) right_dx|=0xffff0000;
	if(right_dy&0x8000) right_dy|=0xffff0000;
	if(down_dx &0x8000) down_dx |=0xffff0000;
	if(down_dy &0x8000) down_dy |=0xffff0000;

	/* Correct to 16 bit fixed point from 8/12 bit */
	right_dx <<=8;
	right_dy <<=8;
	down_dx  <<=8;
	down_dy  <<=8;
	start_x  <<=12;
	start_y  <<=12;

	/* Correction factor is needed for the screen offset */
	start_x+=38*right_dx;
	start_y+=38*right_dy;

	/* Pre-calculate the colour palette array pointer */
	colour=(namcos2_68k_sprite_bank_r(0)>>8)&0x000f;
	paldata = &rozgfx->colortable[rozgfx->color_granularity * colour];

	/* Select correct drawing code based on destination bitmap pixel depth */

	if(dest_bitmap->depth == 16)
	{
		unsigned short *dest_line,pixel;

		if(Machine->orientation & ORIENTATION_SWAP_XY)
		{
			int tmp;
			tmp=right_dx; right_dx=down_dx; down_dx=tmp;
			tmp=right_dy; right_dy=down_dy; down_dy=tmp;
		}

		dest_y_delta=(Machine->orientation & ORIENTATION_FLIP_Y)?-1:1;
		dest_y_start=(Machine->orientation & ORIENTATION_FLIP_Y)?dest_bitmap->height-1:0;
		dest_y_end  =(Machine->orientation & ORIENTATION_FLIP_Y)?-1:dest_bitmap->height;

		dest_x_delta=(Machine->orientation & ORIENTATION_FLIP_X)?-1:1;
		dest_x_start=(Machine->orientation & ORIENTATION_FLIP_X)?dest_bitmap->width-1:0;
		dest_x_end  =(Machine->orientation & ORIENTATION_FLIP_X)?-1:dest_bitmap->width;

		for( dest_y=dest_y_start; dest_y!=dest_y_end; dest_y+=dest_y_delta )
		{
			dest_line = (unsigned short*)dest_bitmap->line[dest_y];
			tmp_x = start_x;
			tmp_y = start_y;

			for( dest_x=dest_x_start; dest_x!=dest_x_end; dest_x+=dest_x_delta )
			{
				int xind=(tmp_x>>16)&ROTATE_MASK_HEIGHT;
				int yind=(tmp_y>>16)&ROTATE_MASK_WIDTH;

				pixel= fetch_rotated_pixel(xind,yind);

				/* Only process non-transparent pixels */
				if(pixel!=0xff)
				{
					/* Now remap the colour space of the pixel and store */
					dest_line[dest_x]=paldata[pixel];
				}
				tmp_x += right_dx;
				tmp_y += right_dy;
			}
			start_x += down_dx;
			start_y += down_dy;
		}
	}
	else
	{
		unsigned char *dest_line,pixel;

		if(Machine->orientation & ORIENTATION_SWAP_XY)
		{
			int tmp;
			tmp=right_dx; right_dx=down_dx; down_dx=tmp;
			tmp=right_dy; right_dy=down_dy; down_dy=tmp;
		}

		dest_y_delta=(Machine->orientation & ORIENTATION_FLIP_Y)?-1:1;
		dest_y_start=(Machine->orientation & ORIENTATION_FLIP_Y)?dest_bitmap->height-1:0;
		dest_y_end  =(Machine->orientation & ORIENTATION_FLIP_Y)?-1:dest_bitmap->height;

		dest_x_delta=(Machine->orientation & ORIENTATION_FLIP_X)?-1:1;
		dest_x_start=(Machine->orientation & ORIENTATION_FLIP_X)?dest_bitmap->width-1:0;
		dest_x_end  =(Machine->orientation & ORIENTATION_FLIP_X)?-1:dest_bitmap->width;

		for( dest_y=dest_y_start; dest_y!=dest_y_end; dest_y+=dest_y_delta )
		{
			dest_line = dest_bitmap->line[dest_y];
			tmp_x = start_x;
			tmp_y = start_y;

			for( dest_x=dest_x_start; dest_x!=dest_x_end; dest_x+=dest_x_delta )
			{
				int xind=(tmp_x>>16)&ROTATE_MASK_HEIGHT;
				int yind=(tmp_y>>16)&ROTATE_MASK_WIDTH;

				pixel= fetch_rotated_pixel(xind,yind);

				/* Only process non-transparent pixels */
				if(pixel!=0xff)
				{
					/* Now remap the colour space of the pixel and store */
					dest_line[dest_x]=paldata[pixel];
				}
				tmp_x += right_dx;
				tmp_y += right_dy;
			}
			start_x += down_dx;
			start_y += down_dy;
		}
	}
}


static void draw_sprites_default( struct osd_bitmap *bitmap, int priority )
{
	int sprn,flipy,flipx,ypos,xpos,sizex,sizey,scalex,scaley;
	int offset,offset0,offset2,offset4,offset6;
	int loop,spr_region;
	struct rectangle rect;

	offset=(namcos2_68k_sprite_bank_r(0)&0x000f)*(128*8);

	for(loop=0;loop < 128;loop++)
	{
		/****************************************
		* Sprite data is 8 byte packed format   *
		*                                       *
		* Offset 0,1                            *
		*   Sprite Y position           D00-D08 *
		*   Sprite Size 16/32           D09     *
		*   Sprite Size Y               D10-D15 *
		*                                       *
		* Offset 2,3                            *
		*   Sprite Quadrant             D00-D01 *
		*   Sprite Number               D02-D12 *
		*   Sprite ROM Bank select      D13     *
		*   Sprite flip X               D14     *
		*   Sprite flip Y               D15     *
		*                                       *
		* Offset 4,5                            *
		*   Sprite X position           D00-D10 *
		*                                       *
		* Offset 6,7                            *
		*   Sprite priority             D00-D02 *
		*   Sprite colour index         D04-D07 *
		*   Sprite Size X               D10-D15 *
		*                                       *
		****************************************/

		offset0 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+0]);
		offset2 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+2]);
		offset4 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+4]);
		offset6 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+6]);

		/* Fetch sprite size registers */

		sizey=((offset0>>10)&0x3f)+1;
		sizex=(offset6>>10)&0x3f;

		if((offset0&0x0200)==0) sizex>>=1;

		if((sizey-1) && sizex && (offset6&0x0007)==priority)
		{
			rect=Machine->visible_area;

			sprn=(offset2>>2)&0x7ff;
			spr_region=(offset2&0x2000)?GFX_OBJ2:GFX_OBJ1;

			ypos=(0x1ff-(offset0&0x01ff))-0x50+0x02;
			xpos=(offset4&0x03ff)-0x50+0x07;

			flipy=offset2&0x8000;
			flipx=offset2&0x4000;

			scalex=((sizex<<16)/((offset0&0x0200)?0x20:0x10));
			scaley=((sizey<<16)/((offset0&0x0200)?0x20:0x10));

			/* Set the clipping rect to mask off the other portion of the sprite */
			rect.min_x=xpos;
			rect.max_x=xpos+(sizex-1);
			rect.min_y=ypos;
			rect.max_y=ypos+(sizey-1);

			if((offset0&0x0200)==0)
			{
				if(((offset2&0x0001) && !flipx) || (!(offset2&0x0001) && flipx)) xpos-=sizex;
				if(((offset2&0x0002) && !flipy) || (!(offset2&0x0002) && flipy)) ypos-=sizey;
			}

			if((scalex==(1<<16)) && (scaley==(1<<16)))
			{
				drawgfx(bitmap,Machine->gfx[spr_region],
					sprn,
					(offset6>>4)&0x000f,     /* Selected colour bank */
					flipx,flipy,
					xpos,ypos,
					&rect,TRANSPARENCY_PEN,0xff);
			}
			else if(scalex && scaley)
			{
				drawgfxzoom(bitmap,Machine->gfx[spr_region],
					sprn,
					(offset6>>4)&0x000f,     /* Selected colour bank */
					flipx,flipy,
					xpos,ypos,
					&rect,TRANSPARENCY_PEN,0xff,
					scalex,scaley);
			}
		}
	}
}


static void draw_sprites_finallap( struct osd_bitmap *bitmap, int priority )
{
	int sprn,flipy,flipx,ypos,xpos,sizex,sizey,scalex,scaley;
	int offset,offset0,offset2,offset4,offset6;
	int loop,spr_region;
	struct rectangle rect;

	offset=(namcos2_68k_sprite_bank_r(0)&0x000f)*(128*8);

	for(loop=0;loop < 128;loop++)
	{
		/****************************************
		* Sprite data is 8 byte packed format   *
		*                                       *
		* Offset 0,1                            *
		*   Sprite Y position           D00-D08 *
		*   Sprite ROM Bank select      D09     * DIFFERENT FROM DEFAULT SPRITES
		*   Sprite Size Y               D10-D15 *
		*                                       *
		* Offset 2,3                            *
		*   Sprite Quadrant             D00-D01 *
		*   Sprite Number               D02-D12 *
		*   Sprite Size 16/32           D13     * DIFFERENT FROM DEFAULT SPRITES
		*   Sprite flip X               D14     *
		*   Sprite flip Y               D15     *
		*                                       *
		* Offset 4,5                            *
		*   Sprite X position           D00-D10 *
		*                                       *
		* Offset 6,7                            *
		*   Sprite priority             D00-D03 * DIFFERENT FROM DEFAULT SPRITES 4 BIT
		*   Sprite colour index         D04-D07 *
		*   Sprite Size X               D10-D15 *
		*                                       *
		****************************************/

		offset0 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+0]);
		offset2 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+2]);
		offset4 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+4]);
		offset6 = READ_WORD(&namcos2_sprite_ram[offset+(loop*8)+6]);

		/* Fetch sprite size registers */

		sizey=((offset0>>10)&0x3f)+1;
		sizex=(offset6>>10)&0x3f;

		if((offset2&0x2000)==0) sizex>>=1;

		if((sizey-1) && sizex && (offset6&0x000f)==priority)
		{
			rect=Machine->visible_area;

			sprn=(offset2>>2)&0x7ff;
			spr_region=(offset0&0x0200)?GFX_OBJ2:GFX_OBJ1;
			spr_region=GFX_OBJ1;	// Always fixed on Final Lap

			if(offset0&0x0200)
			{
				ypos=((offset0&0x0100)?-0x100:0)+(offset0&0x00ff)+0xa8;	// 0x70
				xpos=((offset4&0x0200)?-0x200:0)+(offset4&0x01ff)+0x90;	// 0x90
			}
			else
			{
				ypos=(0x1ff-(offset0&0x01ff))-0x50+0x02;
				xpos=(offset4&0x03ff)-0x50+0x07;
			}


			flipy=offset2&0x8000;
			flipx=offset2&0x4000;

			scalex=((sizex<<16)/((offset2&0x2000)?0x20:0x10));
			scaley=((sizey<<16)/((offset2&0x2000)?0x20:0x10));

			/* Set the clipping rect to mask off the other portion of the sprite */
			rect.min_x=xpos;
			rect.max_x=xpos+(sizex-1);
			rect.min_y=ypos;
			rect.max_y=ypos+(sizey-1);

			if((offset2&0x2000)==0)
			{
				if(((offset2&0x0001) && !flipx) || (!(offset2&0x0001) && flipx)) xpos-=sizex;
				if(((offset2&0x0002) && !flipy) || (!(offset2&0x0002) && flipy)) ypos-=sizey;
			}

			if((scalex==(1<<16)) && (scaley==(1<<16)))
			{
				drawgfx(bitmap,Machine->gfx[spr_region],
					sprn,
					(offset6>>4)&0x000f,     /* Selected colour bank */
					flipx,flipy,
					xpos,ypos,
					&rect,TRANSPARENCY_PEN,0xff);
			}
			else if(scalex && scaley)
			{
				drawgfxzoom(bitmap,Machine->gfx[spr_region],
					sprn,
					(offset6>>4)&0x000f,     /* Selected colour bank */
					flipx,flipy,
					xpos,ypos,
					&rect,TRANSPARENCY_PEN,0xff,
					scalex,scaley);
			}
		}
	}
}

void namcos2_vh_update_default(struct osd_bitmap *bitmap, int full_refresh)
{
	int priority;
	static int show[10] = {1,1,1,1,1,1,1,1,1,1};

	tilemap_update(namcos2_tilemap0);
	tilemap_update(namcos2_tilemap1);
	tilemap_update(namcos2_tilemap2);
	tilemap_update(namcos2_tilemap3);
	tilemap_update(namcos2_tilemap4);
	tilemap_update(namcos2_tilemap5);

	/* Only piss around with the palette if we are in 8 bit mode as 16 bit */
	/* mode has a direct mapping and doesnt need palette management        */
	{
		/* Initialise palette_used_colors array */
		palette_init_used_colors();

		/* Mark any colours in the palette_used_colors array */
		/* Only process ROZ if she is enabled */
		if(((namcos2_68k_sprite_bank_r(0)>>12)&0x07)>0) namcos2_mark_used_ROZ_colours();
		/* Finally the sprites */
		namcos2_mark_used_sprite_colours();

		if (palette_recalc())
		{
			/* Mark all planes as dirty */
			tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
		}
		tilemap_render(ALL_TILEMAPS);
	}


	/* Scrub the bitmap clean */
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	/* Render the screen */
	for(priority=0;priority<=7;priority++)
	{
		if((namcos2_68k_vram_ctrl_r(0x20)&0x07)==priority && show[0]) tilemap_draw(bitmap,namcos2_tilemap0,0);
		if((namcos2_68k_vram_ctrl_r(0x22)&0x07)==priority && show[1]) tilemap_draw(bitmap,namcos2_tilemap1,0);
		if((namcos2_68k_vram_ctrl_r(0x24)&0x07)==priority && show[2]) tilemap_draw(bitmap,namcos2_tilemap2,0);
		if((namcos2_68k_vram_ctrl_r(0x26)&0x07)==priority && show[3]) tilemap_draw(bitmap,namcos2_tilemap3,0);
		if((namcos2_68k_vram_ctrl_r(0x28)&0x07)==priority && show[4]) tilemap_draw(bitmap,namcos2_tilemap4,0);
		if((namcos2_68k_vram_ctrl_r(0x2a)&0x07)==priority && show[5]) tilemap_draw(bitmap,namcos2_tilemap5,0);

		/* Draw ROZ if enabled */
		if(priority>=1 && ((namcos2_68k_sprite_bank_r(0)>>12)&0x07)==priority && show[6]) draw_layerROZ(bitmap);

		/* Sprites */
		draw_sprites_default( bitmap,priority );
	}

#ifdef NAMCOS2_DEBUG_MODE
	/* NAMCOS2 Video debugging */
	if(keyboard_pressed(KEYCODE_Z))     { while( keyboard_pressed(KEYCODE_Z));     show[0]=(show[0])?0:1;  }
	if(keyboard_pressed(KEYCODE_X))     { while( keyboard_pressed(KEYCODE_X));     show[1]=(show[1])?0:1;  }
	if(keyboard_pressed(KEYCODE_C))     { while( keyboard_pressed(KEYCODE_C));     show[2]=(show[2])?0:1;  }
	if(keyboard_pressed(KEYCODE_V))     { while( keyboard_pressed(KEYCODE_V));     show[3]=(show[3])?0:1;  }
	if(keyboard_pressed(KEYCODE_B))     { while( keyboard_pressed(KEYCODE_B));     show[4]=(show[4])?0:1;  }
	if(keyboard_pressed(KEYCODE_N))     { while( keyboard_pressed(KEYCODE_N));     show[5]=(show[5])?0:1;  }
	if(keyboard_pressed(KEYCODE_COMMA)) { while( keyboard_pressed(KEYCODE_COMMA)); show[6]=(show[6])?0:1;  }

	if(!show[0] || !show[1] || !show[2] || !show[3] || !show[4] || !show[5] || !show[6])
	{
		char buffer[256];
		sprintf(buffer,"Planes %d%d%d%d %d%d %d",show[0],show[1],show[2],show[3],show[4],show[5],show[6]);
		ui_text(buffer,4,4);
	}

	if(0)
	{
		char buffer[256];
		int loop;
		namcos2_used_colour_count=0;
		namcos2_used_colour_cached=0;
		for(loop=0;loop<8192;loop++) if(palette_used_colors[loop]&PALETTE_COLOR_VISIBLE) namcos2_used_colour_count++;
		for(loop=0;loop<8192;loop++) if(palette_used_colors[loop]&PALETTE_COLOR_CACHED) namcos2_used_colour_cached++;
		if(namcos2_used_colour_count>namcos2_max_used_colour_count) namcos2_max_used_colour_count=namcos2_used_colour_count;
		if(namcos2_used_colour_cached>namcos2_max_used_colour_cached) namcos2_max_used_colour_cached=namcos2_used_colour_cached;

		sprintf(buffer,"Pens   = %03d (%03d)",namcos2_used_colour_count,namcos2_max_used_colour_count);
		ui_text(buffer,4,12);
		sprintf(buffer,"Cached = %03d (%03d)",namcos2_used_colour_cached,namcos2_max_used_colour_cached);
		ui_text(buffer,4,20);
		/* sprintf(buffer,"RunOut = %03d (%03d)",palette_ran_out,palette_max_ran_out); */
		/* ui_text(buffer,4,28); */
		sprintf(buffer,"Depth  = %d",Machine->scrbitmap->depth);
		ui_text(buffer,4,36);
	}

	{
		static int regshow = 0;
		if(keyboard_pressed(KEYCODE_L)) { while( keyboard_pressed(KEYCODE_L)); regshow++;if(regshow>5) regshow=0; }
		if(regshow) show_reg(regshow);
	}
#endif
}


void namcos2_vh_update_finallap(struct osd_bitmap *bitmap, int full_refresh)
{
	int priority;
	static int show[10] = {1,1,1,1,1,1,1,1,1,1};

	tilemap_update(namcos2_tilemap0);
	tilemap_update(namcos2_tilemap1);
	tilemap_update(namcos2_tilemap2);
	tilemap_update(namcos2_tilemap3);
	tilemap_update(namcos2_tilemap4);
	tilemap_update(namcos2_tilemap5);

	/* Only piss around with the palette if we are in 8 bit mode as 16 bit */
	/* mode has a direct mapping and doesnt need palette management        */
	{
		/* Initialise palette_used_colors array */
		palette_init_used_colors();

		/* Mark any colours in the palette_used_colors array */
		namcos2_mark_used_sprite_colours();

		if (palette_recalc())
		{
			/* Mark all planes as dirty */
			tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
		}
		tilemap_render(ALL_TILEMAPS);
	}


	/* Scrub the bitmap clean */
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	/* Render the screen */
	for(priority=0;priority<=15;priority++)
	{
		if((namcos2_68k_vram_ctrl_r(0x20)&0x0f)==priority && show[0]) tilemap_draw(bitmap,namcos2_tilemap0,0);
		if((namcos2_68k_vram_ctrl_r(0x22)&0x0f)==priority && show[1]) tilemap_draw(bitmap,namcos2_tilemap1,0);
		if((namcos2_68k_vram_ctrl_r(0x24)&0x0f)==priority && show[2]) tilemap_draw(bitmap,namcos2_tilemap2,0);
		if((namcos2_68k_vram_ctrl_r(0x26)&0x0f)==priority && show[3]) tilemap_draw(bitmap,namcos2_tilemap3,0);
		if((namcos2_68k_vram_ctrl_r(0x28)&0x0f)==priority && show[4]) tilemap_draw(bitmap,namcos2_tilemap4,0);
		if((namcos2_68k_vram_ctrl_r(0x2a)&0x0f)==priority && show[5]) tilemap_draw(bitmap,namcos2_tilemap5,0);
		/* Not sure if priority should be 0x07 or 0x0f */

		/* Sprites */
		draw_sprites_finallap( bitmap,priority );
	}


	/********************************************************************/
	/*                                                                  */
	/* Final Lap roadway implementation notes                           */
	/*                                                                  */
	/* namcos2_68k_roadtile_ram                                         */
	/* ------------------------                                         */
	/* Overall tilemap looks to be 64 wide x 512 deep in terms of tiles */
	/* this gives and overall dimension of 512 x 4096 pixels. This is   */
	/* very large and it may be that it uses double buffering which     */
	/* would halve the depth of the buffer.                             */
	/*                                                                  */
	/* Each row of the tilemap is made of 64 words (128 bytes), the     */
	/* first byte most likely holds the colour+attr, the second byte    */
	/* contains the tilenumber, this references a RAM based tile        */
	/* definition within namcos2_68k_roadgfx_ram, tiles are 8x8 8bpp.   */
	/*                                                                  */
	/* namcos2_68k_roadgfx_ram                                          */
	/* -----------------------                                          */
	/* This ram holds the gfx data for the 8x8 tiles in 8bpp format.    */
	/* The data is arranged in 2 x 32 byte blocks with each block       */
	/* arranged in a 4 x 8 byte pattern.                                */
	/*                                                                  */
	/* Bxxby == Byte <xx> bit <y>                                       */
	/*                                                                  */
	/* So Pixel 0,0 = B00b0 B01b0 B02b0 B03b0 B32b0 B33b0 B34b0 B35b0   */
	/*    Pixel 0,1 = B00b1 B01b1 B02b1 B03b1 B32b1 B33b1 B34b1 B35b1   */
	/*                          ..................                      */
	/*    Pixel 0,7 = B00b7 B01b7 B02b7 B03b7 B32b7 B33b7 B34b7 B35b7   */
	/*                                                                  */
	/*    Pixel 1,0 = B04b0 B05b0 B06b0 B07b0 B36b0 B37b0 B38b0 B39b0   */
	/*                                                                  */
	/********************************************************************/

	/* Flat rendering of the road tilemap */
	if(0)
	{
		int loop,linel,data;
		unsigned char *dest_line;
		for(loop=0;loop<28*8;loop++)
		{
			dest_line = bitmap->line[loop];
			for(linel=0;linel<128;linel+=2)
			{
				data=READ_WORD(&namcos2_68k_roadtile_ram[(loop*128)+linel]);
				*(dest_line++)=(data&0x000f)>>0;
				*(dest_line++)=(data&0x00f0)>>4;
				*(dest_line++)=(data&0x0f00)>>8;
				*(dest_line++)=(data&0xf000)>>12;
			}
		}
	}

#ifdef NAMCOS2_DEBUG_MODE
	/* NAMCOS2 Video debugging */
	if(keyboard_pressed(KEYCODE_Z))     { while( keyboard_pressed(KEYCODE_Z));     show[0]=(show[0])?0:1;  }
	if(keyboard_pressed(KEYCODE_X))     { while( keyboard_pressed(KEYCODE_X));     show[1]=(show[1])?0:1;  }
	if(keyboard_pressed(KEYCODE_C))     { while( keyboard_pressed(KEYCODE_C));     show[2]=(show[2])?0:1;  }
	if(keyboard_pressed(KEYCODE_V))     { while( keyboard_pressed(KEYCODE_V));     show[3]=(show[3])?0:1;  }
	if(keyboard_pressed(KEYCODE_B))     { while( keyboard_pressed(KEYCODE_B));     show[4]=(show[4])?0:1;  }
	if(keyboard_pressed(KEYCODE_N))     { while( keyboard_pressed(KEYCODE_N));     show[5]=(show[5])?0:1;  }

	if(!show[0] || !show[1] || !show[2] || !show[3] || !show[4] || !show[5])
	{
		char buffer[256];
		sprintf(buffer,"Planes %d%d%d%d %d%d",show[0],show[1],show[2],show[3],show[4],show[5]);
		ui_text(buffer,4,4);
	}

	{
		static int regshow = 0;
		if(keyboard_pressed(KEYCODE_L)) { while( keyboard_pressed(KEYCODE_L)); regshow++;if(regshow>5) regshow=0; }
		if(regshow) show_reg(regshow);
	}

	{
		if(keyboard_pressed(KEYCODE_S))
		{
			FILE *f=NULL;
			int i;

			while( keyboard_pressed(KEYCODE_S));
			f = fopen ("finallap.log", "w");
			fprintf (f, "Sprite Memory (Bank 0)\n");
			for (i = 0; i < 0x80; i++)
				fprintf (f, "%04x %04x %04x %04x\n",
					READ_WORD(&namcos2_sprite_ram[(i*8)+0]),
					READ_WORD(&namcos2_sprite_ram[(i*8)+2]),
					READ_WORD(&namcos2_sprite_ram[(i*8)+4]),
					READ_WORD(&namcos2_sprite_ram[(i*8)+6]));
			fprintf (f, "\n\n\n\n");

			fprintf (f, "Sprite Memory (Bank 1)\n");
			for (i = 0; i < 0x80; i++)
				fprintf (f, "%04x %04x %04x %04x\n",
					READ_WORD(&namcos2_sprite_ram[0x400+(i*8)+0]),
					READ_WORD(&namcos2_sprite_ram[0x400+(i*8)+2]),
					READ_WORD(&namcos2_sprite_ram[0x400+(i*8)+4]),
					READ_WORD(&namcos2_sprite_ram[0x400+(i*8)+6]));
			fprintf (f, "\n\n\n\n");

			fprintf (f, "Road Tile Memory\n");
			for (i = 0; i < 0x800; i++)
			{
				if((i%4)==0) fprintf(f,"0x%03x    ",i/4);
				fprintf (f, "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x ",
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x00]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x02]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x04]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x06]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x08]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x0a]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x0c]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x0e]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x10]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x12]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x14]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x16]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x18]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x1a]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x1c]),
					READ_WORD(&namcos2_68k_roadtile_ram[(i*32)+0x1e]));
				if((i%4)==3) fprintf(f,"\n");
			}
			fprintf (f, "\n\n\n\n");

			fclose(f);
		}
	}
#endif
}
