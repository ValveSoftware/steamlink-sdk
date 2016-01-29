/*********************************************************

	Konami 054539 PCM Sound Chip

	A lot of information comes from Amuse.
	Big thanks to them.

*********************************************************/

#include "driver.h"
#include "k054539.h"
#include <math.h>

/* Registers:
   00..ff: 20 bytes/channel, 8 channels
     00..02: pitch (lsb, mid, msb)
	 03:     volume (0=max, 0x40=-36dB)
     04:
	 05:     pan (1-f right, 10 middle, 11-1f left)
	 06:
	 07:
	 08..0a: loop (lsb, mid, msb)
	 0c..0e: start (lsb, mid, msb) (and current position ?)

   100.1ff: effects?
     13f: pan of the analog input (1-1f)

   200..20f: 2 bytes/channel, 8 channels
     00: type (b2-3), reverse (b5)
	 01: loop (b0)

   214: keyon (b0-7 = channel 0-7)
   215: keyoff          ""
   22c: channel active? ""
   22d: data read/write port
   22e: rom/ram select (00..7f == rom banks, 80 = ram)
   22f: enable pcm (b0), disable registers updating (b7)
*/

#define MAX_K054539 2

struct K054539_channel {
	UINT32 pos;
	UINT32 pfrac;
	INT32 val;
	INT32 pval;
};

struct K054539_chip {
        unsigned char regs[0x230];
        unsigned char *ram;
        int cur_ptr;
        int cur_limit;
        unsigned char *cur_zone;
        void *timer;
        unsigned char *rom;
        UINT32 rom_size;
        UINT32 rom_mask;
        int stream;

        struct K054539_channel channels[8];
};

static struct {
	const struct K054539interface *intf;
	double freq_ratio;
	double voltab[256];
	double pantab[0xf];

	struct K054539_chip chip[MAX_K054539];

} K054539_chips;

static int K054539_regupdate(int chip)
{
	return !(K054539_chips.chip[chip].regs[0x22f] & 0x80);
}

static void K054539_keyon(int chip, int channel)
{
	if(K054539_regupdate(chip))
		K054539_chips.chip[chip].regs[0x22c] |= 1 << channel;
}

static void K054539_keyoff(int chip, int channel)
{
	if(K054539_regupdate(chip))
		K054539_chips.chip[chip].regs[0x22c] &= ~(1 << channel);
}

