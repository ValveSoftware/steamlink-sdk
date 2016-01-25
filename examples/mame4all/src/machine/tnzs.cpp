/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

  The I8742 MCU takes care of handling the coin inputs and the tilt switch.
  To simulate this, we read the status in the interrupt handler for the main
  CPU and update the counters appropriately. We also must take care of
  handling the coin/credit settings ourselves.

***************************************************************************/

#include "driver.h"

extern unsigned char *tnzs_workram;

static int mcu_type;

enum
{
	MCU_NONE,
	MCU_EXTRMATN,
	MCU_ARKANOID,
	MCU_DRTOPPEL,
	MCU_CHUKATAI,
	MCU_TNZS
};

static int mcu_initializing,mcu_coinage_init,mcu_command,mcu_readcredits;
static int mcu_reportcoin;
static int tnzs_workram_backup;
static unsigned char mcu_coinage[4];
static unsigned char mcu_coinsA,mcu_coinsB,mcu_credits;



READ_HANDLER( arkanoi2_sh_f000_r )
{
	int val;

	//logerror("PC %04x: read input %04x\n", cpu_get_pc(), 0xf000 + offset);

	val = readinputport(5 + offset/2);
	if (offset & 1)
	{
		return ((val >> 8) & 0xff);
	}
	else
	{
		return val & 0xff;
	}
}


static void mcu_reset(void)
{
	mcu_initializing = 3;
	mcu_coinage_init = 0;
	mcu_coinage[0] = 1;
	mcu_coinage[1] = 1;
	mcu_coinage[2] = 1;
	mcu_coinage[3] = 1;
	mcu_coinsA = 0;
	mcu_coinsB = 0;
	mcu_credits = 0;
	mcu_reportcoin = 0;
	mcu_command = 0;
	tnzs_workram_backup = -1;
}

static void mcu_handle_coins(int coin)
{
	static int insertcoin;

	/* The coin inputs and coin counter is managed by the i8742 mcu. */
	/* Here we simulate it. */
	/* Chuka Taisen has a limit of 9 credits, so any */
	/* coins that could push it over 9 should be rejected */
	/* Coin/Play settings must also be taken into consideration */

	if (coin & 0x08)	/* tilt */
		mcu_reportcoin = coin;
	else if (coin && coin != insertcoin)
	{
		if (coin & 0x01)	/* coin A */
		{
			if ((mcu_type == MCU_CHUKATAI) && ((mcu_credits+mcu_coinage[1]) > 9))
			{
				coin_lockout_global_w(0,1); /* Lock all coin slots */
			}
			else
			{
				//logerror("Coin dropped into slot A\n");
				coin_lockout_global_w(0,0); /* Unlock all coin slots */
				coin_counter_w(0,1); coin_counter_w(0,0); /* Count slot A */
				mcu_coinsA++;
				if (mcu_coinsA >= mcu_coinage[0])
				{
					mcu_coinsA -= mcu_coinage[0];
					mcu_credits += mcu_coinage[1];
				}
			}
		}
		if (coin & 0x02)	/* coin B */
		{
			if ((mcu_type == MCU_CHUKATAI) && ((mcu_credits+mcu_coinage[3]) > 9))
			{
				coin_lockout_global_w(0,1); /* Lock all coin slots */
			}
			else
			{
				//logerror("Coin dropped into slot B\n");
				coin_lockout_global_w(0,0); /* Unlock all coin slots */
				coin_counter_w(1,1); coin_counter_w(1,0); /* Count slot B */
				mcu_coinsB++;
				if (mcu_coinsB >= mcu_coinage[2])
				{
					mcu_coinsB -= mcu_coinage[2];
					mcu_credits += mcu_coinage[3];
				}
			}
		}
		if (coin & 0x04)	/* service */
		{
			//logerror("Coin dropped into service slot C\n");
			mcu_credits++;
		}
		mcu_reportcoin = coin;
	}
	else
	{
		coin_lockout_global_w(0,0); /* Unlock all coin slots */
		mcu_reportcoin = 0;
	}
	insertcoin = coin;
}



