#include "../machine/starwars.cpp"
#include "../machine/swmathbx.cpp"
#include "../sndhrdw/starwars.cpp"

/***************************************************************************
File: drivers\starwars.c

STARWARS HARDWARE FILE

This file is Copyright 1997, Steve Baines.
Modified by Frank Palazzolo for sound support

Current e-mail contact address:  sjb@ohm.york.ac.uk

Release 2.0 (6 August 1997)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"
#include "machine/swmathbx.h"
#include "machine/atari_vg.h"

/* formerly sndhrdw/starwars.h */
READ_HANDLER( starwars_main_read_r );
READ_HANDLER( starwars_main_ready_flag_r );
WRITE_HANDLER( starwars_main_wr_w );
WRITE_HANDLER( starwars_soundrst_w );

READ_HANDLER( starwars_sin_r );
READ_HANDLER( starwars_m6532_r );

WRITE_HANDLER( starwars_sout_w );
WRITE_HANDLER( starwars_m6532_w );

/* formerly machine/starwars.h */
READ_HANDLER( starwars_input_bank_1_r );
void starwars_wdclr(int, int);

READ_HANDLER( starwars_control_r );
WRITE_HANDLER( starwars_control_w );
int  starwars_interrupt (void);


static unsigned char *nvram;
static size_t nvram_size;

static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0,nvram_size);
	}
}



WRITE_HANDLER( starwars_out_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	switch (offset)
	{
		case 0:		/* Coin counter 1 */
			coin_counter_w (0, data);
			break;
		case 1:		/* Coin counter 2 */
			coin_counter_w (1, data);
			break;
		case 2:		/* LED 3 */
			osd_led_w (2, data >> 7);
			break;
		case 3:		/* LED 2 */
			osd_led_w (1, data >> 7);
			break;
		case 4:
//			logerror("bank_switch_w, %02x\n", data);
			if (data & 0x80)
			{
				cpu_setbank(1,&RAM[0x10000])
				cpu_setbank(2,&RAM[0x1c000])
			}
			else
			{
				cpu_setbank(1,&RAM[0x06000]);
				cpu_setbank(2,&RAM[0x0a000]);
			}
			break;
		case 5:
			prngclr_w (offset, data);
			break;
		case 6:
			osd_led_w (0, data >> 7);
			break;	/* LED 1 */
		case 7:
			/*logerror("recall\n");*/ /* what's that? */
			break;
	}
}

/* machine/slapstic.c */
void slapstic_init (int chip);
int slapstic_bank (void);
int slapstic_tweak (offs_t offset);

static unsigned char *slapstic_base;	/* ASG - made static */
static unsigned char *slapstic_area;

/* ASG - added this (including the function) */
static int last_bank;
static int esb_slapstic_tweak(offs_t offset)
{
	int bank = slapstic_tweak(offset);
	if (bank != last_bank)
	{
		memcpy(slapstic_area, &slapstic_base[bank * 0x2000], 0x2000);
		last_bank = bank;
	}
	return bank;
}

static OPBASE_HANDLER( esb_setopbase )
{
	int prevpc = cpu_getpreviouspc ();
	int bank;

	/*
	 *		This is a slightly ugly kludge for Indiana Jones & the Temple of Doom because it jumps
	 *		directly to code in the slapstic.  The general order of things is this:
	 *
	 *			jump to $3A, which turns off interrupts and jumps to $00 (the reset address)
	 *			look up the request in a table and jump there
	 *			(under some circumstances, tweak the special addresses)
	 *			return via an RTS at the real bankswitch address
	 *
	 *		To simulate this, we tweak the slapstic reset address on entry into slapstic code; then
	 *		we let the system tweak whatever other addresses it wishes.  On exit, we tweak the
	 *		address of the previous PC, which is the RTS instruction, thereby completing the
	 *		bankswitch sequence.
	 *
	 *		Fortunately for us, all 4 banks have exactly the same code at this point in their
	 *		ROM, so it doesn't matter which version we're actually executing.
	 */

	if ((address & 0xe000) == 0x8000)
	{
//		logerror("      new pc inside of slapstic region: %04x (prev = %04x)\n", pc, prevpc);
		bank = esb_slapstic_tweak ((address) & 0x1fff);	/* ASG - switched to ESB version */
		/* catching every branch during slapstic area */
		catch_nextBranch();
		return -1;
	}
	else if ((prevpc & 0xe000) == 0x8000)
 {
//  logerror("      old pc inside of slapstic region: %04x (new = %04x)\n", prevpc, pc);
if (prevpc != 0x8080 && prevpc != 0x8090 && prevpc != 0x80a0 && prevpc !=0x80b0)
  bank = esb_slapstic_tweak ((prevpc) & 0x1fff);	/* ASG - switched to ESB version */
 }

	return address;
}

