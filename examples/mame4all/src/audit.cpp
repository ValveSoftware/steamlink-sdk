#include "driver.h"
#include "strings.h"
#include "audit.h"


static tAuditRecord *gAudits = NULL;



/* returns 1 if rom is defined in this set */
int RomInSet (const struct GameDriver *gamedrv, unsigned int crc)
{
	const struct RomModule *romp = gamedrv->rom;

	if (!romp) return 0;

	while (romp->name || romp->offset || romp->length)
	{
		romp++;	/* skip ROM_REGION */

		while (romp->length)
		{
			if (romp->crc == crc) return 1;
			do
			{
				romp++;
				/* skip ROM_CONTINUEs and ROM_RELOADs */
			}
			while (romp->length && (romp->name == 0 || romp->name == (char *)-1));
		}
	}
	return 0;
}


/* returns nonzero if romset is missing */
int RomsetMissing (int game)
{
	const struct GameDriver *gamedrv = drivers[game];

	if (gamedrv->clone_of)
	{
		tAuditRecord	*aud;
		int				count;
		int 			i;
		int 			cloneRomsFound = 0;

		if ((count = AuditRomSet (game, &aud)) == 0)
			return 1;

		if (count == -1) return 0;

		/* count number of roms found that are unique to clone */
		for (i = 0; i < count; i++)
			if (aud[i].status != AUD_ROM_NOT_FOUND)
				if (!RomInSet (gamedrv->clone_of, aud[i].expchecksum))
					cloneRomsFound++;

		return !cloneRomsFound;
	}
	else
		return !osd_faccess (gamedrv->name, OSD_FILETYPE_ROM);
}


/* Fills in an audit record for each rom in the romset. Sets 'audit' to
   point to the list of audit records. Returns total number of roms
   in the romset (same as number of audit records), 0 if romset missing. */
int AuditRomSet (int game, tAuditRecord **audit)
{
	const struct RomModule *romp;
	const char *name;
	const struct GameDriver *gamedrv;

	int count = 0;
	tAuditRecord *aud;
	int	err;

	if (!gAudits)
		gAudits = (tAuditRecord *)malloc(AUD_MAX_ROMS * sizeof (tAuditRecord));

	if (gAudits)
		*audit = aud = gAudits;
	else
		return 0;


	gamedrv = drivers[game];
	romp = gamedrv->rom;

	if (!romp) return -1;

	/* check for existence of romset */
	if (!osd_faccess (gamedrv->name, OSD_FILETYPE_ROM))
	{
		/* if the game is a clone, check for parent */
		if (gamedrv->clone_of == 0 || (gamedrv->clone_of->flags & NOT_A_DRIVER) ||
				!osd_faccess(gamedrv->clone_of->name,OSD_FILETYPE_ROM))
			return 0;
	}

	while (romp->name || romp->offset || romp->length)
	{
		if (romp->name || romp->length) return 0; /* expecting ROM_REGION */

		romp++;

		while (romp->length)
		{
			const struct GameDriver *drv;


			if (romp->name == 0)
				return 0;	/* ROM_CONTINUE not preceded by ROM_LOAD */
			else if (romp->name == (char *)-1)
				return 0;	/* ROM_RELOAD not preceded by ROM_LOAD */

			name = romp->name;
			strcpy (aud->rom, name);
			aud->explength = 0;
			aud->length = 0;
			aud->expchecksum = romp->crc;
			/* NS981003: support for "load by CRC" */
			aud->checksum = romp->crc;
			count++;

			/* obtain CRC-32 and length of ROM file */
			drv = gamedrv;
			do
			{
				err = osd_fchecksum (drv->name, name, &aud->length, &aud->checksum);
				drv = drv->clone_of;
			} while (err && drv);

			/* spin through ROM_CONTINUEs and ROM_RELOADs, totaling length */
			do
			{
				if (romp->name != (char *)-1) /* ROM_RELOAD */
					aud->explength += romp->length & ~ROMFLAG_MASK;
				romp++;
			}
			while (romp->length && (romp->name == 0 || romp->name == (char *)-1));

			if (err)
			{
				if (!aud->expchecksum)
					/* not found but it's not good anyway */
					aud->status = AUD_NOT_AVAILABLE;
				else
					/* not found */
					aud->status = AUD_ROM_NOT_FOUND;
			}
			/* all cases below assume the ROM was at least found */
			else if (aud->explength != aud->length)
				aud->status = AUD_LENGTH_MISMATCH;
			else if (aud->checksum != aud->expchecksum)
			{
				if (!aud->expchecksum)
					aud->status = AUD_ROM_NEED_DUMP; /* new case - found but not known to be dumped */
				else if (aud->checksum == BADCRC (aud->expchecksum))
					aud->status = AUD_ROM_NEED_REDUMP;
				else
					aud->status = AUD_BAD_CHECKSUM;
			}
			else
				aud->status = AUD_ROM_GOOD;

			aud++;
		}
	}

        #ifdef MESS
        if (!count)
                return -1;
        else
        #endif
	return count;
}


