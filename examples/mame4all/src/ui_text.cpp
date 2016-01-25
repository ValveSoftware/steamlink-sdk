/*********************************************************************

  ui_text.c

  Functions used to retrieve text used by MAME, to aid in
  translation.

*********************************************************************/

#include "driver.h"
#include "ui_text.h"

struct lang_struct lang;

char *trans_text[UI_last_entry + 1];

/* All entries in this table must match the enum ordering in "ui_text.h" */
const char * default_text[] =
{
#ifndef MESS
	"MAME",
#else
	"MESS",
#endif

	/* copyright stuff */
	"Usage of emulators in conjunction with ROMs you don't own is forbidden by copyright law.",
	"IF YOU ARE NOT LEGALLY ENTITLED TO PLAY \"%s\" ON THIS EMULATOR, PRESS ESC.",
	"Otherwise, type OK to continue",

	/* misc stuff */
	"Return to Main Menu",
	"Return to Prior Menu",
	"Press Any Key",
	"On",
	"Off",
	"NA",
	"INVALID",
	"(none)",
	"CPU",
	"Address",
	"Value",
	"Sound",
	"sound",
	"stereo",
	"Vector Game",
	"Screen Resolution",
	"Text",
	"Volume",
	"Relative",
	"ALL CHANNELS",
	"Brightness",
	"Gamma",
	"Vector Intensity",
	"Overclock",
	"ALL CPUS",
#ifndef MESS
	"History not available",
#else
	"System Info not available",
#endif

	/* special characters */
	"\x11",
	"\x10",
	"\x18",
	"\x19",
	"\x1a",
	"\x1b",

	/* known problems */
#ifndef MESS
	"There are known problems with this game:",
#else
	"There are known problems with this system",
#endif
	"The colors aren't 100% accurate.",
	"The colors are completely wrong.",
	"The sound emulation isn't 100% accurate.",
	"The game lacks sound.",
	"Screen flipping in cocktail mode is not supported.",
#ifndef MESS
	"THIS GAME DOESN'T WORK PROPERLY",
#else
	"THIS SYSTEM DOESN'T WORK PROPERLY",
#endif
	"The game has protection which isn't fully emulated.",
	"There are working clones of this game. They are:",
	"Type OK to continue",
#ifdef MESS
	"The emulated system is a computer: ",
	"The keyboard emulation may not be 100% accurate.",
#endif

	/* main menu */
	"Input (general)",
	"Dip Switches",
	"Analog Controls",
	"Calibrate Joysticks",
	"Bookkeeping Info",
#ifndef MESS
	"Input (this game)",
	"Game Information",
	"Game History",
	"Reset Game",
	"Return to Game",
#else
	"Input (this machine)",
	"Machine Information",
	"Machine Usage & History",
	"Reset Machine",
	"Return to Machine",
	"Image Information",
	"File Manager",
	"Tape Control",
#endif
	"Cheat",
	"Memory Card",

	/* input */
	"Key/Joy Speed",
	"Reverse",
	"Sensitivity",

	/* stats */
	"Tickets dispensed",
	"Coin",
	"(locked)",

	/* memory card */
	"Load Memory Card",
	"Eject Memory Card",
	"Create Memory Card",
	"Call Memory Card Manager (RESET)",
	"Failed To Load Memory Card!",
	"Load OK!",
	"Memory Card Ejected!",
	"Memory Card Created OK!",
	"Failed To Create Memory Card!",
	"(It already exists ?)",
	"DAMN!! Internal Error!",

	/* cheats */
	"Enable/Disable a Cheat",
	"Add/Edit a Cheat",
	"Start a New Cheat Search",
	"Continue Search",
	"View Last Results",
	"Restore Previous Results",
	"Configure Watchpoints",
	"General Help",
	"Watchpoint",
	"Disabled",
	"Cheats",
	"Watchpoints",
	"More Info",
	"More Info for",
	"Name",
	"Description",
	"Code",

	/* watchpoints */
	"Number of bytes",
	"Label type",
	"Label",
	"X position",
	"Y position",
	"Watch",

	/* searching */
	"Lives (or some other value)",
	"Timers (+/- some value)",
	"Energy (greater or less)",
	"Status (bits or flags)",
	"Slow but sure (changed or not)",
	"Change Search Speed",
	"Matches found",
	"Search not initialized",
	"No previous values saved",
	"Previous values already restored",
	"Restoration successful",

	NULL
};

int uistring_init (void *langfile)
{
	/*
		TODO: This routine needs to do several things:
			- load an external font if needed
			- determine the number of characters in the font
			- deal with multibyte languages

	*/

	int i;
	char curline[255];
	char section[255] = "\0";
	char *ptr;

	/* Clear out any default strings */
	for (i = 0; i <= UI_last_entry; i ++)
	{
		trans_text[i] = NULL;
	}

	memset(&lang, 0, sizeof(lang));

	if (!langfile) return 0;

	while (osd_fgets (curline, 255, langfile) != NULL)
	{
		/* Ignore commented and blank lines */
		if (curline[0] == ';') continue;
		if (curline[0] == '\n') continue;

		if (curline[0] == '[')
		{
			ptr = strtok (&curline[1], "]");
			/* Found a section, indicate as such */
			strcpy (section, ptr);

			/* Skip to the next line */
			continue;
		}

		/* Parse the LangInfo section */
		if (strcmp (section, "LangInfo") == 0)
		{
			ptr = strtok (curline, "=");
			if (strcmp (ptr, "Version") == 0)
			{
				ptr = strtok (NULL, "\n");
				sscanf (ptr, "%d", &lang.version);
			}
			else if (strcmp (ptr, "Language") == 0)
			{
				ptr = strtok (NULL, "\n");
				strcpy (lang.langname, ptr);
			}
			else if (strcmp (ptr, "Author") == 0)
			{
				ptr = strtok (NULL, "\n");
				strcpy (lang.author, ptr);
			}
			else if (strcmp (ptr, "Font") == 0)
			{
				ptr = strtok (NULL, "\n");
				strcpy (lang.fontname, ptr);
			}
		}

		/* Parse the Strings section */
		if (strcmp (section, "Strings") == 0)
		{
			/* Get all text up to the first line ending */
			ptr = strtok (curline, "\n");

			/* Find a matching default string */
			for (i = 0; i < UI_last_entry; i ++)
			{
				if (strcmp (curline, default_text[i]) == 0)
				{
					char transline[255];

					/* Found a match, read next line as the translation */
					osd_fgets (transline, 255, langfile);

					/* Get all text up to the first line ending */
					ptr = strtok (transline, "\n");

					/* Allocate storage and copy the string */
					trans_text[i] = (char*) malloc(strlen(transline)+1);
					strcpy (trans_text[i], transline);

					/* Done searching */
					break;
				}
			}
		}
	}

	/* indicate success */
	return 0;
}

void uistring_shutdown (void)
{
	int i;

	/* Deallocate storage for the strings */
	for (i = 0; i <= UI_last_entry; i ++)
	{
		if (trans_text[i])
		{
			free(trans_text[i]);
			trans_text[i] = NULL;
		}
	}
}

const char * ui_getstring (int string_num)
{
	/* Try to use the language file strings first */
	if (trans_text[string_num] != NULL)
		return trans_text[string_num];
	else
		/* That failed, use the default strings */
		return default_text[string_num];
}
