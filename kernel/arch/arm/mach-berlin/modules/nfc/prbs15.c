/** Generate PRBS-15.
 *
 * At present, only x^15 + x^14 + 1 (0xC001) is supported.
 *
 * @ref ITU-T Recommendation O.150, 05/96
 *
 * @warning: This file is used in other places, so don't inlcude any
 *           kernel related files to it.
 *
 * Author: Yongsen Chen, YongsenChen@gmail.com
 */

/* ALGORITHM (x^15 + x^14 + 1)
 *
 * The polynomial is x^15 + x^14 + 1 (0xC001), it feedback (x15 XOR x14) as
 * the input, and output it (or its reserved bit).
 *
 *    +----------------------------------------------------------+-----NOT-->
 *    |                                                        +XOR +
 *    v                                                        |    |
 *    x1->x2->x3->x4->x5->x6->x7->x8->x9->x10->x11->x12->x13->x14->x15
 *    0   1   0   1   0   1   1   0   1   1    1    0    1    0    1
 *    ------A------   ------6------   -------7-------    -----5-----
 *
 * Here, we take seed = 0x576A(101011101101010b) as example, then the output
 * bit sequence (non-reserved) is
 *
 *      111100110111111 000101011000001 001111101000011 010000111000101 1100...
 *      ^ first output bit
 * so the byte sequence is 0xCF, 0x7E, 0xD4, 0x20, 0x5F, 0x58, 0x38, 0x3A, ...
 *
 * As the algorithm defines, only (x15 XOR x14), so we can convert it as 2
 * unsigned short do the XOR. It would get 14 bits as result. It's shown as follow.
 *      101011101101010                 -> seed = 0x576A
 *    ^  10101110110101                 -> (seed >> 1)
 *   -----------------------------------
 *      x11110011011111                 -> prbs_result: 14bits = 0x3CDF
 *
 * But unfortunately, it's not in the right bit order. The right one should be
 * 11111011001111b = 0x3ECF. There are simple way2 to make it work,
 * (1) one is reversing the result.
 *      101011101101010                 -> seed = 0x576A
 *    ^  10101110110101                 -> (seed >> 1)
 *   -----------------------------------
 *    r x11110011011111                 -> prbs_result': 14bits = 0x3CDF
 *   -----------------------------------
 *       11111011001111                 -> prbs_result: 14bits = 0x3ECF
 *
 * (2) the other is reversing the seed, then it would be correct.
 *    r 101011101101010                 -> seed = 0x576A
 *   -----------------------------------
 *      010101101110101                 -> seed_rev = reverse(seed) = 0x2B75
 *    ^  01010110111010                 -> (seed_rev >> 1)
 *   -----------------------------------
 *      x11111011001111                 -> prbs_result: 14bits = 0x3ECF
 *
 * In fact, it just like consider x15 to be y1 in the LFSR as follow.
 *    +-----------------------------------------------------------+----NOT-->
 *    |                                                         +XOR+
 *    v                                                         |   |
 *    y15->y14->y13->y12->y11->y10->y9->y8->y7->y6->y5->y4->y3->y2->y1
 *    0   1   0   1   0   1   1   0   1   1    1    0    1    0    1
 *    ----2----   ------B------   ------7-------    --------5-------
 *
 * And in order to make the algorithm more efficient, we calculate it for 2 or
 * 4 times to get 28 bits (for 32-bit processor) or 56 bits (for 64-bit processor).
 * Here, we suppose the processor is 32-bit.
 * Then, for the way (1), we get
 *
 *                    101011101101010   -> seed = 0x576A
 *    ^                10101110110101   -> (seed >> 1)
 *   -----------------------------------
 *                    x11110011011111   -> first_prbs_result_rev: 14bits
 *    | 101011101101010                 -> (seed << 14)
 *   -----------------------------------
 *      10101110110101011110011011111   -> temp = (first_prbs_result_rev & 0x3FFF) | (seed << 14)
 *    ^  1010111011010101111001101111   -> (temp >> 1)
 *                    * --> this bit should be calculated again.
 *   -----------------------------------
 *    r x1111001101111110001010110000   -> final_prbs_result_rev: 28 bits = 0xF37E2B0
 *                    110001010110000   -> next_seed = 0x62B0
 *       * the first output bit
 *   -----------------------------------
 *       0000110101000111111011001111   -> final_prbs_result: 28 bits = 0x0D47ECF
 *
 * so, here are the steps.
 * step 1, first_prbs_result_rev = seed ^ (seed >> 1);  // get 14 bits prbs
 * step 2, temp = (first_prbs_result_rev & 0x3FFF) | (seed << 14);
 * step 3, final_prbs_result_rev = (temp ^ (temp >> 1)) & 0xFFFFFFF;
 *         next_seed = final_prbs_result_rev & 0x7FFF;
 * step 4, final_prbs_result_rev = reverse(final_prbs_result_rev)
 *
 * In each loop, we need to do step 1-4.
 *
 * for way (2), we get the follow algorithm. --- Used in the code.
 *
 *    r 101011101101010                 -> seed = 0x576A
 *   -----------------------------------
 *      010101101110101                 -> seed_rev = reverse(seed) = 0x2B75
 *    ^  01010110111010                 -> (seed_rev >> 1)
 *   -----------------------------------
 *      x11111011001111                 -> first_prbs_result: 14bits
 *    |                010101101110101  -> seed_rev
 *   -----------------------------------
 *      x11111011001111010101101110101  -> temp = (first_prbs_result << 15) | seed_rev
 *    ^  x1111101100111101010110111010  -> (temp >> 1)
 *                     * --> this bit should be calculated again.
 *   -----------------------------------
 *      xx0000110101000111111011001111  -> final_prbs_result: 28 bits = 0x0D47ECF
 *    r   000011010100011               -> next_seed_rev = final_prbs_result[13..27] = 0x06A3
 *   -----------------------------------
 *        110001010110000               -> next_seed = reverse(next_seed_rev) = 0x62B0
 *
 * So, here are the steps.
 * step 1, seed_rev = reverse(seed)
 * step 2, first_prbs_result = seed_rev ^ (seed_rev >> 1);  // get 14 bits prbs
 * step 3, temp = (first_prbs_result << 15) | seed_rev;
 * step 4, final_prbs_result = (temp ^ (temp >> 1)) & 0xFFFFFFF;
 *         next_seed_rev = final_prbs_result >> (28 - 15);
 * step 5, next_seed = reverse(next_seed_rev)
 *
 * In fact, from step 2-4, we always use seed_rev, so we only need to reverse
 * the seed enter and exit the main routine. It would get better performance
 * than way (1), so we use it for the code.
 *
 * @note As the algorithm shows, if it works on a 64-bit processor, then it
 *       can loop 2 more times, so it will get 56 bits. It's good, for it's
 *       byte-aligned (7 bytes).
 *
 * The 56 bits result is: (go on with the 28 bits result)
 * xx00001101010001 11111011001111 010101101110101
 *    0000110101000 11111101100111 101010110111010
 * ---------------------------------------------------------------
 *    0001011111001 00000110101000 111111011001111 010101101110101
 *     000101111100 10000011010100 011111101100111 101010110111010
 * ---------------------------------------------------------------
 *     001110000101 10000101111100 100000110101000 111111011001111 -> 0x38585F20D47ECF
 *     001110000101 100                                            -> seed_rev = 0x1C2C
 *
 */


