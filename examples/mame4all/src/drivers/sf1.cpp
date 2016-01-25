#include "../vidhrdw/sf1.cpp"

/***************************************************************************

  Street Fighter 1

  driver by Olivier Galibert

***************************************************************************/

#include "driver.h"
#include "cpuintrf.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "cpu/m68000/m68000.h"


extern unsigned char *sf1_objectram;
extern int sf1_deltaxb;
extern int sf1_deltaxm;
extern int sf1_active;

static unsigned char *sharedram;

int sf1_vh_start(void);
void sf1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( sf1_deltaxb_w );
WRITE_HANDLER( sf1_deltaxm_w );
WRITE_HANDLER( sf1_videoram_w );
void sf1_active_w(int data);


static READ_HANDLER( dummy_r )
{
	return 0xffff;
}

/* b0 = reset, or maybe "set anyway" */
/* b1 = pulsed when control6.b6==0 until it's 1 */
/* b2 = active when dip 8 (flip) on */
/* b3 = active character plane */
/* b4 = unused */
/* b5 = active background plane */
/* b6 = active middle plane */
/* b7 = active sprites */
static WRITE_HANDLER( active_w )
{
	if((data&0xff)!=0)
		sf1_active_w(data);
}


static WRITE_HANDLER( soundcmd_w )
{
	if (data != 0xffff) {
		soundlatch_w(offset,data);
		cpu_cause_interrupt(1,Z80_NMI_INT);
	}
}

/* Idle cycles skipping */
static WRITE_HANDLER( shared_w )
{
	COMBINE_WORD_MEM (sharedram+offset, data);

	if (offset)
		return;
	switch(cpu_get_pc()) {
	case 0x3b1a6:
	case 0x3b1a2:
	case 0x3a1d6:
		if(!(cpu_get_reg(M68K_SR) & 0x0700)) {
			int i;
			unsigned char *adr = sharedram+6;
			for(i=0;i<16;i++) {
				if(READ_WORD(adr) & 0x0300)
					return;
				adr += 16;
			}
			cpu_spinuntil_int();
		}
		break;
	}
}

/* The protection of the japanese version */
/* I'd love to see someone dump the 68705 rom */

static WRITE_HANDLER( protection_w )
{
	static int maplist[4][10] = {
		{ 1, 0, 3, 2, 4, 5, 6, 7, 8, 9 },
		{ 4, 5, 6, 7, 1, 0, 3, 2, 8, 9 },
		{ 3, 2, 1, 0, 6, 7, 4, 5, 8, 9 },
		{ 6, 7, 4, 5, 3, 2, 1, 0, 8, 9 }
	};
	int map;

	map = maplist
		[cpu_readmem24bew(0xffc006)]
		[(cpu_readmem24bew(0xffc003)<<1) + (cpu_readmem24bew_word(0xffc004)>>8)];

	switch(cpu_readmem24bew(0xffc684)) {
	case 1:
		{
			int base;

			base = 0x1b6e8+0x300e*map;

			cpu_writemem24bew_dword(0xffc01c, 0x16bfc+0x270*map);
			cpu_writemem24bew_dword(0xffc020, base+0x80);
			cpu_writemem24bew_dword(0xffc024, base);
			cpu_writemem24bew_dword(0xffc028, base+0x86);
			cpu_writemem24bew_dword(0xffc02c, base+0x8e);
			cpu_writemem24bew_dword(0xffc030, base+0x20e);
			cpu_writemem24bew_dword(0xffc034, base+0x30e);
			cpu_writemem24bew_dword(0xffc038, base+0x38e);
			cpu_writemem24bew_dword(0xffc03c, base+0x40e);
			cpu_writemem24bew_dword(0xffc040, base+0x80e);
			cpu_writemem24bew_dword(0xffc044, base+0xc0e);
			cpu_writemem24bew_dword(0xffc048, base+0x180e);
			cpu_writemem24bew_dword(0xffc04c, base+0x240e);
			cpu_writemem24bew_dword(0xffc050, 0x19548+0x60*map);
			cpu_writemem24bew_dword(0xffc054, 0x19578+0x60*map);
			break;
		}
	case 2:
		{
			static int delta1[10] = {
				0x1f80, 0x1c80, 0x2700, 0x2400, 0x2b80, 0x2e80, 0x3300, 0x3600, 0x3a80, 0x3d80
			};
			static int delta2[10] = {
				0x2180, 0x1800, 0x3480, 0x2b00, 0x3e00, 0x4780, 0x5100, 0x5a80, 0x6400, 0x6d80
			};

			int d1 = delta1[map] + 0xc0;
			int d2 = delta2[map];

			cpu_writemem24bew_word(0xffc680, d1);
			cpu_writemem24bew_word(0xffc682, d2);
			cpu_writemem24bew_word(0xffc00c, 0xc0);
			cpu_writemem24bew_word(0xffc00e, 0);

			sf1_deltaxm_w(0, d1);
			sf1_deltaxb_w(0, d2);
			break;
		}
	case 4:
		{
			int pos = cpu_readmem24bew(0xffc010);
			pos = (pos+1) & 3;
			cpu_writemem24bew(0xffc010, pos);
			if(!pos) {
				int d1 = cpu_readmem24bew_word(0xffc682);
				int off = cpu_readmem24bew_word(0xffc00e);
				if(off!=512) {
					off++;
					d1++;
				} else {
					off = 0;
					d1 -= 512;
				}
				cpu_writemem24bew_word(0xffc682, d1);
				cpu_writemem24bew_word(0xffc00e, off);
				sf1_deltaxb_w(0, d1);
			}
			break;
		}
	default:
		{
			//logerror("Write protection at %06x (%04x)\n", cpu_get_pc(), data&0xffff);
			//logerror("*** Unknown protection %d\n", cpu_readmem24bew(0xffc684));
			break;
		}
	}
}


