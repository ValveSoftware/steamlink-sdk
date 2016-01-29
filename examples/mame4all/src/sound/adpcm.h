#ifndef ADPCM_H
#define ADPCM_H

#define MAX_ADPCM 8

/* NOTE: Actual sample data is specified in the sound_prom parameter of the game driver, but
   since the MAME code expects this to be an array of char *'s, we do a small kludge here */

struct ADPCMsample
{
	int num;       /* trigger number (-1 to mark the end) */
	int offset;    /* offset in that region */
	int length;    /* length of the sample */
};


/* a generic ADPCM interface, for unknown chips */

struct ADPCMinterface
{
	int num;			       /* total number of ADPCM decoders in the machine */
	int frequency;             /* playback frequency */
	int region;                /* memory region where the samples come from */
	void (*init)(const struct ADPCMinterface *, struct ADPCMsample *, int max); /* initialization function */
	int mixing_level[MAX_ADPCM];     /* master volume */
};

int ADPCM_sh_start(const struct MachineSound *msound);
void ADPCM_sh_stop(void);
void ADPCM_sh_update(void);

void ADPCM_trigger(int num, int which);
void ADPCM_play(int num, int offset, int length);
void ADPCM_setvol(int num, int vol);
void ADPCM_stop(int num);
int ADPCM_playing(int num);


/* an interface for the OKIM6295 and similar chips */

#define MAX_OKIM6295 			2
#define MAX_OKIM6295_VOICES		4
#define ALL_VOICES				-1

struct OKIM6295interface
{
	int num;                  		/* total number of chips */
	int frequency[MAX_OKIM6295];	/* playback frequency */
	int region[MAX_OKIM6295];		/* memory region where the sample ROM lives */
	int mixing_level[MAX_OKIM6295];	/* master volume */
};

int OKIM6295_sh_start(const struct MachineSound *msound);
void OKIM6295_sh_stop(void);
void OKIM6295_sh_update(void);
void OKIM6295_set_bank_base(int which, int voice, int base);	/* set voice to ALL_VOICES to set all banks at once */
void OKIM6295_set_frequency(int which, int voice, int frequency);	/* set voice to ALL_VOICES to set all banks at once */

READ_HANDLER( OKIM6295_status_0_r );
READ_HANDLER( OKIM6295_status_1_r );
WRITE_HANDLER( OKIM6295_data_0_w );
WRITE_HANDLER( OKIM6295_data_1_w );


#endif