/*
 * CONFIGURATIONS
 */
#define xUNIT_TEST

#ifndef ASSERT
#	define ASSERT(x)
#endif /* ASSERT */

/*
 * INCLUDES
 */
#include "prbs.h"

#define PRBS15_BITS				(15)
#define PRBS15_PROCESS_BITS			(28)
#define PRBS15_PROCESS_BYTES			(7)

#ifndef TRUE
#define TRUE					(1)
#endif

#ifndef FALSE
#define FALSE					(0)
#endif

#if 0 /* we don't use it at present, so don't compile it to avoid warning */
/** Reverse 8 bits.
 *
 * @param n                 Data to reverse.
 *
 * @return unsigned char    The reversed data.
 *
 * @sa reverse_bit16(), reverse_bit32()
 */
static unsigned char reverse_bit8(unsigned char n)
{
	n = ((n >> 1) & 0x55) | ((n << 1) & 0xaa);
	n = ((n >> 2) & 0x33) | ((n << 2) & 0xcc);
	n = ((n >> 4) & 0x0f) | ((n << 4) & 0xf0);

	return n;
}
#endif

/** Reverse 16 bits.
 *
 * @param n                 Data to reverse.
 *
 * @return unsigned short   The reversed data.
 *
 * @sa reverse_bit16(), reverse_bit32()
 */
