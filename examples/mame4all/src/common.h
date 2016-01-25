/*********************************************************************

  common.h

  Generic functions, mostly ROM related.

*********************************************************************/

#ifndef COMMON_H
#define COMMON_H

struct RomModule
{
	const char *name;	/* name of the file to load */
	UINT32 offset;		/* offset to load it to */
	UINT32 length;		/* length of the file */
	UINT32 crc;			/* standard CRC-32 checksum */
};

/* there are some special cases for the above. name, offset and size all set to 0 */
/* mark the end of the array. If name is 0 and the others aren't, that means "continue */
/* reading the previous rom from this address". If length is 0 and offset is not 0, */
/* that marks the start of a new memory region. Confused? Well, don't worry, just use */
/* the macros below. */

#define ROMFLAG_MASK          0xfc000000           /* 6 bits worth of flags in the high nibble */

/* Masks for individual ROMs */
#define ROMFLAG_ALTERNATE     0x80000000           /* Alternate bytes, either even or odd, or nibbles, low or high */
#define ROMFLAG_WIDE          0x40000000           /* 16-bit ROM; may need byte swapping */
#define ROMFLAG_SWAP          0x20000000           /* 16-bit ROM with bytes in wrong order */
#define ROMFLAG_NIBBLE        0x10000000           /* Nibble-wide ROM image */
#define ROMFLAG_QUAD          0x08000000           /* 32-bit data arranged as 4 interleaved 8-bit roms */
#define ROMFLAG_OPTIONAL      0x04000000           /* Optional ROM, not needed for basic emulation */

/* start of table */
#define ROM_START(name) static struct RomModule rom_##name[] = {
/* start of memory region */
#define ROM_REGION(length,type) { 0, length, 0, type },

enum {
	REGION_INVALID = 0x80,
	REGION_CPU1,
	REGION_CPU2,
	REGION_CPU3,
	REGION_CPU4,
	REGION_CPU5,
	REGION_CPU6,
	REGION_CPU7,
	REGION_CPU8,
	REGION_GFX1,
	REGION_GFX2,
	REGION_GFX3,
	REGION_GFX4,
	REGION_GFX5,
	REGION_GFX6,
	REGION_GFX7,
	REGION_GFX8,
	REGION_PROMS,
	REGION_SOUND1,
	REGION_SOUND2,
	REGION_SOUND3,
	REGION_SOUND4,
	REGION_SOUND5,
	REGION_SOUND6,
	REGION_SOUND7,
	REGION_SOUND8,
	REGION_USER1,
	REGION_USER2,
	REGION_USER3,
	REGION_USER4,
	REGION_USER5,
	REGION_USER6,
	REGION_USER7,
	REGION_USER8,
	REGION_MAX
};

#define REGIONFLAG_MASK			0xf8000000
#define REGIONFLAG_DISPOSE		0x80000000           /* Dispose of this region when done */
#define REGIONFLAG_SOUNDONLY	0x40000000           /* load only if sound emulation is turned on */


#define BADCRC( crc ) (~(crc))

/* ROM to load */
#define ROM_LOAD(name,offset,length,crc) { name, offset, length, crc },

/* continue loading the previous ROM to a new address */
#define ROM_CONTINUE(offset,length) { 0, offset, length, 0 },
/* restart loading the previous ROM to a new address */
#define ROM_RELOAD(offset,length) { (char *)-1, offset, length, 0 },

/* These are for nibble-wide ROMs, can be used with code or data */
#define ROM_LOAD_NIB_LOW(name,offset,length,crc) { name, offset, (length) | ROMFLAG_NIBBLE, crc },
#define ROM_LOAD_NIB_HIGH(name,offset,length,crc) { name, offset, (length) | ROMFLAG_NIBBLE | ROMFLAG_ALTERNATE, crc },
#define ROM_RELOAD_NIB_LOW(offset,length) { (char *)-1, offset, (length) | ROMFLAG_NIBBLE, 0 },
#define ROM_RELOAD_NIB_HIGH(offset,length) { (char *)-1, offset, (length) | ROMFLAG_NIBBLE | ROMFLAG_ALTERNATE, 0 },

