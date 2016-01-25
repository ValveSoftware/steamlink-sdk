#include "driver.h"
#include "cpu/z80/z80.h"


#define VERBOSE 0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif


/* interrupt states */
static UINT8 z80_irq_enabled = 0, z8002_1_nvi_enabled = 0, z8002_2_nvi_enabled = 0;

/* ADC states */
static UINT8 adc_input = 0;

/* protection states */
static INT16 ic25_last_result;
static UINT8 ic25_last_signed;
static UINT8 ic25_last_unsigned;

/* 4-bit MCU state */
static struct polepos_mcu_def
{
	int		enabled;			/* Enabled */
	int		status;				/* Status */
	int		transfer_id;		/* Transfer id */
	void	*timer;				/* Transfer timer */
	int		coin1_coinpercred;	/* Coinage info */
	int		coin1_credpercoin;	/* Coinage info */
	int		coin2_coinpercred;	/* Coinage info */
	int		coin2_credpercoin;	/* Coinage info */
	int		mode;				/* Mode ( 0 = read switches, 1 = Coinage, 2 = in-game ) */
	int		credits;			/* Credits count */
	int		start;				/* Start switch */
} polepos_mcu;

/* Prototypes */
static void z80_interrupt(int scanline);
void polepos_sample_play(int sample); /* from sndhrdw */

/*************************************************************************************/
/* Interrupt handling                                                                */
/*************************************************************************************/

void polepos_init_machine(void)
{
	/* reset all the interrupt states */
	z80_irq_enabled = z8002_1_nvi_enabled = z8002_2_nvi_enabled = 0;

	/* reset the ADC state */
	adc_input = 0;

	/* reset the protection state */
	ic25_last_result = 0;
	ic25_last_signed = 0;
	ic25_last_unsigned = 0;

	/* Initialize the MCU */
	polepos_mcu.enabled = 0; /* disabled */
	polepos_mcu.status = 0x10; /* ready to transfer */
	polepos_mcu.transfer_id = 0; /* clear out the transfer id */
	polepos_mcu.timer = 0;
	polepos_mcu.start = 0;

	/* halt the two Z8002 cpus */
	cpu_set_reset_line(1, ASSERT_LINE);
	cpu_set_reset_line(2, ASSERT_LINE);

	/* start a timer for the Z80's interrupt */
	timer_set(cpu_getscanlinetime(0), 0, z80_interrupt);
}

static void z80_interrupt(int scanline)
{
	cpu_set_irq_line(0, 0, ((scanline & 64) == 0) ? ASSERT_LINE : CLEAR_LINE);
	scanline += 64;
	if (scanline >= 256) scanline = 0;
	timer_set(cpu_getscanlinetime(scanline), scanline, z80_interrupt);
}

WRITE_HANDLER( polepos_z80_irq_enable_w )
{
	z80_irq_enabled = data & 1;
	if ((data & 1) == 0) cpu_set_irq_line(0, 0, CLEAR_LINE);
}

WRITE_HANDLER( polepos_z8002_nvi_enable_w )
{
	int which = (offset / 2) + 1;

	if (which == cpu_getactivecpu())
	{
		if (which == 1)
			z8002_1_nvi_enabled = data & 1;
		else
			z8002_2_nvi_enabled = data & 1;
		if ((data & 1) == 0) cpu_set_irq_line(which, 0, CLEAR_LINE);
	}
	LOG(("Z8K#%d cpu%d_nvi_enable_w $%02x\n", cpu_getactivecpu(), which, data));
}

int polepos_z8002_1_interrupt(void)
{
	if (z8002_1_nvi_enabled)
		cpu_set_irq_line(1, 0, ASSERT_LINE);

	return ignore_interrupt();
}

int polepos_z8002_2_interrupt(void)
{
	if (z8002_2_nvi_enabled)
		cpu_set_irq_line(2, 0, ASSERT_LINE);

	return ignore_interrupt();
}

WRITE_HANDLER( polepos_z8002_enable_w )
{
	if (data & 1)
		cpu_set_reset_line(offset + 1, CLEAR_LINE);
	else
		cpu_set_reset_line(offset + 1, ASSERT_LINE);
}


/*************************************************************************************/
/* I/O and ADC handling                                                              */
/*************************************************************************************/

WRITE_HANDLER( polepos_adc_select_w )
{
	adc_input = data;
}

