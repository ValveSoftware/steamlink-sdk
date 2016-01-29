#ifndef SNDINTRF_H
#define SNDINTRF_H


struct MachineSound
{
	int sound_type;
	void *sound_interface;
};


#include "sound/mixer.h"
#include "sound/streams.h"

#if (HAS_SAMPLES)
#include "sound/samples.h"
#endif
#if (HAS_DAC)
#include "sound/dac.h"
#endif
#if (HAS_DISCRETE)
#include "sound/discrete.h"
#endif
#if (HAS_AY8910)
#include "sound/ay8910.h"
#endif
#if (HAS_YM2203)
#include "sound/2203intf.h"
#endif
#if (HAS_YM2608)
#include "sound/2608intf.h"
#endif
#if (HAS_YM2612 || HAS_YM3438)
#include "sound/2612intf.h"
#endif
#if (HAS_YM2151 || HAS_YM2151_ALT)
#include "sound/2151intf.h"
#endif
#if (HAS_YM2608)
#include "sound/2608intf.h"
#endif
#if (HAS_YM2610 || HAS_YM2610B)
#include "sound/2610intf.h"
#endif
#if (HAS_YM3812 || HAS_YM3526 || HAS_Y8950)
#include "sound/3812intf.h"
#endif
#if (HAS_YM2413)
#include "sound/2413intf.h"
#endif
#if (HAS_YMZ280B)
#include "sound/ymz280b.h"
#endif
#if (HAS_SN76477)
#include "sound/sn76477.h"
#endif
#if (HAS_SN76496)
#include "sound/sn76496.h"
#endif
#if (HAS_POKEY)
#include "sound/pokey.h"
#endif
#if (HAS_TIA)
#include "sound/tiaintf.h"
#endif
#if (HAS_NES)
#include "sound/nes_apu.h"
#endif
#if (HAS_ASTROCADE)
#include "sound/astrocde.h"
#endif
#if (HAS_NAMCO)
#include "sound/namco.h"
#endif
#if (HAS_TMS36XX)
#include "sound/tms36xx.h"
#endif
#if (HAS_TMS5220)
#include "sound/5220intf.h"
#endif
#if (HAS_VLM5030)
#include "sound/vlm5030.h"
#endif
#if (HAS_ADPCM || HAS_OKIM6295)
#include "sound/adpcm.h"
#endif
#if (HAS_MSM5205)
#include "sound/msm5205.h"
#endif
#if (HAS_UPD7759)
#include "sound/upd7759.h"
#endif
#if (HAS_HC55516)
#include "sound/hc55516.h"
#endif
#if (HAS_K005289)
#include "sound/k005289.h"
#endif
#if (HAS_K007232)
#include "sound/k007232.h"
#endif
#if (HAS_K051649)
#include "sound/k051649.h"
#endif
#if (HAS_K053260)
#include "sound/k053260.h"
#endif
#if (HAS_K054539)
#include "sound/k054539.h"
#endif
#if (HAS_SEGAPCM)
#include "sound/segapcm.h"
#endif
#if (HAS_RF5C68)
#include "sound/rf5c68.h"
#endif
#if (HAS_CEM3394)
#include "sound/cem3394.h"
#endif
#if (HAS_C140)
#include "sound/c140.h"
#endif
#if (HAS_QSOUND)
#include "sound/qsound.h"
#endif
#if (HAS_SPEAKER)
#include "sound/speaker.h"
#endif
#if (HAS_WAVE)
#include "sound/wave.h"
#endif


