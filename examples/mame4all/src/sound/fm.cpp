#define YM2610B_WARNING

/* YM2608 rhythm data is PCM ,not an ADPCM */
#define YM2608_RHYTHM_PCM

/*
**
** File: fm.c -- software implementation of FM sound generator
**
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmurator development
**
** Version 0.37
**
*/

/*
**** change log. (hiro-shi) ****
** 08-12-98:
** rename ADPCMA -> ADPCMB, ADPCMB -> ADPCMA
** move ROM limit check.(CALC_CH? -> 2610Write1/2)
** test program (ADPCMB_TEST)
** move ADPCM A/B end check.
** ADPCMB repeat flag(no check)
** change ADPCM volume rate (8->16) (32->48).
**
** 09-12-98:
** change ADPCM volume. (8->16, 48->64)
** replace ym2610 ch0/3 (YM-2610B)
** init cur_chip (restart bug fix)
** change ADPCM_SHIFT (10->8) missing bank change 0x4000-0xffff.
** add ADPCM_SHIFT_MASK
** change ADPCMA_DECODE_MIN/MAX.
*/

/*
    no check:
		YM2608 rhythm sound
		OPN SSG type envelope (SEG)
		YM2151 CSM speech mode

	no support:
		status BUSY flag (everytime not busy)
		YM2608 status mask (register :0x110)
		YM2608 RYTHM sound
		YM2608 PCM memory data access , DELTA-T-ADPCM with PCM port
		YM2151 CSM speech mode with internal timer

	preliminary :
		key scale level rate (?)
		attack rate time rate , curve
		decay  rate time rate , curve
		self-feedback algorythm
		YM2610 ADPCM-A mixing level , decode algorythm
		YM2151 noise mode (CH7.OP4)
		LFO contoller (YM2612/YM2610/YM2608/YM2151)

	note:
                        OPN                           OPM
		fnum          fM * 2^20 / (fM/(12*n))
		TimerOverA    ( 12*n)*(1024-NA)/fM        64*(1024-Na)/fM
		TimerOverB    (192*n)*(256-NB)/fM       1024*(256-Nb)/fM
		output bits   10bit<<3bit               16bit * 2ch (YM3012=10bit<<3bit)
		sampling rate fFM / (12*priscaler)      fM / 64
		lfo freq                                ( fM*2^(LFRQ/16) ) / (4295*10^6)
*/

/************************************************************************/
/*    comment of hiro-shi(Hiromitsu Shioya)                             */
/*    YM2610(B) = (OPN-B                                                */
/*    YM2610  : PSG:3ch FM:4ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/*    YM2610B : PSG:3ch FM:6ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#ifndef __RAINE__
#include "driver.h"		/* use M.A.M.E. */
#else
#include "deftypes.h"		/* use RAINE */
#include "support.h"		/* use RAINE */
#endif
#include "ay8910.h"
#include "fm.h"
#include "osinline.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

/***** shared function building option ****/
#define BUILD_OPN (BUILD_YM2203||BUILD_YM2608||BUILD_YM2610||BUILD_YM2612)
#define BUILD_OPNB (BUILD_YM2610||BUILD_YM2610B)
#define BUILD_FM_ADPCMA (BUILD_YM2608||BUILD_OPNB)
#define BUILD_FM_ADPCMB (BUILD_YM2608||BUILD_OPNB)
#define BUILD_STEREO (BUILD_YM2608||BUILD_YM2610||BUILD_YM2612||BUILD_YM2151)
#define BUILD_LFO (BUILD_YM2608||BUILD_YM2610||BUILD_YM2612||BUILD_YM2151)

#if BUILD_FM_ADPCMB
/* include external DELTA-T ADPCM unit */
  #include "ymdeltat.h"		/* DELTA-T ADPCM UNIT */
  #define DELTAT_MIXING_LEVEL (4) /* DELTA-T ADPCM MIXING LEVEL */
#endif

/* -------------------- sound quality define selection --------------------- */
/* sinwave entries */
/* used static memory = SIN_ENT * 4 (byte) */
#define SIN_ENT 2048

/* lower bits of envelope counter */
#define ENV_BITS 16

/* envelope output entries */
#define EG_ENT   4096
#define EG_STEP (96.0/EG_ENT) /* OPL == 0.1875 dB */

#if FM_LFO_SUPPORT
/* LFO table entries */
#define LFO_ENT 512
#define LFO_SHIFT (32-9)
#define LFO_RATE 0x10000
#endif

/* -------------------- preliminary define section --------------------- */
/* attack/decay rate time rate */
#define OPM_ARRATE     399128
#define OPM_DRRATE    5514396
/* It is not checked , because I haven't YM2203 rate */
#define OPN_ARRATE  OPM_ARRATE
#define OPN_DRRATE  OPM_DRRATE

/* PG output cut off level : 78dB(14bit)? */
#define PG_CUT_OFF ((int)(78.0/EG_STEP))
/* EG output cut off level : 68dB? */
#define EG_CUT_OFF ((int)(68.0/EG_STEP))

#define FREQ_BITS 24		/* frequency turn          */

/* PG counter is 21bits @oct.7 */
#define FREQ_RATE   (1<<(FREQ_BITS-21))
#define TL_BITS    (FREQ_BITS+2)
/* OPbit = 14(13+sign) : TL_BITS+1(sign) / output = 16bit */
#define TL_SHIFT (TL_BITS+1-(14-16))

/* output final shift */
#define FM_OUTSB  (TL_SHIFT-FM_OUTPUT_BIT)
#define FM_MAXOUT ((1<<(TL_SHIFT-1))-1)
#define FM_MINOUT (-(1<<(TL_SHIFT-1)))

/* -------------------- local defines , macros --------------------- */

/* envelope counter position */
#define EG_AST   0					/* start of Attack phase */
#define EG_AED   (EG_ENT<<ENV_BITS)	/* end   of Attack phase */
#define EG_DST   EG_AED				/* start of Decay/Sustain/Release phase */
#define EG_DED   (EG_DST+(EG_ENT<<ENV_BITS)-1)	/* end   of Decay/Sustain/Release phase */
#define EG_OFF   EG_DED				/* off */
#if FM_SEG_SUPPORT
#define EG_UST   ((2*EG_ENT)<<ENV_BITS)  /* start of SEG UPSISE */
#define EG_UED   ((3*EG_ENT)<<ENV_BITS)  /* end of SEG UPSISE */
#endif

/* register number to channel number , slot offset */
#define OPN_CHAN(N) (N&3)
#define OPN_SLOT(N) ((N>>2)&3)
#define OPM_CHAN(N) (N&7)
#define OPM_SLOT(N) ((N>>3)&3)
/* slot number */
#define SLOT1 0
#define SLOT2 2
#define SLOT3 1
#define SLOT4 3

/* bit0 = Right enable , bit1 = Left enable */
#define OUTD_RIGHT  1
#define OUTD_LEFT   2
#define OUTD_CENTER 3

/* FM timer model */
#define FM_TIMER_SINGLE (0)
#define FM_TIMER_INTERVAL (1)

/* ---------- OPN / OPM one channel  ---------- */
typedef struct fm_slot {
	INT32 *DT;			/* detune          :DT_TABLE[DT]       */
	int DT2;			/* multiple,Detune2:(DT2<<4)|ML for OPM*/
	int TL;				/* total level     :TL << 8            */
	UINT8 KSR;			/* key scale rate  :3-KSR              */
	const INT32 *AR;	/* attack rate     :&AR_TABLE[AR<<1]   */
	const INT32 *DR;	/* decay rate      :&DR_TABLE[DR<<1]   */
	const INT32 *SR;	/* sustin rate     :&DR_TABLE[SR<<1]   */
	int   SL;			/* sustin level    :SL_TABLE[SL]       */
	const INT32 *RR;	/* release rate    :&DR_TABLE[RR<<2+2] */
	UINT8 SEG;			/* SSG EG type     :SSGEG              */
	UINT8 ksr;			/* key scale rate  :kcode>>(3-KSR)     */
	UINT32 mul;			/* multiple        :ML_TABLE[ML]       */
	/* Phase Generator */
	UINT32 Cnt;			/* frequency count :                   */
	UINT32 Incr;		/* frequency step  :                   */
	/* Envelope Generator */
	void (*eg_next)(struct fm_slot *SLOT);	/* pointer of phase handler */
	INT32 evc;			/* envelope counter                    */
	INT32 eve;			/* envelope counter end point          */
	INT32 evs;			/* envelope counter step               */
	INT32 evsa;			/* envelope step for Attack            */
	INT32 evsd;			/* envelope step for Decay             */
	INT32 evss;			/* envelope step for Sustain           */
	INT32 evsr;			/* envelope step for Release           */
	INT32 TLL;			/* adjusted TotalLevel                 */
	/* LFO */
	UINT8 amon;			/* AMS enable flag              */
	UINT32 ams;			/* AMS depth level of this SLOT */
}FM_SLOT;

typedef struct fm_chan {
	FM_SLOT	SLOT[4];
	UINT8 PAN;			/* PAN :NONE,LEFT,RIGHT or CENTER */
	UINT8 ALGO;			/* Algorythm                      */
	UINT8 FB;			/* shift count of self feed back  */
	INT32 op1_out[2];	/* op1 output for beedback        */
	/* Algorythm (connection) */
	INT32 *connect1;		/* pointer of SLOT1 output    */
	INT32 *connect2;		/* pointer of SLOT2 output    */
	INT32 *connect3;		/* pointer of SLOT3 output    */
	INT32 *connect4;		/* pointer of SLOT4 output    */
	/* LFO */
	INT32 pms;				/* PMS depth level of channel */
	UINT32 ams;				/* AMS depth level of channel */
	/* Phase Generator */
	UINT32 fc;			/* fnum,blk    :adjusted to sampling rate */
	UINT8 fn_h;			/* freq latch  :                   */
	UINT8 kcode;		/* key code    :                   */
} FM_CH;

/* OPN/OPM common state */
typedef struct fm_state {
	UINT8 index;		/* chip index (number of chip) */
	int clock;			/* master clock  (Hz)  */
	int rate;			/* sampling rate (Hz)  */
	double freqbase;	/* frequency base      */
	timer_tm TimerBase;	/* Timer base time     */
	UINT8 address;		/* address register    */
	UINT8 irq;			/* interrupt level     */
	UINT8 irqmask;		/* irq mask            */
	UINT8 status;		/* status flag         */
	UINT32 mode;		/* mode  CSM / 3SLOT   */
	int TA;				/* timer a             */
	int TAC;			/* timer a counter     */
	UINT8 TB;			/* timer b             */
	int TBC;			/* timer b counter     */
	/* speedup customize */
	/* local time tables */
	INT32 DT_TABLE[8][32];	/* DeTune tables       */
	INT32 AR_TABLE[94];		/* Atttack rate tables */
	INT32 DR_TABLE[94];		/* Decay rate tables   */
	/* Extention Timer and IRQ handler */
	FM_TIMERHANDLER	Timer_Handler;
	FM_IRQHANDLER	IRQ_Handler;
	/* timer model single / interval */
	UINT8 timermodel;
}FM_ST;

/* -------------------- tables --------------------- */

/* sustain lebel table (3db per step) */
/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db) (int)((db*((3/EG_STEP)*(1<<ENV_BITS)))+EG_DST)
static const int SL_TABLE[16]={
 SC( 0),SC( 1),SC( 2),SC(3 ),SC(4 ),SC(5 ),SC(6 ),SC( 7),
 SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)
};
#undef SC

/* size of TL_TABLE = sinwave(max cut_off) + cut_off(tl + ksr + envelope + ams) */
#define TL_MAX (PG_CUT_OFF+EG_CUT_OFF+1)

/* TotalLevel : 48 24 12  6  3 1.5 0.75 (dB) */
/* TL_TABLE[ 0      to TL_MAX          ] : plus  section */
/* TL_TABLE[ TL_MAX to TL_MAX+TL_MAX-1 ] : minus section */
static INT32 *TL_TABLE = 0;

/* pointers to TL_TABLE with sinwave output offset */
static INT32 *SIN_TABLE[SIN_ENT];

/* envelope output curve table */
#if FM_SEG_SUPPORT
/* attack + decay + SSG upside + OFF */
static INT32 ENV_CURVE[3*EG_ENT+1];
#else
/* attack + decay + OFF */
static INT32 ENV_CURVE[2*EG_ENT+1];
#endif
/* envelope counter conversion table when change Decay to Attack phase */
static int DRAR_TABLE[EG_ENT];

#define OPM_DTTABLE OPN_DTTABLE
static UINT8 OPN_DTTABLE[4 * 32]={
/* this table is YM2151 and YM2612 data */
/* FD=0 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* FD=1 */
  0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
  2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
/* FD=2 */
  1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
  5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,
/* FD=3 */
  2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
  8 , 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22
};

/* multiple table */
#define ML(n) (int)(n*2)
static const int MUL_TABLE[4*16]= {
/* 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 */
   ML(0.50),ML( 1.00),ML( 2.00),ML( 3.00),ML( 4.00),ML( 5.00),ML( 6.00),ML( 7.00),
   ML(8.00),ML( 9.00),ML(10.00),ML(11.00),ML(12.00),ML(13.00),ML(14.00),ML(15.00),
/* DT2=1 *SQL(2)   */
   ML(0.71),ML( 1.41),ML( 2.82),ML( 4.24),ML( 5.65),ML( 7.07),ML( 8.46),ML( 9.89),
   ML(11.30),ML(12.72),ML(14.10),ML(15.55),ML(16.96),ML(18.37),ML(19.78),ML(21.20),
/* DT2=2 *SQL(2.5) */
   ML( 0.78),ML( 1.57),ML( 3.14),ML( 4.71),ML( 6.28),ML( 7.85),ML( 9.42),ML(10.99),
   ML(12.56),ML(14.13),ML(15.70),ML(17.27),ML(18.84),ML(20.41),ML(21.98),ML(23.55),
/* DT2=3 *SQL(3)   */
   ML( 0.87),ML( 1.73),ML( 3.46),ML( 5.19),ML( 6.92),ML( 8.65),ML(10.38),ML(12.11),
   ML(13.84),ML(15.57),ML(17.30),ML(19.03),ML(20.76),ML(22.49),ML(24.22),ML(25.95)
};
#undef ML

#if FM_LFO_SUPPORT

#define PMS_RATE 0x400
/* LFO runtime work */
/*static*/ UINT32 lfo_amd;
/*static*/ INT32 lfo_pmd;
#endif

/* Dummy table of Attack / Decay rate ( use when rate == 0 ) */
static const INT32 RATE_0[32]=
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* -------------------- state --------------------- */

/* some globals */
#define TYPE_SSG    0x01    /* SSG support          */
#define TYPE_OPN    0x02    /* OPN device           */
#define TYPE_LFOPAN 0x04    /* OPN type LFO and PAN */
#define TYPE_6CH    0x08    /* FM 6CH / 3CH         */
#define TYPE_DAC    0x10    /* YM2612's DAC device  */
#define TYPE_ADPCM  0x20    /* two ADPCM unit       */

#define TYPE_YM2203 (TYPE_SSG)
#define TYPE_YM2608 (TYPE_SSG |TYPE_LFOPAN |TYPE_6CH |TYPE_ADPCM)
#define TYPE_YM2610 (TYPE_SSG |TYPE_LFOPAN |TYPE_6CH |TYPE_ADPCM)
#define TYPE_YM2612 (TYPE_6CH |TYPE_LFOPAN |TYPE_DAC)

/* current chip state */
static void *cur_chip = 0;		/* pointer of current chip struct */
static FM_ST  *State;			/* basic status */
static FM_CH  *cch[8];			/* pointer of FM channels */
#if (BUILD_LFO)
#if FM_LFO_SUPPORT
static UINT32 LFOCnt,LFOIncr;	/* LFO PhaseGenerator */
#endif
#endif

/* runtime work */
/*static*/ INT32 out_ch[4];		/* channel output NONE,LEFT,RIGHT or CENTER */
/*static*/ INT32 pg_in1,pg_in2,pg_in3,pg_in4;	/* PG input of SLOTs */

/* -------------------- log output  -------------------- */
/* log output level */
#define LOG_ERR  3      /* ERROR       */
#define LOG_WAR  2      /* WARNING     */
#define LOG_INF  1      /* INFORMATION */
#define LOG_LEVEL LOG_INF

//#ifndef __RAINE__
//#define LOG(n,x) if( (n)>=LOG_LEVEL ) logerror x
//#endif
#define LOG(n,x)

/* ----- limitter ----- */
#define Limit(val, max,min) { \
	if ( val > max )      val = max; \
	else if ( val < min ) val = min; \
}

/* ----- buffering one of data(STEREO chip) ----- */
#if FM_STEREO_MIX
/* stereo mixing */
#define FM_BUFFERING_STEREO \
{														\
	/* get left & right output with clipping */			\
	out_ch[OUTD_LEFT]  += out_ch[OUTD_CENTER];				\
	Limit( out_ch[OUTD_LEFT] , FM_MAXOUT, FM_MINOUT );	\
	out_ch[OUTD_RIGHT] += out_ch[OUTD_CENTER];				\
	Limit( out_ch[OUTD_RIGHT], FM_MAXOUT, FM_MINOUT );	\
	/* buffering */										\
	*bufL++ = out_ch[OUTD_LEFT] >>FM_OUTSB;				\
	*bufL++ = out_ch[OUTD_RIGHT]>>FM_OUTSB;				\
}
#else
/* stereo separate */
#define FM_BUFFERING_STEREO \
{														\
	/* get left & right output with clipping */			\
	out_ch[OUTD_LEFT]  += out_ch[OUTD_CENTER];				\
	Limit( out_ch[OUTD_LEFT] , FM_MAXOUT, FM_MINOUT );	\
	out_ch[OUTD_RIGHT] += out_ch[OUTD_CENTER];				\
	Limit( out_ch[OUTD_RIGHT], FM_MAXOUT, FM_MINOUT );	\
	/* buffering */										\
	bufL[i] = out_ch[OUTD_LEFT] >>FM_OUTSB;				\
	bufR[i] = out_ch[OUTD_RIGHT]>>FM_OUTSB;				\
}
#endif