static READ_HANDLER( mcu_arkanoi2_r )
{
	char *mcu_startup = "\x55\xaa\x5a";

//	logerror("PC %04x: read mcu %04x\n", cpu_get_pc(), 0xc000 + offset);

	if (offset == 0)
	{
		/* if the mcu has just been reset, return startup code */
		if (mcu_initializing)
		{
			mcu_initializing--;
			return mcu_startup[2 - mcu_initializing];
		}

		switch (mcu_command)
		{
			case 0x41:
				return mcu_credits;

			case 0xc1:
				/* Read the credit counter or the inputs */
				if (mcu_readcredits == 0)
				{
					mcu_readcredits = 1;
					if (mcu_reportcoin & 0x08)
					{
						mcu_initializing = 3;
						return 0xee;	/* tilt */
					}
					else return mcu_credits;
				}
				else return readinputport(2);	/* buttons */

			default:
//logerror("error, unknown mcu command\n");
				/* should not happen */
				return 0xff;
				break;
		}
	}
	else
	{
		/*
		status bits:
		0 = mcu is ready to send data (read from c000)
		1 = mcu has read data (from c000)
		2 = unused
		3 = unused
		4-7 = coin code
		      0 = nothing
		      1,2,3 = coin switch pressed
		      e = tilt
		*/
		if (mcu_reportcoin & 0x08) return 0xe1;	/* tilt */
		if (mcu_reportcoin & 0x01) return 0x11;	/* coin 1 (will trigger "coin inserted" sound) */
		if (mcu_reportcoin & 0x02) return 0x21;	/* coin 2 (will trigger "coin inserted" sound) */
		if (mcu_reportcoin & 0x04) return 0x31;	/* coin 3 (will trigger "coin inserted" sound) */
		return 0x01;
	}
}

static WRITE_HANDLER( mcu_arkanoi2_w )
{
	if (offset == 0)
	{
//	logerror("PC %04x (re %04x): write %02x to mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), data, 0xc000 + offset);
		if (mcu_command == 0x41)
		{
			mcu_credits = (mcu_credits + data) & 0xff;
		}
	}
	else
	{
		/*
		0xc1: read number of credits, then buttons
		0x54+0x41: add value to number of credits
		0x84: coin 1 lockout (issued only in test mode)
		0x88: coin 2 lockout (issued only in test mode)
		0x80: release coin lockout (issued only in test mode)
		during initialization, a sequence of 4 bytes sets coin/credit settings
		*/
//	logerror("PC %04x (re %04x): write %02x to mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), data, 0xc000 + offset);

		if (mcu_initializing)
		{
			/* set up coin/credit settings */
			mcu_coinage[mcu_coinage_init++] = data;
			if (mcu_coinage_init == 4) mcu_coinage_init = 0;	/* must not happen */
		}

		if (data == 0xc1)
			mcu_readcredits = 0;	/* reset input port number */

		mcu_command = data;
	}
}


static READ_HANDLER( mcu_chukatai_r )
{
	char *mcu_startup = "\xa5\x5a\xaa";

	//logerror("PC %04x (re %04x): read mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), 0xc000 + offset);

	if (offset == 0)
	{
		/* if the mcu has just been reset, return startup code */
		if (mcu_initializing)
		{
			mcu_initializing--;
			return mcu_startup[2 - mcu_initializing];
		}

		switch (mcu_command)
		{
			case 0x1f:
				return (readinputport(4) >> 4) ^ 0x0f;

			case 0x03:
				return readinputport(4) & 0x0f;

			case 0x41:
				return mcu_credits;

			case 0x93:
				/* Read the credit counter or the inputs */
				if (mcu_readcredits == 0)
				{
					mcu_readcredits += 1;
					if (mcu_reportcoin & 0x08)
					{
						mcu_initializing = 3;
						return 0xee;	/* tilt */
					}
					else return mcu_credits;
				}
				/* player 1 joystick and buttons */
				if (mcu_readcredits == 1)
				{
					mcu_readcredits += 1;
					return readinputport(2);
				}
				/* player 2 joystick and buttons */
				if (mcu_readcredits == 2)
				{
					return readinputport(3);
				}

			default:
//logerror("error, unknown mcu command (%02x)\n",mcu_command);
				/* should not happen */
				return 0xff;
				break;
		}
	}
	else
	{
		/*
		status bits:
		0 = mcu is ready to send data (read from c000)
		1 = mcu has read data (from c000)
		2 = mcu is busy
		3 = unused
		4-7 = coin code
		      0 = nothing
		      1,2,3 = coin switch pressed
		      e = tilt
		*/
		if (mcu_reportcoin & 0x08) return 0xe1;	/* tilt */
		if (mcu_reportcoin & 0x01) return 0x11;	/* coin A */
		if (mcu_reportcoin & 0x02) return 0x21;	/* coin B */
		if (mcu_reportcoin & 0x04) return 0x31;	/* coin C */
		return 0x01;
	}
}

