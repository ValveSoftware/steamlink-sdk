#include "driver.h"
#include "unzip.h"
#include "zlib.h"
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

/* Verbose outputs to error.log ? */
#define VERBOSE 	0

/* Use the file cache ? */
#define FILE_CACHE	1

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)	/* x */
#endif

char *roms = NULL;
char **rompathv = NULL;
int rompathc = 0;

char *samples = NULL;
char **samplepathv = NULL;
int samplepathc = 0;

char *cfgdir, *nvdir, *hidir, *inpdir, *stadir;
char *memcarddir, *artworkdir, *screenshotdir;
char *cheatdir;	/* Steph */

char *alternate_name;				   /* for "-romdir" */

typedef enum
{
	kPlainFile,
	kRAMFile,
	kZippedFile
}	eFileType;

typedef struct
{
	FILE *file;
	unsigned char *data;
	unsigned int offset;
	unsigned int length;
	eFileType type;
	unsigned int crc;
}	FakeFileHandle;

//extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
static int checksum_file (const char *file, unsigned char **p, unsigned int *size, unsigned int *crc);

/*
 * File stat cache LRU (Last Recently Used)
 */

#if FILE_CACHE
struct file_cache_entry
{
	struct stat stat_buffer;
	int result;
	char *file;
};

/* File cache buffer */
static struct file_cache_entry **file_cache_map = 0;

/* File cache size */
static unsigned int file_cache_max = 0;

/* AM 980919 */
static int cache_stat (const char *path, struct stat *statbuf)
{
	if( file_cache_max )
	{
		unsigned i;
		struct file_cache_entry *entry;

		/* search in the cache */
		for( i = 0; i < file_cache_max; ++i )
		{
			if( file_cache_map[i]->file && strcmp (file_cache_map[i]->file, path) == 0 )
			{	/* found */
				unsigned j;

//				LOG(("File cache HIT  for %s\n", path));
                /* store */
				entry = file_cache_map[i];

				/* shift */
				for( j = i; j > 0; --j )
					file_cache_map[j] = file_cache_map[j - 1];

				/* set the first entry */
				file_cache_map[0] = entry;

				if( entry->result == 0 )
					memcpy (statbuf, &entry->stat_buffer, sizeof (struct stat));

				return entry->result;
			}
		}
//		LOG(("File cache FAIL for %s\n", path));

		/* oldest entry */
		entry = file_cache_map[file_cache_max - 1];
		free(entry->file);

		/* shift */
		for( i = file_cache_max - 1; i > 0; --i )
			file_cache_map[i] = file_cache_map[i - 1];

		/* set the first entry */
		file_cache_map[0] = entry;

		/* file */
		entry->file = (char *) malloc(strlen (path) + 1);
		strcpy (entry->file, path);

		/* result and stat */
		entry->result = stat (path, &entry->stat_buffer);

		if( entry->result == 0 )
			memcpy (statbuf, &entry->stat_buffer, sizeof (struct stat));

		return entry->result;
	}
	else
	{
		return stat (path, statbuf);
	}
}

/* AM 980919 */
static void cache_allocate (unsigned entries)
{
	if( entries )
	{
		unsigned i;

		file_cache_max = entries;
		file_cache_map = (struct file_cache_entry **) malloc(file_cache_max * sizeof (struct file_cache_entry *));

		for( i = 0; i < file_cache_max; ++i )
		{
			file_cache_map[i] = (struct file_cache_entry *) malloc(sizeof (struct file_cache_entry));
			memset (file_cache_map[i], 0, sizeof (struct file_cache_entry));
		}
		LOG(("File cache allocated for %d entries\n", file_cache_max));
	}
}
#else

#define cache_stat(a,b) stat(a,b)

#endif

/* This function can be called several times with different parameters,
 * for example by "mame -verifyroms *". */
