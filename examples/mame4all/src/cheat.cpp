/*********************************************************************

  cheat.c

  This is a massively rewritten version of the cheat engine. There
  are still quite a few features that are broken or not reimplemented.

  Busted (probably permanently) is the ability to use the UI_CONFIGURE
  key to pop the menu system on and off while retaining the current
  menu selection. The sheer number of submenus involved makes this
  a difficult task to pull off here.

  TODO:

  * comment cheats
  * memory searches
  * saving of cheats to a file
  * inserting/deleting new cheats
  * cheats as watchpoints
  * key shortcuts to the search menus

(Description from Pugsy's cheat.dat)

; all fields are separated by a colon (:)
;  -Name of the game (short name) [zip file or directory]
;  -No of the CPU usually 0, only different in multiple CPU games
;  -Address in Hexadecimal (where to poke)
;  -Data to put at this address in hexadecimal (what to poke)
;  -Special (see below) usually 000
;   -000 : the byte is poked every time and the cheat remains in active list.
;   -001 : the byte is poked once and the cheat is removed from active list.
;   -002 : the byte is poked every one second and the cheat remains in active
;          list.
;   -003 : the byte is poked every two seconds and the cheat remains in active
;          list.
;   -004 : the byte is poked every five seconds and the cheat remains in active
;          list.
;   -005 : the byte is poked one second after the original value has changed
;          and the cheat remains in active list.
;   -006 : the byte is poked two seconds after the original value has changed
;          and the cheat remains in active list.
;   -007 : the byte is poked five seconds after the original value has changed
;          and the cheat remains in active list.
;   -008 : the byte is poked unless the original value in being decremented by
;          1 each frame and the cheat remains in active list.
;   -009 : the byte is poked unless the original value in being decremented by
;          2 each frame and the cheat remains in active list.
;   -010 : the byte is poked unless the original value in being decremented by
;          3 each frame and the cheat remains in active list.
;   -011 : the byte is poked unless the original value in being decremented by
;          4 each frame and the cheat remains in active list.
;   -020 : the bits are set every time and the cheat remains in active list.
;   -021 : the bits are set once and the cheat is removed from active list.
;   -040 : the bits are reset every time and the cheat remains in active list.
;   -041 : the bits are reset once and the cheat is removed from active list.
;   -060 : the user selects a decimal value from 0 to byte
;          (display : 0 to byte) - the value is poked once when it changes and
;          the cheat is removed from the active list.
;   -061 : the user selects a decimal value from 0 to byte
;          (display : 1 to byte+1) - the value is poked once when it changes
;          and the cheat is removed from the active list.
;   -062 : the user selects a decimal value from 1 to byte
;          (display : 1 to byte) - the value is poked once when it changes and
;          the cheat is removed from the active list.
;   -063 : the user selects a BCD value from 0 to byte
;          (display : 0 to byte) - the value is poked once when it changes and
;          the cheat is removed from the active list.
;   -064 : the user selects a BCD value from 0 to byte
;          (display : 1 to byte+1) - the value is poked once when it changes
;          and the cheat is removed from the active list.
;   -065 : the user selects a decimal value from 1 to byte
;          (display : 1 to byte) - the value is poked once when it changes and
;          the cheat is removed from the active list.
;   -070 : the user selects a decimal value from 0 to byte
;          (display : 0 to byte) - the value is poked once and the cheat is
;          removed from the active list.
;   -071 : the user selects a decimal value from 0 to byte
;          (display : 1 to byte+1) - the value is poked once and the cheat is
;          removed from the active list.
;   -072 : the user selects a decimal value from 1 to byte
;          (display : 1 to byte) - the value is poked once and the cheat is
;          removed from the active list.
;   -073 : the user selects a BCD value from 0 to byte
;          (display : 0 to byte) - the value is poked once and the cheat is
;          removed from the active list.
;   -074 : the user selects a BCD value from 0 to byte
;          (display : 1 to byte+1) - the value is poked once and the cheat is
;          removed from the active list.
;   -075 : the user selects a decimal value from 1 to byte
;          (display : 1 to byte) - the value is poked once and the cheat is
;          removed from the active list.
;   -500 to 575: These cheat types are identical to types 000 to 075 except
;                they are used in linked cheats (i.e. of 1/8 type). The first
;                cheat in the link list will be the normal type (eg type 000)
;                and the remaining cheats (eg 2/8...8/8) will be of this type
;                (eg type 500).
;   -998 : this is used as a watch cheat, ideal for showing answers in quiz
;          games .
;   -999 : this is used for comments only, cannot be enabled/selected by the
;          user.
;  -Name of the cheat
;  -Description for the cheat

*********************************************************************/

#include "driver.h"
#include "ui_text.h"

#ifndef MESS
#ifdef NEOMAME
extern struct GameDriver driver_neogeo;
#endif
#endif

extern unsigned char *memory_find_base (int cpu, int offset);

/******************************************
 *
 * Cheats
 *
 */

#define MAX_LOADEDCHEATS	200
#define CHEAT_FILENAME_MAXLEN	255


#define SUBCHEAT_FLAG_DONE		0x0001
#define SUBCHEAT_FLAG_TIMED		0x0002

struct subcheat_struct
{
	int cpu;
	offs_t address;
	data_t data;
#ifdef MESS
	data_t olddata;					/* old data for code patch when cheat is turned OFF */
#endif
	data_t backup;					/* The original value of the memory location, checked against the current */
	UINT32 code;
	UINT16 flags;
	data_t min;
	data_t max;
	UINT32 frames_til_trigger;			/* the number of frames until this cheat fires (does not change) */
	UINT32 frame_count;				/* decrementing frame counter to determine if cheat should fire */
};

#define CHEAT_FLAG_ACTIVE	0x01
#define CHEAT_FLAG_WATCH	0x02
#define CHEAT_FLAG_COMMENT	0x04

struct cheat_struct
{
#ifdef MESS
	unsigned int crc;					/* CRC of the game */
	char patch;						/* 'C' : code patch - 'D' : data patch */
#endif
	char *name;
	char *comment;
	UINT8 flags;					/* bit 0 = active, 1 = watchpoint, 2 = comment */
	int num_sub;					/* number of cheat cpu/address/data/code combos for this one cheat */
	struct subcheat_struct *subcheat;	/* a variable-number of subcheats are attached to each "master" cheat */
};

struct memory_struct
{
	int Enabled;
	char name[40];
	mem_write_handler handler;
};

enum
{
	kCheatSpecial_Poke = 0,
	kCheatSpecial_Poke1 = 2,
	kCheatSpecial_Poke2 = 3,
	kCheatSpecial_Poke5 = 4,
	kCheatSpecial_Delay1 = 5,
	kCheatSpecial_Delay2 = 6,
	kCheatSpecial_Delay5 = 7,
	kCheatSpecial_Backup1 = 8,
	kCheatSpecial_Backup4 = 11,
	kCheatSpecial_SetBit1 = 22,
	kCheatSpecial_SetBit2 = 23,
	kCheatSpecial_SetBit5 = 24,
	kCheatSpecial_ResetBit1 = 42,
	kCheatSpecial_ResetBit2 = 43,
	kCheatSpecial_ResetBit5 = 44,
	kCheatSpecial_UserFirst = 60,
	kCheatSpecial_m0d0c = 60,			/* minimum value 0, display range 0 to byte, poke when changed */
	kCheatSpecial_m0d1c = 61,			/* minimum value 0, display range 1 to byte+1, poke when changed */
	kCheatSpecial_m1d1c = 62,			/* minimum value 1, display range 1 to byte, poke when changed */
	kCheatSpecial_m0d0bcdc = 63,		/* BCD, minimum value 0, display range 0 to byte, poke when changed */
	kCheatSpecial_m0d1bcdc = 64,		/* BCD, minimum value 0, display range 1 to byte+1, poke when changed */
	kCheatSpecial_m1d1bcdc = 65,		/* BCD, minimum value 1, display range 1 to byte, poke when changed */
	kCheatSpecial_m0d0 = 70,			/* minimum value 0, display range 0 to byte */
	kCheatSpecial_m0d1 = 71,			/* minimum value 0, display range 1 to byte+1 */
	kCheatSpecial_m1d1 = 72,			/* minimum value 1, display range 1 to byte */
	kCheatSpecial_m0d0bcd = 73,			/* BCD, minimum value 0, display range 0 to byte */
	kCheatSpecial_m0d1bcd = 74,			/* BCD, minimum value 0, display range 1 to byte+1 */
	kCheatSpecial_m1d1bcd = 75,			/* BCD, minimum value 1, display range 1 to byte */
	kCheatSpecial_UserLast = 75,
	kCheatSpecial_Last = 99,
	kCheatSpecial_LinkStart = 500,		/* only used when loading the database */
	kCheatSpecial_LinkEnd = 599,		/* only used when loading the database */
	kCheatSpecial_Watch = 998,
	kCheatSpecial_Comment = 999,
	kCheatSpecial_Timed = 1000
};

char *cheatfile = "cheat.dat";

char database[CHEAT_FILENAME_MAXLEN+1];

int he_did_cheat;


/******************************************
 *
 * Searches
 *
 */

/* Defines */
#define MAX_SEARCHES 500

enum {
	kSearch_None = 0,
	kSearch_Value =	1,
	kSearch_Time,
	kSearch_Energy,
	kSearch_Bit,
	kSearch_Byte
};

enum {
	kRestore_NoInit = 1,
	kRestore_NoSave,
	kRestore_Done,
	kRestore_OK
};

/* Local variables */
static int searchType;
static int searchCPU;
//static int priorSearchCPU;	/* Steph */
static int searchValue;
static int restoreStatus;

static int fastsearch = 2; /* ?? */

static struct ExtMemory StartRam[MAX_EXT_MEMORY];
static struct ExtMemory BackupRam[MAX_EXT_MEMORY];
static struct ExtMemory FlagTable[MAX_EXT_MEMORY];

static struct ExtMemory OldBackupRam[MAX_EXT_MEMORY];
static struct ExtMemory OldFlagTable[MAX_EXT_MEMORY];

/* Local prototypes */
static void reset_table (struct ExtMemory *table);

/******************************************
 *
 * Watchpoints
 *
 */

#define MAX_WATCHES 	20

struct watch_struct
{
	int cheat_num;		/* if this watchpoint is tied to a cheat, this is the index into the cheat array. -1 if none */
	UINT32 address;
	INT16 cpu;
	UINT8 num_bytes;	/* number of consecutive bytes to display */
	UINT8 label_type;	/* none, address, text */
	char label[255];	/* optional text label */
	UINT16 x, y;		/* position of watchpoint on screen */
};