#if FM_INTERNAL_TIMER
/* ----- internal timer mode , update timer */
/* ---------- calcrate timer A ---------- */
#define INTERNAL_TIMER_A(ST,CSM_CH)					\
{													\
	if( ST->TAC &&  (ST->Timer_Handler==0) )		\
		if( (ST->TAC -= (int)(ST->freqbase*4096)) <= 0 )	\
		{											\
			TimerAOver( ST );						\
			/* CSM mode total level latch and auto key on */	\
			if( ST->mode & 0x80 )					\
				CSMKeyControll( CSM_CH );			\
		}											\
}
/* ---------- calcrate timer B ---------- */
#define INTERNAL_TIMER_B(ST,step)						\
{														\
	if( ST->TBC && (ST->Timer_Handler==0) )				\
		if( (ST->TBC -= (int)(ST->freqbase*4096*step)) <= 0 )	\
			TimerBOver( ST );							\
}
#else /* FM_INTERNAL_TIMER */
/* external timer mode */
#define INTERNAL_TIMER_A(ST,CSM_CH)
#define INTERNAL_TIMER_B(ST,step)
#endif /* FM_INTERNAL_TIMER */

/* --------------------- subroutines  --------------------- */
/* status set and IRQ handling */
INLINE void FM_STATUS_SET(FM_ST *ST,int flag)
{
	/* set status flag */
	ST->status |= flag;
	if ( !(ST->irq) && (ST->status & ST->irqmask) )
	{
		ST->irq = 1;
		/* callback user interrupt handler (IRQ is OFF to ON) */
		if(ST->IRQ_Handler) (ST->IRQ_Handler)(ST->index,1);
	}
}

/* status reset and IRQ handling */
INLINE void FM_STATUS_RESET(FM_ST *ST,int flag)
{
	/* reset status flag */
	ST->status &=~flag;
	if ( (ST->irq) && !(ST->status & ST->irqmask) )
	{
		ST->irq = 0;
		/* callback user interrupt handler (IRQ is ON to OFF) */
		if(ST->IRQ_Handler) (ST->IRQ_Handler)(ST->index,0);
	}
}

/* IRQ mask set */
INLINE void FM_IRQMASK_SET(FM_ST *ST,int flag)
{
	ST->irqmask = flag;
	/* IRQ handling check */
	FM_STATUS_SET(ST,0);
	FM_STATUS_RESET(ST,0);
}

/* ---------- event hander of Phase Generator ---------- */

/* Release end -> stop counter */
static void FM_EG_Release( FM_SLOT *SLOT )
{
	SLOT->evc = EG_OFF;
	SLOT->eve = EG_OFF+1;
	SLOT->evs = 0;
}

/* SUSTAIN end -> stop counter */
static void FM_EG_SR( FM_SLOT *SLOT )
{
	SLOT->evs = 0;
	SLOT->evc = EG_OFF;
	SLOT->eve = EG_OFF+1;
}

/* Decay end -> Sustain */
static void FM_EG_DR( FM_SLOT *SLOT )
{
	SLOT->eg_next = FM_EG_SR;
	SLOT->evc = SLOT->SL;
	SLOT->eve = EG_DED;
	SLOT->evs = SLOT->evss;
}

/* Attack end -> Decay */
static void FM_EG_AR( FM_SLOT *SLOT )
{
	/* next DR */
	SLOT->eg_next = FM_EG_DR;
	SLOT->evc = EG_DST;
	SLOT->eve = SLOT->SL;
	SLOT->evs = SLOT->evsd;
}

#if FM_SEG_SUPPORT
static void FM_EG_SSG_SR( FM_SLOT *SLOT );

/* SEG down side end  */
static void FM_EG_SSG_DR( FM_SLOT *SLOT )
{
	if( SLOT->SEG&2){
		/* reverce */
		SLOT->eg_next = FM_EG_SSG_SR;
		SLOT->evc = SLOT->SL + (EG_UST - EG_DST);
		SLOT->eve = EG_UED;
		SLOT->evs = SLOT->evss;
	}else{
		/* again */
		SLOT->evc = EG_DST;
	}
	/* hold */
	if( SLOT->SEG&1) SLOT->evs = 0;
}

/* SEG upside side end */
static void FM_EG_SSG_SR( FM_SLOT *SLOT )
{
	if( SLOT->SEG&2){
		/* reverce  */
		SLOT->eg_next = FM_EG_SSG_DR;
		SLOT->evc = EG_DST;
		SLOT->eve = EG_DED;
		SLOT->evs = SLOT->evsd;
	}else{
		/* again */
		SLOT->evc = SLOT->SL + (EG_UST - EG_DST);
	}
	/* hold check */
	if( SLOT->SEG&1) SLOT->evs = 0;
}

/* SEG Attack end */
static void FM_EG_SSG_AR( FM_SLOT *SLOT )
{
	if( SLOT->SEG&4){	/* start direction */
		/* next SSG-SR (upside start ) */
		SLOT->eg_next = FM_EG_SSG_SR;
		SLOT->evc = SLOT->SL + (EG_UST - EG_DST);
		SLOT->eve = EG_UED;
		SLOT->evs = SLOT->evss;
	}else{
		/* next SSG-DR (downside start ) */
		SLOT->eg_next = FM_EG_SSG_DR;
		SLOT->evc = EG_DST;
		SLOT->eve = EG_DED;
		SLOT->evs = SLOT->evsd;
	}
}
#endif /* FM_SEG_SUPPORT */

/* ----- key on of SLOT ----- */
#define FM_KEY_IS(SLOT) ((SLOT)->eg_next!=FM_EG_Release)

INLINE void FM_KEYON(FM_CH *CH , int s )
{
	FM_SLOT *SLOT = &CH->SLOT[s];
	if( !FM_KEY_IS(SLOT) )
	{
		/* restart Phage Generator */
		SLOT->Cnt = 0;
		/* phase -> Attack */
#if FM_SEG_SUPPORT
		if( SLOT->SEG&8 ) SLOT->eg_next = FM_EG_SSG_AR;
		else
#endif
		SLOT->eg_next = FM_EG_AR;
		SLOT->evs     = SLOT->evsa;
#if 0
		/* convert decay count to attack count */
		/* --- This caused the problem by credit sound of paper boy. --- */
		SLOT->evc = EG_AST + DRAR_TABLE[ENV_CURVE[SLOT->evc>>ENV_BITS]];/* + SLOT->evs;*/
#else
		/* reset attack counter */
		SLOT->evc = EG_AST;
#endif
		SLOT->eve = EG_AED;
	}
}
/* ----- key off of SLOT ----- */
INLINE void FM_KEYOFF(FM_CH *CH , int s )
{
	FM_SLOT *SLOT = &CH->SLOT[s];
	if( FM_KEY_IS(SLOT) )
	{
		/* if Attack phase then adjust envelope counter */
		if( SLOT->evc < EG_DST )
			SLOT->evc = (ENV_CURVE[SLOT->evc>>ENV_BITS]<<ENV_BITS) + EG_DST;
		/* phase -> Release */
		SLOT->eg_next = FM_EG_Release;
		SLOT->eve     = EG_DED;
		SLOT->evs     = SLOT->evsr;
	}
}

/* setup Algorythm and PAN connection */
static void setup_connection( FM_CH *CH )
{
	INT32 *carrier = &out_ch[CH->PAN]; /* NONE,LEFT,RIGHT or CENTER */

	switch( CH->ALGO ){
	case 0:
		/*  PG---S1---S2---S3---S4---OUT */
		CH->connect1 = &pg_in2;
		CH->connect2 = &pg_in3;
		CH->connect3 = &pg_in4;
		break;
	case 1:
		/*  PG---S1-+-S3---S4---OUT */
		/*  PG---S2-+               */
		CH->connect1 = &pg_in3;
		CH->connect2 = &pg_in3;
		CH->connect3 = &pg_in4;
		break;
	case 2:
		/* PG---S1------+-S4---OUT */
		/* PG---S2---S3-+          */
		CH->connect1 = &pg_in4;
		CH->connect2 = &pg_in3;
		CH->connect3 = &pg_in4;
		break;
	case 3:
		/* PG---S1---S2-+-S4---OUT */
		/* PG---S3------+          */
		CH->connect1 = &pg_in2;
		CH->connect2 = &pg_in4;
		CH->connect3 = &pg_in4;
		break;
	case 4:
		/* PG---S1---S2-+--OUT */
		/* PG---S3---S4-+      */
		CH->connect1 = &pg_in2;
		CH->connect2 = carrier;
		CH->connect3 = &pg_in4;
		break;
	case 5:
		/*         +-S2-+     */
		/* PG---S1-+-S3-+-OUT */
		/*         +-S4-+     */
		CH->connect1 = 0;	/* special case */
		CH->connect2 = carrier;
		CH->connect3 = carrier;
		break;
	case 6:
		/* PG---S1---S2-+     */
		/* PG--------S3-+-OUT */
		/* PG--------S4-+     */
		CH->connect1 = &pg_in2;
		CH->connect2 = carrier;
		CH->connect3 = carrier;
		break;
	case 7:
		/* PG---S1-+     */
		/* PG---S2-+-OUT */
		/* PG---S3-+     */
		/* PG---S4-+     */
		CH->connect1 = carrier;
		CH->connect2 = carrier;
		CH->connect3 = carrier;
	}
	CH->connect4 = carrier;
}

/* set detune & multiple */
INLINE void set_det_mul(FM_ST *ST,FM_CH *CH,FM_SLOT *SLOT,int v)
{
	SLOT->mul = MUL_TABLE[v&0x0f];
	SLOT->DT  = ST->DT_TABLE[(v>>4)&7];
	CH->SLOT[SLOT1].Incr=-1;
}

/* set total level */
INLINE void set_tl(FM_CH *CH,FM_SLOT *SLOT , int v,int csmflag)
{
	v &= 0x7f;
	v = (v<<7)|v; /* 7bit -> 14bit */
	SLOT->TL = (v*EG_ENT)>>14;
	/* if it is not a CSM channel , latch the total level */
	if( !csmflag )
		SLOT->TLL = SLOT->TL;
}

/* set attack rate & key scale  */
INLINE void set_ar_ksr(FM_CH *CH,FM_SLOT *SLOT,int v,INT32 *ar_table)
{
	SLOT->KSR  = 3-(v>>6);
	SLOT->AR   = (v&=0x1f) ? &ar_table[v<<1] : RATE_0;
	SLOT->evsa = SLOT->AR[SLOT->ksr];
	if( SLOT->eg_next == FM_EG_AR ) SLOT->evs = SLOT->evsa;
	CH->SLOT[SLOT1].Incr=-1;
}
/* set decay rate */
INLINE void set_dr(FM_SLOT *SLOT,int v,INT32 *dr_table)
{
	SLOT->DR = (v&=0x1f) ? &dr_table[v<<1] : RATE_0;
	SLOT->evsd = SLOT->DR[SLOT->ksr];
	if( SLOT->eg_next == FM_EG_DR ) SLOT->evs = SLOT->evsd;
}
/* set sustain rate */
INLINE void set_sr(FM_SLOT *SLOT,int v,INT32 *dr_table)
{
	SLOT->SR = (v&=0x1f) ? &dr_table[v<<1] : RATE_0;
	SLOT->evss = SLOT->SR[SLOT->ksr];
	if( SLOT->eg_next == FM_EG_SR ) SLOT->evs = SLOT->evss;
}
/* set release rate */
INLINE void set_sl_rr(FM_SLOT *SLOT,int v,INT32 *dr_table)
{
	SLOT->SL = SL_TABLE[(v>>4)];
	SLOT->RR = &dr_table[((v&0x0f)<<2)|2];
	SLOT->evsr = SLOT->RR[SLOT->ksr];
	if( SLOT->eg_next == FM_EG_Release ) SLOT->evs = SLOT->evsr;
}

/* operator output calcrator */
#define OP_OUT(PG,EG)   SIN_TABLE[(PG/(0x1000000/SIN_ENT))&(SIN_ENT-1)][EG]
#define OP_OUTN(PG,EG)  NOISE_TABLE[(PG/(0x1000000/SIN_ENT))&(SIN_ENT-1)][EG]

/* eg calcration */
#if FM_LFO_SUPPORT
#define FM_CALC_EG(OUT,SLOT)						\
{													\
	if( (SLOT.evc += SLOT.evs) >= SLOT.eve) 		\
		SLOT.eg_next(&(SLOT));						\
	OUT = SLOT.TLL+ENV_CURVE[SLOT.evc>>ENV_BITS];	\
	if(SLOT.ams)									\
		OUT += (SLOT.ams*lfo_amd/LFO_RATE);			\
}
#else
#define FM_CALC_EG(OUT,SLOT)						\
{													\
	if( (SLOT.evc += SLOT.evs) >= SLOT.eve) 		\
		SLOT.eg_next(&(SLOT));						\
	OUT = SLOT.TLL+ENV_CURVE[SLOT.evc>>ENV_BITS];	\
}
#endif

/* ---------- calcrate one of channel ---------- */
INLINE void FM_CALC_CH( FM_CH *CH )
{
	UINT32 eg_out1,eg_out2,eg_out3,eg_out4;  //envelope output

	/* Phase Generator */
#if FM_LFO_SUPPORT
	INT32 pms = lfo_pmd * CH->pms / LFO_RATE;
	if(pms)
	{
		pg_in1 = (CH->SLOT[SLOT1].Cnt += CH->SLOT[SLOT1].Incr + (INT32)(pms * CH->SLOT[SLOT1].Incr) / PMS_RATE);
		pg_in2 = (CH->SLOT[SLOT2].Cnt += CH->SLOT[SLOT2].Incr + (INT32)(pms * CH->SLOT[SLOT2].Incr) / PMS_RATE);
		pg_in3 = (CH->SLOT[SLOT3].Cnt += CH->SLOT[SLOT3].Incr + (INT32)(pms * CH->SLOT[SLOT3].Incr) / PMS_RATE);
		pg_in4 = (CH->SLOT[SLOT4].Cnt += CH->SLOT[SLOT4].Incr + (INT32)(pms * CH->SLOT[SLOT4].Incr) / PMS_RATE);
	}
	else
#endif
	{
		pg_in1 = (CH->SLOT[SLOT1].Cnt += CH->SLOT[SLOT1].Incr);
		pg_in2 = (CH->SLOT[SLOT2].Cnt += CH->SLOT[SLOT2].Incr);
		pg_in3 = (CH->SLOT[SLOT3].Cnt += CH->SLOT[SLOT3].Incr);
		pg_in4 = (CH->SLOT[SLOT4].Cnt += CH->SLOT[SLOT4].Incr);
	}

	/* Envelope Generator */
	FM_CALC_EG(eg_out1,CH->SLOT[SLOT1]);
	FM_CALC_EG(eg_out2,CH->SLOT[SLOT2]);
	FM_CALC_EG(eg_out3,CH->SLOT[SLOT3]);
	FM_CALC_EG(eg_out4,CH->SLOT[SLOT4]);

	/* Connection */
	if( eg_out1 < EG_CUT_OFF )	/* SLOT 1 */
	{
		if( CH->FB ){
			/* with self feed back */
			pg_in1 += (CH->op1_out[0]+CH->op1_out[1])>>CH->FB;
			CH->op1_out[1] = CH->op1_out[0];
		}
		CH->op1_out[0] = OP_OUT(pg_in1,eg_out1);
		/* output slot1 */
		if( !CH->connect1 )
		{
			/* algorythm 5  */
			pg_in2 += CH->op1_out[0];
			pg_in3 += CH->op1_out[0];
			pg_in4 += CH->op1_out[0];
		}else{
			/* other algorythm */
			*CH->connect1 += CH->op1_out[0];
		}
	}
	if( eg_out2 < EG_CUT_OFF )	/* SLOT 2 */
		*CH->connect2 += OP_OUT(pg_in2,eg_out2);
	if( eg_out3 < EG_CUT_OFF )	/* SLOT 3 */
		*CH->connect3 += OP_OUT(pg_in3,eg_out3);
	if( eg_out4 < EG_CUT_OFF )	/* SLOT 4 */
		*CH->connect4 += OP_OUT(pg_in4,eg_out4);
}
/* ---------- frequency counter for operater update ---------- */
INLINE void CALC_FCSLOT(FM_SLOT *SLOT , int fc , int kc )
{
	int ksr;

	/* frequency step counter */
	/* SLOT->Incr= (fc+SLOT->DT[kc])*SLOT->mul; */
	SLOT->Incr= fc*SLOT->mul + SLOT->DT[kc];
	ksr = kc >> SLOT->KSR;
	if( SLOT->ksr != ksr )
	{
		SLOT->ksr = ksr;
		/* attack , decay rate recalcration */
		SLOT->evsa = SLOT->AR[ksr];
		SLOT->evsd = SLOT->DR[ksr];
		SLOT->evss = SLOT->SR[ksr];
		SLOT->evsr = SLOT->RR[ksr];
	}
}

/* ---------- frequency counter  ---------- */
INLINE void OPN_CALC_FCOUNT(FM_CH *CH )
{
	if( CH->SLOT[SLOT1].Incr==-1){
		int fc = CH->fc;
		int kc = CH->kcode;
		CALC_FCSLOT(&CH->SLOT[SLOT1] , fc , kc );
		CALC_FCSLOT(&CH->SLOT[SLOT2] , fc , kc );
		CALC_FCSLOT(&CH->SLOT[SLOT3] , fc , kc );
		CALC_FCSLOT(&CH->SLOT[SLOT4] , fc , kc );
	}
}

