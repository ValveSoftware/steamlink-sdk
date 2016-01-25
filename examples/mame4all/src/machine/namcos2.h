/***************************************************************************

  namcos2.h

  Common functions & declarations for the Namco System 2 driver

***************************************************************************/

// #define NAMCOS2_DEBUG_MODE


/* CPU reference numbers */

#define NAMCOS2_CPU1	0
#define NAMCOS2_CPU2	1
#define NAMCOS2_CPU3	2
#define NAMCOS2_CPU4	3

#define CPU_MASTER	NAMCOS2_CPU1
#define CPU_SLAVE	NAMCOS2_CPU2
#define CPU_SOUND	NAMCOS2_CPU3
#define CPU_MCU 	NAMCOS2_CPU4

/* VIDHRDW */

#define GFX_OBJ1	0
#define GFX_OBJ2	1
#define GFX_CHR 	2
#define GFX_ROZ 	3

/*********************************************/
/* IF GAME SPECIFIC HACKS ARE REQUIRED THEN  */
/* USE THE namcos2_gametype VARIABLE TO FIND */
/* OUT WHAT GAME IS RUNNING 				 */
/*********************************************/

#define NAMCOS2_ASSAULT 			0x1000
#define NAMCOS2_ASSAULT_JP			0x1001
#define NAMCOS2_ASSAULT_PLUS		0x1002
#define NAMCOS2_BUBBLE_TROUBLE		0x1003
#define NAMCOS2_BURNING_FORCE		0x1004
#define NAMCOS2_COSMO_GANG			0x1005
#define NAMCOS2_COSMO_GANG_US		0x1006
#define NAMCOS2_DIRT_FOX			0x1007
#define NAMCOS2_DIRT_FOX_JP 		0x1008
#define NAMCOS2_DRAGON_SABER		0x1009
#define NAMCOS2_DRAGON_SABER_JP 	0x100a
#define NAMCOS2_FINAL_LAP			0x100b
#define NAMCOS2_FINAL_LAP_2 		0x100c
#define NAMCOS2_FINAL_LAP_3 		0x100d
#define NAMCOS2_FINEST_HOUR 		0x100e
#define NAMCOS2_FOUR_TRAX			0x100f
#define NAMCOS2_GOLLY_GHOST 		0x1010
#define NAMCOS2_LUCKY_AND_WILD		0x1011
#define NAMCOS2_MARVEL_LAND 		0x1012
#define NAMCOS2_METAL_HAWK			0x1013
#define NAMCOS2_MIRAI_NINJA 		0x1014
#define NAMCOS2_ORDYNE				0x1015
#define NAMCOS2_PHELIOS 			0x1016
#define NAMCOS2_ROLLING_THUNDER_2	0x1017
#define NAMCOS2_STEEL_GUNNER		0x1018
#define NAMCOS2_STEEL_GUNNER_2		0x1019
#define NAMCOS2_SUPER_WSTADIUM		0x101a
#define NAMCOS2_SUPER_WSTADIUM_92	0x101b
#define NAMCOS2_SUPER_WSTADIUM_93	0x101c
#define NAMCOS2_SUZUKA_8_HOURS		0x101d
#define NAMCOS2_SUZUKA_8_HOURS_2	0x101e
#define NAMCOS2_VALKYRIE			0x101f
#define NAMCOS2_KYUUKAI_DOUCHUUKI	0x1020

extern int namcos2_gametype;

/*********************************************/

int  namcos2_vh_start(void);
void namcos2_vh_stop(void);
void namcos2_vh_update_default(struct osd_bitmap *bitmap, int full_refresh);
void namcos2_vh_update_finallap(struct osd_bitmap *bitmap, int full_refresh);
void namcos2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

/* MACHINE */

void namcos2_init_machine(void);


/**************************************************************/
/* Dual port memory handlers								  */
/**************************************************************/
READ_HANDLER( namcos2_dpram_byte_r );
WRITE_HANDLER( namcos2_dpram_byte_w );
READ_HANDLER( namcos2_68k_dpram_word_r );
WRITE_HANDLER( namcos2_68k_dpram_word_w );