static struct watch_struct watches[MAX_WATCHES];
static int is_watch_active; /* true if at least one watchpoint is active */
static int is_watch_visible; /* we can toggle the visibility for all on or off */



/* in hiscore.c */
int computer_readmem_byte(int cpu, int addr);
void computer_writemem_byte(int cpu, int addr, int value);

/* Some macros to simplify the code */
#define READ_CHEAT		computer_readmem_byte (subcheat->cpu, subcheat->address)
#define WRITE_CHEAT		computer_writemem_byte (subcheat->cpu, subcheat->address, subcheat->data)
#define COMPARE_CHEAT		(computer_readmem_byte (subcheat->cpu, subcheat->address) != subcheat->data)
#define CPU_AUDIO_OFF(index)	((Machine->drv->cpu[index].cpu_type & CPU_AUDIO_CPU) && (Machine->sample_rate == 0))

/* Steph */
#ifdef MESS
#define WRITE_OLD_CHEAT		computer_writemem_byte (subcheat->cpu, subcheat->address, subcheat->olddata)
#endif

/* Local prototypes */
static INT32 DisplayHelpFile (INT32 selected);
static INT32 EditCheatMenu (struct osd_bitmap *bitmap, INT32 selected, UINT8 cheatnum);
static INT32 CommentMenu (struct osd_bitmap *bitmap, INT32 selected, int cheat_index);
static int SkipBank(int CpuToScan, int *BankToScanTable, mem_write_handler handler);	/* Steph */

/* Local variables */
/* static int	search_started = 0; */

static int ActiveCheatTotal;										/* number of cheats currently active */
static int LoadedCheatTotal;										/* total number of cheats */
static struct cheat_struct CheatTable[MAX_LOADEDCHEATS+1];

static int CheatEnabled;

#ifdef MESS
/* Function who tries to find a valid game with a CRC */
int MatchCRC(unsigned int crc)
{
	int type, id;

	if (!crc)
		return 1;

	for (type = 0; type < IO_COUNT; type++)
	{
		for( id = 0; id < device_count(type); id++ )
		{
			if (crc == device_crc(type,id))
				return 1;
		}
	}

	return 0;
}
#endif



/* Function to test if a value is BCD (returns 1) or not (returns 0) */
int IsBCD(int ParamValue)
{
	return(((ParamValue % 0x10 <= 9) & (ParamValue <= 0x99)) ? 1 : 0);
}

/* return a format specifier for printf based on cpu address range */
static char *FormatAddr(int cpu, int addtext)
{
	static char bufadr[10];
	static char buffer[18];
//	int i;

	memset (buffer, '\0', strlen(buffer));
	switch (cpunum_address_bits(cpu) >> 2)
	{
		case 4:
			strcpy (bufadr, "%04X");
			break;
		case 5:
			strcpy (bufadr, "%05X");
			break;
		case 6:
			strcpy (bufadr, "%06X");
			break;
		case 7:
			strcpy (bufadr, "%07X");
			break;
		case 8:
			strcpy (bufadr, "%08X");
			break;
		default:
			strcpy (bufadr, "%X");
			break;
	}
#if 0
	if (addtext)
	{
		strcpy (buffer, "Addr:  ");
		for (i = strlen(bufadr) + 1; i < 8; i ++)
			strcat (buffer, " ");
	}
#endif
	strcat (buffer,bufadr);
	return buffer;
}

/* Function to rename the cheatfile (returns 1 if the file has been renamed else 0)*/
int RenameCheatFile(int merge, int DisplayFileName, char *filename)
{
	return 0;
}

/* Function who loads the cheats for a game */
int SaveCheat(int NoCheat)
{
	return 0;
}

/***************************************************************************

  cheat_set_code

  Given a cheat code, sets the various attribues of the cheat structure.
  This is to aid in making the cheat engine more flexible in the event that
  someday the codes are restructured or the case statement in DoCheat is
  simplified from its current form.

***************************************************************************/
void cheat_set_code (struct subcheat_struct *subcheat, int code, int cheat_num)
{
	switch (code)
	{
		case kCheatSpecial_Poke1:
		case kCheatSpecial_Delay1:
		case kCheatSpecial_SetBit1:
		case kCheatSpecial_ResetBit1:
			subcheat->frames_til_trigger = 1 * Machine->drv->frames_per_second; /* was 60 */
			break;
		case kCheatSpecial_Poke2:
		case kCheatSpecial_Delay2:
		case kCheatSpecial_SetBit2:
		case kCheatSpecial_ResetBit2:
			subcheat->frames_til_trigger = 2 * Machine->drv->frames_per_second; /* was 60 */
			break;
		case kCheatSpecial_Poke5:
		case kCheatSpecial_Delay5:
		case kCheatSpecial_SetBit5:
		case kCheatSpecial_ResetBit5:
			subcheat->frames_til_trigger = 5 * Machine->drv->frames_per_second; /* was 60 */
			break;
		case kCheatSpecial_Comment:
			subcheat->frames_til_trigger = 0;
			subcheat->address = 0;
			subcheat->data = 0;
			CheatTable[cheat_num].flags |= CHEAT_FLAG_COMMENT;
			break;
		case kCheatSpecial_Watch:
			subcheat->frames_til_trigger = 0;
			subcheat->data = 0;
			CheatTable[cheat_num].flags |= CHEAT_FLAG_WATCH;
			break;
		default:
			subcheat->frames_til_trigger = 0;
			break;
	}

	/* Set the minimum value */
	if ((code == kCheatSpecial_m1d1c) ||
		(code == kCheatSpecial_m1d1bcdc) ||
		(code == kCheatSpecial_m1d1) ||
		(code == kCheatSpecial_m1d1bcd))
		subcheat->min = 1;
	else
		subcheat->min = 0;

	/* Set the maximum value */
	if ((code >= kCheatSpecial_UserFirst) &&
		(code <= kCheatSpecial_UserLast))
	{
		subcheat->max = subcheat->data;
		subcheat->data = 0;
	}
	else
		subcheat->max = 0xff;

	subcheat->code = code;
}

/***************************************************************************

  cheat_set_status

  Given an index into the cheat table array, make the selected cheat
  either active or inactive.

  TODO: possibly support converting to a watchpoint in here.

***************************************************************************/
void cheat_set_status (int cheat_num, int active)
{
	int i;

	if (active) /* enable the cheat */
	{
		for (i = 0; i <= CheatTable[cheat_num].num_sub; i ++)
		{
			/* Reset the active variables */
			CheatTable[cheat_num].subcheat[i].frame_count = 0;
			CheatTable[cheat_num].subcheat[i].backup = 0;
		}

		/* only add if there's a cheat active already */
		if ((CheatTable[cheat_num].flags & CHEAT_FLAG_ACTIVE) == 0)
		{
			CheatTable[cheat_num].flags |= CHEAT_FLAG_ACTIVE;
			ActiveCheatTotal++;
		}

		/* tell the MAME core that we're cheaters! */
		he_did_cheat = 1;
	}
	else /* disable the cheat (case 0, 2) */
	{
		for (i = 0; i <= CheatTable[cheat_num].num_sub; i ++)
		{
//			struct subcheat_struct *subcheat = &CheatTable[cheat_num].subcheat[i];	/* Steph */

			/* Reset the active variables */
			CheatTable[cheat_num].subcheat[i].frame_count = 0;
			CheatTable[cheat_num].subcheat[i].backup = 0;

#ifdef MESS
			/* Put the original code if it is a code patch */
			if (CheatTable[cheat_num].patch == 'C')
				WRITE_OLD_CHEAT;
#endif

		}

		/* only add if there's a cheat active already */
		if (CheatTable[cheat_num].flags & CHEAT_FLAG_ACTIVE)
		{
			CheatTable[cheat_num].flags &= ~CHEAT_FLAG_ACTIVE;
			ActiveCheatTotal--;
		}
	}
}

void cheat_insert_new (int cheat_num)
{
	/* if list is full, bail */
	if (LoadedCheatTotal == MAX_LOADEDCHEATS) return;

	/* if the index is off the end of the list, fix it */
	if (cheat_num > LoadedCheatTotal) cheat_num = LoadedCheatTotal;

	/* clear space in the middle of the table if needed */
	if (cheat_num < LoadedCheatTotal)
		memmove (&CheatTable[cheat_num+1], &CheatTable[cheat_num], sizeof (struct cheat_struct) * (LoadedCheatTotal - cheat_num));

	/* clear the new entry */
	memset (&CheatTable[cheat_num], 0, sizeof (struct cheat_struct));

	CheatTable[cheat_num].name = (char*)malloc(strlen (ui_getstring(UI_none)) + 1);
	strcpy (CheatTable[cheat_num].name, ui_getstring(UI_none));

	CheatTable[cheat_num].subcheat = (subcheat_struct*)calloc(1, sizeof (struct subcheat_struct));

	/*add one to the total */
	LoadedCheatTotal ++;
}

void cheat_delete (int cheat_num)
{
	/* if the index is off the end, make it the last one */
	if (cheat_num >= LoadedCheatTotal) cheat_num = LoadedCheatTotal - 1;

	/* deallocate storage for the cheat */
	free(CheatTable[cheat_num].name);
	free(CheatTable[cheat_num].comment);
	free(CheatTable[cheat_num].subcheat);

	/* If it's active, decrease the count */
	if (CheatTable[cheat_num].flags & CHEAT_FLAG_ACTIVE)
		ActiveCheatTotal --;

	/* move all the elements after this one up one slot if there are more than 1 and it's not the last */
	if ((LoadedCheatTotal > 1) && (cheat_num < LoadedCheatTotal - 1))
		memmove (&CheatTable[cheat_num], &CheatTable[cheat_num+1], sizeof (struct cheat_struct) * (LoadedCheatTotal - (cheat_num + 1)));

	/* knock one off the total */
	LoadedCheatTotal --;
}

