/***************************************************************************

Namco System II

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m6805/m6805.h"
#include "machine/namcos2.h"
#include "vidhrdw/generic.h"

unsigned char *namcos2_68k_master_ram=NULL;
unsigned char *namcos2_68k_slave_ram=NULL;
unsigned char *namcos2_68k_mystery_ram=NULL;

int namcos2_gametype=0;

/*************************************************************/
/* Perform basic machine initialisation 					 */
/*************************************************************/

void namcos2_init_machine(void)
{
	int loop;

	if(namcos2_dpram==NULL) namcos2_dpram = (unsigned char*)malloc(0x800);
	memset( namcos2_dpram, 0, 0x800 );

	if(namcos2_sprite_ram==NULL) namcos2_sprite_ram = (unsigned char*)malloc(0x4000);
	memset( namcos2_sprite_ram, 0, 0x4000 );
	namcos2_sprite_bank=0;

	if(namcos2_68k_serial_comms_ram==NULL) namcos2_68k_serial_comms_ram = (unsigned char*)malloc(0x4000);
	memset( namcos2_68k_serial_comms_ram, 0, 0x4000 );

	/* Initialise the bank select in the sound CPU */
	namcos2_sound_bankselect_w(0,0);		/* Page in bank 0 */

	/* Place CPU2 & CPU3 into the reset condition */
	namcos2_68k_master_C148_w(0x1e2000-0x1c0000,0);
	namcos2_68k_master_C148_w(0x1e4000-0x1c0000,0);

	/* Initialise interrupt handlers */
	for(loop=0;loop<20;loop++)
	{
		namcos2_68k_master_C148[loop]=0;
		namcos2_68k_slave_C148[loop]=0;
	}

	/* Initialise the video control registers */
	for(loop=0;loop<0x40;loop+=2) namcos2_68k_vram_ctrl_w(loop,0);

	/* Initialise ROZ */
	for(loop=0;loop<0x10;loop+=2) namcos2_68k_roz_ctrl_w(loop,0);

	/* Initialise the Roadway generator */
//	for(loop=0;loop<0x10;loop+=2) namcos2_68k_road_ctrl_w(loop,0);
}

/*************************************************************/
/* EEPROM Load/Save and read/write handling 				 */
/*************************************************************/

unsigned char *namcos2_eeprom;
size_t namcos2_eeprom_size;

void namcos2_nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite (file, namcos2_eeprom, namcos2_eeprom_size);
	else
	{
		if (file)
			osd_fread (file, namcos2_eeprom, namcos2_eeprom_size);
		else
			memset (namcos2_eeprom, 0xff, namcos2_eeprom_size);
	}
}

WRITE_HANDLER( namcos2_68k_eeprom_w )
{
	int oldword = READ_WORD (&namcos2_eeprom[offset]);
	int newword = COMBINE_WORD (oldword, data);
	WRITE_WORD (&namcos2_eeprom[offset], newword);
}

READ_HANDLER( namcos2_68k_eeprom_r )
{
	return READ_WORD(&namcos2_eeprom[offset]);
}

/*************************************************************/
/* 68000 Shared memory area - Data ROM area 				 */
/*************************************************************/

READ_HANDLER( namcos2_68k_data_rom_r )
{
	unsigned char *ROM=memory_region(REGION_USER1);
	return READ_WORD(&ROM[offset]);
}


/*************************************************************/
/* 68000 Shared memory area - Video RAM control 			 */
/*************************************************************/

size_t namcos2_68k_vram_size;

