#ifndef DRIVER_H
#define DRIVER_H

#include "osd_cpu.h"
#include "memory.h"
#include "osdepend.h"
#include "mame.h"
#include "common.h"
#include "drawgfx.h"
#include "palette.h"
#include "cpuintrf.h"
#include "sndintrf.h"
#include "input.h"
#include "inptport.h"
#include "usrintrf.h"
#include "cheat.h"
#include "tilemap.h"
#include "sprite.h"
#include "gfxobj.h"
#include "profiler.h"

#ifdef MAME_NET
#include "network.h"
#endif /* MAME_NET */

struct MachineCPU
{
	int cpu_type;	/* see #defines below. */
	int cpu_clock;	/* in Hertz */
	const struct MemoryReadAddress *memory_read;
	const struct MemoryWriteAddress *memory_write;
	const struct IOReadPort *port_read;
	const struct IOWritePort *port_write;
	int (*vblank_interrupt)(void);
    int vblank_interrupts_per_frame;    /* usually 1 */
/* use this for interrupts which are not tied to vblank 	*/
/* usually frequency in Hz, but if you need 				*/
/* greater precision you can give the period in nanoseconds */
	int (*timed_interrupt)(void);
	int timed_interrupts_per_second;
/* pointer to a parameter to pass to the CPU cores reset function */
	void *reset_param;
};

enum
{
	CPU_DUMMY,
#if (HAS_Z80)
	CPU_Z80,
#endif
#if (HAS_DRZ80)
	CPU_DRZ80,
#endif
#if (HAS_Z80GB)
	CPU_Z80GB,
#endif
#if (HAS_8080)
	CPU_8080,
#endif
#if (HAS_8085A)
	CPU_8085A,
#endif
#if (HAS_M6502)
	CPU_M6502,
#endif
#if (HAS_M65C02)
	CPU_M65C02,
#endif
#if (HAS_M65SC02)
	CPU_M65SC02,
#endif
#if (HAS_M65CE02)
	CPU_M65CE02,
#endif
#if (HAS_M6509)
    CPU_M6509,
#endif
#if (HAS_M6510)
	CPU_M6510,
#endif
#if (HAS_M6510T)
	CPU_M6510T,
#endif
#if (HAS_M7501)
	CPU_M7501,
#endif
#if (HAS_M8502)
	CPU_M8502,
#endif
#if (HAS_N2A03)
	CPU_N2A03,
#endif
#if (HAS_M4510)
	CPU_M4510,
#endif
#if (HAS_H6280)
	CPU_H6280,
#endif
#if (HAS_I86)
	CPU_I86,
#endif
#if (HAS_I88)
	CPU_I88,
#endif
#if (HAS_I186)
	CPU_I186,
#endif
#if (HAS_I188)
	CPU_I188,
#endif
#if (HAS_I286)
	CPU_I286,
#endif
#if (HAS_V20)
	CPU_V20,
#endif
#if (HAS_V30)
	CPU_V30,
#endif
#if (HAS_V33)
	CPU_V33,
#endif
#if (HAS_ARMNEC)
	CPU_ARMV30,
	CPU_ARMV33,
#endif
#if (HAS_I8035)
	CPU_I8035,		/* same as CPU_I8039 */
#endif
#if (HAS_I8039)
	CPU_I8039,
#endif
#if (HAS_I8048)
	CPU_I8048,		/* same as CPU_I8039 */
#endif
#if (HAS_N7751)
	CPU_N7751,		/* same as CPU_I8039 */
#endif
#if (HAS_M6800)
	CPU_M6800,		/* same as CPU_M6802/CPU_M6808 */
#endif
#if (HAS_M6801)
	CPU_M6801,		/* same as CPU_M6803 */
#endif
#if (HAS_M6802)
	CPU_M6802,		/* same as CPU_M6800/CPU_M6808 */
#endif
#if (HAS_M6803)
	CPU_M6803,		/* same as CPU_M6801 */
#endif
#if (HAS_M6808)
	CPU_M6808,		/* same as CPU_M6800/CPU_M6802 */
#endif
#if (HAS_HD63701)
	CPU_HD63701,	/* 6808 with some additional opcodes */
#endif
#if (HAS_NSC8105)
	CPU_NSC8105,	/* same(?) as CPU_M6802(?) with scrambled opcodes. There is at least one new opcode. */
#endif
#if (HAS_M6805)
	CPU_M6805,
#endif
#if (HAS_M68705)
	CPU_M68705, 	/* same as CPU_M6805 */
#endif
#if (HAS_HD63705)
	CPU_HD63705,	/* M6805 family but larger address space, different stack size */
#endif
#if (HAS_HD6309)
	CPU_HD6309,		/* same as CPU_M6809 (actually it's not 100% compatible) */
#endif
#if (HAS_M6809)
	CPU_M6809,
#endif
#if (HAS_KONAMI)
	CPU_KONAMI,
#endif
#if (HAS_M68000)
	CPU_M68000,
#endif
#if (HAS_CYCLONE)
	CPU_CYCLONE,
#endif
#if (HAS_M68010)
	CPU_M68010,
#endif
#if (HAS_M68EC020)
	CPU_M68EC020,
#endif
#if (HAS_M68020)
	CPU_M68020,
#endif
#if (HAS_T11)
	CPU_T11,
#endif
#if (HAS_S2650)
	CPU_S2650,
#endif
#if (HAS_TMS34010)
	CPU_TMS34010,
#endif
#if (HAS_TMS9900)
	CPU_TMS9900,
#endif
#if (HAS_TMS9940)
	CPU_TMS9940,
#endif
#if (HAS_TMS9980)
	CPU_TMS9980,
#endif
#if (HAS_TMS9985)
	CPU_TMS9985,
#endif
#if (HAS_TMS9989)
	CPU_TMS9989,
#endif
#if (HAS_TMS9995)
	CPU_TMS9995,
#endif
#if (HAS_TMS99105A)
	CPU_TMS99105A,
#endif
#if (HAS_TMS99110A)
	CPU_TMS99110A,
#endif
#if (HAS_Z8000)
	CPU_Z8000,
#endif
#if (HAS_TMS320C10)
	CPU_TMS320C10,
#endif
#if (HAS_CCPU)
	CPU_CCPU,
#endif
#if (HAS_PDP1)
	CPU_PDP1,
#endif
#if (HAS_ADSP2100)
	CPU_ADSP2100,
#endif
#if (HAS_ADSP2105)
	CPU_ADSP2105,
#endif
#if (HAS_MIPS)
	CPU_MIPS,
#endif
#if (HAS_SC61860)
	CPU_SC61860,
#endif
#if (HAS_ARM)
	CPU_ARM,
#endif
    CPU_COUNT
};