/* Generic function for evaluating a romset. Some platforms may wish to
   call AuditRomSet() instead and implement their own reporting (like MacMAME). */
int VerifyRomSet (int game, verify_printf_proc verify_printf)
{
	tAuditRecord			*aud;
	int						count;
	int						archive_status = 0;
	const struct GameDriver *gamedrv = drivers[game];

	if ((count = AuditRomSet (game, &aud)) == 0)
		return NOTFOUND;

	if (count == -1) return CORRECT;

        if (gamedrv->clone_of)
	{
		int i;
		int cloneRomsFound = 0;

		/* count number of roms found that are unique to clone */
		for (i = 0; i < count; i++)
			if (aud[i].status != AUD_ROM_NOT_FOUND)
				if (!RomInSet (gamedrv->clone_of, aud[i].expchecksum))
					cloneRomsFound++;

                #ifndef MESS
                /* Different MESS systems can use the same ROMs */
		if (cloneRomsFound == 0)
			return CLONE_NOTFOUND;
                #endif
	}

	while (count--)
	{
		archive_status |= aud->status;

		switch (aud->status)
		{
			case AUD_ROM_NOT_FOUND:
				verify_printf ("%-8s: %-12s %7d bytes %08x NOT FOUND\n",
					drivers[game]->name, aud->rom, aud->explength, aud->expchecksum);
				break;
			case AUD_NOT_AVAILABLE:
				verify_printf ("%-8s: %-12s %7d bytes NOT FOUND - NO GOOD DUMP KNOWN\n",
					drivers[game]->name, aud->rom, aud->explength);
				break;
			case AUD_ROM_NEED_DUMP:
				verify_printf ("%-8s: %-12s %7d bytes NO GOOD DUMP KNOWN\n",
					drivers[game]->name, aud->rom, aud->explength);
				break;
			case AUD_BAD_CHECKSUM:
				verify_printf ("%-8s: %-12s %7d bytes %08x INCORRECT CHECKSUM: %08x\n",
					drivers[game]->name, aud->rom, aud->explength, aud->expchecksum,aud->checksum);
				break;
			case AUD_ROM_NEED_REDUMP:
				verify_printf ("%-8s: %-12s %7d bytes ROM NEEDS REDUMP\n",
					drivers[game]->name, aud->rom, aud->explength);
				break;
			case AUD_MEM_ERROR:
				verify_printf ("Out of memory reading ROM %s\n", aud->rom);
				break;
			case AUD_LENGTH_MISMATCH:
				verify_printf ("%-8s: %-12s %7d bytes %08x INCORRECT LENGTH: %8d\n",
					drivers[game]->name, aud->rom, aud->explength, aud->expchecksum,aud->length);
				break;
			case AUD_ROM_GOOD:
#if 0    /* if you want a full accounting of roms */
				verify_printf ("%-8s: %-12s %7d bytes %08x ROM GOOD\n",
					drivers[game]->name, aud->rom, aud->explength, aud->expchecksum);
#endif
				break;
		}
		aud++;
	}

	if (archive_status & (AUD_ROM_NOT_FOUND|AUD_BAD_CHECKSUM|AUD_MEM_ERROR|AUD_LENGTH_MISMATCH))
		return INCORRECT;
	if (archive_status & (AUD_ROM_NEED_DUMP|AUD_ROM_NEED_REDUMP|AUD_NOT_AVAILABLE))
		return BEST_AVAILABLE;

	return CORRECT;

}


