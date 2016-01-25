/*
**
** File: ymdeltat.c
**
** YAMAHA DELTA-T adpcm sound emulation subroutine
** used by fmopl.c(v0.36e-) and fm.c(v0.36c-)
**
** Base program is YM2610 emulator by Hiromitsu Shioya.
** Written by Tatsuyuki Satoh
**
** Version 0.36b
**
** sound chips who has this unit
**
** YM2608   OPNA
** YM2610/B OPNB
** Y8950    MSX AUDIO
**
**
*/

#include "osinline.h"
#ifndef YM_INLINE_BLOCK

#include "driver.h"
#include "ymdeltat.h"

/* -------------------- log output  -------------------- */
/* log output level */
#define LOG_ERR  3      /* ERROR       */
#define LOG_WAR  2      /* WARNING     */
#define LOG_INF  1      /* INFORMATION */
#define LOG_LEVEL LOG_INF

#ifndef __RAINE__
#define LOG(n,x) if( (n)>=LOG_LEVEL ) logerror x
#endif

UINT8 *ym_deltat_memory;      /* memory pointer */

/* Forecast to next Forecast (rate = *8) */
/* 1/8 , 3/8 , 5/8 , 7/8 , 9/8 , 11/8 , 13/8 , 15/8 */
const INT32 ym_deltat_decode_tableB1[16] = {
  1,   3,   5,   7,   9,  11,  13,  15,
  -1,  -3,  -5,  -7,  -9, -11, -13, -15,
};
/* delta to next delta (rate= *64) */
/* 0.9 , 0.9 , 0.9 , 0.9 , 1.2 , 1.6 , 2.0 , 2.4 */
const INT32 ym_deltat_decode_tableB2[16] = {
  57,  57,  57,  57, 77, 102, 128, 153,
  57,  57,  57,  57, 77, 102, 128, 153
};