/* ----------- initialize time tabls ----------- */
static void init_timetables( FM_ST *ST , UINT8 *DTTABLE , int ARRATE , int DRRATE )
{
	int i,d;
	double rate;

	/* DeTune table */
	for (d = 0;d <= 3;d++){
		for (i = 0;i <= 31;i++){
			rate = (double)DTTABLE[d*32 + i] * ST->freqbase * FREQ_RATE;
			ST->DT_TABLE[d][i]   = (INT32) rate;
			ST->DT_TABLE[d+4][i] = (INT32)-rate;
		}
	}
	/* make Attack & Decay tables */
	for (i = 0;i < 4;i++) ST->AR_TABLE[i] = ST->DR_TABLE[i] = 0;
	for (i = 4;i < 64;i++){
		rate  = ST->freqbase;						/* frequency rate */
		if( i < 60 ) rate *= 1.0+(i&3)*0.25;		/* b0-1 : x1 , x1.25 , x1.5 , x1.75 */
		rate *= 1<<((i>>2)-1);						/* b2-5 : shift bit */
		rate *= (double)(EG_ENT<<ENV_BITS);
		ST->AR_TABLE[i] = (INT32)(rate / ARRATE);
		ST->DR_TABLE[i] = (INT32)(rate / DRRATE);
	}
	ST->AR_TABLE[62] = EG_AED;
	ST->AR_TABLE[63] = EG_AED;
	for (i = 64;i < 94 ;i++){	/* make for overflow area */
		ST->AR_TABLE[i] = ST->AR_TABLE[63];
		ST->DR_TABLE[i] = ST->DR_TABLE[63];
	}

#if 0
	for (i = 0;i < 64 ;i++){
		LOG(LOG_WAR,("rate %2d , ar %f ms , dr %f ms \n",i,
			((double)(EG_ENT<<ENV_BITS) / ST->AR_TABLE[i]) * (1000.0 / ST->rate),
			((double)(EG_ENT<<ENV_BITS) / ST->DR_TABLE[i]) * (1000.0 / ST->rate) ));
	}
#endif
}

/* ---------- reset one of channel  ---------- */
static void reset_channel( FM_ST *ST , FM_CH *CH , int chan )
{
	int c,s;

	ST->mode   = 0;	/* normal mode */
	FM_STATUS_RESET(ST,0xff);
	ST->TA     = 0;
	ST->TAC    = 0;
	ST->TB     = 0;
	ST->TBC    = 0;

	for( c = 0 ; c < chan ; c++ )
	{
		CH[c].fc = 0;
		CH[c].PAN = OUTD_CENTER;
		for(s = 0 ; s < 4 ; s++ )
		{
			CH[c].SLOT[s].SEG = 0;
			CH[c].SLOT[s].eg_next= FM_EG_Release;
			CH[c].SLOT[s].evc = EG_OFF;
			CH[c].SLOT[s].eve = EG_OFF+1;
			CH[c].SLOT[s].evs = 0;
		}
	}
}

/* ---------- generic table initialize ---------- */
static int FMInitTable( void )
{
	int s,t;
	double rate;
	int i,j;
	double pom;

	/* allocate total level table plus+minus section */
	TL_TABLE = (INT32 *)malloc(2*TL_MAX*sizeof(int));
	if( TL_TABLE == 0 ) return 0;
	/* make total level table */
	for (t = 0;t < TL_MAX ;t++){
		if(t >= PG_CUT_OFF)
			rate = 0;	/* under cut off area */
		else
			rate = ((1<<TL_BITS)-1)/pow(10,EG_STEP*t/20);	/* dB -> voltage */
		TL_TABLE[       t] =  (int)rate;
		TL_TABLE[TL_MAX+t] = -TL_TABLE[t];
/*		LOG(LOG_INF,("TotalLevel(%3d) = %x\n",t,TL_TABLE[t]));*/
	}

	/* make sinwave table (pointer of total level) */
	for (s = 1;s <= SIN_ENT/4;s++){
		pom = sin(2.0*PI*s/SIN_ENT); /* sin   */
		pom = 20*log10(1/pom);	     /* -> decibel */
		j = (int)(pom / EG_STEP);    /* TL_TABLE steps */
		/* cut off check */
		if(j > PG_CUT_OFF)
			j = PG_CUT_OFF;
		/* degree 0   -  90    , degree 180 -  90 : plus section */
		SIN_TABLE[          s] = SIN_TABLE[SIN_ENT/2-s] = &TL_TABLE[j];
		/* degree 180 - 270    , degree 360 - 270 : minus section */
		SIN_TABLE[SIN_ENT/2+s] = SIN_TABLE[SIN_ENT  -s] = &TL_TABLE[TL_MAX+j];
		/* LOG(LOG_INF,("sin(%3d) = %f:%f db\n",s,pom,(double)j * EG_STEP)); */
	}
	/* degree 0 = degree 180                   = off */
	SIN_TABLE[0] = SIN_TABLE[SIN_ENT/2]        = &TL_TABLE[PG_CUT_OFF];

	/* envelope counter -> envelope output table */
	for (i=0; i<EG_ENT; i++)
	{
		/* ATTACK curve */
		/* !!!!! preliminary !!!!! */
		pom = pow( ((double)(EG_ENT-1-i)/EG_ENT) , 8 ) * EG_ENT;
		/* if( pom >= EG_ENT ) pom = EG_ENT-1; */
		ENV_CURVE[i] = (int)pom;
		/* DECAY ,RELEASE curve */
		ENV_CURVE[(EG_DST>>ENV_BITS)+i]= i;
#if FM_SEG_SUPPORT
		/* DECAY UPSIDE (SSG ENV) */
		ENV_CURVE[(EG_UST>>ENV_BITS)+i]= EG_ENT-1-i;
#endif
	}
	/* off */
	ENV_CURVE[EG_OFF>>ENV_BITS]= EG_ENT-1;

	/* decay to reattack envelope converttable */
	j = EG_ENT-1;
	for (i=0; i<EG_ENT; i++)
	{
		while( j && (ENV_CURVE[j] < i) ) j--;
		DRAR_TABLE[i] = j<<ENV_BITS;
		/* LOG(LOG_INF,("DR %06X = %06X,AR=%06X\n",i,DRAR_TABLE[i],ENV_CURVE[DRAR_TABLE[i]>>ENV_BITS] )); */
	}
	return 1;
}


static void FMCloseTable( void )
{
	if( TL_TABLE ) free( TL_TABLE );
	TL_TABLE = 0;
	return;
}

/* OPN/OPM Mode  Register Write */
INLINE void FMSetMode( FM_ST *ST ,int n,int v )
{
	/* b7 = CSM MODE */
	/* b6 = 3 slot mode */
	/* b5 = reset b */
	/* b4 = reset a */
	/* b3 = timer enable b */
	/* b2 = timer enable a */
	/* b1 = load b */
	/* b0 = load a */
	ST->mode = v;

	/* reset Timer b flag */
	if( v & 0x20 )
		FM_STATUS_RESET(ST,0x02);
	/* reset Timer a flag */
	if( v & 0x10 )
		FM_STATUS_RESET(ST,0x01);
	/* load b */
	if( v & 0x02 )
	{
		if( ST->TBC == 0 )
		{
			ST->TBC = ( 256-ST->TB)<<4;
			/* External timer handler */
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,1,ST->TBC,ST->TimerBase);
		}
	}else if (ST->timermodel == FM_TIMER_INTERVAL)
	{	/* stop interbval timer */
		if( ST->TBC != 0 )
		{
			ST->TBC = 0;
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,1,0,ST->TimerBase);
		}
	}
	/* load a */
	if( v & 0x01 )
	{
		if( ST->TAC == 0 )
		{
			ST->TAC = (1024-ST->TA);
			/* External timer handler */
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,0,ST->TAC,ST->TimerBase);
		}
	}else if (ST->timermodel == FM_TIMER_INTERVAL)
	{	/* stop interbval timer */
		if( ST->TAC != 0 )
		{
			ST->TAC = 0;
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,0,0,ST->TimerBase);
		}
	}
}

/* Timer A Overflow */
INLINE void TimerAOver(FM_ST *ST)
{
	/* status set if enabled */
	if(ST->mode & 0x04) FM_STATUS_SET(ST,0x01);
	/* clear or reload the counter */
	if (ST->timermodel == FM_TIMER_INTERVAL)
	{
		ST->TAC = (1024-ST->TA);
		if (ST->Timer_Handler) (ST->Timer_Handler)(ST->index,0,ST->TAC,ST->TimerBase);
	}
	else ST->TAC = 0;
}
/* Timer B Overflow */
INLINE void TimerBOver(FM_ST *ST)
{
	/* status set if enabled */
	if(ST->mode & 0x08) FM_STATUS_SET(ST,0x02);
	/* clear or reload the counter */
	if (ST->timermodel == FM_TIMER_INTERVAL)
	{
		ST->TBC = ( 256-ST->TB)<<4;
		if (ST->Timer_Handler) (ST->Timer_Handler)(ST->index,1,ST->TBC,ST->TimerBase);
	}
	else ST->TBC = 0;
}
/* CSM Key Controll */
INLINE void CSMKeyControll(FM_CH *CH)
{
	/* all key off */
	/* FM_KEYOFF(CH,SLOT1); */
	/* FM_KEYOFF(CH,SLOT2); */
	/* FM_KEYOFF(CH,SLOT3); */
	/* FM_KEYOFF(CH,SLOT4); */
	/* total level latch */
	CH->SLOT[SLOT1].TLL = CH->SLOT[SLOT1].TL;
	CH->SLOT[SLOT2].TLL = CH->SLOT[SLOT2].TL;
	CH->SLOT[SLOT3].TLL = CH->SLOT[SLOT3].TL;
	CH->SLOT[SLOT4].TLL = CH->SLOT[SLOT4].TL;
	/* all key on */
	FM_KEYON(CH,SLOT1);
	FM_KEYON(CH,SLOT2);
	FM_KEYON(CH,SLOT3);
	FM_KEYON(CH,SLOT4);
}

#if BUILD_OPN
/***********************************************************/
/* OPN unit                                                */
/***********************************************************/

/* OPN 3slot struct */
typedef struct opn_3slot {
	UINT32  fc[3];		/* fnum3,blk3  :calcrated */
	UINT8 fn_h[3];		/* freq3 latch            */
	UINT8 kcode[3];		/* key code    :          */
}FM_3SLOT;

/* OPN/A/B common state */
typedef struct opn_f {
	UINT8 type;		/* chip type         */
	FM_ST ST;				/* general state     */
	FM_3SLOT SL3;			/* 3 slot mode state */
	FM_CH *P_CH;			/* pointer of CH     */
	UINT32 FN_TABLE[2048]; /* fnumber -> increment counter */
#if FM_LFO_SUPPORT
	/* LFO */
	UINT32 LFOCnt;
	UINT32 LFOIncr;
	UINT32 LFO_FREQ[8];/* LFO FREQ table */
#endif
} FM_OPN;

/* OPN key frequency number -> key code follow table */
/* fnum higher 4bit -> keycode lower 2bit */
static const UINT8 OPN_FKTABLE[16]={0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};

#if FM_LFO_SUPPORT
/* OPN LFO waveform table */
static INT32 OPN_LFO_wave[LFO_ENT];
#endif

static int OPNInitTable(void)
{
	int i;

#if FM_LFO_SUPPORT
	/* LFO wave table */
	for(i=0;i<LFO_ENT;i++)
	{
		OPN_LFO_wave[i]= i<LFO_ENT/2 ? i*LFO_RATE/(LFO_ENT/2) : (LFO_ENT-i)*LFO_RATE/(LFO_ENT/2);
	}
#endif
	return FMInitTable();
}

/* ---------- priscaler set(and make time tables) ---------- */
static void OPNSetPris(FM_OPN *OPN , int pris , int TimerPris, int SSGpris)
{
	int i;

	/* frequency base */
	OPN->ST.freqbase = (OPN->ST.rate) ? ((double)OPN->ST.clock / OPN->ST.rate) / pris : 0;
	/* Timer base time */
	OPN->ST.TimerBase = (double)TIME_ONE_SEC/((double)OPN->ST.clock / (double)TimerPris);
	/* SSG part  priscaler set */
	if( SSGpris ) SSGClk( OPN->ST.index, OPN->ST.clock * 2 / SSGpris );
	/* make time tables */
	init_timetables( &OPN->ST , OPN_DTTABLE , OPN_ARRATE , OPN_DRRATE );
	/* make fnumber -> increment counter table */
	for( i=0 ; i < 2048 ; i++ )
	{
		/* it is freq table for octave 7 */
		/* opn freq counter = 20bit */
		OPN->FN_TABLE[i] = (UINT32)( (double)i * OPN->ST.freqbase * FREQ_RATE * (1<<7) / 2 );
	}
#if FM_LFO_SUPPORT
	/* LFO freq. table */
	{
		/* 3.98Hz,5.56Hz,6.02Hz,6.37Hz,6.88Hz,9.63Hz,48.1Hz,72.2Hz @ 8MHz */
#define FM_LF(Hz) ((double)LFO_ENT*(1<<LFO_SHIFT)*(Hz)/(8000000.0/144))
		static const double freq_table[8] = { FM_LF(3.98),FM_LF(5.56),FM_LF(6.02),FM_LF(6.37),FM_LF(6.88),FM_LF(9.63),FM_LF(48.1),FM_LF(72.2) };
#undef FM_LF
		for(i=0;i<8;i++)
		{
			OPN->LFO_FREQ[i] = (UINT32)(freq_table[i] * OPN->ST.freqbase);
		}
	}
#endif
/*	LOG(LOG_INF,("OPN %d set priscaler %d\n",OPN->ST.index,pris));*/
}

/* ---------- write a OPN mode register 0x20-0x2f ---------- */
static void OPNWriteMode(FM_OPN *OPN, int r, int v)
{
	UINT8 c;
	FM_CH *CH;

	switch(r){
	case 0x21:	/* Test */
		break;
#if FM_LFO_SUPPORT
	case 0x22:	/* LFO FREQ (YM2608/YM2612) */
		if( OPN->type & TYPE_LFOPAN )
		{
			OPN->LFOIncr = (v&0x08) ? OPN->LFO_FREQ[v&7] : 0;
			cur_chip = NULL;
		}
		break;
#endif
	case 0x24:	/* timer A High 8*/
		OPN->ST.TA = (OPN->ST.TA & 0x03)|(((int)v)<<2);
		break;
	case 0x25:	/* timer A Low 2*/
		OPN->ST.TA = (OPN->ST.TA & 0x3fc)|(v&3);
		break;
	case 0x26:	/* timer B */
		OPN->ST.TB = v;
		break;
	case 0x27:	/* mode , timer controll */
		FMSetMode( &(OPN->ST),OPN->ST.index,v );
		break;
	case 0x28:	/* key on / off */
		c = v&0x03;
		if( c == 3 ) break;
		if( (v&0x04) && (OPN->type & TYPE_6CH) ) c+=3;
		CH = OPN->P_CH;
		CH = &CH[c];
		/* csm mode */
		/* if( c == 2 && (OPN->ST.mode & 0x80) ) break; */
		if(v&0x10) FM_KEYON(CH,SLOT1); else FM_KEYOFF(CH,SLOT1);
		if(v&0x20) FM_KEYON(CH,SLOT2); else FM_KEYOFF(CH,SLOT2);
		if(v&0x40) FM_KEYON(CH,SLOT3); else FM_KEYOFF(CH,SLOT3);
		if(v&0x80) FM_KEYON(CH,SLOT4); else FM_KEYOFF(CH,SLOT4);
/*		LOG(LOG_INF,("OPN %d:%d : KEY %02X\n",n,c,v&0xf0));*/
		break;
	}
}