/* set this if the CPU is used as a slave for audio. It will not be emulated if */
/* sound is disabled, therefore speeding up a lot the emulation. */
#define CPU_AUDIO_CPU 0x8000

/* the Z80 can be wired to use 16 bit addressing for I/O ports */
#define CPU_16BIT_PORT 0x4000

#define CPU_FLAGS_MASK 0xff00


#define MAX_CPU 8	/* MAX_CPU is the maximum number of CPUs which cpuintrf.c */
					/* can run at the same time. Currently, 8 is enough. */


#define MAX_SOUND 5	/* MAX_SOUND is the maximum number of sound subsystems */
					/* which can run at the same time. Currently, 5 is enough. */



struct MachineDriver
{
	/* basic machine hardware */
	struct MachineCPU cpu[MAX_CPU];
	float frames_per_second;
	int vblank_duration;	/* in microseconds - see description below */
	int cpu_slices_per_frame;	/* for multicpu games. 1 is the minimum, meaning */
								/* that each CPU runs for the whole video frame */
								/* before giving control to the others. The higher */
								/* this setting, the more closely CPUs are interleaved */
								/* and therefore the more accurate the emulation is. */
								/* However, an higher setting also means slower */
								/* performance. */
	void (*init_machine)(void);
#ifdef MESS
	void (*stop_machine)(void); /* needed for MESS */
#endif

