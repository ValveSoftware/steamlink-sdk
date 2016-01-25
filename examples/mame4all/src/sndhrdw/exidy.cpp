#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "machine/6821pia.h"
#include "sound/hc55516.h"
#include "sound/tms5220.h"


#define BASE_FREQ 			1789773
#define BASE_TIME 			(1000000000.0f / ((float)BASE_FREQ / 2000000.0f))
#define E_CLOCK				(11289000/16)
#define CVSD_CLOCK_FREQ 	(1000000.0f / 34.0f)

#define RIOT_IDLE 0
#define RIOT_COUNTUP 1
#define RIOT_COUNTDOWN 2


/* 6532 variables */
static void *riot_timer;
static UINT8 riot_irq_flag;
static UINT8 riot_irq_enable;
static UINT8 riot_porta_data;
static UINT8 riot_porta_ddr;
static UINT8 riot_portb_data;
static UINT8 riot_portb_ddr;
static UINT32 riot_divider;
static UINT8 riot_state;

/* 6840 variables */
static UINT8 sh6840_CR[3];
static UINT8 sh6840_MSB;
static UINT16 sh6840_count[3];
static UINT16 sh6840_timer[3];
static UINT8 exidy_sfxctrl;

/* 8253 variables */
static UINT16 sh8253_count[3];
static int sh8253_clstate[3];

/* 5220/CVSD variables */
static UINT8 has_hc55516;
static UINT8 has_tms5220;

/* sound streaming variables */
struct channel_data
{
	UINT8	enable;
	UINT8	noisy;
	INT16	volume;
	UINT32	step;
	UINT32	fraction;
};
static int exidy_stream;
static float freq_to_step;
static struct channel_data music_channel[3];
static struct channel_data sfx_channel[3];



/***************************************************************************
	PIA Interface
***************************************************************************/

static void exidy_irq(int state);

WRITE_HANDLER(victory_sound_response_w);
WRITE_HANDLER(victory_sound_irq_clear_w);
WRITE_HANDLER(victory_main_ack_w);

/* PIA 0 */
static struct pia6821_interface pia_0_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ 0, 0, 0, 0, 0, 0,
	/*outputs: A/B,CA/B2       */ pia_1_portb_w, pia_1_porta_w, pia_1_cb1_w, pia_1_ca1_w,
	/*irqs   : A/B             */ 0, 0
};

/* PIA 1 */
static struct pia6821_interface pia_1_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ 0, 0, 0, 0, 0, 0,
	/*outputs: A/B,CA/B2       */ pia_0_portb_w, pia_0_porta_w, pia_0_cb1_w, pia_0_ca1_w,
	/*irqs   : A/B             */ 0, exidy_irq
};

/* Victory PIA 0 */
static struct pia6821_interface victory_pia_0_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ 0, 0, 0, 0, 0, 0,
	/*outputs: A/B,CA/B2       */ 0, victory_sound_response_w, victory_sound_irq_clear_w, victory_main_ack_w,
	/*irqs   : A/B             */ 0, exidy_irq
};


/**************************************************************************
    Start/Stop Sound
***************************************************************************/

