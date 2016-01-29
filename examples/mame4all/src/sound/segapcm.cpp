/*********************************************************/
/*    SEGA 16ch 8bit PCM                                 */
/*********************************************************/

#include "driver.h"
#include "osinline.h"
#include <math.h>

#define  PCM_NORMALIZE

#define SPCM_CENTER    (0x80)
#define SEGA_SAMPLE_RATE    (15800)
#define SEGA_SAMPLE_SHIFT   (5)
#define SEGA_SAMPLE_RATE_OLD    (15800*2)
#define SEGA_SAMPLE_SHIFT_OLD   (5)

#define  MIN_SLICE    (44/2)

#define  PCM_ADDR_SHIFT   (12)

static SEGAPCM    spcm;
static int emulation_rate;
static int buffer_len;
static unsigned char *pcm_rom;
static int  sample_rate, sample_shift;

static int stream;

static int SEGAPCM_samples[][2] = {
	{ SEGA_SAMPLE_RATE, SEGA_SAMPLE_SHIFT, },
	{ SEGA_SAMPLE_RATE_OLD, SEGA_SAMPLE_SHIFT_OLD, },
};

#if 0
static int segapcm_gaintable[] = {
	0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e,
	0x20,0x22,0x24,0x26,0x28,0x2a,0x2c,0x2e,0x30,0x32,0x34,0x36,0x38,0x3a,0x3c,0x3e,
	0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e,0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e,
	0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x70,0x72,0x74,0x76,0x78,0x7a,0x7c,0x7e,

	0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e,
	0x20,0x22,0x24,0x26,0x28,0x2a,0x2c,0x2e,0x30,0x32,0x34,0x36,0x38,0x3a,0x3c,0x3e,
	0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e,0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e,
	0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x70,0x72,0x74,0x76,0x78,0x7a,0x7c,0x7e,

#if 0
	0x00,0x04,0x08,0x0c,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,
	0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e,0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
#endif
#if 0
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
#endif
};
#endif


static void SEGAPCMUpdate( int num, INT16 **buffer, int length );


/************************************************/
/*                                              */
/************************************************/
int SEGAPCM_sh_start( const struct MachineSound *msound )
{
	struct SEGAPCMinterface *intf = (struct SEGAPCMinterface *)msound->sound_interface;
	if (Machine->sample_rate == 0) return 0;
	if( SEGAPCMInit( msound, intf->bank&0x00ffffff, intf->mode, (unsigned char *)memory_region(intf->region), intf->volume ) )
		return 1;
	return 0;
}
/************************************************/
/*                                              */
/************************************************/
void SEGAPCM_sh_stop( void )
{
	SEGAPCMShutdown();
}
/************************************************/
/*                                              */
/************************************************/
void SEGAPCM_sh_update( void )
{
}

/************************************************/
/*    initial SEGAPCM                           */
/************************************************/
int SEGAPCMInit( const struct MachineSound *msound, int banksize, int mode, unsigned char *inpcm, int volume )
{
	int i;
	int rate = Machine->sample_rate;
	buffer_len = rate / Machine->drv->frames_per_second;
	emulation_rate = buffer_len * Machine->drv->frames_per_second;
	sample_rate = SEGAPCM_samples[mode][0];
	sample_shift = SEGAPCM_samples[mode][1];
	pcm_rom = inpcm;

	//printf( "segaPCM in\n" );

	/**** interface init ****/
	spcm.bankshift = banksize&0xffffff;
	if( (banksize>>16) == 0x00 )
	{
		spcm.bankmask = (BANK_MASK7>>16)&0x00ff;	/* default */
	}
	else
	{
		spcm.bankmask = (banksize>>16)&0x00ff;
	}

	for( i = 0; i < SEGAPCM_MAX; i++ )
	{
		spcm.gain[i][L_PAN] = spcm.gain[i][R_PAN] = (unsigned char)0;
		spcm.vol[i][L_PAN] = spcm.vol[i][R_PAN] = 0;
		spcm.addr_l[i] = 0;
		spcm.addr_h[i] = 0;
		spcm.bank[i] = 0;
		spcm.end_h[i] = 0;
		spcm.delta_t[i] = 0x80;
		spcm.flag[i] = 1;
		spcm.add_addr[i] = 0;
		spcm.step[i] = (int)(((float)sample_rate / (float)emulation_rate) * (float)(0x80<<5));
		spcm.pcmd[i] = 0;
	}
	//printf( "segaPCM work init end\n" );

	{
		char buf[LR_PAN][40];
		const char *name[LR_PAN];
		int  vol[2];
		name[0] = buf[0];
		name[1] = buf[1];
		sprintf( buf[0], "%s L", sound_name(msound) );
		sprintf( buf[1], "%s R", sound_name(msound) );
		vol[0] = (MIXER_PAN_LEFT<<8)  | (volume&0xff);
		vol[1] = (MIXER_PAN_RIGHT<<8) | (volume&0xff);
		stream = stream_init_multi( LR_PAN, name, vol, rate, 0, SEGAPCMUpdate );
	}
	//printf( "segaPCM end\n" );
	return 0;
}

