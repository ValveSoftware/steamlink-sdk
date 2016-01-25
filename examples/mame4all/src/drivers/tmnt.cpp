#include "../vidhrdw/tmnt.cpp"

/***************************************************************************

This driver contains several Konami 68000 based games. For the most part they
run on incompatible boards, but since 90% of the work is done by the custom
ICs emulated in vidhrdw/konamiic.c, we can just as well keep them all
together.

driver by Nicola Salmoria

TODO:

- detatwin: sprites are left on screen during attract mode
- sprite priorities in ssriders (protection)
- sprite colors / zoomed placement in tmnt2 (protection)
- is IPT_VBLANK really vblank or something else? Investigate.
- shadows: they should not be drawn as opaque sprites, instead they should
  make the background darker
- wrong sprites in ssriders at the end of the saloon level. They have the
  "shadow" bit on, but should actually highlight.
- sprite lag, quite evident in lgtnfght and mia but also in the others. Also
  see the left corner of the wall in punkshot DownTown level
- some slowdowns in lgtnfght when there are many sprites on screen - vblank issue?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"
#include "machine/eeprom.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"


WRITE_HANDLER( tmnt_paletteram_w );
WRITE_HANDLER( tmnt_0a0000_w );
WRITE_HANDLER( punkshot_0a0020_w );
WRITE_HANDLER( lgtnfght_0a0018_w );
WRITE_HANDLER( detatwin_700300_w );
WRITE_HANDLER( glfgreat_122000_w );
WRITE_HANDLER( ssriders_1c0300_w );
WRITE_HANDLER( tmnt_priority_w );
int mia_vh_start(void);
int tmnt_vh_start(void);
int punkshot_vh_start(void);
void punkshot_vh_stop(void);
int lgtnfght_vh_start(void);
void lgtnfght_vh_stop(void);
int detatwin_vh_start(void);
void detatwin_vh_stop(void);
int glfgreat_vh_start(void);
void glfgreat_vh_stop(void);
int thndrx2_vh_start(void);
void thndrx2_vh_stop(void);
void mia_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void tmnt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void punkshot_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void lgtnfght_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void glfgreat_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void ssriders_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void thndrx2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
static int tmnt_soundlatch;


static READ_HANDLER( K052109_word_r )
{
	offset >>= 1;
	return K052109_r(offset + 0x2000) | (K052109_r(offset) << 8);
}

static WRITE_HANDLER( K052109_word_w )
{
	offset >>= 1;
	if ((data & 0xff000000) == 0)
		K052109_w(offset,(data >> 8) & 0xff);
	if ((data & 0x00ff0000) == 0)
		K052109_w(offset + 0x2000,data & 0xff);
}

static READ_HANDLER( K052109_word_noA12_r )
{
	/* some games have the A12 line not connected, so the chip spans */
	/* twice the memory range, with mirroring */
	offset = ((offset & 0x6000) >> 1) | (offset & 0x0fff);
	return K052109_word_r(offset);
}

static WRITE_HANDLER( K052109_word_noA12_w )
{
	/* some games have the A12 line not connected, so the chip spans */
	/* twice the memory range, with mirroring */
	offset = ((offset & 0x6000) >> 1) | (offset & 0x0fff);
	K052109_word_w(offset,data);
}

/* the interface with the 053245 is weird. The chip can address only 0x800 bytes */
/* of RAM, but they put 0x4000 there. The CPU can access them all. Address lines */
/* A1, A5 and A6 don't go to the 053245. */
static READ_HANDLER( K053245_scattered_word_r )
{
	if (offset & 0x0062)
		return READ_WORD(&spriteram[offset]);
	else
	{
		offset = ((offset & 0x001c) >> 1) | ((offset & 0x3f80) >> 3);
		return K053245_word_r(offset);
	}
}

static WRITE_HANDLER( K053245_scattered_word_w )
{
	if (offset & 0x0062)
		COMBINE_WORD_MEM(&spriteram[offset],data);
	else
	{
		offset = ((offset & 0x001c) >> 1) | ((offset & 0x3f80) >> 3);
//if ((offset&0xf) == 0)
//	logerror("%04x: write %02x to spriteram %04x\n",cpu_get_pc(),data,offset);
		K053245_word_w(offset,data);
	}
}

static READ_HANDLER( K053244_halfword_r )
{
	return K053244_r(offset >> 1);
}

static WRITE_HANDLER( K053244_halfword_w )
{
	if ((data & 0x00ff0000) == 0)
		K053244_w(offset >> 1,data & 0xff);
}

static READ_HANDLER( K053244_word_noA1_r )
{
	offset &= ~2;	/* handle mirror address */

	return K053244_r(offset/2 + 1) | (K053244_r(offset/2) << 8);
}

static WRITE_HANDLER( K053244_word_noA1_w )
{
	offset &= ~2;	/* handle mirror address */

	if ((data & 0xff000000) == 0)
		K053244_w(offset/2,(data >> 8) & 0xff);
	if ((data & 0x00ff0000) == 0)
		K053244_w(offset/2 + 1,data & 0xff);
}

static WRITE_HANDLER( K053251_halfword_w )
{
	if ((data & 0x00ff0000) == 0)
		K053251_w(offset >> 1,data & 0xff);
}

static WRITE_HANDLER( K053251_halfword_swap_w )
{
	if ((data & 0xff000000) == 0)
		K053251_w(offset >> 1,(data >> 8) & 0xff);
}

static READ_HANDLER( K054000_halfword_r )
{
	return K054000_r(offset >> 1);
}

static WRITE_HANDLER( K054000_halfword_w )
{
	if ((data & 0x00ff0000) == 0)
		K054000_w(offset >> 1,data & 0xff);
}




static int punkshot_interrupt(void)
{
	if (K052109_is_IRQ_enabled()) return m68_level4_irq();
	else return ignore_interrupt();

}

static int lgtnfght_interrupt(void)
{
	if (K052109_is_IRQ_enabled()) return m68_level5_irq();
	else return ignore_interrupt();

}



WRITE_HANDLER( tmnt_sound_command_w )
{
	soundlatch_w(0,data & 0xff);
}

static READ_HANDLER( punkshot_sound_r )
{
	/* If the sound CPU is running, read the status, otherwise
	   just make it pass the test */
	if (Machine->sample_rate != 0) 	return K053260_r(2 + offset/2);
	else return 0x80;
}

static READ_HANDLER( detatwin_sound_r )
{
	/* If the sound CPU is running, read the status, otherwise
	   just make it pass the test */
	if (Machine->sample_rate != 0) 	return K053260_r(2 + offset/2);
	else return offset ? 0xfe : 0x00;
}

static READ_HANDLER( glfgreat_sound_r )
{
	/* If the sound CPU is running, read the status, otherwise
	   just make it pass the test */
	if (Machine->sample_rate != 0) 	return K053260_r(2 + offset/2) << 8;
	else return 0;
}

static WRITE_HANDLER( glfgreat_sound_w )
{
	if ((data & 0xff000000) == 0)
		K053260_w(offset >> 1,(data >> 8) & 0xff);

	if (offset == 2) cpu_cause_interrupt(1,0xff);
}

static READ_HANDLER( tmnt2_sound_r )
{
	/* If the sound CPU is running, read the status, otherwise
	   just make it pass the test */
	if (Machine->sample_rate != 0) 	return K053260_r(2 + offset/2);
	else return offset ? 0x00 : 0x80;
}


READ_HANDLER( tmnt_sres_r )
{
	return tmnt_soundlatch;
}

WRITE_HANDLER( tmnt_sres_w )
{
	/* bit 1 resets the UPD7795C sound chip */
	if ((data & 0x02) == 0)
	{
		UPD7759_reset_w(0,(data & 0x02) >> 1);
	}

	/* bit 2 plays the title music */
	if (data & 0x04)
	{
		if (!sample_playing(0))	sample_start(0,0,0);
	}
	else sample_stop(0);
	tmnt_soundlatch = data;
}


static int tmnt_decode_sample(const struct MachineSound *msound)
{
	int i;
	signed short *dest;
	unsigned char *source = memory_region(REGION_SOUND3);
	struct GameSamples *samples;


	if ((Machine->samples = (struct GameSamples*)malloc(sizeof(struct GameSamples))) == NULL)
		return 1;

	samples = Machine->samples;

	if ((samples->sample[0] = (struct GameSample*)malloc(sizeof(struct GameSample) + (0x40000)*sizeof(short))) == NULL)
		return 1;

	samples->sample[0]->length = 0x40000*2;
	samples->sample[0]->smpfreq = 20000;	/* 20 kHz */
	samples->sample[0]->resolution = 16;
	dest = (signed short *)samples->sample[0]->data;
	samples->total = 1;

	/*	Sound sample for TMNT.D05 is stored in the following mode:
	 *
	 *	Bit 15-13:	Exponent (2 ^ x)
	 *	Bit 12-4 :	Sound data (9 bit)
	 *
	 *	(Sound info courtesy of Dave <dayvee@rocketmail.com>)
	 */

	for (i = 0;i < 0x40000;i++)
	{
		int val = source[2*i] + source[2*i+1] * 256;
		int exp = val >> 13;

	  	val = (val >> 4) & (0x1ff);	/* 9 bit, Max Amplitude 0x200 */
		val -= 0x100;					/* Centralize value	*/

		val <<= exp;

		dest[i] = val;
	}

	/*	The sample is now ready to be used.  It's a 16 bit, 22khz sample.
	 */

	return 0;
}

//static int sound_nmi_enabled;

/*static void sound_nmi_callback( int param )
{
	cpu_set_nmi_line( 1, ( sound_nmi_enabled ) ? CLEAR_LINE : ASSERT_LINE );

	sound_nmi_enabled = 0;
}*/

static void nmi_callback(int param)
{
	cpu_set_nmi_line(1,ASSERT_LINE);
}

static WRITE_HANDLER( sound_arm_nmi_w )
{
//	sound_nmi_enabled = 1;
	cpu_set_nmi_line(1,CLEAR_LINE);
	timer_set(TIME_IN_USEC(50),0,nmi_callback);	/* kludge until the K053260 is emulated correctly */
}





static READ_HANDLER( punkshot_kludge_r )
{
	/* I don't know what's going on here; at one point, the code reads location */
	/* 0xffffff, and returning 0 causes the game to mess up - locking up in a */
	/* loop where the ball is continuously bouncing from the basket. Returning */
	/* a random number seems to prevent that. */
	return rand();
}

static READ_HANDLER( ssriders_kludge_r )
{
    int data = cpu_readmem24bew_word(0x105a0a);

    //logerror("%06x: read 1c0800 (D7=%02x 105a0a=%02x)\n",cpu_get_pc(),cpu_get_reg(M68K_D7),data);

    if (data == 0x075c) data = 0x0064;

    if ( cpu_readmem24bew_word(cpu_get_pc()) == 0x45f9 )
	{
        data = -( ( cpu_get_reg(M68K_D7) & 0xff ) + 32 );
        data = ( ( data / 8 ) & 0x1f ) * 0x40;
        data += ( ( ( cpu_get_reg(M68K_D6) & 0xffff ) + ( K052109_r(0x1a01) * 256 )
				+ K052109_r(0x1a00) + 96 ) / 8 ) & 0x3f;
    }

    return data;
}



/***************************************************************************

  EEPROM

***************************************************************************/

static int init_eeprom_count;