static WRITE_HANDLER( mcu_chukatai_w )
{
	//logerror("PC %04x (re %04x): write %02x to mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), data, 0xc000 + offset);

	if (offset == 0)
	{
		if (mcu_command == 0x41)
		{
			mcu_credits = (mcu_credits + data) & 0xff;
		}
	}
	else
	{
		/*
		0x93: read number of credits, then joysticks/buttons
		0x03: read service & tilt switches
		0x1f: read coin switches
		0x4f+0x41: add value to number of credits

		during initialization, a sequence of 4 bytes sets coin/credit settings
		*/

		if (mcu_initializing)
		{
			/* set up coin/credit settings */
			mcu_coinage[mcu_coinage_init++] = data;
			if (mcu_coinage_init == 4) mcu_coinage_init = 0;	/* must not happen */
		}

		if (data == 0x93)
			mcu_readcredits = 0;	/* reset input port number */

		mcu_command = data;
	}
}



static READ_HANDLER( mcu_tnzs_r )
{
	char *mcu_startup = "\x5a\xa5\x55";

	//logerror("PC %04x (re %04x): read mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), 0xc000 + offset);

	if (offset == 0)
	{
		/* if the mcu has just been reset, return startup code */
		if (mcu_initializing)
		{
			mcu_initializing--;
			return mcu_startup[2 - mcu_initializing];
		}

		switch (mcu_command)
		{
			case 0x01:
				return readinputport(2) ^ 0xff;	/* player 1 joystick + buttons */

			case 0x02:
				return readinputport(3) ^ 0xff;	/* player 2 joystick + buttons */

			case 0x1a:
				return readinputport(4) >> 4;

			case 0x21:
				return readinputport(4) & 0x0f;

			case 0x41:
				return mcu_credits;

			case 0xa0:
				/* Read the credit counter */
				if (mcu_reportcoin & 0x08)
				{
					mcu_initializing = 3;
					return 0xee;	/* tilt */
				}
				else return mcu_credits;

			case 0xa1:
				/* Read the credit counter or the inputs */
				if (mcu_readcredits == 0)
				{
					mcu_readcredits = 1;
					if (mcu_reportcoin & 0x08)
					{
						mcu_initializing = 3;
						return 0xee;	/* tilt */
//						return 0x64;	/* theres a reset input somewhere */
					}
					else return mcu_credits;
				}
				/* buttons */
				else return ((readinputport(2) & 0xf0) | (readinputport(3) >> 4)) ^ 0xff;

			default:
//logerror("error, unknown mcu command\n");
				/* should not happen */
				return 0xff;
				break;
		}
	}
	else
	{
		/*
		status bits:
		0 = mcu is ready to send data (read from c000)
		1 = mcu has read data (from c000)
		2 = unused
		3 = unused
		4-7 = coin code
		      0 = nothing
		      1,2,3 = coin switch pressed
		      e = tilt
		*/
		if (mcu_reportcoin & 0x08) return 0xe1;	/* tilt */
		if (mcu_type == MCU_TNZS)
		{
			if (mcu_reportcoin & 0x01) return 0x31;	/* coin 1 (will trigger "coin inserted" sound) */
			if (mcu_reportcoin & 0x02) return 0x21;	/* coin 2 (will trigger "coin inserted" sound) */
			if (mcu_reportcoin & 0x04) return 0x11;	/* coin 3 (will NOT trigger "coin inserted" sound) */
		}
		else
		{
			if (mcu_reportcoin & 0x01) return 0x11;	/* coin 1 (will trigger "coin inserted" sound) */
			if (mcu_reportcoin & 0x02) return 0x21;	/* coin 2 (will trigger "coin inserted" sound) */
			if (mcu_reportcoin & 0x04) return 0x31;	/* coin 3 (will trigger "coin inserted" sound) */
		}
		return 0x01;
	}
}