static unsigned short reverse_bit16(unsigned short n)
{
	n = ((n >> 1) & 0x5555) | ((n << 1) & 0xaaaa);
	n = ((n >> 2) & 0x3333) | ((n << 2) & 0xcccc);
	n = ((n >> 4) & 0x0f0f) | ((n << 4) & 0xf0f0);
	n = ((n >> 8) & 0x00ff) | ((n << 8) & 0xff00);

	return n;
}

#if 0 /* we don't use it at present, so don't compile it to avoid warning */
/** Reverse 32 bits.
 *
 * @param n                 Data to reverse.
 *
 * @return unsigned int    The reversed data.
 *
 * @sa reverse_bit16(), reverse_bit32()
 */
static unsigned int reverse_bit32 (unsigned int n)
{
	n = ((n >> 1) & 0x55555555) | ((n << 1) & 0xaaaaaaaa);
	n = ((n >> 2) & 0x33333333) | ((n << 2) & 0xcccccccc);
	n = ((n >> 4) & 0x0f0f0f0f) | ((n << 4) & 0xf0f0f0f0);
	n = ((n >> 8) & 0x00ff00ff) | ((n << 8) & 0xff00ff00);
	n = ((n >> 16) & 0x0000ffff) | ((n << 16) & 0xffff0000);

	return n;
}
#endif

/** Get 28 bits PRBS from the reserved seed (seed_rev).
 *
 * It handles step 2-4 in the algorithm.
 * And it would be called by prbs15_next_7_bytes().
 *
 * @param seed_rev          The input 15bit reversed seed.
 * @param next_seed_rev     Field to return the next reversed seed.
 *
 * @return unsigned int    The 28 bits PRBS, in bit0-27.
 *
 * @sa prbs15_next_7_bytes()
 */
static unsigned int prbs15_next_28_bits(unsigned short seed_rev,
					unsigned short *next_seed_rev)
{
	unsigned int prbs_result, temp;

	prbs_result = seed_rev ^ (seed_rev >> 1);

	temp = (prbs_result << PRBS15_BITS) | seed_rev;

	prbs_result = (temp ^ (temp >> 1)) & 0xFFFFFFF; /* results is 28 bits   */

	if (next_seed_rev) {
		*next_seed_rev = (unsigned short)(prbs_result >>
			(PRBS15_PROCESS_BITS - PRBS15_BITS));
	}

	return prbs_result;
}

/** Gets 7 bytes PRBS from the seed, and return new seed for next loop.
 *
 * For prbs15_next_28_bits() only gets 28 bits, so we will call it twice to get
 * 56 bits, so it can be byte-aligned (7 bytes).
 *
 * @param seed_rev          The input reversed seed.
 * @param buf               Buffer to return the 7 bytes PRBS.
 * @param reserve_output    Whether to reserve the output.
 *
 * @return unsigned short   Next reserved seed.
 *
 * @sa prbs15_next_28_bits()
 */