    /* video hardware */
	int screen_width,screen_height;
	struct rectangle default_visible_area;	/* the visible area can be changed at */
									/* run time, but it should never be larger than the */
									/* one specified here, in order not to force the */
									/* OS dependant code to resize the display window. */
	struct GfxDecodeInfo *gfxdecodeinfo;
	unsigned int total_colors;	/* palette is 3*total_colors bytes long */
	unsigned int color_table_len;	/* length in shorts of the color lookup table */
	void (*vh_init_palette)(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

	int video_attributes;	/* ASG 081897 */

	void (*vh_eof_callback)(void);	/* called every frame after osd_update_video_and_audio() */
									/* This is useful when there are operations that need */
									/* to be performed every frame regardless of frameskip, */
									/* e.g. sprite buffering or collision detection. */
	int (*vh_start)(void);
	void (*vh_stop)(void);
	void (*vh_update)(struct osd_bitmap *bitmap,int full_refresh);

	/* sound hardware */
	int sound_attributes;
	int obsolete1;
	int obsolete2;
	int obsolete3;
	struct MachineSound sound[MAX_SOUND];

	/*
	   use this to manage nvram/eeprom/cmos/etc.
	   It is called before the emulation starts and after it ends. Note that it is
	   NOT called when the game is reset, since it is not needed.
	   file == 0, read_or_write == 0 -> first time the game is run, initialize nvram
	   file != 0, read_or_write == 0 -> load nvram from disk
	   file == 0, read_or_write != 0 -> not allowed
	   file != 0, read_or_write != 0 -> save nvram to disk
	 */
	void (*nvram_handler)(void *file,int read_or_write);
};



/* VBlank is the period when the video beam is outside of the visible area and */
/* returns from the bottom to the top of the screen to prepare for a new video frame. */
/* VBlank duration is an important factor in how the game renders itself. MAME */
/* generates the vblank_interrupt, lets the game run for vblank_duration microseconds, */
/* and then updates the screen. This faithfully reproduces the behaviour of the real */
/* hardware. In many cases, the game does video related operations both in its vblank */
/* interrupt, and in the normal game code; it is therefore important to set up */
/* vblank_duration accurately to have everything properly in sync. An example of this */
/* is Commando: if you set vblank_duration to 0, therefore redrawing the screen BEFORE */
/* the vblank interrupt is executed, sprites will be misaligned when the screen scrolls. */

/* Here are some predefined, TOTALLY ARBITRARY values for vblank_duration, which should */
/* be OK for most cases. I have NO IDEA how accurate they are compared to the real */
/* hardware, they could be completely wrong. */
#define DEFAULT_60HZ_VBLANK_DURATION 0
#define DEFAULT_30HZ_VBLANK_DURATION 0
/* If you use IPT_VBLANK, you need a duration different from 0. */
#define DEFAULT_REAL_60HZ_VBLANK_DURATION 2500
#define DEFAULT_REAL_30HZ_VBLANK_DURATION 2500



/* flags for video_attributes */

/* bit 0 of the video attributes indicates raster or vector video hardware */
#define	VIDEO_TYPE_RASTER			0x0000
#define	VIDEO_TYPE_VECTOR			0x0001

/* bit 1 of the video attributes indicates whether or not dirty rectangles will work */
#define	VIDEO_SUPPORTS_DIRTY		0x0002

/* bit 2 of the video attributes indicates whether or not the driver modifies the palette */
#define	VIDEO_MODIFIES_PALETTE	0x0004

/* bit 3 of the video attributes indicates that the game's palette has 6 or more bits */
/*       per gun, and would therefore require a 24-bit display. This is entirely up to */
/*       the OS dpeendant layer, the bitmap will still be 16-bit. */
#define VIDEO_NEEDS_6BITS_PER_GUN	0x0008

/* ASG 980417 - added: */
/* bit 4 of the video attributes indicates that the driver wants its refresh after */
/*       the VBLANK instead of before. */
#define	VIDEO_UPDATE_BEFORE_VBLANK	0x0000
#define	VIDEO_UPDATE_AFTER_VBLANK	0x0010

/* In most cases we assume pixels are square (1:1 aspect ratio) but some games need */
/* different proportions, e.g. 1:2 for Blasteroids */
#define VIDEO_PIXEL_ASPECT_RATIO_MASK 0x0020
#define VIDEO_PIXEL_ASPECT_RATIO_1_1 0x0000
#define VIDEO_PIXEL_ASPECT_RATIO_1_2 0x0020

#define VIDEO_DUAL_MONITOR 0x0040

/* Mish 181099:  See comments in vidhrdw/generic.c for details */
#define VIDEO_BUFFERS_SPRITERAM 0x0080

/* flags for sound_attributes */
#define	SOUND_SUPPORTS_STEREO		0x0001



struct GameDriver
{
	const char *source_file;	/* set this to __FILE__ */
	const struct GameDriver *clone_of;	/* if this is a clone, point to */
										/* the main version of the game */
	const char *name;
	const char *description;
	const char *year;
	const char *manufacturer;
	const struct MachineDriver *drv;
	const struct InputPortTiny *input_ports;
	void (*driver_init)(void);	/* optional function to be called during initialization */
								/* This is called ONCE, unlike Machine->init_machine */
								/* which is called every time the game is reset. */

