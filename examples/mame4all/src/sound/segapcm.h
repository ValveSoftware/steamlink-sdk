/*********************************************************/
/*    SEGA 8bit PCM                                      */
/*********************************************************/

#ifndef __SEGAPCM_H__
#define __SEGAPCM_H__


/************************************************/
#define   BANK_256    (11)
#define   BANK_512    (12)
#define   BANK_12M    (13)
#define   BANK_MASK7    (0x70<<16)
#define   BANK_MASKF    (0xf0<<16)
#define   BANK_MASKF8   (0xf8<<16)

#define   SEGAPCM_MAX    (16)
enum
{
	L_PAN = 0,
	R_PAN = 1,
	LR_PAN = 2
};


typedef struct segapcm
{
	char  writeram[0x1000];

	unsigned char  gain[SEGAPCM_MAX][LR_PAN];
	unsigned char  addr_l[SEGAPCM_MAX];
	unsigned char  addr_h[SEGAPCM_MAX];
	unsigned char  bank[SEGAPCM_MAX];
	unsigned char  end_h[SEGAPCM_MAX];
	unsigned char  delta_t[SEGAPCM_MAX];

	int            vol[SEGAPCM_MAX][LR_PAN];

	unsigned int   add_addr[SEGAPCM_MAX];
	unsigned int   step[SEGAPCM_MAX];
	int   flag[SEGAPCM_MAX];
	int   bankshift;
	int   bankmask;

	int pcmd[SEGAPCM_MAX];
	int pcma[SEGAPCM_MAX];
} SEGAPCM;

struct SEGAPCMinterface
{
	int  mode;
	int  bank;
	int  region;
	int  volume;
};

enum SEGAPCM_samplerate
{
	SEGAPCM_SAMPLE15K,
	SEGAPCM_SAMPLE32K
};

/**************** prottype ****************/
int SEGAPCM_sh_start( const struct MachineSound *msound );
void SEGAPCM_sh_stop( void );
void SEGAPCM_sh_update( void );

int SEGAPCMInit( const struct MachineSound *msound, int banksize, int mode, unsigned char *inpcm, int volume );
void SEGAPCMShutdown( void );
void SEGAPCMResetChip( void );
WRITE_HANDLER( SegaPCM_w );
READ_HANDLER( SegaPCM_r );

/************************************************/
#endif
/**************** end of file ****************/