/* The world version has analog buttons */
/* We simulate them with 3 buttons the same way the other versions
   internally do */

static int scale[8] = { 0x00, 0x40, 0xe0, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe };

static READ_HANDLER( button1_r )
{
	return (scale[input_port_7_r(0)]<<8)|scale[input_port_5_r(0)];
}

static READ_HANDLER( button2_r )
{
	return (scale[input_port_8_r(0)]<<8)|scale[input_port_6_r(0)];
}


static WRITE_HANDLER( sound2_bank_w )
{
	cpu_setbank(4, memory_region(REGION_CPU3)+0x8000*(data+1));
}


static WRITE_HANDLER( msm5205_w )
{
	MSM5205_reset_w(offset,(data>>7)&1);
	/* ?? bit 6?? */
	MSM5205_data_w(offset,data);
	MSM5205_vclk_w(offset,1);
	MSM5205_vclk_w(offset,0);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x04ffff, MRA_ROM },
	{ 0x800000, 0x800fff, MRA_BANK3 },
	{ 0xc00000, 0xc00001, input_port_3_r },
	{ 0xc00002, 0xc00003, input_port_4_r },
	{ 0xc00004, 0xc00005, button1_r },
	{ 0xc00006, 0xc00007, button2_r },
	{ 0xc00008, 0xc00009, input_port_0_r },
	{ 0xc0000a, 0xc0000b, input_port_1_r },
	{ 0xc0000c, 0xc0000d, input_port_2_r },
	{ 0xc0000e, 0xc0000f, dummy_r },
	{ 0xff8000, 0xff80ff, MRA_BANK5 },
	{ 0xff8100, 0xffdfff, MRA_BANK1 },
	{ 0xffe000, 0xffffff, MRA_BANK2 },
	{ -1 }
};

static struct MemoryReadAddress readmemus[] =
{
	{ 0x000000, 0x04ffff, MRA_ROM },
	{ 0x800000, 0x800fff, MRA_BANK3 },
	{ 0xc00000, 0xc00001, input_port_3_r },
	{ 0xc00002, 0xc00003, input_port_4_r },
	{ 0xc00004, 0xc00005, dummy_r },
	{ 0xc00006, 0xc00007, dummy_r },
	{ 0xc00008, 0xc00009, input_port_0_r },
	{ 0xc0000a, 0xc0000b, input_port_1_r },
	{ 0xc0000c, 0xc0000d, input_port_2_r },
	{ 0xc0000e, 0xc0000f, dummy_r },
	{ 0xff8000, 0xff80ff, MRA_BANK5 },
	{ 0xff8100, 0xffdfff, MRA_BANK1 },
	{ 0xffe000, 0xffffff, MRA_BANK2 },
	{ -1 }
};