/* ---------- write a OPN register (0x30-0xff) ---------- */
static void OPNWriteReg(FM_OPN *OPN, int r, int v)
{
	UINT8 c;
	FM_CH *CH;
	FM_SLOT *SLOT;

	/* 0x30 - 0xff */
	if( (c = OPN_CHAN(r)) == 3 ) return; /* 0xX3,0xX7,0xXB,0xXF */
	if( (r >= 0x100) /* && (OPN->type & TYPE_6CH) */ ) c+=3;
		CH = OPN->P_CH;
		CH = &CH[c];

	SLOT = &(CH->SLOT[OPN_SLOT(r)]);
	switch( r & 0xf0 ) {
	case 0x30:	/* DET , MUL */
		set_det_mul(&OPN->ST,CH,SLOT,v);
		break;
	case 0x40:	/* TL */
		set_tl(CH,SLOT,v,(c == 2) && (OPN->ST.mode & 0x80) );
		break;
	case 0x50:	/* KS, AR */
		set_ar_ksr(CH,SLOT,v,OPN->ST.AR_TABLE);
		break;
	case 0x60:	/*     DR */
		/* bit7 = AMS_ON ENABLE(YM2612) */
		set_dr(SLOT,v,OPN->ST.DR_TABLE);
#if FM_LFO_SUPPORT
		if( OPN->type & TYPE_LFOPAN)
		{
			SLOT->amon = v>>7;
			SLOT->ams = CH->ams * SLOT->amon;
		}
#endif
		break;
	case 0x70:	/*     SR */
		set_sr(SLOT,v,OPN->ST.DR_TABLE);
		break;
	case 0x80:	/* SL, RR */
		set_sl_rr(SLOT,v,OPN->ST.DR_TABLE);
		break;
	case 0x90:	/* SSG-EG */
#if !FM_SEG_SUPPORT
		if(v&0x08) LOG(LOG_ERR,("OPN %d,%d,%d :SSG-TYPE envelope selected (not supported )\n",OPN->ST.index,c,OPN_SLOT(r)));
#endif
		SLOT->SEG = v&0x0f;
		break;
	case 0xa0:
		switch( OPN_SLOT(r) ){
		case 0:		/* 0xa0-0xa2 : FNUM1 */
			{
				UINT32 fn  = (((UINT32)( (CH->fn_h)&7))<<8) + v;
				UINT8 blk = CH->fn_h>>3;
				/* make keyscale code */
				CH->kcode = (blk<<2)|OPN_FKTABLE[(fn>>7)];
				/* make basic increment counter 32bit = 1 cycle */
				CH->fc = OPN->FN_TABLE[fn]>>(7-blk);
				CH->SLOT[SLOT1].Incr=-1;
			}
			break;
		case 1:		/* 0xa4-0xa6 : FNUM2,BLK */
			CH->fn_h = v&0x3f;
			break;
		case 2:		/* 0xa8-0xaa : 3CH FNUM1 */
			if( r < 0x100)
			{
				UINT32 fn  = (((UINT32)(OPN->SL3.fn_h[c]&7))<<8) + v;
				UINT8 blk = OPN->SL3.fn_h[c]>>3;
				/* make keyscale code */
				OPN->SL3.kcode[c]= (blk<<2)|OPN_FKTABLE[(fn>>7)];
				/* make basic increment counter 32bit = 1 cycle */
				OPN->SL3.fc[c] = OPN->FN_TABLE[fn]>>(7-blk);
				(OPN->P_CH)[2].SLOT[SLOT1].Incr=-1;
			}
			break;
		case 3:		/* 0xac-0xae : 3CH FNUM2,BLK */
			if( r < 0x100)
				OPN->SL3.fn_h[c] = v&0x3f;
			break;
		}
		break;
	case 0xb0:
		switch( OPN_SLOT(r) ){
		case 0:		/* 0xb0-0xb2 : FB,ALGO */
			{
				int feedback = (v>>3)&7;
				CH->ALGO = v&7;
				CH->FB   = feedback ? 8+1 - feedback : 0;
				setup_connection( CH );
			}
			break;
		case 1:		/* 0xb4-0xb6 : L , R , AMS , PMS (YM2612/YM2608) */
			if( OPN->type & TYPE_LFOPAN)
			{
#if FM_LFO_SUPPORT
				/* b0-2 PMS */
				/* 0,3.4,6.7,10,14,20,40,80(cent) */
				static const double pmd_table[8]={0,3.4,6.7,10,14,20,40,80};
				static const int amd_table[4]={(int)(0/EG_STEP),(int)(1.4/EG_STEP),(int)(5.9/EG_STEP),(int)(11.8/EG_STEP) };
				CH->pms = (INT32)( (1.5/1200.0)*pmd_table[v & 7] * PMS_RATE);
				/* b4-5 AMS */
				/* 0 , 1.4 , 5.9 , 11.8(dB) */
				CH->ams = amd_table[(v>>4) & 0x03];
				CH->SLOT[SLOT1].ams = CH->ams * CH->SLOT[SLOT1].amon;
				CH->SLOT[SLOT2].ams = CH->ams * CH->SLOT[SLOT2].amon;
				CH->SLOT[SLOT3].ams = CH->ams * CH->SLOT[SLOT3].amon;
				CH->SLOT[SLOT4].ams = CH->ams * CH->SLOT[SLOT4].amon;
#endif
				/* PAN */
				CH->PAN = (v>>6)&0x03; /* PAN : b6 = R , b7 = L */
				setup_connection( CH );
				/* LOG(LOG_INF,("OPN %d,%d : PAN %d\n",n,c,CH->PAN));*/
			}
			break;
		}
		break;
	}
}
#endif /* BUILD_OPN */

#if BUILD_YM2203
/*******************************************************************************/
/*		YM2203 local section                                                   */
/*******************************************************************************/

/* here's the virtual YM2203(OPN) */
typedef struct ym2203_f {
	FM_OPN OPN;				/* OPN state         */
	FM_CH CH[3];			/* channel state     */
} YM2203;

static YM2203 *FM2203=NULL;	/* array of YM2203's */
static int YM2203NumChips;	/* total chip */

/* ---------- update one of chip ----------- */
void YM2203UpdateOne(int num, INT16 *buffer, int length)
{
	YM2203 *F2203 = &(FM2203[num]);
	FM_OPN *OPN =   &(FM2203[num].OPN);
	int i;
	FM_CH *ch;
	FMSAMPLE *buf = buffer;

	cur_chip = (void *)F2203;
	State = &F2203->OPN.ST;
	cch[0]   = &F2203->CH[0];
	cch[1]   = &F2203->CH[1];
	cch[2]   = &F2203->CH[2];
#if FM_LFO_SUPPORT
	/* LFO */
	lfo_amd = lfo_pmd = 0;
#endif
	/* frequency counter channel A */
	OPN_CALC_FCOUNT( cch[0] );
	/* frequency counter channel B */
	OPN_CALC_FCOUNT( cch[1] );
	/* frequency counter channel C */
	if( (State->mode & 0xc0) ){
		/* 3SLOT MODE */
		if( cch[2]->SLOT[SLOT1].Incr==-1){
			/* 3 slot mode */
			CALC_FCSLOT(&cch[2]->SLOT[SLOT1] , OPN->SL3.fc[1] , OPN->SL3.kcode[1] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT2] , OPN->SL3.fc[2] , OPN->SL3.kcode[2] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT3] , OPN->SL3.fc[0] , OPN->SL3.kcode[0] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT4] , cch[2]->fc , cch[2]->kcode );
		}
	}else OPN_CALC_FCOUNT( cch[2] );

    for( i=0; i < length ; i++ )
	{
		/*            channel A         channel B         channel C      */
		out_ch[OUTD_CENTER] = 0;
		/* calcrate FM */
		for( ch=cch[0] ; ch <= cch[2] ; ch++)
			FM_CALC_CH( ch );
		/* limit check */
		Limit( out_ch[OUTD_CENTER] , FM_MAXOUT, FM_MINOUT );
		/* store to sound buffer */
		buf[i] = out_ch[OUTD_CENTER] >> FM_OUTSB;
		/* timer controll */
		INTERNAL_TIMER_A( State , cch[2] )
	}
	INTERNAL_TIMER_B(State,length)
}

/* ---------- reset one of chip ---------- */
void YM2203ResetChip(int num)
{
	int i;
	FM_OPN *OPN = &(FM2203[num].OPN);

	/* Reset Priscaler */
	OPNSetPris( OPN , 6*12 , 6*12 ,4); /* 1/6 , 1/4 */
	/* reset SSG section */
	SSGReset(OPN->ST.index);
	/* status clear */
	FM_IRQMASK_SET(&OPN->ST,0x03);
	OPNWriteMode(OPN,0x27,0x30); /* mode 0 , timer reset */
	reset_channel( &OPN->ST , FM2203[num].CH , 3 );
	/* reset OPerator paramater */
	for(i = 0xb6 ; i >= 0xb4 ; i-- ) OPNWriteReg(OPN,i,0xc0); /* PAN RESET */
	for(i = 0xb2 ; i >= 0x30 ; i-- ) OPNWriteReg(OPN,i,0);
	for(i = 0x26 ; i >= 0x20 ; i-- ) OPNWriteReg(OPN,i,0);
}

/* ----------  Initialize YM2203 emulator(s) ----------    */
/* 'num' is the number of virtual YM2203's to allocate     */
/* 'rate' is sampling rate and 'bufsiz' is the size of the */
/* buffer that should be updated at each interval          */
int YM2203Init(int num, int clock, int rate,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler)
{
	int i;

	if (FM2203) return (-1);	/* duplicate init. */
	cur_chip = NULL;	/* hiro-shi!! */

	YM2203NumChips = num;

	/* allocate ym2203 state space */
	if( (FM2203 = (YM2203 *)malloc(sizeof(YM2203) * YM2203NumChips))==NULL)
		return (-1);
	/* clear */
	memset(FM2203,0,sizeof(YM2203) * YM2203NumChips);
	/* allocate total level table (128kb space) */
	if( !OPNInitTable() )
	{
		free( FM2203 );
		return (-1);
	}
	for ( i = 0 ; i < YM2203NumChips; i++ ) {
		FM2203[i].OPN.ST.index = i;
		FM2203[i].OPN.type = TYPE_YM2203;
		FM2203[i].OPN.P_CH = FM2203[i].CH;
		FM2203[i].OPN.ST.clock = clock;
		FM2203[i].OPN.ST.rate = rate;
		/* FM2203[i].OPN.ST.irq = 0; */
		/* FM2203[i].OPN.ST.satus = 0; */
		FM2203[i].OPN.ST.timermodel = FM_TIMER_INTERVAL;
		/* Extend handler */
		FM2203[i].OPN.ST.Timer_Handler = TimerHandler;
		FM2203[i].OPN.ST.IRQ_Handler   = IRQHandler;
		YM2203ResetChip(i);
	}
	return(0);
}

/* ---------- shut down emurator ----------- */
void YM2203Shutdown(void)
{
    if (!FM2203) return;

	FMCloseTable();
	free(FM2203);
	FM2203 = NULL;
}

/* ---------- YM2203 I/O interface ---------- */
int YM2203Write(int n,int a,UINT8 v)
{
	FM_OPN *OPN = &(FM2203[n].OPN);

	if( !(a&1) )
	{	/* address port */
		OPN->ST.address = v & 0xff;
		/* Write register to SSG emurator */
		if( v < 16 ) SSGWrite(n,0,v);
		switch(OPN->ST.address)
		{
		case 0x2d:	/* divider sel */
			OPNSetPris( OPN, 6*12, 6*12 ,4); /* OPN 1/6 , SSG 1/4 */
			break;
		case 0x2e:	/* divider sel */
			OPNSetPris( OPN, 3*12, 3*12,2); /* OPN 1/3 , SSG 1/2 */
			break;
		case 0x2f:	/* divider sel */
			OPNSetPris( OPN, 2*12, 2*12,1); /* OPN 1/2 , SSG 1/1 */
			break;
		}
	}
	else
	{	/* data port */
		int addr = OPN->ST.address;
		switch( addr & 0xf0 )
		{
		case 0x00:	/* 0x00-0x0f : SSG section */
			/* Write data to SSG emurator */
			SSGWrite(n,a,v);
			break;
		case 0x20:	/* 0x20-0x2f : Mode section */
			YM2203UpdateReq(n);
			/* write register */
			 OPNWriteMode(OPN,addr,v);
			break;
		default:	/* 0x30-0xff : OPN section */
			YM2203UpdateReq(n);
			/* write register */
			 OPNWriteReg(OPN,addr,v);
		}
	}
	return OPN->ST.irq;
}

UINT8 YM2203Read(int n,int a)
{
	YM2203 *F2203 = &(FM2203[n]);
	int addr = F2203->OPN.ST.address;
	int ret = 0;

	if( !(a&1) )
	{	/* status port */
		ret = F2203->OPN.ST.status;
	}
	else
	{	/* data port (ONLY SSG) */
		if( addr < 16 ) ret = SSGRead(n);
	}
	return ret;
}

int YM2203TimerOver(int n,int c)
{
	YM2203 *F2203 = &(FM2203[n]);

	if( c )
	{	/* Timer B */
		TimerBOver( &(F2203->OPN.ST) );
	}
	else
	{	/* Timer A */
		YM2203UpdateReq(n);
		/* timer update */
		TimerAOver( &(F2203->OPN.ST) );
		/* CSM mode key,TL controll */
		if( F2203->OPN.ST.mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			CSMKeyControll( &(F2203->CH[2]) );
		}
	}
	return F2203->OPN.ST.irq;
}

#endif /* BUILD_YM2203 */

#if (BUILD_YM2608||BUILD_OPNB)
/* adpcm type A struct */
typedef struct adpcm_state {
	UINT8 flag;			/* port state        */
	UINT8 flagMask;		/* arrived flag mask */
	UINT8 now_data;
	UINT32 now_addr;
	UINT32 now_step;
	UINT32 step;
	UINT32 start;
	UINT32 end;
	int IL;
	int volume;					/* calcrated mixing level */
	INT32 *pan;					/* &out_ch[OPN_xxxx] */
	int /*adpcmm,*/ adpcmx, adpcmd;
	int adpcml;					/* hiro-shi!! */
}ADPCM_CH;

/* here's the virtual YM2610 */
typedef struct ym2610_f {
	FM_OPN OPN;				/* OPN state    */
	FM_CH CH[6];			/* channel state */
	int address1;			/* address register1 */
	/* ADPCM-A unit */
	UINT8 *pcmbuf;			/* pcm rom buffer */
	UINT32 pcm_size;			/* size of pcm rom */
	INT32 *adpcmTL;					/* adpcmA total level */
	ADPCM_CH adpcm[6];				/* adpcm channels */
	UINT32 adpcmreg[0x30];	/* registers */
	UINT8 adpcm_arrivedEndAddress;
	/* Delta-T ADPCM unit */
	YM_DELTAT deltaT;
} YM2610;

/* here's the virtual YM2608 */
typedef YM2610 YM2608;
#endif /* (BUILD_YM2608||BUILD_OPNB) */

#if BUILD_FM_ADPCMA
/***************************************************************/
/*    ADPCMA units are made by Hiromitsu Shioya (MMSND)         */
/***************************************************************/

/**** YM2610 ADPCM defines ****/
#define ADPCMA_MIXING_LEVEL  (3) /* ADPCMA mixing level   */
#define ADPCM_SHIFT    (16)      /* frequency step rate   */
#define ADPCMA_ADDRESS_SHIFT 8   /* adpcm A address shift */

//#define ADPCMA_DECODE_RANGE 1024
#define ADPCMA_DECODE_RANGE 2048
#define ADPCMA_DECODE_MIN (-(ADPCMA_DECODE_RANGE*ADPCMA_MIXING_LEVEL))
#define ADPCMA_DECODE_MAX ((ADPCMA_DECODE_RANGE*ADPCMA_MIXING_LEVEL)-1)
#define ADPCMA_VOLUME_DIV 1

static UINT8 *pcmbufA;
static UINT32 pcmsizeA;

/************************************************************/
/************************************************************/
/* --------------------- subroutines  --------------------- */
/************************************************************/
/************************************************************/
/************************/
/*    ADPCM A tables    */
/************************/
static int jedi_table[(48+1)*16];
static int decode_tableA1[16] = {
  -1*16, -1*16, -1*16, -1*16, 2*16, 5*16, 7*16, 9*16,
  -1*16, -1*16, -1*16, -1*16, 2*16, 5*16, 7*16, 9*16
};

/* 0.9 , 0.9 , 0.9 , 0.9 , 1.2 , 1.6 , 2.0 , 2.4 */
/* 8 = -1 , 2 5 8 11 */
/* 9 = -1 , 2 5 9 13 */
/* 10= -1 , 2 6 10 14 */
/* 12= -1 , 2 7 12 17 */
/* 20= -2 , 4 12 20 32 */

#if 1
static void InitOPNB_ADPCMATable(void){
	int step, nib;

	for (step = 0; step <= 48; step++)
	{
		double stepval = floor(16.0 * pow (11.0 / 10.0, (double)step) * ADPCMA_MIXING_LEVEL);
		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			int value = (int)stepval*((nib&0x07)*2+1)/8;
			jedi_table[step*16+nib] = (nib&0x08) ? -value : value;
		}
	}
}
#else
static int decode_tableA2[49] = {
  0x0010, 0x0011, 0x0013, 0x0015, 0x0017, 0x0019, 0x001c, 0x001f,
  0x0022, 0x0025, 0x0029, 0x002d, 0x0032, 0x0037, 0x003c, 0x0042,
  0x0049, 0x0050, 0x0058, 0x0061, 0x006b, 0x0076, 0x0082, 0x008f,
  0x009d, 0x00ad, 0x00be, 0x00d1, 0x00e6, 0x00fd, 0x0117, 0x0133,
  0x0151, 0x0173, 0x0198, 0x01c1, 0x01ee, 0x0220, 0x0256, 0x0292,
  0x02d4, 0x031c, 0x036c, 0x03c3, 0x0424, 0x048e, 0x0502, 0x0583,
  0x0610
};
static void InitOPNB_ADPCMATable(void){
   int ta,tb,tc;
   for(ta=0;ta<49;ta++){
     for(tb=0;tb<16;tb++){
       tc=0;
       if(tb&0x04){tc+=((decode_tableA2[ta]*ADPCMA_MIXING_LEVEL));}
       if(tb&0x02){tc+=((decode_tableA2[ta]*ADPCMA_MIXING_LEVEL)>>1);}
       if(tb&0x01){tc+=((decode_tableA2[ta]*ADPCMA_MIXING_LEVEL)>>2);}
       tc+=((decode_tableA2[ta]*ADPCMA_MIXING_LEVEL)>>3);
       if(tb&0x08){tc=(0-tc);}
       jedi_table[ta*16+tb]=tc;
     }
   }
}
#endif

/**** ADPCM A (Non control type) ****/
INLINE void OPNB_ADPCM_CALC_CHA( YM2610 *F2610, ADPCM_CH *ch )
{
	UINT32 step;
	int data;

	ch->now_step += ch->step;
	if ( ch->now_step >= (1<<ADPCM_SHIFT) )
	{
		step = ch->now_step >> ADPCM_SHIFT;
		ch->now_step &= (1<<ADPCM_SHIFT)-1;
		/* end check */
		if ( (ch->now_addr+step) > (ch->end<<1) ) {
			ch->flag = 0;
			F2610->adpcm_arrivedEndAddress |= ch->flagMask;
			return;
		}
		do{
#if 0
			if ( ch->now_addr > (pcmsizeA<<1) ) {
				LOG(LOG_WAR,("YM2610: Attempting to play past adpcm rom size!\n" ));
				return;
			}
#endif
			if( ch->now_addr&1 ) data = ch->now_data & 0x0f;
			else
			{
				ch->now_data = *(pcmbufA+(ch->now_addr>>1));
				data = (ch->now_data >> 4)&0x0f;
			}
			ch->now_addr++;

			ch->adpcmx += jedi_table[ch->adpcmd+data];
			Limit( ch->adpcmx,ADPCMA_DECODE_MAX, ADPCMA_DECODE_MIN );
			ch->adpcmd += decode_tableA1[data];
			Limit( ch->adpcmd, 48*16, 0*16 );
			/**** calc pcm * volume data ****/
			ch->adpcml = ch->adpcmx * ch->volume;
		}while(--step);
	}
	/* output for work of output channels (out_ch[OPNxxxx])*/
	*(ch->pan) += ch->adpcml;
}