static unsigned short prbs15_next_7_bytes(unsigned short  seed_rev,
					  unsigned char   buf[PRBS15_PROCESS_BYTES],
					  int       reserve_output)
{
	unsigned short next_seed;
	unsigned int prbs_bits_0_27, prbs_bits_28_55;

	prbs_bits_0_27 = prbs15_next_28_bits(seed_rev, &next_seed);
	prbs_bits_28_55 = prbs15_next_28_bits(next_seed, &next_seed);

	if (reserve_output) {
		prbs_bits_0_27 = ~prbs_bits_0_27;
		prbs_bits_28_55 = ~prbs_bits_28_55;
	}

	buf[0]  = 0xFF & (prbs_bits_0_27);
	buf[1]  = 0xFF & (prbs_bits_0_27 >> 8);
	buf[2]  = 0xFF & (prbs_bits_0_27 >> 16);
	/* buf[3] is 24..27 | 28..31                                            */
	buf[3]  = (prbs_bits_0_27 >> 24) | (0xFF & (prbs_bits_28_55 << 4));
	buf[4]  = 0xFF & (prbs_bits_28_55 >> 4);
	buf[5]  = 0xFF & (prbs_bits_28_55 >> 12);
	buf[6]  = 0xFF & (prbs_bits_28_55 >> 20);

	return next_seed;
}

/*
 * PUBLIC FUNCTIONS
 */

unsigned short prbs15_gen(unsigned short polynomial,
			  unsigned short seed,
			  unsigned char *buf,
			  int      length,
			  int      reverse_output)
{
	unsigned short seed_rev;

	if (PRBS_POLYNOMIAL_DEFAULT == polynomial) {
		polynomial = 0xC001;
	}

	ASSERT((0xC001 == polynomial)           /* only support 0xC001          */
		&& (0 == (seed & 0x8000))       /* seed should only get 15 bits */
		&& (NULL != buf) && (length > 0));

	seed_rev = reverse_bit16(seed);
	seed_rev >>= 1;                         /* only 15 bits in the buffer   */

	while (length > 0) {
		if (length >= PRBS15_PROCESS_BYTES) {
			seed_rev = prbs15_next_7_bytes(seed_rev, buf, reverse_output);
			buf += PRBS15_PROCESS_BYTES;
			length -= PRBS15_PROCESS_BYTES;
		} else {
			/* If the buf is not enough to do prbs15_next_7_bytes(), then we
			* buffer the data, then copy to buf[].
			*/
			int i;
			unsigned char cur_prbs_buf[PRBS15_PROCESS_BYTES];
			seed_rev = prbs15_next_7_bytes(seed_rev, cur_prbs_buf, reverse_output);

			for (i=0; i<length; i++) {
				buf[i] = cur_prbs_buf[i];
			}

			length = 0;
		}
	}

	/* reverse the seed_rev                                                 */
	seed = reverse_bit16(seed_rev);
	seed >>= 1;                             /* only 15 bits in the buffer   */

	return seed;
}

/*
 * UNIT_TEST
 */

#ifdef UNIT_TEST

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>
#include <time.h>

void prbs15_next_28_bits_ut(void)
{
	unsigned short seed_rev = 0x2B75, next_seed_rev;
	unsigned int prbs;

	prbs = prbs15_next_28_bits(seed_rev, &next_seed_rev);
	assert (0xD47ECF == prbs);
	assert (0x06A3 == next_seed_rev);

	prbs = prbs15_next_28_bits(0x06A3, &next_seed_rev);
	assert (0x38585F2 == prbs);
	assert (0x1C2C == next_seed_rev);
}

void prbs15_next_7_bytes_ut(void)
{
	unsigned short seed_rev = 0x2B75, next_seed_rev;
	unsigned char prbs[PRBS15_PROCESS_BYTES];
	unsigned char prbs_expect[] = {0xCF, 0x7E, 0xD4, 0x20, 0x5F, 0x58, 0x38};

	next_seed_rev = prbs15_next_7_bytes(seed_rev, prbs, FALSE);

	assert (0x1C2C == next_seed_rev);
	assert (0 == memcmp(prbs, prbs_expect, sizeof(prbs_expect)));
}

#define PRBS_TEST_LENGTH        (8192 + 640)
static unsigned char g_prbs_buf[PRBS_TEST_LENGTH];