enum
{
	SOUND_DUMMY,
#if (HAS_CUSTOM)
	SOUND_CUSTOM,
#endif
#if (HAS_SAMPLES)
	SOUND_SAMPLES,
#endif
#if (HAS_DAC)
	SOUND_DAC,
#endif
#if (HAS_DISCRETE)
        SOUND_DISCRETE,
#endif
#if (HAS_AY8910)
	SOUND_AY8910,
#endif
#if (HAS_YM2203)
	SOUND_YM2203,
#endif
#if (HAS_YM2151)
	SOUND_YM2151,
#endif
#if (HAS_YM2151_ALT)
	SOUND_YM2151,
#endif
#if (HAS_YM2608)
	SOUND_YM2608,
#endif
#if (HAS_YM2610)
	SOUND_YM2610,
#endif
#if (HAS_YM2610B)
	SOUND_YM2610B,
#endif
#if (HAS_YM2612)
	SOUND_YM2612,
#endif
#if (HAS_YM3438)
	SOUND_YM3438,	/* same as YM2612 */
#endif
#if (HAS_YM2413)
	SOUND_YM2413,	/* YM3812 with predefined instruments */
#endif
#if (HAS_YM3812)
	SOUND_YM3812,
#endif
#if (HAS_YM3526)
	SOUND_YM3526,	/* 100% YM3812 compatible, less features */
#endif
#if (HAS_YMZ280B)
	SOUND_YMZ280B,
#endif
#if (HAS_Y8950)
	SOUND_Y8950,	/* YM3526 compatible with delta-T ADPCM */
#endif
#if (HAS_SN76477)
	SOUND_SN76477,
#endif
#if (HAS_SN76496)
	SOUND_SN76496,
#endif
#if (HAS_POKEY)
	SOUND_POKEY,
#endif
#if (HAS_TIA)
	SOUND_TIA,		/* stripped down Pokey */
#endif
#if (HAS_NES)
	SOUND_NES,
#endif
#if (HAS_ASTROCADE)
	SOUND_ASTROCADE,	/* Custom I/O chip from Bally/Midway */
#endif
#if (HAS_NAMCO)
	SOUND_NAMCO,
#endif
#if (HAS_TMS36XX)
	SOUND_TMS36XX,		/* currently TMS3615 and TMS3617 */
#endif
#if (HAS_TMS5220)
	SOUND_TMS5220,
#endif
#if (HAS_VLM5030)
	SOUND_VLM5030,
#endif
#if (HAS_ADPCM)
	SOUND_ADPCM,
#endif
#if (HAS_OKIM6295)
	SOUND_OKIM6295,	/* ROM-based ADPCM system */
#endif
#if (HAS_MSM5205)
	SOUND_MSM5205,	/* CPU-based ADPCM system */
#endif
#if (HAS_UPD7759)
	SOUND_UPD7759,	/* ROM-based ADPCM system */
#endif
#if (HAS_HC55516)
	SOUND_HC55516,	/* Harris family of CVSD CODECs */
#endif
#if (HAS_K005289)
	SOUND_K005289,	/* Konami 005289 */
#endif
#if (HAS_K007232)
	SOUND_K007232,	/* Konami 007232 */
#endif
#if (HAS_K051649)
	SOUND_K051649,	/* Konami 051649 */
#endif
#if (HAS_K053260)
	SOUND_K053260,	/* Konami 053260 */
#endif
#if (HAS_K054539)
	SOUND_K054539,
#endif
#if (HAS_SEGAPCM)
	SOUND_SEGAPCM,
#endif
#if (HAS_RF5C68)
	SOUND_RF5C68,
#endif
#if (HAS_CEM3394)
	SOUND_CEM3394,
#endif
#if (HAS_C140)
	SOUND_C140,
#endif
#if (HAS_QSOUND)
	SOUND_QSOUND,
#endif
#if (HAS_SPEAKER)
	SOUND_SPEAKER,
#endif
#if (HAS_WAVE)
	SOUND_WAVE,
#endif
    SOUND_COUNT
};


/* structure for SOUND_CUSTOM sound drivers */
struct CustomSound_interface
{
	int (*sh_start)(const struct MachineSound *msound);
	void (*sh_stop)(void);
	void (*sh_update)(void);
};


int sound_start(void);
void sound_stop(void);
void sound_update(void);
void sound_reset(void);

/* returns name of the sound system */
const char *sound_name(const struct MachineSound *msound);
/* returns number of chips, or 0 if the sound type doesn't support multiple instances */
int sound_num(const struct MachineSound *msound);
/* returns clock rate, or 0 if the sound type doesn't support a clock frequency */
int sound_clock(const struct MachineSound *msound);

int sound_scalebufferpos(int value);


WRITE_HANDLER( soundlatch_w );
READ_HANDLER( soundlatch_r );
WRITE_HANDLER( soundlatch_clear_w );
WRITE_HANDLER( soundlatch2_w );
READ_HANDLER( soundlatch2_r );
WRITE_HANDLER( soundlatch2_clear_w );
WRITE_HANDLER( soundlatch3_w );
READ_HANDLER( soundlatch3_r );
WRITE_HANDLER( soundlatch3_clear_w );
WRITE_HANDLER( soundlatch4_w );
READ_HANDLER( soundlatch4_r );
WRITE_HANDLER( soundlatch4_clear_w );

/* If you're going to use soundlatchX_clear_w, and the cleared value is
   something other than 0x00, use this function from machine_init. Note
   that this one call effects all 4 latches */
void soundlatch_setclearedvalue(int value);


#endif