void esb_init_machine (void)
{
	/* Set up the slapstic */
	slapstic_init (101);
	cpu_setOPbaseoverride (0,esb_setopbase);
	/* ASG - added the following: */
	memcpy(slapstic_area, &slapstic_base[slapstic_bank() * 0x2000], 0x2000);

	/* Reset all the banks */
	starwars_out_w (4, 0);

	init_swmathbox ();
}

/*************************************
 *
 *		Slapstic ROM read/write.
 *
 *************************************/

READ_HANDLER( esb_slapstic_r )
{
	int val;

	int bank = (esb_slapstic_tweak (offset) * 0x2000);	/* ASG - switched to ESB version */
	val = slapstic_base[bank + (offset & 0x1fff)];
//	logerror("slapstic_r, %04x: %02x\n", 0x8000 + offset, val);
	return val;
}


WRITE_HANDLER( esb_slapstic_w )
{
//	logerror("esb slapstic tweak via write\n");
	esb_slapstic_tweak (offset);	/* ASG - switched to ESB version */
}

/* Star Wars READ memory map */
static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x2fff, MRA_RAM },   /* vector_ram */
	{ 0x3000, 0x3fff, MRA_ROM },		/* vector_rom */
	{ 0x4300, 0x431f, input_port_0_r }, /* Memory mapped input port 0 */
	{ 0x4320, 0x433f, starwars_input_bank_1_r }, /* Memory mapped input port 1 */
	{ 0x4340, 0x435f, input_port_2_r },	/* DIP switches bank 0 */
	{ 0x4360, 0x437f, input_port_3_r },	/* DIP switches bank 1 */
	{ 0x4380, 0x439f, starwars_control_r }, /* a-d control result */
	{ 0x4400, 0x4400, starwars_main_read_r },
	{ 0x4401, 0x4401, starwars_main_ready_flag_r },
	{ 0x4500, 0x45ff, MRA_RAM },		/* nov_ram */
	{ 0x4700, 0x4700, reh_r },
	{ 0x4701, 0x4701, rel_r },
	{ 0x4703, 0x4703, prng_r },			/* pseudo random number generator */
/*	{ 0x4800, 0x4fff, MRA_RAM }, */		/* cpu_ram */
/*	{ 0x5000, 0x5fff, MRA_RAM }, */		/* (math_ram_r) math_ram */
	{ 0x4800, 0x5fff, MRA_RAM },		/* CPU and Math RAM */
	{ 0x6000, 0x7fff, MRA_BANK1 },	    /* banked ROM */
	{ 0x8000, 0xffff, MRA_ROM },		/* rest of main_rom */
	{ -1 }	/* end of table */
};

/* Star Wars Sound READ memory map */
static struct MemoryReadAddress readmem2[] =
{
	{ 0x0800, 0x0fff, starwars_sin_r },		/* SIN Read */
	{ 0x1000, 0x107f, MRA_RAM },	/* 6532 RAM */
	{ 0x1080, 0x109f, starwars_m6532_r },
	{ 0x2000, 0x27ff, MRA_RAM },	/* program RAM */
	{ 0x4000, 0xbfff, MRA_ROM },	/* sound roms */
	{ 0xc000, 0xffff, MRA_ROM },	/* load last rom twice */
									/* for proper int vec operation */
	{ -1 }  /* end of table */
};