static void exidy_stream_update(int param, INT16 *buffer, int length)
{
	float noise_freq=0;
	int chan, i;

	/* reset */
	memset(buffer, 0, length * sizeof(buffer[0]));

	/* if any channels are noisy, generate the noise wave */
	if (sfx_channel[0].noisy || sfx_channel[1].noisy || sfx_channel[2].noisy)
	{
		/* if noise is clocked by the E, just generate at max frequency */
		if (!(exidy_sfxctrl & 1))
			noise_freq = (float)E_CLOCK;

		/* otherwise, generate noise clocked by channel 1 of the 6840 */
		else if (sfx_channel[0].enable)
			noise_freq = (sh6840_timer[0]) ? ((float)BASE_FREQ / (float)sh6840_timer[0] * 0.5) : 0;

		/* if channel 1 isn't enabled, zap the buffer */
		else
			noise_freq = 0;
	}

	/* process sfx channels first */
	for (chan = 0; chan < 3; chan++)
	{
		struct channel_data *c = &sfx_channel[chan];

		/* only process enabled channels */
		if (c->enable)
		{
			/* special case channel 0: sfxctl controls its output */
			if (chan == 0 && (exidy_sfxctrl & 2))
				c->fraction += length * c->step;

			/* otherwise, generate normally: non-noisy case first */
			else if (!c->noisy)
			{
				UINT32 frac = c->fraction, step = c->step;
				INT16 vol = c->volume;
				for (i = 0; i < length; i++)
				{
					if (frac & 0x1000000)
						buffer[i] += vol;
					frac += step;
				}
				c->fraction = frac;
			}

			/* noisy case - determine the effective noise step */
			else
			{
				/*
					Explanation of noise

					The noise source can be clocked by 1 of 2 sources, depending on sfxctrl bit 0

						(sfxctrl & 1) == 0	--> clock = E
						(sfxctrl & 1) != 0	--> clock = 6840 channel 0

					The noise source then becomes the clock for any channels using the external
					clock. On average, the frequency of the clock for that channel should be
					1/4 of the noise frequency, with a random variance on each step. The external
					clock still causes the timer to count, so we must further divide by the
					timer's count value in order to determine the final frequency. To simulate
					the variance, we compute the effective step value, and then apply a random
					offset to it after each sample is generated
				*/
				UINT32 avgstep = (sh6840_timer[chan]) ? freq_to_step * (noise_freq * 0.25) / (float)sh6840_timer[chan] : 0;
				UINT32 frac = c->fraction;
				INT16 vol = c->volume;

				avgstep /= 32768;
				for (i = 0; i < length; i++)
				{
					if (frac & 0x1000000)
						buffer[i] += vol;
					/* add two random values to get a distribution that is weighted toward the middle */
					frac += ((rand() & 32767) + (rand() & 32767)) * avgstep;
				}
				c->fraction = frac;
			}
		}
	}

	/* process music channels second */
	for (chan = 0; chan < 3; chan++)
	{
		struct channel_data *c = &music_channel[chan];

		/* only process enabled channels */
		if (c->enable)
		{
			UINT32 step = c->step;
			UINT32 frac = c->fraction;

			for (i = 0; i < length; i++)
			{
				if (frac & 0x0800000)
					buffer[i] += c->volume;
				frac += step;
			}
			c->fraction = frac;
		}
	}
}


static int common_start(void)
{
	int i;

	/* determine which sound hardware is installed */
	has_hc55516 = 0;
	has_tms5220 = 0;
	for (i = 0; i < MAX_SOUND; i++)
	{
		if (Machine->drv->sound[i].sound_type == SOUND_TMS5220)
			has_tms5220 = 1;
		if (Machine->drv->sound[i].sound_type == SOUND_HC55516)
			has_hc55516 = 1;
	}

	/* allocate the stream */
	exidy_stream = stream_init("Exidy custom", 100, Machine->sample_rate, 0, exidy_stream_update);

	/* compute the frequency-to-step conversion factor */
	if (Machine->sample_rate != 0)
		freq_to_step = (float)(1 << 24) / (float)Machine->sample_rate;
	else
		freq_to_step = 0.0;

	/* initialize the sound channels */
	memset(music_channel, 0, sizeof(music_channel));
	memset(sfx_channel, 0, sizeof(sfx_channel));
	music_channel[0].volume = music_channel[1].volume = music_channel[2].volume = 32767 / 6;
	music_channel[0].step = music_channel[1].step = music_channel[2].step = 0;
	sfx_channel[0].step = sfx_channel[1].step = sfx_channel[2].step = 0;

	/* Init PIA */
	pia_reset();

	/* Init 6532 */
    riot_timer = 0;
    riot_irq_flag = 0;
    riot_irq_enable = 0;
	riot_porta_data = 0xff;
	riot_portb_data = 0xff;
    riot_divider = 1;
    riot_state = RIOT_IDLE;

	/* Init 6840 */
	sh6840_MSB = 0;
	sh6840_CR[0] = sh6840_CR[1] = sh6840_CR[2] = 0;
	sh6840_timer[0] = sh6840_timer[1] = sh6840_timer[2] = 0;
	exidy_sfxctrl = 0;

	/* Init 8253 */
	sh8253_count[0]   = sh8253_count[1]   = sh8253_count[2]   = 0;
	sh8253_clstate[0] = sh8253_clstate[1] = sh8253_clstate[2] = 0;

	return 0;
}


int exidy_sh_start(const struct MachineSound *msound)
{
	/* Init PIA */
	pia_config(0, PIA_STANDARD_ORDERING, &pia_0_intf);
	pia_config(1, PIA_STANDARD_ORDERING, &pia_1_intf);
	return common_start();
}


int victory_sh_start(const struct MachineSound *msound)
{
	/* Init PIA */
	pia_config(0, PIA_STANDARD_ORDERING, &victory_pia_0_intf);
	pia_0_cb1_w(0, 1);
	return common_start();
}


/*
 *  PIA callback to generate the interrupt to the main CPU
 */

static void exidy_irq(int state)
{
    cpu_set_irq_line(1, 0, state ? ASSERT_LINE : CLEAR_LINE);
}


/**************************************************************************
    6532 RIOT
***************************************************************************/

