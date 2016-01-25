#include "../machine/namcos2.cpp"
#include "../vidhrdw/namcos2.cpp"

/***************************************************************************

Namco System II driver by K.Wilkins  (Jun1998, Oct1999)

Email: kwns2@dysfunction.demon.co.uk

The Namco System II board is a 5 ( only 4 are emulated ) CPU system. The
complete system consists of two boards: CPU + GRAPHICS. It contains a large
number of custom ASICs to perform graphics operations, there is no
documentation available for these parts. There is an Atari Manual for ASSAULT
on www.spies.com that contains scans of the schematics.

The system is extremely powerful and flexible. A standard CPU board is coupled
with a number of different graphics boards to produce a system.



CPU Board details
=================

CPU BOARD - Master/Slave CPU, Sound CPU, I/O CPU, Serial I/F CPU
			Text/Scrolling layer generation and video pixel generator.
			Sound Generation.

CPU1 - Master CPU				(68K)
CPU2 - Slave CPU				(68K)
CPU3 - Sound/IO engine			(6809)
CPU4 - IO Microcontroller		(63705) Dips/input ports
CPU5 - Serial I/F Controller	(??? - Not emulated)

The 4 CPU's are all connected via a central 2KByte dual port SRAM. The two
68000s are on one side and the 6809/63705 are on the other side.

Each 68000 has its own private bus area AND a common shared area between the
two devices, which is where the video ram/dual port/Sprite Generation etc
logic sits.

So far only 1 CPU board variant has been identified, unlike the GFX board...

All sound circuitry is contained on the CPU board, it consists of:
	YM2151
	C140 (24 Channel Stereo PCM Sample player)

The CPU board also contains the frame timing and video image generation
circuitry along with text/scroll planes and the palette generator. The system
has 8192 pens of which 4096+2048 are displayable at any given time. These
pens refernce a 24 bit colour lookup table (R8:G8:B8).

The text/tile plane generator has the following capabilities:

2 x Static tile planes (36x28 tiles)
4 x Scolling tile planes (64x64 tiles)

Each plane has its own colour index (8 total) that is used alongside the
pen number to be looked up in the pen index and generate a 24 bit pixel. Each
plane also has its own priority level.

The video image generator receives a pixel stream from the graphics board
which contains:

		PEN NUMBER
		COLOUR BANK
		PIXEL PRIORITY

This stream is then combined with the stream from the text plane pixel
generator with the highest priority pixel being displayed on screen.


Graphics Board details
======================

There are at least 3 known variants of graphics board that all have their own
unique capabilities and separate memory map definition. The PCB outputs a pixel
stream to the main PCB board via one of the system connectors.


	Standard Namco System 2
		1 x Rotate/Zoom tile plane (256x256 tiles)
		128 Sprites (128 Sprites displayable, but 16 banks of 128 sprites)

	Metal Hawk
		2 x Rotate/Zoom tile planes (256x256 tiles)
		??? Sprites

	Steel Gunner 2
		Capabilities unknown at this time

	Final Lap (1/2/3)
		The Racing games definately have a different GFX board to the standard
		one. The sprite tile layout is differnet and the GFX board has a
		separate roadway generator chip. The tiles for the roadway are all held
		in RAM on on the GFX board.




Memory Map
==========

The Dual 68000 Shared memory map area is shown below, this is taken from the memory
decoding pal from the Cosmo Gang board.


#############################################################
#															#
#		MASTER 68000 PRIVATE MEMORY AREA (MAIN PCB) 		#
#															#
#############################################################
# Function						   Address		  R/W  DATA #
#############################################################
Program ROM 					   000000-03FFFF  R    D00-D15

Program RAM 					   100000-10FFFF  R/W  D00-D15

EEPROM							   180000-183FFF  R/W  D00-D07

Interrupt Controller C148		   1C0000-1FFFFF  R/W  D00-D02
	????????					   1C0XXX
	????????					   1C2XXX
	????????					   1C4XXX
	Master/Slave IRQ level		   1C6XXX			   D00-D02
	EXIRQ level 				   1C8XXX			   D00-D02
	POSIRQ level				   1CAXXX			   D00-D02
	SCIRQ level 				   1CCXXX			   D00-D02
	VBLANK IRQ level			   1CEXXX			   D00-D02
	????????					   1D0XXX
	Acknowlegde Master/Slave IRQ   1D6XXX
	Acknowledge EXIRQ			   1D8XXX
	Acknowledge POSIRQ			   1DAXXX
	Acknowledge SCIRQ			   1DCXXX
	Acknowledge VBLANK IRQ		   1DEXXX
	EEPROM Ready status 		   1E0XXX		  R    D01
	Sound CPU Reset control 	   1E2XXX			W  D01
	Slave 68000 & IO CPU Reset	   1E4XXX			W  D01
	Watchdog reset kicker		   1E6XXX			W



#############################################################
#															#
#		 SLAVE 68000 PRIVATE MEMORY AREA (MAIN PCB) 		#
#															#
#############################################################
# Function						   Address		  R/W  DATA #
#############################################################
Program ROM 					   000000-03FFFF  R    D00-D15

Program RAM 					   100000-10FFFF  R/W  D00-D15

Interrupt Controller C148		   1C0000-1FFFFF  R/W  D00-D02
	????????					   1C0XXX
	????????					   1C2XXX
	????????					   1C4XXX
	Master/Slave IRQ level		   1C6XXX			   D00-D02
	EXIRQ level 				   1C8XXX			   D00-D02
	POSIRQ level				   1CAXXX			   D00-D02
	SCIRQ level 				   1CCXXX			   D00-D02
	VBLANK IRQ level			   1CEXXX			   D00-D02
	????????					   1D0XXX
	Acknowlegde Master/Slave IRQ   1D6XXX
	Acknowledge EXIRQ			   1D8XXX
	Acknowledge POSIRQ			   1DAXXX
	Acknowledge SCIRQ			   1DCXXX
	Acknowledge VBLANK IRQ		   1DEXXX
	Watchdog reset kicker		   1E6XXX			W




#############################################################
#															#
#			SHARED 68000 MEMORY AREA (MAIN PCB) 			#
#															#
#############################################################
# Function						   Address		  R/W  DATA #
#############################################################
Data ROMS 0-1					   200000-2FFFFF  R    D00-D15

Data ROMS 2-3					   300000-3FFFFF  R    D00-D15

Screen memory for text planes	   400000-41FFFF  R/W  D00-D15

Screen control registers		   420000-43FFFF  R/W  D00-D15

	Scroll plane 0 - X offset	   42XX02			W  D00-D11
	Scroll plane 0 - X flip 	   42XX02			W  D15

	??????						   42XX04			W  D14-D15

	Scroll plane 0 - Y offset	   42XX06			W  D00-D11
	Scroll plane 0 - Y flip 	   42XX06			W  D15

	??????						   42XX08			W  D14-D15

	Scroll plane 1 - X offset	   42XX0A			W  D00-D11
	Scroll plane 1 - X flip 	   42XX0A			W  D15

	??????						   42XX0C			W  D14-D15

	Scroll plane 1 - Y offset	   42XX0E			W  D00-D11
	Scroll plane 1 - Y flip 	   42XX0E			W  D15

	??????						   42XX10			W  D14-D15

	Scroll plane 2 - X offset	   42XX12			W  D00-D11
	Scroll plane 2 - X flip 	   42XX12			W  D15

	??????						   42XX14			W  D14-D15

	Scroll plane 2 - Y offset	   42XX16			W  D00-D11
	Scroll plane 2 - Y flip 	   42XX16			W  D15

	??????						   42XX18			W  D14-D15

	Scroll plane 3 - X offset	   42XX1A			W  D00-D11
	Scroll plane 3 - X flip 	   42XX1A			W  D15

	??????						   42XX1C			W  D14-D15

	Scroll plane 3 - Y offset	   42XX1E			W  D00-D11
	Scroll plane 3 - Y flip 	   42XX1E			W  D15

	Scroll plane 0 priority 	   42XX20			W  D00-D02
	Scroll plane 1 priority 	   42XX22			W  D00-D02
	Scroll plane 2 priority 	   42XX24			W  D00-D02
	Scroll plane 3 priority 	   42XX26			W  D00-D02
	Text plane 0 priority		   42XX28			W  D00-D02
	Text plane 1 priority		   42XX2A			W  D00-D02

	Scroll plane 0 colour		   42XX30			W  D00-D03
	Scroll plane 1 colour		   42XX32			W  D00-D03
	Scroll plane 2 colour		   42XX34			W  D00-D03
	Scroll plane 3 colour		   42XX36			W  D00-D03
	Text plane 0 colour 		   42XX38			W  D00-D03
	Text plane 1 colour 		   42XX3A			W  D00-D03

Screen palette control/data 	   440000-45FFFF  R/W  D00-D15
	RED   ROZ/Sprite pens 8x256    440000-440FFF
	GREEN						   441000-441FFF
	BLUE						   442000-442FFF
	Control registers			   443000-44300F  R/W  D00-D15
	RED   ROZ/Sprite pens 8x256    444000-444FFF
	GREEN						   445000-445FFF
	BLUE						   446000-446FFF
																   447000-447FFF
	RED   Text plane pens 8x256    448000-448FFF
	GREEN						   449000-449FFF
	BLUE						   44A000-44AFFF
																   44B000-44BFFF
	RED   Unused pens 8x256 	   44C000-44CFFF
	GREEN						   44D000-44DFFF
	BLUE						   44E000-44EFFF

Dual port memory				   460000-47FFFF  R/W  D00-D07

Serial comms processor			   480000-49FFFF

Serial comms processor - Data	   4A0000-4BFFFF



#############################################################
#															#
#			SHARED 68000 MEMORY AREA (GFX PCB)				#
#			  (STANDARD NAMCO SYSTEM 2 BOARD)				#
#															#
#############################################################
# Function						   Address		  R/W  DATA #
#############################################################
Sprite RAM - 16 banks x 128 spr.   C00000-C03FFF  R/W  D00-D15

Sprite bank select				   C40000			W  D00-D03
Rotate colour bank select							W  D08-D11
Rotate priority level								W  D12-D14

Rotate/Zoom RAM (ROZ)			   C80000-CBFFFF  R/W  D00-D15

Rotate/Zoom - Down dy	  (8:8)    CC0000		  R/W  D00-D15
Rotate/Zoom - Right dy	  (8.8)    CC0002		  R/W  D00-D15
Rotate/Zoom - Down dx	  (8.8)    CC0004		  R/W  D00-D15
Rotate/Zoom - Right dx	  (8.8)    CC0006		  R/W  D00-D15
Rotate/Zoom - Start Ypos  (12.4)   CC0008		  R/W  D00-D15
Rotate/Zoom - Start Xpos  (12.4)   CC000A		  R/W  D00-D15
Rotate/Zoom control 			   CC000E		  R/W  D00-D15

Key generator/Security device	   D00000-D0000F  R/W  D00-D15



#############################################################
#															#
#			SHARED 68000 MEMORY AREA (GFX PCB)				#
#			(METAL HAWK PCB - DUAL ROZ PLANES)				#
#															#
#############################################################
# Function						   Address		  R/W  DATA #
#############################################################
Sprite RAM - 16 banks x 128 spr.   C00000-C03FFF  R/W  D00-D15

Rotate/Zoom RAM (ROZ1)			   C40000-C47FFF  R/W  D00-D15

Rotate/Zoom RAM (ROZ2)			   C48000-C4FFFF  R/W  D00-D15

Rotate/Zoom1 - Down dy	   (8:8)   D00000		  R/W  D00-D15
Rotate/Zoom1 - Right dy    (8.8)   D00002		  R/W  D00-D15
Rotate/Zoom1 - Down dx	   (8.8)   D00004		  R/W  D00-D15
Rotate/Zoom1 - Right dx    (8.8)   D00006		  R/W  D00-D15
Rotate/Zoom1 - Start Ypos  (12.4)  D00008		  R/W  D00-D15
Rotate/Zoom1 - Start Xpos  (12.4)  D0000A		  R/W  D00-D15
Rotate/Zoom1 - control			   D0000E		  R/W  D00-D15

Rotate/Zoom2 - Down dy	   (8:8)   D00010		  R/W  D00-D15
Rotate/Zoom2 - Right dy    (8.8)   D00012		  R/W  D00-D15
Rotate/Zoom2 - Down dx	   (8.8)   D00014		  R/W  D00-D15
Rotate/Zoom2 - Right dx    (8.8)   D00016		  R/W  D00-D15
Rotate/Zoom2 - Start Ypos  (12.4)  D00018		  R/W  D00-D15
Rotate/Zoom2 - Start Xpos  (12.4)  D0001A		  R/W  D00-D15
Rotate/Zoom2 - control			   D0001E		  R/W  D00-D15

Sprite bank select ?			   E00000			W  D00-D15


#############################################################
#															#
#			SHARED 68000 MEMORY AREA (GFX PCB)				#
#			(FINAL LAP PCB) 								#
#															#
#############################################################
# Function						   Address		  R/W  DATA #
#############################################################
Sprite RAM - ?? banks x ??? spr.   800000-80FFFF  R/W  D00-D15
Sprite bank select ?			   840000			W  D00-D15
Road RAM for tile layout		   880000-88FFFF  R/W  D00-D15
Road RAM for tiles gfx data 	   890000-897FFF  R/W  D00-D15
Road Generator controls 		   89F000-89FFFF  R/W  D00-D15
Key generator/Security device	   A00000-A0000F  R/W  D00-D15



All interrupt handling is done on the 68000s by two identical custom devices (C148),
this device takes the level based signals and encodes them into the 3 bit encoded
form for the 68000 CPU. The master CPU C148 also controls the reset for the slave
CPU and MCU which are common. The C148 only has the lower 3 data bits connected.

C148 Features
-------------
3 Bit output port
3 Bit input port
3 Chip selects
68000 Interrupt encoding/handling
Data strobe control
Bus arbitration
Reset output
Watchdog


C148pin 	Master CPU		Slave CPU
-------------------------------------
YBNK		VBLANK			VBLANK
IRQ4		SCIRQ			SCIRQ		(Serial comms IC Interrupt)
IRQ3		POSIRQ			POSIRQ		(Comes from C116, pixel generator, Position interrup ?? line based ??)
IRQ2		EXIRQ			EXIRQ		(Goes to video board but does not appear to be connected)
IRQ1		SCPUIRQ 		MCPUIRQ 	(Master/Slave interrupts)

OP0 		SSRES						(Sound CPU reset - 6809 only)
OP1
OP2

IP0 		EEPROM BUSY
IP1
IP2



Protection
----------
The Chip at $d00000 seems to be heavily involved in protection, some games lock or reset if it doesnt
return the correct values, it MAY be a random number generator and is testing the values based on
the inputted seed value. rthun2 is sprinkled with reads to $d00006 which look like they are being
used as random numbers. rthun 2 also checks the response value after a number is written. Device
takes clock and vblank. Only output is reset.

This chip is based on the graphics board.

$d00000
$d00002
$d00004 	Write 13 x $0000, read back $00bd from $d00002 (burnf)
$d00006 	Write $b929, read $014a (cosmog)
$d00008 	Write $13ec, read $013f (rthun2)
$d0000a 	Write $f00f, read $f00f (phelios)
$d0000c 	Write $8fc8, read $00b2 (rthun2)
$d0000e 	Write $31ad, read $00bd (burnf)


Palette
-------

0x800 (2048) colours

Ram test does:

$440000-$442fff 	Object ???
$444000-$446fff 	Char   ???
$448000-$44afff 	Roz    ???
$44c000-$44efff

$448000-$4487ff 	Red??
$448800-$448fff 	Green??
$449000-$4497ff 	Blue??



----------

Metal Hawk Notes
----------------
This board has a standard System 2 CPU board but it has a different
graphics board, the gfx boards supports sprites & ROZ layer, these are not
working in Metal Hawk and the gfx ROMs do not decode properly, there also
appears to be another shape ROM for the ROZ layer.
If anyone has any schematics for this board please mail the Mame devlist.

Steel Gunner 2
--------------
Again this board has a different graphics layout, also the protection checks
are done at $a00000 as opposed to $d00000 on a standard board. Similar
$a00000 checks have been seen on the Final Lap boards.


***************************************************************************/

#define NAMCOS2_CREDITS "Keith Wilkins\nPhil Stroffolino"

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/namcos2.h"
#include "cpu/m6809/m6809.h"



/*************************************************************/
/* SHARED 68000 CPU Memory declarations 					 */
/*************************************************************/

//	ROM0   = $200000-$2fffff
//	ROM1   = $300000-$3fffff
//	SCR    = $400000-$41ffff
//	SCRDT  = $420000-$43ffff
//	PALET  = $440000-$45ffff
//	DPCS   = $460000-$47ffff
//	SCOM   = $480000-$49ffff
//	SCOMDT = $4a0000-$4bffff

// 0xc00000 ONWARDS are unverified memory locations on the video board

#define NAMCOS2_68K_DEFAULT_CPU_BOARD_READ \
	{ 0x200000, 0x3fffff, namcos2_68k_data_rom_r },\
	{ 0x400000, 0x41ffff, namcos2_68k_vram_r },\
	{ 0x420000, 0x43ffff, namcos2_68k_vram_ctrl_r }, \
	{ 0x440000, 0x45ffff, namcos2_68k_video_palette_r }, \
	{ 0x460000, 0x47ffff, namcos2_68k_dpram_word_r }, \
	{ 0x480000, 0x49ffff, namcos2_68k_serial_comms_ram_r }, \
	{ 0x4a0000, 0x4bffff, namcos2_68k_serial_comms_ctrl_r },