/* Star Wars WRITE memory map */
static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x2fff, MWA_RAM, &vectorram, &vectorram_size }, /* vector_ram */
	{ 0x3000, 0x3fff, MWA_ROM },		/* vector_rom */
	{ 0x4400, 0x4400, starwars_main_wr_w },
	{ 0x4500, 0x45ff, MWA_RAM, &nvram, &nvram_size },		/* nov_ram */
	{ 0x4600, 0x461f, avgdvg_go_w },
	{ 0x4620, 0x463f, avgdvg_reset_w },
	{ 0x4640, 0x465f, MWA_NOP },		/* (wdclr) Watchdog clear */
	{ 0x4660, 0x467f, MWA_NOP },        /* irqclr: clear periodic interrupt */
	{ 0x4680, 0x4687, starwars_out_w },
	{ 0x46a0, 0x46bf, MWA_NOP },		/* nstore */
	{ 0x46c0, 0x46c2, starwars_control_w },	/* Selects which a-d control port (0-3) will be read */
	{ 0x46e0, 0x46e0, starwars_soundrst_w },
	{ 0x4700, 0x4707, swmathbx_w },
/*	{ 0x4800, 0x4fff, MWA_RAM }, */		/* cpu_ram */
/*	{ 0x5000, 0x5fff, MWA_RAM }, */		/* (math_ram_w) math_ram */
	{ 0x4800, 0x5fff, MWA_RAM },		/* CPU and Math RAM */
	{ 0x6000, 0xffff, MWA_ROM },		/* main_rom */
	{ -1 }	/* end of table */
};

