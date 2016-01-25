#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unzip.h"
#include "osdepend.h"	/* for CLIB_DECL */
#include <stdarg.h>
#ifdef macintosh	/* JB 981117 */
#	include "mac_dos.h"
#	include "stat.h"
#else
#ifndef WIN32
#   include <dirent.h>
#   include <sys/errno.h>
#else
#    include "dirent.h"
#endif
#include <sys/stat.h>
#endif


#define MAX_FILES 100

#ifdef macintosh	/* JB 981117 */

static int errno = 0;
#define MAX_FILENAME_LEN 31

#else
#define MAX_FILENAME_LEN 12	/* increase this if you are using a real OS... */
#endif


/* for unzip.c */
void CLIB_DECL logerror(const char *text,...)
{
}


/* compare modes when one file is twice as long as the other */
/* A = All file */
/* 1 = 1st half */
/* 2 = 2nd half */
/* E = Even bytes */
/* O = Odd butes */
/* E1 = Even bytes 1st half */
/* O1 = Odd butes 1st half */
/* E2 = Even bytes 2nd half */
/* O2 = Odd butes 2nd half */
enum {	MODE_A_A, MODE_12_A, MODE_22_A, MODE_E_A, MODE_O_A,
		          MODE_14_A, MODE_24_A, MODE_34_A, MODE_44_A,
		          MODE_E1_A, MODE_O1_A, MODE_E2_A, MODE_O2_A,
		          MODE_A_12, MODE_A_22, MODE_A_E, MODE_A_O,
		          MODE_A_14, MODE_A_24, MODE_A_34, MODE_A_44,
		          MODE_A_E1, MODE_A_O1, MODE_A_E2, MODE_A_O2,
		TOTAL_MODES };
char *modenames[][2] =
{
	{ "          ", "          " },
	{ "[1/2]     ", "          " },
	{ "[2/2]     ", "          " },
	{ "[even]    ", "          " },
	{ "[odd]     ", "          " },
	{ "[1/4]     ", "          " },
	{ "[2/4]     ", "          " },
	{ "[3/4]     ", "          " },
	{ "[4/4]     ", "          " },
	{ "[even 1/2]", "          " },
	{ "[odd 1/2] ", "          " },
	{ "[even 2/2]", "          " },
	{ "[odd 2/2] ", "          " },
	{ "          ", "[1/2]     " },
	{ "          ", "[2/2]     " },
	{ "          ", "[even]    " },
	{ "          ", "[odd]     " },
	{ "          ", "[1/4]     " },
	{ "          ", "[2/4]     " },
	{ "          ", "[3/4]     " },
	{ "          ", "[4/4]     " },
	{ "          ", "[even 1/2]" },
	{ "          ", "[odd 1/2] " },
	{ "          ", "[even 2/2]" },
	{ "          ", "[odd 2/2] " }
};

struct fileinfo
{
	char name[MAX_FILENAME_LEN+1];
	int size;
	unsigned char *buf;	/* file is read in here */
	int listed;
};

struct fileinfo files[2][MAX_FILES];
float matchscore[MAX_FILES][MAX_FILES][TOTAL_MODES];