extern unsigned char *namcos2_dpram;

/**************************************************************/
/* Sprite memory handlers									  */
/**************************************************************/
WRITE_HANDLER( namcos2_68k_sprite_ram_w );
WRITE_HANDLER( namcos2_68k_sprite_bank_w );
READ_HANDLER( namcos2_68k_sprite_ram_r );
READ_HANDLER( namcos2_68k_sprite_bank_r );

extern unsigned char *namcos2_sprite_ram;
extern int namcos2_sprite_bank;


/**************************************************************/
/*	EEPROM memory function handlers 						  */
/**************************************************************/
#define NAMCOS2_68K_EEPROM_W	namcos2_68k_eeprom_w, &namcos2_eeprom, &namcos2_eeprom_size
#define NAMCOS2_68K_EEPROM_R	namcos2_68k_eeprom_r
void	namcos2_nvram_handler(void *file, int read_or_write);
WRITE_HANDLER( 	namcos2_68k_eeprom_w );
READ_HANDLER( 	namcos2_68k_eeprom_r );
extern unsigned char *namcos2_eeprom;
extern size_t namcos2_eeprom_size;

/**************************************************************/
/*	Shared video memory function handlers					  */
/**************************************************************/
WRITE_HANDLER( namcos2_68k_vram_w );
READ_HANDLER( namcos2_68k_vram_r );

extern size_t namcos2_68k_vram_size;

READ_HANDLER( namcos2_68k_vram_ctrl_r );
WRITE_HANDLER( namcos2_68k_vram_ctrl_w );

extern unsigned char namcos2_68k_vram_ctrl[];

extern struct tilemap *namcos2_tilemap0;
extern struct tilemap *namcos2_tilemap1;
extern struct tilemap *namcos2_tilemap2;
extern struct tilemap *namcos2_tilemap3;
extern struct tilemap *namcos2_tilemap4;
extern struct tilemap *namcos2_tilemap5;

extern int namcos2_tilemap0_flip;
extern int namcos2_tilemap1_flip;
extern int namcos2_tilemap2_flip;
extern int namcos2_tilemap3_flip;
extern int namcos2_tilemap4_flip;
extern int namcos2_tilemap5_flip;

/**************************************************************/
/*	Shared video palette function handlers					  */
/**************************************************************/
READ_HANDLER( namcos2_68k_video_palette_r );
WRITE_HANDLER( namcos2_68k_video_palette_w );

#define NAMCOS2_COLOUR_CODES	0x20
extern unsigned char *namcos2_68k_palette_ram;
extern size_t namcos2_68k_palette_size;


/**************************************************************/
/*	Shared data ROM memory handlerhandlers					  */
/**************************************************************/
READ_HANDLER( namcos2_68k_data_rom_r );


/**************************************************************/
/* Shared serial communications processory (CPU5 ????)		  */
/**************************************************************/
READ_HANDLER( namcos2_68k_serial_comms_ram_r );
WRITE_HANDLER( namcos2_68k_serial_comms_ram_w );
READ_HANDLER( namcos2_68k_serial_comms_ctrl_r );
WRITE_HANDLER( namcos2_68k_serial_comms_ctrl_w );

extern unsigned char  namcos2_68k_serial_comms_ctrl[0x10];
extern unsigned char *namcos2_68k_serial_comms_ram;



/**************************************************************/
/* Shared protection/random number generator				  */
/**************************************************************/
READ_HANDLER( namcos2_68k_key_r );
WRITE_HANDLER( namcos2_68k_key_w );

extern unsigned char namcos2_68k_key[];

READ_HANDLER( namcos2_68k_protect_r );
WRITE_HANDLER( namcos2_68k_protect_w );

extern int namcos2_68k_protect;



/**************************************************************/
/* Non-shared memory custom IO device - IRQ/Inputs/Outputs	 */
/**************************************************************/

