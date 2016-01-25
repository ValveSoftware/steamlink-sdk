/* don't support sampling rythm sound yet */
//#define YM2608_USE_SAMPLES
/***************************************************************************

  2608intf.c

  The YM2608 emulator supports up to 2 chips.
  Each chip has the following connections:
  - Status Read / Control Write A
  - Port Read / Data Write A
  - Control Write B
  - Data Write B

***************************************************************************/

#include "driver.h"
#include "ay8910.h"
#include "2608intf.h"
#include "fm.h"

#ifdef BUILD_YM2608

/* use FM.C with stream system */

static int stream[MAX_2608];

static signed short *rhythm_buf;

/* Global Interface holder */
static const struct YM2608interface *intf;

static void *Timer[MAX_2608][2];

#ifdef YM2608_USE_SAMPLES
static const char *ym2608_pDrumNames[] =
{
	"2608_BD.wav",
	"2608_SD.wav",
	"2608_TOP.wav",
	"2608_HH.wav",
	"2608_TOM.wav",
	"2608_RIM.wav",
	0
};
#endif

/*------------------------- TM2608 -------------------------------*/
/* IRQ Handler */
static void IRQHandler(int n,int irq)
{
	if(intf->handler[n]) intf->handler[n](irq);
}

/* Timer overflow callback from timer.c */
static void timer_callback_2608(int param)
{
	int n=param&0x7f;
	int c=param>>7;

//	logerror("2608 TimerOver %d\n",c);
	Timer[n][c] = 0;
	YM2608TimerOver(n,c);
}

/* TimerHandler from fm.c */
static void TimerHandler(int n,int c,int count,timer_tm stepTime)
{
	if( count == 0 )
	{	/* Reset FM Timer */
		if( Timer[n][c] )
		{
//			logerror("2608 TimerReset %d\n",c);
	 		timer_remove (Timer[n][c]);
			Timer[n][c] = 0;
		}
	}
	else
	{	/* Start FM Timer */
		timer_tm timeSec = (timer_tm)count * stepTime;

		if( Timer[n][c] == 0 )
		{
			Timer[n][c] = timer_set (timeSec , (c<<7)|n, timer_callback_2608 );
		}
	}
}

static void FMTimerInit( void )
{
	int i;

	for( i = 0 ; i < MAX_2608 ; i++ )
		Timer[i][0] = Timer[i][1] = 0;
}

/* update request from fm.c */
void YM2608UpdateRequest(int chip)
{
	stream_update(stream[chip],100);
}

int YM2608_sh_start(const struct MachineSound *msound)
{
	int i,j;
	int rate = Machine->sample_rate;
	char buf[YM2608_NUMBUF][40];
	const char *name[YM2608_NUMBUF];
	int mixed_vol,vol[YM2608_NUMBUF];
	void *pcmbufa[YM2608_NUMBUF];
	int  pcmsizea[YM2608_NUMBUF];
	int rhythm_pos[6+1];
	struct GameSamples	*psSamples;
	int total_size,r_offset,s_size;

	intf = (const struct YM2608interface *)msound->sound_interface;
	if( intf->num > MAX_2608 ) return 1;

	if (AY8910_sh_start(msound)) return 1;

	/* Timer Handler set */
	FMTimerInit();

	/* stream system initialize */
	for (i = 0;i < intf->num;i++)
	{
		/* stream setup */
		mixed_vol = intf->volumeFM[i];
		/* stream setup */
		for (j = 0 ; j < YM2608_NUMBUF ; j++)
		{
			name[j]=buf[j];
			vol[j] = mixed_vol & 0xffff;
			mixed_vol>>=16;
			sprintf(buf[j],"%s #%d Ch%d",sound_name(msound),i,j+1);
		}
		stream[i] = stream_init_multi(YM2608_NUMBUF,name,vol,rate,i,YM2608UpdateOne);
		/* setup adpcm buffers */
		pcmbufa[i]  = (void *)(memory_region(intf->pcmrom[i]));
		pcmsizea[i] = memory_region_length(intf->pcmrom[i]);
	}

	/* rythm rom build */
	rhythm_buf = 0;
#ifdef YM2608_USE_SAMPLES
	psSamples = readsamples(ym2608_pDrumNames,"ym2608");
#else
	psSamples = 0;
#endif
	if( psSamples )
	{
		/* calcrate total data size */
		total_size = 0;
		for( i=0;i<6;i++)
		{
			s_size = psSamples->sample[i]->length;
			total_size += s_size ? s_size : 1;
		}
		/* aloocate rythm data */
		rhythm_buf = (short int*)malloc(total_size * sizeof(signed short));
		if( rhythm_buf==0 ) return 0;

		r_offset = 0;
		/* merge sampling data */
		for(i=0;i<6;i++)
		{
			/* set start point */
			rhythm_pos[i] = r_offset*2;
			/* copy sample data */
			s_size = psSamples->sample[i]->length;
			if(s_size && psSamples->sample[i]->data)
			{
				if( psSamples->sample[i]->resolution==16 )
				{
					signed short *s_ptr = (signed short *)psSamples->sample[i]->data;
					for(j=0;j<s_size;j++) rhythm_buf[r_offset++] = *s_ptr++;
				}else{
					signed char *s_ptr = (signed char *)psSamples->sample[i]->data;
					for(j=0;j<s_size;j++) rhythm_buf[r_offset++] = (*s_ptr++)*0x0101;
				}
			}else rhythm_buf[r_offset++] = 0;
			/* set end point */
			rhythm_pos[i+1] = r_offset*2;
		}
		freesamples( psSamples );
	}else
	{
		/* aloocate rythm data */
		rhythm_buf = (short int*)malloc(6 * sizeof(signed short));
		if( rhythm_buf==0 ) return 0;
		for(i=0;i<6;i++)
		{
			/* set start point */
			rhythm_pos[i] = i*2;
			rhythm_buf[i] = 0;
			/* set end point */
			rhythm_pos[i+1] = (i+1)*2;
		}
	}

	/**** initialize YM2608 ****/
	if (YM2608Init(intf->num,intf->baseclock,rate,
		           pcmbufa,pcmsizea,rhythm_buf,rhythm_pos,
		           TimerHandler,IRQHandler) == 0)
		return 0;

	/* error */
	return 1;
}