void subcheat_insert_new (int cheat_num, int subcheat_num)
{
	/* if the index is off the end of the list, fix it */
	if (subcheat_num > CheatTable[cheat_num].num_sub) subcheat_num = CheatTable[cheat_num].num_sub + 1;

	/* grow the subcheat table allocation */
	CheatTable[cheat_num].subcheat = (subcheat_struct*)realloc(CheatTable[cheat_num].subcheat, sizeof (struct subcheat_struct) * (CheatTable[cheat_num].num_sub + 2));
	if (CheatTable[cheat_num].subcheat == NULL) return;

	/* insert space in the middle of the table if needed */
	if ((subcheat_num < CheatTable[cheat_num].num_sub) || (subcheat_num == 0))
		memmove (&CheatTable[cheat_num].subcheat[subcheat_num+1], &CheatTable[cheat_num].subcheat[subcheat_num],
			sizeof (struct subcheat_struct) * (CheatTable[cheat_num].num_sub + 1 - subcheat_num));

	/* clear the new entry */
	memset (&CheatTable[cheat_num].subcheat[subcheat_num], 0, sizeof (struct subcheat_struct));

	/*add one to the total */
	CheatTable[cheat_num].num_sub ++;
}

void subcheat_delete (int cheat_num, int subcheat_num)
{
	if (CheatTable[cheat_num].num_sub < 1) return;
	/* if the index is off the end, make it the last one */
	if (subcheat_num > CheatTable[cheat_num].num_sub) subcheat_num = CheatTable[cheat_num].num_sub;

	/* remove the element in the middle if it's not the last */
	if (subcheat_num < CheatTable[cheat_num].num_sub)
		memmove (&CheatTable[cheat_num].subcheat[subcheat_num], &CheatTable[cheat_num].subcheat[subcheat_num+1],
			sizeof (struct subcheat_struct) * (CheatTable[cheat_num].num_sub - subcheat_num));

	/* shrink the subcheat table allocation */
	CheatTable[cheat_num].subcheat = (subcheat_struct*)realloc(CheatTable[cheat_num].subcheat, sizeof (struct subcheat_struct) * (CheatTable[cheat_num].num_sub));
	if (CheatTable[cheat_num].subcheat == NULL) return;

	/* knock one off the total */
	CheatTable[cheat_num].num_sub --;
}

/* Function to load the cheats for a game from a single database */
void LoadCheatFile (int merge, char *filename)
{
	void *f = osd_fopen (NULL, filename, OSD_FILETYPE_CHEAT, 0);
	char curline[2048];
	int name_length;
	struct subcheat_struct *subcheat;
	int sub = 0;

	if (!merge)
	{
		ActiveCheatTotal = 0;
		LoadedCheatTotal = 0;
	}

	if (!f) return;

	name_length = strlen (Machine->gamedrv->name);

	/* Load the cheats for that game */
	/* Ex.: pacman:0:4E14:06:000:1UP Unlimited lives:Coded on 1 byte */
	while ((osd_fgets (curline, 2048, f) != NULL) && (LoadedCheatTotal < MAX_LOADEDCHEATS))
	{
		char *ptr;
		int temp_cpu;
#ifdef MESS
		unsigned int temp_crc;
		char temp_patch;
#endif
		offs_t temp_address;
		data_t temp_data;
#ifdef MESS
		data_t temp_olddata;
#endif
		INT32 temp_code;


		/* Take a few easy-out cases to speed things up */
		if (curline[name_length] != ':') continue;
		if (strncmp (curline, Machine->gamedrv->name, name_length) != 0) continue;
		if (curline[0] == ';') continue;

#if 0
		if (sologame)
			if ((strstr(str, "2UP") != NULL) || (strstr(str, "PL2") != NULL) ||
				(strstr(str, "3UP") != NULL) || (strstr(str, "PL3") != NULL) ||
				(strstr(str, "4UP") != NULL) || (strstr(str, "PL4") != NULL) ||
				(strstr(str, "2up") != NULL) || (strstr(str, "pl2") != NULL) ||
				(strstr(str, "3up") != NULL) || (strstr(str, "pl3") != NULL) ||
				(strstr(str, "4up") != NULL) || (strstr(str, "pl4") != NULL))
		  continue;
#endif

		/* Extract the fields from the line */
		/* Skip the driver name */
		ptr = strtok(curline, ":");
		if (!ptr) continue;

#ifdef MESS
		/* CRC */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%x", &temp_crc);
		if (! MatchCRC(temp_crc))
			continue;

		/* Patch */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%c", &temp_patch);
		if (temp_patch != 'C')
			temp_patch = 'D';

#endif

		/* CPU number */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%d", &temp_cpu);
//		/* skip if it's a sound cpu and the audio is off */
//		if (CPU_AUDIO_OFF(temp_cpu)) continue;
		/* skip if this is a bogus CPU */
		if (temp_cpu >= cpu_gettotalcpu()) continue;

		/* Address */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%X", &temp_address);
		temp_address &= cpunum_address_mask(temp_cpu);

		/* data byte */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%x", &temp_data);
		temp_data &= 0xff;

#ifdef MESS
		/* old data byte */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%x", &temp_olddata);
		temp_olddata &= 0xff;
#endif

		/* special code */
		ptr = strtok(NULL, ":");
		if (!ptr) continue;
		sscanf(ptr,"%d", &temp_code);

		/* Is this a subcheat? */
		if ((temp_code >= kCheatSpecial_LinkStart) &&
			(temp_code <= kCheatSpecial_LinkEnd))
		{
			sub ++;

			/* Adjust the special flag */
			temp_code -= kCheatSpecial_LinkStart;

			/* point to the last valid main cheat entry */
			LoadedCheatTotal --;
		}
		else
		{
			/* no, make this the first cheat in the series */
			sub = 0;
		}

		/* Allocate (or resize) storage for the subcheat array */
		CheatTable[LoadedCheatTotal].subcheat = (subcheat_struct*)realloc(CheatTable[LoadedCheatTotal].subcheat, sizeof (struct subcheat_struct) * (sub + 1));
		if (CheatTable[LoadedCheatTotal].subcheat == NULL) continue;

		/* Store the current number of subcheats embodied by this code */
		CheatTable[LoadedCheatTotal].num_sub = sub;

		subcheat = &CheatTable[LoadedCheatTotal].subcheat[sub];

		/* Reset the cheat */
		subcheat->frames_til_trigger = 0;
		subcheat->frame_count = 0;
		subcheat->backup = 0;
		subcheat->flags = 0;

		/* Copy the cheat data */
		subcheat->cpu = temp_cpu;

		subcheat->address = temp_address;
		subcheat->data = temp_data;
#ifdef MESS
		subcheat->olddata = temp_olddata;
#endif
		subcheat->code = temp_code;

		cheat_set_code (subcheat, temp_code, LoadedCheatTotal);

		/* don't bother with the names & comments for subcheats */
		if (sub) goto next;

#ifdef MESS
		/* CRC */
		CheatTable[LoadedCheatTotal].crc = temp_crc;
		/* Patch */
		CheatTable[LoadedCheatTotal].patch = temp_patch;
#endif
		/* Disable the cheat */
		CheatTable[LoadedCheatTotal].flags &= ~CHEAT_FLAG_ACTIVE;

		/* cheat name */
		CheatTable[LoadedCheatTotal].name = NULL;
		ptr = strtok(NULL, ":");
		if (!ptr) continue;

		/* Allocate storage and copy the name */
		CheatTable[LoadedCheatTotal].name = (char*)malloc(strlen (ptr) + 1);
		strcpy (CheatTable[LoadedCheatTotal].name,ptr);

		/* Strip line-ending if needed */
		if (strstr(CheatTable[LoadedCheatTotal].name,"\n") != NULL)
			CheatTable[LoadedCheatTotal].name[strlen(CheatTable[LoadedCheatTotal].name)-1] = 0;

		/* read the "comment" field if there */
		ptr = strtok(NULL, ":");
		if (ptr)
		{
			/* Allocate storage and copy the comment */
			CheatTable[LoadedCheatTotal].comment = (char*)malloc(strlen (ptr) + 1);
			strcpy(CheatTable[LoadedCheatTotal].comment,ptr);

			/* Strip line-ending if needed */
			if (strstr(CheatTable[LoadedCheatTotal].comment,"\n") != NULL)
				CheatTable[LoadedCheatTotal].comment[strlen(CheatTable[LoadedCheatTotal].comment)-1] = 0;
		}
		else
			CheatTable[LoadedCheatTotal].comment = NULL;

next:
		LoadedCheatTotal ++;
	}

	osd_fclose (f);
}

/* Function who loads the cheats for a game from many databases */
void LoadCheatFiles (void)
{
	char *ptr;
	char str[CHEAT_FILENAME_MAXLEN+1];
	char filename[CHEAT_FILENAME_MAXLEN+1];

	int pos1, pos2;

	ActiveCheatTotal = 0;
	LoadedCheatTotal = 0;

	/* start off with the default cheat file, cheat.dat */
	strcpy (str, cheatfile);
	ptr = strtok (str, ";");

	/* append any additional cheat files */
	strcpy (database, ptr);
	strcpy (str, cheatfile);
	str[strlen (str) + 1] = 0;
	pos1 = 0;
	while (str[pos1])
	{
		pos2 = pos1;
		while ((str[pos2]) && (str[pos2] != ';'))
			pos2++;
		if (pos1 != pos2)
		{
			memset (filename, '\0', sizeof(filename));
			strncpy (filename, &str[pos1], (pos2 - pos1));
			LoadCheatFile (1, filename);
			pos1 = pos2 + 1;
		}
	}
}

#if 0
void InitMemoryAreas(void)
{
	const struct MemoryWriteAddress *mwa = Machine->drv->cpu[Searchcpu].memory_write;
	char buffer[40];

	MemoryAreasSelected = 0;
	MemoryAreasTotal = 0;

	while (mwa->start != -1)
	{
		sprintf (buffer, FormatAddr(Searchcpu,0), mwa->start);
		strcpy (MemToScanTable[MemoryAreasTotal].Name, buffer);
		strcat (MemToScanTable[MemoryAreasTotal].Name," -> ");
		sprintf (buffer, FormatAddr(Searchcpu,0), mwa->end);
		strcat (MemToScanTable[MemoryAreasTotal].Name, buffer);
		MemToScanTable[MemoryAreasTotal].handler = mwa->handler;
		MemToScanTable[MemoryAreasTotal].Enabled = 0;
		MemoryAreasTotal++;
		mwa++;
	}
}
#endif

/* Init some variables */
void InitCheat(void)
{
	int i;

	he_did_cheat = 0;
	CheatEnabled = 1;

	/* set up the search tables */
	reset_table (StartRam);
	reset_table (BackupRam);
	reset_table (FlagTable);
	reset_table (OldBackupRam);
	reset_table (OldFlagTable);

	restoreStatus = kRestore_NoInit;

	/* Reset the watchpoints to their defaults */
	is_watch_active = 0;
	is_watch_visible = 1;

	for (i = 0;i < MAX_WATCHES;i ++)
	{
		/* disable this watchpoint */
		watches[i].num_bytes = 0;

		watches[i].cpu = 0;
		watches[i].label[0] = 0x00;
		watches[i].label_type = 0;
		watches[i].address = 0;

		/* set the screen position */
		watches[i].x = 0;
		watches[i].y = i * Machine->uifontheight;
	}

	LoadCheatFiles ();
/*	InitMemoryAreas(); */
}

