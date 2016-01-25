#include "driver.h"


/***************************************************************************

  Many games use a master-slave CPU setup. Typically, the main CPU writes
  a command to some register, and then writes to another register to trigger
  an interrupt on the slave CPU (the interrupt might also be triggered by
  the first write). The slave CPU, notified by the interrupt, goes and reads
  the command.

***************************************************************************/

static int cleared_value = 0x00;

static int latch;

static INT64 fps;

static void soundlatch_callback(int param)
{
	latch = param;
}

WRITE_HANDLER( soundlatch_w )
{
	/* make all the CPUs synchronize, and only AFTER that write the new command to the latch */
	timer_set(TIME_NOW,data,soundlatch_callback);
}

READ_HANDLER( soundlatch_r )
{
	return latch;
}

WRITE_HANDLER( soundlatch_clear_w )
{
	latch = cleared_value;
}


static int latch2;

static void soundlatch2_callback(int param)
{
	latch2 = param;
}

WRITE_HANDLER( soundlatch2_w )
{
	/* make all the CPUs synchronize, and only AFTER that write the new command to the latch */
	timer_set(TIME_NOW,data,soundlatch2_callback);
}

READ_HANDLER( soundlatch2_r )
{
	return latch2;
}

WRITE_HANDLER( soundlatch2_clear_w )
{
	latch2 = cleared_value;
}


static int latch3;

static void soundlatch3_callback(int param)
{
	latch3 = param;
}

WRITE_HANDLER( soundlatch3_w )
{
	/* make all the CPUs synchronize, and only AFTER that write the new command to the latch */
	timer_set(TIME_NOW,data,soundlatch3_callback);
}

READ_HANDLER( soundlatch3_r )
{
	return latch3;
}

WRITE_HANDLER( soundlatch3_clear_w )
{
	latch3 = cleared_value;
}


static int latch4;

static void soundlatch4_callback(int param)
{
	latch4 = param;
}

WRITE_HANDLER( soundlatch4_w )
{
	/* make all the CPUs synchronize, and only AFTER that write the new command to the latch */
	timer_set(TIME_NOW,data,soundlatch4_callback);
}

READ_HANDLER( soundlatch4_r )
{
	return latch4;
}

WRITE_HANDLER( soundlatch4_clear_w )
{
	latch4 = cleared_value;
}


void soundlatch_setclearedvalue(int value)
{
	cleared_value = value;
}





/***************************************************************************



***************************************************************************/

static void *sound_update_timer;

struct snd_interface
{
	unsigned sound_num;										/* ID */
	const char *name;										/* description */
	int (*chips_num)(const struct MachineSound *msound);	/* returns number of chips if applicable */
	int (*chips_clock)(const struct MachineSound *msound);	/* returns chips clock if applicable */
	int (*start)(const struct MachineSound *msound);		/* starts sound emulation */
	void (*stop)(void);										/* stops sound emulation */
	void (*update)(void);									/* updates emulation once per frame if necessary */
	void (*reset)(void);									/* resets sound emulation */
};


#if (HAS_CUSTOM)
static const struct CustomSound_interface *cust_intf;