static WRITE_HANDLER( mcu_tnzs_w )
{
	if (offset == 0)
	{
		//logerror("PC %04x (re %04x): write %02x to mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), data, 0xc000 + offset);
		if (mcu_command == 0x41)
		{
			mcu_credits = (mcu_credits + data) & 0xff;
		}
	}
	else
	{
		/*
		0xa0: read number of credits
		0xa1: read number of credits, then buttons
		0x01: read player 1 joystick + buttons
		0x02: read player 2 joystick + buttons
		0x1a: read coin switches
		0x21: read service & tilt switches
		0x4a+0x41: add value to number of credits
		0x84: coin 1 lockout (issued only in test mode)
		0x88: coin 2 lockout (issued only in test mode)
		0x80: release coin lockout (issued only in test mode)
		during initialization, a sequence of 4 bytes sets coin/credit settings
		*/

		//logerror("PC %04x (re %04x): write %02x to mcu %04x\n", cpu_get_pc(), cpu_geturnpc(), data, 0xc000 + offset);

		if (mcu_initializing)
		{
			/* set up coin/credit settings */
			mcu_coinage[mcu_coinage_init++] = data;
			if (mcu_coinage_init == 4) mcu_coinage_init = 0;	/* must not happen */
		}

		if (data == 0xa1)
			mcu_readcredits = 0;	/* reset input port number */

		/* Dr Toppel decrements credits differently. So handle it */
		if ((data == 0x09) && (mcu_type == MCU_DRTOPPEL))
			mcu_credits = (mcu_credits - 1) & 0xff;		/* Player 1 start */
		if ((data == 0x18) && (mcu_type == MCU_DRTOPPEL))
			mcu_credits = (mcu_credits - 2) & 0xff;		/* Player 2 start */

		mcu_command = data;
	}
}



void init_extrmatn(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	mcu_type = MCU_EXTRMATN;

	/* there's code which falls through from the fixed ROM to bank #7, I have to */
	/* copy it there otherwise the CPU bank switching support will not catch it. */
	memcpy(&RAM[0x08000],&RAM[0x2c000],0x4000);
}

void init_arkanoi2(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	mcu_type = MCU_ARKANOID;

	/* there's code which falls through from the fixed ROM to bank #2, I have to */
	/* copy it there otherwise the CPU bank switching support will not catch it. */
	memcpy(&RAM[0x08000],&RAM[0x18000],0x4000);
}

void init_drtoppel(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	mcu_type = MCU_DRTOPPEL;

	/* there's code which falls through from the fixed ROM to bank #0, I have to */
	/* copy it there otherwise the CPU bank switching support will not catch it. */
	memcpy(&RAM[0x08000],&RAM[0x18000],0x4000);
}

void init_chukatai(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	mcu_type = MCU_CHUKATAI;

	/* there's code which falls through from the fixed ROM to bank #0, I have to */
	/* copy it there otherwise the CPU bank switching support will not catch it. */
	memcpy(&RAM[0x08000],&RAM[0x18000],0x4000);
}

void init_tnzs(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	mcu_type = MCU_TNZS;

	/* there's code which falls through from the fixed ROM to bank #0, I have to */
	/* copy it there otherwise the CPU bank switching support will not catch it. */
	memcpy(&RAM[0x08000],&RAM[0x18000],0x4000);
}

void init_insectx(void)
{
	mcu_type = MCU_NONE;

	/* this game has no mcu, replace the handler with plain input port handlers */
	install_mem_read_handler(1, 0xc000, 0xc000, input_port_2_r );
	install_mem_read_handler(1, 0xc001, 0xc001, input_port_3_r );
	install_mem_read_handler(1, 0xc002, 0xc002, input_port_4_r );
}

void init_kageki(void)
{
	/* this game has no mcu */
	mcu_type = MCU_NONE;
}


READ_HANDLER( tnzs_mcu_r )
{
	switch (mcu_type)
	{
		case MCU_ARKANOID:
			return mcu_arkanoi2_r(offset);
			break;
		case MCU_CHUKATAI:
			return mcu_chukatai_r(offset);
			break;
		case MCU_EXTRMATN:
		case MCU_DRTOPPEL:
		case MCU_TNZS:
		default:
			return mcu_tnzs_r(offset);
			break;
	}
}

WRITE_HANDLER( tnzs_mcu_w )
{
	switch (mcu_type)
	{
		case MCU_ARKANOID:
			mcu_arkanoi2_w(offset,data);
			break;
		case MCU_CHUKATAI:
			mcu_chukatai_w(offset,data);
			break;
		case MCU_EXTRMATN:
		case MCU_DRTOPPEL:
		case MCU_TNZS:
		default:
			mcu_tnzs_w(offset,data);
			break;
	}
}