static void checkintegrity(const struct fileinfo *file,int side)
{
	int i;
	int mask0,mask1;
	int addrbit;

	if (file->buf == 0) return;

	/* check for bad data lines */
	mask0 = 0x0000;
	mask1 = 0xffff;

	for (i = 0;i < file->size;i+=2)
	{
		mask0 |= ((file->buf[i] << 8) | file->buf[i+1]);
		mask1 &= ((file->buf[i] << 8) | file->buf[i+1]);
		if (mask0 == 0xffff && mask1 == 0x0000) break;
	}

	if (mask0 != 0xffff || mask1 != 0x0000)
	{
		int fixedmask;
		int bits;


		fixedmask = (~mask0 | mask1) & 0xffff;

		if (((mask0 >> 8) & 0xff) == (mask0 & 0xff) && ((mask1 >> 8) & 0xff) == (mask1 & 0xff))
			bits = 8;
		else bits = 16;

		printf("%-23s %-23s FIXED BITS (",side ? "" : file->name,side ? file->name : "");
		for (i = 0;i < bits;i++)
		{
			if (~mask0 & 0x8000) printf("0");
			else if (mask1 & 0x8000) printf("1");
			else printf("x");

			mask0 <<= 1;
			mask1 <<= 1;
		}
		printf(")\n");

		/* if the file contains a fixed value, we don't need to do the other */
		/* validity checks */
		if (fixedmask == 0xffff || fixedmask == 0x00ff || fixedmask == 0xff00)
			return;
	}


	addrbit = 1;
	mask0 = 0;
	while (addrbit <= file->size/2)
	{
		for (i = 0;i < file->size;i++)
		{
			if (file->buf[i] != file->buf[i ^ addrbit]) break;
		}

		if (i == file->size)
			mask0 |= addrbit;

		addrbit <<= 1;
	}

	if (mask0)
	{
		if (mask0 == file->size/2)
			printf("%-23s %-23s FIRST AND SECOND HALF IDENTICAL\n",side ? "" : file->name,side ? file->name : "");
		else
			printf("%-23s %-23s BAD ADDRESS LINES (mask=%06x)\n",side ? "" : file->name,side ? file->name : "",mask0);
		return;
	}

	mask0 = 0x000000;
	mask1 = file->size-1;
	for (i = 0;i < file->size;i++)
	{
		if (file->buf[i] != 0xff)
		{
			mask0 |= i;
			mask1 &= i;
			if (mask0 == file->size-1 && mask1 == 0x00) break;
		}
	}

	if (mask0 != file->size-1 || mask1 != 0x00)
	{
		printf("%-23s %-23s ",side ? "" : file->name,side ? file->name : "");
		for (i = 0;i < 24;i++)
		{
			if (file->size <= (1<<(23-i))) printf(" ");
			else if (~mask0 & 0x800000) printf("1");
			else if (mask1 & 0x800000) printf("0");
			else printf("x");
			mask0 <<= 1;
			mask1 <<= 1;
		}
		printf(" = 0xFF\n");

		return;
	}


	mask0 = 0x000000;
	mask1 = file->size-1;
	for (i = 0;i < file->size;i++)
	{
		if (file->buf[i] != 0x00)
		{
			mask0 |= i;
			mask1 &= i;
			if (mask0 == file->size-1 && mask1 == 0x00) break;
		}
	}

	if (mask0 != file->size-1 || mask1 != 0x00)
	{
		printf("%-23s %-23s ",side ? "" : file->name,side ? file->name : "");
		for (i = 0;i < 24;i++)
		{
			if (file->size <= (1<<(23-i))) printf(" ");
			else if ((mask0 & 0x800000) == 0) printf("1");
			else if (mask1 & 0x800000) printf("0");
			else printf("x");
			mask0 <<= 1;
			mask1 <<= 1;
		}
		printf(" = 0x00\n");

		return;
	}


	for (i = 0;i < file->size/4;i++)
	{
		if (file->buf[file->size/2 + 2*i+1] != 0xff) break;
		if (file->buf[2*i+1] != file->buf[file->size/2 + 2*i]) break;
	}

	if (i == file->size/4)
		printf("%-23s %-23s BAD NEOGEO DUMP - CUT 2ND HALF\n",side ? "" : file->name,side ? file->name : "");
}


static float filecompare(const struct fileinfo *file1,const struct fileinfo *file2,int mode)
{
	int i;
	int match = 0;
	float score = 0.0;


	if (file1->buf == 0 || file2->buf == 0) return 0.0;

	if (mode >= MODE_A_12 && mode <= MODE_A_O2)
	{
		const struct fileinfo *temp;
		mode -= MODE_A_O2 - MODE_A_12 + 1;

		temp = file1;
		file1 = file2;
		file2 = temp;
	}

	if (mode == MODE_A_A)
	{
		if (file1->size != file2->size) return 0.0;
	}
	else if (mode >= MODE_12_A && mode <= MODE_O_A)
	{
		if (file1->size != 2*file2->size) return 0.0;
	}
	else if (mode >= MODE_14_A && mode <= MODE_O2_A)
	{
		if (file1->size != 4*file2->size) return 0.0;
	}

	switch (mode)
	{
		case MODE_A_A:
		case MODE_12_A:
		case MODE_14_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[i] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_22_A:
		case MODE_24_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[i + file2->size] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_34_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[i + 2*file2->size] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_44_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[i + 3*file2->size] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_E_A:
		case MODE_E1_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[2*i] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_O_A:
		case MODE_O1_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[2*i+1] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_E2_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[2*i + 2*file2->size] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;

		case MODE_O2_A:
			for (i = 0;i < file2->size;i++)
			{
				if (file1->buf[2*i+1 + 2*file2->size] == file2->buf[i]) match++;
			}
			score = (float)match/file2->size;
			break;
	}

	return score;
}