/* The following ones are for code ONLY - don't use for graphics data!!! */
/* load the ROM at even/odd addresses. Useful with 16 bit games */
#define ROM_LOAD_EVEN(name,offset,length,crc) { name, (offset) & ~1, (length) | ROMFLAG_ALTERNATE, crc },
#define ROM_RELOAD_EVEN(offset,length) { (char *)-1, (offset) & ~1, (length) | ROMFLAG_ALTERNATE, 0 },
#define ROM_LOAD_ODD(name,offset,length,crc)  { name, (offset) |  1, (length) | ROMFLAG_ALTERNATE, crc },
#define ROM_RELOAD_ODD(offset,length)  { (char *)-1, (offset) |  1, (length) | ROMFLAG_ALTERNATE, 0 },
/* load the ROM at even/odd addresses. Useful with 16 bit games */
#define ROM_LOAD_WIDE(name,offset,length,crc) { name, offset, (length) | ROMFLAG_WIDE, crc },
#define ROM_RELOAD_WIDE(offset,length) { (char *)-1, offset, (length) | ROMFLAG_WIDE, 0 },
#define ROM_LOAD_WIDE_SWAP(name,offset,length,crc) { name, offset, (length) | ROMFLAG_WIDE | ROMFLAG_SWAP, crc },
#define ROM_RELOAD_WIDE_SWAP(offset,length) { (char *)-1, offset, (length) | ROMFLAG_WIDE | ROMFLAG_SWAP, 0 },
/* Data is split between 4 roms, always use this in groups of 4! */
#define ROM_LOAD_QUAD(name,offset,length,crc) { name, offset, length | ROMFLAG_QUAD, crc },

#define ROM_LOAD_OPTIONAL(name,offset,length,crc) { name, offset, length | ROMFLAG_OPTIONAL, crc },

#ifdef LSB_FIRST
#define ROM_LOAD_V20_EVEN	ROM_LOAD_EVEN
#define ROM_RELOAD_V20_EVEN  ROM_RELOAD_EVEN
#define ROM_LOAD_V20_ODD	ROM_LOAD_ODD
#define ROM_RELOAD_V20_ODD   ROM_RELOAD_ODD
#define ROM_LOAD_V20_WIDE	ROM_LOAD_WIDE
#else
#define ROM_LOAD_V20_EVEN	ROM_LOAD_ODD
#define ROM_RELOAD_V20_EVEN  ROM_RELOAD_ODD
#define ROM_LOAD_V20_ODD	ROM_LOAD_EVEN
#define ROM_RELOAD_V20_ODD   ROM_RELOAD_EVEN
#define ROM_LOAD_V20_WIDE	ROM_LOAD_WIDE_SWAP
#endif

/* Use THESE ones for graphics data */
#ifdef LSB_FIRST
#define ROM_LOAD_GFX_EVEN    ROM_LOAD_ODD
#define ROM_LOAD_GFX_ODD     ROM_LOAD_EVEN
#define ROM_LOAD_GFX_SWAP    ROM_LOAD_WIDE
#else
#define ROM_LOAD_GFX_EVEN    ROM_LOAD_EVEN
#define ROM_LOAD_GFX_ODD     ROM_LOAD_ODD
#define ROM_LOAD_GFX_SWAP    ROM_LOAD_WIDE_SWAP
#endif

/* end of table */
#define ROM_END { 0, 0, 0, 0 } };



struct GameSample
{
	int length;
	int smpfreq;
	int resolution;
	signed char data[1];	/* extendable */
};

struct GameSamples
{
	int total;	/* total number of samples */
	struct GameSample *sample[1];	/* extendable */
};




void showdisclaimer(void);

/* LBO 042898 - added coin counters */
#define COIN_COUNTERS	4	/* total # of coin counters */
WRITE_HANDLER( coin_counter_w );
WRITE_HANDLER( coin_lockout_w );
WRITE_HANDLER( coin_lockout_global_w );  /* Locks out all coin inputs */


int readroms(void);
void printromlist(const struct RomModule *romp,const char *name);

/* helper function that reads samples from disk - this can be used by other */
/* drivers as well (e.g. a sound chip emulator needing drum samples) */
struct GameSamples *readsamples(const char **samplenames,const char *name);
void freesamples(struct GameSamples *samples);

/* return a pointer to the specified memory region - num can be either an absolute */
/* number, or one of the REGION_XXX identifiers defined above */
unsigned char *memory_region(int num);
int memory_region_length(int num);
/* allocate a new memory region - num can be either an absolute */
/* number, or one of the REGION_XXX identifiers defined above */
int new_memory_region(int num, int length);
void free_memory_region(int num);

extern data_t flip_screen_x, flip_screen_y;

WRITE_HANDLER( flip_screen_w );
WRITE_HANDLER( flip_screen_x_w );
WRITE_HANDLER( flip_screen_y_w );
#define flip_screen flip_screen_x

/* sets a variable and schedules a full screen refresh if it changed */
void set_vh_global_attribute( data_t *addr, data_t data );

/* next time vh_screenrefresh is called, full_refresh will be true,
   thus requesting a redraw of the entire screen */
void schedule_full_refresh(void);

void set_visible_area(int min_x,int max_x,int min_y,int max_y);

struct osd_bitmap *bitmap_alloc(int width,int height);
struct osd_bitmap *bitmap_alloc_depth(int width,int height,int depth);
void bitmap_free(struct osd_bitmap *bitmap);

void save_screen_snapshot_as(void *fp,struct osd_bitmap *bitmap);
void save_screen_snapshot(struct osd_bitmap *bitmap);

#endif
