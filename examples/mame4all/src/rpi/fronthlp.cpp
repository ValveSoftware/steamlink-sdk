#include "driver.h"
#include "info.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <dirent.h>
#include <unzip.h>
#include "zlib.h"

#ifdef MESS
#include "mess/msdos.h"
#endif

#ifdef printf
#undef printf
#endif

int silentident,knownstatus;

#define KNOWN_START 0
#define KNOWN_ALL   1
#define KNOWN_NONE  2
#define KNOWN_SOME  3

//extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);


void get_rom_sample_path (int argc, char **argv, int game_index);

static const struct GameDriver *gamedrv;

/* compare string[8] using standard(?) DOS wildchars ('?' & '*')      */
/* for this to work correctly, the shells internal wildcard expansion */
/* mechanism has to be disabled. Look into msdos.c */

int strwildcmp(const char *sp1, const char *sp2)
{
	char s1[9], s2[9];
	int i, l1, l2;
	char *p;

	strncpy(s1, sp1, 8); s1[8] = 0; if (s1[0] == 0) strcpy(s1, "*");

	strncpy(s2, sp2, 8); s2[8] = 0; if (s2[0] == 0) strcpy(s2, "*");

	p = strchr(s1, '*');
	if (p)
	{
		for (i = p - s1; i < 8; i++) s1[i] = '?';
		s1[8] = 0;
	}

	p = strchr(s2, '*');
	if (p)
	{
		for (i = p - s2; i < 8; i++) s2[i] = '?';
		s2[8] = 0;
	}

	l1 = strlen(s1);
	if (l1 < 8)
	{
		for (i = l1 + 1; i < 8; i++) s1[i] = ' ';
		s1[8] = 0;
	}

	l2 = strlen(s2);
	if (l2 < 8)
	{
		for (i = l2 + 1; i < 8; i++) s2[i] = ' ';
		s2[8] = 0;
	}

	for (i = 0; i < 8; i++)
	{
		if (s1[i] == '?' && s2[i] != '?') s1[i] = s2[i];
		if (s2[i] == '?' && s1[i] != '?') s2[i] = s1[i];
	}

	return strcasecmp(s1, s2);
}


/* Identifies a rom from from this checksum */
void identify_rom(const char* name, int checksum, int length)
{
/* Nicola output format */
#if 1
	int found = 0;

	/* remove directory name */
	int i;
	for (i = strlen(name)-1;i >= 0;i--)
	{
		if (name[i] == '/' || name[i] == '\\')
		{
			i++;
			break;
		}
	}
	if (!silentident)
		printf("%-12s ",&name[i]);

	for (i = 0; drivers[i]; i++)
	{
		const struct RomModule *romp;

		romp = drivers[i]->rom;

		while (romp && (romp->name || romp->offset || romp->length))
		{
			if (romp->name && romp->name != (char *)-1 && checksum == romp->crc)
			{
				if (!silentident)
				{
					if (found != 0)
						printf("             ");
					printf("= %-12s  %s\n",romp->name,drivers[i]->description);
				}
				found++;
			}
			romp++;
		}
	}
	if (found == 0)
	{
		unsigned size = length;
		while (size && (size & 1) == 0) size >>= 1;
		if (size & ~1)
		{
			if (!silentident)
				printf("NOT A ROM\n");
		}
		else
		{
			if (!silentident)
				printf("NO MATCH\n");
			if (knownstatus == KNOWN_START)
				knownstatus = KNOWN_NONE;
			else if (knownstatus == KNOWN_ALL)
				knownstatus = KNOWN_SOME;
		}
	}
	else
	{
		if (knownstatus == KNOWN_START)
			knownstatus = KNOWN_ALL;
		else if (knownstatus == KNOWN_NONE)
			knownstatus = KNOWN_SOME;
	}
#else
/* New output format */
	int i;
	printf("%s\n",name);

	for (i = 0; drivers[i]; i++) {
		const struct RomModule *romp;

		romp = drivers[i]->rom;

		while (romp && (romp->name || romp->offset || romp->length))
		{
			if (romp->name && romp->name != (char *)-1 && checksum == romp->crc)
			{
				printf("\t%s/%s %s, %s, %s\n",drivers[i]->name,romp->name,
					drivers[i]->description,
					drivers[i]->manufacturer,
					drivers[i]->year);
			}
			romp++;
		}
	}
#endif
}

/* Identifies a file from from this checksum */
void identify_file(const char* name)
{
	FILE *f;
	int length;
	char* data;

	f = fopen(name,"rb");
	if (!f) {
		return;
	}

	/* determine length of file */
	if (fseek (f, 0L, SEEK_END)!=0)	{
		fclose(f);
		return;
	}

	length = ftell(f);
	if (length == -1L) {
		fclose(f);
		return;
	}

	/* empty file */
	if (!length) {
		fclose(f);
		return;
	}

	/* allocate space for entire file */
	data = (char*)malloc(length);
	if (!data) {
		fclose(f);
		return;
	}

	if (fseek (f, 0L, SEEK_SET)!=0) {
		free(data);
		fclose(f);
		return;
	}

	if (fread(data, 1, length, f) != length) {
		free(data);
		fclose(f);
		return;
	}

	fclose(f);

	identify_rom(name, crc32(0L,(const unsigned char*)data,length),length);

	free(data);
}

void identify_zip(const char* zipname)
{
	struct zipent* ent;

	ZIP* zip = openzip( zipname );
	if (!zip)
		return;

	while ((ent = readzip(zip))) {
		/* Skip empty file and directory */
		if (ent->uncompressed_size!=0) {
			char* buf = (char*)malloc(strlen(zipname)+1+strlen(ent->name)+1);
			sprintf(buf,"%s/%s",zipname,ent->name);
			identify_rom(buf,ent->crc32,ent->uncompressed_size);
			free(buf);
		}
	}

	closezip(zip);
}