/* ADPCM type A */
static void FM_ADPCMAWrite(YM2610 *F2610,int r,int v)
{
	ADPCM_CH *adpcm = F2610->adpcm;
	UINT8 c = r&0x07;

	F2610->adpcmreg[r] = v&0xff; /* stock data */
	switch( r ){
	case 0x00: /* DM,--,C5,C4,C3,C2,C1,C0 */
		/* F2610->port1state = v&0xff; */
		if( !(v&0x80) ){
			/* KEY ON */
			for( c = 0; c < 6; c++ ){
				if( (1<<c)&v ){
					/**** start adpcm ****/
					adpcm[c].step     = (UINT32)((float)(1<<ADPCM_SHIFT)*((float)F2610->OPN.ST.freqbase)/3.0);
					adpcm[c].now_addr = adpcm[c].start<<1;
					adpcm[c].now_step = (1<<ADPCM_SHIFT)-adpcm[c].step;
					/*adpcm[c].adpcmm   = 0;*/
					adpcm[c].adpcmx   = 0;
					adpcm[c].adpcmd   = 0;
					adpcm[c].adpcml   = 0;
					adpcm[c].flag     = 1;
					if(F2610->pcmbuf==NULL){			// Check ROM Mapped
						LOG(LOG_WAR,("YM2610: ADPCM-A rom not mapped\n"));
						adpcm[c].flag = 0;
					} else{
						if(adpcm[c].end >= F2610->pcm_size){		// Check End in Range
							LOG(LOG_WAR,("YM2610: ADPCM-A end out of range: $%08x\n",adpcm[c].end));
							adpcm[c].end = F2610->pcm_size-1;
						}
						if(adpcm[c].start >= F2610->pcm_size)
						{	// Check Start in Range
							LOG(LOG_WAR,("YM2610: ADPCM-A start out of range: $%08x\n",adpcm[c].start));
							adpcm[c].flag = 0;
						}
/*LOG(LOG_WAR,("YM2610: Start %06X : %02X %02X %02X\n",adpcm[c].start,
pcmbufA[adpcm[c].start],pcmbufA[adpcm[c].start+1],pcmbufA[adpcm[c].start+2]));*/
					}
				}	/*** (1<<c)&v ***/
			}	/**** for loop ****/
		} else{
			/* KEY OFF */
			for( c = 0; c < 6; c++ ){
				if( (1<<c)&v )  adpcm[c].flag = 0;
			}
		}
		break;
	case 0x01:	/* B0-5 = TL 0.75dB step */
		F2610->adpcmTL = &(TL_TABLE[((v&0x3f)^0x3f)*(int)(0.75/EG_STEP)]);
		for( c = 0; c < 6; c++ )
		{
			adpcm[c].volume = F2610->adpcmTL[adpcm[c].IL*(int)(0.75/EG_STEP)] / ADPCMA_DECODE_RANGE / ADPCMA_VOLUME_DIV;
			/**** calc pcm * volume data ****/
			adpcm[c].adpcml = adpcm[c].adpcmx * adpcm[c].volume;
		}
		break;
	default:
		c = r&0x07;
		if( c >= 0x06 ) return;
		switch( r&0x38 ){
		case 0x08:	/* B7=L,B6=R,B4-0=IL */
			adpcm[c].IL = (v&0x1f)^0x1f;
			adpcm[c].volume = F2610->adpcmTL[adpcm[c].IL*(int)(0.75/EG_STEP)] / ADPCMA_DECODE_RANGE / ADPCMA_VOLUME_DIV;
			adpcm[c].pan    = &out_ch[(v>>6)&0x03];
			/**** calc pcm * volume data ****/
			adpcm[c].adpcml = adpcm[c].adpcmx * adpcm[c].volume;
			break;
		case 0x10:
		case 0x18:
			adpcm[c].start  = ( (F2610->adpcmreg[0x18 + c]*0x0100 | F2610->adpcmreg[0x10 + c]) << ADPCMA_ADDRESS_SHIFT);
			break;
		case 0x20:
		case 0x28:
			adpcm[c].end    = ( (F2610->adpcmreg[0x28 + c]*0x0100 | F2610->adpcmreg[0x20 + c]) << ADPCMA_ADDRESS_SHIFT);
			adpcm[c].end   += (1<<ADPCMA_ADDRESS_SHIFT) - 1;
			break;
		}
	}
}

#endif /* BUILD_FM_ADPCMA */

#if BUILD_YM2608
/*******************************************************************************/
/*		YM2608 local section                                                   */
/*******************************************************************************/
static YM2608 *FM2608=NULL;	/* array of YM2608's */
static int YM2608NumChips;	/* total chip */

/* YM2608 Rhythm Number */
#define RY_BD  0
#define RY_SD  1
#define RY_TOP 2
#define RY_HH  3
#define RY_TOM 4
#define RY_RIM 5

#if 0
/* Get next pcm data */
INLINE int YM2608ReadADPCM(int n)
{
	YM2608 *F2608 = &(FM2608[n]);
	if( F2608->ADMode & 0x20 )
	{	/* buffer memory */
		/* F2203->OPN.ST.status |= 0x04; */
		return 0;
	}
	else
	{	/* from PCM data register */
		FM_STATUS_SET(F2608->OPN.ST,0x08); /* BRDY = 1 */
		return F2608->ADData;
	}
}

/* Put decoded data */
INLINE void YM2608WriteADPCM(int n,int v)
{
	YM2608 *F2608 = &(FM2608[n]);
	if( F2608->ADMode & 0x20 )
	{	/* for buffer */
		return;
	}
	else
	{	/* for PCM data port */
		F2608->ADData = v;
		FM_STATUS_SET(F2608->OPN.ST,0x08) /* BRDY = 1 */
	}
}
#endif

/* ---------- IRQ flag Controll Write 0x110 ---------- */
INLINE void YM2608IRQFlagWrite(FM_ST *ST,int n,int v)
{
	if( v & 0x80 )
	{	/* Reset IRQ flag */
		FM_STATUS_RESET(ST,0xff);
	}
	else
	{	/* Set IRQ mask */
		/* !!!!!!!!!! pending !!!!!!!!!! */
	}
}

#ifdef YM2608_RHYTHM_PCM
/**** RYTHM (PCM) ****/
INLINE void YM2608_RYTHM( YM2608 *F2608, ADPCM_CH *ch )

{
	UINT32 step;

	ch->now_step += ch->step;
	if ( ch->now_step >= (1<<ADPCM_SHIFT) )
	{
		step = ch->now_step >> ADPCM_SHIFT;
		ch->now_step &= (1<<ADPCM_SHIFT)-1;
		/* end check */
		if ( (ch->now_addr+step) > (ch->end<<1) ) {
			ch->flag = 0;
			F2608->adpcm_arrivedEndAddress |= ch->flagMask;
			return;
		}
		do{
			/* get a next pcm data */
			ch->adpcmx = ((short *)pcmbufA)[ch->now_addr];
			ch->now_addr++;
			/**** calc pcm * volume data ****/
			ch->adpcml = ch->adpcmx * ch->volume;
		}while(--step);
	}
	/* output for work of output channels (out_ch[OPNxxxx])*/
	*(ch->pan) += ch->adpcml;
}
#endif /* YM2608_RHYTHM_PCM */

/* ---------- update one of chip ----------- */
void YM2608UpdateOne(int num, INT16 **buffer, int length)
{
	YM2608 *F2608 = &(FM2608[num]);
	FM_OPN *OPN   = &(FM2608[num].OPN);
	YM_DELTAT *DELTAT = &(F2608[num].deltaT);
	int i,j;
	FM_CH *ch;
	FMSAMPLE  *bufL,*bufR;

	/* setup DELTA-T unit */
	YM_DELTAT_DECODE_PRESET(DELTAT);
	DELTAT->arrivedFlag = 0;	/* ASG */
	DELTAT->flagMask = 1;	/* ASG */

	/* set bufer */
	bufL = buffer[0];
	bufR = buffer[1];

	if( (void *)F2608 != cur_chip ){
		cur_chip = (void *)F2608;

		State = &OPN->ST;
		cch[0]   = &F2608->CH[0];
		cch[1]   = &F2608->CH[1];
		cch[2]   = &F2608->CH[2];
		cch[3]   = &F2608->CH[3];
		cch[4]   = &F2608->CH[4];
		cch[5]   = &F2608->CH[5];
		/* setup adpcm rom address */
		pcmbufA  = F2608->pcmbuf;
		pcmsizeA = F2608->pcm_size;
#if FM_LFO_SUPPORT
		LFOCnt  = OPN->LFOCnt;
		LFOIncr = OPN->LFOIncr;
		if( !LFOIncr ) lfo_amd = lfo_pmd = 0;
#endif
	}
	/* update frequency counter */
	OPN_CALC_FCOUNT( cch[0] );
	OPN_CALC_FCOUNT( cch[1] );
	if( (State->mode & 0xc0) ){
		/* 3SLOT MODE */
		if( cch[2]->SLOT[SLOT1].Incr==-1){
			/* 3 slot mode */
			CALC_FCSLOT(&cch[2]->SLOT[SLOT1] , OPN->SL3.fc[1] , OPN->SL3.kcode[1] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT2] , OPN->SL3.fc[2] , OPN->SL3.kcode[2] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT3] , OPN->SL3.fc[0] , OPN->SL3.kcode[0] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT4] , cch[2]->fc , cch[2]->kcode );
		}
	}else OPN_CALC_FCOUNT( cch[2] );
	OPN_CALC_FCOUNT( cch[3] );
	OPN_CALC_FCOUNT( cch[4] );
	OPN_CALC_FCOUNT( cch[5] );
	/* buffering */
    for( i=0; i < length ; i++ )
	{
#if FM_LFO_SUPPORT
		/* LFO */
		if( LFOIncr )
		{
			lfo_amd = OPN_LFO_wave[(LFOCnt+=LFOIncr)>>LFO_SHIFT];
			lfo_pmd = lfo_amd-(LFO_RATE/2);
		}
#endif
		/* clear output acc. */
		out_ch[OUTD_LEFT] = out_ch[OUTD_RIGHT]= out_ch[OUTD_CENTER] = 0;
		/**** deltaT ADPCM ****/
		if( DELTAT->flag )
			YM_DELTAT_ADPCM_CALC(DELTAT);
		/* FM */
		for(ch = cch[0] ; ch <= cch[5] ; ch++)
			FM_CALC_CH( ch );
		for( j = 0; j < 6; j++ )
		{
			/**** ADPCM ****/
			if( F2608->adpcm[j].flag )
#ifdef YM2608_RHYTHM_PCM
				YM2608_RYTHM(F2608, &F2608->adpcm[j]);
#else
				OPNB_ADPCM_CALC_CHA( F2608, &F2608->adpcm[j]);
#endif
		}
		/* buffering */
		FM_BUFFERING_STEREO;
		/* timer A controll */
		INTERNAL_TIMER_A( State , cch[2] )
	}
	INTERNAL_TIMER_B(State,length)
	if (DELTAT->arrivedFlag) FM_STATUS_SET(State, 0x04);	/* ASG */

#if FM_LFO_SUPPORT
	OPN->LFOCnt = LFOCnt;
#endif
}

/* -------------------------- YM2608(OPNA) ---------------------------------- */
int YM2608Init(int num, int clock, int rate,
               void **pcmrom,int *pcmsize,short *rhythmrom,int *rhythmpos,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler)
{
	int i,j;

    if (FM2608) return (-1);	/* duplicate init. */
    cur_chip = NULL;	/* hiro-shi!! */

	YM2608NumChips = num;

	/* allocate extend state space */
	if( (FM2608 = (YM2608 *)malloc(sizeof(YM2608) * YM2608NumChips))==NULL)
		return (-1);
	/* clear */
	memset(FM2608,0,sizeof(YM2608) * YM2608NumChips);
	/* allocate total level table (128kb space) */
	if( !OPNInitTable() )
	{
		free( FM2608 );
		return (-1);
	}

	for ( i = 0 ; i < YM2608NumChips; i++ ) {
		FM2608[i].OPN.ST.index = i;
		FM2608[i].OPN.type = TYPE_YM2608;
		FM2608[i].OPN.P_CH = FM2608[i].CH;
		FM2608[i].OPN.ST.clock = clock;
		FM2608[i].OPN.ST.rate = rate;
		/* FM2608[i].OPN.ST.irq = 0; */
		/* FM2608[i].OPN.ST.status = 0; */
		FM2608[i].OPN.ST.timermodel = FM_TIMER_INTERVAL;
		/* Extend handler */
		FM2608[i].OPN.ST.Timer_Handler = TimerHandler;
		FM2608[i].OPN.ST.IRQ_Handler   = IRQHandler;
		/* DELTA-T */
		FM2608[i].deltaT.memory = (UINT8 *)(pcmrom[i]);
		FM2608[i].deltaT.memory_size = pcmsize[i];
		/* ADPCM(Rythm) */
		FM2608[i].pcmbuf   = (UINT8 *)rhythmrom;
#ifdef YM2608_RHYTHM_PCM
		/* rhythm sound setup (PCM) */
		for(j=0;j<6;j++)
		{
			/* rhythm sound */
			FM2608[i].adpcm[j].start = rhythmpos[j];
			FM2608[i].adpcm[j].end   = rhythmpos[j+1]-1;
		}
		FM2608[i].pcm_size = rhythmpos[6];
#else
		/* rhythm sound setup (ADPCM) */
		FM2608[i].pcm_size = rhythmsize;
#endif
		YM2608ResetChip(i);
	}
	InitOPNB_ADPCMATable();
	return 0;
}

/* ---------- shut down emurator ----------- */
void YM2608Shutdown()
{
    if (!FM2608) return;

	FMCloseTable();
	free(FM2608);
	FM2608 = NULL;
}

/* ---------- reset one of chip ---------- */
void YM2608ResetChip(int num)
{
	int i;
	YM2608 *F2608 = &(FM2608[num]);
	FM_OPN *OPN   = &(FM2608[num].OPN);
	YM_DELTAT *DELTAT = &(F2608[num].deltaT);

	/* Reset Priscaler */
	OPNSetPris( OPN, 6*24, 6*24,4*2); /* OPN 1/6 , SSG 1/4 */
	/* reset SSG section */
	SSGReset(OPN->ST.index);
	/* status clear */
	FM_IRQMASK_SET(&OPN->ST,0x1f);
	OPNWriteMode(OPN,0x27,0x30); /* mode 0 , timer reset */

	/* extend 3ch. disable */
	//OPN->type &= (~TYPE_6CH);

	reset_channel( &OPN->ST , F2608->CH , 6 );
	/* reset OPerator paramater */
	for(i = 0xb6 ; i >= 0xb4 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0xc0);
		OPNWriteReg(OPN,i|0x100,0xc0);
	}
	for(i = 0xb2 ; i >= 0x30 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0);
		OPNWriteReg(OPN,i|0x100,0);
	}
	for(i = 0x26 ; i >= 0x20 ; i-- ) OPNWriteReg(OPN,i,0);
	/* reset ADPCM unit */
	/**** ADPCM work initial ****/
	for( i = 0; i < 6+1; i++ ){
		F2608->adpcm[i].now_addr  = 0;
		F2608->adpcm[i].now_step  = 0;
		F2608->adpcm[i].step      = 0;
		F2608->adpcm[i].start     = 0;
		F2608->adpcm[i].end       = 0;
		/* F2608->adpcm[i].delta     = 21866; */
		F2608->adpcm[i].volume    = 0;
		F2608->adpcm[i].pan       = &out_ch[OUTD_CENTER]; /* default center */
		F2608->adpcm[i].flagMask  = (i == 6) ? 0x20 : 0;
		F2608->adpcm[i].flag      = 0;
		F2608->adpcm[i].adpcmx    = 0;
		F2608->adpcm[i].adpcmd    = 127;
		F2608->adpcm[i].adpcml    = 0;
	}
	F2608->adpcmTL = &(TL_TABLE[0x3f*(int)(0.75/EG_STEP)]);
	/* F2608->port1state = -1; */
	F2608->adpcm_arrivedEndAddress = 0; /* don't used */

	/* DELTA-T unit */
	DELTAT->freqbase = OPN->ST.freqbase;
	DELTAT->output_pointer = out_ch;
	DELTAT->portshift = 5;		/* allways 5bits shift */ /* ASG */
	DELTAT->output_range = DELTAT_MIXING_LEVEL<<TL_BITS;
	YM_DELTAT_ADPCM_Reset(DELTAT,OUTD_CENTER);
}