static struct EEPROM_interface eeprom_interface =
{
	7,				/* address bits */
	8,				/* data bits */
	"011000",		/*  read command */
	"011100",		/* write command */
	0,				/* erase command */
	"0100000000000",/* lock command */
	"0100110000000" /* unlock command */
};

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface);

		if (file)
		{
			init_eeprom_count = 0;
			EEPROM_load(file);
		}
		else
			init_eeprom_count = 10;
	}
}

static READ_HANDLER( detatwin_coin_r )
{
	int res;
	static int toggle;

	/* bit 3 is service button */
	/* bit 6 is ??? VBLANK? OBJMPX? */
	res = input_port_2_r(0);
	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xf7;
	}
	toggle ^= 0x40;
	return res ^ toggle;
}

static READ_HANDLER( detatwin_eeprom_r )
{
	int res;

	/* bit 0 is EEPROM data */
	/* bit 1 is EEPROM ready */
	res = EEPROM_read_bit() | input_port_3_r(0);
	return res;
}

static READ_HANDLER( ssriders_eeprom_r )
{
	int res;
	static int toggle;

	/* bit 0 is EEPROM data */
	/* bit 1 is EEPROM ready */
	/* bit 2 is VBLANK (???) */
	/* bit 7 is service button */
	res = EEPROM_read_bit() | input_port_3_r(0);
	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0x7f;
	}
	toggle ^= 0x04;
	return res ^ toggle;
}

static WRITE_HANDLER( ssriders_eeprom_w )
{
	/* bit 0 is data */
	/* bit 1 is cs (active low) */
	/* bit 2 is clock (active high) */
	EEPROM_write_bit(data & 0x01);
	EEPROM_set_cs_line((data & 0x02) ? CLEAR_LINE : ASSERT_LINE);
	EEPROM_set_clock_line((data & 0x04) ? ASSERT_LINE : CLEAR_LINE);

	/* bit 5 selects sprite ROM for testing in TMNT2 */
	K053244_bankselect((data & 0x20) >> 5);
}


static struct EEPROM_interface thndrx2_eeprom_interface =
{
	7,				/* address bits */
	8,				/* data bits */
	"011000",		/*  read command */
	"010100",		/* write command */
	0,				/* erase command */
	"0100000000000",/* lock command */
	"0100110000000" /* unlock command */
};

static void thndrx2_nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&thndrx2_eeprom_interface);

		if (file)
		{
			init_eeprom_count = 0;
			EEPROM_load(file);
		}
		else
			init_eeprom_count = 10;
	}
}

static READ_HANDLER( thndrx2_in0_r )
{
	int res;

	res = input_port_0_r(0);
	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xf7ff;
	}
	return res;
}

static READ_HANDLER( thndrx2_eeprom_r )
{
	int res;
	static int toggle;

	/* bit 0 is EEPROM data */
	/* bit 1 is EEPROM ready */
	/* bit 3 is VBLANK (???) */
	/* bit 7 is service button */
	res = (EEPROM_read_bit() << 8) | input_port_1_r(0);
	toggle ^= 0x0800;
	return (res ^ toggle);
}

static WRITE_HANDLER( thndrx2_eeprom_w )
{
	static int last;

	if ((data & 0x00ff0000) == 0)
	{
		/* bit 0 is data */
		/* bit 1 is cs (active low) */
		/* bit 2 is clock (active high) */
		EEPROM_write_bit(data & 0x01);
		EEPROM_set_cs_line((data & 0x02) ? CLEAR_LINE : ASSERT_LINE);
		EEPROM_set_clock_line((data & 0x04) ? ASSERT_LINE : CLEAR_LINE);

		/* bit 5 triggers IRQ on sound cpu */
		if (last == 0 && (data & 0x20) != 0)
			cpu_cause_interrupt(1,0xff);
		last = data & 0x20;

		/* bit 6 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x40) ? ASSERT_LINE : CLEAR_LINE);
	}
}



static struct MemoryReadAddress mia_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x040000, 0x043fff, MRA_BANK3 },	/* main RAM */
	{ 0x060000, 0x063fff, MRA_BANK1 },	/* main RAM */
	{ 0x080000, 0x080fff, paletteram_word_r },
	{ 0x0a0000, 0x0a0001, input_port_0_r },
	{ 0x0a0002, 0x0a0003, input_port_1_r },
	{ 0x0a0004, 0x0a0005, input_port_2_r },
	{ 0x0a0010, 0x0a0011, input_port_3_r },
	{ 0x0a0012, 0x0a0013, input_port_4_r },
	{ 0x0a0018, 0x0a0019, input_port_5_r },
	{ 0x100000, 0x107fff, K052109_word_noA12_r },
	{ 0x140000, 0x140007, K051937_word_r },
	{ 0x140400, 0x1407ff, K051960_word_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress mia_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x040000, 0x043fff, MWA_BANK3 },	/* main RAM */
	{ 0x060000, 0x063fff, MWA_BANK1 },	/* main RAM */
	{ 0x080000, 0x080fff, tmnt_paletteram_w, &paletteram },
	{ 0x0a0000, 0x0a0001, tmnt_0a0000_w },
	{ 0x0a0008, 0x0a0009, tmnt_sound_command_w },
	{ 0x0a0010, 0x0a0011, watchdog_reset_w },
	{ 0x100000, 0x107fff, K052109_word_noA12_w },
	{ 0x140000, 0x140007, K051937_word_w },
	{ 0x140400, 0x1407ff, K051960_word_w },
//	{ 0x10e800, 0x10e801, MWA_NOP }, ???
#if 0
	{ 0x0c0000, 0x0c0001, tmnt_priority_w },
#endif
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress tmnt_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x060000, 0x063fff, MRA_BANK1 },	/* main RAM */
	{ 0x080000, 0x080fff, paletteram_word_r },
	{ 0x0a0000, 0x0a0001, input_port_0_r },
	{ 0x0a0002, 0x0a0003, input_port_1_r },
	{ 0x0a0004, 0x0a0005, input_port_2_r },
	{ 0x0a0006, 0x0a0007, input_port_3_r },
	{ 0x0a0010, 0x0a0011, input_port_4_r },
	{ 0x0a0012, 0x0a0013, input_port_5_r },
	{ 0x0a0014, 0x0a0015, input_port_6_r },
	{ 0x0a0018, 0x0a0019, input_port_7_r },
	{ 0x100000, 0x107fff, K052109_word_noA12_r },
	{ 0x140000, 0x140007, K051937_word_r },
	{ 0x140400, 0x1407ff, K051960_word_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress tmnt_writemem[] =
{
	{ 0x000000, 0x05ffff, MWA_ROM },
	{ 0x060000, 0x063fff, MWA_BANK1 },	/* main RAM */
	{ 0x080000, 0x080fff, tmnt_paletteram_w, &paletteram },
	{ 0x0a0000, 0x0a0001, tmnt_0a0000_w },
	{ 0x0a0008, 0x0a0009, tmnt_sound_command_w },
	{ 0x0a0010, 0x0a0011, watchdog_reset_w },
	{ 0x0c0000, 0x0c0001, tmnt_priority_w },
	{ 0x100000, 0x107fff, K052109_word_noA12_w },
//	{ 0x10e800, 0x10e801, MWA_NOP }, ???
	{ 0x140000, 0x140007, K051937_word_w },
	{ 0x140400, 0x1407ff, K051960_word_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress punkshot_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x080000, 0x083fff, MRA_BANK1 },	/* main RAM */
	{ 0x090000, 0x090fff, paletteram_word_r },
	{ 0x0a0000, 0x0a0001, input_port_0_r },
	{ 0x0a0002, 0x0a0003, input_port_1_r },
	{ 0x0a0004, 0x0a0005, input_port_3_r },
	{ 0x0a0006, 0x0a0007, input_port_2_r },
	{ 0x0a0040, 0x0a0043, punkshot_sound_r },	/* K053260 */
	{ 0x100000, 0x107fff, K052109_word_noA12_r },
	{ 0x110000, 0x110007, K051937_word_r },
	{ 0x110400, 0x1107ff, K051960_word_r },
	{ 0xfffffc, 0xffffff, punkshot_kludge_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress punkshot_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x080000, 0x083fff, MWA_BANK1 },	/* main RAM */
	{ 0x090000, 0x090fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x0a0020, 0x0a0021, punkshot_0a0020_w },
	{ 0x0a0040, 0x0a0041, K053260_w },
	{ 0x0a0060, 0x0a007f, K053251_halfword_w },
	{ 0x0a0080, 0x0a0081, watchdog_reset_w },
	{ 0x100000, 0x107fff, K052109_word_noA12_w },
	{ 0x110000, 0x110007, K051937_word_w },
	{ 0x110400, 0x1107ff, K051960_word_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress lgtnfght_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x080000, 0x080fff, paletteram_word_r },
	{ 0x090000, 0x093fff, MRA_BANK1 },	/* main RAM */
	{ 0x0a0000, 0x0a0001, input_port_0_r },
	{ 0x0a0002, 0x0a0003, input_port_1_r },
	{ 0x0a0004, 0x0a0005, input_port_2_r },
	{ 0x0a0006, 0x0a0007, input_port_3_r },
	{ 0x0a0008, 0x0a0009, input_port_4_r },
	{ 0x0a0010, 0x0a0011, input_port_5_r },
	{ 0x0a0020, 0x0a0023, punkshot_sound_r },	/* K053260 */
	{ 0x0b0000, 0x0b3fff, K053245_scattered_word_r },
	{ 0x0c0000, 0x0c001f, K053244_word_noA1_r },
	{ 0x100000, 0x107fff, K052109_word_noA12_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress lgtnfght_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x080000, 0x080fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x090000, 0x093fff, MWA_BANK1 },	/* main RAM */
	{ 0x0a0018, 0x0a0019, lgtnfght_0a0018_w },
	{ 0x0a0020, 0x0a0021, K053260_w },
	{ 0x0a0028, 0x0a0029, watchdog_reset_w },
	{ 0x0b0000, 0x0b3fff, K053245_scattered_word_w, &spriteram },
	{ 0x0c0000, 0x0c001f, K053244_word_noA1_w },
	{ 0x0e0000, 0x0e001f, K053251_halfword_w },
	{ 0x100000, 0x107fff, K052109_word_noA12_w },
	{ -1 }	/* end of table */
};


static WRITE_HANDLER( ssriders_soundkludge_w )
{
	/* I think this is more than just a trigger */
	cpu_cause_interrupt(1,0xff);
}

static struct MemoryReadAddress detatwin_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x180000, 0x183fff, K052109_word_r },
	{ 0x204000, 0x207fff, MRA_BANK1 },	/* main RAM */
	{ 0x300000, 0x303fff, K053245_scattered_word_r },
	{ 0x400000, 0x400fff, paletteram_word_r },
	{ 0x500000, 0x50003f, K054000_halfword_r },
	{ 0x680000, 0x68001f, K053244_word_noA1_r },
	{ 0x700000, 0x700001, input_port_0_r },
	{ 0x700002, 0x700003, input_port_1_r },
	{ 0x700004, 0x700005, detatwin_coin_r },
	{ 0x700006, 0x700007, detatwin_eeprom_r },
	{ 0x780600, 0x780603, detatwin_sound_r },	/* K053260 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress detatwin_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x180000, 0x183fff, K052109_word_w },
	{ 0x204000, 0x207fff, MWA_BANK1 },	/* main RAM */
	{ 0x300000, 0x303fff, K053245_scattered_word_w, &spriteram },
	{ 0x500000, 0x50003f, K054000_halfword_w },
	{ 0x400000, 0x400fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x680000, 0x68001f, K053244_word_noA1_w },
	{ 0x700200, 0x700201, ssriders_eeprom_w },
	{ 0x700400, 0x700401, watchdog_reset_w },
	{ 0x700300, 0x700301, detatwin_700300_w },
	{ 0x780600, 0x780601, K053260_w },
	{ 0x780604, 0x780605, ssriders_soundkludge_w },
	{ 0x780700, 0x78071f, K053251_halfword_w },
	{ -1 }	/* end of table */
};