void romident(const char* name, int enter_dirs);

void identify_dir(const char* dirname)
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(dirname);
	if (!dir) {
		return;
	}

	ent = readdir(dir);
	while (ent) {
		/* Skip special files */
		if (ent->d_name[0]!='.') {
			char* buf = (char*)malloc(strlen(dirname)+1+strlen(ent->d_name)+1);
			sprintf(buf,"%s/%s",dirname,ent->d_name);
			romident(buf,0);
			free(buf);
		}

		ent = readdir(dir);
	}
	closedir(dir);
}

void romident(const char* name,int enter_dirs) {
	struct stat s;

	if (stat(name,&s) != 0)	{
		printf("%s: %s\n",name,strerror(errno));
		return;
	}

	if (S_ISDIR(s.st_mode)) {
		if (enter_dirs)
			identify_dir(name);
	} else {
		unsigned l = strlen(name);
		if (l>=4 && strcasecmp(name+l-4,".zip")==0)
			identify_zip(name);
		else
			identify_file(name);
		return;
	}
}


#ifndef MESS
enum { LIST_LIST = 1, LIST_LISTINFO, LIST_LISTFULL, LIST_LISTSAMDIR, LIST_LISTROMS, LIST_LISTSAMPLES,
		LIST_LMR, LIST_LISTDETAILS, LIST_GAMELISTHEADER, LIST_GAMELISTFOOTER, LIST_GAMELIST,
		LIST_LISTGAMES, LIST_LISTCLONES,
		LIST_WRONGORIENTATION, LIST_WRONGFPS, LIST_LISTCRC, LIST_LISTDUPCRC, LIST_WRONGMERGE,
		LIST_LISTROMSIZE, LIST_LISTCPU, LIST_SOURCEFILE };
#else
enum { LIST_LIST = 1, LIST_LISTINFO, LIST_LISTFULL, LIST_LISTSAMDIR, LIST_LISTROMS, LIST_LISTSAMPLES,
		LIST_LMR, LIST_LISTDETAILS, LIST_GAMELISTHEADER, LIST_GAMELISTFOOTER, LIST_GAMELIST,
		LIST_LISTGAMES, LIST_LISTCLONES,
		LIST_WRONGORIENTATION, LIST_WRONGFPS, LIST_LISTCRC, LIST_LISTDUPCRC, LIST_WRONGMERGE,
		LIST_LISTROMSIZE, LIST_LISTCPU, LIST_SOURCEFILE, LIST_MESSINFO };
#endif


#define VERIFY_ROMS		0x00000001
#define VERIFY_SAMPLES	0x00000002
#define VERIFY_VERBOSE	0x00000004
#define VERIFY_TERSE	0x00000008

void CLIB_DECL terse_printf(char *fmt,...)
{
	/* no-op */
}