WRITE_HANDLER( namcos2_68k_vram_w )
{
//	int col=(offset>>1)&0x3f;
//	int row=(offset>>7)&0x3f;

	COMBINE_WORD_MEM(&videoram[offset],data);

	/* Some games appear to use the 409000 region as SRAM to */
	/* communicate between master/slave processors ??		 */

	if(offset<0x9000)
	{
		switch(offset&0xe000)
		{
			case 0x0000:
//				if(namcos2_tilemap0_flip&TILEMAP_FLIPX) col=63-col;
//				if(namcos2_tilemap0_flip&TILEMAP_FLIPY) row=63-row;
//				tilemap_mark_tile_dirty(namcos2_tilemap0,col,row);
				tilemap_mark_tile_dirty(namcos2_tilemap0,(offset>>1)&0xfff);
				break;

			case 0x2000:
//				if(namcos2_tilemap1_flip&TILEMAP_FLIPX) col=63-col;
//				if(namcos2_tilemap1_flip&TILEMAP_FLIPY) row=63-row;
//				tilemap_mark_tile_dirty(namcos2_tilemap1,col,row);
				tilemap_mark_tile_dirty(namcos2_tilemap1,(offset>>1)&0xfff);
				break;

			case 0x4000:
//				if(namcos2_tilemap2_flip&TILEMAP_FLIPX) col=63-col;
//				if(namcos2_tilemap2_flip&TILEMAP_FLIPY) row=63-row;
//				tilemap_mark_tile_dirty(namcos2_tilemap2,col,row);
				tilemap_mark_tile_dirty(namcos2_tilemap2,(offset>>1)&0xfff);
				break;

			case 0x6000:
//				if(namcos2_tilemap3_flip&TILEMAP_FLIPX) col=63-col;
//				if(namcos2_tilemap3_flip&TILEMAP_FLIPY) row=63-row;
//				tilemap_mark_tile_dirty(namcos2_tilemap3,col,row);
				tilemap_mark_tile_dirty(namcos2_tilemap3,(offset>>1)&0xfff);
				break;

			case 0x8000:
				if(offset>=0x8010 && offset<0x87f0)
				{
					offset-=0x10;	/* Fixed plane offsets */
//					row=((offset&0x7ff)>>1)/36;
//					col=((offset&0x7ff)>>1)%36;
//					if(namcos2_tilemap4_flip&TILEMAP_FLIPX) col=35-col;
//					if(namcos2_tilemap4_flip&TILEMAP_FLIPY) row=27-row;
//					tilemap_mark_tile_dirty(namcos2_tilemap4,col,row);
					tilemap_mark_tile_dirty(namcos2_tilemap4,(offset&0x7ff)>>1);
				}
				else if(offset>=0x8810 && offset<0x8ff0)
				{
					offset-=0x10;	/* Fixed plane offsets */
//					row=((offset&0x7ff)>>1)/36;
//					col=((offset&0x7ff)>>1)%36;
//					if(namcos2_tilemap5_flip&TILEMAP_FLIPX) col=35-col;
//					if(namcos2_tilemap5_flip&TILEMAP_FLIPY) row=27-row;
//					tilemap_mark_tile_dirty(namcos2_tilemap5,col,row);
					tilemap_mark_tile_dirty(namcos2_tilemap5,(offset&0x7ff)>>1);
				}
				break;

			default:
				break;
		}
	}
}

READ_HANDLER( namcos2_68k_vram_r )
{
	int data=READ_WORD(&videoram[offset]);
	return data;
}



unsigned char namcos2_68k_vram_ctrl[0x40];

