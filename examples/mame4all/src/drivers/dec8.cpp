#include "../vidhrdw/dec8.cpp"

/***************************************************************************

Various Data East 8 bit games:

	Cobra Command (World)       (c) 1988 Data East Corporation (6809)
	Cobra Command (Japan)       (c) 1988 Data East Corporation (6809)
	The Real Ghostbusters (2p)  (c) 1987 Data East USA (6809 + I8751)
	The Real Ghostbusters (3p)  (c) 1987 Data East USA (6809 + I8751)
	Meikyuu Hunter G            (c) 1987 Data East Corporation (6809 + I8751)
	Super Real Darwin           (c) 1987 Data East Corporation (6809 + I8751)
	Psycho-Nics Oscar           (c) 1988 Data East USA (2*6809 + I8751)
	Psycho-Nics Oscar (Japan)   (c) 1987 Data East Corporation (2*6809 + I8751)
	Gondomania                  (c) 1987 Data East USA (6809 + I8751)
	Makyou Senshi               (c) 1987 Data East Corporation (6809 + I8751)
	Last Mission (rev 6)        (c) 1986 Data East USA (2*6809 + I8751)
	Last Mission (rev 5)        (c) 1986 Data East USA (2*6809 + I8751)
	Shackled                    (c) 1986 Data East USA (2*6809 + I8751)
	Breywood                    (c) 1986 Data East Corporation (2*6809 + I8751)
	Captain Silver (Japan)      (c) 1987 Data East Corporation (2*6809 + I8751)
	Garyo Retsuden (Japan)      (c) 1987 Data East Corporation (6809 + I8751)

	All games use a 6502 for sound (some are encrypted), all games except Cobracom
	use an Intel 8751 for protection & coinage.  For these games the coinage dip
	switch is not currently supported, they are fixed at 1 coin 1 credit.

	Meikyuu Hunter G was formerly known as Mazehunter.

	Emulation by Bryan McPhail, mish@tendril.co.uk

To do:
	Weird cpu race condition in Last Mission.
	Support coinage options for all i8751 emulations.
	Dips needed to be worked on several games
	Super Real Darwin 'Double' sprites appearing from the top of the screen are clipped
	Strangely coloured butterfly on Garyo Retsuden water levels!

  Thanks to José Miguel Morales Farreras for Super Real Darwin information!

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m6502/m6502.h"

void ghostb_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void cobracom_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void ghostb_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void srdarwin_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void gondo_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void garyoret_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void lastmiss_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void shackled_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void oscar_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
int cobracom_vh_start(void);
int oscar_vh_start(void);
int ghostb_vh_start(void);
int lastmiss_vh_start(void);
int shackled_vh_start(void);
int srdarwin_vh_start(void);
int gondo_vh_start(void);
int garyoret_vh_start(void);

WRITE_HANDLER( dec8_bac06_0_w );
WRITE_HANDLER( dec8_bac06_1_w );
WRITE_HANDLER( dec8_pf0_data_w );
WRITE_HANDLER( dec8_pf1_data_w );
READ_HANDLER( dec8_pf0_data_r );
READ_HANDLER( dec8_pf1_data_r );
WRITE_HANDLER( srdarwin_videoram_w );
WRITE_HANDLER( dec8_scroll1_w );
WRITE_HANDLER( dec8_scroll2_w );
WRITE_HANDLER( srdarwin_control_w );
WRITE_HANDLER( gondo_scroll_w );
WRITE_HANDLER( lastmiss_control_w );
WRITE_HANDLER( lastmiss_scrollx_w );
WRITE_HANDLER( lastmiss_scrolly_w );
WRITE_HANDLER( dec8_bac06_0_w );
WRITE_HANDLER( dec8_bac06_1_w );
WRITE_HANDLER( dec8_videoram_w );

/******************************************************************************/

static unsigned char *dec8_shared_ram,*dec8_shared2_ram;
extern unsigned char *dec8_pf0_data,*dec8_pf1_data,*dec8_row;

static int nmi_enable,int_enable;
static int i8751_return, i8751_value;
static int msm5205next;

/******************************************************************************/

/* Only used by ghostb, gondo, garyoret, other games can control buffering */
static void dec8_eof_callback(void)
{
	buffer_spriteram_w(0,0);
}

static READ_HANDLER( i8751_h_r )
{
	return i8751_return>>8; /* MSB */
}

static READ_HANDLER( i8751_l_r )
{
	return i8751_return&0xff; /* LSB */
}

static WRITE_HANDLER( i8751_reset_w )
{
	i8751_return=0;
}

/******************************************************************************/

static READ_HANDLER( gondo_player_1_r )
{
	switch (offset) {
		case 0: /* Rotary low byte */
			return ~((1 << (readinputport(5) * 12 / 256))&0xff);
		case 1: /* Joystick = bottom 4 bits, rotary = top 4 */
			return ((~((1 << (readinputport(5) * 12 / 256))>>4))&0xf0) | (readinputport(0)&0xf);
	}
	return 0xff;
}

static READ_HANDLER( gondo_player_2_r )
{
	switch (offset) {
		case 0: /* Rotary low byte */
			return ~((1 << (readinputport(6) * 12 / 256))&0xff);
		case 1: /* Joystick = bottom 4 bits, rotary = top 4 */
			return ((~((1 << (readinputport(6) * 12 / 256))>>4))&0xf0) | (readinputport(1)&0xf);
	}
	return 0xff;
}

/******************************************************************************/

static WRITE_HANDLER( ghostb_i8751_w )
{
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	if (i8751_value==0x00aa) i8751_return=0x655;
	if (i8751_value==0x021a) i8751_return=0x6e5; /* Ghostbusters ID */
	if (i8751_value==0x021b) i8751_return=0x6e4; /* Meikyuu Hunter G ID */
}

static WRITE_HANDLER( srdarwin_i8751_w )
{
	static int coins,latch;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	if (i8751_value==0x0000) {i8751_return=0;coins=0;}
	if (i8751_value==0x3063) i8751_return=0x9c; /* Protection */
	if ((i8751_value&0xff00)==0x4000) i8751_return=i8751_value; /* Coinage settings */
 	if (i8751_value==0x5000) i8751_return=((coins / 10) << 4) | (coins % 10); /* Coin request */
 	if (i8751_value==0x6000) {i8751_value=-1; coins--; } /* Coin clear */
	/* Nb:  Command 0x4000 for setting coinage options is not supported */
 	if ((readinputport(4)&1)==1) latch=1;
 	if ((readinputport(4)&1)!=1 && latch) {coins++; latch=0;}

	/* This next value is the index to a series of tables,
	each table controls the end of level bad guy, wrong values crash the
	cpu right away via a bogus jump.

	Level number requested is in low byte

	Addresses on left hand side are from the protection vector table which is
	stored at location 0xf580 in rom dy_01.rom

ba5e (lda #00) = Level 0?
ba82 (lda #01) = Pyramid boss, Level 1?
baaa           = No boss appears, game hangs
bacc (lda #04) = Killer Bee boss, Level 4?
bae0 (lda #03) = Snake type boss, Level 3?
baf9           = Double grey thing boss...!
bb0a      	   = Single grey thing boss!
bb18 (lda #00) = Hailstorm from top of screen.
bb31 (lda #28) = Small hailstorm
bb47 (ldb #05) = Small hailstorm
bb5a (lda #08) = Weird square things..
bb63           = Square things again
(24)           = Another square things boss..
(26)           = Clock boss! (level 3)
(28)           = Big dragon boss, perhaps an end-of-game baddy
(30)           = 4 things appear at corners, seems to fit with attract mode (level 1)
(32)           = Grey things teleport onto screen..
(34)           = Grey thing appears in middle of screen
(36)           = As above
(38)           = Circle thing with two pincers
(40)           = Grey bird
(42)           = Crash (end of table)

	The table below is hopefully correct thanks to José Miguel Morales Farreras,
	but Boss #6 is uncomfirmed as correct.

*/
	if (i8751_value==0x8000) i8751_return=0xf580 +  0; /* Boss #1: Snake + Bee */
	if (i8751_value==0x8001) i8751_return=0xf580 + 30; /* Boss #2: 4 Corners */
	if (i8751_value==0x8002) i8751_return=0xf580 + 26; /* Boss #3: Clock */
	if (i8751_value==0x8003) i8751_return=0xf580 +  6; /* Boss #4: Pyramid */
	if (i8751_value==0x8004) i8751_return=0xf580 + 12; /* Boss #5: Grey things */
	if (i8751_value==0x8005) i8751_return=0xf580 + 20; /* Boss #6: Ground Base?! */
	if (i8751_value==0x8006) i8751_return=0xf580 + 28; /* Boss #7: Dragon */
	if (i8751_value==0x8007) i8751_return=0xf580 + 32; /* Boss #8: Teleport */
	if (i8751_value==0x8008) i8751_return=0xf580 + 38; /* Boss #9: Octopus (Pincer) */
	if (i8751_value==0x8009) i8751_return=0xf580 + 40; /* Boss #10: Bird */
}

static WRITE_HANDLER( gondo_i8751_w )
{
	static int coin1,coin2,latch,snd;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		if (int_enable) cpu_cause_interrupt (0, M6809_INT_IRQ); /* IRQ on *high* byte only */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(4)&3)==3) latch=1;
 	if ((readinputport(4)&1)!=1 && latch) {coin1++; snd=1; latch=0;}
 	if ((readinputport(4)&2)!=2 && latch) {coin2++; snd=1; latch=0;}

	/* Work out return values */
	if (i8751_value==0x0000) {i8751_return=0; coin1=coin2=snd=0;}
	if (i8751_value==0x038a)  i8751_return=0x375; /* Makyou Senshi ID */
	if (i8751_value==0x038b)  i8751_return=0x374; /* Gondomania ID */
	if ((i8751_value>>8)==0x04)  i8751_return=0x40f; /* Coinage settings (Not supported) */
	if ((i8751_value>>8)==0x05) {i8751_return=0x500 | ((coin1 / 10) << 4) | (coin1 % 10);  } /* Coin 1 */
	if ((i8751_value>>8)==0x06 && coin1 && !offset) {i8751_return=0x600; coin1--; } /* Coin 1 clear */
	if ((i8751_value>>8)==0x07) {i8751_return=0x700 | ((coin2 / 10) << 4) | (coin2 % 10);  } /* Coin 2 */
	if ((i8751_value>>8)==0x08 && coin2 && !offset) {i8751_return=0x800; coin2--; } /* Coin 2 clear */
	/* Commands 0x9xx do nothing */
	if ((i8751_value>>8)==0x0a) {i8751_return=0xa00 | snd; if (snd) snd=0; }
}