#ifdef macintosh
#pragma mark -
#endif

INT32 EnableDisableCheatMenu (struct osd_bitmap *bitmap, INT32 selected)
{
	int sel;
	static INT8 submenu_choice;
	const char *menu_item[MAX_LOADEDCHEATS + 2];
	const char *menu_subitem[MAX_LOADEDCHEATS];
	char buf[MAX_LOADEDCHEATS][80];
	char buf2[MAX_LOADEDCHEATS][10];
	static int tag[MAX_LOADEDCHEATS];
	int i, total = 0;


	sel = selected - 1;

	/* If a submenu has been selected, go there */
	if (submenu_choice)
	{
		submenu_choice = CommentMenu (bitmap, submenu_choice, tag[sel]);
		if (submenu_choice == -1)
		{
			submenu_choice = 0;
			sel = -2;
		}

		return sel + 1;
	}

	/* No submenu active, do the watchpoint menu */
	for (i = 0; i < LoadedCheatTotal; i ++)
	{
		int string_num;

		if (CheatTable[i].comment && (CheatTable[i].comment[0] != 0x00))
		{
			sprintf (buf[total], "%s (%s...)", CheatTable[i].name, ui_getstring (UI_moreinfo));
		}
		else
			sprintf (buf[total], "%s", CheatTable[i].name);

		tag[total] = i;
		menu_item[total] = buf[total];

		/* add submenu options for all cheats that are not comments */
		if ((CheatTable[i].flags & CHEAT_FLAG_COMMENT) == 0)
		{
			if (CheatTable[i].flags & CHEAT_FLAG_ACTIVE)
				string_num = UI_on;
			else
				string_num = UI_off;
			sprintf (buf2[total], "%s", ui_getstring (string_num));
			menu_subitem[total] = buf2[total];
		}
		else
			menu_subitem[total] = NULL;
		total ++;
	}

	menu_item[total] = ui_getstring (UI_returntoprior);
	menu_subitem[total++] = NULL;
	menu_item[total] = 0;	/* terminate array */

	ui_displaymenu(bitmap,menu_item,menu_subitem,0,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if ((input_ui_pressed_repeat(IPT_UI_LEFT,8)) || (input_ui_pressed_repeat(IPT_UI_RIGHT,8)))
	{
		if ((CheatTable[tag[sel]].flags & CHEAT_FLAG_COMMENT) == 0)
		{
			int active = CheatTable[tag[sel]].flags & CHEAT_FLAG_ACTIVE;

			active ^= 0x01;

			cheat_set_status (tag[sel], active);
			CheatEnabled = 1;
		}
	}


	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == (total - 1))
		{
			/* return to prior menu */
			submenu_choice = 0;
			sel = -1;
		}
		else
		{
			if (CheatTable[tag[sel]].comment && (CheatTable[tag[sel]].comment[0] != 0x00))
			{
				submenu_choice = 1;
				/* tell updatescreen() to clean after us */
				need_to_clear_bitmap = 1;
			}
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

static INT32 CommentMenu (struct osd_bitmap *bitmap, INT32 selected, int cheat_index)
{
	char buf[2048];
	char buf2[256];
	int sel;


	sel = selected - 1;

	buf[0] = 0;

	if (CheatTable[cheat_index].comment[0] == 0x00)
	{
		sel = -1;
		buf[0] = 0;
	}
	else
	{
		sprintf (buf2, "\t%s\n\t%s\n\n", ui_getstring (UI_moreinfoheader), CheatTable[cheat_index].name);
		strcpy (buf, buf2);
		strcat (buf, CheatTable[cheat_index].comment);
	}

	/* menu system, use the normal menu keys */
	strcat(buf,"\n\n\t");
	strcat(buf,ui_getstring (UI_lefthilight));
	strcat(buf," ");
	strcat(buf,ui_getstring (UI_returntoprior));
	strcat(buf," ");
	strcat(buf,ui_getstring (UI_righthilight));

	ui_displaymessagewindow(bitmap, buf);

	if (input_ui_pressed(IPT_UI_SELECT))
		sel = -1;

	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

INT32 AddEditCheatMenu (struct osd_bitmap *bitmap, INT32 selected)
{
	int sel;
	static INT8 submenu_choice;
	const char *menu_item[MAX_LOADEDCHEATS + 4];
//	char buf[MAX_LOADEDCHEATS][80];
//	char buf2[MAX_LOADEDCHEATS][10];
	int tag[MAX_LOADEDCHEATS];
	int i, total = 0;


	sel = selected - 1;

	/* Set up the "tag" table so we know which cheats belong in the menu */
	for (i = 0; i < LoadedCheatTotal; i ++)
	{
		/* add menu listings for all cheats that are not comments */
		if (((CheatTable[i].flags & CHEAT_FLAG_COMMENT) == 0)
#ifdef MESS
		/* only data patches can be edited within the cheat engine */
		&& (CheatTable[i].patch == 'D')
#endif
		)
		{
			tag[total] = i;
			menu_item[total++] = CheatTable[i].name;
		}
	}

	/* If a submenu has been selected, go there */
	if (submenu_choice)
	{
		submenu_choice = EditCheatMenu (bitmap, submenu_choice, tag[sel]);
		if (submenu_choice == -1)
		{
			submenu_choice = 0;
			sel = -2;
		}

		return sel + 1;
	}

	/* No submenu active, do the watchpoint menu */
	menu_item[total++] = ui_getstring (UI_returntoprior);
	menu_item[total] = NULL; /* TODO: add help string */
	menu_item[total+1] = 0;	/* terminate array */

	ui_displaymenu(bitmap,menu_item,0,0,sel,0);

	if (code_pressed_memory_repeat (KEYCODE_INSERT, 8))
	{
		/* add a new cheat at the current position (or the end) */
		if (sel < total - 1)
			cheat_insert_new (tag[sel]);
		else
			cheat_insert_new (LoadedCheatTotal);
	}

	if (code_pressed_memory_repeat (KEYCODE_DEL, 8))
	{
		if (LoadedCheatTotal)
		{
			/* delete the selected cheat (or the last one) */
			if (sel < total - 1)
				cheat_delete (tag[sel]);
			else
			{
				cheat_delete (LoadedCheatTotal - 1);
				sel = total - 2;
			}
		}
	}

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == (total - 1))
		{
			/* return to prior menu */
			submenu_choice = 0;
			sel = -1;
		}
		else
		{
			submenu_choice = 1;
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

static INT32 EditCheatMenu (struct osd_bitmap *bitmap, INT32 selected, UINT8 cheat_num)
{
	int sel;
	int total, total2;
	struct subcheat_struct *subcheat;
	static INT8 submenu_choice;
	static UINT8 textedit_active;
	const char *menu_item[40];
	const char *menu_subitem[40];
	char setting[40][30];
	char flag[40];
	int arrowize;
	int subcheat_num;
	int i;


	sel = selected - 1;

	total = 0;
	/* No submenu active, display the main cheat menu */
	menu_item[total++] = ui_getstring (UI_cheatname);
	menu_item[total++] = ui_getstring (UI_cheatdescription);
	for (i = 0; i <= CheatTable[cheat_num].num_sub; i ++)
	{
		menu_item[total++] = ui_getstring (UI_cpu);
		menu_item[total++] = ui_getstring (UI_address);
		menu_item[total++] = ui_getstring (UI_value);
		menu_item[total++] = ui_getstring (UI_code);
	}
	menu_item[total++] = ui_getstring (UI_returntoprior);
	menu_item[total] = 0;

	arrowize = 0;

	/* set up the submenu selections */
	total2 = 0;
	for (i = 0; i < 40; i ++)
		flag[i] = 0;

	/* if we're editing the label, make it inverse */
	if (textedit_active)
		flag[sel] = 1;

	/* name */
	if (CheatTable[cheat_num].name != 0x00)
		sprintf (setting[total2], "%s", CheatTable[cheat_num].name);
	else
		strcpy (setting[total2], ui_getstring (UI_none));
	menu_subitem[total2] = setting[total2]; total2++;

	/* comment */
	if (CheatTable[cheat_num].comment && CheatTable[cheat_num].comment != 0x00)
		sprintf (setting[total2], "%s...", ui_getstring (UI_moreinfo));
	else
		strcpy (setting[total2], ui_getstring (UI_none));
	menu_subitem[total2] = setting[total2]; total2++;

	/* Subcheats */
	for (i = 0; i <= CheatTable[cheat_num].num_sub; i ++)
	{
		subcheat = &CheatTable[cheat_num].subcheat[i];

		/* cpu number */
		sprintf (setting[total2], "%d", subcheat->cpu);
		menu_subitem[total2] = setting[total2]; total2++;

		/* address */
		if (cpunum_address_bits(subcheat->cpu) <= 16)
		{
			sprintf (setting[total2], "%04X", subcheat->address);
		}
		else
		{
			sprintf (setting[total2], "%08X", subcheat->address);
		}
		menu_subitem[total2] = setting[total2]; total2++;

		/* value */
		sprintf (setting[total2], "%d", subcheat->data);
		menu_subitem[total2] = setting[total2]; total2++;

		/* code */
		sprintf (setting[total2], "%d", subcheat->code);
		menu_subitem[total2] = setting[total2]; total2++;

		menu_subitem[total2] = NULL;
	}

	ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel,arrowize);

	if (code_pressed_memory_repeat (KEYCODE_INSERT, 8))
	{
		if ((sel >= 2) && (sel <= ((CheatTable[cheat_num].num_sub + 1) * 4) + 1))
		{
			subcheat_num = (sel - 2) % 4;
		}
		else
		{
			subcheat_num = CheatTable[cheat_num].num_sub + 1;
		}

		/* add a new subcheat at the current position (or the end) */
		subcheat_insert_new (cheat_num, subcheat_num);
	}

	if (code_pressed_memory_repeat (KEYCODE_DEL, 8))
	{
		if ((sel >= 2) && (sel <= ((CheatTable[cheat_num].num_sub + 1) * 4) + 1))
		{
			subcheat_num = (sel - 2) % 4;
		}
		else
		{
			subcheat_num = CheatTable[cheat_num].num_sub;
		}

		if (CheatTable[cheat_num].num_sub != 0)
			subcheat_delete (cheat_num, subcheat_num);
	}

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
	{
		textedit_active = 0;
		sel = (sel + 1) % total;
	}

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
	{
		textedit_active = 0;
		sel = (sel + total - 1) % total;
	}

	if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
	{
		if ((sel >= 2) && (sel <= ((CheatTable[cheat_num].num_sub + 1) * 4) + 1))
		{
			int newsel;

			subcheat = &CheatTable[cheat_num].subcheat[(sel - 2) / 4];
			newsel = (sel - 2) % 4;

			switch (newsel)
			{
				case 0: /* CPU */
					subcheat->cpu --;
					/* skip audio CPUs when the sound is off */
					if (CPU_AUDIO_OFF(subcheat->cpu))
						subcheat->cpu --;
					if (subcheat->cpu < 0)
						subcheat->cpu = cpu_gettotalcpu() - 1;
					subcheat->address &= cpunum_address_mask(subcheat->cpu);
					break;
				case 1: /* address */
					textedit_active = 0;
					subcheat->address --;
					subcheat->address &= cpunum_address_mask(subcheat->cpu);
					break;
				case 2: /* value */
					textedit_active = 0;
					subcheat->data --;
					subcheat->data &= 0xff;
					break;
				case 3: /* code */
					textedit_active = 0;
					subcheat->code --;
					break;
			}
		}
	}

	if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
	{
		if ((sel >= 2) && (sel <= ((CheatTable[cheat_num].num_sub+1) * 4) + 1))
		{
			int newsel;

			subcheat = &CheatTable[cheat_num].subcheat[(sel - 2) / 4];
			newsel = (sel - 2) % 4;

			switch (newsel)
			{
				case 0: /* CPU */
					subcheat->cpu ++;
					/* skip audio CPUs when the sound is off */
					if (CPU_AUDIO_OFF(subcheat->cpu))
						subcheat->cpu ++;
					if (subcheat->cpu >= cpu_gettotalcpu())
						subcheat->cpu = 0;
					subcheat->address &= cpunum_address_mask(subcheat->cpu);
					break;
				case 1: /* address */
					textedit_active = 0;
					subcheat->address ++;
					subcheat->address &= cpunum_address_mask(subcheat->cpu);
					break;
				case 2: /* value */
					textedit_active = 0;
					subcheat->data ++;
					subcheat->data &= 0xff;
					break;
				case 3: /* code */
					textedit_active = 0;
					subcheat->code ++;
					break;
			}
		}
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == ((CheatTable[cheat_num].num_sub+1) * 4) + 2)
		{
			/* return to main menu */
			submenu_choice = 0;
			sel = -1;
		}
		else if (/*(sel == 4) ||*/ (sel == 0))
		{
			/* wait for key up */
			while (input_ui_pressed(IPT_UI_SELECT)) {};

			/* flush the text buffer */
			osd_readkey_unicode (1);
			textedit_active ^= 1;
		}
		else
		{
			submenu_choice = 1;
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		textedit_active = 0;
		/* flush the text buffer */
		osd_readkey_unicode (1);
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	/* After we've weeded out any control characters, look for text */
	if (textedit_active)
	{
		int code;

#if 0
		/* is this the address field? */
		if (sel == 1)
		{
			INT8 hex_val;

			/* see if a hex digit was typed */
			hex_val = code_read_hex_async();
			if (hex_val != -1)
			{
				/* shift over one digit, add in the new value and clip */
				subcheat->address <<= 4;
				subcheat->address |= hex_val;
				subcheat->address &= cpunum_address_mask(subcheat->cpu);
			}
		}
		else
#endif
		if (CheatTable[cheat_num].name)
		{
			int length = strlen(CheatTable[cheat_num].name);

			code = osd_readkey_unicode(0) & 0xff; /* no 16-bit support */

			if (code)
			{
				if (code == 0x08) /* backspace */
				{
					/* clear the buffer */
					CheatTable[cheat_num].name[0] = 0x00;
				}
				else
				{
					/* append the character */
					CheatTable[cheat_num].name = (char*)realloc(CheatTable[cheat_num].name, length + 2);
					if (CheatTable[cheat_num].name != NULL)
					{
						CheatTable[cheat_num].name[length] = code;
						CheatTable[cheat_num].name[length+1] = 0x00;
					}
				}
			}
		}
	}

	return sel + 1;
}


#ifdef macintosh
#pragma mark -
#endif

/* make a copy of a source ram table to a dest. ram table */
static void copy_ram (struct ExtMemory *dest, struct ExtMemory *src)
{
	struct ExtMemory *ext_dest, *ext_src;

	for (ext_src = src, ext_dest = dest; ext_src->data; ext_src++, ext_dest++)
	{
		memcpy (ext_dest->data, ext_src->data, ext_src->end - ext_src->start + 1);
	}
}

/* make a copy of each ram area from search CPU ram to the specified table */
static void backup_ram (struct ExtMemory *table, int cpu)
{
	struct ExtMemory *ext;
	unsigned char *gameram;

	for (ext = table; ext->data; ext++)
	{
		int i;
		gameram = memory_find_base (cpu, ext->start);
		memcpy (ext->data, gameram, ext->end - ext->start + 1);
		for (i=0; i <= ext->end - ext->start; i++)
			ext->data[i] = computer_readmem_byte(cpu, i+ext->start);
	}
}

/* set every byte in specified table to data */
static void memset_ram (struct ExtMemory *table, unsigned char data)
{
	struct ExtMemory *ext;

	for (ext = table; ext->data; ext++)
		memset (ext->data, data, ext->end - ext->start + 1);
}

/* free all the memory and init the table */
static void reset_table (struct ExtMemory *table)
{
	struct ExtMemory *ext;

	for (ext = table; ext->data; ext++)
		free(ext->data);
	memset (table, 0, sizeof (struct ExtMemory) * MAX_EXT_MEMORY);
}

/* create tables for storing copies of all MWA_RAM areas */
static int build_tables (int cpu)
{
	/* const struct MemoryReadAddress *mra = Machine->drv->cpu[SearchCpuNo].memory_read; */
	const struct MemoryWriteAddress *mwa = Machine->drv->cpu[cpu].memory_write;

	int region = REGION_CPU1+cpu;

	struct ExtMemory *ext_sr = StartRam;
	struct ExtMemory *ext_br = BackupRam;
	struct ExtMemory *ext_ft = FlagTable;

	struct ExtMemory *ext_obr = OldBackupRam;
	struct ExtMemory *ext_oft = OldFlagTable;

	static int bail = 0; /* set to 1 if this routine fails during startup */

	int i;

	int NoMemArea = 0;

	/* Trap memory allocation errors */
	int MemoryNeeded = 0;

	/* Search speedup : (the games should be dasmed to confirm this) */
	/* Games based on Exterminator driver should scan BANK1		   */
	/* Games based on SmashTV driver should scan BANK2		   */
	/* NEOGEO games should only scan BANK1 (0x100000 -> 0x01FFFF)    */
	int CpuToScan = -1;
	int BankToScanTable[9];	 /* 0 for RAM & 1-8 for Banks 1-8 */

	for (i = 0; i < 9;i ++)
	BankToScanTable[i] = ( fastsearch != 2 );

#if (HAS_TMS34010)
	if ((Machine->drv->cpu[1].cpu_type & ~CPU_FLAGS_MASK) == CPU_TMS34010)
	{
		/* 2nd CPU is 34010: games based on Exterminator driver */
		CpuToScan = 0;
		BankToScanTable[1] = 1;
	}
	else if ((Machine->drv->cpu[0].cpu_type & ~CPU_FLAGS_MASK) == CPU_TMS34010)
	{
		/* 1st CPU but not 2nd is 34010: games based on SmashTV driver */
		CpuToScan = 0;
		BankToScanTable[2] = 1;
	}
#endif
#ifndef MESS
#ifdef NEOMAME
	if (Machine->gamedrv->clone_of == &driver_neogeo)
	{
		/* games based on NEOGEO driver */
		CpuToScan = 0;
		BankToScanTable[1] = 1;
	}
#endif
#endif

	/* No CPU so we scan RAM & BANKn */
	if ((CpuToScan == -1) && (fastsearch == 2))
		for (i = 0; i < 9;i ++)
			BankToScanTable[i] = 1;

	/* free memory that was previously allocated if no error occured */
	/* it must also be there because mwa varies from one CPU to another */
	if (!bail)
	{
		reset_table (StartRam);
		reset_table (BackupRam);
		reset_table (FlagTable);

		reset_table (OldBackupRam);
		reset_table (OldFlagTable);
	}

	bail = 0;

#if 0
	/* Message to show that something is in progress */
	cheat_clearbitmap();
	yPos = (MachHeight - FontHeight) / 2;
	xprintf(0, 0, yPos, "Allocating Memory...");
#endif

	NoMemArea = 0;
	while (mwa->start != -1)
	{
		/* int (*handler)(int) = mra->handler; */
		mem_write_handler handler = mwa->handler;
		int size = (mwa->end - mwa->start) + 1;

		if (SkipBank(CpuToScan, BankToScanTable, handler))
		{
			NoMemArea++;
			mwa++;
			continue;
		}

#if 0
		if ((fastsearch == 3) && (!MemToScanTable[NoMemArea].Enabled))
		{
			NoMemArea++;
			mwa++;
			continue;
		}
#endif

		/* time to allocate */
		if (!bail)
		{
			ext_sr->data = (unsigned char*)malloc(size);
			ext_br->data = (unsigned char*)malloc(size);
			ext_ft->data = (unsigned char*)malloc(size);

			ext_obr->data = (unsigned char*)malloc(size);
			ext_oft->data = (unsigned char*)malloc(size);

			if (ext_sr->data == NULL)
			{
				bail = 1;
				MemoryNeeded += size;
			}
			if (ext_br->data == NULL)
			{
				bail = 1;
				MemoryNeeded += size;
			}
			if (ext_ft->data == NULL)
			{
				bail = 1;
				MemoryNeeded += size;
			}

			if (ext_obr->data == NULL)
			{
				bail = 1;
				MemoryNeeded += size;
			}
			if (ext_oft->data == NULL)
			{
				bail = 1;
				MemoryNeeded += size;
			}

			if (!bail)
			{
				ext_sr->start = ext_br->start = ext_ft->start = mwa->start;
				ext_sr->end = ext_br->end = ext_ft->end = mwa->end;
				ext_sr->region = ext_br->region = ext_ft->region = region;
				ext_sr++, ext_br++, ext_ft++;

				ext_obr->start = ext_oft->start = mwa->start;
				ext_obr->end = ext_oft->end = mwa->end;
				ext_obr->region = ext_oft->region = region;
				ext_obr++, ext_oft++;
			}
		}
		else
			MemoryNeeded += (5 * size);

		NoMemArea++;
		mwa++;
	}

	/* free memory that was previously allocated if an error occured */
	if (bail)
	{
		reset_table (StartRam);
		reset_table (BackupRam);
		reset_table (FlagTable);

		reset_table (OldBackupRam);
		reset_table (OldFlagTable);

#if 0
		cheat_clearbitmap();
		yPos = (MachHeight - 10 * FontHeight) / 2;
		xprintf(0, 0, yPos, "Error while allocating memory !");
		yPos += (2 * FontHeight);
		xprintf(0, 0, yPos, "You need %d more bytes", MemoryNeeded);
		yPos += FontHeight;
		xprintf(0, 0, yPos, "(0x%X) of free memory", MemoryNeeded);
		yPos += (2 * FontHeight);
		xprintf(0, 0, yPos, "No search available for CPU %d", currentSearchCPU);
		yPos += (4 * FontHeight);
		xprintf(0, 0, yPos, "Press A Key To Continue...");
		key = keyboard_read_sync();
		while (keyboard_pressed(key)) ; /* wait for key release */
		cheat_clearbitmap();
#endif
	  }

//	ClearTextLine (1, yPos);

	return bail;
}


/* Returns 1 if memory area has to be skipped */
static int SkipBank(int CpuToScan, int *BankToScanTable, mem_write_handler handler)
{
	int res = 0;

	if ((fastsearch == 1) || (fastsearch == 2))
	{
		if (((FPTR)handler)==((FPTR)MWA_RAM))
		{
			res = !BankToScanTable[0];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK1))
		{
			res = !BankToScanTable[1];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK2))
		{
			res = !BankToScanTable[2];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK3))
		{
			res = !BankToScanTable[3];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK4))
		{
			res = !BankToScanTable[4];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK5))
		{
			res = !BankToScanTable[5];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK6))
		{
			res = !BankToScanTable[6];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK7))
		{
			res = !BankToScanTable[7];
		}
		else if (((FPTR)handler)==((FPTR)MWA_BANK8))
		{
			res = !BankToScanTable[8];
		}
		else
		{
			res = 1;
		}		
	}
	return(res);
}