WRITE_HANDLER( namcos2_68k_vram_ctrl_w )
{
	int olddata=namcos2_68k_vram_ctrl_r(offset&0x3f)&0x0f;
	int newdata=data&0x0f;
	int flip;

	/* Write word to register */
	COMBINE_WORD_MEM(&namcos2_68k_vram_ctrl[offset&0x3f],data);

	switch(offset&0x3f)
	{
		case 0x02:
			/* All planes are flipped X+Y from D15 of this word */
			flip=(namcos2_68k_vram_ctrl_r(0x02)&0x8000)?(TILEMAP_FLIPX|TILEMAP_FLIPY):0;
			if(namcos2_tilemap0_flip!=flip) tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);

			tilemap_set_flip( ALL_TILEMAPS, flip );
//			namcos2_tilemap0_flip=flip;
//			namcos2_tilemap1_flip=flip;
//			namcos2_tilemap2_flip=flip;
//			namcos2_tilemap3_flip=flip;
//			namcos2_tilemap4_flip=flip;
//			namcos2_tilemap5_flip=flip;

			tilemap_set_scrollx( namcos2_tilemap0, 0, (data+44+4)&0x1ff );
			break;
		case 0x06:
			tilemap_set_scrolly( namcos2_tilemap0, 0, (data+24)&0x1ff );
			break;
		case 0x0a:
			tilemap_set_scrollx( namcos2_tilemap1, 0, (data+44+2)&0x1ff );
			break;
		case 0x0e:
			tilemap_set_scrolly( namcos2_tilemap1, 0, (data+24)&0x1ff );
			break;
		case 0x12:
			tilemap_set_scrollx( namcos2_tilemap2, 0, (data+44+1)&0x1ff );
			break;
		case 0x16:
			tilemap_set_scrolly( namcos2_tilemap2, 0, (data+24)&0x1ff );
			break;
		case 0x1a:
			tilemap_set_scrollx( namcos2_tilemap3, 0, (data+44)&0x1ff );
			break;
		case 0x1e:
			tilemap_set_scrolly( namcos2_tilemap3, 0, (data+24)&0x1ff );
			break;
		case 0x30:
			/* Change of colour bank needs to force a tilemap redraw */
			if(newdata!=olddata) tilemap_mark_all_tiles_dirty(namcos2_tilemap0);
			break;
		case 0x32:
			/* Change of colour bank needs to force a tilemap redraw */
			if(newdata!=olddata) tilemap_mark_all_tiles_dirty(namcos2_tilemap1);
			break;
		case 0x34:
			/* Change of colour bank needs to force a tilemap redraw */
			if(newdata!=olddata) tilemap_mark_all_tiles_dirty(namcos2_tilemap2);
			break;
		case 0x36:
			/* Change of colour bank needs to force a tilemap redraw */
			if(newdata!=olddata) tilemap_mark_all_tiles_dirty(namcos2_tilemap3);
			break;
		case 0x38:
			/* Change of colour bank needs to force a tilemap redraw */
			if(newdata!=olddata) tilemap_mark_all_tiles_dirty(namcos2_tilemap4);
			break;
		case 0x3a:
			/* Change of colour bank needs to force a tilemap redraw */
			if(newdata!=olddata) tilemap_mark_all_tiles_dirty(namcos2_tilemap5);
			break;
		default:
			break;
	}
}

READ_HANDLER( namcos2_68k_vram_ctrl_r )
{
	return READ_WORD(&namcos2_68k_vram_ctrl[offset&0x3f]);
}


/*************************************************************/
/* 68000 Shared memory area - Video palette control 		 */
/*************************************************************/

unsigned char *namcos2_68k_palette_ram;
size_t namcos2_68k_palette_size;

READ_HANDLER( namcos2_68k_video_palette_r )
{
	if((offset&0xf000)==0x3000)
	{
		/* Palette chip control registers */
		offset&=0x001f;
		switch(offset)
		{
			case 0x1a:
			case 0x1e:
				return 0xff;
				break;
			default:
				break;
		}
	}
	return READ_WORD(&namcos2_68k_palette_ram[offset&0xffff]);
}

WRITE_HANDLER( namcos2_68k_video_palette_w )
{
	int oldword = READ_WORD(&namcos2_68k_palette_ram[offset&0xffff]);
	int newword = COMBINE_WORD(oldword, data);
	int pen,red,green,blue;

	if(oldword != newword)
	{
		WRITE_WORD(&namcos2_68k_palette_ram[offset&0xffff],newword);

		/* 0x3000 offset is control registers */
		if((offset&0x3000)!=0x3000)
		{
			pen=(((offset&0xc000)>>2) | (offset&0x0fff))>>1;

			red  =(READ_WORD(&namcos2_68k_palette_ram[offset&0xcfff]))&0x00ff;
			green=(READ_WORD(&namcos2_68k_palette_ram[(offset&0xcfff)+0x1000]))&0x00ff;
			blue =(READ_WORD(&namcos2_68k_palette_ram[(offset&0xcfff)+0x2000]))&0x00ff;

			palette_change_color(pen,red,green,blue);
		}
	}
}