static READ_HANDLER( ball_r )
{
	return 0x11;
}

static struct MemoryReadAddress glfgreat_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x103fff, MRA_BANK1 },	/* main RAM */
	{ 0x104000, 0x107fff, K053245_scattered_word_r },
	{ 0x108000, 0x108fff, paletteram_word_r },
	{ 0x10c000, 0x10cfff, MRA_BANK2 },	/* 053936? */
	{ 0x114000, 0x11401f, K053244_halfword_r },
	{ 0x120000, 0x120001, input_port_0_r },
	{ 0x120002, 0x120003, input_port_1_r },
	{ 0x120004, 0x120005, input_port_3_r },
	{ 0x120006, 0x120007, input_port_2_r },
	{ 0x121000, 0x121001, ball_r },	/* protection? returning 0, every shot is a "water" */
	{ 0x125000, 0x125003, glfgreat_sound_r },	/* K053260 */
	{ 0x200000, 0x207fff, K052109_word_noA12_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress glfgreat_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x103fff, MWA_BANK1 },	/* main RAM */
	{ 0x104000, 0x107fff, K053245_scattered_word_w, &spriteram },
	{ 0x108000, 0x108fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x10c000, 0x10cfff, MWA_BANK2 },	/* 053936? */
	{ 0x110000, 0x11001f, K053244_word_noA1_w },	/* duplicate! */
	{ 0x114000, 0x11401f, K053244_halfword_w },		/* duplicate! */
	{ 0x118000, 0x11801f, MWA_NOP },	/* 053936 control? */
	{ 0x11c000, 0x11c01f, K053251_halfword_swap_w },
	{ 0x122000, 0x122001, glfgreat_122000_w },
	{ 0x124000, 0x124001, watchdog_reset_w },
	{ 0x125000, 0x125003, glfgreat_sound_w },	/* K053260 */
	{ 0x200000, 0x207fff, K052109_word_noA12_w },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress tmnt2_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x104000, 0x107fff, MRA_BANK1 },	/* main RAM */
	{ 0x140000, 0x140fff, paletteram_word_r },
	{ 0x180000, 0x183fff, K053245_scattered_word_r },
	{ 0x1c0000, 0x1c0001, input_port_0_r },
	{ 0x1c0002, 0x1c0003, input_port_1_r },
	{ 0x1c0004, 0x1c0005, input_port_4_r },
	{ 0x1c0006, 0x1c0007, input_port_5_r },
	{ 0x1c0100, 0x1c0101, input_port_2_r },
	{ 0x1c0102, 0x1c0103, ssriders_eeprom_r },
	{ 0x1c0400, 0x1c0401, watchdog_reset_r },
	{ 0x1c0500, 0x1c057f, MRA_BANK3 },	/* TMNT2 only (1J); unknown */
//	{ 0x1c0800, 0x1c0801, ssriders_kludge_r },	/* protection device */
	{ 0x5a0000, 0x5a001f, K053244_word_noA1_r },
	{ 0x5c0600, 0x5c0603, tmnt2_sound_r },	/* K053260 */
	{ 0x600000, 0x603fff, K052109_word_r },
	{ -1 }	/* end of table */
};

static unsigned char *tmnt2_1c0800,*sunset_104000;

WRITE_HANDLER( tmnt2_1c0800_w )
{
    COMBINE_WORD_MEM( &tmnt2_1c0800[offset], data );
    if ( offset == 0x0010 && ( ( READ_WORD( &tmnt2_1c0800[0x10] ) & 0xff00 ) == 0x8200 ) )
	{
		unsigned int CellSrc;
		unsigned int CellVar;
		unsigned char *src;
		int dst;
		int x,y;

		CellVar = ( READ_WORD( &tmnt2_1c0800[0x08] ) | ( READ_WORD( &tmnt2_1c0800[0x0A] ) << 16 ) );
		dst = ( READ_WORD( &tmnt2_1c0800[0x04] ) | ( READ_WORD( &tmnt2_1c0800[0x06] ) << 16 ) );
		CellSrc = ( READ_WORD( &tmnt2_1c0800[0x00] ) | ( READ_WORD( &tmnt2_1c0800[0x02] ) << 16 ) );
//        if ( CellDest >= 0x180000 && CellDest < 0x183fe0 ) {
        CellVar -= 0x104000;
		src = &memory_region(REGION_CPU1)[CellSrc];

		cpu_writemem24bew_word(dst+0x00,0x8000 | ((READ_WORD(src+2) & 0xfc00) >> 2));	/* size, flip xy */
        cpu_writemem24bew_word(dst+0x04,READ_WORD(src+0));	/* code */
        cpu_writemem24bew_word(dst+0x18,(READ_WORD(src+2) & 0x3ff) ^		/* color, mirror, priority */
				(READ_WORD( &sunset_104000[CellVar + 0x00] ) & 0x0060));

		/* base color modifier */
		/* TODO: this is wrong, e.g. it breaks the explosions when you kill an */
		/* enemy, or surfs in the sewer level (must be blue for all enemies). */
		/* It fixes the enemies, though, they are not all purple when you throw them around. */
		/* Also, the bosses don't blink when they are about to die - don't know */
		/* if this is correct or not. */
//		if (READ_WORD( &sunset_104000[CellVar + 0x2a] ) & 0x001f)
//			cpu_writemem24bew_word(dst+0x18,(cpu_readmem24bew_word(dst+0x18) & 0xffe0) |
//					(READ_WORD( &sunset_104000[CellVar + 0x2a] ) & 0x001f));

		x = READ_WORD(src+4);
		if (READ_WORD( &sunset_104000[CellVar + 0x00] ) & 0x4000)
		{
			/* flip x */
			cpu_writemem24bew_word(dst+0x00,cpu_readmem24bew_word(dst+0x00) ^ 0x1000);
			x = -x;
		}
		x += READ_WORD( &sunset_104000[CellVar + 0x0C] );
		cpu_writemem24bew_word(dst+0x0c,x);
		y = READ_WORD(src+6);
		y += READ_WORD( &sunset_104000[CellVar + 0x0E] );
		/* don't do second offset for shadows */
		if ((READ_WORD(&tmnt2_1c0800[0x10]) & 0x00ff) != 0x01)
			y += READ_WORD( &sunset_104000[CellVar + 0x10] );
		cpu_writemem24bew_word(dst+0x08,y);
#if 0
logerror("copy command %04x sprite %08x data %08x: %04x%04x %04x%04x  modifiers %08x:%04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x %04x%04x\n",
	READ_WORD( &tmnt2_1c0800[0x10] ),
	CellDest,CellSrc,
	READ_WORD(src+0),READ_WORD(src+2),READ_WORD(src+4),READ_WORD(src+6),
	CellVar,
	READ_WORD( &sunset_104000[CellVar + 0x00]),
	READ_WORD( &sunset_104000[CellVar + 0x02]),
	READ_WORD( &sunset_104000[CellVar + 0x04]),
	READ_WORD( &sunset_104000[CellVar + 0x06]),
	READ_WORD( &sunset_104000[CellVar + 0x08]),
	READ_WORD( &sunset_104000[CellVar + 0x0a]),
	READ_WORD( &sunset_104000[CellVar + 0x0c]),
	READ_WORD( &sunset_104000[CellVar + 0x0e]),
	READ_WORD( &sunset_104000[CellVar + 0x10]),
	READ_WORD( &sunset_104000[CellVar + 0x12]),
	READ_WORD( &sunset_104000[CellVar + 0x14]),
	READ_WORD( &sunset_104000[CellVar + 0x16]),
	READ_WORD( &sunset_104000[CellVar + 0x18]),
	READ_WORD( &sunset_104000[CellVar + 0x1a]),
	READ_WORD( &sunset_104000[CellVar + 0x1c]),
	READ_WORD( &sunset_104000[CellVar + 0x1e]),
	READ_WORD( &sunset_104000[CellVar + 0x20]),
	READ_WORD( &sunset_104000[CellVar + 0x22]),
	READ_WORD( &sunset_104000[CellVar + 0x24]),
	READ_WORD( &sunset_104000[CellVar + 0x26]),
	READ_WORD( &sunset_104000[CellVar + 0x28]),
	READ_WORD( &sunset_104000[CellVar + 0x2a]),
	READ_WORD( &sunset_104000[CellVar + 0x2c]),
	READ_WORD( &sunset_104000[CellVar + 0x2e])
	);
#endif
//        }
    }
}

static struct MemoryWriteAddress tmnt2_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x104000, 0x107fff, MWA_BANK1, &sunset_104000 },	/* main RAM */
	{ 0x140000, 0x140fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x180000, 0x183fff, K053245_scattered_word_w, &spriteram },
	{ 0x1c0200, 0x1c0201, ssriders_eeprom_w },
	{ 0x1c0300, 0x1c0301, ssriders_1c0300_w },
	{ 0x1c0400, 0x1c0401, watchdog_reset_w },
	{ 0x1c0500, 0x1c057f, MWA_BANK3 },	/* unknown: TMNT2 only (1J) */
	{ 0x1c0800, 0x1c081f, tmnt2_1c0800_w, &tmnt2_1c0800 },	/* protection device */
	{ 0x5a0000, 0x5a001f, K053244_word_noA1_w },
	{ 0x5c0600, 0x5c0601, K053260_w },
	{ 0x5c0604, 0x5c0605, ssriders_soundkludge_w },
	{ 0x5c0700, 0x5c071f, K053251_halfword_w },
	{ 0x600000, 0x603fff, K052109_word_w },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress ssriders_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x104000, 0x107fff, MRA_BANK1 },	/* main RAM */
	{ 0x140000, 0x140fff, paletteram_word_r },
	{ 0x180000, 0x183fff, K053245_scattered_word_r },
	{ 0x1c0000, 0x1c0001, input_port_0_r },
	{ 0x1c0002, 0x1c0003, input_port_1_r },
	{ 0x1c0004, 0x1c0005, input_port_4_r },
	{ 0x1c0006, 0x1c0007, input_port_5_r },
	{ 0x1c0100, 0x1c0101, input_port_2_r },
	{ 0x1c0102, 0x1c0103, ssriders_eeprom_r },
	{ 0x1c0400, 0x1c0401, watchdog_reset_r },
	{ 0x1c0500, 0x1c057f, MRA_BANK3 },	/* TMNT2 only (1J); unknown */
	{ 0x1c0800, 0x1c0801, ssriders_kludge_r },	/* protection device */
	{ 0x5a0000, 0x5a001f, K053244_word_noA1_r },
	{ 0x5c0600, 0x5c0603, punkshot_sound_r },	/* K053260 */
	{ 0x600000, 0x603fff, K052109_word_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ssriders_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x104000, 0x107fff, MWA_BANK1 },	/* main RAM */
	{ 0x140000, 0x140fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x180000, 0x183fff, K053245_scattered_word_w, &spriteram },
	{ 0x1c0200, 0x1c0201, ssriders_eeprom_w },
	{ 0x1c0300, 0x1c0301, ssriders_1c0300_w },
	{ 0x1c0400, 0x1c0401, watchdog_reset_w },
	{ 0x1c0500, 0x1c057f, MWA_BANK3 },	/* TMNT2 only (1J); unknown */