static WRITE_HANDLER( shackled_i8751_w )
{
	static int coin1,coin2,latch=0;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		cpu_cause_interrupt (1, M6809_INT_FIRQ); /* Signal main cpu */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if (/*(readinputport(2)&3)==3*/!latch) {latch=1;coin1=coin2=0;}
 	if ((readinputport(2)&1)!=1 && latch) {coin1=1; latch=0;}
 	if ((readinputport(2)&2)!=2 && latch) {coin2=1; latch=0;}

	if (i8751_value==0x0050) i8751_return=0; /* Breywood ID */
	if (i8751_value==0x0051) i8751_return=0; /* Shackled ID */
	if (i8751_value==0x0102) i8751_return=0; /* ?? */
	if (i8751_value==0x0101) i8751_return=0; /* ?? */
	if (i8751_value==0x8101) i8751_return=((coin2 / 10) << 4) | (coin2 % 10) |
			((((coin1 / 10) << 4) | (coin1 % 10))<<8); /* Coins */
}

static WRITE_HANDLER( lastmiss_i8751_w )
{
	static int coin,latch=0,snd;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		cpu_cause_interrupt (0, M6809_INT_FIRQ); /* Signal main cpu */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(2)&3)==3 && !latch) latch=1;
 	if ((readinputport(2)&3)!=3 && latch) {coin++; latch=0;snd=0x400;i8751_return=0x400;return;}

	if (i8751_value==0x007b) i8751_return=0x0184; //???
	if (i8751_value==0x0000) {i8751_return=0x0184; coin=snd=0;}//???
	if (i8751_value==0x0401) i8751_return=0x0184; //???
	if ((i8751_value>>8)==0x01) i8751_return=0x0184; /* Coinage setup */
	if ((i8751_value>>8)==0x02) {i8751_return=snd | ((coin / 10) << 4) | (coin % 10); snd=0;} /* Coin return */
	if ((i8751_value>>8)==0x03) {i8751_return=0; coin--; } /* Coin clear */
}

static WRITE_HANDLER( csilver_i8751_w )
{
	static int coin,latch=0,snd;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		cpu_cause_interrupt (0, M6809_INT_FIRQ); /* Signal main cpu */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(2)&3)==3 && !latch) latch=1;
 	if ((readinputport(2)&3)!=3 && latch) {coin++; latch=0;snd=0x1200; i8751_return=0x1200;return;}

	if (i8751_value==0x054a) {i8751_return=~(0x4a); coin=0; snd=0;} /* Captain Silver ID */
	if ((i8751_value>>8)==0x01) i8751_return=0; /* Coinage - Not Supported */
	if ((i8751_value>>8)==0x02) {i8751_return=snd | coin; snd=0; } /* Coin Return */
	if (i8751_value==0x0003 && coin) {i8751_return=0; coin--;} /* Coin Clear */
}

static WRITE_HANDLER( garyoret_i8751_w )
{
	static int coin1,coin2,latch;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(2)&3)==3) latch=1;
 	if ((readinputport(2)&1)!=1 && latch) {coin1++; latch=0;}
 	if ((readinputport(2)&2)!=2 && latch) {coin2++; latch=0;}

	/* Work out return values */
	if ((i8751_value>>8)==0x00) {i8751_return=0; coin1=coin2=0;}
	if ((i8751_value>>8)==0x01)  i8751_return=0x59a; /* ID */
	if ((i8751_value>>8)==0x04)  i8751_return=i8751_value; /* Coinage settings (Not supported) */
	if ((i8751_value>>8)==0x05) {i8751_return=0x00 | ((coin1 / 10) << 4) | (coin1 % 10);  } /* Coin 1 */
	if ((i8751_value>>8)==0x06 && coin1 && !offset) {i8751_return=0x600; coin1--; } /* Coin 1 clear */
}

/******************************************************************************/

static WRITE_HANDLER( dec8_bank_w )
{
 	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + (data & 0x0f) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);
}

/* Used by Ghostbusters, Meikyuu Hunter G & Gondomania */
static WRITE_HANDLER( ghostb_bank_w )
{
 	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* Bit 0: Interrupt enable/disable (I think..)
	   Bit 1: NMI enable/disable
	   Bit 2: ??
	   Bit 3: Screen flip
	   Bits 4-7: Bank switch
	*/

	bankaddress = 0x10000 + (data >> 4) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);

	if (data&1) int_enable=1; else int_enable=0;
	if (data&2) nmi_enable=1; else nmi_enable=0;
	flip_screen_w(0,data & 0x08);
}

WRITE_HANDLER( csilver_control_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* Bottom 4 bits - bank switch */
	bankaddress = 0x10000 + (data & 0x0f) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);

	/* There are unknown bits in the top half of the byte! */
 //logerror("PC %06x - Write %02x to %04x\n",cpu_get_pc(),data,offset+0x1802);
}

static WRITE_HANDLER( dec8_sound_w )
{
 	soundlatch_w(0,data);
	cpu_cause_interrupt(1,M6502_INT_NMI);
}

static WRITE_HANDLER( oscar_sound_w )
{
 	soundlatch_w(0,data);
	cpu_cause_interrupt(2,M6502_INT_NMI);
}

static void csilver_adpcm_int(int data)
{
	static int toggle =0;

	toggle ^= 1;
	if (toggle)
		cpu_cause_interrupt(2,M6502_INT_IRQ);

	MSM5205_data_w (0,msm5205next>>4);
	msm5205next<<=4;
}

static READ_HANDLER( csilver_adpcm_reset_r )
{
	MSM5205_reset_w(0,0);
	return 0;
}

static WRITE_HANDLER( csilver_adpcm_data_w )
{
	msm5205next = data;
}

static WRITE_HANDLER( csilver_sound_bank_w )
{
	unsigned char *RAM = memory_region(REGION_CPU3);

	if (data&8) { cpu_setbank(3,&RAM[0x14000]); }
	else { cpu_setbank(3,&RAM[0x10000]); }
}

/******************************************************************************/

static WRITE_HANDLER( oscar_int_w )
{
	/* Deal with interrupts, coins also generate NMI to CPU 0 */
	switch (offset) {
		case 0: /* IRQ2 */
			cpu_cause_interrupt (1, M6809_INT_IRQ);
			return;
		case 1: /* IRC 1 */
			return;
		case 2: /* IRQ 1 */
			cpu_cause_interrupt (0, M6809_INT_IRQ);
			return;
		case 3: /* IRC 2 */
			return;
	}
}

/* Used by Shackled, Last Mission, Captain Silver */
static WRITE_HANDLER( shackled_int_w )
{
	switch (offset) {
		case 0: /* CPU 2 - IRQ acknowledge */
            return;
        case 1: /* CPU 1 - IRQ acknowledge */
        	return;
        case 2: /* i8751 - FIRQ acknowledge */
            return;
        case 3: /* IRQ 1 */
			cpu_cause_interrupt (0, M6809_INT_IRQ);
			return;
        case 4: /* IRQ 2 */
            cpu_cause_interrupt (1, M6809_INT_IRQ);
            return;
	}
}

/******************************************************************************/

static READ_HANDLER( dec8_share_r ) { return dec8_shared_ram[offset]; }
static READ_HANDLER( dec8_share2_r ) { return dec8_shared2_ram[offset]; }
static WRITE_HANDLER( dec8_share_w ) { dec8_shared_ram[offset]=data; }
static WRITE_HANDLER( dec8_share2_w ) { dec8_shared2_ram[offset]=data; }
static READ_HANDLER( shackled_sprite_r ) { return spriteram[offset]; }
static WRITE_HANDLER( shackled_sprite_w ) { spriteram[offset]=data; }

/******************************************************************************/