#define NAMCOS2_68K_DEFAULT_GFX_BOARD_READ \
/*	{ 0xc00000, 0xc03fff, namcos2_68k_sprite_ram_r },  CANNOT READ BACK - DEBUG ONLY */ \
/*	{ 0xc40000, 0xc4ffff, namcos2_68k_sprite_bank_r }, CANNOT READ BACK - DEBUG ONLY */ \
	{ 0xc80000, 0xcbffff, namcos2_68k_roz_ram_r },	\
	{ 0xcc0000, 0xcc000f, namcos2_68k_roz_ctrl_r }, \
	{ 0xd00000, 0xd0000f, namcos2_68k_key_r },

#define NAMCOS2_68K_FINALLAP_GFX_BOARD_READ \
	{ 0x800000, 0x80ffff, namcos2_68k_sprite_ram_r },  /* CANNOT READ BACK - DEBUG ONLY */ \
	{ 0x840000, 0x84ffff, namcos2_68k_sprite_bank_r }, /* CANNOT READ BACK - DEBUG ONLY */ \
	{ 0x880000, 0x89ffff, namcos2_68k_roadtile_ram_r }, \
	{ 0x890000, 0x897fff, namcos2_68k_roadgfx_ram_r }, \
	{ 0x89f000, 0x89ffff, namcos2_68k_road_ctrl_r },

#define NAMCOS2_68K_METLHAWK_GFX_BOARD_READ \
/*	{ 0xc00000, 0xc03fff, namcos2_68k_sprite_ram_r },  CANNOT READ BACK - DEBUG ONLY */ \
	{ 0xc80000, 0xcbffff, namcos2_68k_roz_ram_r },	\
	{ 0xcc0000, 0xcc000f, namcos2_68k_roz_ctrl_r }, \
	{ 0xc40000, 0xc47fff, MRA_RAM }, /* ROZ 1 RAM */ \
	{ 0xc48000, 0xc4ffff, MRA_RAM }, /* ROZ 2 RAM */ \
	{ 0xd00000, 0xd0000f, MRA_RAM }, /* ROZ 1 CTL */ \
	{ 0xd00010, 0xd0001f, MRA_RAM }, /* ROZ 2 CTL */ \
/*	{ 0xe00000, 0xe0ffff, namcos2_68k_sprite_bank_r }, CANNOT READ BACK - DEBUG ONLY */


#define NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE \
	{ 0x400000, 0x41ffff, namcos2_68k_vram_w, &videoram, &namcos2_68k_vram_size },\
	{ 0x420000, 0x43ffff, namcos2_68k_vram_ctrl_w }, \
	{ 0x440000, 0x45ffff, namcos2_68k_video_palette_w, &namcos2_68k_palette_ram, &namcos2_68k_palette_size }, \
	{ 0x460000, 0x47ffff, namcos2_68k_dpram_word_w }, \
	{ 0x480000, 0x49ffff, namcos2_68k_serial_comms_ram_w }, \
	{ 0x4a0000, 0x4bffff, namcos2_68k_serial_comms_ctrl_w },

#define NAMCOS2_68K_DEFAULT_GFX_BOARD_WRITE \
	{ 0xc00000, 0xc03fff, namcos2_68k_sprite_ram_w }, \
	{ 0xc40000, 0xc4ffff, namcos2_68k_sprite_bank_w }, \
	{ 0xc80000, 0xcbffff, namcos2_68k_roz_ram_w, &namcos2_68k_roz_ram, &namcos2_68k_roz_ram_size }, \
	{ 0xcc0000, 0xcc000f, namcos2_68k_roz_ctrl_w }, \
	{ 0xd00000, 0xd0000f, namcos2_68k_key_w },

#define NAMCOS2_68K_FINALLAP_GFX_BOARD_WRITE \
	{ 0x800000, 0x80ffff, namcos2_68k_sprite_ram_w }, \
	{ 0x840000, 0x84ffff, namcos2_68k_sprite_bank_w }, \
	{ 0x880000, 0x89ffff, namcos2_68k_roadtile_ram_w, &namcos2_68k_roadtile_ram, &namcos2_68k_roadtile_ram_size }, \
	{ 0x890000, 0x897fff, namcos2_68k_roadgfx_ram_w, &namcos2_68k_roadgfx_ram, &namcos2_68k_roadgfx_ram_size }, \
	{ 0x89f000, 0x89ffff, namcos2_68k_road_ctrl_w },

#define NAMCOS2_68K_METLHAWK_GFX_BOARD_WRITE \
	{ 0xc00000, 0xc03fff, namcos2_68k_sprite_ram_w }, \
	{ 0xc80000, 0xcbffff, namcos2_68k_roz_ram_w, &namcos2_68k_roz_ram, &namcos2_68k_roz_ram_size }, \
	{ 0xcc0000, 0xcc000f, namcos2_68k_roz_ctrl_w }, \
	{ 0xc40000, 0xc47fff, MWA_RAM }, /* ROZ 1 RAM */ \
	{ 0xc48000, 0xc4ffff, MWA_RAM }, /* ROZ 2 RAM */ \
	{ 0xd00000, 0xd0000f, MWA_RAM }, /* ROZ 1 CTL */ \
	{ 0xd00010, 0xd0001f, MWA_RAM }, /* ROZ 2 CTL */ \
	{ 0xe00000, 0xe0ffff, namcos2_68k_sprite_bank_w },




/*************************************************************/
/* MASTER 68000 CPU Memory declarations 					 */
/*************************************************************/

static struct MemoryReadAddress readmem_master_default[] = {
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x10ffff, NAMCOS2_68K_MASTER_RAM_R },
	{ 0x180000, 0x183fff, NAMCOS2_68K_EEPROM_R },
	{ 0x1c0000, 0x1fffff, namcos2_68k_master_C148_r },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_READ
	NAMCOS2_68K_DEFAULT_GFX_BOARD_READ
	{ -1 }
};

static struct MemoryWriteAddress writemem_master_default[] = {
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x10ffff, NAMCOS2_68K_MASTER_RAM_W },
	{ 0x180000, 0x183fff, NAMCOS2_68K_EEPROM_W },
	{ 0x1c0000, 0x1fffff, namcos2_68k_master_C148_w },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE
	NAMCOS2_68K_DEFAULT_GFX_BOARD_WRITE
	{ -1 }
};

static struct MemoryReadAddress readmem_master_finallap[] = {
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x10ffff, NAMCOS2_68K_MASTER_RAM_R },
	{ 0x180000, 0x183fff, NAMCOS2_68K_EEPROM_R },
	{ 0x1c0000, 0x1fffff, namcos2_68k_master_C148_r },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_READ
	NAMCOS2_68K_FINALLAP_GFX_BOARD_READ
	{ -1 }
};

static struct MemoryWriteAddress writemem_master_finallap[] = {
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x10ffff, NAMCOS2_68K_MASTER_RAM_W },
	{ 0x180000, 0x183fff, NAMCOS2_68K_EEPROM_W },
	{ 0x1c0000, 0x1fffff, namcos2_68k_master_C148_w },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE
	NAMCOS2_68K_FINALLAP_GFX_BOARD_WRITE
	{ -1 }
};

static struct MemoryReadAddress readmem_master_metlhawk[] = {
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x10ffff, NAMCOS2_68K_MASTER_RAM_R },
	{ 0x180000, 0x183fff, NAMCOS2_68K_EEPROM_R },
	{ 0x1c0000, 0x1fffff, namcos2_68k_master_C148_r },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_READ
	NAMCOS2_68K_METLHAWK_GFX_BOARD_READ
	{ -1 }
};

static struct MemoryWriteAddress writemem_master_metlhawk[] = {
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x10ffff, NAMCOS2_68K_MASTER_RAM_W },
	{ 0x180000, 0x183fff, NAMCOS2_68K_EEPROM_W },
	{ 0x1c0000, 0x1fffff, namcos2_68k_master_C148_w },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE
	NAMCOS2_68K_METLHAWK_GFX_BOARD_WRITE
	{ -1 }
};

/*************************************************************/
/* SLAVE 68000 CPU Memory declarations						 */
/*************************************************************/

static struct MemoryReadAddress readmem_slave_default[] = {
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x13ffff, NAMCOS2_68K_SLAVE_RAM_R },
	{ 0x1c0000, 0x1fffff, namcos2_68k_slave_C148_r },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_READ
	NAMCOS2_68K_DEFAULT_GFX_BOARD_READ
	{ -1 }
};

static struct MemoryWriteAddress writemem_slave_default[] ={
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x13ffff, NAMCOS2_68K_SLAVE_RAM_W },
	{ 0x1c0000, 0x1fffff, namcos2_68k_slave_C148_w },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE
	NAMCOS2_68K_DEFAULT_GFX_BOARD_WRITE
	{ -1 }
};

static struct MemoryReadAddress readmem_slave_finallap[] = {
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x13ffff, NAMCOS2_68K_SLAVE_RAM_R },
	{ 0x1c0000, 0x1fffff, namcos2_68k_slave_C148_r },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_READ
	NAMCOS2_68K_FINALLAP_GFX_BOARD_READ
	{ -1 }
};

static struct MemoryWriteAddress writemem_slave_finallap[] ={
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x13ffff, NAMCOS2_68K_SLAVE_RAM_W },
	{ 0x1c0000, 0x1fffff, namcos2_68k_slave_C148_w },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE
	NAMCOS2_68K_FINALLAP_GFX_BOARD_WRITE
	{ -1 }
};

static struct MemoryReadAddress readmem_slave_metlhawk[] = {
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x13ffff, NAMCOS2_68K_SLAVE_RAM_R },
	{ 0x1c0000, 0x1fffff, namcos2_68k_slave_C148_r },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_READ
	NAMCOS2_68K_METLHAWK_GFX_BOARD_READ
	{ -1 }
};

static struct MemoryWriteAddress writemem_slave_metlhawk[] ={
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x13ffff, NAMCOS2_68K_SLAVE_RAM_W },
	{ 0x1c0000, 0x1fffff, namcos2_68k_slave_C148_w },
	NAMCOS2_68K_DEFAULT_CPU_BOARD_WRITE
	NAMCOS2_68K_METLHAWK_GFX_BOARD_WRITE
	{ -1 }
};


/*************************************************************/
/* 6809 SOUND CPU Memory declarations						 */
/*************************************************************/

static struct MemoryReadAddress readmem_sound[] ={
	{ 0x0000, 0x3fff, BANKED_SOUND_ROM_R }, /* banked */
	{ 0x4000, 0x4001, YM2151_status_port_0_r },
	{ 0x5000, 0x6fff, C140_r },
	{ 0x7000, 0x7fff, namcos2_dpram_byte_r },		/* 991112.CAB  ($5800-5fff=image of $5000-$57ff) */
	{ 0x8000, 0x9fff, MRA_RAM },
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound[] = {
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x4000, YM2151_register_port_0_w },
	{ 0x4001, 0x4001, YM2151_data_port_0_w },
	{ 0x5000, 0x6fff, C140_w },
	{ 0x7000, 0x7fff, namcos2_dpram_byte_w },		/* 991112.CAB ($5800-5fff=image of $5000-$57ff) */
	{ 0x8000, 0x9fff, MWA_RAM },
	{ 0xa000, 0xbfff, MWA_NOP },					/* Amplifier enable on 1st write */
	{ 0xc000, 0xc001, namcos2_sound_bankselect_w },
	{ 0xd001, 0xd001, MWA_NOP },					/* Watchdog */
	{ 0xc000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};


/*************************************************************/
/* 68705 IO CPU Memory declarations 						 */
/*************************************************************/

static struct MemoryReadAddress readmem_mcu[] ={
	/* input ports and dips are mapped here */

	{ 0x0000, 0x0000, MRA_NOP },			// Keep logging quiet
	{ 0x0001, 0x0001, namcos2_input_port_0_r },
	{ 0x0002, 0x0002, input_port_1_r },
	{ 0x0003, 0x0003, namcos2_mcu_port_d_r },
	{ 0x0007, 0x0007, namcos2_input_port_10_r },
	{ 0x0010, 0x0010, namcos2_mcu_analog_ctrl_r },
	{ 0x0011, 0x0011, namcos2_mcu_analog_port_r },
	{ 0x0008, 0x003f, MRA_RAM },			// Fill in register to stop logging
	{ 0x0040, 0x01bf, MRA_RAM },
	{ 0x01c0, 0x1fff, MRA_ROM },
	{ 0x2000, 0x2000, input_port_11_r },
	{ 0x3000, 0x3000, namcos2_input_port_12_r },
	{ 0x3001, 0x3001, input_port_13_r },
	{ 0x3002, 0x3002, input_port_14_r },
	{ 0x3003, 0x3003, input_port_15_r },
	{ 0x5000, 0x57ff, namcos2_dpram_byte_r },
	{ 0x6000, 0x6fff, MRA_NOP },				/* watchdog */
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_mcu[] ={
	{ 0x0003, 0x0003, namcos2_mcu_port_d_w },
	{ 0x0010, 0x0010, namcos2_mcu_analog_ctrl_w },
	{ 0x0011, 0x0011, namcos2_mcu_analog_port_w },
	{ 0x0000, 0x003f, MWA_RAM },			// Fill in register to stop logging
	{ 0x0040, 0x01bf, MWA_RAM },
	{ 0x01c0, 0x1fff, MWA_ROM },
	{ 0x5000, 0x57ff, namcos2_dpram_byte_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};



/*************************************************************/
/*															 */
/*	NAMCO SYSTEM 2 PORT MACROS								 */
/*															 */
/*	Below are the port defintion macros that should be used  */
/*	as the basis for definig a port set for a Namco System2  */
/*	game.													 */
/*															 */
/*************************************************************/

#define NAMCOS2_MCU_PORT_B_DEFAULT \
	PORT_START		/* 63B05Z0 - PORT B */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

#define NAMCOS2_MCU_PORT_C_DEFAULT \
	PORT_START		/* 63B05Z0 - PORT C & SCI */ \
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, "Service Button", KEYCODE_F1, IP_JOY_NONE ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

#define NAMCOS2_MCU_ANALOG_PORT_DEFAULT \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 0 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 1 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 2 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 3 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 4 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 5 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 6 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 7 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

#define NAMCOS2_MCU_PORT_H_DEFAULT \
	PORT_START		/* 63B05Z0 - PORT H */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )

#define NAMCOS2_MCU_DIPSW_DEFAULT \
	PORT_START		/* 63B05Z0 - $2000 DIP SW */ \
	PORT_DIPNAME( 0x01, 0x01, "Video Display") \
	PORT_DIPSETTING(	0x01, "Normal" ) \
	PORT_DIPSETTING(	0x00, "Frozen" ) \
	PORT_DIPNAME( 0x02, 0x02, "$2000-1") \
	PORT_DIPSETTING(	0x02, "H" ) \
	PORT_DIPSETTING(	0x00, "L" ) \
	PORT_DIPNAME( 0x04, 0x04, "$2000-2") \
	PORT_DIPSETTING(	0x04, "H" ) \
	PORT_DIPSETTING(	0x00, "L" ) \
	PORT_DIPNAME( 0x08, 0x08, "$2000-3") \
	PORT_DIPSETTING(	0x08, "H" ) \
	PORT_DIPSETTING(	0x00, "L" ) \
	PORT_DIPNAME( 0x10, 0x10, "$2000-4") \
	PORT_DIPSETTING(	0x10, "H" ) \
	PORT_DIPSETTING(	0x00, "L" ) \
	PORT_DIPNAME( 0x20, 0x20, "$2000-5") \
	PORT_DIPSETTING(	0x20, "H" ) \
	PORT_DIPSETTING(	0x00, "L" ) \
	PORT_DIPNAME( 0x40, 0x40, "$2000-6") \
	PORT_DIPSETTING(	0x40, "H" ) \
	PORT_DIPSETTING(	0x00, "L" ) \
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

#define NAMCOS2_MCU_DIAL_DEFAULT \
	PORT_START		/* 63B05Z0 - $3000 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
/*	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 20, 10, 0, 0 ) */ \
	PORT_START		/* 63B05Z0 - $3001 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - $3002 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START		/* 63B05Z0 - $3003 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

/*************************************************************/
/*															 */
/*	NAMCO SYSTEM 2 PORT DEFINITIONS 						 */
/*															 */
/*	There is a standard port definition defined that will	 */
/*	work for most games, if you wish to produce a special	 */
/*	definition for a particular game then see the assault	 */
/*	and dirtfox definitions for examples of how to construct */
/*	a special port definition								 */
/*															 */
/*	The default definitions includes only the following list */
/*	of connections :										 */
/*	  2 Joysticks, 6 Buttons, 1 Service, 1 Advance			 */
/*	  2 Start												 */
/*															 */
/*************************************************************/

INPUT_PORTS_START( default )
	NAMCOS2_MCU_PORT_B_DEFAULT
	NAMCOS2_MCU_PORT_C_DEFAULT
	NAMCOS2_MCU_ANALOG_PORT_DEFAULT
	NAMCOS2_MCU_PORT_H_DEFAULT
	NAMCOS2_MCU_DIPSW_DEFAULT
	NAMCOS2_MCU_DIAL_DEFAULT
INPUT_PORTS_END

INPUT_PORTS_START( assault )
	PORT_START		/* 63B05Z0 - PORT B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	NAMCOS2_MCU_PORT_C_DEFAULT
	NAMCOS2_MCU_ANALOG_PORT_DEFAULT

	PORT_START		/* 63B05Z0 - PORT H */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT )

	NAMCOS2_MCU_DIPSW_DEFAULT

	PORT_START	 /* 63B05Z0 - $3000 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT )
	PORT_START	 /* 63B05Z0 - $3001 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	 /* 63B05Z0 - $3002 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
//	PORT_START	 /* 63B05Z0 - $3003 */
//	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* fake port15 for single joystick control */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( driving )
	NAMCOS2_MCU_PORT_B_DEFAULT
	NAMCOS2_MCU_PORT_C_DEFAULT

	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 0 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 1 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 4 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* Steering Wheel */
	PORT_ANALOG( 0xff, 0x7f, IPT_DIAL|IPF_CENTER|IPF_PLAYER1, 70, 50, 0x00, 0xff )
	PORT_START		/* Brake pedal */
	PORT_ANALOG( 0xff, 0xff, IPT_PEDAL|IPF_PLAYER2, 100, 30, 0x00, 0x7f )
	PORT_START		/* Accelerator pedal */
	PORT_ANALOG( 0xff, 0xff, IPT_PEDAL|IPF_PLAYER1, 100, 15, 0x00, 0x7f )