#define NAMCOS2_C148_0			0		/* 0x1c0000 */
#define NAMCOS2_C148_1			1
#define NAMCOS2_C148_2			2
#define NAMCOS2_C148_CPUIRQ 	3
#define NAMCOS2_C148_EXIRQ		4		/* 0x1c8000 */
#define NAMCOS2_C148_POSIRQ 	5
#define NAMCOS2_C148_SERIRQ 	6
#define NAMCOS2_C148_VBLANKIRQ	7

extern int	namcos2_68k_master_C148[32];
extern int	namcos2_68k_slave_C148[32];

WRITE_HANDLER( namcos2_68k_master_C148_w );
READ_HANDLER( namcos2_68k_master_C148_r );
int  namcos2_68k_master_vblank( void );
void namcos2_68k_master_posirq( int moog );

WRITE_HANDLER( namcos2_68k_slave_C148_w );
READ_HANDLER( namcos2_68k_slave_C148_r );
int  namcos2_68k_slave_vblank(void);
void namcos2_68k_slave_posirq( int moog );


/**************************************************************/
/* MASTER CPU RAM MEMORY									  */
/**************************************************************/

extern unsigned char *namcos2_68k_master_ram;

#define NAMCOS2_68K_MASTER_RAM_W	MWA_BANK3, &namcos2_68k_master_ram
#define NAMCOS2_68K_MASTER_RAM_R	MRA_BANK3


/**************************************************************/
/* SLAVE CPU RAM MEMORY 									  */
/**************************************************************/

extern unsigned char *namcos2_68k_slave_ram;

#define NAMCOS2_68K_SLAVE_RAM_W 	MWA_BANK4, &namcos2_68k_slave_ram
#define NAMCOS2_68K_SLAVE_RAM_R 	MRA_BANK4


/**************************************************************/
/*	ROZ - Rotate & Zoom memory function handlers			  */
/**************************************************************/

WRITE_HANDLER( namcos2_68k_roz_ctrl_w );
READ_HANDLER( namcos2_68k_roz_ctrl_r );
extern unsigned char namcos2_68k_roz_ctrl[];

WRITE_HANDLER( namcos2_68k_roz_ram_w );
READ_HANDLER( namcos2_68k_roz_ram_r );
extern size_t namcos2_68k_roz_ram_size;
extern unsigned char *namcos2_68k_roz_ram;


/**************************************************************/
/* FINAL LAP road generator definitions.....				  */
/**************************************************************/

WRITE_HANDLER( namcos2_68k_roadtile_ram_w );
READ_HANDLER( namcos2_68k_roadtile_ram_r );
extern unsigned char *namcos2_68k_roadtile_ram;
extern size_t namcos2_68k_roadtile_ram_size;

WRITE_HANDLER( namcos2_68k_roadgfx_ram_w );
READ_HANDLER( namcos2_68k_roadgfx_ram_r );
extern unsigned char *namcos2_68k_roadgfx_ram;
extern size_t namcos2_68k_roadgfx_ram_size;

WRITE_HANDLER( namcos2_68k_road_ctrl_w );
READ_HANDLER( namcos2_68k_road_ctrl_r );


/**************************************************************/
/*															  */
/**************************************************************/
#define BANKED_SOUND_ROM_R		MRA_BANK6
#define CPU3_ROM1				6			/* Bank number */



/**************************************************************/
/* Sound CPU support handlers - 6809						  */
/**************************************************************/

int  namcos2_sound_interrupt(void);
WRITE_HANDLER( namcos2_sound_bankselect_w );


/**************************************************************/
/* MCU Specific support handlers - HD63705					  */
/**************************************************************/

int  namcos2_mcu_interrupt(void);

WRITE_HANDLER( namcos2_mcu_analog_ctrl_w );
READ_HANDLER( namcos2_mcu_analog_ctrl_r );

WRITE_HANDLER( namcos2_mcu_analog_port_w );
READ_HANDLER( namcos2_mcu_analog_port_r );

WRITE_HANDLER( namcos2_mcu_port_d_w );
READ_HANDLER( namcos2_mcu_port_d_r );

READ_HANDLER( namcos2_input_port_0_r );
READ_HANDLER( namcos2_input_port_10_r );
READ_HANDLER( namcos2_input_port_12_r );