static void K054539_update(int chip, INT16 **buffer, int length)
{
	static INT16 dpcm[16] = {
		0<<8, 1<<8, 4<<8, 9<<8, 16<<8, 25<<8, 36<<8, 49<<8,
		-64<<8, -49<<8, -36<<8, -25<<8, -16<<8, -9<<8, -4<<8, -1<<8
	};

	int ch;
	unsigned char *samples;
	UINT32 rom_mask;

	if(!Machine->sample_rate)
		return;

	memset(buffer[0], 0, length*2);
	memset(buffer[1], 0, length*2);

	if(!(K054539_chips.chip[chip].regs[0x22f] & 1))
		return;

	samples = K054539_chips.chip[chip].rom;
	rom_mask = K054539_chips.chip[chip].rom_mask;

	for(ch=0; ch<8; ch++)
		if(K054539_chips.chip[chip].regs[0x22c] & (1<<ch)) {
			unsigned char *base1 = K054539_chips.chip[chip].regs + 0x20*ch;
			unsigned char *base2 = K054539_chips.chip[chip].regs + 0x200 + 0x2*ch;
			struct K054539_channel *chan = (struct K054539_channel*)K054539_chips.chip[chip].channels + ch;

			INT16 *bufl = buffer[0];
			INT16 *bufr = buffer[1];

			UINT32 cur_pos = (base1[0x0c] | (base1[0x0d] << 8) | (base1[0x0e] << 16)) & rom_mask;
			INT32 cur_pfrac;
			INT32 cur_val, cur_pval, rval;

			INT32 delta = (base1[0x00] | (base1[0x01] << 8) | (base1[0x02] << 16)) * K054539_chips.freq_ratio;
			INT32 fdelta;
			int pdelta;

			int pan = base1[0x05] >= 0x11 && base1[0x05] <= 0x1f ? base1[0x05] - 0x11 : 0x18 - 0x11;
			int vol = base1[0x03] & 0x7f;
			double lvol = K054539_chips.voltab[vol] * K054539_chips.pantab[pan];
			double rvol = K054539_chips.voltab[vol] * K054539_chips.pantab[0xe - pan];

			if(base2[0] & 0x20) {
				delta = -delta;
				fdelta = +0x10000;
				pdelta = -1;
			} else {
				fdelta = -0x10000;
				pdelta = +1;
			}

			if(cur_pos != chan->pos) {
				chan->pos = cur_pos;
				cur_pfrac = 0;
				cur_val = 0;
				cur_pval = 0;
			} else {
				cur_pfrac = chan->pfrac;
				cur_val = chan->val;
				cur_pval = chan->pval;
			}

#define UPDATE_CHANNELS																	\
			do {																		\
				rval = (cur_pval*cur_pfrac + cur_val*(0x10000 - cur_pfrac)) >> 16;		\
				*bufl++ += (INT16)(rval*lvol);											\
				*bufr++ += (INT16)(rval*rvol);											\
			} while(0)

			switch(base2[0] & 0xc) {
			case 0x0: { // 8bit pcm
				int i;
				for(i=0; i<length; i++) {
					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = (INT16)(samples[cur_pos] << 8);
						if(cur_val == (INT16)0x8000) {
							if(base2[1] & 1) {
								cur_pos = (base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16)) & rom_mask;
								cur_val = (INT16)(samples[cur_pos] << 8);
								if(cur_val != (INT16)0x8000)
									continue;
							}
							K054539_keyoff(chip, ch);
							goto end_channel_0;
						}
					}

					UPDATE_CHANNELS;
				}
			end_channel_0:
				break;
			}
			case 0x4: { // 16bit pcm lsb first
				int i;
				cur_pos >>= 1;

				for(i=0; i<length; i++) {
					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = (INT16)(samples[cur_pos<<1] | samples[(cur_pos<<1)|1]<<8);
						if(cur_val == (INT16)0x8000) {
							if(base2[1] & 1) {
								cur_pos = ((base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16)) & rom_mask) >> 1;
								cur_val = (INT16)(samples[cur_pos<<1] | samples[(cur_pos<<1)|1]<<8);
								if(cur_val != (INT16)0x8000)
									continue;
							}
							K054539_keyoff(chip, ch);
							goto end_channel_4;
						}
					}

					UPDATE_CHANNELS;
				}
			end_channel_4:
				cur_pos <<= 1;
				break;
			}
			case 0x8: { // 4bit dpcm
				int i;

				cur_pos <<= 1;
				cur_pfrac <<= 1;
				if(cur_pfrac & 0x10000) {
					cur_pfrac &= 0xffff;
					cur_pos |= 1;
				}

				for(i=0; i<length; i++) {
					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = samples[cur_pos>>1];
						if(cur_val == 0x88) {
							if(base2[1] & 1) {
								cur_pos = ((base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16)) & rom_mask) << 1;
								cur_val = samples[cur_pos>>1];
								if(cur_val != 0x88)
									goto next_iter;
							}
							K054539_keyoff(chip, ch);
							goto end_channel_8;
						}
					next_iter:
						if(cur_pos & 1)
							cur_val >>= 4;
						else
							cur_val &= 15;
						cur_val = cur_pval + dpcm[cur_val];
						if(cur_val < -32768)
							cur_val = -32768;
						else if(cur_val > 32767)
							cur_val = 32767;
					}

					UPDATE_CHANNELS;
				}
			end_channel_8:
				cur_pfrac >>= 1;
				if(cur_pos & 1)
					cur_pfrac |= 0x8000;
				cur_pos >>= 1;
				break;
			}
			default:
				logerror("Unknown sample type %x for channel %d\n", base2[0] & 0xc, ch);
				break;
			}
			chan->pos = cur_pos;
			chan->pfrac = cur_pfrac;
			chan->pval = cur_pval;
			chan->val = cur_val;
			if(K054539_regupdate(chip)) {
				base1[0x0c] = cur_pos & 0xff;
				base1[0x0d] = (cur_pos >> 8) & 0xff;
				base1[0x0e] = (cur_pos >> 16) & 0xff;
			}
		}
}