/*************************************************************/
/* 68000/6809/63705 Shared memory area - DUAL PORT Memory	 */
/*************************************************************/

unsigned char *namcos2_dpram=NULL;	/* 2Kx8 */

READ_HANDLER( namcos2_68k_dpram_word_r )
{
	offset = offset/2;
	return namcos2_dpram[offset&0x7ff];
}

WRITE_HANDLER( namcos2_68k_dpram_word_w )
{
	offset = offset/2;
	if(!(data & 0x00ff0000)) namcos2_dpram[offset&0x7ff] = data & 0xff;
}

READ_HANDLER( namcos2_dpram_byte_r )
{
	return namcos2_dpram[offset&0x7ff];
}

WRITE_HANDLER( namcos2_dpram_byte_w )
{
	namcos2_dpram[offset&0x7ff] = data;
}


/**************************************************************/
/* 68000 Shared serial communications processor (CPU5?) 	  */
/**************************************************************/

unsigned char  namcos2_68k_serial_comms_ctrl[0x10];
unsigned char *namcos2_68k_serial_comms_ram=NULL;

READ_HANDLER( namcos2_68k_serial_comms_ram_r )
{
	//logerror("Serial Comms read  Addr=%08x\n",offset);
	return READ_WORD(&namcos2_68k_serial_comms_ram[offset&0x3fff]);
}

WRITE_HANDLER( namcos2_68k_serial_comms_ram_w )
{
	COMBINE_WORD_MEM(&namcos2_68k_serial_comms_ram[offset&0x3fff],data&0x1ff);
	//logerror("Serial Comms write Addr=%08x, Data=%04x\n",offset,data);
}


READ_HANDLER( namcos2_68k_serial_comms_ctrl_r )
{
	int retval=READ_WORD(&namcos2_68k_serial_comms_ctrl[offset&0x0f]);

	switch(offset)
	{
		case 0x00:
			retval|=0x0004; 	/* Set READY? status bit */
			break;
		default:
			break;
	}
//	logerror("Serial Comms read  Addr=%08x\n",offset);

	return retval;
}

WRITE_HANDLER( namcos2_68k_serial_comms_ctrl_w )
{
	COMBINE_WORD_MEM(&namcos2_68k_serial_comms_ctrl[offset&0x0f],data);
//	logerror("Serial Comms write Addr=%08x, Data=%04x\n",offset,data);
}



/*************************************************************/
/* 68000 Shared SPRITE/OBJECT Memory access/control 		 */
/*************************************************************/

/* The sprite bank register also holds the colour bank for */
/* the ROZ memory and some of the priority control data    */

unsigned char *namcos2_sprite_ram=NULL;
int namcos2_sprite_bank=0;

WRITE_HANDLER( namcos2_68k_sprite_ram_w )
{
	COMBINE_WORD_MEM(&namcos2_sprite_ram[offset&0x3fff],data);
}

WRITE_HANDLER( namcos2_68k_sprite_bank_w )
{
	int newword = COMBINE_WORD(namcos2_sprite_bank, data);
	namcos2_sprite_bank=newword;
}

READ_HANDLER( namcos2_68k_sprite_ram_r )
{
	int data=READ_WORD(&namcos2_sprite_ram[offset&0x3fff]);
	return data;
}

READ_HANDLER( namcos2_68k_sprite_bank_r )
{
	return namcos2_sprite_bank;
}



/*************************************************************/
/* 68000 Shared protection/random key generator 			 */
/*************************************************************/
//
//$d00000
//$d00002
//$d00004	  Write 13 x $0000, read back $00bd from $d00002 (burnf)
//$d00006	  Write $b929, read $014a (cosmog, does a jmp $0 if it doesnt get this)
//$d00008	  Write $13ec, read $013f (rthun2)
//$d0000a	  Write $f00f, read $f00f (phelios)
//$d0000c	  Write $8fc8, read $00b2 (rthun2)
//$d0000e	  Write $31ad, read $00bd (burnf)
//