READ_HANDLER( polepos_adc_r )
{
	int ret = 0;

	switch (adc_input)
	{
		case 0x00:
			ret = readinputport(3);
			break;

		case 0x01:
			ret = readinputport(4);
			break;

		default:
			LOG(("Unknown ADC Input select (%02x)!\n", adc_input));
			break;
	}

	return ret;
}

READ_HANDLER( polepos_io_r )
{
	int ret = 0xff;

	if (cpu_getscanline() >= 128)
		ret ^= 0x02;

	ret ^= 0x08; /* ADC End Flag */

	return ret;
}


/*************************************************************************************/
/* Pole Position II protection                                                       */
/*************************************************************************************/

READ_HANDLER( polepos2_ic25_r )
{
	int result;

	offset = offset & 0x3ff;
	if (offset < 0x200)
	{
		ic25_last_signed = (offset / 2) & 0xff;
		result = ic25_last_result & 0xff;
	}
	else
	{
		ic25_last_unsigned = (offset / 2) & 0xff;
		result = (ic25_last_result >> 8) & 0xff;
		ic25_last_result = (INT8)ic25_last_signed * (UINT8)ic25_last_unsigned;
	}

	//logerror("%04X: read IC25 @ %04X = %02X\n", cpu_get_pc(), offset, result);

	return result | (result << 8);
}



/*************************************************************************************/
/* 4 bit cpu emulation                                                               */
/*************************************************************************************/

WRITE_HANDLER( polepos_mcu_enable_w )
{
	polepos_mcu.enabled = data & 1;

	if (polepos_mcu.enabled == 0)
	{
		/* If its getting disabled, kill our timer */
		if (polepos_mcu.timer)
		{
			timer_remove(polepos_mcu.timer);
			polepos_mcu.timer = 0;
		}
	}
}

static void polepos_mcu_callback(int param)
{
	cpu_cause_interrupt(0, Z80_NMI_INT);
}

READ_HANDLER( polepos_mcu_control_r )
{
	if (polepos_mcu.enabled)
		return polepos_mcu.status;

	return 0x00;
}

WRITE_HANDLER( polepos_mcu_control_w )
{
	LOG(("polepos_mcu_control_w: %d, $%02x\n", offset, data));

    if (polepos_mcu.enabled)
    {
		if (data != 0x10)
		{
			/* start transfer */
			polepos_mcu.transfer_id = data; /* get the id */
			polepos_mcu.status = 0xe0; 		/* set status */
			if (polepos_mcu.timer)
				timer_remove(polepos_mcu.timer);
			/* fire off the transfer timer */
			polepos_mcu.timer = timer_pulse(TIME_IN_USEC(50), 0, polepos_mcu_callback);
		}
		else
		{
			/* end transfer */
			if (polepos_mcu.timer) /* shut down our transfer timer */
				timer_remove(polepos_mcu.timer);
			polepos_mcu.timer = 0;
			polepos_mcu.status = 0x10; /* set status */
		}
	}
}

READ_HANDLER( polepos_mcu_data_r )
{
	if (polepos_mcu.enabled)
	{
		LOG(("MCU read: PC = %04x, transfer mode = %02x, offset = %02x\n", cpu_get_pc(), polepos_mcu.transfer_id & 0xff, offset ));

		switch(polepos_mcu.transfer_id)
		{
			case 0x71: /* 3 bytes */
				switch (offset)
				{
					case 0x00:
						if ( polepos_mcu.mode == 0 )
							return ~( readinputport(0) ^ polepos_mcu.start ); /* Service, Gear, etc */
						else {
							static int last_in = 0;
							int in;
							static int coin1inserted;
							static int coin2inserted;

							in = readinputport(0);

							/* check if the user inserted a coin */
							if (polepos_mcu.coin1_coinpercred > 0)
							{
								if ( ( last_in ^ in ) & 0x10 ) {
									if ((in & 0x10) == 0 && polepos_mcu.credits < 99)
									{
										coin1inserted++;
										if (coin1inserted >= polepos_mcu.coin1_coinpercred)
										{
											polepos_mcu.credits += polepos_mcu.coin1_credpercoin;
											coin1inserted = 0;
										}
									}
								}

								if ( ( last_in ^ in ) & 0x20 ) {
									if ((in & 0x20) == 0 && polepos_mcu.credits < 99)
									{
										coin2inserted++;
										if (coin2inserted >= polepos_mcu.coin2_coinpercred)
										{
											polepos_mcu.credits += polepos_mcu.coin2_credpercoin;
											coin2inserted = 0;
										}
									}
								}
							}
							else polepos_mcu.credits = 100; /* freeplay */

							last_in = in;

							return (polepos_mcu.credits / 10) * 16 + polepos_mcu.credits % 10;
						}
					case 0x01:
						return ~readinputport(2); /* DSW1 */
					case 0x02:
						return ~( ( readinputport(0) & 2 ) << 4 );
				}
				break;

			case 0x72: /* 8 bytes */
				switch (offset)
				{
					case 0x00: /* Steering */
						return readinputport(5);

					case 0x04:
						return ~readinputport(1); /* DSW 0 */
				}
				break;

			case 0x91: /* 3 bytes */
				/*
				   Same as 0x71 but sets coinage mode? - Program seems only to check offset 0 and make sure
				   it's 0 or 0xa0 (freeplay?), otherwise, it resets itself.
				*/
				case 0x00:
					polepos_mcu.mode = 1;
					if ( polepos_mcu.coin1_coinpercred > 0 )
						polepos_mcu.credits = 0;
					else
						polepos_mcu.credits = 100; /* freeplay */
					return (polepos_mcu.credits / 10) * 16 + polepos_mcu.credits % 10;
				break;

			default:
				//logerror("Unknwon MCU transfer mode: %02x\n", polepos_mcu.transfer_id);
				break;
		}
	}

	return 0xff; /* pull up */
}