int tnzs_interrupt(void)
{
	int coin;

	switch (mcu_type)
	{
		case MCU_ARKANOID:
			coin = ((readinputport(5) & 0xf000) ^ 0xd000) >> 12;
			coin = (coin & 0x08) | ((coin & 0x03) << 1) | ((coin & 0x04) >> 2);
			mcu_handle_coins(coin);
			break;

		case MCU_EXTRMATN:
		case MCU_DRTOPPEL:
			coin = (((readinputport(4) & 0x30) >> 4) | ((readinputport(4) & 0x03) << 2)) ^ 0x0c;
			mcu_handle_coins(coin);
			break;

		case MCU_CHUKATAI:
		case MCU_TNZS:
			coin = (((readinputport(4) & 0x30) >> 4) | ((readinputport(4) & 0x03) << 2)) ^ 0x0f;
			mcu_handle_coins(coin);
			break;

		case MCU_NONE:
		default:
			break;
	}

	return 0;
}

void tnzs_init_machine (void)
{
	/* initialize the mcu simulation */
	mcu_reset();

	/* preset the banks */
	{
		unsigned char *RAM;

		RAM = memory_region(REGION_CPU1);
		cpu_setbank(1,&RAM[0x18000]);

		RAM = memory_region(REGION_CPU2);
		cpu_setbank(2,&RAM[0x10000]);
	}
}


READ_HANDLER( tnzs_workram_r )
{
	/* Location $EF10 workaround required to stop TNZS getting */
	/* caught in and endless loop due to shared ram sync probs */

	if ((offset == 0xf10) && (mcu_type == MCU_TNZS))
	{
		int tnzs_cpu0_pc;

		tnzs_cpu0_pc = cpu_get_pc();
		switch (tnzs_cpu0_pc)
		{
			case 0xc66:		/* tnzs */
			case 0xc64:		/* tnzsb */
			case 0xab8:		/* tnzs2 */
				tnzs_workram[offset] = (tnzs_workram_backup & 0xff);
				return tnzs_workram_backup;
				break;
			default:
				break;
		}
	}
	return tnzs_workram[offset];
}

READ_HANDLER( tnzs_workram_sub_r )
{
	return tnzs_workram[offset];
}

WRITE_HANDLER( tnzs_workram_w )
{
	/* Location $EF10 workaround required to stop TNZS getting */
	/* caught in and endless loop due to shared ram sync probs */

	tnzs_workram_backup = -1;

	if ((offset == 0xf10) && (mcu_type == MCU_TNZS))
	{
		int tnzs_cpu0_pc;

		tnzs_cpu0_pc = cpu_get_pc();
		switch (tnzs_cpu0_pc)
		{
			case 0xab5:		/* tnzs2 */
				if (cpu_getpreviouspc() == 0xab4)
					break;  /* unfortunantly tnzsb is true here too, so stop it */
			case 0xc63:		/* tnzs */
			case 0xc61:		/* tnzsb */
				tnzs_workram_backup = data;
				break;
			default:
				break;
		}
	}
	if (tnzs_workram_backup == -1)
		tnzs_workram[offset] = data;
}

WRITE_HANDLER( tnzs_workram_sub_w )
{
	tnzs_workram[offset] = data;
}

WRITE_HANDLER( tnzs_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* bit 4 resets the second CPU */
	if (data & 0x10)
		cpu_set_reset_line(1,CLEAR_LINE);
	else
		cpu_set_reset_line(1,ASSERT_LINE);

	/* bits 0-2 select RAM/ROM bank */
//	logerror("PC %04x: writing %02x to bankswitch\n", cpu_get_pc(),data);
	cpu_setbank (1, &RAM[0x10000 + 0x4000 * (data & 0x07)]);
}

WRITE_HANDLER( tnzs_bankswitch1_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);

//	logerror("PC %04x: writing %02x to bankswitch 1\n", cpu_get_pc(),data);

	/* bit 2 resets the mcu */
	if (data & 0x04) mcu_reset();

	/* bits 0-1 select ROM bank */
	cpu_setbank (2, &RAM[0x10000 + 0x2000 * (data & 3)]);
}