int frontend_help (int argc, char **argv)
{
	int i, j;
    int list = 0;
	int listclones = 1;
	int verify = 0;
	int ident = 0;
	int help = 1;    /* by default is TRUE */
	char gamename[9];

	/* covert '/' in '-' */
	for (i = 1;i < argc;i++) if (argv[i][0] == '/') argv[i][0] = '-';

	/* by default display the help unless */
	/* a game or an utility are specified */

	strcpy(gamename, "");

	for (i = 1;i < argc;i++)
	{
		/* find the FIRST "gamename" field (without '-') */
		if ((strlen(gamename) == 0) && (argv[i][0] != '-'))
		{
			/* check if a filename was entered as the game name */
			/* and remove any remaining portion of file extension */
			for (j = 0;j < 8 && argv[i][j] && argv[i][j] != '.';j++)
				gamename[j] = argv[i][j];
			gamename[j] = 0;
		}
	}

	for (i = 1; i < argc; i++)
	{
		/* check for front-end utilities */
		if (!strcasecmp(argv[i],"-list")) list = LIST_LIST;
 		if (!strcasecmp(argv[i],"-listinfo")) list = LIST_LISTINFO;
		if (!strcasecmp(argv[i],"-listfull")) list = LIST_LISTFULL;
        if (!strcasecmp(argv[i],"-listdetails")) list = LIST_LISTDETAILS; /* A detailed MAMELIST.TXT type roms lister */
        if (!strcasecmp(argv[i],"-gamelistheader")) list = LIST_GAMELISTHEADER; /* GAMELIST.TXT */
        if (!strcasecmp(argv[i],"-gamelistfooter")) list = LIST_GAMELISTFOOTER; /* GAMELIST.TXT */
        if (!strcasecmp(argv[i],"-gamelist")) list = LIST_GAMELIST; /* GAMELIST.TXT */
		if (!strcasecmp(argv[i],"-listgames")) list = LIST_LISTGAMES;
		if (!strcasecmp(argv[i],"-listclones")) list = LIST_LISTCLONES;
		if (!strcasecmp(argv[i],"-listsamdir")) list = LIST_LISTSAMDIR;
		if (!strcasecmp(argv[i],"-listcrc")) list = LIST_LISTCRC;
		if (!strcasecmp(argv[i],"-listdupcrc")) list = LIST_LISTDUPCRC;
		if (!strcasecmp(argv[i],"-listwrongmerge")) list = LIST_WRONGMERGE;
		if (!strcasecmp(argv[i],"-listromsize")) list = LIST_LISTROMSIZE;
		if (!strcasecmp(argv[i],"-listcpu")) list = LIST_LISTCPU;
		if (!strcasecmp(argv[i],"-wrongorientation")) list = LIST_WRONGORIENTATION;
		if (!strcasecmp(argv[i],"-wrongfps")) list = LIST_WRONGFPS;
		if (!strcasecmp(argv[i],"-noclones")) listclones = 0;
		#ifdef MESS
				if (!strcasecmp(argv[i],"-listdevices"))  list = LIST_MESSINFO;
				if (!strcasecmp(argv[i],"-listtext")) list = LIST_MESSINFO;
				if (!strcasecmp(argv[i],"-createdir")) list = LIST_MESSINFO;
		#endif


		/* these options REQUIRES gamename field to work */
		if (strlen(gamename) > 0)
		{
			if (!strcasecmp(argv[i],"-listroms")) list = LIST_LISTROMS;
			if (!strcasecmp(argv[i],"-listsamples")) list = LIST_LISTSAMPLES;
			if (!strcasecmp(argv[i],"-verifyroms")) verify = VERIFY_ROMS;
			if (!strcasecmp(argv[i],"-verifysets")) verify = VERIFY_ROMS|VERIFY_VERBOSE|VERIFY_TERSE;
			if (!strcasecmp(argv[i],"-vset")) verify = VERIFY_ROMS|VERIFY_VERBOSE;
			if (!strcasecmp(argv[i],"-verifysamples")) verify = VERIFY_SAMPLES|VERIFY_VERBOSE;
			if (!strcasecmp(argv[i],"-vsam")) verify = VERIFY_SAMPLES|VERIFY_VERBOSE;
			if (!strcasecmp(argv[i],"-romident")) ident = 1;
			if (!strcasecmp(argv[i],"-isknown")) ident = 2;
			if (!strcasecmp(argv[i],"-sourcefile")) list = LIST_SOURCEFILE;
		}
	}

	if ((strlen(gamename)> 0) || list || verify) help = 0;

	for (i = 1;i < argc;i++)
	{
		/* ...however, I WANT the help! */
		if (!strcasecmp(argv[i],"-?") || !strcasecmp(argv[i],"-h") || !strcasecmp(argv[i],"-help"))
			help = 1;
	}

	if (help)  /* brief help - useful to get current version info */
	{
		#ifndef MESS
		printf("M.A.M.E. v%s - Multiple Arcade Machine Emulator\n"
				"Copyright (C) 1997-2013 by Nicola Salmoria and the MAME Team\n"
				 ,build_version);
		showdisclaimer();
		printf("Usage:  MAME gamename [options]\n\n"
				"        MAME -list      for a brief list of supported games\n"
				"        MAME -listfull  for a full list of supported games\n\n"
                "        Other lists:\n"
                "         -listinfo -listdetails -gamelistheader -gamelistfooter -gamelist\n"
                "         -listgames -listclones -listsamdir -listcrc -listdupcrc\n"
                "         -listromsize -listcpu -wrongorientation -wrongfps -noclones\n\n"
                "        Options\n"
                "         -listroms -listsamples -verifyroms -verifysets\n"
                "         -vset -verifysamples -vsam -romident -isknown -sourcefile\n\n"
                "        Pi Options\n"
				"         -nodrz80_snd Switch off DrZ80 core for sound (on by default)\n"
				"         -drz80       Switch on DrZ80 core (may cause instability)\n"
				"         -nocyclone   Switch off Cyclone core\n"
//				"         -ror -rol    Rotate display\n"
				"         -nosound     Switch off sound\n"
			  );
		#else
		showmessinfo();
		#endif
		return 0;
	}

	switch (list)  /* front-end utilities ;) */
	{

        #ifdef MESS
		case LIST_MESSINFO: /* all mess specific calls here */
		{
			for (i=1;i<argc;i++)
			{
				/* list all mess info options here */
				if (
					!strcasecmp(argv[i],"-listdevices") |
					!strcasecmp(argv[i],"-listtext")    |
					!strcasecmp(argv[i],"-createdir")
				   )
			 	{
					/* send the gamename and arg to mess.c */
					list_mess_info(gamename, argv[i], listclones);
				}
			}
			return 0;
			break;
		}
		#endif

		case LIST_LIST: /* simple games list */
			#ifndef MESS
			printf("\nMAME currently supports the following games:\n\n");
			#else
			printf("\nMESS currently supports the following systems:\n\n");
			#endif
			i = 0; j = 0;
			while (drivers[i])
			{
				if ((listclones || drivers[i]->clone_of == 0
						|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)
						) && !strwildcmp(gamename, drivers[i]->name))
				{
					printf("%-8s",drivers[i]->name);
					j++;
					if (!(j % 8)) printf("\n");
					else printf("  ");
				}
				i++;
			}
			if (j % 8) printf("\n");
			printf("\n");
			if (j != i) printf("Total ROM sets displayed: %4d - ", j);
			#ifndef MESS
			printf("Total ROM sets supported: %4d\n", i);
			#else
			printf("Total Systems supported: %4d\n", i);
			#endif
            return 0;
			break;

		case LIST_LISTFULL: /* games list with descriptions */
			printf("Name:     Description:\n");
			i = 0;
			while (drivers[i])
			{
				if ((listclones || drivers[i]->clone_of == 0
						|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)
						) && !strwildcmp(gamename, drivers[i]->name))
				{
					char name[200];

					printf("%-10s",drivers[i]->name);

					strcpy(name,drivers[i]->description);

					/* Move leading "The" to the end */
					if (strstr(name," (")) *strstr(name," (") = 0;
					if (strncmp(name,"The ",4) == 0)
					{
						printf("\"%s",name+4);
						printf(", The");
					}
					else
						printf("\"%s",name);

					/* print the additional description only if we are listing clones */
					if (listclones)
					{
						if (strchr(drivers[i]->description,'('))
							printf(" %s",strchr(drivers[i]->description,'('));
					}
					printf("\"\n");
				}
				i++;
			}
			return 0;
			break;

		case LIST_LISTSAMDIR: /* games list with samples directories */
			printf("Name:     Samples dir:\n");
			i = 0;
			while (drivers[i])
			{
				if ((listclones || drivers[i]->clone_of == 0
						|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)
						) && !strwildcmp(gamename, drivers[i]->name))
				{
#if (HAS_SAMPLES)
					for( j = 0; drivers[i]->drv->sound[j].sound_type && j < MAX_SOUND; j++ )
					{
						const char **samplenames;
						if( drivers[i]->drv->sound[j].sound_type != SOUND_SAMPLES )
							continue;
						samplenames = ((struct Samplesinterface *)drivers[i]->drv->sound[j].sound_interface)->samplenames;
						if (samplenames != 0 && samplenames[0] != 0)
						{
							printf("%-10s",drivers[i]->name);
							if (samplenames[0][0] == '*')
								printf("%s\n",samplenames[0]+1);
							else
								printf("%s\n",drivers[i]->name);
						}
					}
#endif
				}
				i++;
			}
			return 0;
			break;

		case LIST_LISTROMS: /* game roms list or */
		case LIST_LISTSAMPLES: /* game samples list */
			j = 0;
			while (drivers[j] && (strcasecmp(gamename,drivers[j]->name) != 0))
				j++;
			if (drivers[j] == 0)
			{
				printf("Game \"%s\" not supported!\n",gamename);
				return 1;
			}
			gamedrv = drivers[j];
			if (list == LIST_LISTROMS)
				printromlist(gamedrv->rom,gamename);
			else
			{
#if (HAS_SAMPLES)
				int k;
				for( k = 0; gamedrv->drv->sound[k].sound_type && k < MAX_SOUND; k++ )
				{
					const char **samplenames;
					if( gamedrv->drv->sound[k].sound_type != SOUND_SAMPLES )
						continue;
					samplenames = ((struct Samplesinterface *)gamedrv->drv->sound[k].sound_interface)->samplenames;
					if (samplenames != 0 && samplenames[0] != 0)
					{
						i = 0;
						while (samplenames[i] != 0)
						{
							printf("%s\n",samplenames[i]);
							i++;
						}
					}
                }
#endif
			}
			return 0;
			break;

		case LIST_LMR:
			{
				int total;

				total = 0;
				for (i = 0; drivers[i]; i++)
						total++;
				for (i = 0; drivers[i]; i++)
				{
					static int first_missing = 1;
					get_rom_sample_path (argc, argv, i);
					if (RomsetMissing (i))
					{
						if (first_missing)
						{
							first_missing = 0;
							printf ("game      clone of  description\n");
							printf ("--------  --------  -----------\n");
						}
						printf ("%-10s%-10s%s\n",
								drivers[i]->name,
								(drivers[i]->clone_of) ? drivers[i]->clone_of->name : "",
								drivers[i]->description);
					}
					fprintf(stderr,"%d%%\r",100 * (i+1) / total);
				}
			}
			return 0;
			break;

		case LIST_LISTDETAILS: /* A detailed MAMELIST.TXT type roms lister */

			/* First, we shall print the header */

			printf(" romname driver     ");
			for(j=0;j<MAX_CPU;j++) printf("cpu %d    ",j+1);
			for(j=0;j<MAX_SOUND;j++) printf("sound %d     ",j+1);
			printf("name\n");
			printf("-------- ---------- ");
			for(j=0;j<MAX_CPU;j++) printf("-------- ");
			for(j=0;j<MAX_SOUND;j++) printf("----------- ");
			printf("--------------------------\n");

			/* Let's cycle through the drivers */

			i = 0;

			while (drivers[i])
			{
				if ((listclones || drivers[i]->clone_of == 0
						|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)
						) && !strwildcmp(gamename, drivers[i]->name))
				{
					/* Dummy structs to fetch the information from */

					const struct MachineDriver *x_driver = drivers[i]->drv;
					const struct MachineCPU *x_cpu = x_driver->cpu;
					const struct MachineSound *x_sound = x_driver->sound;

					/* First, the rom name */

					printf("%-8s ",drivers[i]->name);

					#ifndef MESS
					/* source file (skip the leading "src/drivers/" */
					printf("%-10s ",&drivers[i]->source_file[12]);
					#else
					/* source file (skip the leading "src/mess/systems/" */
					printf("%-10s ",&drivers[i]->source_file[17]);
					#endif

					/* Then, cpus */

					for(j=0;j<MAX_CPU;j++)
					{
						if (x_cpu[j].cpu_type & CPU_AUDIO_CPU)
							printf("[%-6s] ",cputype_name(x_cpu[j].cpu_type));
						else
							printf("%-8s ",cputype_name(x_cpu[j].cpu_type));
					}

					/* Then, sound chips */

					for(j=0;j<MAX_SOUND;j++)
					{
						if (sound_num(&x_sound[j]))
						{
							printf("%dx",sound_num(&x_sound[j]));
							printf("%-9s ",sound_name(&x_sound[j]));
						}
						else
							printf("%-11s ",sound_name(&x_sound[j]));
					}

					/* Lastly, the name of the game and a \newline */

					printf("%s\n",drivers[i]->description);
				}
				i++;
			}
			return 0;
			break;

		case LIST_GAMELISTHEADER: /* GAMELIST.TXT */
			printf("This is the complete list of games supported by MAME %s\n",build_version);
			if (!listclones)
				printf("Variants of the same game are not included, you can use the -listclones command\n"
					"to get a list of the alternate versions of a given game.\n");
			printf("\n"
				"The list is generated automatically and is not 100%% accurate, particularly in\n"
				"the \"Screen Flip\" column. Please let us know of any errors you find so we can\n"
				"correct them.\n"
				"\n"
				"The meanings of the columns are as follows:\n"
				"Working - \"No\" means that the emulation has shortcomings that cause the game\n"
				"  not to work correctly. This can be anywhere from just showing a black screen\n"
				"  to being playable with major problems.\n"
				"Correct Colors - \"Yes\" means that colors should be identical to the original,\n"
				"  \"Close\" that they are very similar but wrong in places, \"No\" that they are\n"
				"  completely wrong. In some cases, we were not able to find the color PROMs of\n"
				"  the game. Those PROMs will be reported as \"NO GOOD DUMP KNOWN\" on startup,\n"
				"  and the game will have wrong colors. The game is still reported as \"Yes\" in\n"
				"  this column, because the code to handle the color PROMs is in the driver and\n"
				"  if you provide them colors will be correct.\n"
				"Sound - \"Partial\" means that sound support is either incomplete or not entirely\n"
				"  accurate. Note that, due to analog circuitry which is difficult to emulate,\n"
				"  sound may be significantly different from the real board. A common case is\n"
				"  the presence of low pass filters that make the real board sound less harsh\n"
				"  than the emulation.\n"
				"Screen Flip - A large number of games have a dip switch setting for \"Cocktail\"\n"
				"  cabinet, meaning that the players sit in front of each other, and the screen\n"
				"  is flipped when player 2 is playing. Some games also have a \"Flip Screen\" dip\n"
				"  switch. Those need special support in the driver, which is missing in many\n"
				"  cases.\n"
				"Internal Name - This is the unique name that should be specified on the command\n"
				"  line to run the game. ROMs must be placed in the ROM path, either in a .zip\n"
				"  file or in a subdirectory of the same name. The former is suggested, because\n"
				"  the files will be identified by their CRC instead of requiring specific\n"
				"  names.\n\n");
			printf("+----------------------------------+-------+-------+-------+-------+----------+\n");
			printf("|                                  |       |Correct|       |Screen | Internal |\n");
			printf("| Game Name                        |Working|Colors | Sound | Flip  |   Name   |\n");
			printf("+----------------------------------+-------+-------+-------+-------+----------+\n");
			return 0;
			break;

		case LIST_GAMELISTFOOTER: /* GAMELIST.TXT */
			printf("+----------------------------------+-------+-------+-------+-------+----------+\n\n");
			printf("(1) There are variants of the game (usually bootlegs) that work correctly\n");