	NAMCOS2_MCU_PORT_H_DEFAULT
	NAMCOS2_MCU_DIPSW_DEFAULT
	NAMCOS2_MCU_DIAL_DEFAULT
INPUT_PORTS_END

INPUT_PORTS_START( dirtfox )
	PORT_START		/* 63B05Z0 - PORT B */ \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )	/* Gear shift up */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	/* Gear shift down */

	NAMCOS2_MCU_PORT_C_DEFAULT

	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 0 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 1 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 4 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* Steering Wheel */
	PORT_ANALOG( 0xff, 0x7f, IPT_DIAL|IPF_CENTER|IPF_PLAYER1, 70, 50, 0x00, 0xff )
	PORT_START		/* Brake pedal */
	PORT_ANALOG( 0xff, 0xff, IPT_PEDAL|IPF_PLAYER2, 100, 30, 0x00, 0x7f )
	PORT_START		/* Accelerator pedal */
	PORT_ANALOG( 0xff, 0xff, IPT_PEDAL|IPF_PLAYER1, 100, 15, 0x00, 0x7f )

	PORT_START		/* 63B05Z0 - PORT H */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	NAMCOS2_MCU_DIPSW_DEFAULT
	NAMCOS2_MCU_DIAL_DEFAULT
INPUT_PORTS_END

INPUT_PORTS_START( metlhawk )
	PORT_START		/* 63B05Z0 - PORT B */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	NAMCOS2_MCU_PORT_C_DEFAULT

	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 0 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 1 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* 63B05Z0 - 8 CHANNEL ANALOG - CHANNEL 4 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START		/* Joystick Y */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y|IPF_CENTER, 100, 10, 0x40, 0xbe )
	PORT_START		/* Joystick X */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X|IPF_CENTER, 100, 10, 0x40, 0xbe )
	PORT_START		/* Lever */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y|IPF_CENTER|IPF_REVERSE|IPF_PLAYER2, 100, 10, 0x40, 0xbe )

	PORT_START		/* 63B05Z0 - PORT H */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	NAMCOS2_MCU_DIPSW_DEFAULT
	NAMCOS2_MCU_DIAL_DEFAULT
INPUT_PORTS_END




/*************************************************************/
/* Namco System II - Graphics Declarations					 */
/*************************************************************/

static struct GfxLayout obj_layout = {
	32,32,
	0x800,	/* number of sprites */
	8,		/* bits per pixel */
	{		/* plane offsets */
		(0x400000*3),(0x400000*3)+4,(0x400000*2),(0x400000*2)+4,
		(0x400000*1),(0x400000*1)+4,(0x400000*0),(0x400000*0)+4
	},
	{ /* x offsets */
		0*8,0*8+1,0*8+2,0*8+3,
		1*8,1*8+1,1*8+2,1*8+3,
		2*8,2*8+1,2*8+2,2*8+3,
		3*8,3*8+1,3*8+2,3*8+3,

		4*8,4*8+1,4*8+2,4*8+3,
		5*8,5*8+1,5*8+2,5*8+3,
		6*8,6*8+1,6*8+2,6*8+3,
		7*8,7*8+1,7*8+2,7*8+3,
	},
	{ /* y offsets */
		0*128,0*128+64,1*128,1*128+64,
		2*128,2*128+64,3*128,3*128+64,
		4*128,4*128+64,5*128,5*128+64,
		6*128,6*128+64,7*128,7*128+64,

		8*128,8*128+64,9*128,9*128+64,
		10*128,10*128+64,11*128,11*128+64,
		12*128,12*128+64,13*128,13*128+64,
		14*128,14*128+64,15*128,15*128+64
	},
	0x800 /* sprite offset */
};

static struct GfxLayout chr_layout = {
	8,8,
	0x10000,
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	{ 0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64 },
	8*64
};

static struct GfxLayout roz_layout = {
	8,8,
	0x10000,
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	{ 0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64 },
	8*64
};

/* static struct GfxLayout mask_layout =
{
	8,8,
	0x10000,
	1,
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
}; */

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1 , 0x000000, &obj_layout  , 0x0000, 0x10 },
	{ REGION_GFX1 , 0x200000, &obj_layout  , 0x0000, 0x10 },
	{ REGION_GFX2 , 0x000000, &chr_layout  , 0x1000, 0x08 },
	{ REGION_GFX3 , 0x000000, &roz_layout  , 0x0000, 0x10 },
/*	{ REGION_GFX4 , 0x000000, &mask_layout , 0x0000, 0x01 }, */
	{ -1 }
};


static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579580,	/* 3.58 MHZ ? */
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) },
	{ NULL }	/* YM2151 IRQ line is NOT connected on the PCB */
};


static struct C140interface C140_interface =
{
	8000000/374,
	REGION_SOUND1,
	50
};



/******************************************

Master clock = 49.152Mhz

68000 Measured at  84ns = 12.4Mhz	BUT 49.152MHz/4 = 12.288MHz = 81ns
6809  Measured at 343ns = 2.915 MHz BUT 49.152MHz/16 = 3.072MHz = 325ns
63B05 Measured at 120ns = 8.333 MHz BUT 49.152MHz/6 = 8.192MHz = 122ns

I've corrected all frequencies to be multiples of integer divisions of
the 49.152Mhz master clock. Additionally the 6305 looks to hav an
internal divider.

Soooo;

680000	= 12288000
6809	=  3072000
63B05Z0 =  2048000

The interrupts to CPU4 has been measured at 60Hz (16.5mS period) on a
logic analyser. This interrupt is wired to port PA1 which is configured
via software as INT1

*******************************************/

/*************************************************************/
/*															 */
/*	NAMCO SYSTEM 2 MACHINE DEFINTIONS						 */
/*															 */
/*	Below are the machine definitions for the various Namco  */
/*	System 2 board configurations, this mechanism is also	 */
/*	used to provide 8/16bpp drivers for different games 	 */
/*															 */
/*************************************************************/

static struct MachineDriver machine_driver_default =
{
	{
		{
			CPU_M68000,
			12288000,
			readmem_master_default,writemem_master_default,0,0,
			namcos2_68k_master_vblank,1,
			0,0
		},
		{
			CPU_M68000,
			12288000,
			readmem_slave_default,writemem_slave_default,0,0,
			namcos2_68k_slave_vblank,1,
			0,0
		},
		{
			CPU_M6809, // Sound handling
			3072000,
			readmem_sound,writemem_sound,0,0,
			interrupt,2,
			namcos2_sound_interrupt,120
		},
		{
			CPU_HD63705, // I/O handling
			2048000,
			readmem_mcu,writemem_mcu,0,0,
			namcos2_mcu_interrupt,1,
			0,0
		}
	},
	60,
	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100, /* 100 CPU slices per frame */
	namcos2_init_machine,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	8192,8192,
	namcos2_vh_convert_color_prom,				/* Convert colour prom	   */
	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,

	0,											/* Video initialisation    */
	namcos2_vh_start,							/* Video start			   */
	namcos2_vh_stop,							/* Video stop			   */
	namcos2_vh_update_default,					/* Video update 		   */

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	/* Sound struct here */
	{
		{
			SOUND_C140,
			&C140_interface
		},
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	},

	/* NV RAM Support */
	namcos2_nvram_handler
};


static struct MachineDriver machine_driver_driving =
{
	{
		{
			CPU_M68000,
			12288000,
			readmem_master_finallap,writemem_master_finallap,0,0,
			namcos2_68k_master_vblank,1,
			0,0
		},
		{
			CPU_M68000,
			12288000,
			readmem_slave_finallap,writemem_slave_finallap,0,0,
			namcos2_68k_slave_vblank,1,
			0,0
		},
		{
			CPU_M6809, // Sound handling
			3072000,
			readmem_sound,writemem_sound,0,0,
			interrupt,2,
			namcos2_sound_interrupt,120
		},
		{
			CPU_HD63705, // I/O handling
			2048000,
			readmem_mcu,writemem_mcu,0,0,
			namcos2_mcu_interrupt,1,
			0,0
		}
	},
	60,
	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100, /* 100 CPU slices per frame */
	namcos2_init_machine,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	8192,8192,
	namcos2_vh_convert_color_prom,				/* Convert colour prom	   */
	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,

	0,											/* Video initialisation    */
	namcos2_vh_start,							/* Video start			   */
	namcos2_vh_stop,							/* Video stop			   */
	namcos2_vh_update_finallap, 				/* Video update 		   */

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	/* Sound struct here */
	{
		{
			SOUND_C140,
			&C140_interface
		},
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	},

	/* NV RAM Support */
	namcos2_nvram_handler
};


static struct MachineDriver machine_driver_metlhawk =
{
	{
		{
			CPU_M68000,
			12288000,
			readmem_master_metlhawk,writemem_master_metlhawk,0,0,
			namcos2_68k_master_vblank,1,
			0,0
		},
		{
			CPU_M68000,
			12288000,
			readmem_slave_metlhawk,writemem_slave_metlhawk,0,0,
			namcos2_68k_slave_vblank,1,
			0,0
		},
		{
			CPU_M6809, // Sound handling
			3072000,
			readmem_sound,writemem_sound,0,0,
			interrupt,2,
			namcos2_sound_interrupt,120
		},
		{
			CPU_HD63705, // I/O handling
			2048000,
			readmem_mcu,writemem_mcu,0,0,
			namcos2_mcu_interrupt,1,
			0,0
		}
	},
	60,
	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100, /* 100 CPU slices per frame */
	namcos2_init_machine,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	8192,8192,
	namcos2_vh_convert_color_prom,				/* Convert colour prom	   */
	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,

	0,											/* Video initialisation    */
	namcos2_vh_start,							/* Video start			   */
	namcos2_vh_stop,							/* Video stop			   */
	namcos2_vh_update_default,					/* Video update 		   */

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	/* Sound struct here */
	{
		{
			SOUND_C140,
			&C140_interface
		},
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	},

	/* NV RAM Support */
	namcos2_nvram_handler
};




/*************************************************************/
/* Namco System II - ROM Declarations						 */
/*************************************************************/

/*************************************************************/
/* IF YOU ARE ADDING A NEW DRIVER PLEASE MAKE SURE YOU DO	 */
/* NOT CHANGE THE SIZES OF THE MEMORY REGIONS, AS THESE ARE  */
/* ALWAYS SET TO THE CORRECT SYSTEM SIZE AS OPPOSED TO THE	 */
/* SIZE OF THE INDIVIDUAL ROMS. 							 */
/*************************************************************/

/*************************************************************/
/* YOU MUST MAKE SURE ANY ROM DECLARATIONS FOR GRAPHICS 	 */
/* DATA ROMS ARE REPEATED TO COMPLETELY FILL THE ALLOCATED	 */
/* SPACE. EACH ROM IS EXPECTED TO BE 512K SO 128K ROMS MUST  */
/* BE REPEATED 4 TIMES AND 256K 2 TIMES.					 */
/*															 */
/*															 */
/*			****** USE THE MACROS BELOW ******				 */
/*															 */
/*															 */
/*************************************************************/

#define NAMCOS2_GFXROM_LOAD_128K(romname,start,chksum)\
	ROM_LOAD( romname		, (start + 0x000000), 0x020000, chksum )\
	ROM_RELOAD( 			  (start + 0x020000), 0x020000 )\
	ROM_RELOAD( 			  (start + 0x040000), 0x020000 )\
	ROM_RELOAD( 			  (start + 0x060000), 0x020000 )

#define NAMCOS2_GFXROM_LOAD_256K(romname,start,chksum)\
	ROM_LOAD( romname		, (start + 0x000000), 0x040000, chksum )\
	ROM_RELOAD( 			  (start + 0x040000), 0x040000 )

#define NAMCOS2_GFXROM_LOAD_512K(romname,start,chksum)\
	ROM_LOAD( romname		, (start + 0x000000), 0x080000, chksum )

#define NAMCOS2_DATA_LOAD_E_128K(romname,start,chksum)\
	ROM_LOAD_EVEN(romname		, (start + 0x000000), 0x020000, chksum )\
	ROM_RELOAD_EVEN(			  (start + 0x040000), 0x020000 )\
	ROM_RELOAD_EVEN(			  (start + 0x080000), 0x020000 )\
	ROM_RELOAD_EVEN(			  (start + 0x0c0000), 0x020000 )

#define NAMCOS2_DATA_LOAD_O_128K(romname,start,chksum)\
	ROM_LOAD_ODD( romname		, (start + 0x000000), 0x020000, chksum )\
	ROM_RELOAD_ODD( 			  (start + 0x040000), 0x020000 )\
	ROM_RELOAD_ODD( 			  (start + 0x080000), 0x020000 )\
	ROM_RELOAD_ODD( 			  (start + 0x0c0000), 0x020000 )

#define NAMCOS2_DATA_LOAD_E_256K(romname,start,chksum)\
	ROM_LOAD_EVEN(romname		, (start + 0x000000), 0x040000, chksum )\
	ROM_RELOAD_EVEN(			  (start + 0x080000), 0x040000 )

#define NAMCOS2_DATA_LOAD_O_256K(romname,start,chksum)\
	ROM_LOAD_ODD( romname		, (start + 0x000000), 0x040000, chksum )\
	ROM_RELOAD_ODD( 			  (start + 0x080000), 0x040000 )

#define NAMCOS2_DATA_LOAD_E_512K(romname,start,chksum)\
	ROM_LOAD_EVEN(romname		, (start + 0x000000), 0x080000, chksum )

#define NAMCOS2_DATA_LOAD_O_512K(romname,start,chksum)\
	ROM_LOAD_ODD( romname		, (start + 0x000000), 0x080000, chksum )