static void readfile(const char *path,struct fileinfo *file)
{
	char fullname[256];
	FILE *f = 0;


	if (path)
	{
		strcpy(fullname,path);
#ifdef macintosh	/* JB 981117 */
		strcat(fullname,":");
#else
		strcat(fullname,"/");
#endif
	}
	else fullname[0] = 0;
	strcat(fullname,file->name);

	if ((file->buf = malloc(file->size)) == 0)
	{
		printf("%s: out of memory!\n",file->name);
		return;
	}

	if ((f = fopen(fullname,"rb")) == 0)
	{
#ifdef macintosh	/* JB 981117 */
		errno = fnfErr;
#endif
		printf("%s: %s\n",fullname,strerror(errno));
		return;
	}

	if (fread(file->buf,1,file->size,f) != file->size)
	{
#ifdef macintosh	/* JB 981117 */
		errno = fnfErr;
#endif
		printf("%s: %s\n",fullname,strerror(errno));
		fclose(f);
		return;
	}

	fclose(f);

	return;
}


static void freefile(struct fileinfo *file)
{
	free(file->buf);
	file->buf = 0;
}


static void printname(const struct fileinfo *file1,const struct fileinfo *file2,float score,int mode)
{
	printf("%-12s %s %-12s %s ",file1 ? file1->name : "",modenames[mode][0],file2 ? file2->name : "",modenames[mode][1]);
	if (score == 0.0) printf("NO MATCH\n");
	else if (score == 1.0) printf("IDENTICAL\n");
	else printf("%3.3f%%\n",score*100);
}


static int load_files(int i, int *found, const char *path)
{
	struct stat st;


	if (stat(path,&st) != 0)
	{
#ifdef macintosh
		errno = fnfErr;
#endif
		printf("%s: %s\n",path,strerror(errno));
		return 10;
	}

	if (S_ISDIR(st.st_mode))
	{
		DIR *dir;
		struct dirent *d;

		/* load all files in directory */
		dir = opendir(path);
		if (dir)
		{
			while((d = readdir(dir)) != NULL)
			{
				char buf[255+1];
				struct stat st_file;

				sprintf(buf, "%s/%s", path, d->d_name);
				if(stat(buf, &st_file) == 0 && S_ISREG(st_file.st_mode))
				{
					unsigned size = st_file.st_size;
					while (size && (size & 1) == 0) size >>= 1;
					if (size & ~1)
						printf("%-23s %-23s ignored (not a ROM)\n",i ? "" : d->d_name,i ? d->d_name : "");
					else
					{
						strcpy(files[i][found[i]].name,d->d_name);
						files[i][found[i]].size = st_file.st_size;
						readfile(path,&files[i][found[i]]);
						files[i][found[i]].listed = 0;
						if (found[i] >= MAX_FILES)
						{
							printf("%s: max of %d files exceeded\n",path,MAX_FILES);
							break;
						}
						found[i]++;
					}
				}
			}
			closedir(dir);
		}
	}
	else
	{
		ZIP *zip;
		struct zipent* zipent;

		/* wasn't a directory, so try to open it as a zip file */
		if ((zip = openzip(path)) == 0)
		{
			printf("Error, cannot open zip file '%s' !\n", path);
			return 1;
		}

		/* load all files in zip file */
		while ((zipent = readzip(zip)) != 0)
		{
			int size;

			size = zipent->uncompressed_size;
			while (size && (size & 1) == 0) size >>= 1;
			if (zipent->uncompressed_size == 0 || (size & ~1))
				printf("%-23s %-23s ignored (not a ROM)\n",
					i ? "" : zipent->name, i ? zipent->name : "");
			else
			{
				struct fileinfo *file = &files[i][found[i]];
				const char *delim = strrchr(zipent->name,'/');

				if (delim)
					strcpy (file->name,delim+1);
				else
					strcpy(file->name,zipent->name);
				file->size = zipent->uncompressed_size;
				if ((file->buf = malloc(file->size)) == 0)
					printf("%s: out of memory!\n",file->name);
				else
				{
					if (readuncompresszip(zip, zipent, (char *)file->buf) != 0)
					{
						free(file->buf);
						file->buf = 0;
					}
				}

				file->listed = 0;
				if (found[i] >= MAX_FILES)
				{
					printf("%s: max of %d files exceeded\n",path,MAX_FILES);
					break;
				}
				found[i]++;
			}
		}
		closezip(zip);
	}
	return 0;
}