void prbs15_gen_ut(void)
{
	int i;
	unsigned short seed = 0x576A, next_seed;
	unsigned char prbs_expect[] = {0xCF, 0x7E, 0xD4, 0x20, 0x5F, 0x58, 0x38};

	next_seed = prbs15_gen(PRBS_POLYNOMIAL_DEFAULT, seed,
		g_prbs_buf, PRBS_TEST_LENGTH, FALSE);

	for (i=0; i<PRBS_TEST_LENGTH; i++) {
		printf("%02X ", g_prbs_buf[i]);
		if (0 == (i % 16)) {
			printf("\n");
		}
	}

	printf("\n");

	assert (0 == memcmp(g_prbs_buf, prbs_expect, sizeof(prbs_expect)));
}

#define ARRAYSIZE(a)                    (sizeof(a)/sizeof(*a))
void prbs15_gen_all_ut(void)
{
	FILE *fp = NULL;
	unsigned short seed, next_seed;
	int seed_index;
	clock_t start;
	/* Generate all the PRBS in SAMSUNG, Recommendation for a randomizer v0.1,
	* and do the performance test.
	*/
	const unsigned short seed_table[] = {
		0x576A, 0x05E8, 0x629D, 0x45A3, 0x649C, 0x4BF0, 0x2342, 0x272E,
		0x7358, 0x4FF3, 0x73EC, 0x5F70, 0x7A60, 0x1AD8, 0x3472, 0x3612,
		0x224F, 0x0454, 0x030E, 0x70A5, 0x7809, 0x2521, 0x48F4, 0x5A2D,
		0x492A, 0x043D, 0x7F61, 0x3969, 0x517A, 0x3B42, 0x769D, 0x0647,
		0x7E2A, 0x1383, 0x49D9, 0x07B8, 0x2578, 0x4EEC, 0x4423, 0x352F,
		0x5B22, 0x72B9, 0x367B, 0x24B6, 0x7E8E, 0x2318, 0x6BD0, 0x5519,
		0x1783, 0x18A7, 0x7B6E, 0x7602, 0x4B7F, 0x3648, 0x2C53, 0x6B99,
		0x0C23, 0x67CF, 0x7E0E, 0x4D8C, 0x5079, 0x209D, 0x244A, 0x747B,
		0x350B, 0x0E4D, 0x7004, 0x6AC3, 0x7F3E, 0x21F5, 0x7A15, 0x2379,
		0x1517, 0x1ABA, 0x4E77, 0x15A1, 0x04FA, 0x2D61, 0x253A, 0x1302,
		0x1F63, 0x5AB3, 0x049A, 0x5AE8, 0x1CD7, 0x4A00, 0x30C8, 0x3247,
		0x729C, 0x5034, 0x2B0E, 0x57F2, 0x00E4, 0x575B, 0x6192, 0x38F8,
		0x2F6A, 0x0C14, 0x45FC, 0x41DF, 0x38DA, 0x7AE1, 0x7322, 0x62DF,
		0x5E39, 0x0E64, 0x6D85, 0x5951, 0x5937, 0x6281, 0x33A1, 0x6A32,
		0x3A5A, 0x2BAC, 0x743A, 0x5E74, 0x3B2E, 0x7EC7, 0x4FD2, 0x5D28,
		0x751F, 0x3EF8, 0x39B1, 0x4E49, 0x746B, 0x6EF6, 0x44BE, 0x6DB7
	};

	fp = fopen("samsung_nand_prbs15_128pages_8192_640.bin", "wb");

	start = clock();
	for (seed_index=0; seed_index<ARRAYSIZE(seed_table); seed_index++) {
		seed = seed_table[seed_index];
		next_seed = prbs15_gen(PRBS_POLYNOMIAL_DEFAULT, seed,
			g_prbs_buf, PRBS_TEST_LENGTH, FALSE);
		if (fp) {
			fwrite(g_prbs_buf, 1, PRBS_TEST_LENGTH, fp);
		}
	}

	printf("cost %dms\n", clock()-start);

	if (fp) {
		fclose(fp);
	}
}

int main(int argc, const char* argv[])
{
	prbs15_next_28_bits_ut();
	prbs15_next_7_bytes_ut();
	prbs15_gen_ut();
	prbs15_gen_all_ut();

	return 0;
}

#endif /* UNIT_TEST */