/*************************************************************/
/*					   ASSAULT (NAMCO)						 */
/*************************************************************/
ROM_START( assault )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "at2mp0b.bin",  0x000000, 0x010000, 0x801f71c5 )
	ROM_LOAD_ODD(  "at2mp1b.bin",  0x000000, 0x010000, 0x72312d4f )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "at1sp0.bin",  0x000000, 0x010000, 0x0de2a0da )
	ROM_LOAD_ODD(  "at1sp1.bin",  0x000000, 0x010000, 0x02d15fbe )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "at1snd0.bin",  0x00c000, 0x004000, 0x1d1ffe12 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65b.bin",  0x008000, 0x008000, 0xe9f2922a )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_128K( "atobj0.bin",  0x000000, 0x22240076 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj1.bin",  0x080000, 0x2284a8e8 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj2.bin",  0x100000, 0x51425476 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj3.bin",  0x180000, 0x791f42ce )
	NAMCOS2_GFXROM_LOAD_128K( "atobj4.bin",  0x200000, 0x4782e1b0 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj5.bin",  0x280000, 0xf5d158cf )
	NAMCOS2_GFXROM_LOAD_128K( "atobj6.bin",  0x300000, 0x12f6a569 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj7.bin",  0x380000, 0x06a929f2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "atchr0.bin",  0x000000, 0x6f8e968a )
	NAMCOS2_GFXROM_LOAD_128K( "atchr1.bin",  0x080000, 0x88cf7cbe )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "atroz0.bin",  0x000000, 0x8c247a97 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz1.bin",  0x080000, 0xe44c475b )
	NAMCOS2_GFXROM_LOAD_128K( "atroz2.bin",  0x100000, 0x770f377f )
	NAMCOS2_GFXROM_LOAD_128K( "atroz3.bin",  0x180000, 0x01d93d0b )
	NAMCOS2_GFXROM_LOAD_128K( "atroz4.bin",  0x200000, 0xf96feab5 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz5.bin",  0x280000, 0xda2f0d9e )
	NAMCOS2_GFXROM_LOAD_128K( "atroz6.bin",  0x300000, 0x9089e477 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz7.bin",  0x380000, 0x62b2783a )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "atshape.bin",  0x000000, 0xdfcad82b )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "at1dat0.bin",  0x000000, 0x844890f4 )
	NAMCOS2_DATA_LOAD_O_128K( "at1dat1.bin",  0x000000, 0x21715313 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "atvoi1.bin",  0x000000, 0x080000, 0xd36a649e )
ROM_END


/*************************************************************/
/*					   ASSAULT (JAPAN)						 */
/*************************************************************/
ROM_START( assaultj )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "at1_mp0.bin",  0x000000, 0x010000, 0x2d3e5c8c )
	ROM_LOAD_ODD(  "at1_mp1.bin",  0x000000, 0x010000, 0x851cec3a )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "at1sp0.bin",  0x000000, 0x010000, 0x0de2a0da )
	ROM_LOAD_ODD(  "at1sp1.bin",  0x000000, 0x010000, 0x02d15fbe )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "at1snd0.bin",  0x00c000, 0x004000, 0x1d1ffe12 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65b.bin",  0x008000, 0x008000, 0xe9f2922a )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_128K( "atobj0.bin",  0x000000, 0x22240076 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj1.bin",  0x080000, 0x2284a8e8 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj2.bin",  0x100000, 0x51425476 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj3.bin",  0x180000, 0x791f42ce )
	NAMCOS2_GFXROM_LOAD_128K( "atobj4.bin",  0x200000, 0x4782e1b0 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj5.bin",  0x280000, 0xf5d158cf )
	NAMCOS2_GFXROM_LOAD_128K( "atobj6.bin",  0x300000, 0x12f6a569 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj7.bin",  0x380000, 0x06a929f2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "atchr0.bin",  0x000000, 0x6f8e968a )
	NAMCOS2_GFXROM_LOAD_128K( "atchr1.bin",  0x080000, 0x88cf7cbe )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "atroz0.bin",  0x000000, 0x8c247a97 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz1.bin",  0x080000, 0xe44c475b )
	NAMCOS2_GFXROM_LOAD_128K( "atroz2.bin",  0x100000, 0x770f377f )
	NAMCOS2_GFXROM_LOAD_128K( "atroz3.bin",  0x180000, 0x01d93d0b )
	NAMCOS2_GFXROM_LOAD_128K( "atroz4.bin",  0x200000, 0xf96feab5 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz5.bin",  0x280000, 0xda2f0d9e )
	NAMCOS2_GFXROM_LOAD_128K( "atroz6.bin",  0x300000, 0x9089e477 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz7.bin",  0x380000, 0x62b2783a )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "atshape.bin",  0x000000, 0xdfcad82b )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "at1dat0.bin",  0x000000, 0x844890f4 )
	NAMCOS2_DATA_LOAD_O_128K( "at1dat1.bin",  0x000000, 0x21715313 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "atvoi1.bin",  0x000000, 0x080000, 0xd36a649e )
ROM_END


/*************************************************************/
/*					   ASSAULT PLUS (NAMCO) 				 */
/*************************************************************/
ROM_START( assaultp )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mpr0.bin",	0x000000, 0x010000, 0x97519f9f )
	ROM_LOAD_ODD(  "mpr1.bin",	0x000000, 0x010000, 0xc7f437c7 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "at1sp0.bin",  0x000000, 0x010000, 0x0de2a0da )
	ROM_LOAD_ODD(  "at1sp1.bin",  0x000000, 0x010000, 0x02d15fbe )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "at1snd0.bin",  0x00c000, 0x004000, 0x1d1ffe12 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65b.bin",  0x008000, 0x008000, 0xe9f2922a )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_128K( "atobj0.bin",  0x000000, 0x22240076 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj1.bin",  0x080000, 0x2284a8e8 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj2.bin",  0x100000, 0x51425476 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj3.bin",  0x180000, 0x791f42ce )
	NAMCOS2_GFXROM_LOAD_128K( "atobj4.bin",  0x200000, 0x4782e1b0 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj5.bin",  0x280000, 0xf5d158cf )
	NAMCOS2_GFXROM_LOAD_128K( "atobj6.bin",  0x300000, 0x12f6a569 )
	NAMCOS2_GFXROM_LOAD_128K( "atobj7.bin",  0x380000, 0x06a929f2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "atchr0.bin",  0x000000, 0x6f8e968a )
	NAMCOS2_GFXROM_LOAD_128K( "atchr1.bin",  0x080000, 0x88cf7cbe )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "atroz0.bin",  0x000000, 0x8c247a97 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz1.bin",  0x080000, 0xe44c475b )
	NAMCOS2_GFXROM_LOAD_128K( "atroz2.bin",  0x100000, 0x770f377f )
	NAMCOS2_GFXROM_LOAD_128K( "atroz3.bin",  0x180000, 0x01d93d0b )
	NAMCOS2_GFXROM_LOAD_128K( "atroz4.bin",  0x200000, 0xf96feab5 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz5.bin",  0x280000, 0xda2f0d9e )
	NAMCOS2_GFXROM_LOAD_128K( "atroz6.bin",  0x300000, 0x9089e477 )
	NAMCOS2_GFXROM_LOAD_128K( "atroz7.bin",  0x380000, 0x62b2783a )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "atshape.bin",  0x000000, 0xdfcad82b )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "at1dat0.bin",  0x000000, 0x844890f4 )
	NAMCOS2_DATA_LOAD_O_128K( "at1dat1.bin",  0x000000, 0x21715313 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "atvoi1.bin",  0x000000, 0x080000, 0xd36a649e )
ROM_END


/*************************************************************/
/*					   BURNING FORCE						 */
/*************************************************************/
ROM_START( burnforc )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "bumpr0c.bin",  0x000000, 0x020000, 0xcc5864c6 )
	ROM_LOAD_ODD(  "bumpr1c.bin",  0x000000, 0x020000, 0x3e6b4b1b )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "bu1spr0.bin",  0x000000, 0x010000, 0x17022a21 )
	ROM_LOAD_ODD(  "bu1spr1.bin",  0x000000, 0x010000, 0x5255f8a5 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "busnd0.bin",  0x00c000, 0x004000, 0xfabb1150 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "buobj0.bin",  0x000000, 0x24c919a1 )
	NAMCOS2_GFXROM_LOAD_512K( "buobj1.bin",  0x080000, 0x5bcb519b )
	NAMCOS2_GFXROM_LOAD_512K( "buobj2.bin",  0x100000, 0x509dd5d0 )
	NAMCOS2_GFXROM_LOAD_512K( "buobj3.bin",  0x180000, 0x270a161e )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "buchr0.bin",  0x000000, 0xc2109f73 )
	NAMCOS2_GFXROM_LOAD_128K( "buchr1.bin",  0x080000, 0x67d6aa67 )
	NAMCOS2_GFXROM_LOAD_128K( "buchr2.bin",  0x100000, 0x52846eff )
	NAMCOS2_GFXROM_LOAD_128K( "buchr3.bin",  0x180000, 0xd1326d7f )
	NAMCOS2_GFXROM_LOAD_128K( "buchr4.bin",  0x200000, 0x81a66286 )
	NAMCOS2_GFXROM_LOAD_128K( "buchr5.bin",  0x280000, 0x629aa67f )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "buroz0.bin",  0x000000, 0x65fefc83 )
	NAMCOS2_GFXROM_LOAD_128K( "buroz1.bin",  0x080000, 0x979580c2 )
	NAMCOS2_GFXROM_LOAD_128K( "buroz2.bin",  0x100000, 0x548b6ad8 )
	NAMCOS2_GFXROM_LOAD_128K( "buroz3.bin",  0x180000, 0xa633cea0 )
	NAMCOS2_GFXROM_LOAD_128K( "buroz4.bin",  0x200000, 0x1b1f56a6 )
	NAMCOS2_GFXROM_LOAD_128K( "buroz5.bin",  0x280000, 0x4b864b0e )
	NAMCOS2_GFXROM_LOAD_128K( "buroz6.bin",  0x300000, 0x38bd25ba )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "bushape.bin",  0x000000,0x80a6b722 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "bu1dat0.bin",  0x000000, 0xe0a9d92f )
	NAMCOS2_DATA_LOAD_O_128K( "bu1dat1.bin",  0x000000, 0x5fe54b73 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "buvoi1.bin",  0x000000, 0x080000, 0x99d8a239 )
ROM_END


/*************************************************************/
/*				   COSMO GANG THE VIDEO (USA)				 */
/*************************************************************/
ROM_START( cosmogng )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "co2_mp0",  0x000000, 0x020000, 0x2632c209 )
	ROM_LOAD_ODD(  "co2_mp1",  0x000000, 0x020000, 0x65840104 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "co1spr0.bin",  0x000000, 0x020000, 0xbba2c28f )
	ROM_LOAD_ODD(  "co1spr1.bin",  0x000000, 0x020000, 0xc029b459 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "co2_s0",  0x00c000, 0x004000, 0x4ca59338 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "co1obj0.bin",  0x000000, 0x5df8ce0c )
	NAMCOS2_GFXROM_LOAD_512K( "co1obj1.bin",  0x080000, 0x3d152497 )
	NAMCOS2_GFXROM_LOAD_512K( "co1obj2.bin",  0x100000, 0x4e50b6ee )
	NAMCOS2_GFXROM_LOAD_512K( "co1obj3.bin",  0x180000, 0x7beed669 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "co1chr0.bin",  0x000000, 0xee375b3e )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr1.bin",  0x080000, 0x0149de65 )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr2.bin",  0x100000, 0x93d565a0 )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr3.bin",  0x180000, 0x4d971364 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "co1roz0.bin",  0x000000, 0x2bea6951 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "co1sha0.bin",  0x000000, 0x063a70cc )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "co1dat0.bin",  0x000000, 0xb53da2ae )
	NAMCOS2_DATA_LOAD_O_128K( "co1dat1.bin",  0x000000, 0xd21ad10b )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "co2_v1",  0x000000, 0x080000, 0x5a301349 )
	ROM_LOAD( "co2_v2",  0x080000, 0x080000, 0xa27cb45a )
ROM_END


/*************************************************************/
/*				  COSMO GANG THE VIDEO (JAPAN)				 */
/*************************************************************/
ROM_START( cosmognj )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "co1mpr0.bin",  0x000000, 0x020000, 0xd1b4c8db )
	ROM_LOAD_ODD(  "co1mpr1.bin",  0x000000, 0x020000, 0x2f391906 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "co1spr0.bin",  0x000000, 0x020000, 0xbba2c28f )
	ROM_LOAD_ODD(  "co1spr1.bin",  0x000000, 0x020000, 0xc029b459 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "co1snd0.bin",  0x00c000, 0x004000, 0x6bfa619f )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "co1obj0.bin",  0x000000, 0x5df8ce0c )
	NAMCOS2_GFXROM_LOAD_512K( "co1obj1.bin",  0x080000, 0x3d152497 )
	NAMCOS2_GFXROM_LOAD_512K( "co1obj2.bin",  0x100000, 0x4e50b6ee )
	NAMCOS2_GFXROM_LOAD_512K( "co1obj3.bin",  0x180000, 0x7beed669 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "co1chr0.bin",  0x000000, 0xee375b3e )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr1.bin",  0x080000, 0x0149de65 )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr2.bin",  0x100000, 0x93d565a0 )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr3.bin",  0x180000, 0x4d971364 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "co1roz0.bin",  0x000000, 0x2bea6951 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "co1sha0.bin",  0x000000, 0x063a70cc )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "co1dat0.bin",  0x000000, 0xb53da2ae )
	NAMCOS2_DATA_LOAD_O_128K( "co1dat1.bin",  0x000000, 0xd21ad10b )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "co1voi1.bin",  0x000000, 0x080000, 0xb5ba8f15 )
	ROM_LOAD( "co1voi2.bin",  0x080000, 0x080000, 0xb566b105 )
ROM_END


/*************************************************************/
/*					   DIRT FOX (JAPAN) 					 */
/*************************************************************/
ROM_START( dirtfoxj )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "df1_mpr0.bin",	0x000000, 0x020000, 0x8386c820 )
	ROM_LOAD_ODD(  "df1_mpr1.bin",	0x000000, 0x020000, 0x51085728 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "df1_spr0.bin",	0x000000, 0x020000, 0xd4906585 )
	ROM_LOAD_ODD(  "df1_spr1.bin",	0x000000, 0x020000, 0x7d76cf57 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "df1_snd0.bin",  0x00c000, 0x004000, 0x66b4f3ab )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "df1_obj0.bin",  0x000000, 0xb6bd1a68 )
	NAMCOS2_GFXROM_LOAD_512K( "df1_obj1.bin",  0x080000, 0x05421dc1 )
	NAMCOS2_GFXROM_LOAD_512K( "df1_obj2.bin",  0x100000, 0x9390633e )
	NAMCOS2_GFXROM_LOAD_512K( "df1_obj3.bin",  0x180000, 0xc8447b33 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "df1_chr0.bin",  0x000000, 0x4b10e4ed )
	NAMCOS2_GFXROM_LOAD_128K( "df1_chr1.bin",  0x080000, 0x8f63f3d6 )
	NAMCOS2_GFXROM_LOAD_128K( "df1_chr2.bin",  0x100000, 0x5a1b852a )
	NAMCOS2_GFXROM_LOAD_128K( "df1_chr3.bin",  0x180000, 0x28570676 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz0.bin",  0x000000, 0xa6129f94 )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz1.bin",  0x080000, 0xc8e7ce73 )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz2.bin",  0x100000, 0xc598e923 )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz3.bin",  0x180000, 0x5a38b062 )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz4.bin",  0x200000, 0xe196d2e8 )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz5.bin",  0x280000, 0x1f8a1a3c )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz6.bin",  0x300000, 0x7f3a1ed9 )
	NAMCOS2_GFXROM_LOAD_256K( "df1_roz7.bin",  0x380000, 0xdd546ae8 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "df1_sha.bin",  0x000000, 0x9a7c9a9b )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_256K( "df1_dat0.bin",  0x000000, 0xf5851c85 )
	NAMCOS2_DATA_LOAD_O_256K( "df1_dat1.bin",  0x000000, 0x1a31e46b )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "df1_voi1.bin",  0x000000, 0x080000, 0x15053904 )
ROM_END


/*************************************************************/
/*					   DRAGON SABER 						 */
/*************************************************************/
ROM_START( dsaber )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mpr0.bin",	0x000000, 0x020000, 0x45309ddc )
	ROM_LOAD_ODD(  "mpr1.bin",	0x000000, 0x020000, 0xcbfc4cba )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "spr0.bin",	0x000000, 0x010000, 0x013faf80 )
	ROM_LOAD_ODD(  "spr1.bin",	0x000000, 0x010000, 0xc36242bb )

	ROM_REGION( 0x050000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "snd0.bin",  0x00c000, 0x004000, 0xaf5b1ff8 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )
	ROM_LOAD( "snd1.bin",  0x030000, 0x020000, 0xc4ca6f3f )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj0.bin",  0x000000, 0xf08c6648 )
	NAMCOS2_GFXROM_LOAD_512K( "obj1.bin",  0x080000, 0x34e0810d )
	NAMCOS2_GFXROM_LOAD_512K( "obj2.bin",  0x100000, 0xbccdabf3 )
	NAMCOS2_GFXROM_LOAD_512K( "obj3.bin",  0x180000, 0x2a60a4b8 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "co1chr0.bin",  0x000000, 0xc6058df6 )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr1.bin",  0x080000, 0x67aaab36 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "roz0.bin",  0x000000, 0x32aab758 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "shape.bin",	0x000000, 0x698e7a3e )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "data0.bin",	0x000000, 0x3e53331f )
	NAMCOS2_DATA_LOAD_O_128K( "data1.bin",	0x000000, 0xd5427f11 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "voi1.bin",  0x000000, 0x080000, 0xdadf6a57 )
	ROM_LOAD( "voi2.bin",  0x080000, 0x080000, 0x81078e01 )
ROM_END


/*************************************************************/
/*					   DRAGON SABER (JAPAN) 				 */
/*************************************************************/
ROM_START( dsaberj )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "do1mpr0b.bin",	0x000000, 0x020000, 0x2898e791 )
	ROM_LOAD_ODD(  "do1mpr1b.bin",	0x000000, 0x020000, 0x5fa9778e )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "spr0.bin",	0x000000, 0x010000, 0x013faf80 )
	ROM_LOAD_ODD(  "spr1.bin",	0x000000, 0x010000, 0xc36242bb )

	ROM_REGION( 0x050000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "snd0.bin",  0x00c000, 0x004000, 0xaf5b1ff8 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )
	ROM_LOAD( "snd1.bin",  0x030000, 0x020000, 0xc4ca6f3f )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj0.bin",  0x000000, 0xf08c6648 )
	NAMCOS2_GFXROM_LOAD_512K( "obj1.bin",  0x080000, 0x34e0810d )
	NAMCOS2_GFXROM_LOAD_512K( "obj2.bin",  0x100000, 0xbccdabf3 )
	NAMCOS2_GFXROM_LOAD_512K( "obj3.bin",  0x180000, 0x2a60a4b8 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "co1chr0.bin",  0x000000, 0xc6058df6 )
	NAMCOS2_GFXROM_LOAD_512K( "co1chr1.bin",  0x080000, 0x67aaab36 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "roz0.bin",  0x000000, 0x32aab758 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "shape.bin",	0x000000, 0x698e7a3e )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "data0.bin",	0x000000, 0x3e53331f )
	NAMCOS2_DATA_LOAD_O_128K( "data1.bin",	0x000000, 0xd5427f11 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "voi1.bin",  0x000000, 0x080000, 0xdadf6a57 )
	ROM_LOAD( "voi2.bin",  0x080000, 0x080000, 0x81078e01 )
ROM_END


/*************************************************************/
/*					   FINAL LAP (REV E)					 */
/*************************************************************/
ROM_START( finallap )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fl2mp0e",  0x000000, 0x010000, 0xed805674 )
	ROM_LOAD_ODD(  "fl2mp1e",  0x000000, 0x010000, 0x4c1d523b )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fl1-sp0",  0x000000, 0x010000, 0x2c5ff15d )
	ROM_LOAD_ODD(  "fl1-sp1",  0x000000, 0x010000, 0xea9d1a2e )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fl1-s0b",  0x00c000, 0x004000, 0xf5d76989 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x000000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x080000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x100000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x180000, 0xdba830a2 )

	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x200000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x280000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x300000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x380000, 0xdba830a2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c0",  0x000000, 0xcd9d2966 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c1",  0x080000, 0xb0efec87 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c2",  0x100000, 0x263b8e31 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c3",  0x180000, 0xc2c56743 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c4",  0x200000, 0x83c77a50 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c5",  0x280000, 0xab89da77 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c6",  0x300000, 0x239bd9a0 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fl2-sha",  0x000000, 0x5fda0b6d )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	/* No DAT files present in ZIP archive - Must be wrong */

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fl1-v1",  0x000000, 0x020000, 0x86b21996 )
	ROM_RELOAD( 			  0x020000, 0x020000 )
	ROM_RELOAD( 			  0x040000, 0x020000 )
	ROM_RELOAD( 			  0x060000, 0x020000 )
	ROM_LOAD( "fl1-v2",  0x080000, 0x020000, 0x6a164647 )
	ROM_RELOAD( 			  0x0a0000, 0x020000 )
	ROM_RELOAD( 			  0x0c0000, 0x020000 )
	ROM_RELOAD( 			  0x0e0000, 0x020000 )
ROM_END