static struct MemoryReadAddress readmemjp[] =
{
	{ 0x000000, 0x04ffff, MRA_ROM },
	{ 0x800000, 0x800fff, MRA_BANK3 },
	{ 0xc00000, 0xc00001, input_port_3_r },
	{ 0xc00002, 0xc00003, input_port_4_r },
	{ 0xc00004, 0xc00005, input_port_5_r },
	{ 0xc00006, 0xc00007, dummy_r },
	{ 0xc00008, 0xc00009, input_port_0_r },
	{ 0xc0000a, 0xc0000b, input_port_1_r },
	{ 0xc0000c, 0xc0000d, input_port_2_r },
	{ 0xc0000e, 0xc0000f, dummy_r },
	{ 0xff8000, 0xff80ff, MRA_BANK5 },
	{ 0xff8100, 0xffdfff, MRA_BANK1 },
	{ 0xffe000, 0xffffff, MRA_BANK2 },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x04ffff, MWA_ROM },
	{ 0x800000, 0x800fff, sf1_videoram_w, &videoram, &videoram_size },
	{ 0xb00000, 0xb007ff, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },
	{ 0xc00010, 0xc00011, MWA_NOP }, /* Coin counters, etc... */
	{ 0xc00014, 0xc00015, sf1_deltaxm_w },
	{ 0xc00017, 0xc00018, sf1_deltaxb_w },
	{ 0xc0001a, 0xc0001b, active_w },
	{ 0xc0001c, 0xc0001d, soundcmd_w },
	{ 0xc0001e, 0xc0001f, protection_w },
	{ 0xff8000, 0xff80ff, shared_w, &sharedram },
	{ 0xff8100, 0xffdfff, MWA_BANK1 },
	{ 0xffe000, 0xffffff, MWA_BANK2, &sf1_objectram },
	{ -1 }
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc800, soundlatch_r },
	{ 0xe001, 0xe001, YM2151_status_port_0_r },
	{ -1 }
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xe000, 0xe000, YM2151_register_port_0_w },
	{ 0xe001, 0xe001, YM2151_data_port_0_w },
	{ -1 }
};


static struct MemoryReadAddress sound2_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xffff, MRA_BANK4 },
	{ -1 }
};