int CLIB_DECL main(int argc,char **argv)
{
	int	err;

	if (argc < 2)
	{
		printf("usage: romcmp [dir1 | zip1] [dir2 | zip2]\n");
		return 0;
	}

	{
		int found[2];
		int i,j,mode;
		int besti,bestj;


		found[0] = found[1] = 0;
		for (i = 0;i < 2;i++)
		{
			if (argc > i+1)
			{
				err = load_files (i, found, argv[i+1]);
				if (err != 0)
					return err;
			}
		}

        if (argc >= 3)
			printf("%d and %d files\n",found[0],found[1]);
		else
			printf("%d files\n",found[0]);

		for (i = 0;i < found[0];i++)
		{
			checkintegrity(&files[0][i],0);
		}

		for (j = 0;j < found[1];j++)
		{
			checkintegrity(&files[1][j],1);
		}

		if (argc < 3)
		{
			/* find duplicates in one dir */
			for (i = 0;i < found[0];i++)
			{
				for (j = i+1;j < found[0];j++)
				{
					for (mode = 0;mode < TOTAL_MODES;mode++)
					{
						if (filecompare(&files[0][i],&files[0][j],mode) == 1.0)
							printname(&files[0][i],&files[0][j],1.0,mode);
					}
				}
			}
		}
		else
		{
			/* compare two dirs */
			for (i = 0;i < found[0];i++)
			{
				for (j = 0;j < found[1];j++)
				{
					for (mode = 0;mode < TOTAL_MODES;mode++)
					{
						matchscore[i][j][mode] = filecompare(&files[0][i],&files[1][j],mode);
					}
				}
			}

			do
			{
				float bestscore;
				int bestmode;

				besti = -1;
				bestj = -1;
				bestscore = 0.0;
				bestmode = -1;

				for (mode = 0;mode < TOTAL_MODES;mode++)
				{
					for (i = 0;i < found[0];i++)
					{
						for (j = 0;j < found[1];j++)
						{
							if (matchscore[i][j][mode] > bestscore)
							{
								bestscore = matchscore[i][j][mode];
								besti = i;
								bestj = j;
								bestmode = mode;
							}
						}
					}
				}

				if (besti != -1)
				{
					printname(&files[0][besti],&files[1][bestj],bestscore,bestmode);
					files[0][besti].listed = 1;
					files[1][bestj].listed = 1;

					matchscore[besti][bestj][bestmode] = 0.0;
					for (mode = 0;mode < TOTAL_MODES;mode++)
					{
						if (bestmode == MODE_A_A || mode == bestmode ||
								(bestmode >= MODE_12_A && bestmode <= MODE_O_A &&
								((mode-MODE_12_A)&~1) != ((bestmode-MODE_12_A)&~1)) ||
								(bestmode >= MODE_14_A && bestmode <= MODE_44_A &&
								((mode-MODE_14_A)&~3) != ((bestmode-MODE_14_A)&~3)) ||
								(bestmode >= MODE_E1_A && bestmode <= MODE_O2_A &&
								((mode-MODE_E1_A)&~3) != ((bestmode-MODE_E1_A)&~3)) ||
								(bestmode >= MODE_A_12 && bestmode <= MODE_A_O &&
								((mode-MODE_A_12)&~1) != ((bestmode-MODE_A_12)&~1)) ||
								(bestmode >= MODE_A_14 && bestmode <= MODE_A_44 &&
								((mode-MODE_A_14)&~3) != ((bestmode-MODE_A_14)&~3)) ||
								(bestmode >= MODE_A_E1 && bestmode <= MODE_A_O2 &&
								((mode-MODE_A_E1)&~3) != ((bestmode-MODE_A_E1)&~3)))
						{
							for (i = 0;i < found[0];i++)
							{
								if (matchscore[i][bestj][mode] < bestscore)
									matchscore[i][bestj][mode] = 0.0;
							}
							for (j = 0;j < found[1];j++)
							{
								if (matchscore[besti][j][mode] < bestscore)
									matchscore[besti][j][mode] = 0.0;
							}
						}
						if (files[0][besti].size > files[1][bestj].size)
						{
							for (i = 0;i < found[0];i++)
							{
								if (matchscore[i][bestj][mode] < bestscore)
									matchscore[i][bestj][mode] = 0.0;
							}
						}
						else
						{
							for (j = 0;j < found[1];j++)
							{
								if (matchscore[besti][j][mode] < bestscore)
									matchscore[besti][j][mode] = 0.0;
							}
						}
					}
				}
			} while (besti != -1);


			for (i = 0;i < found[0];i++)
			{
				if (files[0][i].listed == 0) printname(&files[0][i],0,0.0,0);
				freefile(&files[0][i]);
			}
			for (i = 0;i < found[1];i++)
			{
				if (files[1][i].listed == 0) printname(0,&files[1][i],0.0,0);
				freefile(&files[1][i]);
			}
		}
	}

	return 0;
}