static tMissingSample *gMissingSamples = NULL;

/* Builds a list of every missing sample. Returns total number of missing
   samples, or -1 if no samples were found. Sets audit to point to the
   list of missing samples. */
int AuditSampleSet (int game, tMissingSample **audit)
{
	int skipfirst;
	void *f;
	const char **samplenames, *sharedname;
	int exist;
	static const struct GameDriver *gamedrv;
	int j;
	int count = 0;
	tMissingSample *aud;

	gamedrv = drivers[game];
	samplenames = NULL;
#if (HAS_SAMPLES)
	for( j = 0; gamedrv->drv->sound[j].sound_type && j < MAX_SOUND; j++ )
	{
		if( gamedrv->drv->sound[j].sound_type == SOUND_SAMPLES )
			samplenames = ((struct Samplesinterface *)gamedrv->drv->sound[j].sound_interface)->samplenames;
	}
#endif
    /* does the game use samples at all? */
	if (samplenames == 0 || samplenames[0] == 0)
		return 0;

	/* take care of shared samples */
	if (samplenames[0][0] == '*')
	{
		sharedname=samplenames[0]+1;
		skipfirst = 1;
	}
	else
	{
		sharedname = NULL;
		skipfirst = 0;
	}

	/* do we have samples for this game? */
	exist = osd_faccess (gamedrv->name, OSD_FILETYPE_SAMPLE);

	/* try shared samples */
	if (!exist && skipfirst)
		exist = osd_faccess (sharedname, OSD_FILETYPE_SAMPLE);

	/* if still not found, we're done */
	if (!exist)
		return -1;

	/* allocate missing samples list (if necessary) */
	if (!gMissingSamples)
		gMissingSamples = (tMissingSample *)malloc(AUD_MAX_SAMPLES * sizeof (tMissingSample));

	if (gMissingSamples)
		*audit = aud = gMissingSamples;
	else
		return 0;

	for (j = skipfirst; samplenames[j] != 0; j++)
	{
		/* skip empty definitions */
		if (strlen (samplenames[j]) == 0)
			continue;
		f = osd_fopen (gamedrv->name, samplenames[j], OSD_FILETYPE_SAMPLE, 0);
		if (f == NULL && skipfirst)
			f = osd_fopen (sharedname, samplenames[j], OSD_FILETYPE_SAMPLE, 0);

		if (f)
			osd_fclose(f);
		else
		{
			strcpy (aud->name, samplenames[j]);
			count++;
			aud++;
		}
	}
	return count;
}


/* Generic function for evaluating a sampleset. Some platforms may wish to
   call AuditSampleSet() instead and implement their own reporting (like MacMAME). */
int VerifySampleSet (int game, verify_printf_proc verify_printf)
{
	tMissingSample	*aud;
	int				count;

	count = AuditSampleSet (game, &aud);
	if (count==-1)
		return NOTFOUND;
	else if (count==0)
		return CORRECT;

	/* list missing samples */
	while (count--)
	{
		verify_printf ("%-8s: %s NOT FOUND\n", drivers[game]->name, aud->name);
		aud++;
	}

	return INCORRECT;
}