unsigned char namcos2_68k_key[0x10];

READ_HANDLER( namcos2_68k_key_r )
{
/*	return READ_WORD(&namcos2_68k_key[offset&0x0f]); */
	return rand()&0xffff;
}

WRITE_HANDLER( namcos2_68k_key_w )
{
	/* Seed the random number generator */
	srand(data);
	COMBINE_WORD_MEM(&namcos2_68k_key[offset&0x0f],data);
}


/**************************************************************/
/*	ROZ - Rotate & Zoom memory function handlers			  */
/*															  */
/*	ROZ control made up of 8 registers, looks to be split	  */
/*	into 2 groups of 3 registers one for each plane and two   */
/*	other registers ??										  */
/*															  */
/*	0 - Plane 2 Offset 0x20000								  */
/*	2 - Plane 2 											  */
/*	4 - Plane 2 											  */
/*	6 - Plane 1 Offset 0x00000								  */
/*	8 - Plane 1 											  */
/*	A	Plane 1 											  */
/*	C - Unused												  */
/*	E - Control reg ??										  */
/*															  */
/**************************************************************/

unsigned char namcos2_68k_roz_ctrl[0x10];
size_t namcos2_68k_roz_ram_size;
unsigned char *namcos2_68k_roz_ram=NULL;

WRITE_HANDLER( namcos2_68k_roz_ctrl_w )
{
	COMBINE_WORD_MEM(&namcos2_68k_roz_ctrl[offset&0x0f],data);
}

READ_HANDLER( namcos2_68k_roz_ctrl_r )
{
	return READ_WORD(&namcos2_68k_roz_ctrl[offset&0x0f]);
}

WRITE_HANDLER( namcos2_68k_roz_ram_w )
{
	COMBINE_WORD_MEM(&namcos2_68k_roz_ram[offset],data);
}

READ_HANDLER( namcos2_68k_roz_ram_r )
{
	return READ_WORD(&namcos2_68k_roz_ram[offset]);
}



/**************************************************************/
/*															  */
/*	Final Lap 1/2/3 Roadway generator function handlers 	  */
/*															  */
/**************************************************************/

unsigned char *namcos2_68k_roadtile_ram=NULL;
unsigned char *namcos2_68k_roadgfx_ram=NULL;
size_t namcos2_68k_roadtile_ram_size;
size_t namcos2_68k_roadgfx_ram_size;

WRITE_HANDLER( namcos2_68k_roadtile_ram_w )
{
	COMBINE_WORD_MEM(&namcos2_68k_roadtile_ram[offset],data);
}

READ_HANDLER( namcos2_68k_roadtile_ram_r )
{
	return READ_WORD(&namcos2_68k_roadtile_ram[offset]);
}

WRITE_HANDLER( namcos2_68k_roadgfx_ram_w )
{
	COMBINE_WORD_MEM(&namcos2_68k_roadgfx_ram[offset],data);
}

READ_HANDLER( namcos2_68k_roadgfx_ram_r )
{
	return READ_WORD(&namcos2_68k_roadgfx_ram[offset]);
}

WRITE_HANDLER( namcos2_68k_road_ctrl_w )
{
}

READ_HANDLER( namcos2_68k_road_ctrl_r )
{
	return 0;
}


/*************************************************************/
/* 68000 Interrupt/IO Handlers - CUSTOM 148 - NOT SHARED	 */
/*************************************************************/

#define NO_OF_LINES 	256
#define FRAME_TIME		(1.0/60.0)
#define LINE_LENGTH 	(FRAME_TIME/NO_OF_LINES)

int  namcos2_68k_master_C148[0x20];
int  namcos2_68k_slave_C148[0x20];