static void K054539_irq(int chip)
{
	if(K054539_chips.chip[chip].regs[0x22f] & 0x20)
		K054539_chips.intf->irq[chip] ();
}

static void K054539_init_chip(int chip, const struct MachineSound *msound)
{
	char buf[2][50];
	const char *bufp[2];
	int vol[2];
	int i;

	memset(K054539_chips.chip[chip].regs, 0, sizeof(K054539_chips.chip[chip].regs));
	K054539_chips.chip[chip].ram = (unsigned char *)malloc(0x4000);
	K054539_chips.chip[chip].cur_ptr = 0;
	K054539_chips.chip[chip].rom = memory_region(K054539_chips.intf->region[chip]);
	K054539_chips.chip[chip].rom_size = memory_region_length(K054539_chips.intf->region[chip]);
	K054539_chips.chip[chip].rom_mask = 0xffffffffU;
	for(i=0; i<32; i++)
		if((1U<<i) >= K054539_chips.chip[chip].rom_size) {
			K054539_chips.chip[chip].rom_mask = (1U<<i) - 1;
			break;
		}

	if(K054539_chips.intf->irq[chip])
		// One or more of the registers must be the timer period
		// And anyway, this particular frequency is probably wrong
		K054539_chips.chip[chip].timer = timer_pulse(TIME_IN_HZ(500), 0, K054539_irq);
	else
		K054539_chips.chip[chip].timer = 0;

	sprintf(buf[0], "%s.%d L", sound_name(msound), chip);
	sprintf(buf[1], "%s.%d R", sound_name(msound), chip);
	bufp[0] = buf[0];
	bufp[1] = buf[1];
	vol[0] = MIXER(K054539_chips.intf->mixing_level[chip][0], MIXER_PAN_LEFT);
	vol[1] = MIXER(K054539_chips.intf->mixing_level[chip][1], MIXER_PAN_RIGHT);
	K054539_chips.chip[chip].stream = stream_init_multi(2, bufp, vol, Machine->sample_rate, chip, K054539_update);
}

static void K054539_stop_chip(int chip)
{
	free(K054539_chips.chip[chip].ram);
	if (K054539_chips.chip[chip].timer)
		timer_remove(K054539_chips.chip[chip].timer);
}

static void K054539_w(int chip, offs_t offset, data8_t data)
{
	data8_t old = K054539_chips.chip[chip].regs[offset];
	K054539_chips.chip[chip].regs[offset] = data;
	switch(offset) {
	case 0x13f: {
		int pan = data >= 0x11 && data <= 0x1f ? data - 0x11 : 0x18 - 0x11;
		if(K054539_chips.intf->apan[chip])
			K054539_chips.intf->apan[chip](K054539_chips.pantab[pan], K054539_chips.pantab[0xe - pan]);
		break;
	}
	case 0x214: {
		int ch;
		for(ch=0; ch<8; ch++)
			if(data & (1<<ch))
				K054539_keyon(chip, ch);
		break;
	}
	case 0x215: {
		int ch;
		for(ch=0; ch<8; ch++)
			if(data & (1<<ch))
				K054539_keyoff(chip, ch);
		break;
	}
	case 0x22d:
		if(K054539_chips.chip[chip].regs[0x22e] == 0x80)
			K054539_chips.chip[chip].cur_zone[K054539_chips.chip[chip].cur_ptr] = data;
		K054539_chips.chip[chip].cur_ptr++;
		if(K054539_chips.chip[chip].cur_ptr == K054539_chips.chip[chip].cur_limit)
			K054539_chips.chip[chip].cur_ptr = 0;
		break;
	case 0x22e:
		K054539_chips.chip[chip].cur_zone =
			data == 0x80 ? K054539_chips.chip[chip].ram :
			K054539_chips.chip[chip].rom + 0x20000*data;
		K054539_chips.chip[chip].cur_limit = data == 0x80 ? 0x4000 : 0x20000;
		K054539_chips.chip[chip].cur_ptr = 0;
		break;
	default:
		if(old != data) {
			if((offset & 0xff00) == 0) {
				int chanoff = offset & 0x1f;
				if(chanoff < 4 || chanoff == 5 ||
				   (chanoff >=8 && chanoff <= 0xa) ||
				   (chanoff >= 0xc && chanoff <= 0xe))
					break;
			}
			if(1 || ((offset >= 0x200) && (offset <= 0x210)))
				break;
			logerror("K054539 %03x = %02x\n", offset, data);
		}
		break;
	}
}