//	{ 0x1c0800, 0x1c081f,  },	/* protection device */
	{ 0x5a0000, 0x5a001f, K053244_word_noA1_w },
	{ 0x5c0600, 0x5c0601, K053260_w },
	{ 0x5c0604, 0x5c0605, ssriders_soundkludge_w },
	{ 0x5c0700, 0x5c071f, K053251_halfword_w },
	{ 0x600000, 0x603fff, K052109_word_w },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress thndrx2_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x103fff, MRA_BANK1 },	/* main RAM */
	{ 0x200000, 0x200fff, paletteram_word_r },
	{ 0x400000, 0x400003, punkshot_sound_r },	/* K053260 */
	{ 0x500000, 0x50003f, K054000_halfword_r },
	{ 0x500200, 0x500201, thndrx2_in0_r },
	{ 0x500202, 0x500203, thndrx2_eeprom_r },
	{ 0x600000, 0x607fff, K052109_word_noA12_r },
	{ 0x700000, 0x700007, K051937_word_r },
	{ 0x700400, 0x7007ff, K051960_word_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress thndrx2_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x103fff, MWA_BANK1 },	/* main RAM */
	{ 0x200000, 0x200fff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x300000, 0x30001f, K053251_halfword_w },
	{ 0x400000, 0x400001, K053260_w },
	{ 0x500000, 0x50003f, K054000_halfword_w },
	{ 0x500100, 0x500101, thndrx2_eeprom_w },
	{ 0x500300, 0x500301, MWA_NOP },	/* watchdog reset? irq enable? */
	{ 0x600000, 0x607fff, K052109_word_noA12_w },
	{ 0x700000, 0x700007, K051937_word_w },
	{ 0x700400, 0x7007ff, K051960_word_w },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress mia_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0xb000, 0xb00d, K007232_read_port_0_r },
	{ 0xc001, 0xc001, YM2151_status_port_0_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress mia_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xb000, 0xb00d, K007232_write_port_0_w },
	{ 0xc000, 0xc000, YM2151_register_port_0_w },
	{ 0xc001, 0xc001, YM2151_data_port_0_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress tmnt_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x9000, tmnt_sres_r },	/* title music & UPD7759C reset */
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0xb000, 0xb00d, K007232_read_port_0_r },
	{ 0xc001, 0xc001, YM2151_status_port_0_r },
	{ 0xf000, 0xf000, UPD7759_0_busy_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress tmnt_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x9000, tmnt_sres_w },	/* title music & UPD7759C reset */
	{ 0xb000, 0xb00d, K007232_write_port_0_w  },
	{ 0xc000, 0xc000, YM2151_register_port_0_w },
	{ 0xc001, 0xc001, YM2151_data_port_0_w },
	{ 0xd000, 0xd000, UPD7759_0_message_w },
	{ 0xe000, 0xe000, UPD7759_0_start_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress punkshot_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf801, 0xf801, YM2151_status_port_0_r },
	{ 0xfc00, 0xfc2f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress punkshot_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf800, YM2151_register_port_0_w },
	{ 0xf801, 0xf801, YM2151_data_port_0_w },
	{ 0xfa00, 0xfa00, sound_arm_nmi_w },
	{ 0xfc00, 0xfc2f, K053260_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress lgtnfght_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa001, 0xa001, YM2151_status_port_0_r },
	{ 0xc000, 0xc02f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress lgtnfght_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa000, 0xa000, YM2151_register_port_0_w },
	{ 0xa001, 0xa001, YM2151_data_port_0_w },
	{ 0xc000, 0xc02f, K053260_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress glfgreat_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xf82f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress glfgreat_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf82f, K053260_w },
	{ 0xfa00, 0xfa00, sound_arm_nmi_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress ssriders_s_readmem[] =
{
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf801, 0xf801, YM2151_status_port_0_r },
	{ 0xfa00, 0xfa2f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ssriders_s_writemem[] =
{
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf800, YM2151_register_port_0_w },
	{ 0xf801, 0xf801, YM2151_data_port_0_w },
	{ 0xfa00, 0xfa2f, K053260_w },
	{ 0xfc00, 0xfc00, sound_arm_nmi_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress thndrx2_s_readmem[] =
{
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf801, 0xf801, YM2151_status_port_0_r },
	{ 0xfc00, 0xfc2f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress thndrx2_s_writemem[] =
{
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf800, YM2151_register_port_0_w },
	{ 0xf801, 0xf801, YM2151_data_port_0_w },
	{ 0xf811, 0xf811, YM2151_data_port_0_w },	/* mirror */
	{ 0xfa00, 0xfa00, sound_arm_nmi_w },
	{ 0xfc00, 0xfc2f, K053260_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( mia )
	PORT_START      /* COINS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* PLAYER 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* PLAYER 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "30000 80000" )
	PORT_DIPSETTING(    0x10, "50000 100000" )
	PORT_DIPSETTING(    0x08, "50000" )
	PORT_DIPSETTING(    0x00, "100000" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Character Test" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( tmnt )
	PORT_START      /* COINS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE4 )

	PORT_START      /* PLAYER 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* PLAYER 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START      /* PLAYER 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* PLAYER 4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( tmnt2p )
	PORT_START      /* COINS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* PLAYER 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* PLAYER 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START      /* PLAYER 3 */
//	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER3 | IPF_8WAY )
//	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
//	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER3 | IPF_8WAY )
//	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER3 | IPF_8WAY )
//	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
//	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
//	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
//	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* PLAYER 4 */
//	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER4 | IPF_8WAY )
//	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
//	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER4 | IPF_8WAY )
//	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER4 | IPF_8WAY )
//	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
//	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
//	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
//	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( punkshot )
	PORT_START	/* DSW1/DSW2 */
	PORT_DIPNAME( 0x000f, 0x000f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x000f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x000d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Continue" )
	PORT_DIPSETTING(      0x0010, "Normal" )
	PORT_DIPSETTING(      0x0000, "1 Coin" )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0300, 0x0300, "Energy" )
	PORT_DIPSETTING(      0x0300, "30" )
	PORT_DIPSETTING(      0x0200, "40" )
	PORT_DIPSETTING(      0x0100, "50" )
	PORT_DIPSETTING(      0x0000, "60" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Period Length" )
	PORT_DIPSETTING(      0x0c00, "2 Minutes" )
	PORT_DIPSETTING(      0x0800, "3 Minutes" )
	PORT_DIPSETTING(      0x0400, "4 Minutes" )
	PORT_DIPSETTING(      0x0000, "5 Minutes" )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x6000, 0x6000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x6000, "Easy" )
	PORT_DIPSETTING(      0x4000, "Medium" )
	PORT_DIPSETTING(      0x2000, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START	/* COIN/DSW3 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_SERVICE4 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x8000, 0x8000, "Freeze" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START	/* IN0/IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2/IN3 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( punksht2 )
	PORT_START	/* DSW1/DSW2 */
	PORT_DIPNAME( 0x000f, 0x000f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x000f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x000d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Continue" )
	PORT_DIPSETTING(      0x0010, "Normal" )
	PORT_DIPSETTING(      0x0000, "1 Coin" )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0300, 0x0300, "Energy" )
	PORT_DIPSETTING(      0x0300, "40" )
	PORT_DIPSETTING(      0x0200, "50" )
	PORT_DIPSETTING(      0x0100, "60" )
	PORT_DIPSETTING(      0x0000, "70" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Period Length" )
	PORT_DIPSETTING(      0x0c00, "3 Minutes" )
	PORT_DIPSETTING(      0x0800, "4 Minutes" )
	PORT_DIPSETTING(      0x0400, "5 Minutes" )
	PORT_DIPSETTING(      0x0000, "6 Minutes" )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x6000, 0x6000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x6000, "Easy" )
	PORT_DIPSETTING(      0x4000, "Medium" )
	PORT_DIPSETTING(      0x2000, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START	/* COIN/DSW3 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x8000, 0x8000, "Freeze" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START	/* IN0/IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( lgtnfght )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* vblank? checked during boot */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "100000 400000" )
	PORT_DIPSETTING(    0x10, "150000 500000" )
	PORT_DIPSETTING(    0x08, "200000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound" )
	PORT_DIPSETTING(    0x02, "Mono" )
	PORT_DIPSETTING(    0x00, "Stereo" )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( detatwin )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* COIN */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* VBLANK? OBJMPX? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* EEPROM */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status? - always 1 */
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( glfgreat )
	PORT_START	/* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER4 )

	PORT_START
	PORT_DIPNAME( 0x000f, 0x000f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x000f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x000d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x00f0, 0x00f0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0050, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x00f0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x0070, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x00e0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x00d0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(      0x0000, "Invalid" )
	PORT_DIPNAME( 0x0300, 0x0100, "Players/Controllers" )
	PORT_DIPSETTING(      0x0300, "4/1" )
	PORT_DIPSETTING(      0x0200, "4/2" )
	PORT_DIPSETTING(      0x0100, "4/4" )
	PORT_DIPSETTING(      0x0000, "3/3" )
	PORT_DIPNAME( 0x0400, 0x0000, "Sound" )
	PORT_DIPSETTING(      0x0400, "Mono" )
	PORT_DIPSETTING(      0x0000, "Stereo" )
	PORT_DIPNAME( 0x1800, 0x1800, "Initial/Maximum Credit" )
	PORT_DIPSETTING(      0x1800, "2/3" )
	PORT_DIPSETTING(      0x1000, "2/4" )
	PORT_DIPSETTING(      0x0800, "2/5" )
	PORT_DIPSETTING(      0x0000, "3/5" )
	PORT_DIPNAME( 0x6000, 0x4000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x6000, "Easy" )
	PORT_DIPSETTING(      0x4000, "Normal" )
	PORT_DIPSETTING(      0x2000, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )	/* service coin */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BITX(0x0400, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPNAME( 0x8000, 0x0000, "Freeze" )	/* ?? VBLANK ?? */
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
//	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ssridr4p )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* COIN */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE4 )

	PORT_START	/* EEPROM and service */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status? - always 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: OBJMPX */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: NVBLK */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: IPL0 */
	PORT_BIT( 0x60, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( ssriders )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* COIN */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* EEPROM and service */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status? - always 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: OBJMPX */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: NVBLK */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: IPL0 */
	PORT_BIT( 0x60, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( tmnt2a )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* COIN */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE4 )

	PORT_START	/* EEPROM and service */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status? - always 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: OBJMPX */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: NVBLK */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* ?? TMNT2: IPL0 */
	PORT_BIT( 0x60, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused? */
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )
INPUT_PORTS_END

INPUT_PORTS_START( thndrx2 )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* EEPROM and service */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* EEPROM data */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM status? - always 1 */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* VBLK?? */
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ 0 }
};

static void volume_callback(int v)
{
	K007232_set_volume(0,0,(v >> 4) * 0x11,0);
	K007232_set_volume(0,1,0,(v & 0x0f) * 0x11);
}

static struct K007232_interface k007232_interface =
{
	1,		/* number of chips */
	{ REGION_SOUND1 },	/* memory regions */
	{ K007232_VOL(20,MIXER_PAN_CENTER,20,MIXER_PAN_CENTER) },	/* volume */
	{ volume_callback }	/* external port callback */
};

static struct UPD7759_interface upd7759_interface =
{
	1,		/* number of chips */
	UPD7759_STANDARD_CLOCK,
	{ 60 }, /* volume */
	{ REGION_SOUND2 },		/* memory region */
	UPD7759_STANDALONE_MODE,		/* chip mode */
	{0}
};

static struct Samplesinterface samples_interface =
{
	1,	/* 1 channel for the title music */
	25	/* volume */
};

static struct CustomSound_interface custom_interface =
{
	tmnt_decode_sample,
	0,
	0
};

static struct K053260_interface k053260_interface_nmi =
{
	3579545,
	REGION_SOUND1, /* memory region */
	{ MIXER(70,MIXER_PAN_LEFT), MIXER(70,MIXER_PAN_RIGHT) },
//	sound_nmi_callback,
};

static struct K053260_interface k053260_interface =
{
	3579545,
	REGION_SOUND1, /* memory region */
	{ MIXER(70,MIXER_PAN_LEFT), MIXER(70,MIXER_PAN_RIGHT) },
	0
};

static struct K053260_interface glfgreat_k053260_interface =
{
	3579545,
	REGION_SOUND1, /* memory region */
	{ MIXER(100,MIXER_PAN_LEFT), MIXER(100,MIXER_PAN_RIGHT) },
//	sound_nmi_callback,
};


static struct MachineDriver machine_driver_mia =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 MHz */
			mia_readmem,mia_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			mia_s_readmem,mia_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	mia_vh_start,
	punkshot_vh_stop,
	mia_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K007232,
			&k007232_interface,
		}
	}
};

static struct MachineDriver machine_driver_tmnt =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 MHz */
			tmnt_readmem,tmnt_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			tmnt_s_readmem,tmnt_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	tmnt_vh_start,
	punkshot_vh_stop,
	tmnt_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K007232,
			&k007232_interface,
		},
		{
			SOUND_UPD7759,
			&upd7759_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
		},
		{
			SOUND_CUSTOM,	/* actually initializes the samples */
			&custom_interface
		}
	}
};