int custom_sh_start(const struct MachineSound *msound)
{
	cust_intf = (const struct CustomSound_interface *)msound->sound_interface;

	if (cust_intf->sh_start)
		return (*cust_intf->sh_start)(msound);
	else return 0;
}
void custom_sh_stop(void)
{
	if (cust_intf->sh_stop) (*cust_intf->sh_stop)();
}
void custom_sh_update(void)
{
	if (cust_intf->sh_update) (*cust_intf->sh_update)();
}
#endif
#if (HAS_DAC)
int DAC_num(const struct MachineSound *msound) { return ((struct DACinterface*)msound->sound_interface)->num; }
#endif
#if (HAS_ADPCM)
int ADPCM_num(const struct MachineSound *msound) { return ((struct ADPCMinterface*)msound->sound_interface)->num; }
#endif
#if (HAS_OKIM6295)
int OKIM6295_num(const struct MachineSound *msound) { return ((struct OKIM6295interface*)msound->sound_interface)->num; }
int OKIM6295_clock(const struct MachineSound *msound) { return ((struct OKIM6295interface*)msound->sound_interface)->frequency[0]; }
#endif
#if (HAS_MSM5205)
int MSM5205_num(const struct MachineSound *msound) { return ((struct MSM5205interface*)msound->sound_interface)->num; }
#endif
#if (HAS_HC55516)
int HC55516_num(const struct MachineSound *msound) { return ((struct hc55516_interface*)msound->sound_interface)->num; }
#endif
#if (HAS_K007232)
int K007232_num(const struct MachineSound *msound) { return ((struct K007232_interface*)msound->sound_interface)->num_chips; }
#endif
#if (HAS_AY8910)
int AY8910_clock(const struct MachineSound *msound) { return ((struct AY8910interface*)msound->sound_interface)->baseclock; }
int AY8910_num(const struct MachineSound *msound) { return ((struct AY8910interface*)msound->sound_interface)->num; }
#endif
#if (HAS_YM2203)
int YM2203_clock(const struct MachineSound *msound) { return ((struct YM2203interface*)msound->sound_interface)->baseclock; }
int YM2203_num(const struct MachineSound *msound) { return ((struct YM2203interface*)msound->sound_interface)->num; }
#endif
#if (HAS_YM2413)
int YM2413_clock(const struct MachineSound *msound) { return ((struct YM2413interface*)msound->sound_interface)->baseclock; }
int YM2413_num(const struct MachineSound *msound) { return ((struct YM2413interface*)msound->sound_interface)->num; }
#endif
#if (HAS_YM2608)
int YM2608_clock(const struct MachineSound *msound) { return ((struct YM2608interface*)msound->sound_interface)->baseclock; }
int YM2608_num(const struct MachineSound *msound) { return ((struct YM2608interface*)msound->sound_interface)->num; }
#endif
#if (HAS_YM2610)
int YM2610_clock(const struct MachineSound *msound) { return ((struct YM2610interface*)msound->sound_interface)->baseclock; }
int YM2610_num(const struct MachineSound *msound) { return ((struct YM2610interface*)msound->sound_interface)->num; }
#endif
#if (HAS_YM2612 || HAS_YM3438)
int YM2612_clock(const struct MachineSound *msound) { return ((struct YM2612interface*)msound->sound_interface)->baseclock; }
int YM2612_num(const struct MachineSound *msound) { return ((struct YM2612interface*)msound->sound_interface)->num; }
#endif
#if (HAS_POKEY)
int POKEY_clock(const struct MachineSound *msound) { return ((struct POKEYinterface*)msound->sound_interface)->baseclock; }
int POKEY_num(const struct MachineSound *msound) { return ((struct POKEYinterface*)msound->sound_interface)->num; }
#endif
#if (HAS_TIA)
int TIA_clock(const struct MachineSound *msound) { return ((struct TIAinterface*)msound->sound_interface)->baseclock; }
#endif
#if (HAS_YM3812)
int YM3812_clock(const struct MachineSound *msound) { return ((struct YM3812interface*)msound->sound_interface)->baseclock; }
int YM3812_num(const struct MachineSound *msound) { return ((struct YM3812interface*)msound->sound_interface)->num; }
#endif
#if (HAS_YMZ280B)
int YMZ280B_clock(const struct MachineSound *msound) { return ((struct YMZ280Binterface*)msound->sound_interface)->baseclock[0]; }
int YMZ280B_num(const struct MachineSound *msound) { return ((struct YMZ280Binterface*)msound->sound_interface)->num; }
#endif
#if (HAS_VLM5030)
int VLM5030_clock(const struct MachineSound *msound) { return ((struct VLM5030interface*)msound->sound_interface)->baseclock; }
#endif
#if (HAS_TMS36XX)
int TMS36XX_num(const struct MachineSound *msound) { return ((struct TMS36XXinterface*)msound->sound_interface)->num; }
#endif
#if (HAS_TMS5220)
int TMS5220_clock(const struct MachineSound *msound) { return ((struct TMS5220interface*)msound->sound_interface)->baseclock; }
#endif
#if (HAS_YM2151 || HAS_YM2151_ALT)
int YM2151_clock(const struct MachineSound *msound) { return ((struct YM2151interface*)msound->sound_interface)->baseclock; }
int YM2151_num(const struct MachineSound *msound) { return ((struct YM2151interface*)msound->sound_interface)->num; }
#endif
#if (HAS_NES)
int NES_num(const struct MachineSound *msound) { return ((struct NESinterface*)msound->sound_interface)->num; }
#endif
#if (HAS_SN76477)
int SN76477_num(const struct MachineSound *msound) { return ((struct SN76477interface*)msound->sound_interface)->num; }
#endif
#if (HAS_SN76496)
int SN76496_clock(const struct MachineSound *msound) { return ((struct SN76496interface*)msound->sound_interface)->baseclock[0]; }
int SN76496_num(const struct MachineSound *msound) { return ((struct SN76496interface*)msound->sound_interface)->num; }
#endif
#if (HAS_MSM5205)
int MSM5205_clock(const struct MachineSound *msound) { return ((struct MSM5205interface*)msound->sound_interface)->baseclock; }
#endif
#if (HAS_UPD7759)
int UPD7759_clock(const struct MachineSound *msound) { return ((struct UPD7759_interface*)msound->sound_interface)->clock_rate; }
#endif
#if (HAS_ASTROCADE)
int ASTROCADE_clock(const struct MachineSound *msound) { return ((struct astrocade_interface*)msound->sound_interface)->baseclock; }
int ASTROCADE_num(const struct MachineSound *msound) { return ((struct astrocade_interface*)msound->sound_interface)->num; }
#endif
#if (HAS_K051649)
int K051649_clock(const struct MachineSound *msound) { return ((struct k051649_interface*)msound->sound_interface)->master_clock; }
#endif
#if (HAS_K053260)
int K053260_clock(const struct MachineSound *msound) { return ((struct K053260_interface*)msound->sound_interface)->clock; }
#endif
#if (HAS_K054539)
int K054539_clock(const struct MachineSound *msound) { return ((struct K054539interface*)msound->sound_interface)->clock; }
int K054539_num(const struct MachineSound *msound) { return ((struct K054539interface*)msound->sound_interface)->num; }
#endif
#if (HAS_CEM3394)
int cem3394_num(const struct MachineSound *msound) { return ((struct cem3394_interface*)msound->sound_interface)->numchips; }
#endif
#if (HAS_QSOUND)
int qsound_clock(const struct MachineSound *msound) { return ((struct QSound_interface*)msound->sound_interface)->clock; }
#endif
#if (HAS_SPEAKER)
int speaker_num(const struct MachineSound *msound) { return ((struct Speaker_interface*)msound->sound_interface)->num; }
#endif
#if (HAS_WAVE)
int wave_num(const struct MachineSound *msound) { return ((struct Wave_interface*)msound->sound_interface)->num; }
#endif