WRITE_HANDLER( namcos2_68k_master_C148_w )
{
	offset+=0x1c0000;
	offset&=0x1fe000;

	data&=0x0007;
	namcos2_68k_master_C148[(offset>>13)&0x1f]=data;

	switch(offset)
	{
		case 0x1d4000:
			/* Trigger Master to Slave interrupt */
//			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_CPUIRQ], ASSERT_LINE);
			break;
		case 0x1d6000:
			/* Clear Slave to Master*/
//			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_CPUIRQ], CLEAR_LINE);
			break;
		case 0x1de000:
			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_VBLANKIRQ], CLEAR_LINE);
			break;
		case 0x1da000:
			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_POSIRQ], CLEAR_LINE);
			break;

		case 0x1e2000:				/* Reset register CPU3 */
			{
				data&=0x01;
				if(data&0x01)
				{
					/* Resume execution */
					cpu_set_reset_line (NAMCOS2_CPU3, CLEAR_LINE);
					cpu_yield();
				}
				else
				{
					/* Suspend execution */
					cpu_set_reset_line(NAMCOS2_CPU3, ASSERT_LINE);
				}
			}
			break;
		case 0x1e4000:				/* Reset register CPU2 & CPU4 */
			{
				data&=0x01;
				if(data&0x01)
				{
					/* Resume execution */
					cpu_set_reset_line(NAMCOS2_CPU2, CLEAR_LINE);
					cpu_set_reset_line(NAMCOS2_CPU4, CLEAR_LINE);
					/* Give the new CPU an immediate slice of the action */
					cpu_yield();
				}
				else
				{
					/* Suspend execution */
					cpu_set_reset_line(NAMCOS2_CPU2, ASSERT_LINE);
					cpu_set_reset_line(NAMCOS2_CPU4, ASSERT_LINE);
				}
			}
			break;
		case 0x1e6000:					/* Watchdog reset */
			/* watchdog_reset_w(0,0); */
			break;
		default:
			break;
	}
}


READ_HANDLER( namcos2_68k_master_C148_r )
{
	offset+=0x1c0000;
	offset&=0x1fe000;

	switch(offset)
	{
		case 0x1d6000:
			/* Clear Slave to Master*/
//			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_CPUIRQ], CLEAR_LINE);
			break;
		case 0x1de000:
			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_VBLANKIRQ], CLEAR_LINE);
			break;
		case 0x1da000:
			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_POSIRQ], CLEAR_LINE);
			break;
		case 0x1e0000:					/* EEPROM Status register*/
			return 0xffff;				/* Only BIT0 used: 1=EEPROM READY 0=EEPROM BUSY */
			break;
		case 0x1e6000:					/* Watchdog reset */
			/* watchdog_reset_w(0,0); */
			break;
		default:
			break;
	}
	return namcos2_68k_master_C148[(offset>>13)&0x1f];
}

int namcos2_68k_master_vblank(void)
{
	/* If the POS interrupt is running then set it at half way thru the frame */
	if(namcos2_68k_master_C148[NAMCOS2_C148_POSIRQ])
	{
		timer_set(TIME_IN_NSEC(LINE_LENGTH*100), 0, namcos2_68k_master_posirq);
	}

	/* Assert the VBLANK interrupt */
	return namcos2_68k_master_C148[NAMCOS2_C148_VBLANKIRQ];
}

void namcos2_68k_master_posirq( int moog )
{
	cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_POSIRQ], ASSERT_LINE);
	cpu_set_irq_line(CPU_SLAVE , namcos2_68k_slave_C148[NAMCOS2_C148_POSIRQ] , ASSERT_LINE);
}


WRITE_HANDLER( namcos2_68k_slave_C148_w )
{
	offset+=0x1c0000;
	offset&=0x1fe000;

	data&=0x0007;
	namcos2_68k_slave_C148[(offset>>13)&0x1f]=data;

	switch(offset)
	{
		case 0x1d4000:
			/* Trigger Slave to Master interrupt */
//			cpu_set_irq_line(CPU_MASTER, namcos2_68k_master_C148[NAMCOS2_C148_CPUIRQ], ASSERT_LINE);
			break;
		case 0x1d6000:
			/* Clear Master to Slave */
//			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_CPUIRQ], CLEAR_LINE);
			break;
		case 0x1de000:
			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_VBLANKIRQ], CLEAR_LINE);
			break;
		case 0x1da000:
			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_POSIRQ], CLEAR_LINE);
			break;
		case 0x1e6000:					/* Watchdog reset */
			/* watchdog_reset_w(0,0); */
			break;
		default:
			break;
	}
}