WRITE_HANDLER( polepos_mcu_data_w )
{
	if (polepos_mcu.enabled)
	{
		LOG(("MCU write: PC = %04x, transfer mode = %02x, offset = %02x, data = %02x\n", cpu_get_pc(), polepos_mcu.transfer_id & 0xff, offset, data ));

		if ( polepos_mcu.transfer_id == 0xa1 ) { /* setup coins/credits, etc ( 8 bytes ) */
			switch( offset ) {
				case 1:
					polepos_mcu.coin1_coinpercred = data;
					break;

				case 2:
					polepos_mcu.coin1_credpercoin = data;
					break;

				case 3:
					polepos_mcu.coin2_coinpercred = data;
					break;

				case 4:
					polepos_mcu.coin2_credpercoin = data;
					break;

				/* NOTE: I still have no clue what offset 0, 5, 6 & 7 do */
			}
		}

		if ( polepos_mcu.transfer_id == 0xc1 ) { /* set switch mode */
			polepos_mcu.mode = 0;
		}

		if ( polepos_mcu.transfer_id == 0x84 ) { /* play sample */
			if ( offset == 0 ) {
				switch( data ) {
					case 0x01:
						polepos_sample_play( 0 );
					break;

					case 0x02:
						polepos_sample_play( 1 );
					break;

					case 0x04:
						polepos_sample_play( 2 );
					break;

					default:
						//logerror("Unknown sample triggered (%d)\n", data );
					break;
				}
			}
		}

		if ( polepos_mcu.transfer_id == 0x88 ) { /* play screech/explosion */
			if ( offset == 0 ) {
				/* 0x40 = Start Explosion sample */
				/* 0x20 = ???? */
				/* 0x7n = Screech sound. n = pitch (if 0 then no sound) */
				if ( data == 0x40 )
					sample_start( 0, 0, 0 );

				if ( ( data & 0xf0 ) == 0x70 ) {
					if ( ( data & 0x0f ) == 0 ) {
						if ( sample_playing(1) )
							sample_stop(1);
					} else {
						int freq = (int)( ( 44100.0f / 10.0f ) * (float)(data & 0x0f) );

						if ( !sample_playing(1) )
							sample_start( 1, 1, 1 );
						sample_set_freq(1, freq);
					}
				}
			}
		}

		if ( polepos_mcu.transfer_id == 0x81 ) { /* Set coinage mode */
			polepos_mcu.mode = 1;
		}
	}
}

WRITE_HANDLER( polepos_start_w )
{
	static int last_start = 0;

	data &= 1;

	polepos_mcu.start = data << 2;

	/* check for start button if not in-game */
	if ( polepos_mcu.mode != 2 ) {
		if ( ( last_start ^ data ) && ( data == 0 ) ) {
			if (polepos_mcu.credits >= 1) {
				polepos_mcu.mode = 2; /* in-game */
				polepos_mcu.credits--;
			}
		}
	}

	last_start = data;
}
