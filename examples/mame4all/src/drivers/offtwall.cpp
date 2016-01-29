#include "../vidhrdw/offtwall.cpp"

/***************************************************************************

	Off the Wall

    driver by Aaron Giles

****************************************************************************/


#include "driver.h"
#include "sound/fm.h"
#include "machine/atarigen.h"
#include "sndhrdw/atarijsa.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( offtwall_playfieldram_w );
WRITE_HANDLER( offtwall_video_control_w );

int offtwall_vh_start(void);
void offtwall_vh_stop(void);
void offtwall_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

void offtwall_scanline_update(int scanline);



/*************************************
 *
 *	Interrupt handling
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_scanline_int_state)
		newstate = 4;
	if (atarigen_sound_int_state)
		newstate = 6;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_video_control_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(offtwall_scanline_update, 8);
	atarijsa_reset();
}



/*************************************
 *
 *	I/O handling
 *
 *************************************/

static READ_HANDLER( special_port3_r )
{
	int result = input_port_3_r(offset);
	if (atarigen_cpu_to_sound_ready) result ^= 0x0020;
	return result;
}


static WRITE_HANDLER( io_latch_w )
{
	/* lower byte */
	if (!(data & 0x00ff0000))
	{
		/* bit 4 resets the sound CPU */
		cpu_set_reset_line(1, (data & 0x10) ? CLEAR_LINE : ASSERT_LINE);
		if (!(data & 0x10)) atarijsa_reset();
	}

	//logerror("sound control = %04X\n", data);
}



/*************************************
 *
 *	Son-of-slapstic workarounds
 *
 *************************************/


/*-------------------------------------------------------------------------

	Bankswitching

	Like the slapstic, the SoS bankswitches memory using A13 and A14.
	Unlike the slapstic, the exact addresses to trigger the bankswitch
	are unknown.

	Fortunately, Off the Wall uses a common routine for the important
	bankswitching. The playfield data is stored in the banked area of
	ROM, and by comparing the playfields to a real system, a mechanism
	to bankswitch at the appropriate time was discovered. Fortunately,
	it's really basic.

	OtW looks up the address to read playfield data from a table which
	is 58 words long. Word 0 assumes the bank is 0, word 1 assumes the
	bank is 1, etc. So we just trigger off of the table read and cause
	the bank to switch then.

	In addition, there is code which checksums longs from $40000 down to
	$3e000. The code wants that checksum to be $aaaa5555, but there is
	no obvious way for this to happen. To work around this, we watch for
	the final read from $3e000 and tweak the value such that the checksum
	will come out the $aaaa5555 magically.

-------------------------------------------------------------------------*/

static UINT8 *bankswitch_base;
static UINT8 *bankrom_base;
static UINT32 bank_offset;

static READ_HANDLER( bankswitch_r )
{
	/* this is the table lookup; the bank is determined by the address that was requested */
	bank_offset = ((offset / 2) & 3) * 0x2000;
	//logerror("Bankswitch index %d -> %04X\n", offset, bank_offset);

	return READ_WORD(&bankswitch_base[offset]);
}

static READ_HANDLER( bankrom_r )
{
	/* this is the banked ROM read */
	//logerror("%06X: %04X\n", cpu_getpreviouspc(), offset);

	/* if the values are $3e000 or $3e002 are being read by code just below the
		ROM bank area, we need to return the correct value to give the proper checksum */
	if ((offset == 0x6000 || offset == 0x6002) && cpu_getpreviouspc() > 0x37000)
	{
		unsigned int checksum = cpu_readmem24bew_dword(0x3fd210);
		unsigned int us = 0xaaaa5555 - checksum;
		if (offset == 0x6002)
			return us & 0xffff;
		else
			return us >> 16;
	}

	return READ_WORD(&bankrom_base[(bank_offset + offset) & 0x7fff]);
}


/*-------------------------------------------------------------------------

	Sprite Cache

	Somewhere in the code, if all the hardware tests are met properly,
	some additional dummy sprites are added to the sprite cache before
	they are copied to sprite RAM. The sprite RAM copy routine computes
	the total width of all sprites as they are copied and if the total
	width is less than or equal to 38, it adds a "HARDWARE ERROR" sprite
	to the end.

	Here we detect the read of the sprite count from within the copy
	routine, and add some dummy sprites to the cache ourself if there
	isn't enough total width.

-------------------------------------------------------------------------*/

static UINT8 *spritecache_count;

static READ_HANDLER( spritecache_count_r )
{
	int prevpc = cpu_getpreviouspc();

	/* if this read is coming from $99f8 or $9992, it's in the sprite copy loop */
	if (prevpc == 0x99f8 || prevpc == 0x9992)
	{
		UINT16 *data = (UINT16 *)&spritecache_count[-0x200];
		int oldword = READ_WORD(&spritecache_count[0]);
		int count = oldword >> 8;
		int i, width = 0;

		/* compute the current total width */
		for (i = 0; i < count; i++)
			width += 1 + ((data[i * 4 + 1] >> 4) & 7);

		/* if we're less than 39, keep adding dummy sprites until we hit it */
		if (width <= 38)
		{
			while (width <= 38)
			{
				data[count * 4 + 0] = (42 * 8) << 7;
				data[count * 4 + 1] = ((30 * 8) << 7) | (7 << 4);
				data[count * 4 + 2] = 0;
				width += 8;
				count++;
			}

			/* update the final count in memory */
			WRITE_WORD(&spritecache_count[0], (count << 8) | (oldword & 0xff));
		}
	}

	/* and then read the data */
	return READ_WORD(&spritecache_count[offset]);
}