/* DELTA-T-ADPCM write register */
void YM_DELTAT_ADPCM_Write(YM_DELTAT *DELTAT,int r,int v)
{
	if(r>=0x10) return;
	DELTAT->reg[r] = v; /* stock data */

	switch( r ){
	case 0x00:	/* START,REC,MEMDATA,REPEAT,SPOFF,--,--,RESET */
#if 0
		case 0x60:	/* write buffer MEMORY from PCM data port */
		case 0x20:	/* read  buffer MEMORY to   PCM data port */
#endif
		if( v&0x80 ){
			DELTAT->portstate = v&0x90; /* start req,memory mode,repeat flag copy */
			/**** start ADPCM ****/
			DELTAT->volume_w_step = (float)DELTAT->volume * DELTAT->step / (1<<YM_DELTAT_SHIFT);
			DELTAT->now_addr = (DELTAT->start)<<1;
			DELTAT->now_step = (1<<YM_DELTAT_SHIFT)-DELTAT->step;
			/*adpcm->adpcmm   = 0;*/
			DELTAT->adpcmx   = 0;
			DELTAT->adpcml   = 0;
			DELTAT->adpcmd   = YM_DELTAT_DELTA_DEF;
			DELTAT->next_leveling=0;
			DELTAT->flag     = 1; /* start ADPCM */

			if( !DELTAT->step )
			{
				DELTAT->flag = 0;
				DELTAT->portstate = 0x00;
			}
			/**** PCM memory check & limit check ****/
			if(DELTAT->memory == 0){			// Check memory Mapped
				LOG(LOG_ERR,("YM Delta-T ADPCM rom not mapped\n"));
				DELTAT->flag = 0;
				DELTAT->portstate = 0x00;
				//logerror("DELTAT memory 0\n");
			}else{
				if( DELTAT->end >= DELTAT->memory_size )
				{		// Check End in Range
					LOG(LOG_ERR,("YM Delta-T ADPCM end out of range: $%08x\n",DELTAT->end));
					DELTAT->end = DELTAT->memory_size - 1;
					//logerror("DELTAT end over\n");
				}
				if( DELTAT->start >= DELTAT->memory_size )
				{		// Check Start in Range
					LOG(LOG_ERR,("YM Delta-T ADPCM start out of range: $%08x\n",DELTAT->start));
					DELTAT->flag = 0;
					DELTAT->portstate = 0x00;
					//logerror("DELTAT start under\n");
				}
			}
		} else if( v&0x01 ){
			DELTAT->flag = 0;
			DELTAT->portstate = 0x00;
		}
		break;
	case 0x01:	/* L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM */
		DELTAT->portcontrol = v&0xff;
		DELTAT->pan = &DELTAT->output_pointer[(v>>6)&0x03];
		break;
	case 0x02:	/* Start Address L */
	case 0x03:	/* Start Address H */
		DELTAT->start  = (DELTAT->reg[0x3]*0x0100 | DELTAT->reg[0x2]) << DELTAT->portshift;
		break;
	case 0x04:	/* Stop Address L */
	case 0x05:	/* Stop Address H */
		DELTAT->end    = (DELTAT->reg[0x5]*0x0100 | DELTAT->reg[0x4]) << DELTAT->portshift;
		DELTAT->end   += (1<<DELTAT->portshift) - 1;
		break;
	case 0x06:	/* Prescale L (PCM and Recoard frq) */
	case 0x07:	/* Proscale H */
	case 0x08:	/* ADPCM data */
	  break;
	case 0x09:	/* DELTA-N L (ADPCM Playback Prescaler) */
	case 0x0a:	/* DELTA-N H */
		DELTAT->delta  = (DELTAT->reg[0xa]*0x0100 | DELTAT->reg[0x9]);
		DELTAT->step     = (UINT32)((float)(DELTAT->delta*(1<<(YM_DELTAT_SHIFT-16)))*(DELTAT->freqbase));
		DELTAT->volume_w_step = (float)DELTAT->volume * DELTAT->step / (1<<YM_DELTAT_SHIFT);
		break;
	case 0x0b:	/* Level control (volume , voltage flat) */
		{
			INT32 oldvol = DELTAT->volume;
			DELTAT->volume = (v&0xff)*(DELTAT->output_range/256) / YM_DELTAT_DECODE_RANGE;
			if( oldvol != 0 )
			{
				DELTAT->adpcml      = (int)((float)DELTAT->adpcml      / (float)oldvol * (float)DELTAT->volume);
				DELTAT->sample_step = (int)((float)DELTAT->sample_step / (float)oldvol * (float)DELTAT->volume);
			}
			DELTAT->volume_w_step = (int)((float)DELTAT->volume * (float)DELTAT->step / (float)(1<<YM_DELTAT_SHIFT));
		}
		break;
	}
}

void YM_DELTAT_ADPCM_Reset(YM_DELTAT *DELTAT,int pan)
{
	DELTAT->now_addr  = 0;
	DELTAT->now_step  = 0;
	DELTAT->step      = 0;
	DELTAT->start     = 0;
	DELTAT->end       = 0;
	/* F2610->adpcm[i].delta     = 21866; */
	DELTAT->volume    = 0;
	DELTAT->pan       = &DELTAT->output_pointer[pan];
	/* DELTAT->flagMask  = 0; */
	DELTAT->arrivedFlag = 0;
	DELTAT->flag      = 0;
	DELTAT->adpcmx    = 0;
	DELTAT->adpcmd    = 127;
	DELTAT->adpcml    = 0;
	/*DELTAT->adpcmm    = 0;*/
	DELTAT->volume_w_step = 0;
    DELTAT->next_leveling = 0;
	DELTAT->portstate = 0;
	/* DELTAT->portshift = 8; */
}

#else /* YM_INLINE_BLOCK */

/* ---------- inline block ---------- */

/* DELTA-T particle adjuster */
#define YM_DELTAT_DELTA_MAX (24576)
#define YM_DELTAT_DELTA_MIN (127)
#define YM_DELTAT_DELTA_DEF (127)

#define YM_DELTAT_DECODE_RANGE 32768
#define YM_DELTAT_DECODE_MIN (-(YM_DELTAT_DECODE_RANGE))
#define YM_DELTAT_DECODE_MAX ((YM_DELTAT_DECODE_RANGE)-1)

extern const INT32 ym_deltat_decode_tableB1[];
extern const INT32 ym_deltat_decode_tableB2[];