/*************************************************************/
/*					   FINAL LAP (revision D)				 */
/*************************************************************/
ROM_START( finalapd )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fl2-mp0d",	0x000000, 0x010000, 0x3576d3aa )
	ROM_LOAD_ODD(  "fl2-mp1d",	0x000000, 0x010000, 0x22d3906d )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fl1-sp0",  0x000000, 0x010000, 0x2c5ff15d )
	ROM_LOAD_ODD(  "fl1-sp1",  0x000000, 0x010000, 0xea9d1a2e )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fl1-s0b",  0x00c000, 0x004000, 0xf5d76989 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x000000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x080000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x100000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x180000, 0xdba830a2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c0",  0x000000, 0xcd9d2966 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c1",  0x080000, 0xb0efec87 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c2",  0x100000, 0x263b8e31 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c3",  0x180000, 0xc2c56743 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c4",  0x200000, 0x83c77a50 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c5",  0x280000, 0xab89da77 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c6",  0x300000, 0x239bd9a0 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fl2-sha",  0x000000, 0x5fda0b6d )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	/* No DAT files present in ZIP archive - Must be wrong */

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fl1-v1",  0x000000, 0x020000, 0x86b21996 )
	ROM_RELOAD( 			  0x020000, 0x020000 )
	ROM_RELOAD( 			  0x040000, 0x020000 )
	ROM_RELOAD( 			  0x060000, 0x020000 )
	ROM_LOAD( "fl1-v2",  0x080000, 0x020000, 0x6a164647 )
	ROM_RELOAD( 			  0x0a0000, 0x020000 )
	ROM_RELOAD( 			  0x0c0000, 0x020000 )
	ROM_RELOAD( 			  0x0e0000, 0x020000 )
ROM_END


/*************************************************************/
/*					   FINAL LAP (revision C)				 */
/*************************************************************/
ROM_START( finalapc )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fl2-mp0c",	0x000000, 0x010000, 0xf667f2c9 )
	ROM_LOAD_ODD(  "fl2-mp1c",	0x000000, 0x010000, 0xb8615d33 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fl1-sp0",  0x000000, 0x010000, 0x2c5ff15d )
	ROM_LOAD_ODD(  "fl1-sp1",  0x000000, 0x010000, 0xea9d1a2e )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fl1-s0",  0x00c000, 0x004000, 0x1f8ff494 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x000000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x080000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x100000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x180000, 0xdba830a2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c0",  0x000000, 0xcd9d2966 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c1",  0x080000, 0xb0efec87 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c2",  0x100000, 0x263b8e31 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c3",  0x180000, 0xc2c56743 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c4",  0x200000, 0x83c77a50 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c5",  0x280000, 0xab89da77 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c6",  0x300000, 0x239bd9a0 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fl2-sha",  0x000000, 0x5fda0b6d )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	/* No DAT files present in ZIP archive - Must be wrong */

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fl1-v1",  0x000000, 0x020000, 0x86b21996 )
	ROM_RELOAD( 			  0x020000, 0x020000 )
	ROM_RELOAD( 			  0x040000, 0x020000 )
	ROM_RELOAD( 			  0x060000, 0x020000 )
	ROM_LOAD( "fl1-v2",  0x080000, 0x020000, 0x6a164647 )
	ROM_RELOAD( 			  0x0a0000, 0x020000 )
	ROM_RELOAD( 			  0x0c0000, 0x020000 )
	ROM_RELOAD( 			  0x0e0000, 0x020000 )
ROM_END


/*************************************************************/
/*					   FINAL LAP (Rev C - Japan)			 */
/*************************************************************/
ROM_START( finlapjc )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fl1_mp0c.bin",	0x000000, 0x010000, 0x63cd7304 )
	ROM_LOAD_ODD(  "fl1_mp1c.bin",	0x000000, 0x010000, 0xcc9c5fb6 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fl1-sp0",  0x000000, 0x010000, 0x2c5ff15d )
	ROM_LOAD_ODD(  "fl1-sp1",  0x000000, 0x010000, 0xea9d1a2e )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fl1_s0b",  0x00c000, 0x004000, 0xf5d76989 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x000000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x080000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x100000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x180000, 0xdba830a2 )

	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x200000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x280000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x300000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x380000, 0xdba830a2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c0",  0x000000, 0xcd9d2966 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c1",  0x080000, 0xb0efec87 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c2",  0x100000, 0x263b8e31 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c3",  0x180000, 0xc2c56743 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c4",  0x200000, 0x83c77a50 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c5",  0x280000, 0xab89da77 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2-c6",  0x300000, 0x239bd9a0 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fl2-sha",  0x000000, 0x5fda0b6d )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	/* No DAT files present in ZIP archive - Must be wrong */

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fl1-v1",  0x000000, 0x020000, 0x86b21996 )
	ROM_RELOAD( 			  0x020000, 0x020000 )
	ROM_RELOAD( 			  0x040000, 0x020000 )
	ROM_RELOAD( 			  0x060000, 0x020000 )
	ROM_LOAD( "fl1-v2",  0x080000, 0x020000, 0x6a164647 )
	ROM_RELOAD( 			  0x0a0000, 0x020000 )
	ROM_RELOAD( 			  0x0c0000, 0x020000 )
	ROM_RELOAD( 			  0x0e0000, 0x020000 )
ROM_END


/*************************************************************/
/*					   FINAL LAP  (REV B - JAPAN)			 */
/*************************************************************/
ROM_START( finlapjb )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fl1_mp0b.bin",	0x000000, 0x010000, 0x870a482a )
	ROM_LOAD_ODD(  "fl1_mp1b.bin",	0x000000, 0x010000, 0xaf52c991 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fl1-sp0",  0x000000, 0x010000, 0x2c5ff15d )
	ROM_LOAD_ODD(  "fl1-sp1",  0x000000, 0x010000, 0xea9d1a2e )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fl1_s0.bin",  0x00c000, 0x004000, 0x1f8ff494 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x000000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x080000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x100000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x180000, 0xdba830a2 )

	NAMCOS2_GFXROM_LOAD_512K( "obj-0b",  0x200000, 0xc6986523 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-1b",  0x280000, 0x6af7d284 )
	NAMCOS2_GFXROM_LOAD_512K( "obj-2b",  0x300000, 0xde45ca8d )
	NAMCOS2_GFXROM_LOAD_512K( "obj-3b",  0x380000, 0xdba830a2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c0",  0x000000, 0xcd9d2966 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c1",  0x080000, 0xb0efec87 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c2",  0x100000, 0x263b8e31 )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c3",  0x180000, 0xc2c56743 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2_c4.bin",  0x200000, 0xcdc1de2e )
	NAMCOS2_GFXROM_LOAD_128K( "fl1-c5",  0x280000, 0xab89da77 )
	NAMCOS2_GFXROM_LOAD_128K( "fl2_c6.bin",  0x300000, 0x8e78a3c3 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fl1_sha.bin",  0x000000, 0xb7e1c7a3 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	/* No DAT files present in ZIP archive - Must be wrong */

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fl1-v1",  0x000000, 0x020000, 0x86b21996 )
	ROM_RELOAD( 			  0x020000, 0x020000 )
	ROM_RELOAD( 			  0x040000, 0x020000 )
	ROM_RELOAD( 			  0x060000, 0x020000 )
	ROM_LOAD( "fl1-v2",  0x080000, 0x020000, 0x6a164647 )
	ROM_RELOAD( 			  0x0a0000, 0x020000 )
	ROM_RELOAD( 			  0x0c0000, 0x020000 )
	ROM_RELOAD( 			  0x0e0000, 0x020000 )
ROM_END


/*************************************************************/
/*					   FINAL LAP 2							 */
/*************************************************************/
ROM_START( finalap2 )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fls2mp0b",	0x000000, 0x020000, 0x97b48aae )
	ROM_LOAD_ODD(  "fls2mp1b",	0x000000, 0x020000, 0xc9f3e0e7 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fls2sp0b",	0x000000, 0x020000, 0x8bf15d9c )
	ROM_LOAD_ODD(  "fls2sp1b",	0x000000, 0x020000, 0xc1a31086 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "flss0",	0x00c000, 0x004000, 0xc07cc10a )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj0",  0x000000, 0xeab19ec6 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj2",  0x080000, 0x2a3b7ded )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj4",  0x100000, 0x84aa500c )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj6",  0x180000, 0x33118e63 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj1",  0x200000, 0x4ef37a51 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj3",  0x280000, 0xb86dc7cd )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj5",  0x300000, 0x6a53e603 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj7",  0x380000, 0xb52a85e2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr0",  0x000000, 0xb3541a31 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr1",  0x080000, 0xb92fb6f9 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr2",  0x100000, 0x2e386ec8 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr3",  0x180000, 0x970255d3 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr4",  0x200000, 0x1328d87d )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr5",  0x280000, 0x67f535fd )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr6",  0x300000, 0x6aded8ce )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr7",  0x380000, 0x742bae28 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fls2sha",  0x000000, 0x95a63037 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_256K( "fls2dat0",  0x000000, 0xf1af432c )
	NAMCOS2_DATA_LOAD_O_256K( "fls2dat1",  0x000000, 0x8719533e )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "flsvoi1",  0x000000, 0x080000, 0x590be52f )
	ROM_LOAD( "flsvoi2",  0x080000, 0x080000, 0x204b3c27 )
ROM_END


/*************************************************************/
/*					   FINAL LAP 2 (Japan)					 */
/*************************************************************/
ROM_START( finalp2j )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fls1_mp0.bin",	0x000000, 0x020000, 0x05ea8090 )
	ROM_LOAD_ODD(  "fls1_mp1.bin",	0x000000, 0x020000, 0xfb189f50 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fls2sp0b",	0x000000, 0x020000, 0x8bf15d9c )
	ROM_LOAD_ODD(  "fls2sp1b",	0x000000, 0x020000, 0xc1a31086 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "flss0",	0x00c000, 0x004000, 0xc07cc10a )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj0",  0x000000, 0xeab19ec6 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj2",  0x080000, 0x2a3b7ded )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj4",  0x100000, 0x84aa500c )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj6",  0x180000, 0x33118e63 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj1",  0x200000, 0x4ef37a51 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj3",  0x280000, 0xb86dc7cd )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj5",  0x300000, 0x6a53e603 )
	NAMCOS2_GFXROM_LOAD_512K( "fl3obj7",  0x380000, 0xb52a85e2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr0",  0x000000, 0xb3541a31 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr1",  0x080000, 0xb92fb6f9 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr2",  0x100000, 0x2e386ec8 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr3",  0x180000, 0x970255d3 )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr4",  0x200000, 0x1328d87d )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr5",  0x280000, 0x67f535fd )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr6",  0x300000, 0x6aded8ce )
	NAMCOS2_GFXROM_LOAD_128K( "fls2chr7",  0x380000, 0x742bae28 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fls2sha",  0x000000, 0x95a63037 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_256K( "fls2dat0",  0x000000, 0xf1af432c )
	NAMCOS2_DATA_LOAD_O_256K( "fls2dat1",  0x000000, 0x8719533e )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "flsvoi1",  0x000000, 0x080000, 0x590be52f )
	ROM_LOAD( "flsvoi2",  0x080000, 0x080000, 0x204b3c27 )
ROM_END


/*************************************************************/
/*					   FINAL LAP 3							 */
/*************************************************************/
ROM_START( finalap3 )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fltmp0",  0x000000, 0x020000, 0x2f2a997a )
	ROM_LOAD_ODD(  "fltmp1",  0x000000, 0x020000, 0xb505ca0b )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "flt1sp0",  0x000000, 0x020000, 0xe804ced1 )
	ROM_LOAD_ODD(  "flt1sp1",  0x000000, 0x020000, 0x3a2b24ee )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "flt1snd0",  0x00c000, 0x004000, 0x60b72aed )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "fltobj0",  0x000000, 0xeab19ec6 )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj2",  0x080000, 0x2a3b7ded )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj4",  0x100000, 0x84aa500c )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj6",  0x180000, 0x33118e63 )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj1",  0x200000, 0x4ef37a51 )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj3",  0x280000, 0xb86dc7cd )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj5",  0x300000, 0x6a53e603 )
	NAMCOS2_GFXROM_LOAD_512K( "fltobj7",  0x380000, 0xb52a85e2 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fltchr0",  0x000000, 0x93d58fbb )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr1",  0x080000, 0xabbc411b )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr2",  0x100000, 0x7de05a4a )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr3",  0x180000, 0xac4e9b8a )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr4",  0x200000, 0x55c3434d )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr5",  0x280000, 0xfbaa5c89 )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr6",  0x300000, 0xe90279ce )
	NAMCOS2_GFXROM_LOAD_128K( "fltchr7",  0x380000, 0xb9c1ea47 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fltsha",  0x000000, 0x089dc194 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "flt1d0",  0x000000, 0x80004966 )
	NAMCOS2_DATA_LOAD_O_128K( "flt1d1",  0x000000, 0xa2e93e8c )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fltvoi1",  0x000000, 0x080000, 0x4fc7c0ba )
	ROM_LOAD( "fltvoi2",  0x080000, 0x080000, 0x409c62df )
ROM_END


/*************************************************************/
/*					   FINEST HOUR							 */
/*************************************************************/
ROM_START( finehour )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fh1_mp0.bin",  0x000000, 0x020000, 0x355d9119 )
	ROM_LOAD_ODD(  "fh1_mp1.bin",  0x000000, 0x020000, 0x647eb621 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fh1_sp0.bin",  0x000000, 0x020000, 0xaa6289e9 )
	ROM_LOAD_ODD(  "fh1_sp1.bin",  0x000000, 0x020000, 0x8532d5c7 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fh1_sd0.bin",  0x00c000, 0x004000, 0x059a9cfd )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "fh1_ob0.bin",  0x000000, 0xb1fd86f1 )
	NAMCOS2_GFXROM_LOAD_512K( "fh1_ob1.bin",  0x080000, 0x519c44ce )
	NAMCOS2_GFXROM_LOAD_512K( "fh1_ob2.bin",  0x100000, 0x9c5de4fa )
	NAMCOS2_GFXROM_LOAD_512K( "fh1_ob3.bin",  0x180000, 0x54d4edce )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "fh1_ch0.bin",  0x000000, 0x516900d1 )
	NAMCOS2_GFXROM_LOAD_256K( "fh1_ch1.bin",  0x080000, 0x964d06bd )
	NAMCOS2_GFXROM_LOAD_256K( "fh1_ch2.bin",  0x100000, 0xfbb9449e )
	NAMCOS2_GFXROM_LOAD_256K( "fh1_ch3.bin",  0x180000, 0xc18eda8a )
	NAMCOS2_GFXROM_LOAD_256K( "fh1_ch4.bin",  0x200000, 0x80dd188a )
	NAMCOS2_GFXROM_LOAD_256K( "fh1_ch5.bin",  0x280000, 0x40969876 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fh1_rz0.bin",  0x000000, 0x6c96c5c1 )
	NAMCOS2_GFXROM_LOAD_128K( "fh1_rz1.bin",  0x080000, 0x44699eb9 )
	NAMCOS2_GFXROM_LOAD_128K( "fh1_rz2.bin",  0x100000, 0x5ec14abf )
	NAMCOS2_GFXROM_LOAD_128K( "fh1_rz3.bin",  0x180000, 0x9f5a91b2 )
	NAMCOS2_GFXROM_LOAD_128K( "fh1_rz4.bin",  0x200000, 0x0b4379e6 )
	NAMCOS2_GFXROM_LOAD_128K( "fh1_rz5.bin",  0x280000, 0xe034e560 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_256K( "fh1_sha.bin",  0x000000, 0x15875eb0 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "fh1_dt0.bin",  0x000000, 0x2441c26f )
	NAMCOS2_DATA_LOAD_O_128K( "fh1_dt1.bin",  0x000000, 0x48154deb )
	NAMCOS2_DATA_LOAD_E_128K( "fh1_dt2.bin",  0x100000, 0x12453ba4 )
	NAMCOS2_DATA_LOAD_O_128K( "fh1_dt3.bin",  0x100000, 0x50bab9da )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fh1_vo1.bin",  0x000000, 0x080000, 0x07560fc7 )
ROM_END


/*************************************************************/
/*					   FOUR TRAX							 */
/*************************************************************/
ROM_START( fourtrax )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "fx2mp0",  0x000000, 0x020000, 0xf147cd6b )
	ROM_LOAD_ODD(  "fx2mp1",  0x000000, 0x020000, 0x8af4a309 )
//	ROM_LOAD_ODD(  "fx2mp1",  0x000000, 0x020000, 0xd1138c85 ) BAD ROM ?

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "fx2sp0",  0x000000, 0x020000, 0x48548e78 )
	ROM_LOAD_ODD(  "fx2sp1",  0x000000, 0x020000, 0xd2861383 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "fx1sd0",  0x00c000, 0x004000, 0xacccc934 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	ROM_LOAD( "fxobj0",  0x000000, 0x040000, 0x1aa60ffa )
	ROM_LOAD( "fxobj2",  0x040000, 0x040000, 0x243affc7 )
	ROM_LOAD( "fxobj4",  0x080000, 0x040000, 0x30add52a )
	ROM_LOAD( "fxobj6",  0x0c0000, 0x040000, 0xa2d5ce4a )
	ROM_LOAD( "fxobj8",  0x100000, 0x040000, 0xb165acab )
	ROM_LOAD( "fxobj10",  0x140000, 0x040000, 0x7a01e86f )
	ROM_LOAD( "fxobj12",  0x180000, 0x040000, 0xf5e23b78 )
	ROM_LOAD( "fxobj14",  0x1c0000, 0x040000, 0xc1658c77 )
	ROM_LOAD( "fxobj1",  0x200000, 0x040000, 0x7509bc09 )
	ROM_LOAD( "fxobj3",  0x240000, 0x040000, 0xb7e5d17d )
	ROM_LOAD( "fxobj5",  0x280000, 0x040000, 0xe3cd2776 )
	ROM_LOAD( "fxobj7",  0x2c0000, 0x040000, 0x4d91c929 )
	ROM_LOAD( "fxobj9",  0x300000, 0x040000, 0x90f0735b )
	ROM_LOAD( "fxobj11",  0x340000, 0x040000, 0x514b3fe5 )
	ROM_LOAD( "fxobj13",  0x380000, 0x040000, 0x04a25007 )
	ROM_LOAD( "fxobj15",  0x3c0000, 0x040000, 0x2bc909b3 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "fxchr0",  0x000000, 0x6658c1c3 )
	NAMCOS2_GFXROM_LOAD_128K( "fxchr1",  0x080000, 0x3a888943 )
	NAMCOS2_GFXROM_LOAD_128K( "fxch2",	0x100000, 0xfdf1e86b )
//	NAMCOS2_GFXROM_LOAD_128K( "fxchr2a",  0x100000, 0xa5d1ab10 ) BAD ROM
	NAMCOS2_GFXROM_LOAD_128K( "fxchr3",  0x180000, 0x47fa7e61 )
	NAMCOS2_GFXROM_LOAD_128K( "fxchr4",  0x200000, 0xc720c5f5 )
	NAMCOS2_GFXROM_LOAD_128K( "fxchr5",  0x280000, 0x9eacdbc8 )
	NAMCOS2_GFXROM_LOAD_128K( "fxchr6",  0x300000, 0xc3dba42e )
	NAMCOS2_GFXROM_LOAD_128K( "fxchr7",  0x380000, 0xc009f3ae )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files in zip, not sure if they are missing ? */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "fxsha",	0x000000, 0xf7aa4af7 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_256K( "fxdat0",  0x000000, 0x63abf69b )
	NAMCOS2_DATA_LOAD_O_256K( "fxdat1",  0x000000, 0x725bed14 )
	NAMCOS2_DATA_LOAD_E_256K( "fxdat2",  0x100000, 0x71e4a5a0 )
	NAMCOS2_DATA_LOAD_O_256K( "fxdat3",  0x100000, 0x605725f7 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "fxvoi1",  0x000000, 0x080000, 0x6173364f )
ROM_END


/*************************************************************/
/*						MARVEL LAND (USA)					 */
/*************************************************************/
ROM_START( marvland )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mv2_mpr0",	0x000000, 0x020000, 0xd8b14fee )
	ROM_LOAD_ODD(  "mv2_mpr1",	0x000000, 0x020000, 0x29ff2738 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "mv2_spr0",	0x000000, 0x010000, 0xaa418f29 )
	ROM_LOAD_ODD(  "mv2_spr1",	0x000000, 0x010000, 0xdbd94def )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "mv2_snd0",  0x0c000, 0x04000, 0xa5b99162 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj0.bin",  0x000000, 0x73a29361 )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj1.bin",  0x080000, 0xabbe4a99 )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj2.bin",  0x100000, 0x753659e0 )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj3.bin",  0x180000, 0xd1ce7339 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr0.bin",  0x000000, 0x1c7e8b4f )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr1.bin",  0x080000, 0x01e4cafd )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr2.bin",  0x100000, 0x198fcc6f )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr3.bin",  0x180000, 0xed6f22a5 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz0.bin",  0x000000, 0x7381a5a9 )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz1.bin",  0x080000, 0xe899482e )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz2.bin",  0x100000, 0xde141290 )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz3.bin",  0x180000, 0xe310324d )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz4.bin",  0x200000, 0x48ddc5a9 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_256K( "mv1-sha.bin",  0x000000, 0xa47db5d3 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "mv2_dat0",  0x000000, 0x62e6318b )
	NAMCOS2_DATA_LOAD_O_128K( "mv2_dat1",  0x000000, 0x8a6902ca )
	NAMCOS2_DATA_LOAD_E_128K( "mv2_dat2",  0x100000, 0xf5c6408c )
	NAMCOS2_DATA_LOAD_O_128K( "mv2_dat3",  0x100000, 0x6df76955 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "mv1-voi1.bin",  0x000000, 0x080000, 0xde5cac09 )