static void riot_interrupt(int parm)
{
	if (riot_state == RIOT_COUNTUP)
	{
		riot_irq_flag |= 0x80; /* set timer interrupt flag */
		if (riot_irq_enable) cpu_set_irq_line(1, M6502_INT_IRQ, ASSERT_LINE);
		riot_state = RIOT_COUNTDOWN;
		riot_timer = timer_set(TIME_IN_USEC(BASE_TIME * 0xFF), 0, riot_interrupt);
	}
	else
	{
		riot_timer = 0;
		riot_state = RIOT_IDLE;
	}
}


WRITE_HANDLER( exidy_shriot_w )
{
	offset &= 0x7f;
	switch (offset)
	{
		case 0:	/* port A */
			if (has_hc55516) cpu_set_reset_line(2, (data & 0x10) ? CLEAR_LINE : ASSERT_LINE);
			riot_porta_data = (riot_porta_data & ~riot_porta_ddr) | (data & riot_porta_ddr);
			return;

		case 1:	/* port A DDR */
			riot_porta_ddr = data;
			break;

		case 2:	/* port B */
			if (has_tms5220)
			{
				if (!(data & 0x01) && (riot_portb_data & 0x01))
				{
					riot_porta_data = tms5220_status_r(0);
				}
				if ((data & 0x02) && !(riot_portb_data & 0x02))
				{
					tms5220_data_w(0, riot_porta_data);
				}
			}
			riot_portb_data = (riot_portb_data & ~riot_portb_ddr) | (data & riot_portb_ddr);
			return;

		case 3:	/* port B DDR */
			riot_portb_ddr = data;
			break;

		case 7: /* 0x87 - Enable Interrupt on PA7 Transitions */
			return;

		case 0x14:
		case 0x1c:
			cpu_set_irq_line(1, M6502_INT_IRQ, CLEAR_LINE);
			riot_irq_enable = offset & 0x08;
			riot_divider = 1;
			if (riot_timer) timer_remove(riot_timer);
			riot_timer = timer_set(TIME_IN_USEC((riot_divider * BASE_TIME) * data), 0, riot_interrupt);
			riot_state = RIOT_COUNTUP;
			return;

		case 0x15:
		case 0x1d:
			cpu_set_irq_line(1, M6502_INT_IRQ, CLEAR_LINE);
			riot_irq_enable = offset & 0x08;
			riot_divider = 8;
			if (riot_timer) timer_remove(riot_timer);
			riot_timer = timer_set(TIME_IN_USEC((riot_divider * BASE_TIME) * data), 0, riot_interrupt);
			riot_state = RIOT_COUNTUP;
			return;

		case 0x16:
		case 0x1e:
			cpu_set_irq_line(1, M6502_INT_IRQ, CLEAR_LINE);
			riot_irq_enable = offset & 0x08;
			riot_divider = 64;
			if (riot_timer) timer_remove(riot_timer);
			riot_timer = timer_set(TIME_IN_USEC((riot_divider * BASE_TIME) * data), 0, riot_interrupt);
			riot_state = RIOT_COUNTUP;
			return;

		case 0x17:
		case 0x1f:
			cpu_set_irq_line(1, M6502_INT_IRQ, CLEAR_LINE);
			riot_irq_enable = offset & 0x08;
			riot_divider = 1024;
			if (riot_timer) timer_remove(riot_timer);
			riot_timer = timer_set(TIME_IN_USEC((riot_divider * BASE_TIME) * data), 0, riot_interrupt);
			riot_state = RIOT_COUNTUP;
			return;

		default:
			//logerror("Undeclared RIOT write: %x=%x\n",offset,data);
			return;
	}
	return; /* will never execute this */
}


READ_HANDLER( exidy_shriot_r )
{
	static int temp;

	offset &= 7;
	switch (offset)
	{
		case 0x00:
			return riot_porta_data;

		case 0x01:	/* port A DDR */
			return riot_porta_ddr;

		case 0x02:
			if (has_tms5220)
			{
				riot_portb_data &= ~0x0c;
				if (!tms5220_ready_r()) riot_portb_data |= 0x04;
				if (!tms5220_int_r()) riot_portb_data |= 0x08;
			}
			return riot_portb_data;

		case 0x03:	/* port B DDR */
			return riot_portb_ddr;

		case 0x05: /* 0x85 - Read Interrupt Flag Register */
		case 0x07:
			temp = riot_irq_flag;
			riot_irq_flag = 0;   /* Clear int flags */
			cpu_set_irq_line(1, M6502_INT_IRQ, CLEAR_LINE);
			return temp;

		case 0x04:
		case 0x06:
			riot_irq_flag = 0;
			cpu_set_irq_line(1, M6502_INT_IRQ, CLEAR_LINE);
			if (riot_state == RIOT_COUNTUP)
				return timer_timeelapsed(riot_timer) / TIME_IN_USEC(riot_divider * BASE_TIME);
			else
				return timer_timeleft(riot_timer) / TIME_IN_USEC(riot_divider * BASE_TIME);

		default:
			//logerror("Undeclared RIOT read: %x  PC:%x\n",offset,cpu_get_pc());
			return 0xff;
	}
	return 0;
}