#define YM_DELTAT_Limit(val,max,min)	\
{										\
	if ( val > max ) val = max;			\
	else if ( val < min ) val = min;	\
}

/**** ADPCM B (Delta-T control type) ****/
INLINE void YM_DELTAT_ADPCM_CALC(YM_DELTAT *DELTAT)
{
	UINT32 step;
	int data;
	INT32 old_m;
	INT32 now_leveling;
	INT32 delta_next;

	DELTAT->now_step += DELTAT->step;
	if ( DELTAT->now_step >= (1<<YM_DELTAT_SHIFT) )
	{
		step = DELTAT->now_step >> YM_DELTAT_SHIFT;
		DELTAT->now_step &= (1<<YM_DELTAT_SHIFT)-1;
		do{
			if ( DELTAT->now_addr > (DELTAT->end<<1) ) {
				if( DELTAT->portstate&0x10 ){
					/**** repeat start ****/
					DELTAT->now_addr = DELTAT->start<<1;
					DELTAT->adpcmx   = 0;
					DELTAT->adpcmd   = YM_DELTAT_DELTA_DEF;
					DELTAT->next_leveling = 0;
					DELTAT->flag     = 1;
				}else{
					DELTAT->arrivedFlag |= DELTAT->flagMask;
					DELTAT->flag = 0;
					DELTAT->adpcml = 0;
					now_leveling = 0;
					return;
				}
			}
			if( DELTAT->now_addr&1 ) data = DELTAT->now_data & 0x0f;
			else
			{
				DELTAT->now_data = *(ym_deltat_memory+(DELTAT->now_addr>>1));
				data = DELTAT->now_data >> 4;
			}
			DELTAT->now_addr++;
			/* shift Measurement value */
			old_m      = DELTAT->adpcmx/*adpcmm*/;
			/* ch->adpcmm = YM_DELTAT_Limit( ch->adpcmx + (decode_tableB3[data] * ch->adpcmd / 8) ,YM_DELTAT_DECODE_MAX, YM_DELTAT_DECODE_MIN ); */
			/* Forecast to next Forecast */
#ifndef mix_sample
			DELTAT->adpcmx += (ym_deltat_decode_tableB1[data] * DELTAT->adpcmd / 8);
			YM_DELTAT_Limit(DELTAT->adpcmx,YM_DELTAT_DECODE_MAX, YM_DELTAT_DECODE_MIN);
#else
			mix_sample(DELTAT->adpcmx,(ym_deltat_decode_tableB1[data] * DELTAT->adpcmd / 8));
#endif
			/* delta to next delta */
			DELTAT->adpcmd = (DELTAT->adpcmd * ym_deltat_decode_tableB2[data] ) / 64;
			YM_DELTAT_Limit(DELTAT->adpcmd,YM_DELTAT_DELTA_MAX, YM_DELTAT_DELTA_MIN );
			/* shift leveling value */
			delta_next        = DELTAT->adpcmx/*adpcmm*/ - old_m;
			now_leveling      = DELTAT->next_leveling;
			DELTAT->next_leveling = old_m + (delta_next / 2);
		}while(--step);
//#define YM_DELTAT_CUT_RE_SAMPLING
#ifdef YM_DELTAT_CUT_RE_SAMPLING
		DELTAT->adpcml  = DELTAT->next_leveling * DELTAT->volume;
		DELTAT->adpcml  = DELTAT->adpcmx/*adpcmm*/ * DELTAT->volume;
	}
#else
		/* delta step of re-sampling */
		DELTAT->sample_step = (DELTAT->next_leveling - now_leveling) * DELTAT->volume_w_step;
		/* output of start point */
		DELTAT->adpcml  = now_leveling * DELTAT->volume;
		/* adjust to now */
		DELTAT->adpcml += (int)((float)DELTAT->sample_step * ((float)DELTAT->now_step/(float)DELTAT->step));
	}
	DELTAT->adpcml += DELTAT->sample_step;
#endif
	/* output for work of output channels (outd[OPNxxxx])*/
	*(DELTAT->pan) += DELTAT->adpcml;
}

#endif /* YM_INLINE_BLOCK */