/* YM2608 write */
/* n = number  */
/* a = address */
/* v = value   */
int YM2608Write(int n, int a,UINT8 v)
{
	YM2608 *F2608 = &(FM2608[n]);
	FM_OPN *OPN   = &(FM2608[n].OPN);
	int addr;

	switch(a&3){
	case 0:	/* address port 0 */
		OPN->ST.address = v & 0xff;
		/* Write register to SSG emurator */
		if( v < 16 ) SSGWrite(n,0,v);
		switch(OPN->ST.address)
		{
		case 0x2d:	/* divider sel */
			OPNSetPris( OPN, 6*24, 6*24, 4*2); /* OPN 1/6 , SSG 1/4 */
			F2608->deltaT.freqbase = OPN->ST.freqbase;
			break;
		case 0x2e:	/* divider sel */
			OPNSetPris( OPN, 3*24, 3*24,2*2); /* OPN 1/3 , SSG 1/2 */
			F2608->deltaT.freqbase = OPN->ST.freqbase;
			break;
		case 0x2f:	/* divider sel */
			OPNSetPris( OPN, 2*24, 2*24,1*2); /* OPN 1/2 , SSG 1/1 */
			F2608->deltaT.freqbase = OPN->ST.freqbase;
			break;
		}
		break;
	case 1:	/* data port 0    */
		addr = OPN->ST.address;
		switch(addr & 0xf0)
		{
		case 0x00:	/* SSG section */
			/* Write data to SSG emurator */
			SSGWrite(n,a,v);
			break;
		case 0x10:	/* 0x10-0x1f : Rhythm section */
			YM2608UpdateReq(n);
			FM_ADPCMAWrite(F2608,addr-0x10,v);
			break;
		case 0x20:	/* Mode Register */
			switch(addr)
			{
			case 0x29: /* SCH,xirq mask */
				/* SCH,xx,xxx,EN_ZERO,EN_BRDY,EN_EOS,EN_TB,EN_TA */
				/* extend 3ch. enable/disable */
				if(v&0x80) OPN->type |= TYPE_6CH;
				else       OPN->type &= ~TYPE_6CH;
				/* IRQ MASK */
				FM_IRQMASK_SET(&OPN->ST,v&0x1f);
				break;
			default:
				YM2608UpdateReq(n);
				OPNWriteMode(OPN,addr,v);
			}
			break;
		default:	/* OPN section */
			YM2608UpdateReq(n);
			OPNWriteReg(OPN,addr,v);
		}
		break;
	case 2:	/* address port 1 */
		F2608->address1 = v & 0xff;
		break;
	case 3:	/* data port 1    */
		addr = F2608->address1;
		YM2608UpdateReq(n);
		switch( addr & 0xf0 )
		{
		case 0x00:	/* ADPCM PORT */
			switch( addr )
			{
			case 0x0c:	/* Limit address L */
				//F2608->ADLimit = (F2608->ADLimit & 0xff00) | v;
				//break;
			case 0x0d:	/* Limit address H */
				//F2608->ADLimit = (F2608->ADLimit & 0x00ff) | (v<<8);
				//break;
			case 0x0e:	/* DAC data */
				//break;
			case 0x0f:	/* PCM data port */
				//F2608->ADData = v;
				//FM_STATUS_RESET(F2608->OPN.ST,0x08);
				break;
			default:
				/* 0x00-0x0b */
				YM_DELTAT_ADPCM_Write(&F2608->deltaT,addr,v);
			}
			break;
		case 0x10:	/* IRQ Flag controll */
			if( addr == 0x10 )
				YM2608IRQFlagWrite(&(OPN->ST),n,v);
			break;
		default:
			OPNWriteReg(OPN,addr|0x100,v);
		}
	}
	return OPN->ST.irq;
}
UINT8 YM2608Read(int n,int a)
{
	YM2608 *F2608 = &(FM2608[n]);
	int addr = F2608->OPN.ST.address;
	int ret = 0;

	switch( a&3 ){
	case 0:	/* status 0 : YM2203 compatible */
		/* BUSY:x:x:x:x:x:FLAGB:FLAGA */
		if(addr==0xff) ret = 0x00; /* ID code */
		else ret = F2608->OPN.ST.status & 0x83;
		break;
	case 1:	/* status 0 */
		if( addr < 16 ) ret = SSGRead(n);
		break;
	case 2:	/* status 1 : + ADPCM status */
		/* BUSY:x:PCMBUSY:ZERO:BRDY:EOS:FLAGB:FLAGA */
		if(addr==0xff) ret = 0x00; /* ID code */
		else ret = F2608->OPN.ST.status | (F2608->adpcm[6].flag ? 0x20 : 0);
		break;
	case 3:
		ret = 0;
		break;
	}
	return ret;
}

int YM2608TimerOver(int n,int c)
{
	YM2608 *F2608 = &(FM2608[n]);

	if( c )
	{	/* Timer B */
		TimerBOver( &(F2608->OPN.ST) );
	}
	else
	{	/* Timer A */
		YM2608UpdateReq(n);
		/* timer update */
		TimerAOver( &(F2608->OPN.ST) );
		/* CSM mode key,TL controll */
		if( F2608->OPN.ST.mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			CSMKeyControll( &(F2608->CH[2]) );
		}
	}
	return FM2608->OPN.ST.irq;
}

#endif /* BUILD_YM2608 */

#if BUILD_OPNB
/* -------------------------- YM2610(OPNB) ---------------------------------- */
static YM2610 *FM2610=NULL;	/* array of YM2610's */
static int YM2610NumChips;	/* total chip */

/* ---------- update one of chip (YM2610B FM6: ADPCM-A6: ADPCM-B:1) ----------- */
void YM2610UpdateOne(int num, INT16 **buffer, int length)
{
	YM2610 *F2610 = &(FM2610[num]);
	FM_OPN *OPN   = &(FM2610[num].OPN);
	YM_DELTAT *DELTAT = &(F2610[num].deltaT);
	int i,j;
	int ch;
	FMSAMPLE  *bufL,*bufR;

	/* setup DELTA-T unit */
	YM_DELTAT_DECODE_PRESET(DELTAT);

	/* buffer setup */
	bufL = buffer[0];
	bufR = buffer[1];

	if( (void *)F2610 != cur_chip ){
		cur_chip = (void *)F2610;
		State = &OPN->ST;
		cch[0] = &F2610->CH[1];
		cch[1] = &F2610->CH[2];
		cch[2] = &F2610->CH[4];
		cch[3] = &F2610->CH[5];
		/* setup adpcm rom address */
		pcmbufA  = F2610->pcmbuf;
		pcmsizeA = F2610->pcm_size;
#if FM_LFO_SUPPORT
		LFOCnt  = OPN->LFOCnt;
		LFOIncr = OPN->LFOIncr;
		if( !LFOIncr ) lfo_amd = lfo_pmd = 0;
#endif
	}
#ifdef YM2610B_WARNING
#define FM_MSG_YM2610B "YM2610-%d.CH%d is playing,Check whether the type of the chip is YM2610B\n"
	/* Check YM2610B worning message */
	if( FM_KEY_IS(&F2610->CH[0].SLOT[3]) )
		LOG(LOG_WAR,(FM_MSG_YM2610B,num,0));
	if( FM_KEY_IS(&F2610->CH[3].SLOT[3]) )
		LOG(LOG_WAR,(FM_MSG_YM2610B,num,3));
#endif
	/* update frequency counter */
	OPN_CALC_FCOUNT( cch[0] );
	if( (State->mode & 0xc0) ){
		/* 3SLOT MODE */
		if( cch[1]->SLOT[SLOT1].Incr==-1){
			/* 3 slot mode */
			CALC_FCSLOT(&cch[1]->SLOT[SLOT1] , OPN->SL3.fc[1] , OPN->SL3.kcode[1] );
			CALC_FCSLOT(&cch[1]->SLOT[SLOT2] , OPN->SL3.fc[2] , OPN->SL3.kcode[2] );
			CALC_FCSLOT(&cch[1]->SLOT[SLOT3] , OPN->SL3.fc[0] , OPN->SL3.kcode[0] );
			CALC_FCSLOT(&cch[1]->SLOT[SLOT4] , cch[1]->fc , cch[1]->kcode );
		}
	}else OPN_CALC_FCOUNT( cch[1] );
	OPN_CALC_FCOUNT( cch[2] );
	OPN_CALC_FCOUNT( cch[3] );

	/* buffering */
    for( i=0; i < length ; i++ )
	{
#if FM_LFO_SUPPORT
		/* LFO */
		if( LFOIncr )
		{
			lfo_amd = OPN_LFO_wave[(LFOCnt+=LFOIncr)>>LFO_SHIFT];
			lfo_pmd = lfo_amd-(LFO_RATE/2);
		}
#endif
		/* clear output acc. */
		out_ch[OUTD_LEFT] = out_ch[OUTD_RIGHT]= out_ch[OUTD_CENTER] = 0;
		/**** deltaT ADPCM ****/
		if( DELTAT->flag )
			YM_DELTAT_ADPCM_CALC(DELTAT);
		/* FM */
		for(ch = 0 ; ch < 4 ; ch++)
			FM_CALC_CH( cch[ch] );
		for( j = 0; j < 6; j++ )
		{
			/**** ADPCM ****/
			if( F2610->adpcm[j].flag )
				OPNB_ADPCM_CALC_CHA( F2610, &F2610->adpcm[j]);
		}
		/* buffering */
		FM_BUFFERING_STEREO;
		/* timer A controll */
		INTERNAL_TIMER_A( State , cch[1] )
	}
	INTERNAL_TIMER_B(State,length)
#if FM_LFO_SUPPORT
	OPN->LFOCnt = LFOCnt;
#endif
}
#endif /* BUILD_OPNB */

#if BUILD_YM2610B
/* ---------- update one of chip (YM2610B FM6: ADPCM-A6: ADPCM-B:1) ----------- */
void YM2610BUpdateOne(int num, INT16 **buffer, int length)
{
	YM2610 *F2610 = &(FM2610[num]);
	FM_OPN *OPN   = &(FM2610[num].OPN);
	YM_DELTAT *DELTAT = &(FM2610[num].deltaT);
	int i,j;
	FM_CH *ch;
	FMSAMPLE  *bufL,*bufR;

	/* setup DELTA-T unit */
	YM_DELTAT_DECODE_PRESET(DELTAT);
	/* buffer setup */
	bufL = buffer[0];
	bufR = buffer[1];

	if( (void *)F2610 != cur_chip ){
		cur_chip = (void *)F2610;
		State = &OPN->ST;
		cch[0] = &F2610->CH[0];
		cch[1] = &F2610->CH[1];
		cch[2] = &F2610->CH[2];
		cch[3] = &F2610->CH[3];
		cch[4] = &F2610->CH[4];
		cch[5] = &F2610->CH[5];
		/* setup adpcm rom address */
		pcmbufA  = F2610->pcmbuf;
		pcmsizeA = F2610->pcm_size;
#if FM_LFO_SUPPORT
		LFOCnt  = OPN->LFOCnt;
		LFOIncr = OPN->LFOIncr;
		if( !LFOIncr ) lfo_amd = lfo_pmd = 0;
#endif
	}

	/* update frequency counter */
	OPN_CALC_FCOUNT( cch[0] );
	OPN_CALC_FCOUNT( cch[1] );
	if( (State->mode & 0xc0) ){
		/* 3SLOT MODE */
		if( cch[2]->SLOT[SLOT1].Incr==-1){
			/* 3 slot mode */
			CALC_FCSLOT(&cch[2]->SLOT[SLOT1] , OPN->SL3.fc[1] , OPN->SL3.kcode[1] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT2] , OPN->SL3.fc[2] , OPN->SL3.kcode[2] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT3] , OPN->SL3.fc[0] , OPN->SL3.kcode[0] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT4] , cch[2]->fc , cch[2]->kcode );
		}
	}else OPN_CALC_FCOUNT( cch[2] );
	OPN_CALC_FCOUNT( cch[3] );
	OPN_CALC_FCOUNT( cch[4] );
	OPN_CALC_FCOUNT( cch[5] );

	/* buffering */
    for( i=0; i < length ; i++ )
	{
#if FM_LFO_SUPPORT
		/* LFO */
		if( LFOIncr )
		{
			lfo_amd = OPN_LFO_wave[(LFOCnt+=LFOIncr)>>LFO_SHIFT];
			lfo_pmd = lfo_amd-(LFO_RATE/2);
		}
#endif
		/* clear output acc. */
		out_ch[OUTD_LEFT] = out_ch[OUTD_RIGHT]= out_ch[OUTD_CENTER] = 0;
		/**** deltaT ADPCM ****/
		if( DELTAT->flag )
			YM_DELTAT_ADPCM_CALC(DELTAT);
		/* FM */
		for(ch = cch[0] ; ch <= cch[5] ; ch++)
			FM_CALC_CH( ch );
		for( j = 0; j < 6; j++ )
		{
			/**** ADPCM ****/
			if( F2610->adpcm[j].flag )
				OPNB_ADPCM_CALC_CHA( F2610, &F2610->adpcm[j]);
		}
		/* buffering */
		FM_BUFFERING_STEREO;
		/* timer A controll */
		INTERNAL_TIMER_A( State , cch[2] )
	}
	INTERNAL_TIMER_B(State,length)
#if FM_LFO_SUPPORT
	OPN->LFOCnt = LFOCnt;
#endif
}
#endif /* BUILD_YM2610B */

#if BUILD_OPNB
int YM2610Init(int num, int clock, int rate,
               void **pcmroma,int *pcmsizea,void **pcmromb,int *pcmsizeb,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler)

{
	int i;

    if (FM2610) return (-1);	/* duplicate init. */
    cur_chip = NULL;	/* hiro-shi!! */

	YM2610NumChips = num;

	/* allocate extend state space */
	if( (FM2610 = (YM2610 *)malloc(sizeof(YM2610) * YM2610NumChips))==NULL)
		return (-1);
	/* clear */
	memset(FM2610,0,sizeof(YM2610) * YM2610NumChips);
	/* allocate total level table (128kb space) */
	if( !OPNInitTable() )
	{
		free( FM2610 );
		return (-1);
	}

	for ( i = 0 ; i < YM2610NumChips; i++ ) {
		/* FM */
		FM2610[i].OPN.ST.index = i;
		FM2610[i].OPN.type = TYPE_YM2610;
		FM2610[i].OPN.P_CH = FM2610[i].CH;
		FM2610[i].OPN.ST.clock = clock;
		FM2610[i].OPN.ST.rate = rate;
		/* FM2610[i].OPN.ST.irq = 0; */
		/* FM2610[i].OPN.ST.status = 0; */
		FM2610[i].OPN.ST.timermodel = FM_TIMER_INTERVAL;
		/* Extend handler */
		FM2610[i].OPN.ST.Timer_Handler = TimerHandler;
		FM2610[i].OPN.ST.IRQ_Handler   = IRQHandler;
		/* ADPCM */
		FM2610[i].pcmbuf   = (UINT8 *)(pcmroma[i]);
		FM2610[i].pcm_size = pcmsizea[i];
		/* DELTA-T */
		FM2610[i].deltaT.memory = (UINT8 *)(pcmromb[i]);
		FM2610[i].deltaT.memory_size = pcmsizeb[i];
		/* */
		YM2610ResetChip(i);
	}
	InitOPNB_ADPCMATable();
	return 0;
}

/* ---------- shut down emurator ----------- */
void YM2610Shutdown()
{
    if (!FM2610) return;

	FMCloseTable();
	free(FM2610);
	FM2610 = NULL;
}

/* ---------- reset one of chip ---------- */
void YM2610ResetChip(int num)
{
	int i;
	YM2610 *F2610 = &(FM2610[num]);
	FM_OPN *OPN   = &(FM2610[num].OPN);
	YM_DELTAT *DELTAT = &(FM2610[num].deltaT);

	/* Reset Priscaler */
	OPNSetPris( OPN, 6*24, 6*24, 4*2); /* OPN 1/6 , SSG 1/4 */
	/* reset SSG section */
	SSGReset(OPN->ST.index);
	/* status clear */
	FM_IRQMASK_SET(&OPN->ST,0x03);
	OPNWriteMode(OPN,0x27,0x30); /* mode 0 , timer reset */

	reset_channel( &OPN->ST , F2610->CH , 6 );
	/* reset OPerator paramater */
	for(i = 0xb6 ; i >= 0xb4 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0xc0);
		OPNWriteReg(OPN,i|0x100,0xc0);
	}
	for(i = 0xb2 ; i >= 0x30 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0);
		OPNWriteReg(OPN,i|0x100,0);
	}
	for(i = 0x26 ; i >= 0x20 ; i-- ) OPNWriteReg(OPN,i,0);
	/**** ADPCM work initial ****/
	for( i = 0; i < 6+1; i++ ){
		F2610->adpcm[i].now_addr  = 0;
		F2610->adpcm[i].now_step  = 0;
		F2610->adpcm[i].step      = 0;
		F2610->adpcm[i].start     = 0;
		F2610->adpcm[i].end       = 0;
		/* F2610->adpcm[i].delta     = 21866; */
		F2610->adpcm[i].volume    = 0;
		F2610->adpcm[i].pan       = &out_ch[OUTD_CENTER]; /* default center */
		F2610->adpcm[i].flagMask  = (i == 6) ? 0x80 : (1<<i);
		F2610->adpcm[i].flag      = 0;
		F2610->adpcm[i].adpcmx    = 0;
		F2610->adpcm[i].adpcmd    = 127;
		F2610->adpcm[i].adpcml    = 0;
	}
	F2610->adpcmTL = &(TL_TABLE[0x3f*(int)(0.75/EG_STEP)]);
	/* F2610->port1state = -1; */
	F2610->adpcm_arrivedEndAddress = 0;

	/* DELTA-T unit */
	DELTAT->freqbase = OPN->ST.freqbase;
	DELTAT->output_pointer = out_ch;
	DELTAT->portshift = 8;		/* allways 8bits shift */
	DELTAT->output_range = DELTAT_MIXING_LEVEL<<TL_BITS;
	YM_DELTAT_ADPCM_Reset(DELTAT,OUTD_CENTER);
}