static struct MachineDriver machine_driver_punkshot =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* CPU is 68000/12, but this doesn't necessarily mean it's */
						/* running at 12MHz. TMNT uses 8MHz */
			punkshot_readmem,punkshot_writemem,0,0,
			punkshot_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			punkshot_s_readmem,punkshot_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are generated by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	punkshot_vh_start,
	punkshot_vh_stop,
	punkshot_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface_nmi
		}
	}
};

static struct MachineDriver machine_driver_lgtnfght =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			lgtnfght_readmem,lgtnfght_writemem,0,0,
			lgtnfght_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			lgtnfght_s_readmem,lgtnfght_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	lgtnfght_vh_start,
	lgtnfght_vh_stop,
	lgtnfght_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface
		}
	}
};

static struct MachineDriver machine_driver_detatwin =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz */
			detatwin_readmem,detatwin_writemem,0,0,
			punkshot_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ????? */
			ssriders_s_readmem,ssriders_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are generated by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	detatwin_vh_start,
	detatwin_vh_stop,
	lgtnfght_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface_nmi
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_glfgreat =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* ? */
			glfgreat_readmem,glfgreat_writemem,0,0,
			lgtnfght_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? */
			glfgreat_s_readmem,glfgreat_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are generated by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	glfgreat_vh_start,
	glfgreat_vh_stop,
	glfgreat_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_K053260,
			&glfgreat_k053260_interface
		}
	}
};

static struct MachineDriver machine_driver_tmnt2 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz */
			tmnt2_readmem,tmnt2_writemem,0,0,
			punkshot_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2*3579545,	/* makes the ROM test sync */
			ssriders_s_readmem,ssriders_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are generated by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	lgtnfght_vh_start,
	lgtnfght_vh_stop,
	lgtnfght_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface_nmi
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_ssriders =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz */
			ssriders_readmem,ssriders_writemem,0,0,
			punkshot_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* ????? makes the ROM test sync */
			ssriders_s_readmem,ssriders_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are generated by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	lgtnfght_vh_start,
	lgtnfght_vh_stop,
	ssriders_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface_nmi
		}
	},

	nvram_handler
};

static struct MachineDriver machine_driver_thndrx2 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,	/* 12 MHz */
			thndrx2_readmem,thndrx2_writemem,0,0,
			punkshot_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ????? */
			thndrx2_s_readmem,thndrx2_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are generated by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	thndrx2_vh_start,
	thndrx2_vh_stop,
	thndrx2_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface_nmi
		}
	},

	thndrx2_nvram_handler
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mia )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "808t20.h17",   0x00000, 0x20000, 0x6f0acb1d )
	ROM_LOAD_ODD ( "808t21.j17",   0x00000, 0x20000, 0x42a30416 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "808e03.f4",    0x00000, 0x08000, 0x3d93a7cd )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_EVEN( "808e12.f28",   0x000000, 0x10000, 0xd62f1fde )        /* 8x8 tiles */
	ROM_LOAD_GFX_ODD ( "808e13.h28",   0x000000, 0x10000, 0x1fa708f4 )        /* 8x8 tiles */
	ROM_LOAD_GFX_EVEN( "808e22.i28",   0x020000, 0x10000, 0x73d758f6 )        /* 8x8 tiles */
	ROM_LOAD_GFX_ODD ( "808e23.k28",   0x020000, 0x10000, 0x8ff08b21 )        /* 8x8 tiles */

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "808d17.j4",    0x00000, 0x80000, 0xd1299082 )	/* sprites */
	ROM_LOAD( "808d15.h4",    0x80000, 0x80000, 0x2b22a6b6 )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "808a18.f16",   0x0000, 0x0100, 0xeb95aede )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "808d01.d4",    0x00000, 0x20000, 0xfd4d37c0 ) /* samples for 007232 */
ROM_END

ROM_START( mia2 )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "808s20.h17",   0x00000, 0x20000, 0xcaa2897f )
	ROM_LOAD_ODD ( "808s21.j17",   0x00000, 0x20000, 0x3d892ffb )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "808e03.f4",    0x00000, 0x08000, 0x3d93a7cd )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_EVEN( "808e12.f28",   0x000000, 0x10000, 0xd62f1fde )        /* 8x8 tiles */
	ROM_LOAD_GFX_ODD ( "808e13.h28",   0x000000, 0x10000, 0x1fa708f4 )        /* 8x8 tiles */
	ROM_LOAD_GFX_EVEN( "808e22.i28",   0x020000, 0x10000, 0x73d758f6 )        /* 8x8 tiles */
	ROM_LOAD_GFX_ODD ( "808e23.k28",   0x020000, 0x10000, 0x8ff08b21 )        /* 8x8 tiles */

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "808d17.j4",    0x00000, 0x80000, 0xd1299082 )	/* sprites */
	ROM_LOAD( "808d15.h4",    0x80000, 0x80000, 0x2b22a6b6 )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "808a18.f16",   0x0000, 0x0100, 0xeb95aede )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "808d01.d4",    0x00000, 0x20000, 0xfd4d37c0 ) /* samples for 007232 */
ROM_END

ROM_START( tmnt )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "963-r23",      0x00000, 0x20000, 0xa7f61195 )
	ROM_LOAD_ODD ( "963-r24",      0x00000, 0x20000, 0x661e056a )
	ROM_LOAD_EVEN( "963-r21",      0x40000, 0x10000, 0xde047bb6 )
	ROM_LOAD_ODD ( "963-r22",      0x40000, 0x10000, 0xd86a0888 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "963-e20",      0x00000, 0x08000, 0x1692a6d6 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a28",      0x000000, 0x80000, 0xdb4769a8 )        /* 8x8 tiles */
	ROM_LOAD( "963-a29",      0x080000, 0x80000, 0x8069cd2e )        /* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a17",      0x000000, 0x80000, 0xb5239a44 )        /* sprites */
	ROM_LOAD( "963-a18",      0x080000, 0x80000, 0xdd51adef )        /* sprites */
	ROM_LOAD( "963-a15",      0x100000, 0x80000, 0x1f324eed )        /* sprites */
	ROM_LOAD( "963-a16",      0x180000, 0x80000, 0xd4bd9984 )        /* sprites */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "tmnt.g7",      0x0000, 0x0100, 0xabd82680 )	/* sprite address decoder */
	ROM_LOAD( "tmnt.g19",     0x0100, 0x0100, 0xf8004a1c )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "963-a26",      0x00000, 0x20000, 0xe2ac3063 ) /* samples for 007232 */

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* 128k for the samples */
	ROM_LOAD( "963-a27",      0x00000, 0x20000, 0x2dfd674b ) /* samples for UPD7759C */

	ROM_REGION( 0x80000, REGION_SOUND3 )	/* 512k for the title music sample */
	ROM_LOAD( "963-a25",      0x00000, 0x80000, 0xfca078c7 )
ROM_END

ROM_START( tmht )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "963f23.j17",   0x00000, 0x20000, 0x9cb5e461 )
	ROM_LOAD_ODD ( "963f24.k17",   0x00000, 0x20000, 0x2d902fab )
	ROM_LOAD_EVEN( "963f21.j15",   0x40000, 0x10000, 0x9fa25378 )
	ROM_LOAD_ODD ( "963f22.k15",   0x40000, 0x10000, 0x2127ee53 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "963-e20",      0x00000, 0x08000, 0x1692a6d6 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a28",      0x000000, 0x80000, 0xdb4769a8 )        /* 8x8 tiles */
	ROM_LOAD( "963-a29",      0x080000, 0x80000, 0x8069cd2e )        /* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a17",      0x000000, 0x80000, 0xb5239a44 )        /* sprites */
	ROM_LOAD( "963-a18",      0x080000, 0x80000, 0xdd51adef )        /* sprites */
	ROM_LOAD( "963-a15",      0x100000, 0x80000, 0x1f324eed )        /* sprites */
	ROM_LOAD( "963-a16",      0x180000, 0x80000, 0xd4bd9984 )        /* sprites */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "tmnt.g7",      0x0000, 0x0100, 0xabd82680 )	/* sprite address decoder */
	ROM_LOAD( "tmnt.g19",     0x0100, 0x0100, 0xf8004a1c )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "963-a26",      0x00000, 0x20000, 0xe2ac3063 ) /* samples for 007232 */

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* 128k for the samples */
	ROM_LOAD( "963-a27",      0x00000, 0x20000, 0x2dfd674b ) /* samples for UPD7759C */

	ROM_REGION( 0x80000, REGION_SOUND3 )	/* 512k for the title music sample */
	ROM_LOAD( "963-a25",      0x00000, 0x80000, 0xfca078c7 )
ROM_END

ROM_START( tmntj )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "963-x23",      0x00000, 0x20000, 0xa9549004 )
	ROM_LOAD_ODD ( "963-x24",      0x00000, 0x20000, 0xe5cc9067 )
	ROM_LOAD_EVEN( "963-x21",      0x40000, 0x10000, 0x5789cf92 )
	ROM_LOAD_ODD ( "963-x22",      0x40000, 0x10000, 0x0a74e277 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "963-e20",      0x00000, 0x08000, 0x1692a6d6 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a28",      0x000000, 0x80000, 0xdb4769a8 )        /* 8x8 tiles */
	ROM_LOAD( "963-a29",      0x080000, 0x80000, 0x8069cd2e )        /* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a17",      0x000000, 0x80000, 0xb5239a44 )        /* sprites */
	ROM_LOAD( "963-a18",      0x080000, 0x80000, 0xdd51adef )        /* sprites */
	ROM_LOAD( "963-a15",      0x100000, 0x80000, 0x1f324eed )        /* sprites */
	ROM_LOAD( "963-a16",      0x180000, 0x80000, 0xd4bd9984 )        /* sprites */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "tmnt.g7",      0x0000, 0x0100, 0xabd82680 )	/* sprite address decoder */
	ROM_LOAD( "tmnt.g19",     0x0100, 0x0100, 0xf8004a1c )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "963-a26",      0x00000, 0x20000, 0xe2ac3063 ) /* samples for 007232 */

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* 128k for the samples */
	ROM_LOAD( "963-a27",      0x00000, 0x20000, 0x2dfd674b ) /* samples for UPD7759C */

	ROM_REGION( 0x80000, REGION_SOUND3 )	/* 512k for the title music sample */
	ROM_LOAD( "963-a25",      0x00000, 0x80000, 0xfca078c7 )