#if (HAS_SAMPLES)
			printf("(2) Needs samples provided separately\n");
#endif
			return 0;
			break;

		case LIST_GAMELIST: /* GAMELIST.TXT */
			i = 0;

			while (drivers[i])
			{
				if ((listclones || drivers[i]->clone_of == 0
						|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)
						) && !strwildcmp(gamename, drivers[i]->name))
				{
					char name[200],name_ref[200];

					strcpy(name,drivers[i]->description);

					/* Move leading "The" to the end */
					if (strstr(name," (")) *strstr(name," (") = 0;
					if (strncmp(name,"The ",4) == 0)
					{
						sprintf(name_ref,"%s, The ",name+4);
					}
					else
						sprintf(name_ref,"%s ",name);

					/* print the additional description only if we are listing clones */
					if (listclones)
					{
						if (strchr(drivers[i]->description,'('))
							strcat(name_ref,strchr(drivers[i]->description,'('));
					}

					printf("| %-33.33s",name_ref);

					if (drivers[i]->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION))
					{
						const struct GameDriver *maindrv;
						int foundworking;

						if (drivers[i]->clone_of && !(drivers[i]->clone_of->flags & NOT_A_DRIVER))
							maindrv = drivers[i]->clone_of;
						else maindrv = drivers[i];

						foundworking = 0;
						j = 0;
						while (drivers[j])
						{
							if (drivers[j] == maindrv || drivers[j]->clone_of == maindrv)
							{
								if ((drivers[j]->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION)) == 0)
								{
									foundworking = 1;
									break;
								}
							}
							j++;
						}

						if (foundworking)
							printf("| No(1) ");
						else
							printf("|   No  ");
					}
					else
						printf("|  Yes  ");

					if (drivers[i]->flags & GAME_WRONG_COLORS)
						printf("|   No  ");
					else if (drivers[i]->flags & GAME_IMPERFECT_COLORS)
						printf("| Close ");
					else
						printf("|  Yes  ");

					{
						const char **samplenames = 0;
#if (HAS_SAMPLES)
						for (j = 0;drivers[i]->drv->sound[j].sound_type && j < MAX_SOUND; j++)
						{
							if (drivers[i]->drv->sound[j].sound_type == SOUND_SAMPLES)
							{
								samplenames = ((struct Samplesinterface *)drivers[i]->drv->sound[j].sound_interface)->samplenames;
								break;
							}
						}
#endif
						if (drivers[i]->flags & GAME_NO_SOUND)
							printf("|   No  ");
						else if (drivers[i]->flags & GAME_IMPERFECT_SOUND)
						{
							if (samplenames)
								printf("|Part(2)");
							else
								printf("|Partial");
						}
						else
						{
							if (samplenames)
								printf("| Yes(2)");
							else
								printf("|  Yes  ");
						}
					}

					if (drivers[i]->flags & GAME_NO_COCKTAIL)
						printf("|   No  ");
					else
						printf("|  Yes  ");

					printf("| %-8s |\n",drivers[i]->name);
				}
				i++;
			}
			return 0;
			break;

		case LIST_LISTGAMES: /* list games, production year, manufacturer */
			i = 0;
			while (drivers[i])
			{
				if ((listclones || drivers[i]->clone_of == 0
						|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)
						) && !strwildcmp(gamename, drivers[i]->description))
				{
					char name[200];

					printf("%-8.8s %-5.5s%-36.36s ",drivers[i]->name, drivers[i]->year,drivers[i]->manufacturer);

					strcpy(name,drivers[i]->description);

					/* Move leading "The" to the end */
					if (strstr(name," (")) *strstr(name," (") = 0;
					if (strncmp(name,"The ",4) == 0)
					{
						printf("%s",name+4);
						printf(", The");
					}
					else
						printf("%s",name);

					/* print the additional description only if we are listing clones */
					if (listclones)
					{
						if (strchr(drivers[i]->description,'('))
							printf(" %s",strchr(drivers[i]->description,'('));
					}
					printf("\n");
				}
				i++;
			}
			return 0;
			break;