/* YM2610 write */
/* n = number  */
/* a = address */
/* v = value   */
int YM2610Write(int n, int a,UINT8 v)
{
	YM2610 *F2610 = &(FM2610[n]);
	FM_OPN *OPN   = &(FM2610[n].OPN);
	int addr;
	int ch;

	switch( a&3 ){
	case 0:	/* address port 0 */
		OPN->ST.address = v & 0xff;
		/* Write register to SSG emurator */
		if( v < 16 ) SSGWrite(n,0,v);
		break;
	case 1:	/* data port 0    */
		addr = OPN->ST.address;
		switch(addr & 0xf0)
		{
		case 0x00:	/* SSG section */
			/* Write data to SSG emurator */
			SSGWrite(n,a,v);
			break;
		case 0x10: /* DeltaT ADPCM */
			YM2610UpdateReq(n);
			switch(addr)
			{
			case 0x1c: /*  FLAG CONTROL : Extend Status Clear/Mask */
			{
				UINT8 statusmask = ~v;
				/* set arrived flag mask */
				for(ch=0;ch<6;ch++)
					F2610->adpcm[ch].flagMask = statusmask&(1<<ch);
				F2610->deltaT.flagMask = statusmask&0x80;
				/* clear arrived flag */
				F2610->adpcm_arrivedEndAddress &= statusmask&0x3f;
				F2610->deltaT.arrivedFlag      &= F2610->deltaT.flagMask;
			}
				break;
			default:
				/* 0x10-0x1b */
				YM_DELTAT_ADPCM_Write(&F2610->deltaT,addr-0x10,v);
			}
			break;
		case 0x20:	/* Mode Register */
			YM2610UpdateReq(n);
			OPNWriteMode(OPN,addr,v);
			break;
		default:	/* OPN section */
			YM2610UpdateReq(n);
			/* write register */
			 OPNWriteReg(OPN,addr,v);
		}
		break;
	case 2:	/* address port 1 */
		F2610->address1 = v & 0xff;
		break;
	case 3:	/* data port 1    */
		YM2610UpdateReq(n);
		addr = F2610->address1;
		if( addr < 0x30 )
			/* 100-12f : ADPCM A section */
			FM_ADPCMAWrite(F2610,addr,v);
		else
			OPNWriteReg(OPN,addr|0x100,v);
	}
	return OPN->ST.irq;
}
UINT8 YM2610Read(int n,int a)
{
	YM2610 *F2610 = &(FM2610[n]);
	int addr = F2610->OPN.ST.address;
	UINT8 ret = 0;

	switch( a&3){
	case 0:	/* status 0 : YM2203 compatible */
		ret = F2610->OPN.ST.status & 0x83;
		break;
	case 1:	/* data 0 */
		if( addr < 16 ) ret = SSGRead(n);
		if( addr == 0xff ) ret = 0x01;
		break;
	case 2:	/* status 1 : + ADPCM status */
		/* ADPCM STATUS (arrived End Address) */
		/* B,--,A5,A4,A3,A2,A1,A0 */
		/* B     = ADPCM-B(DELTA-T) arrived end address */
		/* A0-A5 = ADPCM-A          arrived end address */
		ret = F2610->adpcm_arrivedEndAddress | F2610->deltaT.arrivedFlag;
		break;
	case 3:
		ret = 0;
		break;
	}
	return ret;
}

int YM2610TimerOver(int n,int c)
{
	YM2610 *F2610 = &(FM2610[n]);

	if( c )
	{	/* Timer B */
		TimerBOver( &(F2610->OPN.ST) );
	}
	else
	{	/* Timer A */
		YM2610UpdateReq(n);
		/* timer update */
		TimerAOver( &(F2610->OPN.ST) );
		/* CSM mode key,TL controll */
		if( F2610->OPN.ST.mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			CSMKeyControll( &(F2610->CH[2]) );
		}
	}
	return F2610->OPN.ST.irq;
}

#endif /* BUILD_OPNB */


#if BUILD_YM2612
/*******************************************************************************/
/*		YM2612 local section                                                   */
/*******************************************************************************/
/* here's the virtual YM2612 */
typedef struct ym2612_f {
	FM_OPN OPN;						/* OPN state       */
	FM_CH CH[6];					/* channel state */
	int address1;	/* address register1 */
	/* dac output (YM2612) */
	int dacen;
	int dacout;
} YM2612;

static int YM2612NumChips;	/* total chip */
static YM2612 *FM2612=NULL;	/* array of YM2612's */

static int dacen;

/* ---------- update one of chip ----------- */
void YM2612UpdateOne(int num, INT16 **buffer, int length)
{
	YM2612 *F2612 = &(FM2612[num]);
	FM_OPN *OPN   = &(FM2612[num].OPN);
	int i;
	FM_CH *ch,*ech;
	FMSAMPLE  *bufL,*bufR;
	int dacout  = F2612->dacout;

	/* set bufer */
	bufL = buffer[0];
	bufR = buffer[1];

	if( (void *)F2612 != cur_chip ){
		cur_chip = (void *)F2612;

		State = &OPN->ST;
		cch[0]   = &F2612->CH[0];
		cch[1]   = &F2612->CH[1];
		cch[2]   = &F2612->CH[2];
		cch[3]   = &F2612->CH[3];
		cch[4]   = &F2612->CH[4];
		cch[5]   = &F2612->CH[5];
		/* DAC mode */
		dacen = F2612->dacen;
#if FM_LFO_SUPPORT
		LFOCnt  = OPN->LFOCnt;
		LFOIncr = OPN->LFOIncr;
		if( !LFOIncr ) lfo_amd = lfo_pmd = 0;
#endif
	}
	/* update frequency counter */
	OPN_CALC_FCOUNT( cch[0] );
	OPN_CALC_FCOUNT( cch[1] );
	if( (State->mode & 0xc0) ){
		/* 3SLOT MODE */
		if( cch[2]->SLOT[SLOT1].Incr==-1){
			/* 3 slot mode */
			CALC_FCSLOT(&cch[2]->SLOT[SLOT1] , OPN->SL3.fc[1] , OPN->SL3.kcode[1] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT2] , OPN->SL3.fc[2] , OPN->SL3.kcode[2] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT3] , OPN->SL3.fc[0] , OPN->SL3.kcode[0] );
			CALC_FCSLOT(&cch[2]->SLOT[SLOT4] , cch[2]->fc , cch[2]->kcode );
		}
	}else OPN_CALC_FCOUNT( cch[2] );
	OPN_CALC_FCOUNT( cch[3] );
	OPN_CALC_FCOUNT( cch[4] );
	OPN_CALC_FCOUNT( cch[5] );

	ech = dacen ? cch[4] : cch[5];
	/* buffering */
    for( i=0; i < length ; i++ )
	{
#if FM_LFO_SUPPORT
		/* LFO */
		if( LFOIncr )
		{
			lfo_amd = OPN_LFO_wave[(LFOCnt+=LFOIncr)>>LFO_SHIFT];
			lfo_pmd = lfo_amd-(LFO_RATE/2);
		}
#endif
		/* clear output acc. */
		out_ch[OUTD_LEFT] = out_ch[OUTD_RIGHT]= out_ch[OUTD_CENTER] = 0;
		/* calcrate channel output */
		for(ch = cch[0] ; ch <= ech ; ch++)
			FM_CALC_CH( ch );
		if( dacen )  *cch[5]->connect4 += dacout;
		/* buffering */
		FM_BUFFERING_STEREO;
		/* timer A controll */
		INTERNAL_TIMER_A( State , cch[2] )
	}
	INTERNAL_TIMER_B(State,length)
#if FM_LFO_SUPPORT
	OPN->LFOCnt = LFOCnt;
#endif
}

/* -------------------------- YM2612 ---------------------------------- */
int YM2612Init(int num, int clock, int rate,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler)
{
	int i;

    if (FM2612) return (-1);	/* duplicate init. */
    cur_chip = NULL;	/* hiro-shi!! */

	YM2612NumChips = num;

	/* allocate extend state space */
	if( (FM2612 = (YM2612 *)malloc(sizeof(YM2612) * YM2612NumChips))==NULL)
		return (-1);
	/* clear */
	memset(FM2612,0,sizeof(YM2612) * YM2612NumChips);
	/* allocate total level table (128kb space) */
	if( !OPNInitTable() )
	{
		free( FM2612 );
		return (-1);
	}

	for ( i = 0 ; i < YM2612NumChips; i++ ) {
		FM2612[i].OPN.ST.index = i;
		FM2612[i].OPN.type = TYPE_YM2612;
		FM2612[i].OPN.P_CH = FM2612[i].CH;
		FM2612[i].OPN.ST.clock = clock;
		FM2612[i].OPN.ST.rate = rate;
		/* FM2612[i].OPN.ST.irq = 0; */
		/* FM2612[i].OPN.ST.status = 0; */
		FM2612[i].OPN.ST.timermodel = FM_TIMER_INTERVAL;
		/* Extend handler */
		FM2612[i].OPN.ST.Timer_Handler = TimerHandler;
		FM2612[i].OPN.ST.IRQ_Handler   = IRQHandler;
		YM2612ResetChip(i);
	}
	return 0;
}

/* ---------- shut down emurator ----------- */
void YM2612Shutdown()
{
    if (!FM2612) return;

	FMCloseTable();
	free(FM2612);
	FM2612 = NULL;
}

/* ---------- reset one of chip ---------- */
void YM2612ResetChip(int num)
{
	int i;
	YM2612 *F2612 = &(FM2612[num]);
	FM_OPN *OPN   = &(FM2612[num].OPN);

	OPNSetPris( OPN , 12*12, 12*12, 0);
	/* status clear */
	FM_IRQMASK_SET(&OPN->ST,0x03);
	OPNWriteMode(OPN,0x27,0x30); /* mode 0 , timer reset */

	reset_channel( &OPN->ST , &F2612->CH[0] , 6 );

	for(i = 0xb6 ; i >= 0xb4 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0xc0);
		OPNWriteReg(OPN,i|0x100,0xc0);
	}
	for(i = 0xb2 ; i >= 0x30 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0);
		OPNWriteReg(OPN,i|0x100,0);
	}
	for(i = 0x26 ; i >= 0x20 ; i-- ) OPNWriteReg(OPN,i,0);
	/* DAC mode clear */
	F2612->dacen = 0;
}

/* YM2612 write */
/* n = number  */
/* a = address */
/* v = value   */
int YM2612Write(int n, int a,UINT8 v)
{
	YM2612 *F2612 = &(FM2612[n]);
	int addr;

	switch( a&3){
	case 0:	/* address port 0 */
		F2612->OPN.ST.address = v & 0xff;
		break;
	case 1:	/* data port 0    */
		addr = F2612->OPN.ST.address;
		switch( addr & 0xf0 )
		{
		case 0x20:	/* 0x20-0x2f Mode */
			switch( addr )
			{
			case 0x2a:	/* DAC data (YM2612) */
				YM2612UpdateReq(n);
				F2612->dacout = ((int)v - 0x80)<<(TL_BITS-7);
				break;
			case 0x2b:	/* DAC Sel  (YM2612) */
				/* b7 = dac enable */
				F2612->dacen = v & 0x80;
				cur_chip = NULL;
				break;
			default:	/* OPN section */
				YM2612UpdateReq(n);
				/* write register */
				 OPNWriteMode(&(F2612->OPN),addr,v);
			}
			break;
		default:	/* 0x30-0xff OPN section */
			YM2612UpdateReq(n);
			/* write register */
			 OPNWriteReg(&(F2612->OPN),addr,v);
		}
		break;
	case 2:	/* address port 1 */
		F2612->address1 = v & 0xff;
		break;
	case 3:	/* data port 1    */
		addr = F2612->address1;
		YM2612UpdateReq(n);
		OPNWriteReg(&(F2612->OPN),addr|0x100,v);
		break;
	}
	return F2612->OPN.ST.irq;
}
UINT8 YM2612Read(int n,int a)
{
	YM2612 *F2612 = &(FM2612[n]);

	switch( a&3){
	case 0:	/* status 0 */
		return F2612->OPN.ST.status;
	case 1:
	case 2:
	case 3:
		LOG(LOG_WAR,("YM2612 #%d:A=%d read unmapped area\n"));
		return F2612->OPN.ST.status;
	}
	return 0;
}

int YM2612TimerOver(int n,int c)
{
	YM2612 *F2612 = &(FM2612[n]);

	if( c )
	{	/* Timer B */
		TimerBOver( &(F2612->OPN.ST) );
	}
	else
	{	/* Timer A */
		YM2612UpdateReq(n);
		/* timer update */
		TimerAOver( &(F2612->OPN.ST) );
		/* CSM mode key,TL controll */
		if( F2612->OPN.ST.mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			CSMKeyControll( &(F2612->CH[2]) );
		}
	}
	return F2612->OPN.ST.irq;
}

#endif /* BUILD_YM2612 */


#if BUILD_YM2151
/*******************************************************************************/
/*		YM2151 local section                                                   */
/*******************************************************************************/
/* -------------------------- OPM ---------------------------------- */
#undef  FM_SEG_SUPPORT
#define FM_SEG_SUPPORT 0	/* OPM has not SEG type envelope */

/* here's the virtual YM2151(OPM)  */
typedef struct ym2151_f {
	FM_ST ST;					/* general state     */
	FM_CH CH[8];				/* channel state     */
	UINT8 ct;					/* CT0,1             */
	UINT32 NoiseCnt;			/* noise generator   */
	UINT32 NoiseIncr;			/* noise mode enable & step */
#if FM_LFO_SUPPORT
	/* LFO */
	UINT32 LFOCnt;
	UINT32 LFOIncr;
	UINT8 pmd;					/* LFO pmd level     */
	UINT8 amd;					/* LFO amd level     */
	INT32 *wavetype;			/* LFO waveform      */
	UINT8 testreg;				/* test register (LFO reset) */
#endif
	UINT32 KC_TABLE[8*12*64+950];/* keycode,keyfunction -> count */
	mem_write_handler PortWrite;/*  callback when write CT0/CT1 */
} YM2151;

static YM2151 *FMOPM=NULL;	/* array of YM2151's */
static int YM2151NumChips;	/* total chip */

#if FM_LFO_SUPPORT
static INT32 OPM_LFO_waves[LFO_ENT*4];	/* LFO wave tabel    */
static INT32 *OPM_LFO_wave;
#endif

/* current chip state */
static UINT32 NoiseCnt , NoiseIncr;

static INT32 *NOISE_TABLE[SIN_ENT];

static const int DT2_TABLE[4]={ /* 4 DT2 values */
/*
 *   DT2 defines offset in cents from base note
 *
 *   The table below defines offset in deltas table...
 *   User's Manual page 22
 *   Values below were calculated using formula:  value = orig.val * 1.5625
 *
 * DT2=0 DT2=1 DT2=2 DT2=3
 * 0     600   781   950
 */
	0,    384,  500,  608
};

static const int KC_TO_SEMITONE[16]={
	/*translate note code KC into more usable number of semitone*/
	0*64, 1*64, 2*64, 3*64,
	3*64, 4*64, 5*64, 6*64,
	6*64, 7*64, 8*64, 9*64,
	9*64,10*64,11*64,12*64
};

/* ---------- frequency counter  ---------- */
INLINE void OPM_CALC_FCOUNT(YM2151 *OPM , FM_CH *CH )
{
	if( CH->SLOT[SLOT1].Incr==-1)
	{
		int fc = CH->fc;
		int kc = CH->kcode;

		CALC_FCSLOT(&CH->SLOT[SLOT1] , OPM->KC_TABLE[fc + CH->SLOT[SLOT1].DT2] , kc );
		CALC_FCSLOT(&CH->SLOT[SLOT2] , OPM->KC_TABLE[fc + CH->SLOT[SLOT2].DT2] , kc );
		CALC_FCSLOT(&CH->SLOT[SLOT3] , OPM->KC_TABLE[fc + CH->SLOT[SLOT3].DT2] , kc );
		CALC_FCSLOT(&CH->SLOT[SLOT4] , OPM->KC_TABLE[fc + CH->SLOT[SLOT4].DT2] , kc );
	}
}

/* ---------- calcrate one of channel7 ---------- */
INLINE void OPM_CALC_CH7( FM_CH *CH )
{
	UINT32 eg_out1,eg_out2,eg_out3,eg_out4;  //envelope output

	/* Phase Generator */
#if FM_LFO_SUPPORT
	INT32 pms = lfo_pmd * CH->pms / LFO_RATE;
	if(pms)
	{
		pg_in1 = (CH->SLOT[SLOT1].Cnt += CH->SLOT[SLOT1].Incr + (INT32)(pms * CH->SLOT[SLOT1].Incr) / PMS_RATE);
		pg_in2 = (CH->SLOT[SLOT2].Cnt += CH->SLOT[SLOT2].Incr + (INT32)(pms * CH->SLOT[SLOT2].Incr) / PMS_RATE);
		pg_in3 = (CH->SLOT[SLOT3].Cnt += CH->SLOT[SLOT3].Incr + (INT32)(pms * CH->SLOT[SLOT3].Incr) / PMS_RATE);
		pg_in4 = (CH->SLOT[SLOT4].Cnt += CH->SLOT[SLOT4].Incr + (INT32)(pms * CH->SLOT[SLOT4].Incr) / PMS_RATE);
	}
	else
#endif
	{
		pg_in1 = (CH->SLOT[SLOT1].Cnt += CH->SLOT[SLOT1].Incr);
		pg_in2 = (CH->SLOT[SLOT2].Cnt += CH->SLOT[SLOT2].Incr);
		pg_in3 = (CH->SLOT[SLOT3].Cnt += CH->SLOT[SLOT3].Incr);
		pg_in4 = (CH->SLOT[SLOT4].Cnt += CH->SLOT[SLOT4].Incr);
	}
	/* Envelope Generator */
	FM_CALC_EG(eg_out1,CH->SLOT[SLOT1]);
	FM_CALC_EG(eg_out2,CH->SLOT[SLOT2]);
	FM_CALC_EG(eg_out3,CH->SLOT[SLOT3]);
	FM_CALC_EG(eg_out4,CH->SLOT[SLOT4]);

	/* connection */
	if( eg_out1 < EG_CUT_OFF )	/* SLOT 1 */
	{
		if( CH->FB ){
			/* with self feed back */
			pg_in1 += (CH->op1_out[0]+CH->op1_out[1])>>CH->FB;
			CH->op1_out[1] = CH->op1_out[0];
		}
		CH->op1_out[0] = OP_OUT(pg_in1,eg_out1);
		/* output slot1 */
		if( !CH->connect1 )
		{
			/* algorythm 5  */
			pg_in2 += CH->op1_out[0];
			pg_in3 += CH->op1_out[0];
			pg_in4 += CH->op1_out[0];
		}else{
			/* other algorythm */
			*CH->connect1 += CH->op1_out[0];
		}
	}
	if( eg_out2 < EG_CUT_OFF )	/* SLOT 2 */
		*CH->connect2 += OP_OUT(pg_in2,eg_out2);
	if( eg_out3 < EG_CUT_OFF )	/* SLOT 3 */
		*CH->connect3 += OP_OUT(pg_in3,eg_out3);
	/* SLOT 4 */
	if(NoiseIncr)
	{
		NoiseCnt += NoiseIncr;
		if( eg_out4 < EG_CUT_OFF )
			*CH->connect4 += OP_OUTN(NoiseCnt,eg_out4);
	}
	else
	{
		if( eg_out4 < EG_CUT_OFF )
			*CH->connect4 += OP_OUT(pg_in4,eg_out4);
	}
}