struct snd_interface sndintf[] =
{
    {
		SOUND_DUMMY,
		"",
		0,
		0,
		0,
		0,
		0,
		0
	},
#if (HAS_CUSTOM)
    {
		SOUND_CUSTOM,
		"Custom",
		0,
		0,
		custom_sh_start,
		custom_sh_stop,
		custom_sh_update,
		0
	},
#endif
#if (HAS_SAMPLES)
    {
		SOUND_SAMPLES,
		"Samples",
		0,
		0,
		samples_sh_start,
		0,
		0,
		0
	},
#endif
#if (HAS_DAC)
    {
		SOUND_DAC,
		"DAC",
		DAC_num,
		0,
		DAC_sh_start,
		0,
		0,
		0
	},
#endif
#if (HAS_DISCRETE)
    {
                SOUND_DISCRETE,
                "Discrete Components",
                0,
                0,
                discrete_sh_start,
                discrete_sh_stop,
                0,
                discrete_sh_reset
        },
#endif
#if (HAS_AY8910)
    {
		SOUND_AY8910,
		"AY-8910",
		AY8910_num,
		AY8910_clock,
		AY8910_sh_start,
		0,
		0,
		0
	},
#endif
#if (HAS_YM2203)
    {
		SOUND_YM2203,
		"YM-2203",
		YM2203_num,
		YM2203_clock,
		YM2203_sh_start,
		YM2203_sh_stop,
		0,
		YM2203_sh_reset
	},
#endif
#if (HAS_YM2151 || HAS_YM2151_ALT)
    {
		SOUND_YM2151,
		"YM-2151",
		YM2151_num,
		YM2151_clock,
		YM2151_sh_start,
		YM2151_sh_stop,
		0,
		YM2151_sh_reset
	},
#endif
#if (HAS_YM2608)
    {
		SOUND_YM2608,
		"YM-2608",
		YM2608_num,
		YM2608_clock,
		YM2608_sh_start,
		YM2608_sh_stop,
		0,
		YM2608_sh_reset
	},
#endif
#if (HAS_YM2610)
    {
		SOUND_YM2610,
		"YM-2610",
		YM2610_num,
		YM2610_clock,
		YM2610_sh_start,
		YM2610_sh_stop,
		0,
		YM2610_sh_reset
	},
#endif
#if (HAS_YM2610B)
    {
		SOUND_YM2610B,
		"YM-2610B",
		YM2610_num,
		YM2610_clock,
		YM2610B_sh_start,
		YM2610_sh_stop,
		0,
		YM2610_sh_reset
	},
#endif
#if (HAS_YM2612)
    {
		SOUND_YM2612,
		"YM-2612",
		YM2612_num,
		YM2612_clock,
		YM2612_sh_start,
		YM2612_sh_stop,
		0,
		YM2612_sh_reset
	},
#endif
#if (HAS_YM3438)
    {
		SOUND_YM3438,
		"YM-3438",
		YM2612_num,
		YM2612_clock,
		YM2612_sh_start,
		YM2612_sh_stop,
		0,
		YM2612_sh_reset
	},
#endif
#if (HAS_YM2413)
    {
		SOUND_YM2413,
		"YM-2413",
		YM2413_num,
		YM2413_clock,
		YM2413_sh_start,
		YM2413_sh_stop,
		0,
		0
	},
#endif
#if (HAS_YM3812)
    {
		SOUND_YM3812,
		"YM-3812",
		YM3812_num,
		YM3812_clock,
		YM3812_sh_start,
		YM3812_sh_stop,
		0,
		0
	},
#endif
#if (HAS_YM3526)
    {
		SOUND_YM3526,
		"YM-3526",
		YM3812_num,
		YM3812_clock,
		YM3812_sh_start,
		YM3812_sh_stop,
		0,
		0
	},
#endif
#if (HAS_YMZ280B)
    {
		SOUND_YMZ280B,
		"YMZ280B",
		YMZ280B_num,
		YMZ280B_clock,
		YMZ280B_sh_start,
		YMZ280B_sh_stop,
		0,
		0
	},
#endif
#if (HAS_Y8950)
	{
		SOUND_Y8950,
		"Y8950",	/* (MSX-AUDIO) */
		YM3812_num,
		YM3812_clock,
		Y8950_sh_start,
		Y8950_sh_stop,
		0,
		0
	},
#endif
#if (HAS_SN76477)
    {
		SOUND_SN76477,
		"SN76477",
		SN76477_num,
		0,
		SN76477_sh_start,
		SN76477_sh_stop,
		0,
		0
	},
#endif
#if (HAS_SN76496)
    {
		SOUND_SN76496,
		"SN76496",
		SN76496_num,
		SN76496_clock,
		SN76496_sh_start,
        0,
		0
	},
#endif
#if (HAS_POKEY)
    {
		SOUND_POKEY,
		"Pokey",
		POKEY_num,
		POKEY_clock,
		pokey_sh_start,
		pokey_sh_stop,
		0,
		0
	},
#endif
#if (HAS_TIA)
    {
		SOUND_TIA,
		"TIA",
		0,
		TIA_clock,
		tia_sh_start,
		tia_sh_stop,
		tia_sh_update,
		0
	},
#endif
#if (HAS_NES)
    {
		SOUND_NES,
		"Nintendo",
		NES_num,
		0,
		NESPSG_sh_start,
		NESPSG_sh_stop,
		NESPSG_sh_update,
		0
	},
#endif
#if (HAS_ASTROCADE)
    {
		SOUND_ASTROCADE,
		"Astrocade",
		ASTROCADE_num,
		ASTROCADE_clock,
		astrocade_sh_start,
		astrocade_sh_stop,
		astrocade_sh_update,
		0
	},
#endif
#if (HAS_NAMCO)
    {
		SOUND_NAMCO,
		"Namco",
		0,
		0,
		namco_sh_start,
		namco_sh_stop,
		0,
		0
	},
#endif
#if (HAS_TMS36XX)
    {
		SOUND_TMS36XX,
		"TMS36XX",
		TMS36XX_num,
        0,
		tms36xx_sh_start,
		tms36xx_sh_stop,
		tms36xx_sh_update,
		0
	},
#endif
#if (HAS_TMS5220)
    {
		SOUND_TMS5220,
		"TMS5520",
		0,
		TMS5220_clock,
		tms5220_sh_start,
		tms5220_sh_stop,
		tms5220_sh_update,
		0
	},
#endif
#if (HAS_VLM5030)
    {
		SOUND_VLM5030,
		"VLM5030",
		0,
		VLM5030_clock,
		VLM5030_sh_start,
		VLM5030_sh_stop,
		VLM5030_sh_update,
		0
	},
#endif
#if (HAS_ADPCM)
    {
		SOUND_ADPCM,
		"ADPCM",
		ADPCM_num,
		0,
		ADPCM_sh_start,
		ADPCM_sh_stop,
		ADPCM_sh_update,
		0
	},
#endif
#if (HAS_OKIM6295)
    {
		SOUND_OKIM6295,
		"OKI6295",
		OKIM6295_num,
		OKIM6295_clock,
		OKIM6295_sh_start,
		OKIM6295_sh_stop,
		OKIM6295_sh_update,
		0
	},
#endif
#if (HAS_MSM5205)
    {
		SOUND_MSM5205,
		"MSM5205",
		MSM5205_num,
		MSM5205_clock,
		MSM5205_sh_start,
		0,
		0,
		MSM5205_sh_reset,
	},
#endif
#if (HAS_UPD7759)
    {
		SOUND_UPD7759,
		"uPD7759",
		0,
		UPD7759_clock,
		UPD7759_sh_start,
		UPD7759_sh_stop,
		0,
		0
	},
#endif
#if (HAS_HC55516)
    {
		SOUND_HC55516,
		"HC55516",
		HC55516_num,
		0,
		hc55516_sh_start,
		0,
		0,
		0
	},
#endif
#if (HAS_K005289)
    {
		SOUND_K005289,
		"005289",
		0,
		0,
		K005289_sh_start,
		K005289_sh_stop,
		0,
		0
	},
#endif
#if (HAS_K007232)
    {
		SOUND_K007232,
		"007232",
		K007232_num,
		0,
		K007232_sh_start,
		0,
		0,
		0
	},
#endif
#if (HAS_K051649)
    {
		SOUND_K051649,
		"051649",
		0,
		K051649_clock,
		K051649_sh_start,
		K051649_sh_stop,
		0,
		0
	},
#endif
#if (HAS_K053260)
    {
		SOUND_K053260,
		"053260",
		0,
		K053260_clock,
		K053260_sh_start,
		K053260_sh_stop,
		0,
		0
	},
#endif
#if (HAS_K054539)
    {
		SOUND_K054539,
		"054539",
		K054539_num,
		K054539_clock,
		K054539_sh_start,
		K054539_sh_stop,
		0,
		0
	},
#endif
#if (HAS_SEGAPCM)
	{
		SOUND_SEGAPCM,
		"Sega PCM",
		0,
		0,
		SEGAPCM_sh_start,
		SEGAPCM_sh_stop,
		SEGAPCM_sh_update,
		0
	},
#endif
#if (HAS_RF5C68)
	{
		SOUND_RF5C68,
		"RF5C68",
		0,
		0,
		RF5C68_sh_start,
		RF5C68_sh_stop,
		0,
		0
	},
#endif
#if (HAS_CEM3394)
	{
		SOUND_CEM3394,
		"CEM3394",
		cem3394_num,
		0,
		cem3394_sh_start,
		cem3394_sh_stop,
		0,
		0
	},
#endif
#if (HAS_C140)
	{
		SOUND_C140,
		"C140",
		0,
		0,
		C140_sh_start,
		C140_sh_stop,
		0,
		0
	},
#endif
#if (HAS_QSOUND)
	{
		SOUND_QSOUND,
		"QSound",
		0,
		qsound_clock,
		qsound_sh_start,
		qsound_sh_stop,
		0,
		0
	},
#endif
#if (HAS_SPEAKER)
	{
		SOUND_SPEAKER,
		"Speaker",
		speaker_num,
		0,
		speaker_sh_start,
		speaker_sh_stop,
		speaker_sh_update,
		0
	},
#endif
#if (HAS_WAVE)
	{
		SOUND_WAVE,
		"Cassette",
		wave_num,
		0,
		wave_sh_start,
		wave_sh_stop,
		wave_sh_update,
		0
	},
#endif
};