ROM_END


/*************************************************************/
/*						MARVEL LAND (JAPAN) 				 */
/*************************************************************/
ROM_START( marvlanj )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mv1-mpr0.bin",	0x000000, 0x010000, 0x8369120f )
	ROM_LOAD_ODD(  "mv1-mpr1.bin",	0x000000, 0x010000, 0x6d5442cc )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "mv1-spr0.bin",	0x000000, 0x010000, 0xc3909925 )
	ROM_LOAD_ODD(  "mv1-spr1.bin",	0x000000, 0x010000, 0x1c5599f5 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "mv1-snd0.bin",  0x0c000, 0x04000, 0x51b8ccd7 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj0.bin",  0x000000, 0x73a29361 )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj1.bin",  0x080000, 0xabbe4a99 )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj2.bin",  0x100000, 0x753659e0 )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-obj3.bin",  0x180000, 0xd1ce7339 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr0.bin",  0x000000, 0x1c7e8b4f )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr1.bin",  0x080000, 0x01e4cafd )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr2.bin",  0x100000, 0x198fcc6f )
	NAMCOS2_GFXROM_LOAD_256K( "mv1-chr3.bin",  0x180000, 0xed6f22a5 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz0.bin",  0x000000, 0x7381a5a9 )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz1.bin",  0x080000, 0xe899482e )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz2.bin",  0x100000, 0xde141290 )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz3.bin",  0x180000, 0xe310324d )
	NAMCOS2_GFXROM_LOAD_128K( "mv1-roz4.bin",  0x200000, 0x48ddc5a9 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_256K( "mv1-sha.bin",  0x000000, 0xa47db5d3 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "mv1-dat0.bin",  0x000000, 0xe15f412e )
	NAMCOS2_DATA_LOAD_O_128K( "mv1-dat1.bin",  0x000000, 0x73e1545a )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "mv1-voi1.bin",  0x000000, 0x080000, 0xde5cac09 )
ROM_END


/*************************************************************/
/*						   METAL HAWK						 */
/*************************************************************/
ROM_START( metlhawk )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mh2mp0c.11d",  0x000000, 0x020000, 0xcd7dae6e )
	ROM_LOAD_ODD(  "mh2mp1c.13d",  0x000000, 0x020000, 0xe52199fd )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "mh1sp0f.11k",  0x000000, 0x010000, 0x2c141fea )
	ROM_LOAD_ODD(  "mh1sp1f.13k",  0x000000, 0x010000, 0x8ccf98e0 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "mh1s0.7j",  0x0c000, 0x04000, 0x79e054cf )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-0.5d",  0x000000, 0x52ae6620 )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-1.5b",  0x080000, 0x2c2a1291 )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-2.6d",  0x100000, 0x6221b927 )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-3.6b",  0x180000, 0xfd09f2f1 )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-4.5c",  0x200000, 0xe3590e1a )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-5.5a",  0x280000, 0xb85c0d07 )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-6.6c",  0x300000, 0x90c4523d )
	NAMCOS2_GFXROM_LOAD_256K( "mhobj-7.6a",  0x380000, 0xf00edb39 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "mhchr-0.11n",  0x000000, 0xe2da1b14 )
	NAMCOS2_GFXROM_LOAD_128K( "mhchr-1.11p",  0x080000, 0x023c78f9 )
	NAMCOS2_GFXROM_LOAD_128K( "mhchr-2.11r",  0x100000, 0xece47e91 )
	NAMCOS2_GFXROM_LOAD_128K( "mh1c3.11s",	0x180000, 0x9303aa7f )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-0.2d",  0x000000, 0x30ade98f )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-1.2c",  0x080000, 0xa7fff42a )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-2.2b",  0x100000, 0x6abec820 )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-3.2a",  0x180000, 0xd53cec6d )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-4.3d",  0x200000, 0x922213e2 )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-5.3c",  0x280000, 0x78418a54 )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-6.3b",  0x300000, 0x6c74977e )
	NAMCOS2_GFXROM_LOAD_256K( "mhr0z-7.3a",  0x380000, 0x68a19cbd )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "mh1sha.7n",	0x000000, 0x6ac22294 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "mh1d0.13s",	0x000000, 0x8b178ac7 )
	NAMCOS2_DATA_LOAD_O_128K( "mh1d1.13p",	0x000000, 0x10684fd6 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "mhvoi-1.bin",  0x000000, 0x080000, 0x2723d137 )
	ROM_LOAD( "mhvoi-2.bin",  0x080000, 0x080000, 0xdbc92d91 )
ROM_END


/*************************************************************/
/*						   MIRAI NINJA						 */
/*************************************************************/
ROM_START( mirninja )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mn_mpr0e.bin",	0x000000, 0x010000, 0xfa75f977 )
	ROM_LOAD_ODD(  "mn_mpr1e.bin",	0x000000, 0x010000, 0x58ddd464 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "mn1_spr0.bin",	0x000000, 0x010000, 0x3f1a17be )
	ROM_LOAD_ODD(  "mn1_spr1.bin",	0x000000, 0x010000, 0x2bc66f60 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "mn_snd0.bin",  0x0c000, 0x04000, 0x6aa1ae84 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65b.bin",  0x008000, 0x008000, 0xe9f2922a )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj0.bin",  0x000000, 0x6bd1e290 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj1.bin",  0x080000, 0x5e8503be )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj2.bin",  0x100000, 0xa96d9b45 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj3.bin",  0x180000, 0x0086ef8b )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj4.bin",  0x200000, 0xb3f48755 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj5.bin",  0x280000, 0xc21e995b )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj6.bin",  0x300000, 0xa052c582 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_obj7.bin",  0x380000, 0x1854c6f5 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr0.bin",  0x000000, 0x4f66df26 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr1.bin",  0x080000, 0xf5de5ea7 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr2.bin",  0x100000, 0x9ff61924 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr3.bin",  0x180000, 0xba208bf5 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr4.bin",  0x200000, 0x0ef00aff )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr5.bin",  0x280000, 0x4cd9d377 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr6.bin",  0x300000, 0x114aca76 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_chr7.bin",  0x380000, 0x2d5705d3 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "mn_roz0.bin",  0x000000, 0x677a4f25 )
	NAMCOS2_GFXROM_LOAD_128K( "mn_roz1.bin",  0x080000, 0xf00ae572 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "mn_sha.bin",  0x000000, 0xc28af90f )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "mn1_dat0.bin",  0x000000, 0x104bcca8 )
	NAMCOS2_DATA_LOAD_O_128K( "mn1_dat1.bin",  0x000000, 0xd2a918fb )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "mn_voi1.bin",  0x000000, 0x080000, 0x2ca3573c )
	ROM_LOAD( "mn_voi2.bin",  0x080000, 0x080000, 0x466c3b47 )
ROM_END


/*************************************************************/
/*						   ORDYNE							 */
/*************************************************************/
ROM_START( ordyne )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "or1_mp0.bin",  0x000000, 0x020000, 0xf5929ed3 )
	ROM_LOAD_ODD ( "or1_mp1.bin",  0x000000, 0x020000, 0xc1c8c1e2 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "or1_sp0.bin",  0x000000, 0x010000, 0x01ef6638 )
	ROM_LOAD_ODD ( "or1_sp1.bin",  0x000000, 0x010000, 0xb632adc3 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "or1_sd0.bin",  0x00c000, 0x004000, 0xc41e5d22 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob0.bin",  0x000000, 0x67b2b9e4 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob1.bin",  0x080000, 0x8a54fa5e )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob2.bin",  0x100000, 0xa2c1cca0 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob3.bin",  0x180000, 0xe0ad292c )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob4.bin",  0x200000, 0x7aefba59 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob5.bin",  0x280000, 0xe4025be9 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob6.bin",  0x300000, 0xe284c30c )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ob7.bin",  0x380000, 0x262b7112 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "or1_ch0.bin",  0x000000, 0xe7c47934 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ch1.bin",  0x080000, 0x874b332d )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ch3.bin",  0x180000, 0x5471a834 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ch5.bin",  0x280000, 0xa7d3a296 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ch6.bin",  0x300000, 0x3adc09c8 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_ch7.bin",  0x380000, 0xf050a152 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "or1_rz0.bin",  0x000000, 0xc88a9e6b )
	NAMCOS2_GFXROM_LOAD_128K( "or1_rz1.bin",  0x080000, 0xc20cc749 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_rz2.bin",  0x100000, 0x148c9866 )
	NAMCOS2_GFXROM_LOAD_128K( "or1_rz3.bin",  0x180000, 0x4e727b7e )
	NAMCOS2_GFXROM_LOAD_128K( "or1_rz4.bin",  0x200000, 0x17b04396 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "or1_sha.bin",  0x000000, 0x7aec9dee )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "or1_dt0.bin",  0x000000, 0xde214f7a )
	NAMCOS2_DATA_LOAD_O_128K( "or1_dt1.bin",  0x000000, 0x25e3e6c8 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "or1_vo1.bin",  0x000000, 0x080000, 0x369e0bca )
	ROM_LOAD( "or1_vo2.bin",  0x080000, 0x080000, 0x9f4cd7b5 )
ROM_END


/*************************************************************/
/*						   PHELIOS							 */
/*************************************************************/
ROM_START( phelios )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "ps1mpr0.bin",  0x000000, 0x020000, 0xbfbe96c6 )
	ROM_LOAD_ODD ( "ps1mpr1.bin",  0x000000, 0x020000, 0xf5c0f883 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "ps1spr0.bin",  0x000000, 0x010000, 0xe9c6987e )
	ROM_LOAD_ODD ( "ps1spr1.bin",  0x000000, 0x010000, 0x02b074fb )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "ps1snd1.bin",  0x00c000, 0x004000, 0xda694838 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_256K( "psobj0.bin",  0x000000, 0xf323db2b )
	NAMCOS2_GFXROM_LOAD_256K( "psobj1.bin",  0x080000, 0xfaf7c2f5 )
	NAMCOS2_GFXROM_LOAD_256K( "psobj2.bin",  0x100000, 0x828178ba )
	NAMCOS2_GFXROM_LOAD_256K( "psobj3.bin",  0x180000, 0xe84771c8 )
	NAMCOS2_GFXROM_LOAD_256K( "psobj4.bin",  0x200000, 0x81ea86c6 )
	NAMCOS2_GFXROM_LOAD_256K( "psobj5.bin",  0x280000, 0xaaebd51a )
	NAMCOS2_GFXROM_LOAD_256K( "psobj6.bin",  0x300000, 0x032ea497 )
	NAMCOS2_GFXROM_LOAD_256K( "psobj7.bin",  0x380000, 0xf6183b36 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "pschr0.bin",  0x000000, 0x668b6670 )
	NAMCOS2_GFXROM_LOAD_128K( "pschr1.bin",  0x080000, 0x80c30742 )
	NAMCOS2_GFXROM_LOAD_128K( "pschr2.bin",  0x100000, 0xf4e11bf7 )
	NAMCOS2_GFXROM_LOAD_128K( "pschr3.bin",  0x180000, 0x97a93dc5 )
	NAMCOS2_GFXROM_LOAD_128K( "pschr4.bin",  0x200000, 0x81d965bf )
	NAMCOS2_GFXROM_LOAD_128K( "pschr5.bin",  0x280000, 0x8ca72d35 )
	NAMCOS2_GFXROM_LOAD_128K( "pschr6.bin",  0x300000, 0xda3543a9 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "psroz0.bin",  0x000000, 0x68043d7e )
	NAMCOS2_GFXROM_LOAD_128K( "psroz1.bin",  0x080000, 0x029802b4 )
	NAMCOS2_GFXROM_LOAD_128K( "psroz2.bin",  0x100000, 0xbf0b76dc )
	NAMCOS2_GFXROM_LOAD_128K( "psroz3.bin",  0x180000, 0x9c821455 )
	NAMCOS2_GFXROM_LOAD_128K( "psroz4.bin",  0x200000, 0x63a39b7a )
	NAMCOS2_GFXROM_LOAD_128K( "psroz5.bin",  0x280000, 0xfc5a99d0 )
	NAMCOS2_GFXROM_LOAD_128K( "psroz6.bin",  0x300000, 0xa2a17587 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "ps1-sha.bin",  0x000000, 0x58e26fcf )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "ps1dat0.bin",  0x000000, 0xee4194b0 )
	NAMCOS2_DATA_LOAD_O_128K( "ps1dat1.bin",  0x000000, 0x5b22d714 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "psvoi-1.bin",  0x000000, 0x080000, 0xf67376ed )
ROM_END


/*************************************************************/
/*					   ROLLING THUNDER 2					 */
/*************************************************************/
ROM_START( rthun2 )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mpr0.bin",	0x000000, 0x020000, 0xe09a3549 )
	ROM_LOAD_ODD(  "mpr1.bin",	0x000000, 0x020000, 0x09573bff )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "spr0.bin",	0x000000, 0x010000, 0x54c22ac5 )
	ROM_LOAD_ODD(  "spr1.bin",	0x000000, 0x010000, 0x060eb393 )

	ROM_REGION( 0x050000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "snd0.bin",  0x00c000, 0x004000, 0x55b7562a )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )
	ROM_LOAD( "snd1.bin",  0x030000, 0x020000, 0x00445a4f )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj0.bin",  0x000000, 0xe5cb82c1 )
	NAMCOS2_GFXROM_LOAD_512K( "obj1.bin",  0x080000, 0x19ebe9fd )
	NAMCOS2_GFXROM_LOAD_512K( "obj2.bin",  0x100000, 0x455c4a2f )
	NAMCOS2_GFXROM_LOAD_512K( "obj3.bin",  0x180000, 0xfdcae8a9 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "chr0.bin",  0x000000, 0x6f0e9a68 )
	NAMCOS2_GFXROM_LOAD_512K( "chr1.bin",  0x080000, 0x15e44adc )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "roz0.bin",  0x000000, 0x482d0554 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "shape.bin",	0x000000, 0xcf58fbbe )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "data0.bin",	0x000000, 0x0baf44ee )
	NAMCOS2_DATA_LOAD_O_128K( "data1.bin",	0x000000, 0x58a8daac )
	NAMCOS2_DATA_LOAD_E_128K( "data2.bin",	0x100000, 0x8e850a2a )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "voi1.bin",  0x000000, 0x080000, 0xe42027cd )
	ROM_LOAD( "voi2.bin",  0x080000, 0x080000, 0x0c4c2b66 )