void decompose_rom_sample_path (char *rompath, char *samplepath)
{
	char *token;

	/* start with zero path components */
	rompathc = samplepathc = 0;

	if (!roms)
		roms = (char*)malloc( strlen(rompath) + 1);
	else
		roms = (char*)realloc( roms, strlen(rompath) + 1);

	if (!samples)
		samples = (char*)malloc( strlen(samplepath) + 1);
	else
		samples = (char*)realloc( samples, strlen(samplepath) + 1);

	if( !roms || !samples )
	{
		logerror("decompose_rom_sample_path: failed to malloc!\n");
		raise(SIGABRT);
	}

	strcpy (roms, rompath);
	token = strtok (roms, ";");
	while( token )
	{
		if( rompathc )
			rompathv = (char**)realloc(rompathv, (rompathc + 1) * sizeof(char *));
		else
			rompathv = (char**)malloc(sizeof(char *));
		if( !rompathv )
			break;
		rompathv[rompathc++] = token;
		token = strtok (NULL, ";");
	}

	strcpy (samples, samplepath);
	token = strtok (samples, ";");
	while( token )
	{
		if( samplepathc )
			samplepathv = (char**)realloc(samplepathv, (samplepathc + 1) * sizeof(char *));
		else
			samplepathv = (char**)malloc(sizeof(char *));
		if( !samplepathv )
			break;
		samplepathv[samplepathc++] = token;
		token = strtok (NULL, ";");
	}

#if FILE_CACHE
    /* AM 980919 */
    if( file_cache_max == 0 )
    {
        /* (rom path directories + 1 buffer)==rompathc+1 */
        /* (dir + .zip + .zif)==3 */
        /* (clone+parent)==2 */
        cache_allocate ((rompathc + 1) * 3 * 2);
    }
#endif

}

/*
 * file handling routines
 *
 * gamename holds the driver name, filename is only used for ROMs and samples.
 * if 'write' is not 0, the file is opened for write. Otherwise it is opened
 * for read.
 */

/*
 * check if roms/samples for a game exist at all
 * return index+1 of the path vector component on success, otherwise 0
 */
int osd_faccess (const char *newfilename, int filetype)
{
	static int indx;
	static const char *filename;
    char name[256];
    char **pathv;
    int pathc;
	char *dir_name;

	/* if filename == NULL, continue the search */
	if( newfilename != NULL )
	{
		indx = 0;
		filename = newfilename;
	}
	else
		indx++;

#ifdef MESS
	if( filetype == OSD_FILETYPE_ROM ||
		filetype == OSD_FILETYPE_IMAGE_R ||
		filetype == OSD_FILETYPE_IMAGE_RW )
#else
	if( filetype == OSD_FILETYPE_ROM )
#endif
	{
		pathv = rompathv;
		pathc = rompathc;
	}
	else
	if( filetype == OSD_FILETYPE_SAMPLE )
	{
		pathv = samplepathv;
		pathc = samplepathc;
	}
	else
	if( filetype == OSD_FILETYPE_SCREENSHOT )
	{
		void *f;

		sprintf (name, "%s/%s.png", screenshotdir, newfilename);
		f = fopen (name, "rb");
		if( f )
		{
			fclose ((FILE*)f);
			return 1;
		}
		else
			return 0;
	}
	else
		return 0;

	for( ; indx < pathc; indx++ )
	{
		struct stat stat_buffer;

		dir_name = pathv[indx];

		/* does such a directory (or file) exist? */
		sprintf (name, "%s/%s", dir_name, filename);
		if( cache_stat (name, &stat_buffer) == 0 )
			return indx + 1;

		/* try again with a .zip extension */
		sprintf (name, "%s/%s.zip", dir_name, filename);
		if( cache_stat (name, &stat_buffer) == 0 )
			return indx + 1;

		/* try again with a .zif extension */
		sprintf (name, "%s/%s.zif", dir_name, filename);
		if( cache_stat (name, &stat_buffer) == 0 )
			return indx + 1;
	}

	/* no match */
	return 0;
}