int sound_start(void)
{
	int totalsound = 0;
	int i;

	/* Verify the order of entries in the sndintf[] array */
	for (i = 0;i < SOUND_COUNT;i++)
	{
		if (sndintf[i].sound_num != i)
		{
logerror("Sound #%d wrong ID %d: check enum SOUND_... in src/sndintrf.h!\n",i,sndintf[i].sound_num);
			return 1;
		}
	}


	/* samples will be read later if needed */
	Machine->samples = 0;
    fps = (INT64)Machine->drv->frames_per_second;
	sound_update_timer = timer_set(TIME_NEVER,0,NULL);

	if (mixer_sh_start() != 0)
		return 1;

	if (streams_sh_start() != 0)
		return 1;

	while (Machine->drv->sound[totalsound].sound_type != 0 && totalsound < MAX_SOUND)
	{
		if ((*sndintf[Machine->drv->sound[totalsound].sound_type].start)(&Machine->drv->sound[totalsound]) != 0)
			goto getout;

		totalsound++;
	}

	return 0;


getout:
	/* TODO: should also free the resources allocated before */
	return 1;
}



void sound_stop(void)
{
	int totalsound = 0;


	while (Machine->drv->sound[totalsound].sound_type != 0 && totalsound < MAX_SOUND)
	{
		if (sndintf[Machine->drv->sound[totalsound].sound_type].stop)
			(*sndintf[Machine->drv->sound[totalsound].sound_type].stop)();

		totalsound++;
	}

	streams_sh_stop();
	mixer_sh_stop();

	if (sound_update_timer)
	{
		timer_remove(sound_update_timer);
		sound_update_timer = 0;
	}

	/* free audio samples */
	freesamples(Machine->samples);
	Machine->samples = 0;
}