#if 0
static void reset_zones(void)
{
	int chip;
	for(chip=0; chip<K054539_chips.intf->num; chip++) {
		int data = K054539_chips.chip[chip].regs[0x22e];
		K054539_chips.chip[chip].cur_zone =
			data == 0x80 ? K054539_chips.chip[chip].ram :
			K054539_chips.chip[chip].rom + 0x20000*data;
		K054539_chips.chip[chip].cur_limit = data == 0x80 ? 0x4000 : 0x20000;
	}
}
#endif

static data8_t K054539_r(int chip, offs_t offset)
{
	switch(offset) {
	case 0x22d:
		if(K054539_chips.chip[chip].regs[0x22f] & 0x10) {
			data8_t res = K054539_chips.chip[chip].cur_zone[K054539_chips.chip[chip].cur_ptr];
			K054539_chips.chip[chip].cur_ptr++;
			if(K054539_chips.chip[chip].cur_ptr == K054539_chips.chip[chip].cur_limit)
				K054539_chips.chip[chip].cur_ptr = 0;
			return res;
		} else
			return 0;
	case 0x22c:
		break;
	default:
		logerror("K054539 read %03x\n", offset);
		break;
	}
	return K054539_chips.chip[chip].regs[offset];
}

int K054539_sh_start(const struct MachineSound *msound)
{
	int i;

	K054539_chips.intf = (const K054539interface*)msound->sound_interface;

	if(Machine->sample_rate)
		K054539_chips.freq_ratio = (double)(K054539_chips.intf->clock) / (double)(Machine->sample_rate);
	else
		K054539_chips.freq_ratio = 1.0;

	// Factor the 1/4 for the number of channels in the volume (1/8 is too harsh, 1/2 gives clipping)
	// vol=0 -> no attenuation, vol=0x40 -> -36dB
	for(i=0; i<256; i++)
		K054539_chips.voltab[i] = pow(10.0, (-36.0 * (double)i / (double)0x40) / 20.0) / 4.0;

	// Pan table for the left channel
	// Right channel is identical with inverted index
	// Formula is such that pan[i]**2+pan[0xe-i]**2 = 1 (constant output power)
	// and pan[0] = 1 (full panning)
	for(i=0; i<0xf; i++)
		K054539_chips.pantab[i] = sqrt(0xe - i) / sqrt(0xe);

	for(i=0; i<K054539_chips.intf->num; i++)
		K054539_init_chip(i, msound);
	return 0;
}

void K054539_sh_stop(void)
{
	int i;
	for(i=0; i<K054539_chips.intf->num; i++)
		K054539_stop_chip(i);
}

WRITE_HANDLER( K054539_0_w )
{
	K054539_w(0, offset, data);
}

READ_HANDLER( K054539_0_r )
{
	return K054539_r(0, offset);
}

WRITE_HANDLER( K054539_1_w )
{
	K054539_w(1, offset, data);
}

READ_HANDLER( K054539_1_r )
{
	return K054539_r(1, offset);
}