/************************************************/
/*    shutdown SEGAPCM                          */
/************************************************/
void SEGAPCMShutdown( void )
{
}

/************************************************/
/*    reset SEGAPCM                             */
/************************************************/
void SEGAPCMResetChip( void )
{
	int i;
	for( i = 0; i < SEGAPCM_MAX; i++ )
	{
		spcm.gain[i][L_PAN] = spcm.gain[i][R_PAN] = (unsigned char)0;
		spcm.vol[i][L_PAN] = spcm.vol[i][R_PAN] = 0;
		spcm.addr_l[i] = 0;
		spcm.addr_h[i] = 0;
		spcm.bank[i] = 0;
		spcm.end_h[i] = 0;
		spcm.delta_t[i] = 0;
		spcm.flag[i] = 1;
		spcm.add_addr[i] = 0;
		spcm.step[i] = (int)(((float)sample_rate / (float)emulation_rate) * (float)(0x80<<5));
	}
}

/************************************************/
/*    update SEGAPCM                            */
/************************************************/

INLINE int ILimit(int v, int max, int min) { return v > max ? max : (v < min ? min : v); }

static void SEGAPCMUpdate( int num, INT16 **buffer, int length )
{
	int i, j;
	unsigned int addr, old_addr, end_addr, end_check_addr;
	unsigned char *pcm_buf;
	int  lv, rv;
	INT16  *datap[2];
	int tmp;
#ifdef clip_short
	clip_short_pre();
	int val;
#endif

	if( Machine->sample_rate == 0 ) return;
	if( pcm_rom == NULL )    return;

	datap[0] = buffer[0];
	datap[1] = buffer[1];

	memset( datap[0], 0x00, length * sizeof(INT16) );
	memset( datap[1], 0x00, length * sizeof(INT16) );

	for( i = 0; i < SEGAPCM_MAX; i++ )
	{
		if( spcm.flag[i] == 2)
		{
			spcm.flag[i]=0;
			spcm.add_addr[i] = (( (((int)spcm.addr_h[i]<<8)&0xff00) |
				  (spcm.addr_l[i]&0x00ff) ) << PCM_ADDR_SHIFT) &0x0ffff000;
		}
		if( !spcm.flag[i] )
		{
			lv = spcm.vol[i][L_PAN];   rv = spcm.vol[i][R_PAN];
			if(lv==0 && rv==0) continue;

			pcm_buf = pcm_rom + (((int)spcm.bank[i]&spcm.bankmask)<<spcm.bankshift);
			addr = (spcm.add_addr[i]>>PCM_ADDR_SHIFT)&0x0000ffff;
			end_addr = ((((unsigned int)spcm.end_h[i]<<8)&0xff00) + 0x00ff);
			if(spcm.end_h[i] < spcm.addr_h[i]) end_addr+=0x10000;

			for( j = 0; j < length; j++ )
			{
				old_addr = addr;
				/**** make address ****/
				end_check_addr = (spcm.add_addr[i]>>PCM_ADDR_SHIFT);
				addr = end_check_addr&0x0000ffff;
				for(; old_addr <= addr; old_addr++ )
				{
					/**** end address check ****/
					if( end_check_addr >= end_addr )
					{
						if(spcm.writeram[i*8+0x86] & 0x02)
						{
							spcm.flag[i] = 1;
							spcm.writeram[i*8+0x86] = (spcm.writeram[i*8+0x86]&0xfe)|1;
							break;
						}
						else
						{ /* Loop */
							spcm.add_addr[i] = (( (((int)spcm.addr_h[i]<<8)&0xff00) |
									(spcm.addr_l[i]&0x00ff) ) << PCM_ADDR_SHIFT) &0x0ffff000;
						}
					}
#ifdef PCM_NORMALIZE
					tmp = spcm.pcmd[i];
					spcm.pcmd[i] = (int)(*(pcm_buf + old_addr) - SPCM_CENTER);
					spcm.pcma[i] = (tmp - spcm.pcmd[i]) / 2;
					spcm.pcmd[i] += spcm.pcma[i];
#endif
				}
				spcm.add_addr[i] += spcm.step[i];
				if( spcm.flag[i] == 1 )  break;
#ifndef PCM_NORMALIZE
#ifndef clip_short
				*(datap[0] + j) = ILimit( (int)*(datap[0] + j) + ((int)(*(pcm_buf + addr) - SPCM_CENTER)*lv), 32767, -32768 );
				*(datap[1] + j) = ILimit( (int)*(datap[1] + j) + ((int)(*(pcm_buf + addr) - SPCM_CENTER)*rv), 32767, -32768 );
#else
				val=( (int)*(datap[0] + j) + ((int)(*(pcm_buf + addr) - SPCM_CENTER)*lv));
				clip_short(val);
				*(datap[0] + j) = val;
				val=( (int)*(datap[1] + j) + ((int)(*(pcm_buf + addr) - SPCM_CENTER)*rv));
				clip_short(val);
				*(datap[1] + j) = val;
#endif
#else
#ifndef clip_short
				*(datap[0] + j) = ILimit( (int)*(datap[0] + j) + (spcm.pcmd[i] * lv), 32767, -32768 );
				*(datap[1] + j) = ILimit( (int)*(datap[1] + j) + (spcm.pcmd[i] * rv), 32767, -32768 );
#else
				val=( (int)*(datap[0] + j) + (spcm.pcmd[i] * lv));
				clip_short(val);
				*(datap[0] + j) = val;
				val=( (int)*(datap[1] + j) + (spcm.pcmd[i] * rv));
				clip_short(val);
				*(datap[1] + j) = val;
#endif
#endif
			}
			/**** end of length ****/
		}
		/**** end flag check ****/
	}
	/**** pcm channel end ****/

}