	const struct RomModule *rom;
#ifdef MESS
	const struct IODevice *dev;
#endif

	UINT32 flags;	/* orientation and other flags; see defines below */
};


/* values for the flags field */

#define ORIENTATION_MASK        	0x0007
#define	ORIENTATION_FLIP_X			0x0001	/* mirror everything in the X direction */
#define	ORIENTATION_FLIP_Y			0x0002	/* mirror everything in the Y direction */
#define ORIENTATION_SWAP_XY			0x0004	/* mirror along the top-left/bottom-right diagonal */

#define GAME_NOT_WORKING			0x0008
#define GAME_WRONG_COLORS			0x0010	/* colors are totally wrong */
#define GAME_IMPERFECT_COLORS		0x0020	/* colors are not 100% accurate, but close */
#define GAME_NO_SOUND				0x0040	/* sound is missing */
#define GAME_IMPERFECT_SOUND		0x0080	/* sound is known to be wrong */
#define	GAME_REQUIRES_16BIT			0x0100	/* cannot fit in 256 colors */
#define GAME_NO_COCKTAIL			0x0200	/* screen flip support is missing */
#define GAME_UNEMULATED_PROTECTION	0x0400	/* game's protection not fully emulated */
#define NOT_A_DRIVER				0x4000	/* set by the fake "root" driver_ and by "containers" */
											/* e.g. driver_neogeo. */
#ifdef MESS
#define GAME_COMPUTER				0x8000	/* Driver is a computer (needs full keyboard) */
#define GAME_COMPUTER_MODIFIED      0x0800	/* Official? Hack */
#define GAME_ALIAS					NOT_A_DRIVER	/* Driver is only an alias for an existing model */
#endif


#define GAME(YEAR,NAME,PARENT,MACHINE,INPUT,INIT,MONITOR,COMPANY,FULLNAME)	\
extern struct GameDriver driver_##PARENT;	\
struct GameDriver driver_##NAME =			\
{											\
	__FILE__,								\
	&driver_##PARENT,						\
	#NAME,									\
	FULLNAME,								\
	#YEAR,									\
	COMPANY,								\
	&machine_driver_##MACHINE,				\
	input_ports_##INPUT,					\
	init_##INIT,							\
	rom_##NAME,								\
	MONITOR,								\
};

#define GAMEX(YEAR,NAME,PARENT,MACHINE,INPUT,INIT,MONITOR,COMPANY,FULLNAME,FLAGS)	\
extern struct GameDriver driver_##PARENT;	\
struct GameDriver driver_##NAME =			\
{											\
	__FILE__,								\
	&driver_##PARENT,						\
	#NAME,									\
	FULLNAME,								\
	#YEAR,									\
	COMPANY,								\
	&machine_driver_##MACHINE,				\
	input_ports_##INPUT,					\
	init_##INIT,							\
	rom_##NAME,								\
	(MONITOR)|(FLAGS),						\
};


/* monitor parameters to be used with the GAME() macro */
#define	ROT0	0x0000
#define	ROT90	(ORIENTATION_SWAP_XY|ORIENTATION_FLIP_X)	/* rotate clockwise 90 degrees */
#define	ROT180	(ORIENTATION_FLIP_X|ORIENTATION_FLIP_Y)		/* rotate 180 degrees */
#define	ROT270	(ORIENTATION_SWAP_XY|ORIENTATION_FLIP_Y)	/* rotate counter-clockwise 90 degrees */
#define	ROT0_16BIT		(ROT0|GAME_REQUIRES_16BIT)
#define	ROT90_16BIT		(ROT90|GAME_REQUIRES_16BIT)
#define	ROT180_16BIT	(ROT180|GAME_REQUIRES_16BIT)
#define	ROT270_16BIT	(ROT270|GAME_REQUIRES_16BIT)

/* this allows to leave the INIT field empty in the GAME() macro call */
#define init_0 0


extern const struct GameDriver *drivers[];

#endif