ROM_END

ROM_START( tmht2p )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "963-u23",      0x00000, 0x20000, 0x58bec748 )
	ROM_LOAD_ODD ( "963-u24",      0x00000, 0x20000, 0xdce87c8d )
	ROM_LOAD_EVEN( "963-u21",      0x40000, 0x10000, 0xabce5ead )
	ROM_LOAD_ODD ( "963-u22",      0x40000, 0x10000, 0x4ecc8d6b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "963-e20",      0x00000, 0x08000, 0x1692a6d6 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a28",      0x000000, 0x80000, 0xdb4769a8 )        /* 8x8 tiles */
	ROM_LOAD( "963-a29",      0x080000, 0x80000, 0x8069cd2e )        /* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a17",      0x000000, 0x80000, 0xb5239a44 )        /* sprites */
	ROM_LOAD( "963-a18",      0x080000, 0x80000, 0xdd51adef )        /* sprites */
	ROM_LOAD( "963-a15",      0x100000, 0x80000, 0x1f324eed )        /* sprites */
	ROM_LOAD( "963-a16",      0x180000, 0x80000, 0xd4bd9984 )        /* sprites */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "tmnt.g7",      0x0000, 0x0100, 0xabd82680 )	/* sprite address decoder */
	ROM_LOAD( "tmnt.g19",     0x0100, 0x0100, 0xf8004a1c )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "963-a26",      0x00000, 0x20000, 0xe2ac3063 ) /* samples for 007232 */

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* 128k for the samples */
	ROM_LOAD( "963-a27",      0x00000, 0x20000, 0x2dfd674b ) /* samples for UPD7759C */

	ROM_REGION( 0x80000, REGION_SOUND3 )	/* 512k for the title music sample */
	ROM_LOAD( "963-a25",      0x00000, 0x80000, 0xfca078c7 )
ROM_END

ROM_START( tmnt2pj )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "963-123",      0x00000, 0x20000, 0x6a3527c9 )
	ROM_LOAD_ODD ( "963-124",      0x00000, 0x20000, 0x2c4bfa15 )
	ROM_LOAD_EVEN( "963-121",      0x40000, 0x10000, 0x4181b733 )
	ROM_LOAD_ODD ( "963-122",      0x40000, 0x10000, 0xc64eb5ff )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "963-e20",      0x00000, 0x08000, 0x1692a6d6 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a28",      0x000000, 0x80000, 0xdb4769a8 )        /* 8x8 tiles */
	ROM_LOAD( "963-a29",      0x080000, 0x80000, 0x8069cd2e )        /* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a17",      0x000000, 0x80000, 0xb5239a44 )        /* sprites */
	ROM_LOAD( "963-a18",      0x080000, 0x80000, 0xdd51adef )        /* sprites */
	ROM_LOAD( "963-a15",      0x100000, 0x80000, 0x1f324eed )        /* sprites */
	ROM_LOAD( "963-a16",      0x180000, 0x80000, 0xd4bd9984 )        /* sprites */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "tmnt.g7",      0x0000, 0x0100, 0xabd82680 )	/* sprite address decoder */
	ROM_LOAD( "tmnt.g19",     0x0100, 0x0100, 0xf8004a1c )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "963-a26",      0x00000, 0x20000, 0xe2ac3063 ) /* samples for 007232 */

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* 128k for the samples */
	ROM_LOAD( "963-a27",      0x00000, 0x20000, 0x2dfd674b ) /* samples for UPD7759C */

	ROM_REGION( 0x80000, REGION_SOUND3 )	/* 512k for the title music sample */
	ROM_LOAD( "963-a25",      0x00000, 0x80000, 0xfca078c7 )
ROM_END

ROM_START( tmnt2po )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 2*128k and 2*64k for 68000 code */
	ROM_LOAD_EVEN( "tmnt123.bin",  0x00000, 0x20000, 0x2d905183 )
	ROM_LOAD_ODD ( "tmnt124.bin",  0x00000, 0x20000, 0xe0125352 )
	ROM_LOAD_EVEN( "tmnt21.bin",   0x40000, 0x10000, 0x12deeafb )
	ROM_LOAD_ODD ( "tmnt22.bin",   0x40000, 0x10000, 0xaec4f1c3 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "963-e20",      0x00000, 0x08000, 0x1692a6d6 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a28",      0x000000, 0x80000, 0xdb4769a8 )        /* 8x8 tiles */
	ROM_LOAD( "963-a29",      0x080000, 0x80000, 0x8069cd2e )        /* 8x8 tiles */

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "963-a17",      0x000000, 0x80000, 0xb5239a44 )        /* sprites */
	ROM_LOAD( "963-a18",      0x080000, 0x80000, 0xdd51adef )        /* sprites */
	ROM_LOAD( "963-a15",      0x100000, 0x80000, 0x1f324eed )        /* sprites */
	ROM_LOAD( "963-a16",      0x180000, 0x80000, 0xd4bd9984 )        /* sprites */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "tmnt.g7",      0x0000, 0x0100, 0xabd82680 )	/* sprite address decoder */
	ROM_LOAD( "tmnt.g19",     0x0100, 0x0100, 0xf8004a1c )	/* priority encoder (not used) */

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for the samples */
	ROM_LOAD( "963-a26",      0x00000, 0x20000, 0xe2ac3063 ) /* samples for 007232 */

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* 128k for the samples */
	ROM_LOAD( "963-a27",      0x00000, 0x20000, 0x2dfd674b ) /* samples for UPD7759C */

	ROM_REGION( 0x80000, REGION_SOUND3 )	/* 512k for the title music sample */
	ROM_LOAD( "963-a25",      0x00000, 0x80000, 0xfca078c7 )
ROM_END

ROM_START( punkshot )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "907-j02.i7",   0x00000, 0x20000, 0xdbb3a23b )
	ROM_LOAD_ODD ( "907-j03.i10",  0x00000, 0x20000, 0x2151d1ab )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "907f01.e8",    0x0000, 0x8000, 0xf040c484 )

	ROM_REGION( 0x80000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "907d06.e23",   0x000000, 0x40000, 0xf5cc38f4 )
	ROM_LOAD( "907d05.e22",   0x040000, 0x40000, 0xe25774c1 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "907d07l.k2",   0x000000, 0x80000, 0xfeeb345a )
	ROM_LOAD( "907d07h.k2",   0x080000, 0x80000, 0x0bff4383 )
	ROM_LOAD( "907d08l.k7",   0x100000, 0x80000, 0x05f3d196 )
	ROM_LOAD( "907d08h.k7",   0x180000, 0x80000, 0xeaf18c22 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for 053260 */
	ROM_LOAD( "907d04.d3",    0x0000, 0x80000, 0x090feb5e )
ROM_END

ROM_START( punksht2 )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "907m02.i7",    0x00000, 0x20000, 0x59e14575 )
	ROM_LOAD_ODD ( "907m03.i10",   0x00000, 0x20000, 0xadb14b1e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "907f01.e8",    0x0000, 0x8000, 0xf040c484 )

	ROM_REGION( 0x80000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "907d06.e23",   0x000000, 0x40000, 0xf5cc38f4 )
	ROM_LOAD( "907d05.e22",   0x040000, 0x40000, 0xe25774c1 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "907d07l.k2",   0x000000, 0x80000, 0xfeeb345a )
	ROM_LOAD( "907d07h.k2",   0x080000, 0x80000, 0x0bff4383 )
	ROM_LOAD( "907d08l.k7",   0x100000, 0x80000, 0x05f3d196 )
	ROM_LOAD( "907d08h.k7",   0x180000, 0x80000, 0xeaf18c22 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "907d04.d3",    0x0000, 0x80000, 0x090feb5e )
ROM_END

ROM_START( lgtnfght )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "939m02.e11",   0x00000, 0x20000, 0x61a12184 )
	ROM_LOAD_ODD ( "939m03.e15",   0x00000, 0x20000, 0x6db6659d )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "939e01.d7",    0x0000, 0x8000, 0x4a5fc848 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "939a07.k14",   0x000000, 0x80000, 0x7955dfcf )
	ROM_LOAD( "939a08.k19",   0x080000, 0x80000, 0xed95b385 )

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "939a06.k8",    0x000000, 0x80000, 0xe393c206 )
	ROM_LOAD( "939a05.k2",    0x080000, 0x80000, 0x3662d47a )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "939a04.c5",    0x0000, 0x80000, 0xc24e2b6e )
ROM_END

ROM_START( trigon )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "939j02.bin",   0x00000, 0x20000, 0x38381d1b )
	ROM_LOAD_ODD ( "939j03.bin",   0x00000, 0x20000, 0xb5beddcd )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "939e01.d7",    0x0000, 0x8000, 0x4a5fc848 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "939a07.k14",   0x000000, 0x80000, 0x7955dfcf )
	ROM_LOAD( "939a08.k19",   0x080000, 0x80000, 0xed95b385 )

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "939a06.k8",    0x000000, 0x80000, 0xe393c206 )
	ROM_LOAD( "939a05.k2",    0x080000, 0x80000, 0x3662d47a )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "939a04.c5",    0x0000, 0x80000, 0xc24e2b6e )
ROM_END

ROM_START( blswhstl )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "e09.bin",     0x000000, 0x20000, 0xe8b7b234 )
	ROM_LOAD_ODD ( "g09.bin",     0x000000, 0x20000, 0x3c26d281 )
	ROM_LOAD_EVEN( "e11.bin",     0x040000, 0x20000, 0x14628736 )
	ROM_LOAD_ODD ( "g11.bin",     0x040000, 0x20000, 0xf738ad4a )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "060_j01.rom",  0x0000, 0x10000, 0xf9d9a673 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_SWAP( "060_e07.r16",  0x000000, 0x080000, 0xc400edf3 )	/* tiles */
	ROM_LOAD_GFX_SWAP( "060_e08.r16",  0x080000, 0x080000, 0x70dddba1 )

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_SWAP( "060_e06.r16",  0x000000, 0x080000, 0x09381492 )	/* sprites */
	ROM_LOAD_GFX_SWAP( "060_e05.r16",  0x080000, 0x080000, 0x32454241 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "060_e04.r16",  0x0000, 0x100000, 0xc680395d )
ROM_END

ROM_START( detatwin )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "060_j02.rom", 0x000000, 0x20000, 0x11b761ac )
	ROM_LOAD_ODD ( "060_j03.rom", 0x000000, 0x20000, 0x8d0b588c )
	ROM_LOAD_EVEN( "060_j09.rom", 0x040000, 0x20000, 0xf2a5f15f )
	ROM_LOAD_ODD ( "060_j10.rom", 0x040000, 0x20000, 0x36eefdbc )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "060_j01.rom",  0x0000, 0x10000, 0xf9d9a673 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_SWAP( "060_e07.r16",  0x000000, 0x080000, 0xc400edf3 )	/* tiles */
	ROM_LOAD_GFX_SWAP( "060_e08.r16",  0x080000, 0x080000, 0x70dddba1 )

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_SWAP( "060_e06.r16",  0x000000, 0x080000, 0x09381492 )	/* sprites */
	ROM_LOAD_GFX_SWAP( "060_e05.r16",  0x080000, 0x080000, 0x32454241 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "060_e04.r16",  0x0000, 0x100000, 0xc680395d )