/************************************************/
/*    wrtie register SEGAPCM                    */
/************************************************/
WRITE_HANDLER( SegaPCM_w )
{
	int r = offset;
	int v = data;
	int rate;
	int  lv, rv, cen;

	int channel = (r>>3)&0x0f;

	spcm.writeram[r&0x07ff] = (char)v;		/* write value data */
	switch( (r&0x87) )
	{
		case 0x00:
		case 0x01:
		case 0x84:  case 0x85:
		case 0x87:
			break;

		case 0x02:
			spcm.gain[channel][L_PAN] = v&0xff;
remake_vol:
			lv = spcm.gain[channel][L_PAN];   rv = spcm.gain[channel][R_PAN];
			cen = (lv + rv) / 4;
//			spcm.vol[channel][L_PAN] = (lv + cen)<<1;
//			spcm.vol[channel][R_PAN] = (rv + cen)<<1;
			spcm.vol[channel][L_PAN] = (lv + cen)*9/5;	// too much clipping
			spcm.vol[channel][R_PAN] = (rv + cen)*9/5;
			break;
		case 0x03:
			spcm.gain[channel][R_PAN] = v&0xff;
			goto remake_vol;


		case 0x04:
			spcm.addr_l[channel]= v;
			break;
		case 0x05:
			spcm.addr_h[channel]= v;
			break;
		case 0x06:
			spcm.end_h[channel]= v;
			break;
		case 0x07:
			spcm.delta_t[channel]= v;
			rate = (v&0x00ff)<<sample_shift;
			spcm.step[channel] = (int)(((float)sample_rate / (float)emulation_rate) * (float)rate);
			break;
		case 0x86:
			spcm.bank[channel]= v;
			if( v&1 )    spcm.flag[channel] = 1; /* stop D/A */
			else
			{
				/**** start D/A ****/
//				spcm.flag[channel] = 0;
				spcm.flag[channel] = 2;
//				spcm.add_addr[channel] = (( (((int)spcm.addr_h[channel]<<8)&0xff00) |
//					  (spcm.addr_l[channel]&0x00ff) ) << PCM_ADDR_SHIFT) &0x0ffff000;
				spcm.pcmd[channel] = 0;
			}
			break;
		/*
		default:
			printf( "unknown %d = %02x : %02x\n", channel, r, v );
			break;
		*/
	}
}

/************************************************/
/*    read register SEGAPCM                     */
/************************************************/
READ_HANDLER( SegaPCM_r )
{
	return spcm.writeram[offset & 0x07ff];		/* read value data */
}

/**************** end of file ****************/