static struct MemoryReadAddress cobra_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0fff, dec8_pf0_data_r },
	{ 0x1000, 0x17ff, dec8_pf1_data_r },
	{ 0x1800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x31ff, paletteram_r },
	{ 0x3800, 0x3800, input_port_0_r }, /* Player 1 */
	{ 0x3801, 0x3801, input_port_1_r }, /* Player 2 */
	{ 0x3802, 0x3802, input_port_3_r }, /* Dip 1 */
	{ 0x3803, 0x3803, input_port_4_r }, /* Dip 2 */
	{ 0x3a00, 0x3a00, input_port_2_r }, /* VBL & coins */
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cobra_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x1000, 0x17ff, dec8_pf1_data_w, &dec8_pf1_data },
	{ 0x1800, 0x1fff, MWA_RAM },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x31ff, paletteram_xxxxBBBBGGGGRRRR_swap_w, &paletteram },
	{ 0x3200, 0x37ff, MWA_RAM }, /* Unused */
	{ 0x3800, 0x381f, dec8_bac06_0_w },
	{ 0x3a00, 0x3a1f, dec8_bac06_1_w },
	{ 0x3c00, 0x3c00, dec8_bank_w },
	{ 0x3c02, 0x3c02, buffer_spriteram_w }, /* DMA */
	{ 0x3e00, 0x3e00, dec8_sound_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress ghostb_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x1800, 0x1fff, videoram_r },
	{ 0x2000, 0x27ff, dec8_pf0_data_r },
	{ 0x2800, 0x2dff, MRA_RAM },
	{ 0x3000, 0x37ff, MRA_RAM },
	{ 0x3800, 0x3800, input_port_0_r }, /* Player 1 */
	{ 0x3801, 0x3801, input_port_1_r }, /* Player 2 */
	{ 0x3802, 0x3802, input_port_2_r }, /* Player 3 */
	{ 0x3803, 0x3803, input_port_3_r }, /* Start buttons + VBL */
	{ 0x3820, 0x3820, input_port_5_r }, /* Dip */
	{ 0x3840, 0x3840, i8751_h_r },
	{ 0x3860, 0x3860, i8751_l_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress ghostb_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x27ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x2800, 0x2bff, MWA_RAM }, /* Scratch ram for rowscroll? */
	{ 0x2c00, 0x2dff, MWA_RAM, &dec8_row },
	{ 0x2e00, 0x2fff, MWA_RAM }, /* Unused */
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3800, 0x3800, dec8_sound_w },
	{ 0x3820, 0x383f, dec8_bac06_0_w },
	{ 0x3840, 0x3840, ghostb_bank_w },
	{ 0x3860, 0x3861, ghostb_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress srdarwin_readmem[] =
{
	{ 0x0000, 0x13ff, MRA_RAM },
	{ 0x1400, 0x17ff, dec8_pf0_data_r },
	{ 0x2000, 0x2000, i8751_h_r },
	{ 0x2001, 0x2001, i8751_l_r },
	{ 0x3800, 0x3800, input_port_2_r }, /* Dip 1 */
	{ 0x3801, 0x3801, input_port_0_r }, /* Player 1 */
	{ 0x3802, 0x3802, input_port_1_r }, /* Player 2 (cocktail) + VBL */
	{ 0x3803, 0x3803, input_port_3_r }, /* Dip 2 */
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress srdarwin_writemem[] =
{
	{ 0x0000, 0x05ff, MWA_RAM },
	{ 0x0600, 0x07ff, MWA_RAM, &spriteram },
	{ 0x0800, 0x0fff, srdarwin_videoram_w, &videoram, &spriteram_size },
	{ 0x1000, 0x13ff, MWA_RAM },
	{ 0x1400, 0x17ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x1800, 0x1801, srdarwin_i8751_w },
	{ 0x1802, 0x1802, i8751_reset_w },		/* Maybe.. */
	{ 0x1803, 0x1803, MWA_NOP },            /* NMI ack */
	{ 0x1804, 0x1804, buffer_spriteram_w }, /* DMA */
	{ 0x1805, 0x1806, srdarwin_control_w }, /* Scroll & Bank */
	{ 0x2000, 0x2000, dec8_sound_w },       /* Sound */
	{ 0x2001, 0x2001, flip_screen_w },  /* Flipscreen */
	{ 0x2800, 0x288f, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x3000, 0x308f, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress gondo_readmem[] =
{
	{ 0x0000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1fff, videoram_r },
	{ 0x2000, 0x27ff, dec8_pf0_data_r },
	{ 0x2800, 0x2bff, paletteram_r },
	{ 0x2c00, 0x2fff, paletteram_2_r },
	{ 0x3000, 0x37ff, MRA_RAM },          /* Sprites */
	{ 0x3800, 0x3800, input_port_7_r },   /* Dip 1 */
	{ 0x3801, 0x3801, input_port_8_r },   /* Dip 2 */
	{ 0x380a, 0x380b, gondo_player_1_r }, /* Player 1 rotary */
	{ 0x380c, 0x380d, gondo_player_2_r }, /* Player 2 rotary */
	{ 0x380e, 0x380e, input_port_3_r },   /* VBL */
	{ 0x380f, 0x380f, input_port_2_r },   /* Fire buttons */
	{ 0x3838, 0x3838, i8751_h_r },
	{ 0x3839, 0x3839, i8751_l_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress gondo_writemem[] =
{
	{ 0x0000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x27ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x2800, 0x2bff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x2c00, 0x2fff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3810, 0x3810, dec8_sound_w },
	{ 0x3818, 0x382f, gondo_scroll_w },
	{ 0x3830, 0x3830, ghostb_bank_w }, /* Bank + NMI enable */
	{ 0x383a, 0x383b, gondo_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress oscar_readmem[] =
{
	{ 0x0000, 0x0eff, dec8_share_r },
	{ 0x0f00, 0x0fff, MRA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, dec8_pf0_data_r },
	{ 0x3000, 0x37ff, MRA_RAM }, /* Sprites */
	{ 0x3800, 0x3bff, paletteram_r },
	{ 0x3c00, 0x3c00, input_port_0_r },
	{ 0x3c01, 0x3c01, input_port_1_r },
	{ 0x3c02, 0x3c02, input_port_2_r }, /* VBL & coins */
	{ 0x3c03, 0x3c03, input_port_3_r }, /* Dip 1 */
	{ 0x3c04, 0x3c04, input_port_4_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress oscar_writemem[] =
{
	{ 0x0000, 0x0eff, dec8_share_w, &dec8_shared_ram },
	{ 0x0f00, 0x0fff, MWA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3800, 0x3bff, paletteram_xxxxBBBBGGGGRRRR_swap_w, &paletteram },
	{ 0x3c00, 0x3c1f, dec8_bac06_0_w },
	{ 0x3c80, 0x3c80, buffer_spriteram_w },	/* DMA */
	{ 0x3d00, 0x3d00, dec8_bank_w },   		/* BNKS */
	{ 0x3d80, 0x3d80, oscar_sound_w }, 		/* SOUN */
	{ 0x3e00, 0x3e00, MWA_NOP },       		/* COINCL */
	{ 0x3e80, 0x3e83, oscar_int_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress oscar_sub_readmem[] =
{
	{ 0x0000, 0x0eff, dec8_share_r },
	{ 0x0f00, 0x0fff, MRA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress oscar_sub_writemem[] =
{
	{ 0x0000, 0x0eff, dec8_share_w },
	{ 0x0f00, 0x0fff, MWA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_w },
	{ 0x3e80, 0x3e83, oscar_int_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress lastmiss_readmem[] =
{
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r }, /* Dip 1 */
	{ 0x1804, 0x1804, input_port_4_r }, /* Dip 2 */
	{ 0x1806, 0x1806, i8751_h_r },
	{ 0x1807, 0x1807, i8751_l_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress lastmiss_writemem[] =
{
	{ 0x0000, 0x0fff, dec8_share_w, &dec8_shared_ram },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1809, 0x1809, lastmiss_scrollx_w }, /* Scroll LSB */
	{ 0x180b, 0x180b, lastmiss_scrolly_w }, /* Scroll LSB */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, lastmiss_control_w }, /* Bank switch + Scroll MSB */
	{ 0x180e, 0x180f, lastmiss_i8751_w },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x37ff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x3800, 0x3fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress lastmiss_sub_readmem[] =
{
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r }, /* Dip 1 */
	{ 0x1804, 0x1804, input_port_4_r }, /* Dip 2 */
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress lastmiss_sub_writemem[] =
{
	{ 0x0000, 0x0fff, dec8_share_w },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x2000, 0x27ff, dec8_videoram_w },
	{ 0x2800, 0x2fff, shackled_sprite_w },
	{ 0x3000, 0x37ff, dec8_share2_w },
	{ 0x3800, 0x3fff, dec8_pf0_data_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress shackled_readmem[] =
{
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r },
	{ 0x1804, 0x1804, input_port_4_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, shackled_sprite_r },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress shackled_writemem[] =
{
	{ 0x0000, 0x0fff, dec8_share_w, &dec8_shared_ram },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1809, 0x1809, lastmiss_scrollx_w }, /* Scroll LSB */
	{ 0x180b, 0x180b, lastmiss_scrolly_w }, /* Scroll LSB */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, lastmiss_control_w }, /* Bank switch + Scroll MSB */
	{ 0x2000, 0x27ff, dec8_videoram_w },
	{ 0x2800, 0x2fff, shackled_sprite_w },
	{ 0x3000, 0x37ff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x3800, 0x3fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress shackled_sub_readmem[] =
{
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r },
	{ 0x1804, 0x1804, input_port_4_r },
	{ 0x1806, 0x1806, i8751_h_r },
	{ 0x1807, 0x1807, i8751_l_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress shackled_sub_writemem[] =
{
	{ 0x0000, 0x0fff, dec8_share_w },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1809, 0x1809, lastmiss_scrollx_w }, /* Scroll LSB */
	{ 0x180b, 0x180b, lastmiss_scrolly_w }, /* Scroll LSB */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, lastmiss_control_w }, /* Bank switch + Scroll MSB */
	{ 0x180e, 0x180f, shackled_i8751_w },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x37ff, dec8_share2_w },
	{ 0x3800, 0x3fff, dec8_pf0_data_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress csilver_readmem[] =
{
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_1_r },
	{ 0x1801, 0x1801, input_port_0_r },
	{ 0x1803, 0x1803, input_port_2_r },
	{ 0x1804, 0x1804, input_port_4_r }, /* Dip 2 */
	{ 0x1805, 0x1805, input_port_3_r }, /* Dip 1 */
	{ 0x1c00, 0x1c00, i8751_h_r },
	{ 0x1e00, 0x1e00, i8751_l_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, shackled_sprite_r },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress csilver_writemem[] =
{
	{ 0x0000, 0x0fff, dec8_share_w, &dec8_shared_ram },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1808, 0x180b, dec8_scroll2_w },
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, csilver_control_w },
	{ 0x180e, 0x180f, csilver_i8751_w },
	{ 0x2000, 0x27ff, dec8_videoram_w },
	{ 0x2800, 0x2fff, shackled_sprite_w },
	{ 0x3000, 0x37ff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x3800, 0x3fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress csilver_sub_readmem[] =
{
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
//	{ 0x1800, 0x1800, input_port_0_r },
//	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1803, 0x1803, input_port_2_r },
	{ 0x1804, 0x1804, input_port_4_r },
	{ 0x1805, 0x1805, input_port_3_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress csilver_sub_writemem[] =
{
	{ 0x0000, 0x0fff, dec8_share_w },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, lastmiss_control_w }, /* Bank switch + Scroll MSB */
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x37ff, dec8_share2_w },
	{ 0x3800, 0x3fff, dec8_pf0_data_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress garyoret_readmem[] =
{
	{ 0x0000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1fff, videoram_r },
	{ 0x2000, 0x27ff, dec8_pf0_data_r },
	{ 0x2800, 0x2bff, paletteram_r },
	{ 0x2c00, 0x2fff, paletteram_2_r },
	{ 0x3000, 0x37ff, MRA_RAM },          /* Sprites */
	{ 0x3800, 0x3800, input_port_3_r },   /* Dip 1 */
	{ 0x3801, 0x3801, input_port_4_r },   /* Dip 2 */
	{ 0x3808, 0x3808, MRA_NOP },          /* ? */
	{ 0x380a, 0x380a, input_port_1_r },   /* Player 2 + VBL */
	{ 0x380b, 0x380b, input_port_0_r },   /* Player 1 */
	{ 0x383a, 0x383a, i8751_h_r },
	{ 0x383b, 0x383b, i8751_l_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress garyoret_writemem[] =
{
	{ 0x0000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x27ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x2800, 0x2bff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x2c00, 0x2fff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3810, 0x3810, dec8_sound_w },
	{ 0x3818, 0x382f, gondo_scroll_w },
	{ 0x3830, 0x3830, ghostb_bank_w }, /* Bank + NMI enable */
	{ 0x3838, 0x3839, garyoret_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/******************************************************************************/

/* Used for Cobra Command, Maze Hunter, Super Real Darwin, Gondomania, etc */
static struct MemoryReadAddress dec8_s_readmem[] =
{
	{ 0x0000, 0x05ff, MRA_RAM},
	{ 0x6000, 0x6000, soundlatch_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress dec8_s_writemem[] =
{
	{ 0x0000, 0x05ff, MWA_RAM},
	{ 0x2000, 0x2000, YM2203_control_port_0_w }, /* OPN */
	{ 0x2001, 0x2001, YM2203_write_port_0_w },
	{ 0x4000, 0x4000, YM3812_control_port_0_w }, /* OPL */
	{ 0x4001, 0x4001, YM3812_write_port_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/* Used by Last Mission, Shackled & Breywood */
static struct MemoryReadAddress ym3526_s_readmem[] =
{
	{ 0x0000, 0x05ff, MRA_RAM},
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress ym3526_s_writemem[] =
{
	{ 0x0000, 0x05ff, MWA_RAM},
	{ 0x0800, 0x0800, YM2203_control_port_0_w }, /* OPN */
	{ 0x0801, 0x0801, YM2203_write_port_0_w },
	{ 0x1000, 0x1000, YM3526_control_port_0_w }, /* OPL? */
	{ 0x1001, 0x1001, YM3526_write_port_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/* Captain Silver - same sound system as Pocket Gal */
static struct MemoryReadAddress csilver_s_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x3400, 0x3400, csilver_adpcm_reset_r },	/* ? not sure */
	{ 0x4000, 0x7fff, MRA_BANK3 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress csilver_s_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0800, YM2203_control_port_0_w },
	{ 0x0801, 0x0801, YM2203_write_port_0_w },
	{ 0x1000, 0x1000, YM3812_control_port_0_w },
	{ 0x1001, 0x1001, YM3812_write_port_0_w },
	{ 0x1800, 0x1800, csilver_adpcm_data_w },	/* ADPCM data for the MSM5205 chip */
	{ 0x2000, 0x2000, csilver_sound_bank_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

/******************************************************************************/

#define PLAYER1_JOYSTICK /* Player 1 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )

#define PLAYER2_JOYSTICK /* Player 2 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )

INPUT_PORTS_START( cobracom )
	PORT_START /* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ghostb )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 3 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dummy input for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* Dip switch */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, "Scene Time" )
	PORT_DIPSETTING(    0x00, "4.00" )
	PORT_DIPSETTING(    0x10, "4.30" )
	PORT_DIPSETTING(    0x30, "5.00" )
	PORT_DIPSETTING(    0x20, "6.00" )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Beam Energy Pickup" ) /* Ghostb only */
	PORT_DIPSETTING(    0x00, "Up 1.5%" )
	PORT_DIPSETTING(    0x80, "Normal" )
INPUT_PORTS_END

INPUT_PORTS_START( meikyuh )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 3 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* Dummy input for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* Dip switch */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( srdarwin )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "28", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Continues" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) /* Fake */
INPUT_PORTS_END

INPUT_PORTS_START( gondo )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	/* Top 4 bits are rotary controller */

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	/* Top 4 bits are rotary controller */

 	PORT_START	/* Player 1 & 2 fire buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* Fake port for the i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* player 1 12-way rotary control */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, JOYCODE_1_BUTTON5, JOYCODE_1_BUTTON6, 0, 0 )

	PORT_START	/* player 2 12-way rotary control */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 10, 0, 0, JOYCODE_2_BUTTON5, JOYCODE_2_BUTTON6, 0, 0 )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( oscar )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

 	PORT_START	/* Player 2 controls */
	//PLAYER2_JOYSTICK
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x20, 0x20, "Demo Freeze Mode" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "Every 40000" )
	PORT_DIPSETTING(    0x20, "Every 60000" )
	PORT_DIPSETTING(    0x10, "Every 90000" )
	PORT_DIPSETTING(    0x00, "50000 only" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) ) //invincible?
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( lastmiss )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Cabinet?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue?" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( shackled )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )	/* game doesn't boot when this is On */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( csilver )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( garyoret )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* Fake port for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout_32k =
{
	8,8,
	1024,
	2,
	{ 0x4000*8,0x0000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout chars_3bpp =
{
	8,8,
	1024,
	3,
	{ 0x6000*8,0x4000*8,0x2000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

/* SRDarwin characters - very unusual layout for Data East */
static struct GfxLayout charlayout_16k =
{
	8,8,	/* 8*8 characters */
	1024,
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0x2000*8+0, 0x2000*8+1, 0x2000*8+2, 0x2000*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout oscar_charlayout =
{
	8,8,
	1024,
	3,
	{ 0x3000*8,0x2000*8,0x1000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

/* Darwin sprites - only 3bpp */
static struct GfxLayout sr_sprites =
{
	16,16,
	2048,
	3,
 	{ 0x10000*8,0x20000*8,0x00000*8 },
	{ 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8),
		0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 },
	16*16
};

static struct GfxLayout srdarwin_tiles =
{
	16,16,
	256,
	4,
	{ 0x8000*8, 0x8000*8+4, 0, 4 },
	{ 0, 1, 2, 3, 1024*8*8+0, 1024*8*8+1, 1024*8*8+2, 1024*8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+1024*8*8+0, 16*8+1024*8*8+1, 16*8+1024*8*8+2, 16*8+1024*8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every tile takes 32 consecutive bytes */
};

static struct GfxLayout tiles =
{
	16,16,
	4096,
	4,
 	{ 0x60000*8,0x40000*8,0x20000*8,0x00000*8 },
	{ 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8),
		0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	16*16
};

/* X flipped on Ghostbusters tiles */
static struct GfxLayout tiles_r =
{
	16,16,
	2048,
	4,
 	{ 0x20000*8,0x00000*8,0x30000*8,0x10000*8 },
	{ 7,6,5,4,3,2,1,0,
		7+(16*8), 6+(16*8), 5+(16*8), 4+(16*8), 3+(16*8), 2+(16*8), 1+(16*8), 0+(16*8) },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	16*16
};

static struct GfxDecodeInfo cobracom_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_32k, 0, 8 },
	{ REGION_GFX2, 0, &tiles,         64, 4 },
	{ REGION_GFX3, 0, &tiles,        192, 4 },
	{ REGION_GFX4, 0, &tiles,        128, 4 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ghostb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars_3bpp,	0,  4 },
	{ REGION_GFX2, 0, &tiles,     256, 16 },
	{ REGION_GFX3, 0, &tiles_r,   512, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo srdarwin_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout_16k,128, 4 }, /* Only 1 used so far :/ */
	{ REGION_GFX2, 0x00000, &sr_sprites,     64, 8 },
	{ REGION_GFX3, 0x00000, &srdarwin_tiles,  0, 8 },
  	{ REGION_GFX3, 0x10000, &srdarwin_tiles,  0, 8 },
    { REGION_GFX3, 0x20000, &srdarwin_tiles,  0, 8 },
    { REGION_GFX3, 0x30000, &srdarwin_tiles,  0, 8 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gondo_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars_3bpp,  0, 16 }, /* Chars */
	{ REGION_GFX2, 0, &tiles,     256, 32 }, /* Sprites */
	{ REGION_GFX3, 0, &tiles,     768, 16 }, /* Tiles */
 	{ -1 } /* end of array */
};

static struct GfxDecodeInfo oscar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &oscar_charlayout, 256,  8 }, /* Chars */
	{ REGION_GFX2, 0, &tiles,              0, 16 }, /* Sprites */
	{ REGION_GFX3, 0, &tiles,            384,  8 }, /* Tiles */
 	{ -1 } /* end of array */
};

static struct GfxDecodeInfo shackled_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars_3bpp,   0,  4 },
	{ REGION_GFX2, 0, &tiles,      256, 16 },
	{ REGION_GFX3, 0, &tiles,      768, 16 },
 	{ -1 } /* end of array */
};

/******************************************************************************/

static struct YM2203interface ym2203_interface =
{
	1,
	1500000,	/* Should be accurate for all games, derived from 12MHz crystal */
	{ YM2203_VOL(20,23) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

/* handler called by the 3812 emulator when the internal timers cause an IRQ */
static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,0,linestate); /* M6502_INT_IRQ */
}

static void oscar_irqhandler(int linestate)
{
	cpu_set_irq_line(2,0,linestate); /* M6502_INT_IRQ */
}

static struct YM3526interface ym3526_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 35 },
	{ irqhandler },
};

static struct YM3526interface oscar_ym3526_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 35 },
	{ oscar_irqhandler },
};

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 35 },
	{ irqhandler },
};

static struct MSM5205interface msm5205_interface =
{
	1,					/* 1 chip             */
	384000,				/* 384KHz             */
	{ csilver_adpcm_int },/* interrupt function */
	{ MSM5205_S48_4B },	/* 8KHz               */
	{ 88 }
};

/******************************************************************************/

static int ghostb_interrupt(void)
{
	static int latch[4];
	int i8751_out=readinputport(4);

	/* Ghostbusters coins are controlled by the i8751 */
	if ((i8751_out & 0x8) == 0x8) latch[0]=1;
	if ((i8751_out & 0x4) == 0x4) latch[1]=1;
	if ((i8751_out & 0x2) == 0x2) latch[2]=1;
	if ((i8751_out & 0x1) == 0x1) latch[3]=1;

	if (((i8751_out & 0x8) != 0x8) && latch[0]) {latch[0]=0; cpu_cause_interrupt(0,M6809_INT_IRQ); i8751_return=0x8001; } /* Player 1 coin */
	if (((i8751_out & 0x4) != 0x4) && latch[1]) {latch[1]=0; cpu_cause_interrupt(0,M6809_INT_IRQ); i8751_return=0x4001; } /* Player 2 coin */
	if (((i8751_out & 0x2) != 0x2) && latch[2]) {latch[2]=0; cpu_cause_interrupt(0,M6809_INT_IRQ); i8751_return=0x2001; } /* Player 3 coin */
	if (((i8751_out & 0x1) != 0x1) && latch[3]) {latch[3]=0; cpu_cause_interrupt(0,M6809_INT_IRQ); i8751_return=0x1001; } /* Service */

	if (nmi_enable) return M6809_INT_NMI; /* VBL */

	return 0; /* VBL */
}

static int gondo_interrupt(void)
{
	if (nmi_enable)
		return M6809_INT_NMI; /* VBL */

	return 0; /* VBL */
}

/* Coins generate NMI's */
static int oscar_interrupt(void)
{
	static int latch=1;

	if ((readinputport(2) & 0x7) == 0x7) latch=1;
	if (latch && (readinputport(2) & 0x7) != 0x7) {
		latch=0;
    	cpu_cause_interrupt (0, M6809_INT_NMI);
    }

	return 0;
}

/******************************************************************************/

static struct MachineDriver machine_driver_cobracom =
{
	/* basic machine hardware */
	{
 		{
			CPU_M6809,
			2000000,
			cobra_readmem,cobra_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec8_s_readmem,dec8_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3812 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 529, /* 58Hz, 529ms Vblank duration */
	1,
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	cobracom_gfxdecodeinfo,
	256,256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	cobracom_vh_start,
	0,
	cobracom_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		}
	}
};

static struct MachineDriver machine_driver_ghostb =
{
	/* basic machine hardware */
	{
		{
			CPU_HD6309,
			3000000,
			ghostb_readmem,ghostb_writemem,0,0,
			ghostb_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec8_s_readmem,dec8_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3812 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 2500, /* 58Hz, 529ms Vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
  	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	ghostb_gfxdecodeinfo,
	1024,1024,
	ghostb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	dec8_eof_callback,
	ghostb_vh_start,
	0,
	ghostb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		}
	}
};

static struct MachineDriver machine_driver_srdarwin =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,  /* MC68A09EP */
			2000000,
			srdarwin_readmem,srdarwin_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec8_s_readmem,dec8_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3812 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 529, /* 58Hz, 529ms Vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
  	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	srdarwin_gfxdecodeinfo,
	144,144,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	srdarwin_vh_start,
	0,
	srdarwin_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		}
	}
};

static struct MachineDriver machine_driver_gondo =
{
	/* basic machine hardware */
	{
 		{
			CPU_HD6309, /* HD63C09EP */
			3000000,
			gondo_readmem,gondo_writemem,0,0,
			gondo_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec8_s_readmem,dec8_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 529, /* 58Hz, 529ms Vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gondo_gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	dec8_eof_callback,
	gondo_vh_start,
	0,
	gondo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&ym3526_interface
		}
	}
};

static struct MachineDriver machine_driver_oscar =
{
	/* basic machine hardware */
	{
	  	{
			CPU_HD6309,
			2000000,
			oscar_readmem,oscar_writemem,0,0,
			oscar_interrupt,1
		},
	 	{
			CPU_HD6309,
			2000000,
			oscar_sub_readmem,oscar_sub_writemem,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec8_s_readmem,dec8_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 2500, /* 58Hz, 529ms Vblank duration */
	40, /* 40 CPU slices per frame */
	0,	/* init machine */

	/* video hardware */
  	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	oscar_gfxdecodeinfo,
	512,512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	oscar_vh_start,
	0,
	oscar_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&oscar_ym3526_interface
		}
	}
};

static struct MachineDriver machine_driver_lastmiss =
{
	/* basic machine hardware */
	{
  		{
			CPU_M6809,
			2000000,
			lastmiss_readmem,lastmiss_writemem,0,0,
			ignore_interrupt,0
		},
     	{
			CPU_M6809,
			2000000,
			lastmiss_sub_readmem,lastmiss_sub_writemem,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			ym3526_s_readmem,ym3526_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 2500, /* 58Hz, 529ms Vblank duration */
	200,
	0,	/* init machine */

	/* video hardware */
  	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	shackled_gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	lastmiss_vh_start,
	0,
	lastmiss_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&oscar_ym3526_interface
		}
	}
};

static struct MachineDriver machine_driver_shackled =
{
	/* basic machine hardware */
	{
  		{
			CPU_M6809,
			2000000,
			shackled_readmem,shackled_writemem,0,0,
		   	ignore_interrupt,0
		},
     	{
			CPU_M6809,
			2000000,
			shackled_sub_readmem,shackled_sub_writemem,0,0,
			ignore_interrupt,0
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			ym3526_s_readmem,ym3526_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 2500, /* 58Hz, 529ms Vblank duration */
	80,
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	shackled_gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	shackled_vh_start,
	0,
	shackled_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&oscar_ym3526_interface
		}
	}
};

static struct MachineDriver machine_driver_csilver =
{
	/* basic machine hardware */
	{
  		{
			CPU_M6809,
			2000000,
			csilver_readmem,csilver_writemem,0,0,
		   	ignore_interrupt,0
		},
     	{
			CPU_M6809,
			2000000,
			csilver_sub_readmem,csilver_sub_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			csilver_s_readmem,csilver_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the MSM5205 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 529, /* 58Hz, 529ms Vblank duration */
	60,
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	shackled_gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	lastmiss_vh_start,
	0,
	lastmiss_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&oscar_ym3526_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
	    }
	}
};

static struct MachineDriver machine_driver_garyoret =
{
	/* basic machine hardware */
	{
 		{
			CPU_HD6309, /* HD63C09EP */
			3000000,
			garyoret_readmem,garyoret_writemem,0,0,
			gondo_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec8_s_readmem,dec8_s_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
								/* NMIs are caused by the main CPU */
		}
	},
	58, 529, /* 58Hz, 529ms Vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gondo_gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	dec8_eof_callback,
	garyoret_vh_start,
	0,
	garyoret_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&ym3526_interface
		}
	}
};

/******************************************************************************/

ROM_START( cobracom )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "el11-5.bin",  0x08000, 0x08000, 0xaf0a8b05 )
	ROM_LOAD( "el12-4.bin",  0x10000, 0x10000, 0x7a44ef38 )
	ROM_LOAD( "el13.bin",    0x20000, 0x10000, 0x04505acb )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "el10-4.bin",  0x8000,  0x8000,  0xedfad118 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "el14.bin",    0x00000, 0x08000, 0x47246177 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "el00-4.bin",  0x00000, 0x10000, 0x122da2a8 )
	ROM_LOAD( "el01-4.bin",  0x20000, 0x10000, 0x27bf705b )
	ROM_LOAD( "el02-4.bin",  0x40000, 0x10000, 0xc86fede6 )
	ROM_LOAD( "el03-4.bin",  0x60000, 0x10000, 0x1d8a855b )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles 1 */
	ROM_LOAD( "el05.bin",    0x00000, 0x10000, 0x1c4f6033 )
	ROM_LOAD( "el06.bin",    0x20000, 0x10000, 0xd24ba794 )
	ROM_LOAD( "el04.bin",    0x40000, 0x10000, 0xd80a49ce )
	ROM_LOAD( "el07.bin",    0x60000, 0x10000, 0x6d771fc3 )

	ROM_REGION( 0x80000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* tiles 2 */
	ROM_LOAD( "el08.bin",    0x00000, 0x08000, 0xcb0dcf4c )
	ROM_CONTINUE(            0x40000, 0x08000 )
	ROM_LOAD( "el09.bin",    0x20000, 0x08000, 0x1fae5be7 )
	ROM_CONTINUE(            0x60000, 0x08000 )
ROM_END

ROM_START( cobracmj )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "eh-11.rom",    0x08000, 0x08000, 0x868637e1 )
	ROM_LOAD( "eh-12.rom",    0x10000, 0x10000, 0x7c878a83 )
	ROM_LOAD( "el13.bin",     0x20000, 0x10000, 0x04505acb )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "eh-10.rom",    0x8000,  0x8000,  0x62ca5e89 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "el14.bin",    0x00000, 0x08000, 0x47246177 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "eh-00.rom",    0x00000, 0x10000, 0xd96b6797 )
	ROM_LOAD( "eh-01.rom",    0x20000, 0x10000, 0x3fef9c02 )
	ROM_LOAD( "eh-02.rom",    0x40000, 0x10000, 0xbfae6c34 )
	ROM_LOAD( "eh-03.rom",    0x60000, 0x10000, 0xd56790f8 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles 1 */
	ROM_LOAD( "el05.bin",    0x00000, 0x10000, 0x1c4f6033 )
	ROM_LOAD( "el06.bin",    0x20000, 0x10000, 0xd24ba794 )
	ROM_LOAD( "el04.bin",    0x40000, 0x10000, 0xd80a49ce )
	ROM_LOAD( "el07.bin",    0x60000, 0x10000, 0x6d771fc3 )

	ROM_REGION( 0x80000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* tiles 2 */
	ROM_LOAD( "el08.bin",    0x00000, 0x08000, 0xcb0dcf4c )
	ROM_CONTINUE(            0x40000, 0x08000 )
	ROM_LOAD( "el09.bin",    0x20000, 0x08000, 0x1fae5be7 )
	ROM_CONTINUE(            0x60000, 0x08000 )
ROM_END

ROM_START( ghostb )
	ROM_REGION( 0x50000, REGION_CPU1 )
	ROM_LOAD( "dz-01.rom", 0x08000, 0x08000, 0x7c5bb4b1 )
	ROM_LOAD( "dz-02.rom", 0x10000, 0x10000, 0x8e117541 )
	ROM_LOAD( "dz-03.rom", 0x20000, 0x10000, 0x5606a8f4 )
	ROM_LOAD( "dz-04.rom", 0x30000, 0x10000, 0xd09bad99 )
	ROM_LOAD( "dz-05.rom", 0x40000, 0x10000, 0x0315f691 )

	ROM_REGION( 2*0x10000, REGION_CPU2 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dz-06.rom", 0x8000, 0x8000, 0x798f56df )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dz-00.rom", 0x00000, 0x08000, 0x992b4f31 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dz-15.rom", 0x00000, 0x10000, 0xa01a5fd9 )
	ROM_LOAD( "dz-16.rom", 0x10000, 0x10000, 0x5a9a344a )
	ROM_LOAD( "dz-12.rom", 0x20000, 0x10000, 0x817fae99 )
	ROM_LOAD( "dz-14.rom", 0x30000, 0x10000, 0x0abbf76d )
	ROM_LOAD( "dz-11.rom", 0x40000, 0x10000, 0xa5e19c24 )
	ROM_LOAD( "dz-13.rom", 0x50000, 0x10000, 0x3e7c0405 )
	ROM_LOAD( "dz-17.rom", 0x60000, 0x10000, 0x40361b8b )
	ROM_LOAD( "dz-18.rom", 0x70000, 0x10000, 0x8d219489 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dz-07.rom", 0x00000, 0x10000, 0xe7455167 )
	ROM_LOAD( "dz-08.rom", 0x10000, 0x10000, 0x32f9ddfe )
	ROM_LOAD( "dz-09.rom", 0x20000, 0x10000, 0xbb6efc02 )
	ROM_LOAD( "dz-10.rom", 0x30000, 0x10000, 0x6ef9963b )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "dz19a.10d", 0x0000, 0x0400, 0x47e1f83b )
	ROM_LOAD( "dz20a.11d", 0x0400, 0x0400, 0xd8fe2d99 )
ROM_END

ROM_START( ghostb3 )
	ROM_REGION( 0x50000, REGION_CPU1 )
	ROM_LOAD( "dz01-3b",   0x08000, 0x08000, 0xc8cc862a )
	ROM_LOAD( "dz-02.rom", 0x10000, 0x10000, 0x8e117541 )
	ROM_LOAD( "dz-03.rom", 0x20000, 0x10000, 0x5606a8f4 )
	ROM_LOAD( "dz04-1",    0x30000, 0x10000, 0x3c3eb09f )
	ROM_LOAD( "dz05",      0x40000, 0x10000, 0xb4971d33 )

	ROM_REGION( 2*0x10000, REGION_CPU2 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dz-06.rom", 0x8000, 0x8000, 0x798f56df )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dz-00.rom", 0x00000, 0x08000, 0x992b4f31 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dz-15.rom", 0x00000, 0x10000, 0xa01a5fd9 )
	ROM_LOAD( "dz-16.rom", 0x10000, 0x10000, 0x5a9a344a )
	ROM_LOAD( "dz-12.rom", 0x20000, 0x10000, 0x817fae99 )
	ROM_LOAD( "dz-14.rom", 0x30000, 0x10000, 0x0abbf76d )
	ROM_LOAD( "dz-11.rom", 0x40000, 0x10000, 0xa5e19c24 )
	ROM_LOAD( "dz-13.rom", 0x50000, 0x10000, 0x3e7c0405 )
	ROM_LOAD( "dz-17.rom", 0x60000, 0x10000, 0x40361b8b )
	ROM_LOAD( "dz-18.rom", 0x70000, 0x10000, 0x8d219489 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dz-07.rom", 0x00000, 0x10000, 0xe7455167 )
	ROM_LOAD( "dz-08.rom", 0x10000, 0x10000, 0x32f9ddfe )
	ROM_LOAD( "dz-09.rom", 0x20000, 0x10000, 0xbb6efc02 )
	ROM_LOAD( "dz-10.rom", 0x30000, 0x10000, 0x6ef9963b )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "dz19a.10d", 0x0000, 0x0400, 0x47e1f83b )
	ROM_LOAD( "dz20a.11d", 0x0400, 0x0400, 0xd8fe2d99 )
ROM_END

ROM_START( meikyuh )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD( "dw-01.rom", 0x08000, 0x08000, 0x87610c39 )
	ROM_LOAD( "dw-02.rom", 0x10000, 0x10000, 0x40c9b0b8 )
	ROM_LOAD( "dz-03.rom", 0x20000, 0x10000, 0x5606a8f4 )
	ROM_LOAD( "dw-04.rom", 0x30000, 0x10000, 0x235c0c36 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "dw-05.rom", 0x8000, 0x8000, 0xc28c4d82 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dw-00.rom", 0x00000, 0x8000, 0x3d25f15c )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dw-14.rom", 0x00000, 0x10000, 0x9b0dbfa9 )
	ROM_LOAD( "dw-15.rom", 0x10000, 0x10000, 0x95683fda )
	ROM_LOAD( "dw-11.rom", 0x20000, 0x10000, 0x1b1fcca7 )
	ROM_LOAD( "dw-13.rom", 0x30000, 0x10000, 0xe7413056 )
	ROM_LOAD( "dw-10.rom", 0x40000, 0x10000, 0x57667546 )
	ROM_LOAD( "dw-12.rom", 0x50000, 0x10000, 0x4c548db8 )
	ROM_LOAD( "dw-16.rom", 0x60000, 0x10000, 0xe5bcf927 )
	ROM_LOAD( "dw-17.rom", 0x70000, 0x10000, 0x9e10f723 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dw-06.rom", 0x00000, 0x10000, 0xb65e029d )
	ROM_LOAD( "dw-07.rom", 0x10000, 0x10000, 0x668d995d )
	ROM_LOAD( "dw-08.rom", 0x20000, 0x10000, 0xbb2cf4a0 )
	ROM_LOAD( "dw-09.rom", 0x30000, 0x10000, 0x6a528d13 )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "dz19a.10d", 0x0000, 0x0400, 0x00000000 ) /* Not the real ones! */
	ROM_LOAD( "dz20a.11d", 0x0400, 0x0400, 0x00000000 ) /* These are from ghostbusters */
ROM_END

ROM_START( srdarwin )
	ROM_REGION( 0x28000, REGION_CPU1 )
	ROM_LOAD( "dy_01.rom", 0x20000, 0x08000, 0x1eeee4ff )
	ROM_CONTINUE(          0x08000, 0x08000 )
	ROM_LOAD( "dy_00.rom", 0x10000, 0x10000, 0x2bf6b461 )

	ROM_REGION( 2*0x10000, REGION_CPU2 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dy_04.rom", 0x8000, 0x8000, 0x2ae3591c )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dy_05.rom", 0x00000, 0x4000, 0x8780e8a3 )

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dy_07.rom", 0x00000, 0x8000, 0x97eaba60 )
	ROM_LOAD( "dy_06.rom", 0x08000, 0x8000, 0xc279541b )
	ROM_LOAD( "dy_09.rom", 0x10000, 0x8000, 0xd30d1745 )
	ROM_LOAD( "dy_08.rom", 0x18000, 0x8000, 0x71d645fd )
	ROM_LOAD( "dy_11.rom", 0x20000, 0x8000, 0xfd9ccc5b )
	ROM_LOAD( "dy_10.rom", 0x28000, 0x8000, 0x88770ab8 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dy_03.rom", 0x00000, 0x4000, 0x44f2a4f9 )
	ROM_CONTINUE(          0x10000, 0x4000 )
	ROM_CONTINUE(          0x20000, 0x4000 )
	ROM_CONTINUE(          0x30000, 0x4000 )
	ROM_LOAD( "dy_02.rom", 0x08000, 0x4000, 0x522d9a9e )
	ROM_CONTINUE(          0x18000, 0x4000 )
	ROM_CONTINUE(          0x28000, 0x4000 )
	ROM_CONTINUE(          0x38000, 0x4000 )
ROM_END

ROM_START( gondo )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD( "dt-00.256", 0x08000, 0x08000, 0xa8cf9118 )
	ROM_LOAD( "dt-01.512", 0x10000, 0x10000, 0xc39bb877 )
	ROM_LOAD( "dt-02.512", 0x20000, 0x10000, 0xbb5e674b )
	ROM_LOAD( "dt-03.512", 0x30000, 0x10000, 0x99c32b13 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "dt-05.256", 0x8000, 0x8000, 0xec08aa29 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dt-14.256", 0x00000, 0x08000, 0x4bef16e1 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dt-19.512", 0x00000, 0x10000, 0xda2abe4b )
	ROM_LOAD( "dt-20.256", 0x10000, 0x08000, 0x42d01002 )
	ROM_LOAD( "dt-16.512", 0x20000, 0x10000, 0xe9955d8f )
	ROM_LOAD( "dt-18.256", 0x30000, 0x08000, 0xc0c5df1c )
	ROM_LOAD( "dt-15.512", 0x40000, 0x10000, 0xa54b2eb6 )
	ROM_LOAD( "dt-17.256", 0x50000, 0x08000, 0x3bbcff0d )
	ROM_LOAD( "dt-21.512", 0x60000, 0x10000, 0x1c5f682d )
	ROM_LOAD( "dt-22.256", 0x70000, 0x08000, 0xc1876a5f )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dt-08.512", 0x00000, 0x08000, 0xaec483f5 )
	ROM_CONTINUE(          0x10000, 0x08000 )
	ROM_LOAD( "dt-09.256", 0x08000, 0x08000, 0x446f0ce0 )
	ROM_LOAD( "dt-06.512", 0x20000, 0x08000, 0x3fe1527f )
	ROM_CONTINUE(          0x30000, 0x08000 )
	ROM_LOAD( "dt-07.256", 0x28000, 0x08000, 0x61f9bce5 )
	ROM_LOAD( "dt-12.512", 0x40000, 0x08000, 0x1a72ca8d )
	ROM_CONTINUE(          0x50000, 0x08000 )
	ROM_LOAD( "dt-13.256", 0x48000, 0x08000, 0xccb81aec )
	ROM_LOAD( "dt-10.512", 0x60000, 0x08000, 0xcfcfc9ed )
	ROM_CONTINUE(          0x70000, 0x08000 )
	ROM_LOAD( "dt-11.256", 0x68000, 0x08000, 0x53e9cf17 )
ROM_END

ROM_START( makyosen )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD( "ds00",      0x08000, 0x08000, 0x33bb16fe )
	ROM_LOAD( "dt-01.512", 0x10000, 0x10000, 0xc39bb877 )
	ROM_LOAD( "ds02",      0x20000, 0x10000, 0x925307a4 )
	ROM_LOAD( "ds03",      0x30000, 0x10000, 0x9c0fcbf6 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "ds05",      0x8000, 0x8000, 0xe6e28ca9 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "ds14",      0x00000, 0x08000, 0x00cbe9c8 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dt-19.512", 0x00000, 0x10000, 0xda2abe4b )
	ROM_LOAD( "ds20",      0x10000, 0x08000, 0x0eef7f56 )
	ROM_LOAD( "dt-16.512", 0x20000, 0x10000, 0xe9955d8f )
	ROM_LOAD( "ds18",      0x30000, 0x08000, 0x2b2d1468 )
	ROM_LOAD( "dt-15.512", 0x40000, 0x10000, 0xa54b2eb6 )
	ROM_LOAD( "ds17",      0x50000, 0x08000, 0x75ae349a )
	ROM_LOAD( "dt-21.512", 0x60000, 0x10000, 0x1c5f682d )
	ROM_LOAD( "ds22",      0x70000, 0x08000, 0xc8ffb148 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dt-08.512", 0x00000, 0x08000, 0xaec483f5 )
	ROM_CONTINUE(          0x10000, 0x08000 )
	ROM_LOAD( "dt-09.256", 0x08000, 0x08000, 0x446f0ce0 )
	ROM_LOAD( "dt-06.512", 0x20000, 0x08000, 0x3fe1527f )
	ROM_CONTINUE(          0x30000, 0x08000 )
	ROM_LOAD( "dt-07.256", 0x28000, 0x08000, 0x61f9bce5 )
	ROM_LOAD( "dt-12.512", 0x40000, 0x08000, 0x1a72ca8d )
	ROM_CONTINUE(          0x50000, 0x08000 )
	ROM_LOAD( "dt-13.256", 0x48000, 0x08000, 0xccb81aec )
	ROM_LOAD( "dt-10.512", 0x60000, 0x08000, 0xcfcfc9ed )
	ROM_CONTINUE(          0x70000, 0x08000 )
	ROM_LOAD( "dt-11.256", 0x68000, 0x08000, 0x53e9cf17 )
ROM_END

ROM_START( oscar )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "ed10", 0x08000, 0x08000, 0xf9b0d4d4 )
	ROM_LOAD( "ed09", 0x10000, 0x10000, 0xe2d4bba9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "ed11", 0x0000, 0x10000,  0x10e5d919 )

	ROM_REGION( 2*0x10000, REGION_CPU3 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000,  0x432031c5 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, 0x308ac264 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, 0x416a791b )
	ROM_LOAD( "ed05", 0x20000, 0x10000, 0xfcdba431 )
	ROM_LOAD( "ed06", 0x40000, 0x10000, 0x7d50bebc )
	ROM_LOAD( "ed07", 0x60000, 0x10000, 0x8fdf0fa5 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, 0xd3a58e9e )
	ROM_LOAD( "ed03", 0x20000, 0x10000, 0x4fc4fb0f )
	ROM_LOAD( "ed00", 0x40000, 0x10000, 0xac201f2d )
	ROM_LOAD( "ed02", 0x60000, 0x10000, 0x7ddc5651 )
ROM_END

ROM_START( oscarj0 )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "du10", 0x08000, 0x08000, 0x120040d8 )
	ROM_LOAD( "ed09", 0x10000, 0x10000, 0xe2d4bba9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "du11", 0x0000, 0x10000, 0xff45c440 )

	ROM_REGION( 2*0x10000, REGION_CPU3 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000, 0x432031c5 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, 0x308ac264 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, 0x416a791b )
	ROM_LOAD( "ed05", 0x20000, 0x10000, 0xfcdba431 )
	ROM_LOAD( "ed06", 0x40000, 0x10000, 0x7d50bebc )
	ROM_LOAD( "ed07", 0x60000, 0x10000, 0x8fdf0fa5 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, 0xd3a58e9e )
	ROM_LOAD( "ed03", 0x20000, 0x10000, 0x4fc4fb0f )
	ROM_LOAD( "ed00", 0x40000, 0x10000, 0xac201f2d )
	ROM_LOAD( "ed02", 0x60000, 0x10000, 0x7ddc5651 )
ROM_END

ROM_START( oscarj1 )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "oscr10-1.bin", 0x08000, 0x08000, 0x4ebc9f7a ) /* DU10-1 */
	ROM_LOAD( "ed09", 0x10000, 0x10000, 0xe2d4bba9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "du11", 0x0000, 0x10000, 0xff45c440 )

	ROM_REGION( 2*0x10000, REGION_CPU3 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000, 0x432031c5 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, 0x308ac264 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, 0x416a791b )
	ROM_LOAD( "ed05", 0x20000, 0x10000, 0xfcdba431 )
	ROM_LOAD( "ed06", 0x40000, 0x10000, 0x7d50bebc )
	ROM_LOAD( "ed07", 0x60000, 0x10000, 0x8fdf0fa5 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, 0xd3a58e9e )
	ROM_LOAD( "ed03", 0x20000, 0x10000, 0x4fc4fb0f )
	ROM_LOAD( "ed00", 0x40000, 0x10000, 0xac201f2d )
	ROM_LOAD( "ed02", 0x60000, 0x10000, 0x7ddc5651 )
ROM_END

ROM_START( oscarj )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "oscr10-2.bin", 0x08000, 0x08000, 0x114e898d ) /* DU10-2 */
	ROM_LOAD( "ed09", 0x10000, 0x10000, 0xe2d4bba9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "du11", 0x0000, 0x10000, 0xff45c440 )

	ROM_REGION( 2*0x10000, REGION_CPU3 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000, 0x432031c5 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, 0x308ac264 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, 0x416a791b )
	ROM_LOAD( "ed05", 0x20000, 0x10000, 0xfcdba431 )
	ROM_LOAD( "ed06", 0x40000, 0x10000, 0x7d50bebc )
	ROM_LOAD( "ed07", 0x60000, 0x10000, 0x8fdf0fa5 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, 0xd3a58e9e )
	ROM_LOAD( "ed03", 0x20000, 0x10000, 0x4fc4fb0f )
	ROM_LOAD( "ed00", 0x40000, 0x10000, 0xac201f2d )
	ROM_LOAD( "ed02", 0x60000, 0x10000, 0x7ddc5651 )
ROM_END

ROM_START( lastmiss )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "dl03-6",      0x08000, 0x08000, 0x47751a5e ) /* Rev 6 roms */
	ROM_LOAD( "lm_dl04.rom", 0x10000, 0x10000, 0x7dea1552 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "lm_dl02.rom", 0x0000, 0x10000, 0xec9b5daf )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64K for sound CPU */
	ROM_LOAD( "lm_dl05.rom", 0x8000, 0x8000, 0x1a5df8c0 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "lm_dl01.rom", 0x00000, 0x2000, 0xf3787a5d )
	ROM_CONTINUE(		     0x06000, 0x2000 )
	ROM_CONTINUE(		 	 0x04000, 0x2000 )
	ROM_CONTINUE(		 	 0x02000, 0x2000 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "lm_dl11.rom", 0x00000, 0x08000, 0x36579d3b )
	ROM_LOAD( "lm_dl12.rom", 0x20000, 0x08000, 0x2ba6737e )
	ROM_LOAD( "lm_dl13.rom", 0x40000, 0x08000, 0x39a7dc93 )
	ROM_LOAD( "lm_dl10.rom", 0x60000, 0x08000, 0xfe275ea8 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "lm_dl09.rom", 0x00000, 0x10000, 0x6a5a0c5d )
	ROM_LOAD( "lm_dl08.rom", 0x20000, 0x10000, 0x3b38cfce )
	ROM_LOAD( "lm_dl07.rom", 0x40000, 0x10000, 0x1b60604d )
	ROM_LOAD( "lm_dl06.rom", 0x60000, 0x10000, 0xc43c26a7 )
ROM_END

ROM_START( lastmss2 )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "lm_dl03.rom", 0x08000, 0x08000, 0x357f5f6b ) /* Rev 5 roms */
	ROM_LOAD( "lm_dl04.rom", 0x10000, 0x10000, 0x7dea1552 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "lm_dl02.rom", 0x0000, 0x10000, 0xec9b5daf )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64K for sound CPU */
	ROM_LOAD( "lm_dl05.rom", 0x8000, 0x8000, 0x1a5df8c0 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "lm_dl01.rom", 0x00000, 0x2000, 0xf3787a5d )
	ROM_CONTINUE(		     0x06000, 0x2000 )
	ROM_CONTINUE(		 	 0x04000, 0x2000 )
	ROM_CONTINUE(		 	 0x02000, 0x2000 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "lm_dl11.rom", 0x00000, 0x08000, 0x36579d3b )
	ROM_LOAD( "lm_dl12.rom", 0x20000, 0x08000, 0x2ba6737e )
	ROM_LOAD( "lm_dl13.rom", 0x40000, 0x08000, 0x39a7dc93 )
	ROM_LOAD( "lm_dl10.rom", 0x60000, 0x08000, 0xfe275ea8 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "lm_dl09.rom", 0x00000, 0x10000, 0x6a5a0c5d )
	ROM_LOAD( "lm_dl08.rom", 0x20000, 0x10000, 0x3b38cfce )
	ROM_LOAD( "lm_dl07.rom", 0x40000, 0x10000, 0x1b60604d )
	ROM_LOAD( "lm_dl06.rom", 0x60000, 0x10000, 0xc43c26a7 )
ROM_END

ROM_START( shackled )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "dk-02.rom", 0x08000, 0x08000, 0x87f8fa85 )
	ROM_LOAD( "dk-06.rom", 0x10000, 0x10000, 0x69ad62d1 )
	ROM_LOAD( "dk-05.rom", 0x20000, 0x10000, 0x598dd128 )
	ROM_LOAD( "dk-04.rom", 0x30000, 0x10000, 0x36d305d4 )
	ROM_LOAD( "dk-03.rom", 0x40000, 0x08000, 0x6fd90fd1 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "dk-01.rom", 0x00000, 0x10000, 0x71fe3bda )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64K for sound CPU */
	ROM_LOAD( "dk-07.rom", 0x08000, 0x08000, 0x887e4bcc )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dk-00.rom", 0x00000, 0x08000, 0x69b975aa )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dk-12.rom", 0x00000, 0x10000, 0x615c2371 )
	ROM_LOAD( "dk-13.rom", 0x10000, 0x10000, 0x479aa503 )
	ROM_LOAD( "dk-14.rom", 0x20000, 0x10000, 0xcdc24246 )
	ROM_LOAD( "dk-15.rom", 0x30000, 0x10000, 0x88db811b )
	ROM_LOAD( "dk-16.rom", 0x40000, 0x10000, 0x061a76bd )
	ROM_LOAD( "dk-17.rom", 0x50000, 0x10000, 0xa6c5d8af )
	ROM_LOAD( "dk-18.rom", 0x60000, 0x10000, 0x4d466757 )
	ROM_LOAD( "dk-19.rom", 0x70000, 0x10000, 0x1911e83e )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dk-11.rom", 0x00000, 0x10000, 0x5cf5719f )
	ROM_LOAD( "dk-10.rom", 0x20000, 0x10000, 0x408e6d08 )
	ROM_LOAD( "dk-09.rom", 0x40000, 0x10000, 0xc1557fac )
	ROM_LOAD( "dk-08.rom", 0x60000, 0x10000, 0x5e54e9f5 )
ROM_END

ROM_START( breywood )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "7.bin", 0x08000, 0x08000, 0xc19856b9 )
	ROM_LOAD( "3.bin", 0x10000, 0x10000, 0x2860ea02 )
	ROM_LOAD( "4.bin", 0x20000, 0x10000, 0x0fdd915e )
	ROM_LOAD( "5.bin", 0x30000, 0x10000, 0x71036579 )
	ROM_LOAD( "6.bin", 0x40000, 0x08000, 0x308f4893 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "8.bin", 0x0000, 0x10000,  0x3d9fb623 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64K for sound CPU */
	ROM_LOAD( "2.bin", 0x8000, 0x8000,  0x4a471c38 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "1.bin",  0x00000, 0x08000, 0x815a891a )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "20.bin", 0x00000, 0x10000, 0x2b7634f2 )
	ROM_LOAD( "19.bin", 0x10000, 0x10000, 0x4530a952 )
	ROM_LOAD( "18.bin", 0x20000, 0x10000, 0x87c28833 )
	ROM_LOAD( "17.bin", 0x30000, 0x10000, 0xbfb43a4d )
	ROM_LOAD( "16.bin", 0x40000, 0x10000, 0xf9848cc4 )
	ROM_LOAD( "15.bin", 0x50000, 0x10000, 0xbaa3d218 )
	ROM_LOAD( "14.bin", 0x60000, 0x10000, 0x12afe533 )
	ROM_LOAD( "13.bin", 0x70000, 0x10000, 0x03373755 )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "9.bin",  0x00000, 0x10000, 0x067e2a43 )
	ROM_LOAD( "10.bin", 0x20000, 0x10000, 0xc19733aa )
	ROM_LOAD( "11.bin", 0x40000, 0x10000, 0xe37d5dbe )
	ROM_LOAD( "12.bin", 0x60000, 0x10000, 0xbeee880f )
ROM_END

ROM_START( csilver )
	ROM_REGION( 0x48000, REGION_CPU1 )
	ROM_LOAD( "a4", 0x08000, 0x08000, 0x02dd8cfc )
	ROM_LOAD( "a2", 0x10000, 0x10000, 0x570fb50c )
	ROM_LOAD( "a3", 0x20000, 0x10000, 0x58625890 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "a5", 0x0000, 0x10000,  0x29432691 )

	ROM_REGION( 0x18000, REGION_CPU3 )	/* 64K for sound CPU */
	ROM_LOAD( "a6", 0x10000, 0x08000,  0xeb32cf25 )
	ROM_CONTINUE(   0x08000, 0x08000 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "a1",  0x00000, 0x08000, 0xf01ef985 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites (3bpp) */
	ROM_LOAD( "b5",  0x00000, 0x10000, 0x80f07915 )
	/* 0x10000-0x1ffff empy */
	ROM_LOAD( "b4",  0x20000, 0x10000, 0xd32c02e7 )
	/* 0x30000-0x3ffff empy */
	ROM_LOAD( "b3",  0x40000, 0x10000, 0xac78b76b )
	/* 0x50000-0x5ffff empy */
	/* 0x60000-0x7ffff empy (no 4th plane) */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles (3bpp) */
	ROM_LOAD( "a7",  0x00000, 0x10000, 0xb6fb208c )
	ROM_LOAD( "a8",  0x10000, 0x10000, 0xee3e1817 )
	ROM_LOAD( "a9",  0x20000, 0x10000, 0x705900fe )
	ROM_LOAD( "a10", 0x30000, 0x10000, 0x3192571d )
	ROM_LOAD( "b1",  0x40000, 0x10000, 0x3ef77a32 )
	ROM_LOAD( "b2",  0x50000, 0x10000, 0x9cf3d5b8 )
ROM_END

ROM_START( garyoret )
	ROM_REGION( 0x58000, REGION_CPU1 )
	ROM_LOAD( "dv00", 0x08000, 0x08000, 0xcceaaf05 )
	ROM_LOAD( "dv01", 0x10000, 0x10000, 0xc33fc18a )
	ROM_LOAD( "dv02", 0x20000, 0x10000, 0xf9e26ce7 )
	ROM_LOAD( "dv03", 0x30000, 0x10000, 0x55d8d699 )
	ROM_LOAD( "dv04", 0x40000, 0x10000, 0xed3d00ee )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "dv05", 0x08000, 0x08000, 0xc97c347f )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* characters */
	ROM_LOAD( "dv14", 0x00000, 0x08000, 0xfb2bc581 )	/* Characters */

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD( "dv22", 0x00000, 0x10000, 0xcef0367e )
	ROM_LOAD( "dv21", 0x10000, 0x08000, 0x90042fb7 )
	ROM_LOAD( "dv20", 0x20000, 0x10000, 0x451a2d8c )
	ROM_LOAD( "dv19", 0x30000, 0x08000, 0x14e1475b )
	ROM_LOAD( "dv18", 0x40000, 0x10000, 0x7043bead )
	ROM_LOAD( "dv17", 0x50000, 0x08000, 0x28f449d7 )
	ROM_LOAD( "dv16", 0x60000, 0x10000, 0x37e4971e )
	ROM_LOAD( "dv15", 0x70000, 0x08000, 0xca41b6ac )

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* tiles */
	ROM_LOAD( "dv08", 0x00000, 0x08000, 0x89c13e15 )
	ROM_CONTINUE(     0x10000, 0x08000 )
	ROM_LOAD( "dv09", 0x08000, 0x08000, 0x6a345a23 )
	ROM_CONTINUE(     0x18000, 0x08000 )

	ROM_LOAD( "dv06", 0x20000, 0x08000, 0x1eb52a20 )
	ROM_CONTINUE(     0x30000, 0x08000 )
	ROM_LOAD( "dv07", 0x28000, 0x08000, 0xe7346ef8 )
	ROM_CONTINUE(     0x38000, 0x08000 )

	ROM_LOAD( "dv12", 0x40000, 0x08000, 0x46ba5af4 )
	ROM_CONTINUE(     0x50000, 0x08000 )
	ROM_LOAD( "dv13", 0x48000, 0x08000, 0xa7af6dfd )
	ROM_CONTINUE(     0x58000, 0x08000 )

	ROM_LOAD( "dv10", 0x60000, 0x08000, 0x68b6d75c )
	ROM_CONTINUE(     0x70000, 0x08000 )
	ROM_LOAD( "dv11", 0x68000, 0x08000, 0xb5948aee )
	ROM_CONTINUE(     0x78000, 0x08000 )
ROM_END

/******************************************************************************/

/* Ghostbusters, Darwin, Oscar use a "Deco 222" custom 6502 for sound. */
static void init_deco222(void)
{
	int A,sound_cpu,diff;
	unsigned char *rom;

	sound_cpu = 1;
	/* Oscar has three CPUs */
	if (Machine->drv->cpu[2].cpu_type != 0) sound_cpu = 2;

	/* bits 5 and 6 of the opcodes are swapped */
	rom = memory_region(REGION_CPU1+sound_cpu);
	diff = memory_region_length(REGION_CPU1+sound_cpu) / 2;

	memory_set_opcode_base(sound_cpu,rom+diff);

	for (A = 0;A < 0x10000;A++)
		rom[A + diff] = (rom[A] & 0x9f) | ((rom[A] & 0x20) << 1) | ((rom[A] & 0x40) >> 1);
}

static void init_meikyuh(void)
{
	/* Blank out unused garbage in colour prom to avoid colour overflow */
	unsigned char *RAM = memory_region(REGION_PROMS);
	memset(RAM+0x20,0,0xe0);
}

static void init_ghostb(void)
{
	init_deco222();
	init_meikyuh();
}

/******************************************************************************/

GAME(1988, cobracom, 0,        cobracom, cobracom, 0,       ROT0,   "Data East Corporation", "Cobra-Command (World revision 5)" )
GAME(1988, cobracmj, cobracom, cobracom, cobracom, 0,       ROT0,   "Data East Corporation", "Cobra-Command (Japan)" )
GAME(1987, ghostb,   0,        ghostb,   ghostb,   ghostb,  ROT0,   "Data East USA", "The Real Ghostbusters (US 2 Players)" )
GAME(1987, ghostb3,  ghostb,   ghostb,   ghostb,   ghostb,  ROT0,   "Data East USA", "The Real Ghostbusters (US 3 Players)" )
GAME(1987, meikyuh,  ghostb,   ghostb,   meikyuh,  meikyuh, ROT0,   "Data East Corporation", "Meikyuu Hunter G (Japan)" )
GAME(1987, srdarwin, 0,        srdarwin, srdarwin, deco222, ROT270, "Data East Corporation", "Super Real Darwin (Japan)" )
GAME(1987, gondo,    0,        gondo,    gondo,    0,       ROT270, "Data East USA", "Gondomania (US)" )
GAME(1987, makyosen, gondo,    gondo,    gondo,    0,       ROT270, "Data East Corporation", "Makyou Senshi (Japan)" )
GAME(1988, oscar,    0,        oscar,    oscar,    deco222, ROT0,   "Data East USA", "Psycho-Nics Oscar (US)" )
GAME(1987, oscarj,   oscar,    oscar,    oscar,    deco222, ROT0,   "Data East Corporation", "Psycho-Nics Oscar (Japan revision 2)" )
GAME(1987, oscarj1,  oscar,    oscar,    oscar,    deco222, ROT0,   "Data East Corporation", "Psycho-Nics Oscar (Japan revision 1)" )
GAME(1987, oscarj0,  oscar,    oscar,    oscar,    deco222, ROT0,   "Data East Corporation", "Psycho-Nics Oscar (Japan revision 0)" )
GAME(1986, lastmiss, 0,        lastmiss, lastmiss, 0,       ROT270, "Data East USA", "Last Mission (US revision 6)" )
GAME(1986, lastmss2, lastmiss, lastmiss, lastmiss, 0,       ROT270, "Data East USA", "Last Mission (US revision 5)" )
GAME(1986, shackled, 0,        shackled, shackled, 0,       ROT0,   "Data East USA", "Shackled (US)" )
GAME(1986, breywood, shackled, shackled, shackled, 0,       ROT0,   "Data East Corporation", "Breywood (Japan revision 2)" )
GAME(1987, csilver,  0,        csilver,  csilver,  0,       ROT0,   "Data East Corporation", "Captain Silver (Japan)" )
GAME(1987, garyoret, 0,        garyoret, garyoret, 0,       ROT0,   "Data East Corporation", "Garyo Retsuden (Japan)" )