ROM_END


/*************************************************************/
/*					   ROLLING THUNDER 2 (Japan)			 */
/*************************************************************/
ROM_START( rthun2j )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "mpr0j.bin",  0x000000, 0x020000, 0x2563b9ee )
	ROM_LOAD_ODD(  "mpr1j.bin",  0x000000, 0x020000, 0x14c4c564 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "spr0j.bin",  0x000000, 0x010000, 0xf8ef5150 )
	ROM_LOAD_ODD(  "spr1j.bin",  0x000000, 0x010000, 0x52ed3a48 )

	ROM_REGION( 0x050000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "snd0.bin",  0x00c000, 0x004000, 0x55b7562a )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )
	ROM_LOAD( "snd1.bin",  0x030000, 0x020000, 0x00445a4f )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "obj0.bin",  0x000000, 0xe5cb82c1 )
	NAMCOS2_GFXROM_LOAD_512K( "obj1.bin",  0x080000, 0x19ebe9fd )
	NAMCOS2_GFXROM_LOAD_512K( "obj2.bin",  0x100000, 0x455c4a2f )
	NAMCOS2_GFXROM_LOAD_512K( "obj3.bin",  0x180000, 0xfdcae8a9 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "chr0.bin",  0x000000, 0x6f0e9a68 )
	NAMCOS2_GFXROM_LOAD_512K( "chr1.bin",  0x080000, 0x15e44adc )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "roz0.bin",  0x000000, 0x482d0554 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "shape.bin",	0x000000, 0xcf58fbbe )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "data0.bin",	0x000000, 0x0baf44ee )
	NAMCOS2_DATA_LOAD_O_128K( "data1.bin",	0x000000, 0x58a8daac )
	NAMCOS2_DATA_LOAD_E_128K( "data2.bin",	0x100000, 0x8e850a2a )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "voi1.bin",  0x000000, 0x080000, 0xe42027cd )
	ROM_LOAD( "voi2.bin",  0x080000, 0x080000, 0x0c4c2b66 )
ROM_END


/*************************************************************/
/*					   STEEL GUNNER 2						 */
/*************************************************************/
ROM_START( sgunner2)
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "sns_mpr0.bin",	0x000000, 0x020000, 0xe7216ad7 )
	ROM_LOAD_ODD(  "sns_mpr1.bin",	0x000000, 0x020000, 0x6caef2ee )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "sns_spr0.bin",	0x000000, 0x010000, 0xe5e40ed0 )
	ROM_LOAD_ODD(  "sns_spr1.bin",	0x000000, 0x010000, 0x3a85a5e9 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "sns_snd0.bin",  0x00c000, 0x004000, 0xf079cd32 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj0.bin",  0x000000, 0xc762445c )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj1.bin",  0x080000, 0xe9e379d8 )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj2.bin",  0x100000, 0x0d076f6c )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj3.bin",  0x180000, 0x0fb01e8b )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj4.bin",  0x200000, 0x0b1be894 )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj5.bin",  0x280000, 0x416b14e1 )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj6.bin",  0x300000, 0xc2e94ed2 )
	NAMCOS2_GFXROM_LOAD_512K( "sns_obj7.bin",  0x380000, 0xfc1f26af )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "sns_chr0.bin",  0x000000, 0xcdc42b61 )
	NAMCOS2_GFXROM_LOAD_512K( "sns_chr1.bin",  0x080000, 0x42d4cbb7 )
	NAMCOS2_GFXROM_LOAD_512K( "sns_chr2.bin",  0x100000, 0x7dbaa14e )
	NAMCOS2_GFXROM_LOAD_512K( "sns_chr3.bin",  0x180000, 0xb562ff72 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* NO ROZ ROMS PRESENT IN ZIP */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "sns_sha0.bin",  0x000000, 0x5687e8c0 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "sns_dat0.bin",  0x000000, 0x48295d93 )
	NAMCOS2_DATA_LOAD_O_128K( "sns_dat1.bin",  0x000000, 0xb44cc656 )
	NAMCOS2_DATA_LOAD_E_128K( "sns_dat2.bin",  0x100000, 0xca2ae645 )
	NAMCOS2_DATA_LOAD_O_128K( "sns_dat3.bin",  0x100000, 0x203bb018 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "sns_voi1.bin",  0x000000, 0x080000, 0x219c97f7 )
	ROM_LOAD( "sns_voi2.bin",  0x080000, 0x080000, 0x562ec86b )
ROM_END


/*************************************************************/
/*					 SUPER WORLD STADIUM 92 				 */
/*************************************************************/
ROM_START( sws92 )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "sss1mpr0.bin",	0x000000, 0x020000, 0xdbea0210 )
	ROM_LOAD_ODD(  "sss1mpr1.bin",	0x000000, 0x020000, 0xb5e6469a )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "sst1spr0.bin",	0x000000, 0x020000, 0x9777ee2f )
	ROM_LOAD_ODD(  "sst1spr1.bin",	0x000000, 0x020000, 0x27a35c69 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "sst1snd0.bin",  0x00c000, 0x004000, 0x8fc45114 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c68.bin",  0x008000, 0x008000, 0xca64550a )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "sss_obj0.bin",  0x000000, 0x375e8f1f )
	NAMCOS2_GFXROM_LOAD_512K( "sss_obj1.bin",  0x080000, 0x675c1014 )
	NAMCOS2_GFXROM_LOAD_512K( "sss_obj2.bin",  0x100000, 0xbdc55f1c )
	NAMCOS2_GFXROM_LOAD_512K( "sss_obj3.bin",  0x180000, 0xe32ac432 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "sss_chr0.bin",  0x000000, 0x1d2876f2 )
	NAMCOS2_GFXROM_LOAD_512K( "sss_chr6.bin",  0x300000, 0x354f0ed2 )
	NAMCOS2_GFXROM_LOAD_512K( "sss_chr7.bin",  0x380000, 0x4032f4c1 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "ss_roz0.bin",  0x000000, 0x40ce9a58 )
	NAMCOS2_GFXROM_LOAD_512K( "ss_roz1.bin",  0x080000, 0xc98902ff )
	NAMCOS2_GFXROM_LOAD_512K( "sss_roz2.bin",  0x100000, 0xc9855c10 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "sss_sha0.bin",  0x000000, 0xb71a731a )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_256K( "sss1dat0.bin",  0x000000, 0xdb3e6aec )
	NAMCOS2_DATA_LOAD_O_256K( "sss1dat1.bin",  0x000000, 0x463b5ba8 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "ss_voi1.bin",  0x000000, 0x080000, 0x503e51b7 )
ROM_END


/*************************************************************/
/*					 SUPER WORLD STADIUM 93 				 */
/*************************************************************/
ROM_START( sws93 )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "sst1mpr0.bin",	0x000000, 0x020000, 0xbd2679bc )
	ROM_LOAD_ODD(  "sst1mpr1.bin",	0x000000, 0x020000, 0x9132e220 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "sst1spr0.bin",	0x000000, 0x020000, 0x9777ee2f )
	ROM_LOAD_ODD(  "sst1spr1.bin",	0x000000, 0x020000, 0x27a35c69 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "sst1snd0.bin",  0x00c000, 0x004000, 0x8fc45114 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "sst_obj0.bin",  0x000000, 0x4089dfd7 )
	NAMCOS2_GFXROM_LOAD_512K( "sst_obj1.bin",  0x080000, 0xcfbc25c7 )
	NAMCOS2_GFXROM_LOAD_512K( "sst_obj2.bin",  0x100000, 0x61ed3558 )
	NAMCOS2_GFXROM_LOAD_512K( "sst_obj3.bin",  0x180000, 0x0e3bc05d )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "sst_chr0.bin",  0x000000, 0x3397850d )
	NAMCOS2_GFXROM_LOAD_512K( "sss_chr6.bin",  0x300000, 0x354f0ed2 )
	NAMCOS2_GFXROM_LOAD_512K( "sst_chr7.bin",  0x380000, 0xe0abb763 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "ss_roz0.bin",  0x000000, 0x40ce9a58 )
	NAMCOS2_GFXROM_LOAD_512K( "ss_roz1.bin",  0x080000, 0xc98902ff )
	NAMCOS2_GFXROM_LOAD_512K( "sss_roz2.bin",  0x100000, 0xc9855c10 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "sst_sha0.bin",  0x000000, 0x4f64d4bd )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_512K( "sst1dat0.bin",  0x000000, 0xb99c9656 )
	NAMCOS2_DATA_LOAD_O_512K( "sst1dat1.bin",  0x000000, 0x60cf6281 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "ss_voi1.bin",  0x000000, 0x080000, 0x503e51b7 )
ROM_END


/*************************************************************/
/*					   SUZUKA 8 HOURS						 */
/*************************************************************/
ROM_START( suzuka8h )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "eh1-mp0b.bin",	0x000000, 0x020000, 0x2850f469 )
	ROM_LOAD_ODD(  "eh1-mpr1.bin",	0x000000, 0x020000, 0xbe83eb2c )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "eh1-sp0.bin",  0x000000, 0x020000, 0x4a8c4709 )
	ROM_LOAD_ODD(  "eh1-sp1.bin",  0x000000, 0x020000, 0x2256b14e )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "eh1-snd0.bin",  0x00c000, 0x004000, 0x36748d3c )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, BADCRC(0xa5b2a4ff))

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj0.bin",  0x000000, 0x864b6816 )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj2.bin",  0x080000, 0x966d3f19 )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj4.bin",  0x100000, 0xcde13867 )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj6.bin",  0x180000, 0x6019fc8c )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj1.bin",  0x200000, 0xd4921c35 )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj3.bin",  0x280000, 0x7d253cbe )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj5.bin",  0x300000, 0x9f210546 )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-obj7.bin",  0x380000, 0x0bd966b8 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "eh1-chr0.bin",  0x000000, 0xbc90ebef )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-chr1.bin",  0x080000, 0x61395018 )
	NAMCOS2_GFXROM_LOAD_512K( "eh1-chr2.bin",  0x100000, 0x8150f644 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "eh1-shrp.bin",  0x000000, 0x39585cf9 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "eh1-d0.bin",  0x000000, 0xb3c4243b )
	NAMCOS2_DATA_LOAD_O_128K( "eh1-d1.bin",  0x000000, 0xc946e79c )
	NAMCOS2_DATA_LOAD_O_128K( "eh1-d3.bin",  0x100000, 0x8425a9c7 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "eh1-voi1.bin",  0x000000, 0x080000, 0x71e534d3 )
	ROM_LOAD( "eh1-voi2.bin",  0x080000, 0x080000, 0x3e20df8e )
ROM_END


/*************************************************************/
/*					   SUZUKA 8 HOURS 2 					 */
/*************************************************************/
ROM_START( suzuk8h2 )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "ehs2mp0b.bin",	0x000000, 0x020000, 0xade97f90 )
	ROM_LOAD_ODD(  "ehs2mp1b.bin",	0x000000, 0x020000, 0x19744a66 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "ehs1sp0.bin",  0x000000, 0x020000, 0x9ca967bc )
	ROM_LOAD_ODD(  "ehs1sp1.bin",  0x000000, 0x020000, 0xf25bfaaa )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "ehs1snd0.bin",  0x00c000, 0x004000, 0xfc95993b )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj0.bin",  0x000000, 0xa0acf307 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj2.bin",  0x080000, 0x83b45afe )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj4.bin",  0x100000, 0x4e503ca5 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj6.bin",  0x180000, 0xf5fc8b23 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj1.bin",  0x200000, 0xca780b44 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj3.bin",  0x280000, 0x360c03a8 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj5.bin",  0x300000, 0x5405f2d9 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1obj7.bin",  0x380000, 0xda6bf51b )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr0.bin", 0x000000, 0x844efe0d )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr1.bin", 0x080000, 0xe8480a6d )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr2.bin", 0x100000, 0xace2d871 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr3.bin", 0x180000, 0xc1680818 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr4.bin", 0x200000, 0x82e8c1d5 )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr5.bin", 0x280000, 0x9448537c )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr6.bin", 0x300000, 0x2d1c01ad )
	NAMCOS2_GFXROM_LOAD_512K( "ehs1chr7.bin", 0x380000, 0x18dd8676 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	/* No ROZ files present in ZIP archive */

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_512K( "ehs1shap.bin",  0x000000, 0x0f0e2dbf )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_512K( "ehs1dat0.bin",  0x000000, 0x12a202fb )
	NAMCOS2_DATA_LOAD_O_512K( "ehs1dat1.bin",  0x000000, 0x91790905 )
	NAMCOS2_DATA_LOAD_E_512K( "ehs1dat2.bin",  0x100000, 0x087da1f3 )
	NAMCOS2_DATA_LOAD_O_512K( "ehs1dat3.bin",  0x100000, 0x85aecb3f )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "ehs1voi1.bin",  0x000000, 0x080000, 0xbf94eb42 )
	ROM_LOAD( "ehs1voi2.bin",  0x080000, 0x080000, 0x0e427604 )
ROM_END


/*************************************************************/
/*					   LEGEND OF THE VALKYRIE				 */
/*************************************************************/
ROM_START( valkyrie )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "wd1mpr0.bin",  0x000000, 0x020000, 0x94111a2e )
	ROM_LOAD_ODD(  "wd1mpr1.bin",  0x000000, 0x020000, 0x57b5051c )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "wd1spr0.bin",  0x000000, 0x010000, 0xb2398321 )
	ROM_LOAD_ODD(  "wd1spr1.bin",  0x000000, 0x010000, 0x38dba897 )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "wd1snd0.bin",  0x00c000, 0x004000, 0xd0fbf58b )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_256K( "wdobj0.bin",  0x000000, 0xe8089451 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj1.bin",  0x080000, 0x7ca65666 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj2.bin",  0x100000, 0x7c159407 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj3.bin",  0x180000, 0x649f8760 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj4.bin",  0x200000, 0x7ca39ae7 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj5.bin",  0x280000, 0x9ead2444 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj6.bin",  0x300000, 0x9fa2ea21 )
	NAMCOS2_GFXROM_LOAD_256K( "wdobj7.bin",  0x380000, 0x66e07a36 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "wdchr0.bin",  0x000000, 0xdebb0116 )
	NAMCOS2_GFXROM_LOAD_128K( "wdchr1.bin",  0x080000, 0x8a1431e8 )
	NAMCOS2_GFXROM_LOAD_128K( "wdchr2.bin",  0x100000, 0x62f75f69 )
	NAMCOS2_GFXROM_LOAD_128K( "wdchr3.bin",  0x180000, 0xcc43bbe7 )
	NAMCOS2_GFXROM_LOAD_128K( "wdchr4.bin",  0x200000, 0x2f73d05e )
	NAMCOS2_GFXROM_LOAD_128K( "wdchr5.bin",  0x280000, 0xb632b2ec )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "wdroz0.bin",  0x000000, 0xf776bf66 )
	NAMCOS2_GFXROM_LOAD_128K( "wdroz1.bin",  0x080000, 0xc1a345c3 )
	NAMCOS2_GFXROM_LOAD_128K( "wdroz2.bin",  0x100000, 0x28ffb44a )
	NAMCOS2_GFXROM_LOAD_128K( "wdroz3.bin",  0x180000, 0x7e77b46d )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "wdshape.bin",  0x000000, 0x3b5e0249 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "wd1dat0.bin",  0x000000, 0xea209f48 )
	NAMCOS2_DATA_LOAD_O_128K( "wd1dat1.bin",  0x000000, 0x04b48ada )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "wd1voi1.bin",  0x000000, 0x040000, 0xf1ace193 )
	ROM_RELOAD( 			  0x040000, 0x040000 )
	ROM_LOAD( "wd1voi2.bin",  0x080000, 0x020000, 0xe95c5cf3 )
	ROM_RELOAD( 			  0x0a0000, 0x020000 )
	ROM_RELOAD( 			  0x0c0000, 0x020000 )
	ROM_RELOAD( 			  0x0e0000, 0x020000 )
ROM_END

/*************************************************************/
/*					   KYUUKAI DOUCHUUKI					 */
/*************************************************************/
ROM_START( kyukaidk )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "ky1_mp0b.bin", 0x000000, 0x010000, 0xd1c992c8 )
	ROM_LOAD_ODD(  "ky1_mp1b.bin", 0x000000, 0x010000, 0x723553af )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "ky1_sp0.bin",  0x000000, 0x010000, 0x4b4d2385 )
	ROM_LOAD_ODD(  "ky1_sp1.bin",  0x000000, 0x010000, 0xbd3368cd )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "ky1_s0.bin",   0x00c000, 0x004000, 0x27aea3e9 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o0.bin",  0x000000, 0xebec5132 )
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o1.bin",  0x080000, 0xfde7e5ae )
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o2.bin",  0x100000, 0x2a181698 )
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o3.bin",  0x180000, 0x71fcd3a6 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c0.bin",  0x000000, 0x7bd69a2d )
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c1.bin",  0x080000, 0x66a623fe )
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c2.bin",  0x100000, 0xe84b3dfd )
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c3.bin",  0x180000, 0x69e67c86 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r0.bin",  0x000000, 0x9213e8c4 )
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r1.bin",  0x080000, 0x97d1a641 )
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r2.bin",  0x100000, 0x39b58792 )
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r3.bin",  0x180000, 0x90c60d92 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "ky1_sha.bin",  0x000000, 0x380a20d7 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "ky1_d0.bin",   0x000000, 0xc9cf399d )
	NAMCOS2_DATA_LOAD_O_128K( "ky1_d1.bin",   0x000000, 0x6d4f21b9 )
	NAMCOS2_DATA_LOAD_E_128K( "ky1_d2.bin",   0x100000, 0xeb6d19c8 )
	NAMCOS2_DATA_LOAD_O_128K( "ky1_d3.bin",   0x100000, 0x95674701 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "ky1_v1.bin", 0x000000, 0x080000, 0x5ff81aec )