//		case LIST_LISTCLONES: /* list clones */
//			printf("Name:    Clone of:\n");
//			i = 0;
//			while (drivers[i])
//			{
//				if (drivers[i]->clone_of && !(drivers[i]->clone_of->flags & NOT_A_DRIVER) &&
//						(!strwildcmp(gamename,drivers[i]->name)
//								|| !strwildcmp(gamename,drivers[i]->clone_of->name)))
//					printf("%-8s %-8s\n",drivers[i]->name,drivers[i]->clone_of->name);
//				i++;
//			}
//			return 0;
//			break;

//sq, special list for rominfo.fba
//		case LIST_LISTCLONES: 
//			i = 0;
//			while (drivers[i])
//			{
//				if (drivers[i]->clone_of && !(drivers[i]->clone_of->flags & NOT_A_DRIVER) &&
//						(!strwildcmp(gamename,drivers[i]->name)
//								|| !strwildcmp(gamename,drivers[i]->clone_of->name)))
//					printf("FILENAME( %-8s %-8s %-5s \"%.13s\" )\n",drivers[i]->name,drivers[i]->clone_of->name, drivers[i]->year, drivers[i]->manufacturer);
//				else
//					printf("FILENAME( %-8s mame %-5s \"%.13s\" )\n",drivers[i]->name,drivers[i]->year, drivers[i]->manufacturer);
//
//				i++;
//			}
//			return 0;
//			break;