/* Yes, _no_ ram */
static struct MemoryWriteAddress sound2_writemem[] =
{
	{ 0x0000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct IOReadPort sound2_readport[] =
{
	{ 0x01, 0x01, soundlatch_r },
	{ -1 }
};


static struct IOWritePort sound2_writeport[] =
{
	{ 0x00, 0x01, msm5205_w },
	{ 0x02, 0x02, sound2_bank_w },
	{ -1 }
};


INPUT_PORTS_START( sf1jp )
	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "Attract music" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x1000, 0x1000, "Speed" )
	PORT_DIPSETTING(      0x0000, "Slow" )
	PORT_DIPSETTING(      0x1000, "Normal" )
	PORT_DIPNAME( 0x2000, 0x2000, "Attract sound?" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Freeze" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, "Continuation max stage" )
	PORT_DIPSETTING(      0x0007, "5th" )
	PORT_DIPSETTING(      0x0006, "4th" )
	PORT_DIPSETTING(      0x0005, "3rd" )
	PORT_DIPSETTING(      0x0004, "2nd" )
	PORT_DIPSETTING(      0x0003, "1st" )
	PORT_DIPSETTING(      0x0002, "No continuation" )
	PORT_DIPNAME( 0x0018, 0x0018, "Round time" )
	PORT_DIPSETTING(      0x0018, "100" )
	PORT_DIPSETTING(      0x0010, "150" )
	PORT_DIPSETTING(      0x0008, "200" )
	PORT_DIPSETTING(      0x0000, "250" )
	PORT_DIPNAME( 0x0060, 0x0060, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0060, "Normal" )
	PORT_DIPSETTING(      0x0040, "Easy" )
	PORT_DIPSETTING(      0x0020, "Difficult" )
	PORT_DIPSETTING(      0x0000, "Very difficult" )
	PORT_DIPNAME( 0x0380, 0x0380, "Buy-in max stage" )
	PORT_DIPSETTING(      0x0380, "5th" )
	PORT_DIPSETTING(      0x0300, "4th" )
	PORT_DIPSETTING(      0x0280, "3rd" )
	PORT_DIPSETTING(      0x0200, "2nd" )
	PORT_DIPSETTING(      0x0180, "1st" )
	PORT_DIPSETTING(      0x0080, "No buy-in" )
	PORT_DIPNAME( 0x0400, 0x0000, "Number of start countries" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPSETTING(      0x0400, "2" )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Freezes the game ? */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )


	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( sf1us )
	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "Attract music" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x1000, 0x1000, "Speed" )
	PORT_DIPSETTING(      0x0000, "Slow" )
	PORT_DIPSETTING(      0x1000, "Normal" )
	PORT_DIPNAME( 0x2000, 0x2000, "Attract sound?" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Freeze" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, "Continuation max stage" )
	PORT_DIPSETTING(      0x0007, "5th" )
	PORT_DIPSETTING(      0x0006, "4th" )
	PORT_DIPSETTING(      0x0005, "3rd" )
	PORT_DIPSETTING(      0x0004, "2nd" )
	PORT_DIPSETTING(      0x0003, "1st" )
	PORT_DIPSETTING(      0x0002, "No continuation" )
	PORT_DIPNAME( 0x0018, 0x0018, "Round time" )
	PORT_DIPSETTING(      0x0018, "100" )
	PORT_DIPSETTING(      0x0010, "150" )
	PORT_DIPSETTING(      0x0008, "200" )
	PORT_DIPSETTING(      0x0000, "250" )
	PORT_DIPNAME( 0x0060, 0x0060, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0060, "Normal" )
	PORT_DIPSETTING(      0x0040, "Easy" )
	PORT_DIPSETTING(      0x0020, "Difficult" )
	PORT_DIPSETTING(      0x0000, "Very difficult" )
	PORT_DIPNAME( 0x0380, 0x0380, "Buy-in max stage" )
	PORT_DIPSETTING(      0x0380, "5th" )
	PORT_DIPSETTING(      0x0300, "4th" )
	PORT_DIPSETTING(      0x0280, "3rd" )
	PORT_DIPSETTING(      0x0200, "2nd" )
	PORT_DIPSETTING(      0x0180, "1st" )
	PORT_DIPSETTING(      0x0080, "No buy-in" )
	PORT_DIPNAME( 0x0400, 0x0000, "Number of start countries" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPSETTING(      0x0400, "2" )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Freezes the game ? */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( sf1 )
	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "Attract music" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x1000, 0x1000, "Speed" )
	PORT_DIPSETTING(      0x0000, "Slow" )
	PORT_DIPSETTING(      0x1000, "Normal" )
	PORT_DIPNAME( 0x2000, 0x2000, "Attract sound?" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Freeze" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, "Continuation max stage" )
	PORT_DIPSETTING(      0x0007, "5th" )
	PORT_DIPSETTING(      0x0006, "4th" )
	PORT_DIPSETTING(      0x0005, "3rd" )
	PORT_DIPSETTING(      0x0004, "2nd" )
	PORT_DIPSETTING(      0x0003, "1st" )
	PORT_DIPSETTING(      0x0002, "No continuation" )
	PORT_DIPNAME( 0x0018, 0x0018, "Round time" )
	PORT_DIPSETTING(      0x0018, "100" )
	PORT_DIPSETTING(      0x0010, "150" )
	PORT_DIPSETTING(      0x0008, "200" )
	PORT_DIPSETTING(      0x0000, "250" )
	PORT_DIPNAME( 0x0060, 0x0060, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0060, "Normal" )
	PORT_DIPSETTING(      0x0040, "Easy" )
	PORT_DIPSETTING(      0x0020, "Difficult" )
	PORT_DIPSETTING(      0x0000, "Very difficult" )
	PORT_DIPNAME( 0x0380, 0x0380, "Buy-in max stage" )
	PORT_DIPSETTING(      0x0380, "5th" )
	PORT_DIPSETTING(      0x0300, "4th" )
	PORT_DIPSETTING(      0x0280, "3rd" )
	PORT_DIPSETTING(      0x0200, "2nd" )
	PORT_DIPSETTING(      0x0180, "1st" )
	PORT_DIPSETTING(      0x0080, "No buy-in" )
	PORT_DIPNAME( 0x0400, 0x0400, "Number of start countries" )
	PORT_DIPSETTING(      0x0400, "4" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Freezes the game ? */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )


	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout char_layout =
{
	8,8,
	1024,
	2,
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8
};

static struct GfxLayout sprite_layoutb =
{
	16,16,
	4096,
	4,
	{ 4, 0, 4096*64*8+4, 4096*64*8 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			16*16+0, 16*16+1, 16*16+2, 16*16+3, 16*16+8+0, 16*16+8+1, 16*16+8+2, 16*16+8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout sprite_layoutm =
{
	16,16,
	8192,
	4,
	{ 4, 0, 8192*64*8+4, 8192*64*8 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			16*16+0, 16*16+1, 16*16+2, 16*16+3, 16*16+8+0, 16*16+8+1, 16*16+8+2, 16*16+8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout sprite_layouts =
{
	16,16,
	14336,
	4,
	{ 4, 0, 14336*64*8+4, 14336*64*8 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			16*16+0, 16*16+1, 16*16+2, 16*16+3, 16*16+8+0, 16*16+8+1, 16*16+8+2, 16*16+8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &sprite_layoutb,   0, 16 },
	{ REGION_GFX2, 0, &sprite_layoutm, 256, 16 },
	{ REGION_GFX3, 0, &sprite_layouts, 512, 16 },
	{ REGION_GFX4, 0, &char_layout,    768, 16 },
	{ -1 }
};



static void irq_handler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2151interface ym2151_interface =
{
	1,	/* 1 chip */
	3579545,	/* ? xtal is 3.579545MHz */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ irq_handler }
};

static struct MSM5205interface msm5205_interface =
{
	2,		/* 2 chips */
	384000,				/* 384KHz ?           */
	{ 0, 0 },/* interrupt function */
	{ MSM5205_SEX_4B,MSM5205_SEX_4B},	/* 8KHz playback ?    */
	{ 100, 100 }
};

static struct MachineDriver machine_driver_sf1 =
{
	{
		{
			CPU_M68000,
			8000000,	/* 8 MHz ? (xtal is 16MHz) */
			readmem,writemem,0,0,
			m68_level1_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? xtal is 3.579545MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM2151 */
								/* NMIs are caused by the main CPU */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? xtal is 3.579545MHz */
			sound2_readmem, sound2_writemem,
			sound2_readport, sound2_writeport,
			0,0,
			interrupt,8000
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 8*8, (64-8)*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sf1_vh_start,
	0,
	sf1_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};

static struct MachineDriver machine_driver_sf1us =
{
	{
		{
			CPU_M68000,
			8000000,	/* 8 MHz ? (xtal is 16MHz) */
			readmemus,writemem,0,0,
			m68_level1_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? xtal is 3.579545MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM2151 */
								/* NMIs are caused by the main CPU */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? xtal is 3.579545MHz */
			sound2_readmem, sound2_writemem,
			sound2_readport, sound2_writeport,
			0,0,
			interrupt,8000
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 8*8, (64-8)*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sf1_vh_start,
	0,
	sf1_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};

static struct MachineDriver machine_driver_sf1jp =
{
	{
		{
			CPU_M68000,
			8000000,	/* 8 MHz ? (xtal is 16MHz) */
			readmemjp,writemem,0,0,
			m68_level1_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? xtal is 3.579545MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM2151 */
								/* NMIs are caused by the main CPU */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* ? xtal is 3.579545MHz */
			sound2_readmem, sound2_writemem,
			sound2_readport, sound2_writeport,
			0,0,
			interrupt,8000
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 8*8, (64-8)*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sf1_vh_start,
	0,
	sf1_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};


ROM_START( sf1 )
	ROM_REGION( 0x60000, REGION_CPU1 )
	ROM_LOAD_EVEN("sfe-19", 0x00000, 0x10000, 0x8346c3ca )
	ROM_LOAD_ODD ("sfe-22", 0x00000, 0x10000, 0x3a4bfaa8 )
	ROM_LOAD_EVEN("sfe-20", 0x20000, 0x10000, 0xb40e67ee )
	ROM_LOAD_ODD ("sfe-23", 0x20000, 0x10000, 0x477c3d5b )
	ROM_LOAD_EVEN("sfe-21", 0x40000, 0x10000, 0x2547192b )
	ROM_LOAD_ODD ("sfe-24", 0x40000, 0x10000, 0x79680f4e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the music CPU */
	ROM_LOAD( "sf-02.bin", 0x0000, 0x8000, 0x4a9ac534 )

	ROM_REGION( 0x40000, REGION_CPU3 )	/* 256k for the samples CPU */
	ROM_LOAD( "sfu-00",    0x00000, 0x20000, 0xa7cce903 )
	ROM_LOAD( "sf-01.bin", 0x20000, 0x20000, 0x86e0f0d5 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-39.bin", 0x000000, 0x020000, 0xcee3d292 ) /* Background b planes 0-1*/
	ROM_LOAD( "sf-38.bin", 0x020000, 0x020000, 0x2ea99676 )
	ROM_LOAD( "sf-41.bin", 0x040000, 0x020000, 0xe0280495 ) /* planes 2-3 */
	ROM_LOAD( "sf-40.bin", 0x060000, 0x020000, 0xc70b30de )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-25.bin", 0x000000, 0x020000, 0x7f23042e )	/* Background m planes 0-1 */
	ROM_LOAD( "sf-28.bin", 0x020000, 0x020000, 0x92f8b91c )
	ROM_LOAD( "sf-30.bin", 0x040000, 0x020000, 0xb1399856 )
	ROM_LOAD( "sf-34.bin", 0x060000, 0x020000, 0x96b6ae2e )
	ROM_LOAD( "sf-26.bin", 0x080000, 0x020000, 0x54ede9f5 ) /* planes 2-3 */
	ROM_LOAD( "sf-29.bin", 0x0a0000, 0x020000, 0xf0649a67 )
	ROM_LOAD( "sf-31.bin", 0x0c0000, 0x020000, 0x8f4dd71a )
	ROM_LOAD( "sf-35.bin", 0x0e0000, 0x020000, 0x70c00fb4 )

	ROM_REGION( 0x1c0000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-15.bin", 0x000000, 0x020000, 0xfc0113db ) /* Sprites planes 1-2 */
	ROM_LOAD( "sf-16.bin", 0x020000, 0x020000, 0x82e4a6d3 )
	ROM_LOAD( "sf-11.bin", 0x040000, 0x020000, 0xe112df1b )
	ROM_LOAD( "sf-12.bin", 0x060000, 0x020000, 0x42d52299 )
	ROM_LOAD( "sf-07.bin", 0x080000, 0x020000, 0x49f340d9 )
	ROM_LOAD( "sf-08.bin", 0x0a0000, 0x020000, 0x95ece9b1 )
	ROM_LOAD( "sf-03.bin", 0x0c0000, 0x020000, 0x5ca05781 )
	ROM_LOAD( "sf-17.bin", 0x0e0000, 0x020000, 0x69fac48e ) /* planes 2-3 */
	ROM_LOAD( "sf-18.bin", 0x100000, 0x020000, 0x71cfd18d )
	ROM_LOAD( "sf-13.bin", 0x120000, 0x020000, 0xfa2eb24b )
	ROM_LOAD( "sf-14.bin", 0x140000, 0x020000, 0xad955c95 )
	ROM_LOAD( "sf-09.bin", 0x160000, 0x020000, 0x41b73a31 )
	ROM_LOAD( "sf-10.bin", 0x180000, 0x020000, 0x91c41c50 )
	ROM_LOAD( "sf-05.bin", 0x1a0000, 0x020000, 0x538c7cbe )

	ROM_REGION( 0x004000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-27.bin", 0x000000, 0x004000, 0x2b09b36d ) /* Characters planes 1-2 */

	ROM_REGION( 0x40000, REGION_GFX5 )	/* background tilemaps */
	ROM_LOAD( "sf-37.bin", 0x000000, 0x010000, 0x23d09d3d )
	ROM_LOAD( "sf-36.bin", 0x010000, 0x010000, 0xea16df6c )
	ROM_LOAD( "sf-32.bin", 0x020000, 0x010000, 0x72df2bd9 )
	ROM_LOAD( "sf-33.bin", 0x030000, 0x010000, 0x3e99d3d5 )
ROM_END

ROM_START( sf1us )
	ROM_REGION( 0x60000, REGION_CPU1 )
	ROM_LOAD_EVEN("sfd-19", 0x00000, 0x10000, 0xfaaf6255 )
	ROM_LOAD_ODD ("sfd-22", 0x00000, 0x10000, 0xe1fe3519 )
	ROM_LOAD_EVEN("sfd-20", 0x20000, 0x10000, 0x44b915bd )
	ROM_LOAD_ODD ("sfd-23", 0x20000, 0x10000, 0x79c43ff8 )
	ROM_LOAD_EVEN("sfd-21", 0x40000, 0x10000, 0xe8db799b )
	ROM_LOAD_ODD ("sfd-24", 0x40000, 0x10000, 0x466a3440 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the music CPU */
	ROM_LOAD( "sf-02.bin", 0x0000, 0x8000, 0x4a9ac534 )

	ROM_REGION( 0x40000, REGION_CPU3 )	/* 256k for the samples CPU */
	ROM_LOAD( "sfu-00",    0x00000, 0x20000, 0xa7cce903 )
	ROM_LOAD( "sf-01.bin", 0x20000, 0x20000, 0x86e0f0d5 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-39.bin", 0x000000, 0x020000, 0xcee3d292 ) /* Background b planes 0-1*/
	ROM_LOAD( "sf-38.bin", 0x020000, 0x020000, 0x2ea99676 )
	ROM_LOAD( "sf-41.bin", 0x040000, 0x020000, 0xe0280495 ) /* planes 2-3 */
	ROM_LOAD( "sf-40.bin", 0x060000, 0x020000, 0xc70b30de )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-25.bin", 0x000000, 0x020000, 0x7f23042e )	/* Background m planes 0-1 */
	ROM_LOAD( "sf-28.bin", 0x020000, 0x020000, 0x92f8b91c )
	ROM_LOAD( "sf-30.bin", 0x040000, 0x020000, 0xb1399856 )
	ROM_LOAD( "sf-34.bin", 0x060000, 0x020000, 0x96b6ae2e )
	ROM_LOAD( "sf-26.bin", 0x080000, 0x020000, 0x54ede9f5 ) /* planes 2-3 */
	ROM_LOAD( "sf-29.bin", 0x0a0000, 0x020000, 0xf0649a67 )
	ROM_LOAD( "sf-31.bin", 0x0c0000, 0x020000, 0x8f4dd71a )
	ROM_LOAD( "sf-35.bin", 0x0e0000, 0x020000, 0x70c00fb4 )

	ROM_REGION( 0x1c0000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-15.bin", 0x000000, 0x020000, 0xfc0113db ) /* Sprites planes 1-2 */
	ROM_LOAD( "sf-16.bin", 0x020000, 0x020000, 0x82e4a6d3 )
	ROM_LOAD( "sf-11.bin", 0x040000, 0x020000, 0xe112df1b )
	ROM_LOAD( "sf-12.bin", 0x060000, 0x020000, 0x42d52299 )
	ROM_LOAD( "sf-07.bin", 0x080000, 0x020000, 0x49f340d9 )
	ROM_LOAD( "sf-08.bin", 0x0a0000, 0x020000, 0x95ece9b1 )
	ROM_LOAD( "sf-03.bin", 0x0c0000, 0x020000, 0x5ca05781 )
	ROM_LOAD( "sf-17.bin", 0x0e0000, 0x020000, 0x69fac48e ) /* planes 2-3 */
	ROM_LOAD( "sf-18.bin", 0x100000, 0x020000, 0x71cfd18d )
	ROM_LOAD( "sf-13.bin", 0x120000, 0x020000, 0xfa2eb24b )
	ROM_LOAD( "sf-14.bin", 0x140000, 0x020000, 0xad955c95 )
	ROM_LOAD( "sf-09.bin", 0x160000, 0x020000, 0x41b73a31 )
	ROM_LOAD( "sf-10.bin", 0x180000, 0x020000, 0x91c41c50 )
	ROM_LOAD( "sf-05.bin", 0x1a0000, 0x020000, 0x538c7cbe )

	ROM_REGION( 0x004000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-27.bin", 0x000000, 0x004000, 0x2b09b36d ) /* Characters planes 1-2 */

	ROM_REGION( 0x40000, REGION_GFX5 )	/* background tilemaps */
	ROM_LOAD( "sf-37.bin", 0x000000, 0x010000, 0x23d09d3d )
	ROM_LOAD( "sf-36.bin", 0x010000, 0x010000, 0xea16df6c )
	ROM_LOAD( "sf-32.bin", 0x020000, 0x010000, 0x72df2bd9 )
	ROM_LOAD( "sf-33.bin", 0x030000, 0x010000, 0x3e99d3d5 )
ROM_END

ROM_START( sf1jp )
	ROM_REGION( 0x60000, REGION_CPU1 )
	ROM_LOAD_EVEN("sf-19.bin", 0x00000, 0x10000, 0x116027d7 )
	ROM_LOAD_ODD ("sf-22.bin", 0x00000, 0x10000, 0xd3cbd09e )
	ROM_LOAD_EVEN("sf-20.bin", 0x20000, 0x10000, 0xfe07e83f )
	ROM_LOAD_ODD ("sf-23.bin", 0x20000, 0x10000, 0x1e435d33 )
	ROM_LOAD_EVEN("sf-21.bin", 0x40000, 0x10000, 0xe086bc4c )
	ROM_LOAD_ODD ("sf-24.bin", 0x40000, 0x10000, 0x13a6696b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the music CPU */
	ROM_LOAD( "sf-02.bin", 0x0000, 0x8000, 0x4a9ac534 )

	ROM_REGION( 0x40000, REGION_CPU3 )	/* 256k for the samples CPU */
	ROM_LOAD( "sf-00.bin", 0x00000, 0x20000, 0x4b733845 )
	ROM_LOAD( "sf-01.bin", 0x20000, 0x20000, 0x86e0f0d5 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-39.bin", 0x000000, 0x020000, 0xcee3d292 ) /* Background b planes 0-1*/
	ROM_LOAD( "sf-38.bin", 0x020000, 0x020000, 0x2ea99676 )
	ROM_LOAD( "sf-41.bin", 0x040000, 0x020000, 0xe0280495 ) /* planes 2-3 */
	ROM_LOAD( "sf-40.bin", 0x060000, 0x020000, 0xc70b30de )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-25.bin", 0x000000, 0x020000, 0x7f23042e )	/* Background m planes 0-1 */
	ROM_LOAD( "sf-28.bin", 0x020000, 0x020000, 0x92f8b91c )
	ROM_LOAD( "sf-30.bin", 0x040000, 0x020000, 0xb1399856 )
	ROM_LOAD( "sf-34.bin", 0x060000, 0x020000, 0x96b6ae2e )
	ROM_LOAD( "sf-26.bin", 0x080000, 0x020000, 0x54ede9f5 ) /* planes 2-3 */
	ROM_LOAD( "sf-29.bin", 0x0a0000, 0x020000, 0xf0649a67 )
	ROM_LOAD( "sf-31.bin", 0x0c0000, 0x020000, 0x8f4dd71a )
	ROM_LOAD( "sf-35.bin", 0x0e0000, 0x020000, 0x70c00fb4 )

	ROM_REGION( 0x1c0000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-15.bin", 0x000000, 0x020000, 0xfc0113db ) /* Sprites planes 1-2 */
	ROM_LOAD( "sf-16.bin", 0x020000, 0x020000, 0x82e4a6d3 )
	ROM_LOAD( "sf-11.bin", 0x040000, 0x020000, 0xe112df1b )
	ROM_LOAD( "sf-12.bin", 0x060000, 0x020000, 0x42d52299 )
	ROM_LOAD( "sf-07.bin", 0x080000, 0x020000, 0x49f340d9 )
	ROM_LOAD( "sf-08.bin", 0x0a0000, 0x020000, 0x95ece9b1 )
	ROM_LOAD( "sf-03.bin", 0x0c0000, 0x020000, 0x5ca05781 )
	ROM_LOAD( "sf-17.bin", 0x0e0000, 0x020000, 0x69fac48e ) /* planes 2-3 */
	ROM_LOAD( "sf-18.bin", 0x100000, 0x020000, 0x71cfd18d )
	ROM_LOAD( "sf-13.bin", 0x120000, 0x020000, 0xfa2eb24b )
	ROM_LOAD( "sf-14.bin", 0x140000, 0x020000, 0xad955c95 )
	ROM_LOAD( "sf-09.bin", 0x160000, 0x020000, 0x41b73a31 )
	ROM_LOAD( "sf-10.bin", 0x180000, 0x020000, 0x91c41c50 )
	ROM_LOAD( "sf-05.bin", 0x1a0000, 0x020000, 0x538c7cbe )

	ROM_REGION( 0x004000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sf-27.bin", 0x000000, 0x004000, 0x2b09b36d ) /* Characters planes 1-2 */

	ROM_REGION( 0x40000, REGION_GFX5 )	/* background tilemaps */
	ROM_LOAD( "sf-37.bin", 0x000000, 0x010000, 0x23d09d3d )
	ROM_LOAD( "sf-36.bin", 0x010000, 0x010000, 0xea16df6c )
	ROM_LOAD( "sf-32.bin", 0x020000, 0x010000, 0x72df2bd9 )
	ROM_LOAD( "sf-33.bin", 0x030000, 0x010000, 0x3e99d3d5 )
ROM_END



GAMEX( 1987, sf1,   0,   sf1,   sf1,   0, ROT0, "Capcom", "Street Fighter (World)", GAME_NO_COCKTAIL )
GAMEX( 1987, sf1us, sf1, sf1us, sf1us, 0, ROT0, "Capcom", "Street Fighter (US)", GAME_NO_COCKTAIL )
GAMEX( 1987, sf1jp, sf1, sf1jp, sf1jp, 0, ROT0, "Capcom", "Street Fighter (Japan)", GAME_NO_COCKTAIL )