/************************************************/
/* Sound Hardware Stop							*/
/************************************************/
void YM2608_sh_stop(void)
{
	YM2608Shutdown();
	if( rhythm_buf ) free(rhythm_buf);
	rhythm_buf = 0;
}
/* reset */
void YM2608_sh_reset(void)
{
	int i;

	for (i = 0;i < intf->num;i++)
		YM2608ResetChip(i);
}

/************************************************/
/* Status Read for YM2608 - Chip 0				*/
/************************************************/
READ_HANDLER( YM2608_status_port_0_A_r )
{
//logerror("PC %04x: 2608 S0A=%02X\n",cpu_get_pc(),YM2608Read(0,0));
	return YM2608Read(0,0);
}

READ_HANDLER( YM2608_status_port_0_B_r )
{
//logerror("PC %04x: 2608 S0B=%02X\n",cpu_get_pc(),YM2608Read(0,2));
	return YM2608Read(0,2);
}

/************************************************/
/* Status Read for YM2608 - Chip 1				*/
/************************************************/
READ_HANDLER( YM2608_status_port_1_A_r ) {
	return YM2608Read(1,0);
}

READ_HANDLER( YM2608_status_port_1_B_r ) {
	return YM2608Read(1,2);
}

/************************************************/
/* Port Read for YM2608 - Chip 0				*/
/************************************************/
READ_HANDLER( YM2608_read_port_0_r ){
	return YM2608Read(0,1);
}

/************************************************/
/* Port Read for YM2608 - Chip 1				*/
/************************************************/
READ_HANDLER( YM2608_read_port_1_r ){
	return YM2608Read(1,1);
}

/************************************************/
/* Control Write for YM2608 - Chip 0			*/
/* Consists of 2 addresses						*/
/************************************************/
WRITE_HANDLER( YM2608_control_port_0_A_w )
{
	YM2608Write(0,0,data);
}

WRITE_HANDLER( YM2608_control_port_0_B_w )
{
	YM2608Write(0,2,data);
}

/************************************************/
/* Control Write for YM2608 - Chip 1			*/
/* Consists of 2 addresses						*/
/************************************************/
WRITE_HANDLER( YM2608_control_port_1_A_w ){
	YM2608Write(1,0,data);
}

WRITE_HANDLER( YM2608_control_port_1_B_w ){
	YM2608Write(1,2,data);
}

/************************************************/
/* Data Write for YM2608 - Chip 0				*/
/* Consists of 2 addresses						*/
/************************************************/
WRITE_HANDLER( YM2608_data_port_0_A_w )
{
	YM2608Write(0,1,data);
}

WRITE_HANDLER( YM2608_data_port_0_B_w )
{
	YM2608Write(0,3,data);
}

/************************************************/
/* Data Write for YM2608 - Chip 1				*/
/* Consists of 2 addresses						*/
/************************************************/
WRITE_HANDLER( YM2608_data_port_1_A_w ){
	YM2608Write(1,1,data);
}
WRITE_HANDLER( YM2608_data_port_1_B_w ){
	YM2608Write(1,3,data);
}

/**************** end of file ****************/

#endif