/* JB 980920 update */
/* AM 980919 update */
void *osd_fopen (const char *game, const char *filename, int filetype, int _write)
{
	char name[256];
	char *gamename;
	int found = 0;
	int indx;
	struct stat stat_buffer;
	FakeFileHandle *f;
	int pathc;
	char **pathv;


	f = (FakeFileHandle *) malloc(sizeof (FakeFileHandle));
	if( !f )
	{
		logerror("osd_fopen: failed to mallocFakeFileHandle!\n");
        return 0;
	}
	memset (f, 0, sizeof (FakeFileHandle));

	gamename = (char *) game;

	/* Support "-romdir" yuck. */
	if( alternate_name )
	{
		LOG(("osd_fopen: -romdir overrides '%s' by '%s'\n", gamename, alternate_name));
        gamename = alternate_name;
	}

	switch( filetype )
	{
	case OSD_FILETYPE_ROM:
	case OSD_FILETYPE_SAMPLE:

		/* only for reading */
		if( _write )
		{
			logerror("osd_fopen: type %02x write not supported\n",filetype);
            break;
		}

		if( filetype == OSD_FILETYPE_SAMPLE )
		{
			LOG(("osd_fopen: using samplepath\n"));
            pathc = samplepathc;
            pathv = samplepathv;
        }
		else
		{
			LOG(("osd_fopen: using rompath\n"));
            pathc = rompathc;
            pathv = rompathv;
		}

		for( indx = 0; indx < pathc && !found; ++indx )
		{
			const char *dir_name = pathv[indx];

			if( !found )
			{
				sprintf (name, "%s/%s", dir_name, gamename);
				LOG(("Trying %s\n", name));
                if( cache_stat (name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
				{
					sprintf (name, "%s/%s/%s", dir_name, gamename, filename);
					if( filetype == OSD_FILETYPE_ROM )
					{
						if( checksum_file (name, &f->data, &f->length, &f->crc) == 0 )
						{
							f->type = kRAMFile;
							f->offset = 0;
							found = 1;
						}
					}
					else
					{
						f->type = kPlainFile;
						f->file = fopen (name, "rb");
						found = f->file != 0;
					}
				}
			}

			if( !found )
			{
				/* try with a .zip extension */
				sprintf (name, "%s/%s.zip", dir_name, gamename);
				LOG(("Trying %s file\n", name));
                if( cache_stat (name, &stat_buffer) == 0 )
				{
					if( load_zipped_file (name, filename, &f->data, &f->length) == 0 )
					{
						LOG(("Using (osd_fopen) zip file for %s\n", filename));
						f->type = kZippedFile;
						f->offset = 0;
						f->crc = crc32 (0L, f->data, f->length);
						found = 1;
					}
				}
			}

			if( !found )
			{
				/* try with a .zip directory (if ZipMagic is installed) */
				sprintf (name, "%s/%s.zip", dir_name, gamename);
				LOG(("Trying %s directory\n", name));
                if( cache_stat (name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
				{
					sprintf (name, "%s/%s.zip/%s", dir_name, gamename, filename);
					if( filetype == OSD_FILETYPE_ROM )
					{
						if( checksum_file (name, &f->data, &f->length, &f->crc) == 0 )
						{
							f->type = kRAMFile;
							f->offset = 0;
							found = 1;
						}
					}
					else
					{
						f->type = kPlainFile;
						f->file = fopen (name, "rb");
						found = f->file != 0;
					}
				}
			}
		}

		break;

#ifdef MESS
	case OSD_FILETYPE_IMAGE_R:

		/* only for reading */
		if( _write )
		{
			logerror("osd_fopen: type %02x write not supported\n",filetype);
            break;
		}
        else
		{
			LOG(("osd_fopen: using rompath\n"));
            pathc = rompathc;
            pathv = rompathv;
		}

		LOG(("Open IMAGE_R '%s' for %s\n", filename, game));
        for( indx = 0; indx < pathc && !found; ++indx )
		{
			const char *dir_name = pathv[indx];

			/* this section allows exact path from .cfg */
			if( !found )
			{
				sprintf(name,"%s",dir_name);
				if( cache_stat(name,&stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
				{
					sprintf(name,"%s/%s",dir_name,filename);
					if( filetype == OSD_FILETYPE_ROM )
					{
						if( checksum_file (name, &f->data, &f->length, &f->crc) == 0 )
						{
							f->type = kRAMFile;
							f->offset = 0;
							found = 1;
						}
					}
					else
					{
						f->type = kPlainFile;
						f->file = fopen(name,"rb");
						found = f->file!=0;
					}
				}
			}

			if( !found )
			{
				sprintf (name, "%s/%s", dir_name, gamename);
				LOG(("Trying %s directory\n", name));
                if( cache_stat (name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
				{
					sprintf (name, "%s/%s/%s", dir_name, gamename, filename);
					LOG(("Trying %s file\n", name));
                    if( filetype == OSD_FILETYPE_ROM )
					{
						if( checksum_file(name, &f->data, &f->length, &f->crc) == 0 )
						{
							f->type = kRAMFile;
							f->offset = 0;
							found = 1;
						}
					}
					else
					{
						f->type = kPlainFile;
						f->file = fopen (name, "rb");
						found = f->file != 0;
					}
				}
			}

			/* Zip cart support for MESS */
			if( !found && filetype == OSD_FILETYPE_IMAGE_R )
			{
				char *extension = strrchr (name, '.');    /* find extension */
				if( extension )
					strcpy (extension, ".zip");
				else
					strcat (name, ".zip");
				LOG(("Trying %s file\n", name));
				if( cache_stat(name, &stat_buffer) == 0 )
				{
					if( load_zipped_file(name, filename, &f->data, &f->length) == 0 )
					{
						LOG(("Using (osd_fopen) zip file for %s\n", filename));
						f->type = kZippedFile;
						f->offset = 0;
						f->crc = crc32 (0L, f->data, f->length);
						found = 1;
					}
				}
			}

			if( !found )
			{
				/* try with a .zip extension */
				sprintf (name, "%s/%s.zip", dir_name, gamename);
				LOG(("Trying %s file\n", name));
				if( cache_stat(name, &stat_buffer) == 0 )
				{
					if( load_zipped_file(name, filename, &f->data, &f->length) == 0 )
					{
						LOG(("Using (osd_fopen) zip file for %s\n", filename));
						f->type = kZippedFile;
						f->offset = 0;
						f->crc = crc32 (0L, f->data, f->length);
						found = 1;
					}
				}
			}
    	}
    break; /* end of IMAGE_R */

	case OSD_FILETYPE_IMAGE_RW:
		{
			static char *write_modes[] = {"rb","wb","r+b","r+b","w+b"};
            char file[256];
			char *extension;

			LOG(("Open IMAGE_RW '%s' for %s mode '%s'\n", filename, game, write_modes[_write]));
			strcpy (file, filename);

			do
            {
			/* 29-05-00 Lee Ward: Reversed the search order. */
            for (indx=rompathc-1; indx>=0; --indx)
			{
				const char *dir_name = rompathv[indx];

					/* Exact path support */

					/* 29-05-00 Lee Ward: Changed the search order to prevent new files
					   being created in the application root as default */

					if( !found )
					{
						sprintf (name, "%s/%s", dir_name, gamename);
						LOG(("Trying %s directory\n", name));
						if( cache_stat(name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
						{
							sprintf (name, "%s/%s/%s", dir_name, gamename, file);
							LOG(("Trying %s file\n", name));
                                                        f->file = fopen (name, write_modes[_write]);
							found = f->file != 0;
							if( !found && _write == 3 )
							{
								f->file = fopen(name, write_modes[4]);
								found = f->file != 0;
                                                         }
                                                 }
					}

					/* Steph - Zip disk images support for MESS */
					if( !found && !_write )
					{
						extension = strrchr (name, '.');    /* find extension */
						/* add .zip for zipfile */
						if( extension )
							strcpy(extension, ".zip");
						else
							strcat(extension, ".zip");
						LOG(("Trying %s file\n", name));
						if( cache_stat(name, &stat_buffer) == 0 )
						{
							if( load_zipped_file(name, filename, &f->data, &f->length) == 0 )
							{
								LOG(("Using (osd_fopen) zip file for %s\n", filename));
								f->type = kZippedFile;
								f->offset = 0;
								f->crc = crc32(0L, f->data, f->length);
								found = 1;
							}
						}
					}

					if (!found)
					{
						sprintf(name, "%s", dir_name);
						LOG(("Trying %s directory\n", name));
						if( cache_stat(name,&stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
						{
							sprintf(name,"%s/%s", dir_name, file);
							LOG(("Trying %s file\n", name));
                                                        f->file = fopen(name, write_modes[_write]);
							found = f->file != 0;
							if( !found && _write == 3 )
							{
								f->file = fopen(name, write_modes[4]);
								found = f->file != 0;
                                                         }
						}
					}

                    if( !found && !_write )
                    {
                        extension = strrchr (name, '.');    /* find extension */
                        /* add .zip for zipfile */
                        if( extension )
                            strcpy(extension, ".zip");
                        else
                            strcat(extension, ".zip");
						LOG(("Trying %s file\n", name));
						if( cache_stat(name, &stat_buffer) == 0 )
                        {
							if( load_zipped_file(name, filename, &f->data, &f->length) == 0 )
                            {
                                LOG(("Using (osd_fopen) zip file for %s\n", filename));
                                f->type = kZippedFile;
                                f->offset = 0;
								f->crc = crc32(0L, f->data, f->length);
                                found = 1;
                            }
                        }
                    }

					if( !found && !_write )
					{
						/* try with a .zip extension */
						sprintf (name, "%s/%s.zip", dir_name, gamename);
						LOG(("Trying %s file\n", name));
						if( cache_stat (name, &stat_buffer) == 0 )
						{
							if( load_zipped_file (name, file, &f->data, &f->length) == 0 )
							{
								LOG(("Using (osd_fopen) zip file for %s\n", filename));
								f->type = kZippedFile;
								f->offset = 0;
								f->crc = crc32 (0L, f->data, f->length);
								found = 1;
							}
						}
					}

					if( !found )
					{
						/* try with a .zip directory (if ZipMagic is installed) */
						sprintf (name, "%s/%s.zip", dir_name, gamename);
						LOG(("Trying %s ZipMagic directory\n", name));
						if( cache_stat (name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
						{
							sprintf (name, "%s/%s.zip/%s", dir_name, gamename, file);
							LOG(("Trying %s\n", name));
							f->file = fopen (name, write_modes[_write]);
							found = f->file != 0;
							if( !found && _write == 3 )
							{
								f->file = fopen(name, write_modes[4]);
								found = f->file != 0;
                            }
                        }
					}
					if( found )
						LOG(("IMAGE_RW %s FOUND in %s!\n", file, name));
				}

				extension = strrchr (file, '.');
				if( extension )
					*extension = '\0';
			} while( !found && extension );
		}
		break;
#endif	/* MESS */


	case OSD_FILETYPE_NVRAM:
		if( !found )
		{
			sprintf (name, "%s/%s.nv", nvdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}

		if( !found )
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf (name, "%s.zip/%s.nv", nvdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}

		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s.zif/%s.nv", nvdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}
		break;

	case OSD_FILETYPE_HIGHSCORE:
		if( mame_highscore_enabled () )
		{
			if( !found )
			{
				sprintf (name, "%s/%s.hi", hidir, gamename);
				f->type = kPlainFile;
				f->file = fopen (name, _write ? "wb" : "rb");
				found = f->file != 0;
			}

			if( !found )
			{
				/* try with a .zip directory (if ZipMagic is installed) */
				sprintf (name, "%s.zip/%s.hi", hidir, gamename);
				f->type = kPlainFile;
				f->file = fopen (name, _write ? "wb" : "rb");
				found = f->file != 0;
			}

			if( !found )
			{
				/* try with a .zif directory (if ZipFolders is installed) */
				sprintf (name, "%s.zif/%s.hi", hidir, gamename);
				f->type = kPlainFile;
				f->file = fopen (name, _write ? "wb" : "rb");
				found = f->file != 0;
			}
		}
		break;

    case OSD_FILETYPE_CONFIG:
		sprintf (name, "%s/%s.cfg", cfgdir, gamename);
		f->type = kPlainFile;
		f->file = fopen (name, _write ? "wb" : "rb");
		found = f->file != 0;

		if( !found )
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf (name, "%s.zip/%s.cfg", cfgdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}

		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s.zif/%s.cfg", cfgdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}
		break;

	case OSD_FILETYPE_INPUTLOG:
		sprintf (name, "%s/%s.inp", inpdir, gamename);
		f->type = kPlainFile;
		f->file = fopen (name, _write ? "wb" : "rb");
		found = f->file != 0;

        if( !found )
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf (name, "%s.zip/%s.cfg", inpdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}

		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s.zif/%s.cfg", inpdir, gamename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
        }

		if( !_write )
		{
			char file[256];
			sprintf (file, "%s.inp", gamename);
            sprintf (name, "%s/%s.zip", inpdir, gamename);
			LOG(("Trying %s in %s\n", file, name));
            if( cache_stat (name, &stat_buffer) == 0 )
			{
				if( load_zipped_file (name, file, &f->data, &f->length) == 0 )
				{
					LOG(("Using (osd_fopen) zip file %s for %s\n", name, file));
					f->type = kZippedFile;
					f->offset = 0;
					found = 1;
				}
			}
		}

        break;

	case OSD_FILETYPE_STATE:
		sprintf (name, "%s/%s.sta", stadir, gamename);
		f->file = fopen (name, _write ? "wb" : "rb");
		found = !(f->file == 0);
		if( !found )
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf (name, "%s.zip/%s.sta", stadir, gamename);
			f->file = fopen (name, _write ? "wb" : "rb");
			found = !(f->file == 0);
		}
		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s.zif/%s.sta", stadir, gamename);
			f->file = fopen (name, _write ? "wb" : "rb");
			found = !(f->file == 0);
		}
		break;

	case OSD_FILETYPE_ARTWORK:
		/* only for reading */
		if( _write )
		{
			logerror("osd_fopen: type %02x write not supported\n",filetype);
            break;
		}
		sprintf (name, "%s/%s", artworkdir, filename);
		f->type = kPlainFile;
		f->file = fopen (name, _write ? "wb" : "rb");
		found = f->file != 0;
		if( !found )
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf (name, "%s.zip/%s.png", artworkdir, filename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
		}

		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s.zif/%s.png", artworkdir, filename);
			f->type = kPlainFile;
			f->file = fopen (name, _write ? "wb" : "rb");
			found = f->file != 0;
        }

		if( !found )
		{
			char file[256], *extension;
			sprintf(file, "%s", filename);
            sprintf(name, "%s/%s", artworkdir, filename);
            extension = strrchr(name, '.');
			if( extension )
				strcpy (extension, ".zip");
			else
				strcat (name, ".zip");
			LOG(("Trying %s in %s\n", file, name));
            if( cache_stat (name, &stat_buffer) == 0 )
			{
				if( load_zipped_file (name, file, &f->data, &f->length) == 0 )
				{
					LOG(("Using (osd_fopen) zip file %s\n", name));
					f->type = kZippedFile;
					f->offset = 0;
					found = 1;
				}
			}
			if( !found )
			{
				sprintf(name, "%s/%s.zip", artworkdir, game);
				LOG(("Trying %s in %s\n", file, name));
				if( cache_stat (name, &stat_buffer) == 0 )
				{
					if( load_zipped_file (name, file, &f->data, &f->length) == 0 )
					{
						LOG(("Using (osd_fopen) zip file %s\n", name));
						f->type = kZippedFile;
						f->offset = 0;
						found = 1;
					}
				}
            }
        }
        break;

	case OSD_FILETYPE_MEMCARD:
		sprintf (name, "%s/%s", memcarddir, filename);
		f->type = kPlainFile;
		f->file = fopen (name, _write ? "wb" : "rb");
		found = f->file != 0;
		break;

	case OSD_FILETYPE_SCREENSHOT:
		/* only for writing */
		if( !_write )
		{
			logerror("osd_fopen: type %02x read not supported\n",filetype);
			break;
		}

		sprintf (name, "%s/%s.png", screenshotdir, filename);
		f->type = kPlainFile;
		f->file = fopen (name, _write ? "wb" : "rb");
		found = f->file != 0;
		break;

	case OSD_FILETYPE_HIGHSCORE_DB:
	case OSD_FILETYPE_HISTORY:
		/* only for reading */
		if( _write )
		{
			logerror("osd_fopen: type %02x write not supported\n",filetype);
            break;
		}
		f->type = kPlainFile;
		/* open as ASCII files, not binary like the others */
		f->file = fopen (filename, _write ? "w" : "r");
		found = f->file != 0;
        break;

	/* Steph */
	case OSD_FILETYPE_CHEAT:
		sprintf (name, "%s/%s", cheatdir, filename);
		f->type = kPlainFile;
		/* open as ASCII files, not binary like the others */
		f->file = fopen (filename, _write ? "a" : "r");
		found = f->file != 0;
        break;

	case OSD_FILETYPE_LANGUAGE:
		/* only for reading */
		if( _write )
		{
			logerror("osd_fopen: type %02x write not supported\n",filetype);
            break;
		}
		sprintf (name, "%s.lng", filename);
		f->type = kPlainFile;
		/* open as ASCII files, not binary like the others */
		f->file = fopen (name, _write ? "w" : "r");
		found = f->file != 0;
logerror("fopen %s = %08x\n",name,(int)f->file);
        break;

	default:
		logerror("osd_fopen(): unknown filetype %02x\n",filetype);
	}

	if( !found )
	{
		free(f);
		return 0;
	}

	return f;
}

/* JB 980920 update */
int osd_fread (void *file, void *buffer, int length)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch( f->type )
	{
	case kPlainFile:
		return fread (buffer, 1, length, f->file);
		break;
	case kZippedFile:
	case kRAMFile:
		/* reading from the RAM image of a file */
		if( f->data )
		{
			if( length + f->offset > f->length )
				length = f->length - f->offset;
			memcpy (buffer, f->offset + f->data, length);
			f->offset += length;
			return length;
		}
		break;
	}

	return 0;
}

int osd_fread_swap (void *file, void *buffer, int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	res = osd_fread (file, buffer, length);

	buf = (unsigned char*)buffer;
	for( i = 0; i < length; i += 2 )
	{
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}

	return res;
}


/* AM 980919 update */
int osd_fwrite (void *file, const void *buffer, int length)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch( f->type )
	{
	case kPlainFile:
		return fwrite (buffer, 1, length, ((FakeFileHandle *) file)->file);
	default:
		return 0;
	}
}

int osd_fwrite_swap (void *file, const void *buffer, int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	buf = (unsigned char *) buffer;
	for( i = 0; i < length; i += 2 )
	{
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}

	res = osd_fwrite (file, buffer, length);

	for( i = 0; i < length; i += 2 )
	{
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}

	return res;
}

int osd_fread_scatter (void *file, void *buffer, int length, int increment)
{
	unsigned char *buf = (unsigned char*)buffer;
	FakeFileHandle *f = (FakeFileHandle *) file;
	unsigned char tempbuf[4096];
	int totread, r, i;

	switch( f->type )
	{
	case kPlainFile:
		totread = 0;
		while (length)
		{
			r = length;
			if( r > 4096 )
				r = 4096;
			r = fread (tempbuf, 1, r, f->file);
			if( r == 0 )
				return totread;		   /* error */
			for( i = 0; i < r; i++ )
			{
				*buf = tempbuf[i];
				buf += increment;
			}
			totread += r;
			length -= r;
		}
		return totread;
		break;
	case kZippedFile:
	case kRAMFile:
		/* reading from the RAM image of a file */
		if( f->data )
		{
			if( length + f->offset > f->length )
				length = f->length - f->offset;
			for( i = 0; i < length; i++ )
			{
				*buf = f->data[f->offset + i];
				buf += increment;
			}
			f->offset += length;
			return length;
		}
		break;
	}

	return 0;
}


/* JB 980920 update */
int osd_fseek (void *file, int offset, int whence)
{
	FakeFileHandle *f = (FakeFileHandle *) file;
	int err = 0;

	switch( f->type )
	{
	case kPlainFile:
		return fseek (f->file, offset, whence);
		break;
	case kZippedFile:
	case kRAMFile:
		/* seeking within the RAM image of a file */
		switch( whence )
		{
		case SEEK_SET:
			f->offset = offset;
			break;
		case SEEK_CUR:
			f->offset += offset;
			break;
		case SEEK_END:
			f->offset = f->length + offset;
			break;
		}
		break;
	}

	return err;
}

/* JB 980920 update */
void osd_fclose (void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch( f->type )
	{
	case kPlainFile:
		fclose (f->file);
		#ifdef GP2X
			sync();
		#endif
		break;
	case kZippedFile:
	case kRAMFile:
		if( f->data )
			free(f->data);
		break;
	}
	free(f);
}

void osd_fsync(void)
{
	sync();
}

/* JB 980920 update */
/* AM 980919 */
static int checksum_file (const char *file, unsigned char **p, unsigned int *size, unsigned int *crc)
{
	int length;
	unsigned char *data;
	FILE *f;

	f = fopen (file, "rb");
	if( !f )
		return -1;

	/* determine length of file */
	if( fseek (f, 0L, SEEK_END) != 0 )
	{
		fclose (f);
		return -1;
	}

	length = ftell (f);
	if( length == -1L )
	{
		fclose (f);
		return -1;
	}

	/* allocate space for entire file */
	data = (unsigned char *) malloc(length);
	if( !data )
	{
		fclose (f);
		return -1;
	}

	/* read entire file into memory */
	if( fseek (f, 0L, SEEK_SET) != 0 )
	{
		free(data);
		fclose (f);
		return -1;
	}

	if( fread (data, sizeof (unsigned char), length, f) != length )
	{
		free(data);
		fclose (f);
		return -1;
	}

	*size = length;
	*crc = crc32 (0L, data, length);
	if( p )
		*p = data;
	else
		free(data);

	fclose (f);

	return 0;
}

/* JB 980920 updated */
/* AM 980919 updated */
int osd_fchecksum (const char *game, const char *filename, unsigned int *length, unsigned int *sum)
{
	char name[256];
	int indx;
	struct stat stat_buffer;
	int found = 0;
	const char *gamename = game;

	/* Support "-romdir" yuck. */
	if( alternate_name )
		gamename = alternate_name;

	for( indx = 0; indx < rompathc && !found; ++indx )
	{
		const char *dir_name = rompathv[indx];

		if( !found )
		{
			sprintf (name, "%s/%s", dir_name, gamename);
			if( cache_stat (name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
			{
				sprintf (name, "%s/%s/%s", dir_name, gamename, filename);
				if( checksum_file (name, 0, length, sum) == 0 )
                {
					found = 1;
                }
			}
		}

		if( !found )
		{
			/* try with a .zip extension */
			sprintf (name, "%s/%s.zip", dir_name, gamename);
			if( cache_stat (name, &stat_buffer) == 0 )
			{
				if( checksum_zipped_file (name, filename, length, sum) == 0 )
				{
					LOG(("Using (osd_fchecksum) zip file for %s\n", filename));
					found = 1;
				}
			}
		}

		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s/%s.zif", dir_name, gamename);
			if( cache_stat (name, &stat_buffer) == 0 )
			{
				sprintf (name, "%s/%s.zif/%s", dir_name, gamename, filename);
				if( checksum_file (name, 0, length, sum) == 0 )
				{
					found = 1;
				}
			}
		}
	}

	if( !found )
		return -1;

	return 0;
}

/* JB 980920 */
int osd_fsize (void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if( f->type == kRAMFile || f->type == kZippedFile )
		return f->length;

	if( f->file )
	{
		int size, offs;
		offs = ftell( f->file );
		fseek( f->file, 0, SEEK_END );
		size = ftell( f->file );
		fseek( f->file, offs, SEEK_SET );
		return size;
	}

    return 0;
}

/* JB 980920 */
unsigned int osd_fcrc (void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	return f->crc;
}

int osd_fgetc(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return fgetc(f->file);
	else
		return EOF;
}

int osd_ungetc(int c, void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return ungetc(c,f->file);
	else
		return EOF;
}

char *osd_fgets(char *s, int n, void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return fgets(s,n,f->file);
	else
		return NULL;
}

int osd_feof(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return feof(f->file);
	else
		return 1;
}

int osd_ftell(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return ftell(f->file);
	else
		return -1L;
}


/* called while loading ROMs. It is called a last time with name == 0 to signal */
/* that the ROM loading process is finished. */
/* return non-zero to abort loading */
int osd_display_loading_rom_message (const char *name, int current, int total)
{
//sq	if( name )
//sq		printf ("loading %-12s\n", name);
//sq	else
//sq		printf ("             \n");
//sq	fflush (stdout);
//sq
//sq	if( keyboard_pressed (KEYCODE_LCONTROL) && keyboard_pressed (KEYCODE_C) )
//sq		return 1;
//sq
	return 0;
}