INT32 PerformSearch (struct osd_bitmap *bitmap, INT32 selected)
{
	return -1;
}


/*****************
 * Start a cheat search
 * If the method 1 is selected, ask the user a number
 * In all cases, backup the ram.
 *
 * Ask the user to select one of the following:
 *	1 - Lives or other number (byte) (exact)        ask a start value, ask new value
 *	2 - Timers (byte) (+ or - X)                    nothing at start,  ask +-X
 *	3 - Energy (byte) (less, equal or greater)	    nothing at start,  ask less, equal or greater
 *	4 - Status (bit)  (true or false)               nothing at start,  ask same or opposite
 *	5 - Slow but sure (Same as start or different)  nothing at start,  ask same or different
 *
 * Another method is used in the Pro action Replay the Energy method
 *	you can tell that the value is now 25%/50%/75%/100% as the start
 *	the problem is that I probably cannot search for exactly 50%, so
 *	that do I do? search +/- 10% ?
 * If you think of other way to search for codes, let me know.
 */

INT32 StartSearch (struct osd_bitmap *bitmap, INT32 selected)
{
	enum
	{
		Menu_CPU = 0,
		Menu_Value,
		Menu_Time,
		Menu_Energy,
		Menu_Bit,
		Menu_Byte,
		Menu_Speed,
		Menu_Return,
		Menu_Total
	};

	const char *menu_item[Menu_Total];
	const char *menu_subitem[Menu_Total];
	char setting[Menu_Total][30];
	INT32 sel;
	UINT8 total = 0;
	static INT8 submenu_choice;
	int i;
//	char flag[Menu_Total];

	sel = selected - 1;

	/* If a submenu has been selected, go there */
	if (submenu_choice)
	{
		switch (sel)
		{
			case Menu_Value:
				searchType = kSearch_Value;
				submenu_choice = PerformSearch (bitmap, submenu_choice);
				break;
			case Menu_Time:
				searchType = kSearch_Time;
//				submenu_choice = PerformSearch (submenu_choice);
				break;
			case Menu_Energy:
				searchType = kSearch_Energy;
//				submenu_choice = PerformSearch (submenu_choice);
				break;
			case Menu_Bit:
				searchType = kSearch_Bit;
//				submenu_choice = PerformSearch (submenu_choice);
				break;
			case Menu_Byte:
				searchType = kSearch_Byte;
//				submenu_choice = PerformSearch (submenu_choice);
				break;
			case Menu_Speed:
//				submenu_choice = RestoreSearch (submenu_choice);
				break;
			case Menu_Return:
				submenu_choice = 0;
				sel = -1;
				break;
		}

		if (submenu_choice == -1)
			submenu_choice = 0;

		return sel + 1;
	}

	/* No submenu active, display the main cheat menu */
	menu_item[total++] = ui_getstring (UI_cpu);
	menu_item[total++] = ui_getstring (UI_search_lives);
	menu_item[total++] = ui_getstring (UI_search_timers);
	menu_item[total++] = ui_getstring (UI_search_energy);
	menu_item[total++] = ui_getstring (UI_search_status);
	menu_item[total++] = ui_getstring (UI_search_slow);
	menu_item[total++] = ui_getstring (UI_search_speed);
	menu_item[total++] = ui_getstring (UI_returntoprior);
	menu_item[total] = 0;

	/* clear out the subitem menu */
	for (i = 0; i < Menu_Total; i ++)
		menu_subitem[i] = NULL;

	/* cpu number */
	sprintf (setting[Menu_CPU], "%d", searchCPU);
	menu_subitem[Menu_CPU] = setting[Menu_CPU];

	/* lives/byte value */
	sprintf (setting[Menu_Value], "%d", searchValue);
	menu_subitem[Menu_Value] = setting[Menu_Value];


	ui_displaymenu(bitmap,menu_item,menu_subitem,0,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
	{
		switch (sel)
		{
			case Menu_CPU:
				searchCPU --;
				/* skip audio CPUs when the sound is off */
				if (CPU_AUDIO_OFF(searchCPU))
					searchCPU --;
				if (searchCPU < 0)
					searchCPU = cpu_gettotalcpu() - 1;
				break;
			case Menu_Value:
				searchValue --;
				if (searchValue < 0)
					searchValue = 255;
				break;
		}
	}
	if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
	{
		switch (sel)
		{
			case Menu_CPU:
				searchCPU ++;
				/* skip audio CPUs when the sound is off */
				if (CPU_AUDIO_OFF(searchCPU))
					searchCPU ++;
				if (searchCPU >= cpu_gettotalcpu())
					searchCPU = 0;
				break;
			case Menu_Value:
				searchValue ++;
				if (searchValue > 255)
					searchValue = 0;
				break;
		}
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == Menu_Return)
		{
			submenu_choice = 0;
			sel = -1;
		}
		else
		{
			int count = 0;	/* Steph */

			/* set up the search tables */
			build_tables (searchCPU);

			/* backup RAM */
			backup_ram (StartRam, searchCPU);
			backup_ram (BackupRam, searchCPU);

			/* mark all RAM as good */
			memset_ram (FlagTable, 0xff);

			if (sel == Menu_Value)
			{
				/* flag locations that match the starting value */
				struct ExtMemory *ext;
				int j;	/* Steph - replaced all instances of 'i' with 'j' */

				count = 0;
				for (ext = FlagTable; ext->data; ext++)
				{
					for (j=0; j <= ext->end - ext->start; j++)
					if (ext->data[j] != 0)
					{
						if ((computer_readmem_byte(searchCPU, j+ext->start) != searchValue) &&
							((computer_readmem_byte(searchCPU, j+ext->start) != searchValue-1) /*||
							(searchType != kSearch_Value)*/))

							ext->data[j] = 0;
						else
							count ++;
					}
				}
			}

			/* Copy the tables */
			copy_ram (OldBackupRam, BackupRam);
			copy_ram (OldFlagTable, FlagTable);

			restoreStatus = kRestore_NoSave;

			usrintf_showmessage_secs(4, "%s: %d", ui_getstring(UI_search_matches_found), count);
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

INT32 ContinueSearch (INT32 selected)
{
	return -1;
}

INT32 ViewSearchResults (struct osd_bitmap *bitmap, INT32 selected)
{
	int sel;
	static INT8 submenu_choice;
	const char *menu_item[MAX_SEARCHES + 2];
	char buf[MAX_SEARCHES][20];
	int i, total = 0;
	struct ExtMemory *ext;
	struct ExtMemory *ext_sr;


	sel = selected - 1;

	/* Set up the menu */
	for (ext = FlagTable, ext_sr = StartRam; ext->data /*&& Continue==0*/; ext++, ext_sr++)
	{
		for (i=0; i <= ext->end - ext->start; i++)
			if (ext->data[i] != 0)
			{
				int TrueAddr, TrueData;
				char fmt[40];

				strcpy(fmt, FormatAddr(searchCPU,0));
				strcat(fmt," = %02X");

				TrueAddr = i+ext->start;
				TrueData = ext_sr->data[i];
				sprintf (buf[total], fmt, TrueAddr, TrueData);

				menu_item[total] = buf[total];
				total++;
				if (total >= MAX_SEARCHES)
				{
//					Continue = i+ext->start;
					break;
				}
			}
	}

	menu_item[total++] = ui_getstring (UI_returntoprior);
	menu_item[total] = 0;	/* terminate array */

	ui_displaymenu(bitmap,menu_item,0,0,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == total - 1)
		{
			submenu_choice = 0;
			sel = -1;
		}
		else
		{
			submenu_choice = 1;
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

void RestoreSearch (void)
{
	int restoreString = NULL;	/* Steph */

	switch (restoreStatus)
	{
		case kRestore_NoInit: restoreString = UI_search_noinit; break;
		case kRestore_NoSave: restoreString = UI_search_nosave; break;
		case kRestore_Done:   restoreString = UI_search_done; break;
		case kRestore_OK:     restoreString = UI_search_OK; break;
	}
	usrintf_showmessage_secs(4, "%s", ui_getstring(restoreString));

	/* Now restore the tables if possible */
	if (restoreStatus == kRestore_OK)
	{
		copy_ram (BackupRam, OldBackupRam);
		copy_ram (FlagTable, OldFlagTable);

		/* flag it as restored so we don't do it again */
		restoreStatus = kRestore_Done;
	}
}

#ifdef macintosh
#pragma mark -
#endif

/*
static int FindFreeWatch (void)
{
	int i;
	for (i = 0; i < MAX_WATCHES; i ++)
	{
		if (watches[i].num_bytes == 0)
			return i;
	}

	return -1;
}
*/

static void DisplayWatches (struct osd_bitmap *bitmap)
{
	int i;
	char buf[256];

	if ((!is_watch_active) || (!is_watch_visible)) return;

	for (i = 0; i < MAX_WATCHES; i++)
	{
		/* Is this watchpoint active? */
		if (watches[i].num_bytes != 0)
		{
			char buf2[80];

			/* Display the first byte */
			sprintf (buf, "%02x", computer_readmem_byte (watches[i].cpu, watches[i].address));

			/* If this is for more than one byte, display the rest */
			if (watches[i].num_bytes > 1)
			{
				int j;

				for (j = 1; j < watches[i].num_bytes; j ++)
				{
					sprintf (buf2, " %02x", computer_readmem_byte (watches[i].cpu, watches[i].address + j));
					strcat (buf, buf2);
				}
			}

			/* Handle any labels */
			switch (watches[i].label_type)
			{
				case 0:
				default:
					break;
				case 1:
					if (cpunum_address_bits(watches[i].cpu) <= 16)
					{
						sprintf (buf2, " (%04x)", watches[i].address);
						strcat (buf, buf2);
					}
					else
					{
						sprintf (buf2, " (%08x)", watches[i].address);
						strcat (buf, buf2);
					}
					break;
				case 2:
					{
						sprintf (buf2, " (%s)", watches[i].label);
						strcat (buf, buf2);
					}
					break;
			}

			ui_text (bitmap, buf, watches[i].x, watches[i].y);
		}
	}
}

static INT32 ConfigureWatch (struct osd_bitmap *bitmap, INT32 selected, UINT8 watchnum)
{
#ifdef NUM_ENTRIES
#undef NUM_ENTRIES
#endif
#define NUM_ENTRIES 9

	int sel;
	int total, total2;
	static INT8 submenu_choice;
	static UINT8 textedit_active;
	const char *menu_item[NUM_ENTRIES];
	const char *menu_subitem[NUM_ENTRIES];
	char setting[NUM_ENTRIES][30];
	char flag[NUM_ENTRIES];
	int arrowize;
	int i;


	sel = selected - 1;

	total = 0;
	/* No submenu active, display the main cheat menu */
	menu_item[total++] = ui_getstring (UI_cpu);
	menu_item[total++] = ui_getstring (UI_address);
	menu_item[total++] = ui_getstring (UI_watchlength);
	menu_item[total++] = ui_getstring (UI_watchlabeltype);
	menu_item[total++] = ui_getstring (UI_watchlabel);
	menu_item[total++] = ui_getstring (UI_watchx);
	menu_item[total++] = ui_getstring (UI_watchy);
	menu_item[total++] = ui_getstring (UI_returntoprior);
	menu_item[total] = 0;

	arrowize = 0;

	/* set up the submenu selections */
	total2 = 0;
	for (i = 0; i < NUM_ENTRIES; i ++)
		flag[i] = 0;

	/* if we're editing the label, make it inverse */
	if (textedit_active)
		flag[sel] = 1;

	/* cpu number */
	sprintf (setting[total2], "%d", watches[watchnum].cpu);
	menu_subitem[total2] = setting[total2]; total2++;

	/* address */
	if (cpunum_address_bits(watches[watchnum].cpu) <= 16)
	{
		sprintf (setting[total2], "%04x", watches[watchnum].address);
	}
	else
	{
		sprintf (setting[total2], "%08x", watches[watchnum].address);
	}
	menu_subitem[total2] = setting[total2]; total2++;

	/* length */
	sprintf (setting[total2], "%d", watches[watchnum].num_bytes);
	menu_subitem[total2] = setting[total2]; total2++;

	/* label type */
	switch (watches[watchnum].label_type)
	{
		case 0:
			strcpy (setting[total2], ui_getstring (UI_none));
			break;
		case 1:
			strcpy (setting[total2], ui_getstring (UI_address));
			break;
		case 2:
			strcpy (setting[total2], ui_getstring (UI_text));
			break;
	}
	menu_subitem[total2] = setting[total2]; total2++;

	/* label */
	if (watches[watchnum].label[0] != 0x00)
		sprintf (setting[total2], "%s", watches[watchnum].label);
	else
		strcpy (setting[total2], ui_getstring (UI_none));
	menu_subitem[total2] = setting[total2]; total2++;

	/* x */
	sprintf (setting[total2], "%d", watches[watchnum].x);
	menu_subitem[total2] = setting[total2]; total2++;

	/* y */
	sprintf (setting[total2], "%d", watches[watchnum].y);
	menu_subitem[total2] = setting[total2]; total2++;

	menu_subitem[total2] = NULL;

	ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel,arrowize);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
	{
		textedit_active = 0;
		sel = (sel + 1) % total;
	}

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
	{
		textedit_active = 0;
		sel = (sel + total - 1) % total;
	}

	if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
	{
		switch (sel)
		{
			case 0: /* CPU */
				watches[watchnum].cpu --;
				/* skip audio CPUs when the sound is off */
				if (CPU_AUDIO_OFF(watches[watchnum].cpu))
					watches[watchnum].cpu --;
				if (watches[watchnum].cpu < 0)
					watches[watchnum].cpu = cpu_gettotalcpu() - 1;
				watches[watchnum].address &= cpunum_address_mask(watches[watchnum].cpu);
				break;
			case 1: /* address */
				textedit_active = 0;
				watches[watchnum].address --;
				watches[watchnum].address &= cpunum_address_mask(watches[watchnum].cpu);
				break;
			case 2: /* number of bytes */
				watches[watchnum].num_bytes --;
				if (watches[watchnum].num_bytes == (UINT8) -1)
					watches[watchnum].num_bytes = 16;
				break;
			case 3: /* label type */
				watches[watchnum].label_type --;
				if (watches[watchnum].label_type == (UINT8) -1)
					watches[watchnum].label_type = 2;
				break;
			case 4: /* label string */
				textedit_active = 0;
				break;
			case 5: /* x */
				watches[watchnum].x --;
				if (watches[watchnum].x == (UINT16) -1)
					watches[watchnum].x = Machine->uiwidth - 1;
				break;
			case 6: /* y */
				watches[watchnum].y --;
				if (watches[watchnum].y == (UINT16) -1)
					watches[watchnum].y = Machine->uiheight - 1;
				break;
		}
	}

	if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
	{
		switch (sel)
		{
			case 0:
				watches[watchnum].cpu ++;
				/* skip audio CPUs when the sound is off */
				if (CPU_AUDIO_OFF(watches[watchnum].cpu))
					watches[watchnum].cpu ++;
				if (watches[watchnum].cpu >= cpu_gettotalcpu())
					watches[watchnum].cpu = 0;
				watches[watchnum].address &= cpunum_address_mask(watches[watchnum].cpu);
				break;
			case 1:
				textedit_active = 0;
				watches[watchnum].address ++;
				watches[watchnum].address &= cpunum_address_mask(watches[watchnum].cpu);
				break;
			case 2:
				watches[watchnum].num_bytes ++;
				if (watches[watchnum].num_bytes > 16)
					watches[watchnum].num_bytes = 0;
				break;
			case 3:
				watches[watchnum].label_type ++;
				if (watches[watchnum].label_type > 2)
					watches[watchnum].label_type = 0;
				break;
			case 4:
				textedit_active = 0;
				break;
			case 5:
				watches[watchnum].x ++;
				if (watches[watchnum].x >= Machine->uiwidth)
					watches[watchnum].x = 0;
				break;
			case 6:
				watches[watchnum].y ++;
				if (watches[watchnum].y >= Machine->uiheight)
					watches[watchnum].y = 0;
				break;
		}
	}

	/* see if any watchpoints are active and set the flag if so */
	is_watch_active = 0;
	for (i = 0; i < MAX_WATCHES; i ++)
	{
		if (watches[i].num_bytes != 0)
		{
			is_watch_active = 1;
			break;
		}
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == 7)
		{
			/* return to main menu */
			submenu_choice = 0;
			sel = -1;
		}
		else if ((sel == 4) || (sel == 1))
		{
			/* wait for key up */
			while (input_ui_pressed(IPT_UI_SELECT)) {};

			/* flush the text buffer */
			osd_readkey_unicode (1);
			textedit_active ^= 1;
		}
		else
		{
			submenu_choice = 1;
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		textedit_active = 0;
		/* flush the text buffer */
		osd_readkey_unicode (1);
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	/* After we've weeded out any control characters, look for text */
	if (textedit_active)
	{
		int code;

		/* is this the address field? */
		if (sel == 1)
		{
			INT8 hex_val;

			/* see if a hex digit was typed */
			hex_val = code_read_hex_async();
			if (hex_val != -1)
			{
				/* shift over one digit, add in the new value and clip */
				watches[watchnum].address <<= 4;
				watches[watchnum].address |= hex_val;
				watches[watchnum].address &= cpunum_address_mask(watches[watchnum].cpu);
			}
		}
		else
		{
			int length = strlen(watches[watchnum].label);

			if (length < 254)
			{
				code = osd_readkey_unicode(0) & 0xff; /* no 16-bit support */

				if (code)
				{
					if (code == 0x08) /* backspace */
					{
						/* clear the buffer */
						watches[watchnum].label[0] = 0x00;
					}
					else
					{
						/* append the character */
						watches[watchnum].label[length] = code;
						watches[watchnum].label[length+1] = 0x00;
					}
				}
			}
		}
	}

	return sel + 1;
}

static INT32 ChooseWatch (struct osd_bitmap *bitmap, INT32 selected)
{
	int sel;
	static INT8 submenu_choice;
	const char *menu_item[MAX_WATCHES + 2];
	char buf[MAX_WATCHES][80];
	const char *watchpoint_str = ui_getstring (UI_watchpoint);
	const char *disabled_str = ui_getstring (UI_disabled);
	int i, total = 0;


	sel = selected - 1;

	/* If a submenu has been selected, go there */
	if (submenu_choice)
	{
		submenu_choice = ConfigureWatch (bitmap, submenu_choice, sel);

		if (submenu_choice == -1)
		{
			submenu_choice = 0;
			sel = -2;
		}

		return sel + 1;
	}

	/* No submenu active, do the watchpoint menu */
	for (i = 0; i < MAX_WATCHES; i ++)
	{
		sprintf (buf[i], "%s %d: ", watchpoint_str, i);
		/* If the watchpoint is active (1 or more bytes long), show it */
		if (watches[i].num_bytes)
		{
			char buf2[80];

			if (cpunum_address_bits(watches[i].cpu) <= 16)
			{
				sprintf (buf2, "%04x", watches[i].address);
				strcat (buf[i], buf2);
			}
			else
			{
				sprintf (buf2, "%08x", watches[i].address);
				strcat (buf[i], buf2);
			}
		}
		else
			strcat (buf[i], disabled_str);

		menu_item[total++] = buf[i];
	}

	menu_item[total++] = ui_getstring (UI_returntoprior);
	menu_item[total] = 0;	/* terminate array */

	ui_displaymenu(bitmap,menu_item,0,0,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == MAX_WATCHES)
		{
			submenu_choice = 0;
			sel = -1;
		}
		else
		{
			submenu_choice = 1;
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}


#ifdef macintosh
#pragma mark -
#endif

static INT32 DisplayHelpFile (INT32 selected)
{
	return -1;
}

#ifdef macintosh
#pragma mark -
#endif

INT32 cheat_menu(struct osd_bitmap *bitmap, INT32 selected)
{
	enum {
		Menu_EnableDisable = 0,
		Menu_AddEdit,
		Menu_StartSearch,
		Menu_ContinueSearch,
		Menu_ViewResults,
		Menu_RestoreSearch,
		Menu_ChooseWatch,
		Menu_DisplayHelp,
		Menu_Return
	};

	const char *menu_item[10];
	INT32 sel;
	UINT8 total = 0;
	static INT8 submenu_choice;

	sel = selected - 1;

	/* If a submenu has been selected, go there */
	if (submenu_choice)
	{
		switch (sel)
		{
			case Menu_EnableDisable:
				submenu_choice = EnableDisableCheatMenu (bitmap, submenu_choice);
				break;
			case Menu_AddEdit:
				submenu_choice = AddEditCheatMenu (bitmap, submenu_choice);
				break;
			case Menu_StartSearch:
				submenu_choice = StartSearch (bitmap, submenu_choice);
				break;
			case Menu_ContinueSearch:
				submenu_choice = ContinueSearch (submenu_choice);
				break;
			case Menu_ViewResults:
				submenu_choice = ViewSearchResults (bitmap, submenu_choice);
				break;
			case Menu_ChooseWatch:
				submenu_choice = ChooseWatch (bitmap, submenu_choice);
				break;
			case Menu_DisplayHelp:
				submenu_choice = DisplayHelpFile (submenu_choice);
				break;
			case Menu_Return:
				submenu_choice = 0;
				sel = -1;
				break;
		}

		if (submenu_choice == -1)
			submenu_choice = 0;

		return sel + 1;
	}

	/* No submenu active, display the main cheat menu */
	menu_item[total++] = ui_getstring (UI_enablecheat);
	menu_item[total++] = ui_getstring (UI_addeditcheat);
	menu_item[total++] = ui_getstring (UI_startcheat);
	menu_item[total++] = ui_getstring (UI_continuesearch);
	menu_item[total++] = ui_getstring (UI_viewresults);
	menu_item[total++] = ui_getstring (UI_restoreresults);
	menu_item[total++] = ui_getstring (UI_memorywatch);
	menu_item[total++] = ui_getstring (UI_generalhelp);
	menu_item[total++] = ui_getstring (UI_returntomain);
	menu_item[total] = 0;

	ui_displaymenu(bitmap,menu_item,0,0,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == Menu_Return)
		{
			submenu_choice = 0;
			sel = -1;
		}
		else if (sel == Menu_RestoreSearch)
		{
			RestoreSearch ();
		}
		else
		{
			submenu_choice = 1;
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	/* Cancel pops us up a menu level */
	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	/* The UI key takes us all the way back out */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

/* Free allocated arrays */
void StopCheat(void)
{
	int i;

	for (i = 0; i < LoadedCheatTotal; i ++)
	{
		/* free storage for the strings */
		if (CheatTable[i].name)
		{
			free(CheatTable[i].name);
			CheatTable[i].name = NULL;
		}
		if (CheatTable[i].comment)
		{
			free(CheatTable[i].comment);
			CheatTable[i].comment = NULL;
		}
	}

	reset_table (StartRam);
	reset_table (BackupRam);
	reset_table (FlagTable);

	reset_table (OldBackupRam);
	reset_table (OldFlagTable);
}

void DoCheat(struct osd_bitmap *bitmap)
{
	DisplayWatches (bitmap);

	if ((CheatEnabled) && (ActiveCheatTotal))
	{
		int i, j;

		/* At least one cheat is active, handle them */
		for (i = 0; i < LoadedCheatTotal; i ++)
		{
			/* skip if this isn't an active cheat */
			if ((CheatTable[i].flags & CHEAT_FLAG_ACTIVE) == 0) continue;

			/* loop through all subcheats */
			for (j = 0; j <= CheatTable[i].num_sub; j ++)
			{
				struct subcheat_struct *subcheat = &CheatTable[i].subcheat[j];

				if (subcheat->flags & SUBCHEAT_FLAG_DONE) continue;

				/* most common case: 0 */
				if (subcheat->code == kCheatSpecial_Poke)
				{
					WRITE_CHEAT;
				}
				/* Check special function if cheat counter is ready */
				else if (subcheat->frame_count == 0)
				{
					switch (subcheat->code)
					{
						case 1:
							WRITE_CHEAT;
							subcheat->flags |= SUBCHEAT_FLAG_DONE;
							break;
						case 2:
						case 3:
						case 4:
							WRITE_CHEAT;
							subcheat->frame_count = subcheat->frames_til_trigger;
							break;

						/* 5,6,7 check if the value has changed, if yes, start a timer. */
						/* When the timer ends, change the location */
						case 5:
						case 6:
						case 7:
							if (subcheat->flags & SUBCHEAT_FLAG_TIMED)
							{
								WRITE_CHEAT;
								subcheat->flags &= ~SUBCHEAT_FLAG_TIMED;
							}
							else if (COMPARE_CHEAT)
							{
								subcheat->frame_count = subcheat->frames_til_trigger;
								subcheat->flags |= SUBCHEAT_FLAG_TIMED;
							}
							break;

						/* 8,9,10,11 do not change the location if the value change by X every frames
						  This is to try to not change the value of an energy bar
				 		  when a bonus is awarded to it at the end of a level
				 		  See Kung Fu Master */
						case 8:
						case 9:
						case 10:
						case 11:
							if (subcheat->flags & SUBCHEAT_FLAG_TIMED)
							{
								/* Check the value to see if it has increased over the original value by 1 or more */
								if (READ_CHEAT != subcheat->backup - (kCheatSpecial_Backup1 - subcheat->code + 1))
									WRITE_CHEAT;
								subcheat->flags &= ~SUBCHEAT_FLAG_TIMED;
							}
							else
							{
								subcheat->backup = READ_CHEAT;
								subcheat->frame_count = 1;
								subcheat->flags |= SUBCHEAT_FLAG_TIMED;
							}
							break;

						/* 20-24: set bits */
						case 20:
							computer_writemem_byte (subcheat->cpu, subcheat->address, READ_CHEAT | subcheat->data);
							break;
						case 21:
							computer_writemem_byte (subcheat->cpu, subcheat->address, READ_CHEAT | subcheat->data);
							subcheat->flags |= SUBCHEAT_FLAG_DONE;
							break;
						case 22:
						case 23:
						case 24:
							computer_writemem_byte (subcheat->cpu, subcheat->address, READ_CHEAT | subcheat->data);
							subcheat->frame_count = subcheat->frames_til_trigger;
							break;

						/* 40-44: reset bits */
						case 40:
							computer_writemem_byte (subcheat->cpu, subcheat->address, READ_CHEAT & ~subcheat->data);
							break;
						case 41:
							computer_writemem_byte (subcheat->cpu, subcheat->address, READ_CHEAT & ~subcheat->data);
							subcheat->flags |= SUBCHEAT_FLAG_DONE;
							break;
						case 42:
						case 43:
						case 44:
							computer_writemem_byte (subcheat->cpu, subcheat->address, READ_CHEAT & ~subcheat->data);
							subcheat->frame_count = subcheat->frames_til_trigger;
							break;

						/* 60-65: user select, poke when changes */
						case 60: case 61: case 62: case 63: case 64: case 65:
							if (subcheat->flags & SUBCHEAT_FLAG_TIMED)
							{
								if (READ_CHEAT != subcheat->backup)
								{
									WRITE_CHEAT;
									subcheat->flags |= SUBCHEAT_FLAG_DONE;
								}
							}
							else
							{
								subcheat->backup = READ_CHEAT;
								subcheat->frame_count = 1;
								subcheat->flags |= SUBCHEAT_FLAG_TIMED;
							}
							break;

						/* 70-75: user select, poke once */
						case 70: case 71: case 72: case 73: case 74: case 75:
							WRITE_CHEAT;
							subcheat->flags |= SUBCHEAT_FLAG_DONE;
						break;
					}
				}
				else
				{
					subcheat->frame_count--;
				}
			}
		} /* end for */
	}

	/* IPT_UI_TOGGLE_CHEAT Enable/Disable the active cheats on the fly. Required for some cheats. */
	if (input_ui_pressed(IPT_UI_TOGGLE_CHEAT))
	{
		/* Hold down shift to toggle the watchpoints */
		if (code_pressed(KEYCODE_LSHIFT) || code_pressed(KEYCODE_RSHIFT))
		{
			is_watch_visible ^= 1;
			usrintf_showmessage("%s %s", ui_getstring (UI_watchpoints), (is_watch_visible ? ui_getstring (UI_on) : ui_getstring (UI_off)));
		}
		else if (ActiveCheatTotal)
		{
			CheatEnabled ^= 1;
			usrintf_showmessage("%s %s", ui_getstring (UI_cheats), (CheatEnabled ? ui_getstring (UI_on) : ui_getstring (UI_off)));
		}
	}

#if 0
  /* KEYCODE_END loads the "Continue Search" sub-menu of the cheat engine */
  if ( keyboard_pressed_memory( KEYCODE_END ) )
  {
	osd_sound_enable(0);
	osd_pause(1);
	ContinueSearch(0, 0);
	osd_pause(0);
	osd_sound_enable(1);
  }
#endif
}