//sq, special list for zipname.fba
		case LIST_LISTCLONES: 
			i = 0;
			while (drivers[i])
			{
				printf("%s,%s,%s %s\n",drivers[i]->name, drivers[i]->description, drivers[i]->year, drivers[i]->manufacturer);
				i++;
			}
			return 0;
			break;

		case LIST_WRONGORIENTATION: /* list drivers which incorrectly use the orientation and visible area fields */
			while (drivers[i])
			{
				if ((drivers[i]->drv->video_attributes & VIDEO_TYPE_VECTOR) == 0 &&
						(drivers[i]->clone_of == 0
								|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)) &&
						drivers[i]->drv->default_visible_area.max_x - drivers[i]->drv->default_visible_area.min_x + 1 <=
						drivers[i]->drv->default_visible_area.max_y - drivers[i]->drv->default_visible_area.min_y + 1)
				{
					if (strcmp(drivers[i]->name,"crater") &&
						strcmp(drivers[i]->name,"mpatrol") &&
						strcmp(drivers[i]->name,"troangel") &&
						strcmp(drivers[i]->name,"travrusa") &&
						strcmp(drivers[i]->name,"kungfum") &&
						strcmp(drivers[i]->name,"battroad") &&
						strcmp(drivers[i]->name,"vigilant") &&
						strcmp(drivers[i]->name,"sonson") &&
						strcmp(drivers[i]->name,"brkthru") &&
						strcmp(drivers[i]->name,"darwin") &&
						strcmp(drivers[i]->name,"exprraid") &&
						strcmp(drivers[i]->name,"sidetrac") &&
						strcmp(drivers[i]->name,"targ") &&
						strcmp(drivers[i]->name,"spectar") &&
						strcmp(drivers[i]->name,"venture") &&
						strcmp(drivers[i]->name,"mtrap") &&
						strcmp(drivers[i]->name,"pepper2") &&
						strcmp(drivers[i]->name,"hardhat") &&
						strcmp(drivers[i]->name,"fax") &&
						strcmp(drivers[i]->name,"circus") &&
						strcmp(drivers[i]->name,"robotbwl") &&
						strcmp(drivers[i]->name,"crash") &&
						strcmp(drivers[i]->name,"ripcord") &&
						strcmp(drivers[i]->name,"starfire") &&
						strcmp(drivers[i]->name,"fireone") &&
						strcmp(drivers[i]->name,"renegade") &&
						strcmp(drivers[i]->name,"battlane") &&
						strcmp(drivers[i]->name,"megatack") &&
						strcmp(drivers[i]->name,"killcom") &&
						strcmp(drivers[i]->name,"challeng") &&
						strcmp(drivers[i]->name,"kaos") &&
						strcmp(drivers[i]->name,"formatz") &&
						strcmp(drivers[i]->name,"bankp") &&
						strcmp(drivers[i]->name,"liberatr") &&
						strcmp(drivers[i]->name,"toki") &&
						strcmp(drivers[i]->name,"stactics") &&
						strcmp(drivers[i]->name,"sprint1") &&
						strcmp(drivers[i]->name,"sprint2") &&
						strcmp(drivers[i]->name,"nitedrvr") &&
						strcmp(drivers[i]->name,"punchout") &&
						strcmp(drivers[i]->name,"spnchout") &&
						strcmp(drivers[i]->name,"armwrest") &&
						strcmp(drivers[i]->name,"route16") &&
						strcmp(drivers[i]->name,"stratvox") &&
						strcmp(drivers[i]->name,"irobot") &&
						strcmp(drivers[i]->name,"leprechn") &&
						strcmp(drivers[i]->name,"starcrus") &&
						strcmp(drivers[i]->name,"astrof") &&
						strcmp(drivers[i]->name,"tomahawk") &&
						1)
						printf("%s %dx%d\n",drivers[i]->name,
								drivers[i]->drv->default_visible_area.max_x - drivers[i]->drv->default_visible_area.min_x + 1,
								drivers[i]->drv->default_visible_area.max_y - drivers[i]->drv->default_visible_area.min_y + 1);
				}
				i++;
			}
			return 0;
			break;

		case LIST_WRONGFPS: /* list drivers with too high frame rate */
			while (drivers[i])
			{
				if ((drivers[i]->drv->video_attributes & VIDEO_TYPE_VECTOR) == 0 &&
						(drivers[i]->clone_of == 0
								|| (drivers[i]->clone_of->flags & NOT_A_DRIVER)) &&
						drivers[i]->drv->frames_per_second > 57 &&
						drivers[i]->drv->default_visible_area.max_y - drivers[i]->drv->default_visible_area.min_y + 1 > 244 &&
						drivers[i]->drv->default_visible_area.max_y - drivers[i]->drv->default_visible_area.min_y + 1 <= 256)
				{
					printf("%s %dx%d %fHz\n",drivers[i]->name,
							drivers[i]->drv->default_visible_area.max_x - drivers[i]->drv->default_visible_area.min_x + 1,
							drivers[i]->drv->default_visible_area.max_y - drivers[i]->drv->default_visible_area.min_y + 1,
							drivers[i]->drv->frames_per_second);
				}
				i++;
			}
			return 0;
			break;

		case LIST_SOURCEFILE:
			i = 0;
			while (drivers[i])
			{
				if (!strwildcmp(gamename,drivers[i]->name))
					printf("%-8s %s\n",drivers[i]->name,drivers[i]->source_file);
				i++;
			}
			return 0;
			break;

		case LIST_LISTCRC: /* list all crc-32 */
			i = 0;
			while (drivers[i])
			{
				const struct RomModule *romp;

				romp = drivers[i]->rom;

				while (romp && (romp->name || romp->offset || romp->length))
				{
					if (romp->name && romp->name != (char *)-1)
						printf("%08x %-12s %s\n",romp->crc,romp->name,drivers[i]->description);

					romp++;
				}

				i++;
			}
			return 0;
			break;

		case LIST_LISTDUPCRC: /* list duplicate crc-32 (with different ROM name) */
			i = 0;
			while (drivers[i])
			{
				const struct RomModule *romp;

				romp = drivers[i]->rom;

				while (romp && (romp->name || romp->offset || romp->length))
				{
					if (romp->name && romp->name != (char *)-1 && romp->crc)
					{
						j = i+1;
						while (drivers[j])
						{
							const struct RomModule *romp1;

							romp1 = drivers[j]->rom;

							while (romp1 && (romp1->name || romp1->offset || romp1->length))
							{
								if (romp1->name && romp1->name != (char *)-1 &&
										strcmp(romp->name,romp1->name) &&
										romp1->crc == romp->crc)
								{
									printf("%08x %-12s %-8s <-> %-12s %-8s\n",romp->crc,
											romp->name,drivers[i]->name,
											romp1->name,drivers[j]->name);
								}

								romp1++;
							}

							j++;
						}
					}

					romp++;
				}

				i++;
			}
			return 0;
			break;


		case LIST_WRONGMERGE: /* list duplicate crc-32 with different ROM name in clone sets */
			i = 0;
			while (drivers[i])
			{
				const struct RomModule *romp;

				romp = drivers[i]->rom;

				while (romp && (romp->name || romp->offset || romp->length))
				{
					if (romp->name && romp->name != (char *)-1 && romp->crc)
					{
						j = 0;
						while (drivers[j])
						{
							if (j != i &&
								drivers[j]->clone_of &&
								(drivers[j]->clone_of->flags & NOT_A_DRIVER) == 0 &&
								(drivers[j]->clone_of == drivers[i] ||
								(i < j && drivers[j]->clone_of == drivers[i]->clone_of)))
							{
								const struct RomModule *romp1;
								int match;


								romp1 = drivers[j]->rom;
								match = 0;

								while (romp1 && (romp1->name || romp1->offset || romp1->length))
								{
									if (romp1->name && romp1->name != (char *)-1 &&
											!strcmp(romp->name,romp1->name))
									{
										match = 1;
										break;
									}

									romp1++;
								}

								if (match == 0)
								{
									romp1 = drivers[j]->rom;

									while (romp1 && (romp1->name || romp1->offset || romp1->length))
									{
										if (romp1->name && romp1->name != (char *)-1 &&
												strcmp(romp->name,romp1->name) &&
												romp1->crc == romp->crc)
										{
											printf("%08x %-12s %-8s <-> %-12s %-8s\n",romp->crc,
													romp->name,drivers[i]->name,
													romp1->name,drivers[j]->name);
										}

										romp1++;
									}
								}
							}
							j++;
						}
					}

					romp++;
				}

				i++;
			}
			return 0;
			break;

		case LIST_LISTROMSIZE: /* I used this for statistical analysis */
			i = 0;
			while (drivers[i])
			{
				if (drivers[i]->clone_of == 0 || (drivers[i]->clone_of->flags & NOT_A_DRIVER))
				{
					const struct RomModule *romp;
					j = 0;

					romp = drivers[i]->rom;

					while (romp && (romp->name || romp->offset || romp->length))
					{
						j += romp->length & ~ROMFLAG_MASK;

						romp++;
					}
					printf("%-8s\t%-5s\t%u\n",drivers[i]->name,drivers[i]->year,j);
				}

				i++;
			}
			return 0;
			break;

		case LIST_LISTCPU: /* I used this for statistical analysis */
			{
				int year;

				for (j = 1;j < CPU_COUNT;j++)
					printf("\t%s",cputype_name(j));
				printf("\n");

				for (year = 1980;year <= 1995;year++)
				{
					int count[CPU_COUNT];

					for (j = 0;j < CPU_COUNT;j++)
						count[j] = 0;

					i = 0;
					while (drivers[i])
					{
						if (drivers[i]->clone_of == 0 || (drivers[i]->clone_of->flags & NOT_A_DRIVER))
						{
							const struct MachineDriver *x_driver = drivers[i]->drv;
							const struct MachineCPU *x_cpu = x_driver->cpu;

							if (atoi(drivers[i]->year) == year)
							{
//								for (j = 0;j < MAX_CPU;j++)
j = 0;	// count only the main cpu
									count[x_cpu[j].cpu_type & ~CPU_FLAGS_MASK]++;
							}
						}

						i++;
					}

					printf("%d",year);
					for (j = 1;j < CPU_COUNT;j++)
						printf("\t%d",count[j]);
					printf("\n");
				}
			}

			return 0;
			break;

		case LIST_LISTINFO: /* list all info */
			print_mame_info( stdout, drivers );
			return 0;
	}

	if (verify)  /* "verify" utilities */
	{
		int err = 0;
		int correct = 0;
		int incorrect = 0;
		int res = 0;
		int total = 0;
		int checked = 0;
		int notfound = 0;


		for (i = 0; drivers[i]; i++)
		{
			if (!strwildcmp(gamename, drivers[i]->name))
				total++;
		}

		for (i = 0; drivers[i]; i++)
		{
			if (strwildcmp(gamename, drivers[i]->name))
				continue;

			/* set rom and sample path correctly */
			get_rom_sample_path (argc, argv, i);

			if (verify & VERIFY_ROMS)
			{
				res = VerifyRomSet (i,(verify & VERIFY_TERSE) ? terse_printf : (verify_printf_proc)printf);

				if (res == CLONE_NOTFOUND || res == NOTFOUND)
				{
					notfound++;
					goto nextloop;
				}

				if (res == INCORRECT || res == BEST_AVAILABLE || (verify & VERIFY_VERBOSE))
				{
					printf ("romset %s ", drivers[i]->name);
					if (drivers[i]->clone_of && !(drivers[i]->clone_of->flags & NOT_A_DRIVER))
						printf ("[%s] ", drivers[i]->clone_of->name);
				}
			}
			if (verify & VERIFY_SAMPLES)
			{
				const char **samplenames = NULL;
#if (HAS_SAMPLES)
				for( j = 0; drivers[i]->drv->sound[j].sound_type && j < MAX_SOUND; j++ )
					if( drivers[i]->drv->sound[j].sound_type == SOUND_SAMPLES )
						samplenames = ((struct Samplesinterface *)drivers[i]->drv->sound[j].sound_interface)->samplenames;
#endif
				/* ignore games that need no samples */
				if (samplenames == 0 || samplenames[0] == 0)
					goto nextloop;

				res = VerifySampleSet (i,(verify_printf_proc)printf);
				if (res == NOTFOUND)
				{
					notfound++;
					goto nextloop;
				}
				printf ("sampleset %s ", drivers[i]->name);
			}

			if (res == NOTFOUND)
			{
				printf ("oops, should never come along here\n");
			}
			else if (res == INCORRECT)
			{
				printf ("is bad\n");
				incorrect++;
			}
			else if (res == CORRECT)
			{
				if (verify & VERIFY_VERBOSE)
					printf ("is good\n");
				correct++;
			}
			else if (res == BEST_AVAILABLE)
			{
				printf ("is best available\n");
				correct++;
			}
			if (res)
				err = res;

nextloop:
			checked++;
			fprintf(stderr,"%d%%\r",100 * checked / total);
		}

		if (correct+incorrect == 0)
		{
			printf ("%s ", (verify & VERIFY_ROMS) ? "romset" : "sampleset" );
			if (notfound > 0)
				printf("\"%8s\" not found!\n",gamename);
			else
				printf("\"%8s\" not supported!\n",gamename);
			return 1;
		}
		else
		{
			printf("%d %s found, %d were OK.\n", correct+incorrect,
					(verify & VERIFY_ROMS)? "romsets" : "samplesets", correct);
			if (incorrect > 0)
				return 2;
			else
				return 0;
		}
	}
	if (ident)
	{
		if (ident == 2) silentident = 1;
		else silentident = 0;

		for (i = 1;i < argc;i++)
		{
			/* find the FIRST "name" field (without '-') */
			if (argv[i][0] != '-')
			{
				knownstatus = KNOWN_START;
				romident(argv[i],1);
				if (ident == 2)
				{
					switch (knownstatus)
					{
						case KNOWN_START: printf("ERROR     %s\n",argv[i]); break;
						case KNOWN_ALL:   printf("KNOWN     %s\n",argv[i]); break;
						case KNOWN_NONE:  printf("UNKNOWN   %s\n",argv[i]); break;
						case KNOWN_SOME:  printf("PARTKNOWN %s\n",argv[i]); break;
					}
				}
				break;
			}
		}
		return 0;
	}

	/* use a special return value if no frontend function used */

	return 1234;
}
