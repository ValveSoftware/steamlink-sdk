/*$DEADSERIOUSCLAN$*********************************************************************
* FILE
*	Yamaha 3812 emulator interface - MAME VERSION
*
* CREATED BY
*	Ernesto Corvi
*
* UPDATE LOG
*	CHS 1999-01-09	Fixes new ym3812 emulation interface.
*	CHS 1998-10-23	Mame streaming sound chip update
*	EC	1998		Created Interface
*
* NOTES
*
***************************************************************************************/
#include "driver.h"
#include "3812intf.h"
#include "fm.h"

#define OPL3CONVERTFREQUENCY

/* This frequency is from Yamaha 3812 and 2413 documentation */
#define ym3812_StdClock 3579545


/* Emulated YM3812 variables and defines */
static int stream[MAX_3812];
static void *Timer[MAX_3812*2];

/* Non-Emulated YM3812 variables and defines */
typedef struct non_emu3812_state {
	int address_register;
	unsigned char status_register;
	unsigned char timer_register;
	unsigned int timer1_val;
	unsigned int timer2_val;
	void *timer1;
	void *timer2;
	int aOPLFreqArray[16];		/* Up to 9 channels.. */
}NE_OPL_STATE;

static timer_tm timer_step;
static NE_OPL_STATE *nonemu_state;

/* These ones are used by both */
/*static const struct YM3812interface *intf = NULL; */
static const struct Y8950interface *intf = NULL;

/* Function procs to access the selected YM type */
/* static int ( *sh_start )( const struct MachineSound *msound ); */
static void ( *sh_stop )( void );
static int ( *status_port_r )( int chip );
static void ( *control_port_w )( int chip, int data );
static void ( *write_port_w )( int chip, int data );
static int ( *read_port_r )( int chip );

/**********************************************************************************************
	Begin of non-emulated YM3812 interface block
 **********************************************************************************************/

static void timer1_callback (int chip)
{
	NE_OPL_STATE *st = &nonemu_state[chip];
	if (!(st->timer_register & 0x40))
	{
		if(!(st->status_register&0x80))
			if (intf->handler[chip]) (intf->handler[chip])(ASSERT_LINE);
		/* set the IRQ and timer 1 signal bits */
		st->status_register |= 0x80|0x40;
	}

	/* next! */
	st->timer1 = timer_set ((timer_tm)st->timer1_val*4*timer_step, chip, timer1_callback);
}

static void timer2_callback (int chip)
{
	NE_OPL_STATE *st = &nonemu_state[chip];
	if (!(st->timer_register & 0x20))
	{
		if(!(st->status_register&0x80))
			if (intf->handler[chip]) (intf->handler[chip])(ASSERT_LINE);
		/* set the IRQ and timer 2 signal bits */
		st->status_register |= 0x80|0x20;
	}

	/* next! */
	st->timer2 = timer_set ((timer_tm)st->timer2_val*16*timer_step, chip, timer2_callback);
}

static int nonemu_YM3812_sh_start(const struct MachineSound *msound)
{
	int i;

	intf = (const struct Y8950interface *)msound->sound_interface;

	nonemu_state = (NE_OPL_STATE*)malloc(intf->num * sizeof(NE_OPL_STATE) );
	if(nonemu_state==NULL) return 1;
	memset(nonemu_state,0,intf->num * sizeof(NE_OPL_STATE));
	for(i=0;i<intf->num;i++)
	{
		nonemu_state[i].address_register = 0;
		nonemu_state[i].timer1 =
		nonemu_state[i].timer2 = 0;
		nonemu_state[i].status_register = 0;
		nonemu_state[i].timer_register = 0;
		nonemu_state[i].timer1_val =
		nonemu_state[i].timer2_val = 256;
	}
	timer_step = TIME_IN_HZ((float)intf->baseclock / 72.0);
	return 0;
}

static void nonemu_YM3812_sh_stop(void)
{
	YM3812_sh_reset();
	free(nonemu_state);
}

static int nonemu_YM3812_status_port_r(int chip)
{
	NE_OPL_STATE *st = &nonemu_state[chip];
	/* mask out the timer 1 and 2 signal bits as requested by the timer register */
	return st->status_register & ~(st->timer_register & 0x60);
}