/*-------------------------------------------------------------------------

	Unknown Verify

	In several places, the value 1 is stored to the byte at $3fdf1e. A
	fairly complex subroutine is called, and then $3fdf1e is checked to
	see if it was set to zero. If it was, "HARDWARE ERROR" is displayed.

	To avoid this, we just return 1 when this value is read within the
	range of PCs where it is tested.

-------------------------------------------------------------------------*/

static UINT8 *unknown_verify_base;

static READ_HANDLER( unknown_verify_r )
{
	int prevpc = cpu_getpreviouspc();
	if (prevpc < 0x5c5e || prevpc > 0xc432)
		return READ_WORD(&unknown_verify_base[offset]);
	else
		return READ_WORD(&unknown_verify_base[offset]) | 0x100;
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x037fff, MRA_ROM },
	{ 0x038000, 0x03ffff, bankrom_r },
	{ 0x120000, 0x120fff, atarigen_eeprom_r },
	{ 0x260000, 0x260001, input_port_0_r },
	{ 0x260002, 0x260003, input_port_1_r },
	{ 0x260010, 0x260011, special_port3_r },
	{ 0x260012, 0x260013, input_port_4_r },
	{ 0x260020, 0x260021, input_port_5_r },
	{ 0x260022, 0x260023, input_port_6_r },
	{ 0x260024, 0x260025, input_port_7_r },
	{ 0x260030, 0x260031, atarigen_sound_r },
	{ 0x3e0000, 0x3e0fff, MRA_BANK1 },
	{ 0x3effc0, 0x3effff, atarigen_video_control_r },
	{ 0x3f4000, 0x3f7fff, MRA_BANK3 },
	{ 0x3f8000, 0x3fcfff, MRA_BANK4 },
	{ 0x3fd000, 0x3fd3ff, MRA_BANK5 },
	{ 0x3fd400, 0x3fffff, MRA_BANK6 },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x037fff, MWA_ROM },
	{ 0x038000, 0x03ffff, MWA_ROM, &bankrom_base },
	{ 0x120000, 0x120fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0x260040, 0x260041, atarigen_sound_w },
	{ 0x260050, 0x260051, io_latch_w },
	{ 0x260060, 0x260061, atarigen_eeprom_enable_w },
	{ 0x2a0000, 0x2a0001, watchdog_reset_w },
	{ 0x3e0000, 0x3e0fff, atarigen_666_paletteram_w, &paletteram },
	{ 0x3effc0, 0x3effff, atarigen_video_control_w, &atarigen_video_control_data },
	{ 0x3f4000, 0x3f7fff, offtwall_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0x3f8000, 0x3fcfff, MWA_BANK4 },
	{ 0x3fd000, 0x3fd3ff, MWA_BANK5, &atarigen_spriteram },
	{ 0x3fd400, 0x3fffff, MWA_BANK6 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( offtwall )
	PORT_START	/* 260000 */
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )

	PORT_START	/* 260002 */
	PORT_BIT(  0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER3 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER3 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER3 )

	JSA_III_PORT	/* audio board port */

	PORT_START	/* 260010 */
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0002, 0x0000, "Controls" )
	PORT_DIPSETTING(      0x0000, "Whirly-gigs" )	/* this is official Atari terminology! */
	PORT_DIPSETTING(      0x0002, "Joysticks" )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNUSED )	/* tested at a454 */
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNUSED )	/* tested at a466 */
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNUSED )	/* tested before writing to 260040 */
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT(  0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 260012 */
	PORT_BIT(  0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 260020 */
    PORT_ANALOG( 0xff, 0, IPT_DIAL_V | IPF_PLAYER1, 50, 10, 0, 0 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 260022 */
    PORT_ANALOG( 0xff, 0, IPT_DIAL | IPF_PLAYER2, 50, 10, 0, 0 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 260024 */
    PORT_ANALOG( 0xff, 0, IPT_DIAL_V | IPF_PLAYER3 | IPF_REVERSE, 50, 10, 0, 0 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout pfmolayout =
{
	8,8,	/* 8*8 sprites */
	24576,	/* 8192 of them */
	4,		/* 4 bits per pixel */
	{ 0+0x60000*8, 4+0x60000*8, 0, 4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pfmolayout,  256, 32 },		/* sprites & playfield */
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_offtwall =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			ATARI_CLOCK_14MHz/2,
			readmem,writemem,0,0,
			ignore_interrupt,1
		},
		JSA_III_CPU
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_SUPPORTS_DIRTY,
	0,
	offtwall_vh_start,
	offtwall_vh_stop,
	offtwall_vh_screenrefresh,

	/* sound hardware */
	JSA_III_MONO_NO_SPEECH,

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM decoding
 *
 *************************************/

static void rom_decode(void)
{
	int i;

	for (i = 0; i < memory_region_length(REGION_GFX1); i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;
}



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( offtwall )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "otw2012.bin", 0x00000, 0x20000, 0xd08d81eb )
	ROM_LOAD_ODD ( "otw2013.bin", 0x00000, 0x20000, 0x61c2553d )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "otw1020.bin", 0x10000, 0x4000, 0x488112a5 )
	ROM_CONTINUE(            0x04000, 0xc000 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "otw1014.bin", 0x000000, 0x20000, 0x4d64507e )
	ROM_LOAD( "otw1016.bin", 0x020000, 0x20000, 0xf5454f3a )
	ROM_LOAD( "otw1018.bin", 0x040000, 0x20000, 0x17864231 )
	ROM_LOAD( "otw1015.bin", 0x060000, 0x20000, 0x271f7856 )
	ROM_LOAD( "otw1017.bin", 0x080000, 0x20000, 0x7f7f8012 )
	ROM_LOAD( "otw1019.bin", 0x0a0000, 0x20000, 0x9efe511b )
ROM_END


ROM_START( offtwalc )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "090-2612.rom", 0x00000, 0x20000, 0xfc891a3f )
	ROM_LOAD_ODD ( "090-2613.rom", 0x00000, 0x20000, 0x805d79d4 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "otw1020.bin", 0x10000, 0x4000, 0x488112a5 )
	ROM_CONTINUE(            0x04000, 0xc000 )

	ROM_REGION( 0xc0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "090-1614.rom", 0x000000, 0x20000, 0x307ed447 )
	ROM_LOAD( "090-1616.rom", 0x020000, 0x20000, 0xa5bd3d9b )
	ROM_LOAD( "090-1618.rom", 0x040000, 0x20000, 0xc7d9df5d )
	ROM_LOAD( "090-1615.rom", 0x060000, 0x20000, 0xac3642c7 )
	ROM_LOAD( "090-1617.rom", 0x080000, 0x20000, 0x15208a89 )
	ROM_LOAD( "090-1619.rom", 0x0a0000, 0x20000, 0x8a5d79b3 )
ROM_END



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static const UINT16 default_eeprom[] =
{
	0x0001,0x011A,0x012A,0x0146,0x0100,0x0168,0x0300,0x011E,
	0x0700,0x0122,0x0600,0x0120,0x0400,0x0102,0x0300,0x017E,
	0x0200,0x0128,0x0104,0x0100,0x014E,0x0100,0x013E,0x0122,
	0x011A,0x012A,0x0146,0x0100,0x0168,0x0300,0x011E,0x0700,
	0x0122,0x0600,0x0120,0x0400,0x0102,0x0300,0x017E,0x0200,
	0x0128,0x0104,0x0100,0x014E,0x0100,0x013E,0x0122,0x1A00,
	0x0154,0x0125,0x01DC,0x0100,0x0192,0x0105,0x01DC,0x0181,
	0x012E,0x0106,0x0100,0x0105,0x0179,0x0132,0x0101,0x0100,
	0x01D3,0x0105,0x0116,0x0127,0x0134,0x0100,0x0104,0x01B0,
	0x0165,0x0102,0x1600,0x0000
};


static void init_offtwall(void)
{
	atarigen_eeprom_default = default_eeprom;

	atarijsa_init(1, 2, 3, 0x0040);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x41dd, 0x41f5);

	/* install son-of-slapstic workarounds */
	spritecache_count = (UINT8*)install_mem_read_handler(0, 0x3fde42, 0x3fde43, spritecache_count_r);
	bankswitch_base = (UINT8*)install_mem_read_handler(0, 0x037ec2, 0x037f39, bankswitch_r);
	unknown_verify_base = (UINT8*)install_mem_read_handler(0, 0x3fdf1e, 0x3fdf1f, unknown_verify_r);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_offtwalc(void)
{
	atarigen_eeprom_default = default_eeprom;

	atarijsa_init(1, 2, 3, 0x0040);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x41dd, 0x41f5);

	/* install son-of-slapstic workarounds */
	spritecache_count = (UINT8*)install_mem_read_handler(0, 0x3fde42, 0x3fde43, spritecache_count_r);
	bankswitch_base = (UINT8*)install_mem_read_handler(0, 0x037eca, 0x037f43, bankswitch_r);
	unknown_verify_base = (UINT8*)install_mem_read_handler(0, 0x3fdf24, 0x3fdf25, unknown_verify_r);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1991, offtwall, 0,        offtwall, offtwall, offtwall, ROT0, "Atari Games", "Off the Wall (2/3-player upright)" )
GAME( 1991, offtwalc, offtwall, offtwall, offtwall, offtwalc, ROT0, "Atari Games", "Off the Wall (2-player cocktail)" )