ROM_END

/*************************************************************/
/*					   KYUUKAI DOUCHUUKI (OLD)				 */
/*************************************************************/
ROM_START( kyukaido )
	ROM_REGION( 0x040000, REGION_CPU1 ) 							   /* Master CPU */
	ROM_LOAD_EVEN( "ky1_mp0.bin",  0x000000, 0x010000, 0x01978a19 )
	ROM_LOAD_ODD(  "ky1_mp1.bin",  0x000000, 0x010000, 0xb40717a7 )

	ROM_REGION( 0x040000, REGION_CPU2 ) 							   /* Slave CPU */
	ROM_LOAD_EVEN( "ky1_sp0.bin",  0x000000, 0x010000, 0x4b4d2385 )
	ROM_LOAD_ODD(  "ky1_sp1.bin",  0x000000, 0x010000, 0xbd3368cd )

	ROM_REGION( 0x030000, REGION_CPU3 ) 							   /* Sound CPU (Banked) */
	ROM_LOAD( "ky1_s0.bin",   0x00c000, 0x004000, 0x27aea3e9 )
	ROM_CONTINUE(			  0x010000, 0x01c000 )
	ROM_RELOAD( 			  0x010000, 0x020000 )

	ROM_REGION( 0x010000, REGION_CPU4 ) 							   /* I/O MCU */
	ROM_LOAD( "sys2mcpu.bin",  0x000000, 0x002000, 0xa342a97e )
	ROM_LOAD( "sys2c65c.bin",  0x008000, 0x008000, 0xa5b2a4ff )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )		   /* Sprites */
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o0.bin",  0x000000, 0xebec5132 )
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o1.bin",  0x080000, 0xfde7e5ae )
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o2.bin",  0x100000, 0x2a181698 )
	NAMCOS2_GFXROM_LOAD_512K( "ky1_o3.bin",  0x180000, 0x71fcd3a6 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )		   /* Tiles */
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c0.bin",  0x000000, 0x7bd69a2d )
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c1.bin",  0x080000, 0x66a623fe )
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c2.bin",  0x100000, 0xe84b3dfd )
	NAMCOS2_GFXROM_LOAD_128K( "ky1_c3.bin",  0x180000, 0x69e67c86 )

	ROM_REGION( 0x400000, REGION_GFX3 ) 							   /* ROZ Tiles */
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r0.bin",  0x000000, 0x9213e8c4 )
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r1.bin",  0x080000, 0x97d1a641 )
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r2.bin",  0x100000, 0x39b58792 )
	NAMCOS2_GFXROM_LOAD_256K( "ky1_r3.bin",  0x180000, 0x90c60d92 )

	ROM_REGION( 0x080000, REGION_GFX4 ) 							   /* Mask shape */
	NAMCOS2_GFXROM_LOAD_128K( "ky1_sha.bin",  0x000000, 0x380a20d7 )

	ROM_REGION( 0x200000, REGION_USER1 )							   /* Shared data roms */
	NAMCOS2_DATA_LOAD_E_128K( "ky1_d0.bin",   0x000000, 0xc9cf399d )
	NAMCOS2_DATA_LOAD_O_128K( "ky1_d1.bin",   0x000000, 0x6d4f21b9 )
	NAMCOS2_DATA_LOAD_E_128K( "ky1_d2.bin",   0x100000, 0xeb6d19c8 )
	NAMCOS2_DATA_LOAD_O_128K( "ky1_d3.bin",   0x100000, 0x95674701 )

	ROM_REGION( 0x100000, REGION_SOUND1 )							   /* Sound voices */
	ROM_LOAD( "ky1_v1.bin", 0x000000, 0x080000, 0x5ff81aec )
ROM_END


/*************************************************************/
/* Set gametype so that the protection code knows how to	 */
/* emulate the correct responses							 */
/*															 */
/* 0x4e71 == 68000 NOP										 */
/* 0x4e75 == 68000 RTS										 */
/*															 */
/*************************************************************/

void init_assault(void)
{
	namcos2_gametype=NAMCOS2_ASSAULT;
}

void init_assaultj(void)
{
	namcos2_gametype=NAMCOS2_ASSAULT_JP;
}

void init_assaultp(void)
{
	namcos2_gametype=NAMCOS2_ASSAULT_PLUS;
}

void init_burnforc(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_BURNING_FORCE;
	WRITE_WORD( &RAM[0x001e18], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x003a9c], 0x4e75 );	// Patch $d00000 checks
}

void init_cosmogng(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_COSMO_GANG;
	WRITE_WORD( &RAM[0x0034d2], 0x4e75 );	// Patch $d00000 checks
}


void init_dsaber(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_DRAGON_SABER;
	WRITE_WORD( &RAM[0x001172], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x00119c], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x002160], 0x4e75 );	// Patch $d00000 checks
}

void init_dsaberj(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_DRAGON_SABER_JP;
	WRITE_WORD( &RAM[0x001172], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0011a4], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x002160], 0x4e75 );	// Patch $d00000 checks
}

void init_dirtfoxj(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_DIRT_FOX_JP;
	WRITE_WORD( &RAM[0x008876], 0x4e75 );	// Patch $d00000 checks

	/* HACK TO MAKE STEERING WORK */
	WRITE_WORD( &RAM[0x00cd0a], 0x007f );
}

void init_finallap(void)
{
	namcos2_gametype=NAMCOS2_FINAL_LAP;
}

void init_finalap2(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_FINAL_LAP_2;
	WRITE_WORD( &RAM[0x004028], 0x4e71 );	// Patch some protection checks
	WRITE_WORD( &RAM[0x00402a], 0x4e71 );
	WRITE_WORD( &RAM[0x00403e], 0x4e71 );
	WRITE_WORD( &RAM[0x004040], 0x4e71 );
}

void init_finalp2j(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_FINAL_LAP_2;
	WRITE_WORD( &RAM[0x003de2], 0x4e71 );	// Patch some protection checks
	WRITE_WORD( &RAM[0x003de4], 0x4e71 );
	WRITE_WORD( &RAM[0x003df8], 0x4e71 );
	WRITE_WORD( &RAM[0x003dfa], 0x4e71 );
	WRITE_WORD( &RAM[0x004028], 0x4e71 );
	WRITE_WORD( &RAM[0x00402a], 0x4e71 );
	WRITE_WORD( &RAM[0x00403e], 0x4e71 );
	WRITE_WORD( &RAM[0x004040], 0x4e71 );
}

void init_finalap3(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_FINAL_LAP_3;
	WRITE_WORD( &RAM[0x003f36], 0x4e71 );	// Patch some nasty protection
	WRITE_WORD( &RAM[0x003f38], 0x4e71 );	// this stuff runs in the IRQ
	WRITE_WORD( &RAM[0x003f4c], 0x4e71 );
	WRITE_WORD( &RAM[0x003f4e], 0x4e71 );
	WRITE_WORD( &RAM[0x003f66], 0x4e71 );
	WRITE_WORD( &RAM[0x003f68], 0x4e71 );
	WRITE_WORD( &RAM[0x003f7c], 0x4e71 );
	WRITE_WORD( &RAM[0x003f7e], 0x4e71 );
}

void init_finehour(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_FINEST_HOUR;
	WRITE_WORD( &RAM[0x001892], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x003ac0], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x00467c], 0x4e71 );	// Patch $d00000 checks
}

void init_fourtrax(void)
{
	namcos2_gametype=NAMCOS2_FOUR_TRAX;
}

void init_kyukaidk(void)
{
	namcos2_gametype=NAMCOS2_KYUUKAI_DOUCHUUKI;
}

void init_marvlanj(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_MARVEL_LAND;
	WRITE_WORD( &RAM[0x000f24], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x001fb2], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0048b6], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0048d2], 0x4e75 );	// Patch $d00000 checks
}

void init_marvland(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_MARVEL_LAND;
	WRITE_WORD( &RAM[0x00101e], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x00223a], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x004cf4], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x004d10], 0x4e75 );	// Patch $d00000 checks
}

void init_metlhawk(void)
{
	namcos2_gametype=NAMCOS2_METAL_HAWK;
}

void init_mirninja(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_MIRAI_NINJA;
	WRITE_WORD( &RAM[0x00052a], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x01de68], 0x4e75 );	// Patch $d00000 checks
}

void init_ordyne(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_ORDYNE;
	WRITE_WORD( &RAM[0x0025a4], 0x4e75 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0025c2], 0x4e75 );	// Patch $d00000 checks
}

void init_phelios(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_PHELIOS;
	WRITE_WORD( &RAM[0x0011ea], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0011ec], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0011f6], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0011f8], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x00120a], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x00120c], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x001216], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x001218], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x001222], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x001224], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x00122e], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x001230], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x02607a], 0x4e75 );	// Patch $d00000 checks
}

void init_rthun2(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_ROLLING_THUNDER_2;
	WRITE_WORD( &RAM[0x0042b0], 0x4e71 );	// Patch $d00000 checks

	WRITE_WORD( &RAM[0x004260], 0x33fc );	// Protection patch, replace
	WRITE_WORD( &RAM[0x004262], 0x0000 );	//
	WRITE_WORD( &RAM[0x004264], 0x0010 );	// move.w $d00004,$100002
	WRITE_WORD( &RAM[0x004266], 0x0002 );	//		   with
	WRITE_WORD( &RAM[0x004268], 0x4e71 );	// move.w #$0001,$100002
}

void init_rthun2j(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_ROLLING_THUNDER_2;
	WRITE_WORD( &RAM[0x0040d2], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0149cc], 0x4e75 );	// Patch $d00000 checks

	WRITE_WORD( &RAM[0x004082], 0x33fc );	// Protection patch, replace
	WRITE_WORD( &RAM[0x004084], 0x0000 );	//
	WRITE_WORD( &RAM[0x004086], 0x0010 );	// move.w $d00004,$100002
	WRITE_WORD( &RAM[0x004088], 0x0002 );	//		   with
	WRITE_WORD( &RAM[0x00408a], 0x4e71 );	// move.w #$0001,$100002
}

void init_sgunner2(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_STEEL_GUNNER_2;
	WRITE_WORD( &RAM[0x001162], 0x4e71 );	// Patch $a00000 checks
}

void init_sws92(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_SUPER_WSTADIUM_92;
	WRITE_WORD( &RAM[0x0011fc], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0011fe], 0x4e71 );	// Patch $d00000 checks
}

void init_sws93(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1 );
	namcos2_gametype=NAMCOS2_SUPER_WSTADIUM_93;
	WRITE_WORD( &RAM[0x0013ae], 0x4e71 );	// Patch $d00000 checks
	WRITE_WORD( &RAM[0x0013b0], 0x4e71 );	// Patch $d00000 checks
}

void init_suzuka8h(void)
{
	namcos2_gametype=NAMCOS2_SUZUKA_8_HOURS;
}

void init_suzuk8h2(void)
{
	unsigned char *RAM=memory_region(REGION_CPU1);
	namcos2_gametype=NAMCOS2_SUZUKA_8_HOURS_2;
	WRITE_WORD( &RAM[0x003ec8], 0x4e71 );	// Patch some protection checks
	WRITE_WORD( &RAM[0x003ede], 0x4e71 );
	WRITE_WORD( &RAM[0x003ee0], 0x4e71 );
	WRITE_WORD( &RAM[0x003eee], 0x4e71 );
}

void init_valkyrie(void)
{
	namcos2_gametype=NAMCOS2_VALKYRIE;
}

/* Missing ROM sets/games */

/* Bubble Trouble */
/* Super World Stadium */
/* Steel Gunner */
/* Golly Ghost */
/* Lucky & Wild */

/* Based on the dumped BIOS versions it looks like Namco changed the BIOS rom */
/* from sys2c65b to sys2c65c sometime between 1988 and 1990 as mirai ninja	  */
/* and metal hawk have the B version and dragon saber has the C version 	  */



/* In order of appearance...... */

//    YEAR, NAME,     PARENT  , MACHINE,  INPUT  ,  INIT,     MONITOR     , COMPANY, FULLNAME,						 FLAGS
GAMEX(1987, finallap, 0,        driving,  driving,  finallap, ROT0,         "Namco", "Final Lap (Rev E)", GAME_NOT_WORKING)
GAMEX(1987, finalapd, finallap, driving,  driving,  finallap, ROT0,         "Namco", "Final Lap (Rev D)", GAME_NOT_WORKING)
GAMEX(1987, finalapc, finallap, driving,  driving,  finallap, ROT0,         "Namco", "Final Lap (Rev C)", GAME_NOT_WORKING)
GAMEX(1987, finlapjc, finallap, driving,  driving,  finallap, ROT0,         "Namco", "Final Lap (Japan - Rev C)", GAME_NOT_WORKING)
GAMEX(1987, finlapjb, finallap, driving,  driving,  finallap, ROT0,         "Namco", "Final Lap (Japan - Rev B)", GAME_NOT_WORKING)
GAME( 1988, assault,  0,        default,  assault,  assault , ROT90_16BIT,  "Namco", "Assault" )
GAME( 1988, assaultj, assault,  default,  assault,  assaultj, ROT90_16BIT,  "Namco", "Assault (Japan)" )
GAME( 1988, assaultp, assault,  default,  assault,  assaultp, ROT90_16BIT,  "Namco", "Assault Plus (Japan)" )
GAMEX(1988, metlhawk, 0,        metlhawk, metlhawk, metlhawk, ROT90_16BIT,  "Namco", "Metal Hawk (Japan)", GAME_NOT_WORKING)
GAME( 1988, mirninja, 0,        default,  default,  mirninja, ROT0_16BIT,   "Namco", "Mirai Ninja (Japan)" )
GAME( 1988, ordyne,   0,        default,  default,  ordyne,   ROT180_16BIT, "Namco", "Ordyne (Japan)" )
GAME( 1988, phelios,  0,        default,  default,  phelios , ROT90_16BIT,  "Namco", "Phelios (Japan)" )
GAME( 1989, burnforc, 0,        default,  default,  burnforc, ROT0_16BIT,   "Namco", "Burning Force (Japan)" )
GAME( 1989, dirtfoxj, 0,        default,  dirtfox,  dirtfoxj, ROT90_16BIT,  "Namco", "Dirt Fox (Japan)" )
GAME( 1989, finehour, 0,        default,  default,  finehour, ROT0_16BIT,   "Namco", "Finest Hour (Japan)" )
GAMEX(1989, fourtrax, 0,        driving,  driving,  fourtrax, ROT0,         "Namco", "Four Trax", GAME_NOT_WORKING)
GAME( 1989, marvland, 0,        default,  default,  marvland, ROT0,         "Namco", "Marvel Land (US)" )
GAME( 1989, marvlanj, marvland, default,  default,  marvlanj, ROT0,         "Namco", "Marvel Land (Japan)" )
GAME( 1989, valkyrie, 0,        default,  default,  valkyrie, ROT90,        "Namco", "Legend of the Valkyrie (Japan)" )
GAME( 1990, kyukaidk, 0,        default,  default,  kyukaidk, ROT0_16BIT,   "Namco", "Kyuukai Douchuuki (Japan new version)" )
GAME( 1990, kyukaido, kyukaidk, default,  default,  kyukaidk, ROT0_16BIT,   "Namco", "Kyuukai Douchuuki (Japan old version)" )
GAME( 1990, dsaber,   0,        default,  default,  dsaber,   ROT90,        "Namco", "Dragon Saber" )
GAME( 1990, dsaberj,  dsaber,   default,  default,  dsaberj,  ROT90,        "Namco", "Dragon Saber (Japan)" )
GAME( 1990, rthun2,   0,        default,  default,  rthun2,   ROT0_16BIT,   "Namco", "Rolling Thunder 2" )
GAME( 1990, rthun2j,  rthun2,   default,  default,  rthun2j,  ROT0_16BIT,   "Namco", "Rolling Thunder 2 (Japan)" )
GAMEX(1990, finalap2, 0,        driving,  driving,  finalap2, ROT0,         "Namco", "Final Lap 2", GAME_NOT_WORKING )
GAMEX(1990, finalp2j, finalap2, driving,  driving,  finalp2j, ROT0,         "Namco", "Final Lap 2 (Japan)", GAME_NOT_WORKING )
GAMEX(1991, sgunner2, 0,        driving,  default,  sgunner2, ROT0,         "Namco", "Steel Gunner 2 (Japan)", GAME_NOT_WORKING )
GAME( 1991, cosmogng, 0,        default,  default,  cosmogng, ROT90,        "Namco", "Cosmo Gang the Video (US)" )
GAME( 1991, cosmognj, cosmogng, default,  default,  cosmogng, ROT90,        "Namco", "Cosmo Gang the Video (Japan)" )
GAMEX(1992, finalap3, 0,        driving,  driving,  finalap3, ROT0,         "Namco", "Final Lap 3 (Japan)", GAME_NOT_WORKING )
GAMEX(1992, suzuka8h, 0,        driving,  driving,  suzuka8h, ROT0,         "Namco", "Suzuka 8 Hours (Japan)", GAME_NOT_WORKING )
GAME( 1992, sws92,    0,        default,  default,  sws92,    ROT0_16BIT,   "Namco", "Super World Stadium '92 (Japan)" )
GAMEX(1993, suzuk8h2, 0,        driving,  driving,  suzuk8h2, ROT0,         "Namco", "Suzuka 8 Hours 2 (Japan)", GAME_NOT_WORKING )
GAME( 1993, sws93,    sws92,    default,  default,  sws93,    ROT0_16BIT,   "Namco", "Super World Stadium '93 (Japan)" )