static int OPMInitTable(void)
{
	int i;

	/* NOISE wave table */
	for(i=0;i<SIN_ENT;i++)
	{
		int sign = rand()&1 ? TL_MAX : 0;
		int lev = rand()&0x1ff;
		//pom = lev ? 20*log10(0x200/lev) : 0;   /* decibel */
		//NOISE_TABLE[i] = &TL_TABLE[sign + (int)(pom / EG_STEP)]; /* TL_TABLE steps */
		NOISE_TABLE[i] = &TL_TABLE[sign + lev * EG_ENT/0x200]; /* TL_TABLE steps */
	}

#if FM_LFO_SUPPORT
	/* LFO wave tables , 4 pattern */
	for(i=0;i<LFO_ENT;i++)
	{
		OPM_LFO_waves[          i]= LFO_RATE * i / LFO_ENT /127;
		OPM_LFO_waves[LFO_ENT  +i]= ( i<LFO_ENT/2 ? 0 : LFO_RATE )/127;
		OPM_LFO_waves[LFO_ENT*2+i]= LFO_RATE* (i<LFO_ENT/2 ? i : LFO_ENT-i) /(LFO_ENT/2) /127;
		OPM_LFO_waves[LFO_ENT*3+i]= LFO_RATE * (rand()&0xff) /256 /127;
	}
#endif
	return FMInitTable();
}

/* ---------- priscaler set(and make time tables) ---------- */
static void OPMResetTable( int num )
{
    YM2151 *OPM = &(FMOPM[num]);
	int i;
	double pom;
	double rate;

	if (FMOPM[num].ST.rate)
		rate = (double)(1<<FREQ_BITS) / (3579545.0 / FMOPM[num].ST.clock * FMOPM[num].ST.rate);
	else rate = 1;

	for (i=0; i<8*12*64+950; i++)
	{
		/* This calculation type was used from the Jarek's YM2151 emulator */
		pom = 6.875 * pow (2, ((i+4*64)*1.5625/1200.0) ); /*13.75Hz is note A 12semitones below A-0, so D#0 is 4 semitones above then*/
		/*calculate phase increment for above precounted Hertz value*/
		OPM->KC_TABLE[i] = (UINT32)(pom * rate);
		/*LOG(LOG_WAR,("OPM KC %d = %x\n",i,OPM->KC_TABLE[i]));*/
	}

	/* make time tables */
	init_timetables( &OPM->ST , OPM_DTTABLE , OPM_ARRATE , OPM_DRRATE );

}

/* ---------- write a register on YM2151 chip number 'n' ---------- */
static void OPMWriteReg(int n, int r, int v)
{
	UINT8 c;
	FM_CH *CH;
	FM_SLOT *SLOT;

    YM2151 *OPM = &(FMOPM[n]);

	c   = OPM_CHAN(r);
	CH  = &OPM->CH[c];
	SLOT= &CH->SLOT[OPM_SLOT(r)];

	switch( r & 0xe0 ){
	case 0x00: /* 0x00-0x1f */
		switch( r ){
#if FM_LFO_SUPPORT
		case 0x01:	/* test */
			if( (OPM->testreg&(OPM->testreg^v))&0x02 ) /* fall eggge */
			{	/* reset LFO counter */
				OPM->LFOCnt = 0;
				cur_chip = NULL;
			}
			OPM->testreg = v;
			break;
#endif
		case 0x08:	/* key on / off */
			c = v&7;
			/* CSM mode */
			if( OPM->ST.mode & 0x80 ) break;
			CH = &OPM->CH[c];
			if(v&0x08) FM_KEYON(CH,SLOT1); else FM_KEYOFF(CH,SLOT1);
			if(v&0x10) FM_KEYON(CH,SLOT2); else FM_KEYOFF(CH,SLOT2);
			if(v&0x20) FM_KEYON(CH,SLOT3); else FM_KEYOFF(CH,SLOT3);
			if(v&0x40) FM_KEYON(CH,SLOT4); else FM_KEYOFF(CH,SLOT4);
			break;
		case 0x0f:	/* Noise freq (ch7.op4) */
			/* b7 = Noise enable */
			/* b0-4 noise freq  */
			OPM->NoiseIncr = !(v&0x80) ? 0 :
				/* !!!!! unknown noise freqency rate !!!!! */
				(UINT32)((1<<FREQ_BITS) / 65536 * (v&0x1f) * OPM->ST.freqbase);
			cur_chip = NULL;
#if 1
			if( v & 0x80 ){
				LOG(LOG_WAR,("OPM Noise mode selelted\n"));
			}
#endif
			break;
		case 0x10:	/* timer A High 8*/
			OPM->ST.TA = (OPM->ST.TA & 0x03)|(((int)v)<<2);
			break;
		case 0x11:	/* timer A Low 2*/
			OPM->ST.TA = (OPM->ST.TA & 0x3fc)|(v&3);
			break;
		case 0x12:	/* timer B */
			OPM->ST.TB = v;
			break;
		case 0x14:	/* mode , timer controll */
			FMSetMode( &(OPM->ST),n,v );
			break;
#if FM_LFO_SUPPORT
		case 0x18:	/* lfreq   */
			/* f = fm * 2^(LFRQ/16) / (4295*10^6) */
			{
				static double drate[16]={
					1.0        ,1.044273782,1.090507733,1.138788635, //0-3
					1.189207115,1.241857812,1.296839555,1.354255547, //4-7
					1.414213562,1.476826146,1.542210825,1.610490332, //8-11
					1.681792831,1.75625216 ,1.834008086,1.915206561};
				double rate = pow(2.0,v/16)*drate[v&0x0f] / 4295000000.0;
				OPM->LFOIncr = (UINT32)((double)LFO_ENT*(1<<LFO_SHIFT) * (OPM->ST.freqbase*64) * rate);
				cur_chip = NULL;
			}
			break;
		case 0x19:	/* PMD/AMD */
			if( v & 0x80 ) OPM->pmd = v & 0x7f;
			else           OPM->amd = v & 0x7f;
			break;
#endif
		case 0x1b:	/* CT , W  */
			/* b7 = CT1 */
			/* b6 = CT0 */
			/* b0-2 = wave form(LFO) 0=nokogiri,1=houkei,2=sankaku,3=noise */
			//if(OPM->ct != v)
			{
				OPM->ct = v>>6;
				if( OPM->PortWrite != 0)
					OPM->PortWrite(0, OPM->ct ); /* bit0 = CT0,bit1 = CT1 */
			}
#if FM_LFO_SUPPORT
			if( OPM->wavetype != &OPM_LFO_waves[(v&3)*LFO_ENT])
			{
				OPM->wavetype = &OPM_LFO_waves[(v&3)*LFO_ENT];
				cur_chip = NULL;
			}
#endif
			break;
		}
		break;
	case 0x20:	/* 20-3f */
		switch( OPM_SLOT(r) ){
		case 0: /* 0x20-0x27 : RL,FB,CON */
			{
				int feedback = (v>>3)&7;
				CH->ALGO = v&7;
				CH->FB  = feedback ? 8+1 - feedback : 0;
				/* RL order -> LR order */
				CH->PAN = ((v>>7)&1) | ((v>>5)&2);
				setup_connection( CH );
			}
			break;
		case 1: /* 0x28-0x2f : Keycode */
			{
				int blk = (v>>4)&7;
				/* make keyscale code */
				CH->kcode = (v>>2)&0x1f;
				/* make basic increment counter 22bit = 1 cycle */
				CH->fc = (blk * (12*64)) + KC_TO_SEMITONE[v&0x0f] + CH->fn_h;
				CH->SLOT[SLOT1].Incr=-1;
			}
			break;
		case 2: /* 0x30-0x37 : Keyfunction */
			CH->fc -= CH->fn_h;
			CH->fn_h = v>>2;
			CH->fc += CH->fn_h;
			CH->SLOT[SLOT1].Incr=-1;
			break;
#if FM_LFO_SUPPORT
		case 3: /* 0x38-0x3f : PMS / AMS */
			/* b0-1 AMS */
			/* AMS * 23.90625db @ AMD=127 */
			//CH->ams = (v & 0x03) * (23.90625/EG_STEP);
			CH->ams = (UINT32)( (23.90625/EG_STEP) / (1<<(3-(v&3))) );
			CH->SLOT[SLOT1].ams = CH->ams * CH->SLOT[SLOT1].amon;
			CH->SLOT[SLOT2].ams = CH->ams * CH->SLOT[SLOT2].amon;
			CH->SLOT[SLOT3].ams = CH->ams * CH->SLOT[SLOT3].amon;
			CH->SLOT[SLOT4].ams = CH->ams * CH->SLOT[SLOT4].amon;
			/* b4-6 PMS */
			/* 0,5,10,20,50,100,400,700 (cent) @ PMD=127 */
			{
				/* 1 octabe = 1200cent = +100%/-50% */
				/* 100cent  = 1seminote = 6% ?? */
				static const int pmd_table[8] = {0,5,10,20,50,100,400,700};
				CH->pms = (INT32)( (1.5/1200.0)*pmd_table[(v>>4) & 0x07] * PMS_RATE );
			}
			break;
#endif
		}
		break;
	case 0x40:	/* DT1,MUL */
		set_det_mul(&OPM->ST,CH,SLOT,v);
		break;
	case 0x60:	/* TL */
		set_tl(CH,SLOT,v,(OPM->ST.mode & 0x80) );
		break;
	case 0x80:	/* KS, AR */
		set_ar_ksr(CH,SLOT,v,OPM->ST.AR_TABLE);
		break;
	case 0xa0:	/* AMS EN,D1R */
		set_dr(SLOT,v,OPM->ST.DR_TABLE);
#if FM_LFO_SUPPORT
		/* bit7 = AMS ENABLE */
		SLOT->amon = v>>7;
		SLOT->ams = CH->ams * SLOT->amon;
#endif
		break;
	case 0xc0:	/* DT2 ,D2R */
		SLOT->DT2  = DT2_TABLE[v>>6];
		CH->SLOT[SLOT1].Incr=-1;
		set_sr(SLOT,v,OPM->ST.DR_TABLE);
		break;
	case 0xe0:	/* D1L, RR */
		set_sl_rr(SLOT,v,OPM->ST.DR_TABLE);
		break;
    }
}

int YM2151Write(int n,int a,UINT8 v)
{
	YM2151 *F2151 = &(FMOPM[n]);

	if( !(a&1) )
	{	/* address port */
		F2151->ST.address = v & 0xff;
	}
	else
	{	/* data port */
		int addr = F2151->ST.address;
		YM2151UpdateReq(n);
		/* write register */
		 OPMWriteReg(n,addr,v);
	}
	return F2151->ST.irq;
}

/* ---------- reset one of chip ---------- */
void OPMResetChip(int num)
{
	int i;
    YM2151 *OPM = &(FMOPM[num]);

	OPMResetTable( num );
	reset_channel( &OPM->ST , &OPM->CH[0] , 8 );
	/* status clear */
	FM_IRQMASK_SET(&OPM->ST,0x03);
	OPMWriteReg(num,0x1b,0x00);
	/* reset OPerator paramater */
	for(i = 0xff ; i >= 0x20 ; i-- ) OPMWriteReg(num,i,0);
}

/* ----------  Initialize YM2151 emulator(s) ----------    */
/* 'num' is the number of virtual YM2151's to allocate     */
/* 'rate' is sampling rate and 'bufsiz' is the size of the */
/* buffer that should be updated at each interval          */
int OPMInit(int num, int clock, int rate,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler)
{
    int i;

    if (FMOPM) return (-1);	/* duplicate init. */
    cur_chip = NULL;	/* hiro-shi!! */

	YM2151NumChips = num;

	/* allocate ym2151 state space */
	if( (FMOPM = (YM2151 *)malloc(sizeof(YM2151) * YM2151NumChips))==NULL)
		return (-1);

	/* clear */
	memset(FMOPM,0,sizeof(YM2151) * YM2151NumChips);

	/* allocate total lebel table (128kb space) */
	if( !OPMInitTable() )
	{
		free( FMOPM );
		return (-1);
	}
	for ( i = 0 ; i < YM2151NumChips; i++ ) {
		FMOPM[i].ST.index = i;
		FMOPM[i].ST.clock = clock;
		FMOPM[i].ST.rate = rate;
		/* FMOPM[i].ST.irq  = 0; */
		/* FMOPM[i].ST.status = 0; */
		FMOPM[i].ST.timermodel = FM_TIMER_INTERVAL;
		FMOPM[i].ST.freqbase  = rate ? ((double)clock / rate) / 64 : 0;
		FMOPM[i].ST.TimerBase = (double)TIME_ONE_SEC/((double)clock / 64.0);
		/* Extend handler */
		FMOPM[i].ST.Timer_Handler = TimerHandler;
		FMOPM[i].ST.IRQ_Handler   = IRQHandler;
		/* Reset callback handler of CT0/1 */
		FMOPM[i].PortWrite = 0;
		OPMResetChip(i);
	}
	return(0);
}

/* ---------- shut down emurator ----------- */
void OPMShutdown()
{
    if (!FMOPM) return;

	FMCloseTable();
	free(FMOPM);
	FMOPM = NULL;
}

UINT8 YM2151Read(int n,int a)
{
	if( !(a&1) ) return 0;
	else         return FMOPM[n].ST.status;
}

/* ---------- make digital sound data ---------- */
void OPMUpdateOne(int num, INT16 **buffer, int length)
{
	YM2151 *OPM = &(FMOPM[num]);
	int i;
	int amd,pmd;
	FM_CH *ch;
	FMSAMPLE  *bufL,*bufR;

	/* set bufer */
	bufL = buffer[0];
	bufR = buffer[1];

	if( (void *)OPM != cur_chip ){
		cur_chip = (void *)OPM;

		State = &OPM->ST;
		/* channel pointer */
		cch[0] = &OPM->CH[0];
		cch[1] = &OPM->CH[1];
		cch[2] = &OPM->CH[2];
		cch[3] = &OPM->CH[3];
		cch[4] = &OPM->CH[4];
		cch[5] = &OPM->CH[5];
		cch[6] = &OPM->CH[6];
		cch[7] = &OPM->CH[7];
		/* ch7.op4 noise mode / step */
		NoiseIncr = OPM->NoiseIncr;
		NoiseCnt  = OPM->NoiseCnt;
#if FM_LFO_SUPPORT
		/* LFO */
		LFOCnt  = OPM->LFOCnt;
		//LFOIncr = OPM->LFOIncr;
		if( !LFOIncr ) lfo_amd = lfo_pmd = 0;
		OPM_LFO_wave = OPM->wavetype;
#endif
	}
	amd = OPM->amd;
	pmd = OPM->pmd;
	if(amd==0 && pmd==0)
		LFOIncr = 0;
	else
		LFOIncr = OPM->LFOIncr;

	OPM_CALC_FCOUNT( OPM , cch[0] );
	OPM_CALC_FCOUNT( OPM , cch[1] );
	OPM_CALC_FCOUNT( OPM , cch[2] );
	OPM_CALC_FCOUNT( OPM , cch[3] );
	OPM_CALC_FCOUNT( OPM , cch[4] );
	OPM_CALC_FCOUNT( OPM , cch[5] );
	OPM_CALC_FCOUNT( OPM , cch[6] );
	OPM_CALC_FCOUNT( OPM , cch[7] );

	for( i=0; i < length ; i++ )
	{
#if FM_LFO_SUPPORT
		/* LFO */
		if( LFOIncr )
		{
			INT32 depth = OPM_LFO_wave[(LFOCnt+=LFOIncr)>>LFO_SHIFT];
			lfo_amd = depth * amd;
			lfo_pmd = (depth-(LFO_RATE/127/2)) * pmd;
		}
#endif
		/* clear output acc. */
		out_ch[OUTD_LEFT] = out_ch[OUTD_RIGHT]= out_ch[OUTD_CENTER] = 0;
		/* calcrate channel output */
		for(ch = cch[0] ; ch <= cch[6] ; ch++)
			FM_CALC_CH( ch );
		OPM_CALC_CH7( cch[7] );
		/* buffering */
		FM_BUFFERING_STEREO;
		/* timer A controll */
		INTERNAL_TIMER_A( State , cch[7] )
    }
	INTERNAL_TIMER_B(State,length)
	OPM->NoiseCnt = NoiseCnt;
#if FM_LFO_SUPPORT
	OPM->LFOCnt = LFOCnt;
#endif
}

void OPMSetPortHander(int n,mem_write_handler PortWrite)
{
	FMOPM[n].PortWrite = PortWrite;
}

int YM2151TimerOver(int n,int c)
{
	YM2151 *F2151 = &(FMOPM[n]);

	if( c )
	{	/* Timer B */
		TimerBOver( &(F2151->ST) );
	}
	else
	{	/* Timer A */
		YM2151UpdateReq(n);
		/* timer update */
		TimerAOver( &(F2151->ST) );
		/* CSM mode key,TL controll */
		if( F2151->ST.mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			CSMKeyControll( &(F2151->CH[0]) );
			CSMKeyControll( &(F2151->CH[1]) );
			CSMKeyControll( &(F2151->CH[2]) );
			CSMKeyControll( &(F2151->CH[3]) );
			CSMKeyControll( &(F2151->CH[4]) );
			CSMKeyControll( &(F2151->CH[5]) );
			CSMKeyControll( &(F2151->CH[6]) );
			CSMKeyControll( &(F2151->CH[7]) );
		}
	}
	return F2151->ST.irq;
}

#endif /* BUILD_YM2151 */