ROM_END

ROM_START( glfgreat )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD_EVEN( "061l02.1h",   0x000000, 0x20000, 0xac7399f4 )
	ROM_LOAD_ODD ( "061l03.4h",   0x000000, 0x20000, 0x77b0ff5c )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "061f01.4e",    0x0000, 0x8000, 0xab9a2a57 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "061d14.12l",   0x000000, 0x080000, 0xb9440924 )	/* tiles */
	ROM_LOAD( "061d13.12k",   0x080000, 0x080000, 0x9f999f0b )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "061d11.3k",    0x000000, 0x100000, 0xc45b66a3 )	/* sprites */
	ROM_LOAD( "061d12.8k",    0x100000, 0x100000, 0xd305ecd1 )

	ROM_REGION( 0x300000, REGION_GFX3 )	/* unknown (data for the 053936?) */
	ROM_LOAD( "061b05.15d",   0x000000, 0x020000, 0x2456fb11 )	/* gfx */
	ROM_LOAD( "061b06.16d",   0x080000, 0x080000, 0x41ada2ad )
	ROM_LOAD( "061b07.18d",   0x100000, 0x080000, 0x517887e2 )
	ROM_LOAD( "061b08.14g",   0x180000, 0x080000, 0x6ab739c3 )
	ROM_LOAD( "061b09.15g",   0x200000, 0x080000, 0x42c7a603 )
	ROM_LOAD( "061b10.17g",   0x280000, 0x080000, 0x10f89ce7 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "061e04.1d",    0x0000, 0x100000, 0x7921d8df )
ROM_END

ROM_START( glfgretj )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD_EVEN( "061j02.1h",   0x000000, 0x20000, 0x7f0d95f4 )
	ROM_LOAD_ODD ( "061j03.4h",   0x000000, 0x20000, 0x06caa38b )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "061f01.4e",    0x0000, 0x8000, 0xab9a2a57 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "061d14.12l",   0x000000, 0x080000, 0xb9440924 )	/* tiles */
	ROM_LOAD( "061d13.12k",   0x080000, 0x080000, 0x9f999f0b )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "061d11.3k",    0x000000, 0x100000, 0xc45b66a3 )	/* sprites */
	ROM_LOAD( "061d12.8k",    0x100000, 0x100000, 0xd305ecd1 )

	ROM_REGION( 0x300000, REGION_GFX3 )	/* unknown (data for the 053936?) */
	ROM_LOAD( "061b05.15d",   0x000000, 0x020000, 0x2456fb11 )	/* gfx */
	ROM_LOAD( "061b06.16d",   0x080000, 0x080000, 0x41ada2ad )
	ROM_LOAD( "061b07.18d",   0x100000, 0x080000, 0x517887e2 )
	ROM_LOAD( "061b08.14g",   0x180000, 0x080000, 0x6ab739c3 )
	ROM_LOAD( "061b09.15g",   0x200000, 0x080000, 0x42c7a603 )
	ROM_LOAD( "061b10.17g",   0x280000, 0x080000, 0x10f89ce7 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "061e04.1d",    0x0000, 0x100000, 0x7921d8df )
ROM_END

ROM_START( tmnt2 )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "uaa02", 0x000000, 0x20000, 0x58d5c93d )
	ROM_LOAD_ODD ( "uaa03", 0x000000, 0x20000, 0x0541fec9 )
	ROM_LOAD_EVEN( "uaa04", 0x040000, 0x20000, 0x1d441a7d )
	ROM_LOAD_ODD ( "uaa05", 0x040000, 0x20000, 0x9c428273 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "b01",          0x0000, 0x10000, 0x364f548a )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "b12",          0x000000, 0x080000, 0xd3283d19 )	/* tiles */
	ROM_LOAD( "b11",          0x080000, 0x080000, 0x6ebc0c15 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "b09",          0x000000, 0x100000, 0x2d7a9d2a )	/* sprites */
	ROM_LOAD( "b10",          0x100000, 0x080000, 0xf2dd296e )
	/* second half empty */
	ROM_LOAD( "b07",          0x200000, 0x100000, 0xd9bee7bf )
	ROM_LOAD( "b08",          0x300000, 0x080000, 0x3b1ae36f )
	/* second half empty */

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "063b06",       0x0000, 0x200000, 0x1e510aa5 )
ROM_END

ROM_START( tmnt22p )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "a02",   0x000000, 0x20000, 0xaadffe3a )
	ROM_LOAD_ODD ( "a03",   0x000000, 0x20000, 0x125687a8 )
	ROM_LOAD_EVEN( "a04",   0x040000, 0x20000, 0xfb5c7ded )
	ROM_LOAD_ODD ( "a05",   0x040000, 0x20000, 0x3c40fe66 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "b01",          0x0000, 0x10000, 0x364f548a )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "b12",          0x000000, 0x080000, 0xd3283d19 )	/* tiles */
	ROM_LOAD( "b11",          0x080000, 0x080000, 0x6ebc0c15 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "b09",          0x000000, 0x100000, 0x2d7a9d2a )	/* sprites */
	ROM_LOAD( "b10",          0x100000, 0x080000, 0xf2dd296e )
	/* second half empty */
	ROM_LOAD( "b07",          0x200000, 0x100000, 0xd9bee7bf )
	ROM_LOAD( "b08",          0x300000, 0x080000, 0x3b1ae36f )
	/* second half empty */

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "063b06",       0x0000, 0x200000, 0x1e510aa5 )
ROM_END

ROM_START( tmnt2a )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "ada02", 0x000000, 0x20000, 0x4f11b587 )
	ROM_LOAD_ODD ( "ada03", 0x000000, 0x20000, 0x82a1b9ac )
	ROM_LOAD_EVEN( "ada04", 0x040000, 0x20000, 0x05ad187a )
	ROM_LOAD_ODD ( "ada05", 0x040000, 0x20000, 0xd4826547 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "b01",          0x0000, 0x10000, 0x364f548a )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "b12",          0x000000, 0x080000, 0xd3283d19 )	/* tiles */
	ROM_LOAD( "b11",          0x080000, 0x080000, 0x6ebc0c15 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "b09",          0x000000, 0x100000, 0x2d7a9d2a )	/* sprites */
	ROM_LOAD( "b10",          0x100000, 0x080000, 0xf2dd296e )
	/* second half empty */
	ROM_LOAD( "b07",          0x200000, 0x100000, 0xd9bee7bf )
	ROM_LOAD( "b08",          0x300000, 0x080000, 0x3b1ae36f )
	/* second half empty */

	ROM_REGION( 0x200000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "063b06",       0x0000, 0x200000, 0x1e510aa5 )
ROM_END

ROM_START( ssriders )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "064eac02",    0x000000, 0x40000, 0x5a5425f4 )
	ROM_LOAD_ODD ( "064eac03",    0x000000, 0x40000, 0x093c00fb )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdrebd )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "064ebd02",    0x000000, 0x40000, 0x8deef9ac )
	ROM_LOAD_ODD ( "064ebd03",    0x000000, 0x40000, 0x2370c107 )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdrebc )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "sr_c02.rom",  0x000000, 0x40000, 0x9bd7d164 )
	ROM_LOAD_ODD ( "sr_c03.rom",  0x000000, 0x40000, 0x40fd4165 )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdruda )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "064uda02",    0x000000, 0x40000, 0x5129a6b7 )
	ROM_LOAD_ODD ( "064uda03",    0x000000, 0x40000, 0x9f887214 )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdruac )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "064uac02",    0x000000, 0x40000, 0x870473b6 )
	ROM_LOAD_ODD ( "064uac03",    0x000000, 0x40000, 0xeadf289a )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdrubc )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "2pl.8e",      0x000000, 0x40000, 0xaca7fda5 )
	ROM_LOAD_ODD ( "2pl.8g",      0x000000, 0x40000, 0xbb1fdeff )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdrabd )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "064abd02.8e", 0x000000, 0x40000, 0x713406cb )
	ROM_LOAD_ODD ( "064abd03.8g", 0x000000, 0x40000, 0x680feb3c )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( ssrdrjbd )
	ROM_REGION( 0xc0000, REGION_CPU1 )
	ROM_LOAD_EVEN( "064jbd02.8e", 0x000000, 0x40000, 0x7acdc1e3 )
	ROM_LOAD_ODD ( "064jbd03.8g", 0x000000, 0x40000, 0x6a424918 )
	ROM_LOAD_EVEN( "sr_b04.rom",  0x080000, 0x20000, 0xef2315bd )
	ROM_LOAD_ODD ( "sr_b05.rom",  0x080000, 0x20000, 0x51d6fbc4 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "sr_e01.rom",   0x0000, 0x10000, 0x44b9bc52 )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_16k.rom",   0x000000, 0x080000, 0xe2bdc619 )	/* tiles */
	ROM_LOAD( "sr_12k.rom",   0x080000, 0x080000, 0x2d8ca8b0 )

	ROM_REGION( 0x200000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "sr_7l.rom",    0x000000, 0x100000, 0x4160c372 )	/* sprites */
	ROM_LOAD( "sr_3l.rom",    0x100000, 0x100000, 0x64dd673c )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "sr_1d.rom",    0x0000, 0x100000, 0x59810df9 )
ROM_END

ROM_START( thndrx2 )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD_EVEN( "073-k02.11c", 0x000000, 0x20000, 0x0c8b2d3f )
	ROM_LOAD_ODD ( "073-k03.12c", 0x000000, 0x20000, 0x3803b427 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "073-c01.4f",   0x0000, 0x10000, 0x44ebe83c )

    ROM_REGION( 0x100000, REGION_GFX1 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "073-c06.16k",  0x000000, 0x080000, 0x24e22b42 )	/* tiles */
	ROM_LOAD( "073-c05.12k",  0x080000, 0x080000, 0x952a935f )

	ROM_REGION( 0x100000, REGION_GFX2 )	/* graphics (addressable by the main CPU) */
	ROM_LOAD( "073-c07.7k",   0x000000, 0x080000, 0x14e93f38 )	/* sprites */
	ROM_LOAD( "073-c08.3k",   0x080000, 0x080000, 0x09fab3ab )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for the 053260 */
	ROM_LOAD( "073-b04.2d",   0x0000, 0x80000, BADCRC( 0x7f7f2fd3 ) )
ROM_END



static void init_gfx(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}