static void nonemu_YM3812_control_port_w(int chip,int data)
{
	NE_OPL_STATE *st = &nonemu_state[chip];
	st->address_register = data;

	/* pass through all non-timer registers */
#ifdef OPL3CONVERTFREQUENCY
	if ( ((data==0xbd)||((data&0xe0)!=0xa0)) && ((data<2)||(data>4)) )
#else
	if ( ((data<2)||(data>4)) )
#endif
		osd_opl_control(chip,data);
}

static void nonemu_WriteConvertedFrequency( int chip,int nFrq, int nCh )
{
	int		nRealOctave;
	int	vRealFrq;

	vRealFrq = (((nFrq&0x3ff)<<((nFrq&0x7000)>>12))) * (float)intf->baseclock / (float)ym3812_StdClock;
	nRealOctave = 0;

	while( (vRealFrq>1023.0)&&(nRealOctave<7) )
	{
		vRealFrq /= 2.0;
		nRealOctave++;
	}
	osd_opl_control(chip,0xa0|nCh);
	osd_opl_write(chip,((int)vRealFrq)&0xff);
	osd_opl_control(chip,0xb0|nCh);
	osd_opl_write(chip,((((int)vRealFrq)>>8)&3)|(nRealOctave<<2)|((nFrq&0x8000)>>10) );
}

static void nonemu_YM3812_write_port_w(int chip,int data)
{
	NE_OPL_STATE *st = &nonemu_state[chip];
	int nCh = st->address_register&0x0f;

#ifdef OPL3CONVERTFREQUENCY
	if( (nCh<9) )
	{
		if( (st->address_register&0xf0) == 0xa0 )
		{
			st->aOPLFreqArray[nCh] = (st->aOPLFreqArray[nCh] & 0xf300)|(data&0xff);
			nonemu_WriteConvertedFrequency(chip, st->aOPLFreqArray[nCh], nCh );
			return;
		}
		else if( (st->address_register&0xf0)==0xb0 )
		{
			st->aOPLFreqArray[st->address_register&0xf] = (st->aOPLFreqArray[nCh] & 0x00ff)|((data&0x3)<<8)|((data&0x1c)<<10)|((data&0x20)<<10);
			nonemu_WriteConvertedFrequency(chip, st->aOPLFreqArray[nCh], nCh );
			return;
		}
	}
#endif
	switch (st->address_register)
	{
		case 2:
			st->timer1_val = 256 - data;
			break;
		case 3:
			st->timer2_val = 256 - data;
			break;
		case 4:
			/* bit 7 means reset the IRQ signal and status bits, and ignore all the other bits */
			if (data & 0x80)
			{
				if(st->status_register&0x80)
					if (intf->handler[chip]) (intf->handler[chip])(CLEAR_LINE);
				st->status_register = 0;
			}
			else
			{
				/* set the new timer register */
				st->timer_register = data;
					/*  bit 0 starts/stops timer 1 */
				if (data & 0x01)
				{
					if (!st->timer1)
						st->timer1 = timer_set ((timer_tm)st->timer1_val*4*timer_step, chip, timer1_callback);
				}
				else if (st->timer1)
				{
					timer_remove (st->timer1);
					st->timer1 = 0;
				}
				/*  bit 1 starts/stops timer 2 */
				if (data & 0x02)
				{
					if (!st->timer2)
						st->timer2 = timer_set ((timer_tm)st->timer2_val*16*timer_step, chip, timer2_callback);
				}
				else if (st->timer2)
				{
					timer_remove (st->timer2);
					st->timer2 = 0;
				}
				/* bits 5 & 6 clear and mask the appropriate bit in the status register */
				st->status_register &= ~(data & 0x60);

				if(!(st->status_register&0x7f))
				{
					if(st->status_register&0x80)
						if (intf->handler[chip]) (intf->handler[chip])(CLEAR_LINE);
					st->status_register &=0x7f;
				}
			}
			break;
		default:
			osd_opl_write(chip,data);
	}
}