/* Star Wars sound WRITE memory map */
static struct MemoryWriteAddress writemem2[] =
{
	{ 0x0000, 0x07ff, starwars_sout_w },
	{ 0x1000, 0x107f, MWA_RAM }, /* 6532 ram */
	{ 0x1080, 0x109f, starwars_m6532_w },
	{ 0x1800, 0x183f, quad_pokey_w },
	{ 0x2000, 0x27ff, MWA_RAM }, /* program RAM */
	{ 0x4000, 0xbfff, MWA_ROM }, /* sound rom */
	{ 0xc000, 0xffff, MWA_ROM }, /* sound rom again, for intvecs */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress esb_readmem[] =
{
	{ 0x0000, 0x2fff, MRA_RAM },   /* vector_ram */
	{ 0x3000, 0x3fff, MRA_ROM },		/* vector_rom */
	{ 0x4300, 0x431f, input_port_0_r }, /* Memory mapped input port 0 */
	{ 0x4320, 0x433f, starwars_input_bank_1_r }, /* Memory mapped input port 1 */
	{ 0x4340, 0x435f, input_port_2_r },	/* DIP switches bank 0 */
	{ 0x4360, 0x437f, input_port_3_r },	/* DIP switches bank 1 */
	{ 0x4380, 0x439f, starwars_control_r }, /* a-d control result */
	{ 0x4400, 0x4400, starwars_main_read_r },
	{ 0x4401, 0x4401, starwars_main_ready_flag_r },
	{ 0x4500, 0x45ff, MRA_RAM },		/* nov_ram */
	{ 0x4700, 0x4700, reh_r },
	{ 0x4701, 0x4701, rel_r },
	{ 0x4703, 0x4703, prng_r },			/* pseudo random number generator */
/*	{ 0x4800, 0x4fff, MRA_RAM }, */		/* cpu_ram */
/*	{ 0x5000, 0x5fff, MRA_RAM }, */		/* (math_ram_r) math_ram */
	{ 0x4800, 0x5fff, MRA_RAM },		/* CPU and Math RAM */
	{ 0x6000, 0x7fff, MRA_BANK1 },	    /* banked ROM */
	{ 0x8000, 0x9fff, esb_slapstic_r },
	{ 0xa000, 0xffff, MRA_BANK2 },		/* banked ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress esb_writemem[] =
{
	{ 0x0000, 0x2fff, MWA_RAM, &vectorram, &vectorram_size }, /* vector_ram */
	{ 0x3000, 0x3fff, MWA_ROM },		/* vector_rom */
	{ 0x4400, 0x4400, starwars_main_wr_w },
	{ 0x4500, 0x45ff, MWA_RAM },		/* nov_ram */
	{ 0x4600, 0x461f, avgdvg_go_w },
	{ 0x4620, 0x463f, avgdvg_reset_w },
	{ 0x4640, 0x465f, MWA_NOP },		/* (wdclr) Watchdog clear */
	{ 0x4660, 0x467f, MWA_NOP },        /* irqclr: clear periodic interrupt */
	{ 0x4680, 0x4687, starwars_out_w },
	{ 0x46a0, 0x46bf, MWA_NOP },		/* nstore */
	{ 0x46c0, 0x46c2, starwars_control_w },	/* Selects which a-d control port (0-3) will be read */
	{ 0x46e0, 0x46e0, starwars_soundrst_w },
	{ 0x4700, 0x4707, swmathbx_w },
/*	{ 0x4800, 0x4fff, MWA_RAM }, */		/* cpu_ram */
/*	{ 0x5000, 0x5fff, MWA_RAM }, */		/* (math_ram_w) math_ram */
	{ 0x4800, 0x5fff, MWA_RAM },		/* CPU and Math RAM */
	{ 0x8000, 0x9fff, esb_slapstic_w, &slapstic_area },		/* slapstic write */
	{ 0x6000, 0xffff, MWA_ROM },		/* main_rom */

	/* Dummy entry to set up the slapstic */
	{ 0x14000, 0x1bfff, MWA_NOP, &slapstic_base },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( starwars )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	/* Bit 6 is MATH_RUN - see machine/starwars.c */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	/* Bit 7 is VG_HALT - see machine/starwars.c */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME(0x03, 0x00, "Starting Shields" )
	PORT_DIPSETTING (  0x00, "6" )
	PORT_DIPSETTING (  0x01, "7" )
	PORT_DIPSETTING (  0x02, "8" )
	PORT_DIPSETTING (  0x03, "9" )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING (  0x00, "Easy" )
	PORT_DIPSETTING (  0x04, "Moderate" )
	PORT_DIPSETTING (  0x08, "Hard" )
	PORT_DIPSETTING (  0x0c, "Hardest" )
	PORT_DIPNAME(0x30, 0x00, "Bonus Shields" )
	PORT_DIPSETTING (  0x00, "0" )
	PORT_DIPSETTING (  0x10, "1" )
	PORT_DIPSETTING (  0x20, "2" )
	PORT_DIPSETTING (  0x30, "3" )
	PORT_DIPNAME(0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING (  0x40, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x00, DEF_STR( On ) )
	PORT_DIPNAME(0x80, 0x80, "Freeze" )
	PORT_DIPSETTING (  0x80, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (  0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (  0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (  0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x04, "*4" )
	PORT_DIPSETTING (  0x08, "*5" )
	PORT_DIPSETTING (  0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x10, "*2" )
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coinage" )
	PORT_DIPSETTING (  0x20, "2 gives 1" )
	PORT_DIPSETTING (  0x60, "4 gives 2" )
	PORT_DIPSETTING (  0xa0, "3 gives 1" )
	PORT_DIPSETTING (  0x40, "4 gives 1" )
	PORT_DIPSETTING (  0x80, "5 gives 1" )
	PORT_DIPSETTING (  0x00, "None" )
/* 0xc0 and 0xe0 None */

	PORT_START	/* IN4 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y, 70, 30, 0, 255 )

	PORT_START	/* IN5 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 50, 30, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( esb )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	/* Bit 6 is MATH_RUN - see machine/starwars.c */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	/* Bit 7 is VG_HALT - see machine/starwars.c */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME(0x03, 0x03, "Starting Shields" )
	PORT_DIPSETTING (  0x01, "2" )
	PORT_DIPSETTING (  0x00, "3" )
	PORT_DIPSETTING (  0x03, "4" )
	PORT_DIPSETTING (  0x02, "5" )
	PORT_DIPNAME(0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING (  0x08, "Easy" )
	PORT_DIPSETTING (  0x0c, "Moderate" )
	PORT_DIPSETTING (  0x00, "Hard" )
	PORT_DIPSETTING (  0x04, "Hardest" )
	PORT_DIPNAME(0x30, 0x30, "Jedi-Letter Mode" )
	PORT_DIPSETTING (  0x00, "Level Only" )
	PORT_DIPSETTING (  0x10, "Level" )
	PORT_DIPSETTING (  0x20, "Increment Only" )
	PORT_DIPSETTING (  0x30, "Increment" )
	PORT_DIPNAME(0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING (  0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x40, DEF_STR( On ) )
	PORT_DIPNAME(0x80, 0x80, "Freeze" )
	PORT_DIPSETTING (  0x80, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (  0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (  0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (  0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x04, "*4" )
	PORT_DIPSETTING (  0x08, "*5" )
	PORT_DIPSETTING (  0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x10, "*2" )
	PORT_DIPNAME(0xe0, 0xe0, "Bonus Coinage" )
	PORT_DIPSETTING (  0x20, "2 gives 1" )
	PORT_DIPSETTING (  0x60, "4 gives 2" )
	PORT_DIPSETTING (  0xa0, "3 gives 1" )
	PORT_DIPSETTING (  0x40, "4 gives 1" )
	PORT_DIPSETTING (  0x80, "5 gives 1" )
	PORT_DIPSETTING (  0xe0, "None" )
/* 0xc0 and 0x00 None */

	PORT_START	/* IN4 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y, 70, 30, 0, 255 )

	PORT_START	/* IN5 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 50, 30, 0, 255 )
INPUT_PORTS_END



static struct POKEYinterface pokey_interface =
{
	4,			/* 4 chips */
	1500000,	/* 1.5 MHz? */
	{ 20, 20, 20, 20 },	/* volume */
	/* The 8 pot handlers */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	/* The allpot handler */
	{ 0, 0, 0, 0 },
};

static struct TMS5220interface tms5220_interface =
{
	640000,     /* clock speed (80*samplerate) */
	50,         /* volume */
	0           /* IRQ handler */
};



static struct MachineDriver machine_driver_starwars =
{
	/* basic machine hardware */
	{
		/* Main CPU */
		{
			CPU_M6809,
			1500000,					/* 1.5 Mhz CPU clock (Don't know what speed it should be) */
			readmem,writemem,0,0,
			interrupt,6 /* 183Hz ? */
			/* Increasing number of interrupts per frame speeds game up */
		},
		/* Sound CPU */
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			1500000,					/* 1.5 Mhz CPU clock (Don't know what speed it should be) */
			readmem2,writemem2,0,0,
			0, 0,
			0, 0	/* no regular interrupts, see sndhrdw/starwars.c */
		}

	},
	30, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,		/* 1 CPU slice per frame. */
	init_swmathbox,  /* Name of initialisation handler */

	/* video hardware */
	400, 300, { 0, 250, 0, 280 },
	0,
	256,0, /* Number of colours, length of colour lookup table */
	avg_init_palette_swars,

	VIDEO_TYPE_VECTOR,
	0,							/* Handler to initialise video handware */
	avg_start_starwars,			/* Start video hardware */
	avg_stop,					/* Stop video hardware */
	vector_vh_screenrefresh,	/* Do a screen refresh */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_esb =
{
	/* basic machine hardware */
	{
		/* Main CPU */
		{
			CPU_M6809,
			1500000,					/* 1.5 Mhz CPU clock (Don't know what speed it should be) */
			esb_readmem, esb_writemem,0,0,
			interrupt,6 /* 183Hz ? */
			/* Increasing number of interrupts per frame speeds game up */
		},
		/* Sound CPU */
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			1500000,					/* 1.5 Mhz CPU clock (Don't know what speed it should be) */
			readmem2,writemem2,0,0,
			0, 0,
			0, 0	/* no regular interrupts, see sndhrdw/starwars.c */
		}

	},
	30, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,		/* 1 CPU slice per frame. */
	esb_init_machine,  /* Name of initialization handler */

	/* video hardware */
	400, 300, { 0, 250, 0, 280 },
	0,
	256,0, /* Number of colours, length of colour lookup table */
	avg_init_palette_swars,

	VIDEO_TYPE_VECTOR,
	0,							/* Handler to initialise video handware */
	avg_start_starwars,			/* Start video hardware */
	avg_stop,					/* Stop video hardware */
	vector_vh_screenrefresh,	/* Do a screen refresh */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		}
	}
};



/***************************************************************************

  Game driver

***************************************************************************/

ROM_START( starwar1 )
	ROM_REGION( 0x12000, REGION_CPU1 )     /* 2 64k ROM spaces */
	ROM_LOAD( "136021.105",   0x3000, 0x1000, 0x538e7d2f ) /* 3000-3fff is 4k vector rom */
	ROM_LOAD( "136021.114",   0x6000, 0x2000, 0xe75ff867 )   /* ROM 0 bank pages 0 and 1 */
	ROM_CONTINUE(            0x10000, 0x2000 )
	ROM_LOAD( "136021.102",   0x8000, 0x2000, 0xf725e344 ) /*  8k ROM 1 bank */
	ROM_LOAD( "136021.203",   0xa000, 0x2000, 0xf6da0a00 ) /*  8k ROM 2 bank */
	ROM_LOAD( "136021.104",   0xc000, 0x2000, 0x7e406703 ) /*  8k ROM 3 bank */
	ROM_LOAD( "136021.206",   0xe000, 0x2000, 0xc7e51237 ) /*  8k ROM 4 bank */

	/* Load the Mathbox PROM's temporarily into the Vector RAM area */
	/* During initialisation they will be converted into useable form */
	/* and stored elsewhere. */
	ROM_LOAD( "136021.110",   0x0000, 0x0400, 0x01061762 ) /* PROM 0 */
	ROM_LOAD( "136021.111",   0x0400, 0x0400, 0x2e619b70 ) /* PROM 1 */
	ROM_LOAD( "136021.112",   0x0800, 0x0400, 0x6cfa3544 ) /* PROM 2 */
	ROM_LOAD( "136021.113",   0x0c00, 0x0400, 0x03f6acb2 ) /* PROM 3 */

	/* Sound ROMS */
	ROM_REGION( 0x10000, REGION_CPU2 )     /* Really only 32k, but it looks like 64K */
	ROM_LOAD( "136021.107",   0x4000, 0x2000, 0xdbf3aea2 ) /* Sound ROM 0 */
	ROM_RELOAD(               0xc000, 0x2000 ) /* Copied again for */
	ROM_LOAD( "136021.208",   0x6000, 0x2000, 0xe38070a8 ) /* Sound ROM 0 */
	ROM_RELOAD(               0xe000, 0x2000 ) /* proper int vecs */
ROM_END

ROM_START( starwars )
	ROM_REGION( 0x12000, REGION_CPU1 )     /* 2 64k ROM spaces */
	ROM_LOAD( "136021.105",   0x3000, 0x1000, 0x538e7d2f ) /* 3000-3fff is 4k vector rom */
	ROM_LOAD( "136021.214",   0x6000, 0x2000, 0x04f1876e )   /* ROM 0 bank pages 0 and 1 */
	ROM_CONTINUE(            0x10000, 0x2000 )
	ROM_LOAD( "136021.102",   0x8000, 0x2000, 0xf725e344 ) /*  8k ROM 1 bank */
	ROM_LOAD( "136021.203",   0xa000, 0x2000, 0xf6da0a00 ) /*  8k ROM 2 bank */
	ROM_LOAD( "136021.104",   0xc000, 0x2000, 0x7e406703 ) /*  8k ROM 3 bank */
	ROM_LOAD( "136021.206",   0xe000, 0x2000, 0xc7e51237 ) /*  8k ROM 4 bank */

	/* Load the Mathbox PROM's temporarily into the Vector RAM area */
	/* During initialisation they will be converted into useable form */
	/* and stored elsewhere. */
	ROM_LOAD( "136021.110",   0x0000, 0x0400, 0x01061762 ) /* PROM 0 */
	ROM_LOAD( "136021.111",   0x0400, 0x0400, 0x2e619b70 ) /* PROM 1 */
	ROM_LOAD( "136021.112",   0x0800, 0x0400, 0x6cfa3544 ) /* PROM 2 */
	ROM_LOAD( "136021.113",   0x0c00, 0x0400, 0x03f6acb2 ) /* PROM 3 */

	/* Sound ROMS */
	ROM_REGION( 0x10000, REGION_CPU2 )     /* Really only 32k, but it looks like 64K */
	ROM_LOAD( "136021.107",   0x4000, 0x2000, 0xdbf3aea2 ) /* Sound ROM 0 */
	ROM_RELOAD(               0xc000, 0x2000 ) /* Copied again for */
	ROM_LOAD( "136021.208",   0x6000, 0x2000, 0xe38070a8 ) /* Sound ROM 0 */
	ROM_RELOAD(               0xe000, 0x2000 ) /* proper int vecs */
ROM_END

ROM_START( esb )
	ROM_REGION( 0x22000, REGION_CPU1 )     /* 64k for code and a buttload for the banked ROMs */
	ROM_LOAD( "136031.111",   0x03000, 0x1000, 0xb1f9bd12 )    /* 3000-3fff is 4k vector rom */
	ROM_LOAD( "136031.101",   0x06000, 0x2000, 0xef1e3ae5 )
	ROM_CONTINUE(             0x10000, 0x2000 )
	/* $8000 - $9fff : slapstic page */
	ROM_LOAD( "136031.102",   0x0a000, 0x2000, 0x62ce5c12 )
	ROM_CONTINUE(             0x1c000, 0x2000 )
	ROM_LOAD( "136031.203",   0x0c000, 0x2000, 0x27b0889b )
	ROM_CONTINUE(             0x1e000, 0x2000 )
	ROM_LOAD( "136031.104",   0x0e000, 0x2000, 0xfd5c725e )
	ROM_CONTINUE(             0x20000, 0x2000 )

	ROM_LOAD( "136031.105",   0x14000, 0x4000, 0xea9e4dce ) /* slapstic 0, 1 */
	ROM_LOAD( "136031.106",   0x18000, 0x4000, 0x76d07f59 ) /* slapstic 2, 3 */

	/* Load the Mathbox PROM's temporarily into the Vector RAM area */
	/* During initialisation they will be converted into useable form */
	/* and stored elsewhere. These are 4-bit PROMs; the high nibble is ignored. */
	ROM_LOAD( "136031.110",   0x0000, 0x0400, 0xb8d0f69d ) /* PROM 0 */
	ROM_LOAD( "136031.109",   0x0400, 0x0400, 0x6a2a4d98 ) /* PROM 1 */
	ROM_LOAD( "136031.108",   0x0800, 0x0400, 0x6a76138f ) /* PROM 2 */
	ROM_LOAD( "136031.107",   0x0c00, 0x0400, 0xafbf6e01 ) /* PROM 3 */

	/* Sound ROMS */
	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "136031.113",   0x4000, 0x2000, 0x24ae3815 ) /* Sound ROM 0 */
	ROM_CONTINUE(             0xc000, 0x2000 ) /* Copied again for */
	ROM_LOAD( "136031.112",   0x6000, 0x2000, 0xca72d341 ) /* Sound ROM 1 */
	ROM_CONTINUE(             0xe000, 0x2000 ) /* proper int vecs */
ROM_END



GAME( 1983, starwars, 0,        starwars, starwars, starwars, ROT0, "Atari", "Star Wars (rev 2)" )
GAME( 1983, starwar1, starwars, starwars, starwars, starwars, ROT0, "Atari", "Star Wars (rev 1)" )
GAME( 1985, esb,      0,        esb,      esb,      starwars, ROT0, "Atari Games", "The Empire Strikes Back" )