/**************************************************************************
    8253 Timer
***************************************************************************/

WRITE_HANDLER( exidy_sh8253_w )
{
	int chan;

	stream_update(exidy_stream, 0);

	offset &= 3;
	switch (offset)
	{
		case 0:
		case 1:
		case 2:
			chan = offset;
			if (!sh8253_clstate[chan])
			{
				sh8253_clstate[chan] = 1;
				sh8253_count[chan] = (sh8253_count[chan] & 0xff00) | (data & 0x00ff);
			}
			else
			{
				sh8253_clstate[chan] = 0;
				sh8253_count[chan] = (sh8253_count[chan] & 0x00ff) | ((data << 8) & 0xff00);
				if (sh8253_count[chan])
					music_channel[chan].step = freq_to_step * (float)BASE_FREQ / (float)sh8253_count[chan];
				else
					music_channel[chan].step = 0;
			}
			break;

		case 3:
			chan = (data & 0xc0) >> 6;
			music_channel[chan].enable = ((data & 0x0e) != 0);
			break;
	}
}


READ_HANDLER( exidy_sh8253_r )
{
    //logerror("8253(R): %x\n",offset);
	return 0;
}


/**************************************************************************
    6840 Timer
***************************************************************************/

READ_HANDLER( exidy_sh6840_r )
{
    //logerror("6840R %x\n",offset);
    return 0;
}


WRITE_HANDLER( exidy_sh6840_w )
{
	int ch;

	stream_update(exidy_stream, 0);

	offset &= 7;
	switch (offset)
	{
		case 0:
			if (sh6840_CR[1] & 0x01)
				sh6840_CR[0] = data;
			else
				sh6840_CR[2] = data;
			break;

		case 1:
			sh6840_CR[1] = data;
			break;

		case 2:
		case 4:
		case 6:
			sh6840_MSB = data;
			break;

		case 3:
		case 5:
		case 7:
			ch = (offset - 3) / 2;
			sh6840_count[ch] = sh6840_timer[ch] = (sh6840_MSB << 8) | (data & 0xff);
			if (sh6840_timer[ch])
				sfx_channel[ch].step = freq_to_step * (float)BASE_FREQ / (float)sh6840_timer[ch];
			else
				sfx_channel[ch].step = 0;
			break;
	}

	sfx_channel[0].enable = ((sh6840_CR[0] & 0x80) != 0 && sh6840_timer[0] != 0);
	sfx_channel[1].enable = ((sh6840_CR[1] & 0x80) != 0 && sh6840_timer[1] != 0);
	sfx_channel[2].enable = ((sh6840_CR[2] & 0x80) != 0 && sh6840_timer[2] != 0);

	sfx_channel[0].noisy = ((sh6840_CR[0] & 0x02) == 0);
	sfx_channel[1].noisy = ((sh6840_CR[1] & 0x02) == 0);
	sfx_channel[2].noisy = ((sh6840_CR[2] & 0x02) == 0);
}


/**************************************************************************
    Special Sound FX Control
***************************************************************************/

WRITE_HANDLER( exidy_sfxctrl_w )
{
	stream_update(exidy_stream, 0);

	offset &= 3;
	switch (offset)
	{
		case 0:
			exidy_sfxctrl = data;
			break;

		case 1:
		case 2:
		case 3:
			sfx_channel[offset - 1].volume = ((data & 7) * (32767 / 6)) / 7;
			break;
	}
}


/**************************************************************************
    Mousetrap Digital Sound
***************************************************************************/

WRITE_HANDLER( mtrap_voiceio_w )
{
    if (!(offset & 0x10))
    {
    	hc55516_digit_clock_clear_w(0,data);
    	hc55516_clock_set_w(0,data);
	}
    if (!(offset & 0x20))
		riot_portb_data = data & 1;
}


READ_HANDLER( mtrap_voiceio_r )
{
	if (!(offset & 0x80))
	{
       int data = (riot_porta_data & 0x06) >> 1;
       data |= (riot_porta_data & 0x01) << 2;
       data |= (riot_porta_data & 0x08);
       return data;
	}
    if (!(offset & 0x40))
    {
    	int clock_pulse = (int)(( ((float)timer_get_time())) * (2.0f * CVSD_CLOCK_FREQ));
    	return (clock_pulse & 1) << 7;
	}
	return 0;
}
