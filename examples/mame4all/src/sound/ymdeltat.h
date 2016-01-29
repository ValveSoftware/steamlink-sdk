#ifndef __YMDELTAT_H_
#define __YMDELTAT_H_

#define YM_DELTAT_SHIFT    (16)

/* adpcm type A and type B struct */
typedef struct deltat_adpcm_state {
	UINT8 *memory;
	int memory_size;
	float freqbase;
	INT32 *output_pointer; /* pointer of output pointers */
	int output_range;

	UINT8 reg[16];
	UINT8 portstate,portcontrol;
	int portshift;

	UINT8 flag;          /* port state        */
	UINT8 flagMask;      /* arrived flag mask */
	UINT8 now_data;
	UINT32 now_addr;
	UINT32 now_step;
	UINT32 step;
	UINT32 start;
	UINT32 end;
	UINT32 delta;
	INT32 volume;
	INT32 *pan;        /* &output_pointer[pan] */
	INT32 /*adpcmm,*/ adpcmx, adpcmd;
	INT32 adpcml;			/* hiro-shi!! */

	/* leveling and re-sampling state for DELTA-T */
	INT32 volume_w_step;   /* volume with step rate */
	INT32 next_leveling;   /* leveling value        */
	INT32 sample_step;     /* step of re-sampling   */

	UINT8 arrivedFlag;    /* flag of arrived end address */
}YM_DELTAT;

/* static state */
extern UINT8 *ym_deltat_memory;       /* memory pointer */

/* before YM_DELTAT_ADPCM_CALC(YM_DELTAT *DELTAT); */
#define YM_DELTAT_DECODE_PRESET(DELTAT) {ym_deltat_memory = DELTAT->memory;}

void YM_DELTAT_ADPCM_Write(YM_DELTAT *DELTAT,int r,int v);
void YM_DELTAT_ADPCM_Reset(YM_DELTAT *DELTAT,int pan);

/* INLINE void YM_DELTAT_ADPCM_CALC(YM_DELTAT *DELTAT); */
#define YM_INLINE_BLOCK
#include "ymdeltat.cpp" /* include inline function section */
#undef  YM_INLINE_BLOCK

#endif