READ_HANDLER( namcos2_68k_slave_C148_r )
{
	offset+=0x1c0000;
	offset&=0x1fe000;

	switch(offset)
	{
		case 0x1d6000:
			/* Clear Master to Slave */
//			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_CPUIRQ], CLEAR_LINE);
			break;
		case 0x1de000:
			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_VBLANKIRQ], CLEAR_LINE);
			break;
		case 0x1da000:
			cpu_set_irq_line(CPU_SLAVE, namcos2_68k_slave_C148[NAMCOS2_C148_POSIRQ], CLEAR_LINE);
			break;
		case 0x1e6000:					/* Watchdog reset */
			/* watchdog_reset_w(0,0); */
			break;
		default:
			break;
	}
	return namcos2_68k_slave_C148[(offset>>13)&0x1f];
}

int namcos2_68k_slave_vblank(void)
{
	return namcos2_68k_slave_C148[NAMCOS2_C148_VBLANKIRQ];
}

/**************************************************************/
/*	Sound sub-system										  */
/**************************************************************/

int namcos2_sound_interrupt(void)
{
	return M6809_INT_FIRQ;
}


WRITE_HANDLER( namcos2_sound_bankselect_w )
{
	unsigned char *RAM=memory_region(REGION_CPU3);
	int bank = ( data >> 4 ) & 0x0f;	/* 991104.CAB */
	cpu_setbank( CPU3_ROM1, &RAM[ 0x10000 + ( 0x4000 * bank ) ] );
}



/**************************************************************/
/*															  */
/*	68705 IO CPU Support functions							  */
/*															  */
/**************************************************************/

int namcos2_mcu_interrupt(void)
{
	return HD63705_INT_IRQ;
}

static int namcos2_mcu_analog_ctrl=0;
static int namcos2_mcu_analog_data=0xaa;
static int namcos2_mcu_analog_complete=0;

WRITE_HANDLER( namcos2_mcu_analog_ctrl_w )
{
	namcos2_mcu_analog_ctrl=data&0xff;

	/* Check if this is a start of conversion */
	/* Input ports 2 thru 9 are the analog channels */
	if(data&0x40)
	{
		/* Set the conversion complete flag */
		namcos2_mcu_analog_complete=2;
		/* We convert instantly, good eh! */
		switch((data>>2)&0x07)
		{
			case 0:
				namcos2_mcu_analog_data=input_port_2_r(0);
				break;
			case 1:
				namcos2_mcu_analog_data=input_port_3_r(0);
				break;
			case 2:
				namcos2_mcu_analog_data=input_port_4_r(0);
				break;
			case 3:
				namcos2_mcu_analog_data=input_port_5_r(0);
				break;
			case 4:
				namcos2_mcu_analog_data=input_port_6_r(0);
				break;
			case 5:
				namcos2_mcu_analog_data=input_port_7_r(0);
				break;
			case 6:
				namcos2_mcu_analog_data=input_port_8_r(0);
				break;
			case 7:
				namcos2_mcu_analog_data=input_port_9_r(0);
				break;
		}
#if 0
		/* Perform the offset handling on the input port */
		/* this converts it to a twos complement number */
		if ((namcos2_gametype==NAMCOS2_DIRT_FOX) ||
			(namcos2_gametype==NAMCOS2_DIRT_FOX_JP))
		namcos2_mcu_analog_data^=0x80;
#endif

		/* If the interrupt enable bit is set trigger an A/D IRQ */
		if(data&0x20)
		{
			cpu_set_irq_line( CPU_MCU, HD63705_INT_ADCONV , PULSE_LINE);
		}
	}
}