static int nonemu_YM3812_read_port_r( int chip ) {
	return 0;
}

/**********************************************************************************************
	End of non-emulated YM3812 interface block
 **********************************************************************************************/

#include "sound/fmopl.h"

typedef void (*STREAM_HANDLER)(int param,void *buffer,int length);

static int chiptype;
static FM_OPL *F3812[MAX_3812];

/* IRQ Handler */
static void IRQHandler(int n,int irq)
{
	if (intf->handler[n]) (intf->handler[n])(irq ? ASSERT_LINE : CLEAR_LINE);
}

/* update handler */
static void YM3812UpdateHandler(int n, INT16 *buf, int length)
{	YM3812UpdateOne(F3812[n],buf,length); }

#if (HAS_Y8950)
static void Y8950UpdateHandler(int n, INT16 *buf, int length)
{	Y8950UpdateOne(F3812[n],buf,length); }

static unsigned char Y8950PortHandler_r(int chip)
{	return intf->portread[chip](chip); }

static void Y8950PortHandler_w(int chip,unsigned char data)
{	intf->portwrite[chip](chip,data); }

static unsigned char Y8950KeyboardHandler_r(int chip)
{	return intf->keyboardread[chip](chip); }

static void Y8950KeyboardHandler_w(int chip,unsigned char data)
{	intf->keyboardwrite[chip](chip,data); }
#endif

/* Timer overflow callback from timer.c */
static void timer_callback_3812(int param)
{
	int n=param>>1;
	int c=param&1;
	Timer[param] = 0;
	OPLTimerOver(F3812[n],c);
}

/* TimerHandler from fm.c */
static void TimerHandler(int c,timer_tm period)
{
	if( period == 0 )
	{	/* Reset FM Timer */
		if( Timer[c] )
		{
	 		timer_remove (Timer[c]);
			Timer[c] = 0;
		}
	}
	else
	{	/* Start FM Timer */
		Timer[c] = timer_set(period, c, timer_callback_3812 );
	}
}

/************************************************/
/* Sound Hardware Start							*/
/************************************************/
static int emu_YM3812_sh_start(const struct MachineSound *msound)
{
	int i;
	int rate = Machine->sample_rate;

	intf = (const struct Y8950interface *)msound->sound_interface;
	if( intf->num > MAX_3812 ) return 1;

	/* Timer state clear */
	memset(Timer,0,sizeof(Timer));

	/* stream system initialize */
	for (i = 0;i < intf->num;i++)
	{
		/* stream setup */
		char name[40];
		int vol = intf->mixing_level[i];
		/* emulator create */
		F3812[i] = OPLCreate(chiptype,intf->baseclock,rate);
		if(F3812[i] == NULL) return 1;
		/* stream setup */
		sprintf(name,"%s #%d",sound_name(msound),i);
#if (HAS_Y8950)
		/* ADPCM ROM DATA */
		if(chiptype == OPL_TYPE_Y8950)
		{
			F3812[i]->deltat->memory = (unsigned char *)(memory_region(intf->rom_region[i]));
			F3812[i]->deltat->memory_size = memory_region_length(intf->rom_region[i]);
			stream[i] = stream_init(name,vol,rate,i,Y8950UpdateHandler);
			/* port and keyboard handler */
			OPLSetPortHandler(F3812[i],Y8950PortHandler_w,Y8950PortHandler_r,i);
			OPLSetKeyboardHandler(F3812[i],Y8950KeyboardHandler_w,Y8950KeyboardHandler_r,i);
		}
		else
#endif
		stream[i] = stream_init(name,vol,rate,i,YM3812UpdateHandler);
		/* YM3812 setup */
		OPLSetTimerHandler(F3812[i],TimerHandler,i*2);
		OPLSetIRQHandler(F3812[i]  ,IRQHandler,i);
		OPLSetUpdateHandler(F3812[i],stream_update,stream[i]);
	}
	return 0;
}

/************************************************/
/* Sound Hardware Stop							*/
/************************************************/
static void emu_YM3812_sh_stop(void)
{
	int i;

	for (i = 0;i < intf->num;i++)
	{
		OPLDestroy(F3812[i]);
	}
}