void sound_update(void)
{
	int totalsound = 0;


	profiler_mark(PROFILER_SOUND);

	while (Machine->drv->sound[totalsound].sound_type != 0 && totalsound < MAX_SOUND)
	{
		if (sndintf[Machine->drv->sound[totalsound].sound_type].update)
			(*sndintf[Machine->drv->sound[totalsound].sound_type].update)();

		totalsound++;
	}

	streams_sh_update();
	mixer_sh_update();

	timer_reset(sound_update_timer,TIME_NEVER);

	profiler_mark(PROFILER_END);
}


void sound_reset(void)
{
	int totalsound = 0;


	while (Machine->drv->sound[totalsound].sound_type != 0 && totalsound < MAX_SOUND)
	{
		if (sndintf[Machine->drv->sound[totalsound].sound_type].reset)
			(*sndintf[Machine->drv->sound[totalsound].sound_type].reset)();

		totalsound++;
	}
}



const char *sound_name(const struct MachineSound *msound)
{
	if (msound->sound_type < SOUND_COUNT)
		return sndintf[msound->sound_type].name;
	else
		return "";
}

int sound_num(const struct MachineSound *msound)
{
	if (msound->sound_type < SOUND_COUNT && sndintf[msound->sound_type].chips_num)
		return (*sndintf[msound->sound_type].chips_num)(msound);
	else
		return 0;
}

int sound_clock(const struct MachineSound *msound)
{
	if (msound->sound_type < SOUND_COUNT && sndintf[msound->sound_type].chips_clock)
		return (*sndintf[msound->sound_type].chips_clock)(msound);
	else
		return 0;
}


int sound_scalebufferpos(int value)
{
	int result = ( ((INT64)timer_timeelapsed(sound_update_timer)) * fps * ((INT64)value) ) / ((INT64)TIME_ONE_SEC);
	if (value >= 0) return (result < value) ? result : value;
	else return (result > value) ? result : value;
}