static void init_mia(void)
{
	unsigned char *gfxdata;
	int len;
	int i,j,k,A,B;
	int bits[32];
	unsigned char *temp;


	init_gfx();

	/*
		along with the normal byte reordering, TMNT also needs the bits to
		be shuffled around because the ROMs are connected differently to the
		051962 custom IC.
	*/
	gfxdata = memory_region(REGION_GFX1);
	len = memory_region_length(REGION_GFX1);
	for (i = 0;i < len;i += 4)
	{
		for (j = 0;j < 4;j++)
			for (k = 0;k < 8;k++)
				bits[8*j + k] = (gfxdata[i + j] >> k) & 1;

		for (j = 0;j < 4;j++)
		{
			gfxdata[i + j] = 0;
			for (k = 0;k < 8;k++)
				gfxdata[i + j] |= bits[j + 4*k] << k;
		}
	}

	/*
		along with the normal byte reordering, MIA also needs the bits to
		be shuffled around because the ROMs are connected differently to the
		051937 custom IC.
	*/
	gfxdata = memory_region(REGION_GFX2);
	len = memory_region_length(REGION_GFX2);
	for (i = 0;i < len;i += 4)
	{
		for (j = 0;j < 4;j++)
			for (k = 0;k < 8;k++)
				bits[8*j + k] = (gfxdata[i + j] >> k) & 1;

		for (j = 0;j < 4;j++)
		{
			gfxdata[i + j] = 0;
			for (k = 0;k < 8;k++)
				gfxdata[i + j] |= bits[j + 4*k] << k;
		}
	}

	temp = (unsigned char*)malloc(len);
	if (!temp) return;	/* bad thing! */
	memcpy(temp,gfxdata,len);
	for (A = 0;A < len/4;A++)
	{
		/* the bits to scramble are the low 8 ones */
		for (i = 0;i < 8;i++)
			bits[i] = (A >> i) & 0x01;

		B = A & 0x3ff00;

		if ((A & 0x3c000) == 0x3c000)
		{
			B |= bits[3] << 0;
			B |= bits[5] << 1;
			B |= bits[0] << 2;
			B |= bits[1] << 3;
			B |= bits[2] << 4;
			B |= bits[4] << 5;
			B |= bits[6] << 6;
			B |= bits[7] << 7;
		}
		else
		{
			B |= bits[3] << 0;
			B |= bits[5] << 1;
			B |= bits[7] << 2;
			B |= bits[0] << 3;
			B |= bits[1] << 4;
			B |= bits[2] << 5;
			B |= bits[4] << 6;
			B |= bits[6] << 7;
		}

		gfxdata[4*A+0] = temp[4*B+0];
		gfxdata[4*A+1] = temp[4*B+1];
		gfxdata[4*A+2] = temp[4*B+2];
		gfxdata[4*A+3] = temp[4*B+3];
	}
	free(temp);
}


static void init_tmnt(void)
{
	unsigned char *gfxdata;
	int len;
	int i,j,k,A,B,entry;
	int bits[32];
	unsigned char *temp;


	init_gfx();

	/*
		along with the normal byte reordering, TMNT also needs the bits to
		be shuffled around because the ROMs are connected differently to the
		051962 custom IC.
	*/
	gfxdata = memory_region(REGION_GFX1);
	len = memory_region_length(REGION_GFX1);
	for (i = 0;i < len;i += 4)
	{
		for (j = 0;j < 4;j++)
			for (k = 0;k < 8;k++)
				bits[8*j + k] = (gfxdata[i + j] >> k) & 1;

		for (j = 0;j < 4;j++)
		{
			gfxdata[i + j] = 0;
			for (k = 0;k < 8;k++)
				gfxdata[i + j] |= bits[j + 4*k] << k;
		}
	}

	/*
		along with the normal byte reordering, TMNT also needs the bits to
		be shuffled around because the ROMs are connected differently to the
		051937 custom IC.
	*/
	gfxdata = memory_region(REGION_GFX2);
	len = memory_region_length(REGION_GFX2);
	for (i = 0;i < len;i += 4)
	{
		for (j = 0;j < 4;j++)
			for (k = 0;k < 8;k++)
				bits[8*j + k] = (gfxdata[i + j] >> k) & 1;

		for (j = 0;j < 4;j++)
		{
			gfxdata[i + j] = 0;
			for (k = 0;k < 8;k++)
				gfxdata[i + j] |= bits[j + 4*k] << k;
		}
	}

	temp = (unsigned char*)malloc(len);
	if (!temp) return;	/* bad thing! */
	memcpy(temp,gfxdata,len);
	for (A = 0;A < len/4;A++)
	{
		unsigned char *code_conv_table = &memory_region(REGION_PROMS)[0x0000];
#define CA0 0
#define CA1 1
#define CA2 2
#define CA3 3
#define CA4 4
#define CA5 5
#define CA6 6
#define CA7 7
#define CA8 8
#define CA9 9

		/* following table derived from the schematics. It indicates, for each of the */
		/* 9 low bits of the sprite line address, which bit to pick it from. */
		/* For example, when the PROM contains 4, which applies to 4x2 sprites, */
		/* bit OA1 comes from CA5, OA2 from CA0, and so on. */
		static unsigned char bit_pick_table[10][8] =
		{
			/*0(1x1) 1(2x1) 2(1x2) 3(2x2) 4(4x2) 5(2x4) 6(4x4) 7(8x8) */
			{ CA3,   CA3,   CA3,   CA3,   CA3,   CA3,   CA3,   CA3 },	/* CA3 */
			{ CA0,   CA0,   CA5,   CA5,   CA5,   CA5,   CA5,   CA5 },	/* OA1 */
			{ CA1,   CA1,   CA0,   CA0,   CA0,   CA7,   CA7,   CA7 },	/* OA2 */
			{ CA2,   CA2,   CA1,   CA1,   CA1,   CA0,   CA0,   CA9 },	/* OA3 */
			{ CA4,   CA4,   CA2,   CA2,   CA2,   CA1,   CA1,   CA0 },	/* OA4 */
			{ CA5,   CA6,   CA4,   CA4,   CA4,   CA2,   CA2,   CA1 },	/* OA5 */
			{ CA6,   CA5,   CA6,   CA6,   CA6,   CA4,   CA4,   CA2 },	/* OA6 */
			{ CA7,   CA7,   CA7,   CA7,   CA8,   CA6,   CA6,   CA4 },	/* OA7 */
			{ CA8,   CA8,   CA8,   CA8,   CA7,   CA8,   CA8,   CA6 },	/* OA8 */
			{ CA9,   CA9,   CA9,   CA9,   CA9,   CA9,   CA9,   CA8 }	/* OA9 */
		};

		/* pick the correct entry in the PROM (top 8 bits of the address) */
		entry = code_conv_table[(A & 0x7f800) >> 11] & 7;

		/* the bits to scramble are the low 10 ones */
		for (i = 0;i < 10;i++)
			bits[i] = (A >> i) & 0x01;

		B = A & 0x7fc00;

		for (i = 0;i < 10;i++)
			B |= bits[bit_pick_table[i][entry]] << i;

		gfxdata[4*A+0] = temp[4*B+0];
		gfxdata[4*A+1] = temp[4*B+1];
		gfxdata[4*A+2] = temp[4*B+2];
		gfxdata[4*A+3] = temp[4*B+3];
	}
	free(temp);
}

static void shuffle(UINT8 *buf,int len)
{
	int i;
	UINT8 t;

	if (len == 2) return;

	if (len % 4) exit(1);	/* must not happen */

	len /= 2;

	for (i = 0;i < len/2;i++)
	{
		t = buf[len/2 + i];
		buf[len/2 + i] = buf[len + i];
		buf[len + i] = t;
	}

	shuffle(buf,len);
	shuffle(buf + len,len);
}

static void init_glfgreat(void)
{
	/* ROMs are interleaved at byte level */
	shuffle(memory_region(REGION_GFX1),memory_region_length(REGION_GFX1));
	shuffle(memory_region(REGION_GFX2),memory_region_length(REGION_GFX2));
}



GAME( 1989, mia,      0,        mia,      mia,      mia,      ROT0,  "Konami", "Missing in Action (version T)" )
GAME( 1989, mia2,     mia,      mia,      mia,      mia,      ROT0,  "Konami", "Missing in Action (version S)" )

GAME( 1989, tmnt,     0,        tmnt,     tmnt,     tmnt,     ROT0,  "Konami", "Teenage Mutant Ninja Turtles (4 Players US)" )
GAME( 1989, tmht,     tmnt,     tmnt,     tmnt,     tmnt,     ROT0,  "Konami", "Teenage Mutant Hero Turtles (4 Players UK)" )
GAME( 1989, tmntj,    tmnt,     tmnt,     tmnt,     tmnt,     ROT0,  "Konami", "Teenage Mutant Ninja Turtles (4 Players Japan)" )
GAME( 1989, tmht2p,   tmnt,     tmnt,     tmnt2p,   tmnt,     ROT0,  "Konami", "Teenage Mutant Hero Turtles (2 Players UK)" )
GAME( 1990, tmnt2pj,  tmnt,     tmnt,     tmnt2p,   tmnt,     ROT0,  "Konami", "Teenage Mutant Ninja Turtles (2 Players Japan)" )
GAME( 1989, tmnt2po,  tmnt,     tmnt,     tmnt2p,   tmnt,     ROT0,  "Konami", "Teenage Mutant Ninja Turtles (2 Players Oceania)" )

GAME( 1990, punkshot, 0,        punkshot, punkshot, gfx,      ROT0,  "Konami", "Punk Shot (4 Players)" )
GAME( 1990, punksht2, punkshot, punkshot, punksht2, gfx,      ROT0,  "Konami", "Punk Shot (2 Players)" )

GAME( 1990, lgtnfght, 0,        lgtnfght, lgtnfght, gfx,      ROT90, "Konami", "Lightning Fighters (US)" )
GAME( 1990, trigon,   lgtnfght, lgtnfght, lgtnfght, gfx,      ROT90, "Konami", "Trigon (Japan)" )

GAME( 1991, blswhstl, 0,        detatwin, detatwin, gfx,      ROT90, "Konami", "Bells & Whistles" )
GAME( 1991, detatwin, blswhstl, detatwin, detatwin, gfx,      ROT90, "Konami", "Detana!! Twin Bee (Japan)" )

GAMEX(1991, glfgreat, 0,        glfgreat, glfgreat, glfgreat, ROT0,  "Konami", "Golfing Greats", GAME_NOT_WORKING )
GAMEX(1991, glfgretj, glfgreat, glfgreat, glfgreat, glfgreat, ROT0,  "Konami", "Golfing Greats (Japan)", GAME_NOT_WORKING )

GAMEX(1991, tmnt2,    0,        tmnt2,    ssridr4p, gfx,      ROT0,  "Konami", "Teenage Mutant Ninja Turtles - Turtles in Time (4 Players US)", GAME_IMPERFECT_COLORS )
GAMEX(1991, tmnt22p,  tmnt2,    tmnt2,    ssriders, gfx,      ROT0,  "Konami", "Teenage Mutant Ninja Turtles - Turtles in Time (2 Players US)", GAME_IMPERFECT_COLORS )
GAMEX(1991, tmnt2a,   tmnt2,    tmnt2,    tmnt2a,   gfx,      ROT0,  "Konami", "Teenage Mutant Ninja Turtles - Turtles in Time (4 Players Asia)", GAME_IMPERFECT_COLORS )

GAME( 1991, ssriders, 0,        ssriders, ssridr4p, gfx,      ROT0,  "Konami", "Sunset Riders (World 4 Players ver. EAC)" )
GAME( 1991, ssrdrebd, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (World 2 Players ver. EBD)" )
GAME( 1991, ssrdrebc, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (World 2 Players ver. EBC)" )
GAME( 1991, ssrdruda, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (US 4 Players ver. UDA)" )
GAME( 1991, ssrdruac, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (US 4 Players ver. UAC)" )
GAME( 1991, ssrdrubc, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (US 2 Players ver. UBC)" )
GAME( 1991, ssrdrabd, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (Asia 2 Players ver. ABD)" )
GAME( 1991, ssrdrjbd, ssriders, ssriders, ssriders, gfx,      ROT0,  "Konami", "Sunset Riders (Japan 2 Players ver. JBD)" )

GAME( 1991, thndrx2,  0,        thndrx2,  thndrx2,  gfx,      ROT0,  "Konami", "Thunder Cross II (Japan)" )