/* reset */
/*
static void emu_YM3812_sh_reset(void)
{
	int i;

	for (i = 0;i < intf->num;i++)
		OPLResetChip(F3812[i]);
}
*/

static int emu_YM3812_status_port_r(int chip)
{
	return OPLRead(F3812[chip],0);
}
static void emu_YM3812_control_port_w(int chip,int data)
{
	OPLWrite(F3812[chip],0,data);
}
static void emu_YM3812_write_port_w(int chip,int data)
{
	OPLWrite(F3812[chip],1,data);
}

static int emu_YM3812_read_port_r(int chip)
{
	return OPLRead(F3812[chip],1);
}

/**********************************************************************************************
	Begin of YM3812 interface stubs block
 **********************************************************************************************/

static int OPL_sh_start(const struct MachineSound *msound)
{
	if ( options.use_emulated_ym3812 ) {
		sh_stop  = emu_YM3812_sh_stop;
		status_port_r = emu_YM3812_status_port_r;
		control_port_w = emu_YM3812_control_port_w;
		write_port_w = emu_YM3812_write_port_w;
		read_port_r = emu_YM3812_read_port_r;
		return emu_YM3812_sh_start(msound);
	} else {
		sh_stop = nonemu_YM3812_sh_stop;
		status_port_r = nonemu_YM3812_status_port_r;
		control_port_w = nonemu_YM3812_control_port_w;
		write_port_w = nonemu_YM3812_write_port_w;
		read_port_r = nonemu_YM3812_read_port_r;
		return nonemu_YM3812_sh_start(msound);
	}
}

int YM3812_sh_start(const struct MachineSound *msound)
{
	chiptype = OPL_TYPE_YM3812;
	return OPL_sh_start(msound);
}

void YM3812_sh_stop( void ) {
	(*sh_stop)();
}

void YM3812_sh_reset(void)
{
	int i;

	for(i=0xff;i<=0;i--)
	{
		YM3812_control_port_0_w(0,i);
		YM3812_write_port_0_w(0,0);
	}
	/* IRQ clear */
	YM3812_control_port_0_w(0,4);
	YM3812_write_port_0_w(0,0x80);
}

WRITE_HANDLER( YM3812_control_port_0_w ) {
	(*control_port_w)( 0, data );
}

WRITE_HANDLER( YM3812_write_port_0_w ) {
	(*write_port_w)( 0, data );
}

READ_HANDLER( YM3812_status_port_0_r ) {
	return (*status_port_r)( 0 );
}

READ_HANDLER( YM3812_read_port_0_r ) {
	return (*read_port_r)( 0 );
}

WRITE_HANDLER( YM3812_control_port_1_w ) {
	(*control_port_w)( 1, data );
}

WRITE_HANDLER( YM3812_write_port_1_w ) {
	(*write_port_w)( 1, data );
}

READ_HANDLER( YM3812_status_port_1_r ) {
	return (*status_port_r)( 1 );
}

READ_HANDLER( YM3812_read_port_1_r ) {
	return (*read_port_r)( 1 );
}

/**********************************************************************************************
	End of YM3812 interface stubs block
 **********************************************************************************************/

/**********************************************************************************************
	Begin of YM3526 interface stubs block
 **********************************************************************************************/
int YM3526_sh_start(const struct MachineSound *msound)
{
	chiptype = OPL_TYPE_YM3526;
	return OPL_sh_start(msound);
}

/**********************************************************************************************
	End of YM3526 interface stubs block
 **********************************************************************************************/

/**********************************************************************************************
	Begin of Y8950 interface stubs block
 **********************************************************************************************/
#if (HAS_Y8950)
int Y8950_sh_start(const struct MachineSound *msound)
{
	chiptype = OPL_TYPE_Y8950;
	if( OPL_sh_start(msound) ) return 1;
	/* !!!!! port handler set !!!!! */
	/* !!!!! delta-t memory address set !!!!! */
	return 0;
}
#endif

/**********************************************************************************************
	End of Y8950 interface stubs block
 **********************************************************************************************/