READ_HANDLER( namcos2_mcu_analog_ctrl_r )
{
	int data=0;

	/* ADEF flag is only cleared AFTER a read from control THEN a read from DATA */
	if(namcos2_mcu_analog_complete==2) namcos2_mcu_analog_complete=1;
	if(namcos2_mcu_analog_complete) data|=0x80;

	/* Mask on the lower 6 register bits, Irq EN/Channel/Clock */
	data|=namcos2_mcu_analog_ctrl&0x3f;
	/* Return the value */
	return data;
}

WRITE_HANDLER( namcos2_mcu_analog_port_w )
{
}

READ_HANDLER( namcos2_mcu_analog_port_r )
{
	if(namcos2_mcu_analog_complete==1) namcos2_mcu_analog_complete=0;
	return namcos2_mcu_analog_data;
}

WRITE_HANDLER( namcos2_mcu_port_d_w )
{
	/* Undefined operation on write */
}

READ_HANDLER( namcos2_mcu_port_d_r )
{
	/* Provides a digital version of the analog ports */
	int threshold=0x7f;
	int data=0;

	/* Read/convert the bits one at a time */
	if(input_port_2_r(0)>threshold) data|=0x01;
	if(input_port_3_r(0)>threshold) data|=0x02;
	if(input_port_4_r(0)>threshold) data|=0x04;
	if(input_port_5_r(0)>threshold) data|=0x08;
	if(input_port_6_r(0)>threshold) data|=0x10;
	if(input_port_7_r(0)>threshold) data|=0x20;
	if(input_port_8_r(0)>threshold) data|=0x40;
	if(input_port_9_r(0)>threshold) data|=0x80;

	/* Return the result */
	return data;
}

READ_HANDLER( namcos2_input_port_0_r )
{
	int data=readinputport(0);

	int one_joy_trans0[2][10]={
        {0x05,0x01,0x09,0x08,0x0a,0x02,0x06,0x04,0x12,0x14},
        {0x00,0x20,0x20,0x20,0x08,0x08,0x00,0x08,0x02,0x02}};

	int datafake, i;

	switch(namcos2_gametype)
	{
		case NAMCOS2_ASSAULT:
		case NAMCOS2_ASSAULT_JP:
		case NAMCOS2_ASSAULT_PLUS:
			datafake=~readinputport(15) & 0xff;
			//logerror("xxx=%08x\n",datafake);
			for (i=0;i<10;i++)
				if (datafake==one_joy_trans0[0][i])
				{
					data&=~one_joy_trans0[1][i];
					break;
				}
	}
	return data;
}

READ_HANDLER( namcos2_input_port_10_r )
{
	int data=readinputport(10);

	int one_joy_trans10[2][10]={
        {0x05,0x01,0x09,0x08,0x0a,0x02,0x06,0x04,0x1a,0x18},
        {0x08,0x08,0x00,0x02,0x00,0x02,0x02,0x08,0x80,0x80}};

	int datafake, i;

	switch(namcos2_gametype)
	{
		case NAMCOS2_ASSAULT:
		case NAMCOS2_ASSAULT_JP:
		case NAMCOS2_ASSAULT_PLUS:
			datafake=~readinputport(15) & 0xff;
			for (i=0;i<10;i++)
				if (datafake==one_joy_trans10[0][i])
				{
					data&=~one_joy_trans10[1][i];
					break;
				}
	}
	return data;
}

READ_HANDLER( namcos2_input_port_12_r )
{
	int data=readinputport(12);

	int one_joy_trans12[2][4]={
        {0x12,0x14,0x11,0x18},
        {0x02,0x08,0x08,0x02}};

	int datafake, i;

	switch(namcos2_gametype)
	{
		case NAMCOS2_ASSAULT:
		case NAMCOS2_ASSAULT_JP:
		case NAMCOS2_ASSAULT_PLUS:
			datafake=~readinputport(15) & 0xff;
			for (i=0;i<4;i++)
				if (datafake==one_joy_trans12[0][i])
				{
					data&=~one_joy_trans12[1][i];
					break;
				}
	}
	return data;
}
