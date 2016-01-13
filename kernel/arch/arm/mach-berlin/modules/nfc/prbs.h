#ifndef _PRBS_H_
#define _PRBS_H_
/** Generate the PRBS (Pseudo Random Binary Sequence).
 *
 * It's very simple to use it, just call prbs##_gen().
 *
 * At present, it supports the follow PRBS.
 * - PRBS-15: x^15 + x^14 + 1 (0xC001), Period 2^15 - 1 (32767) bits
 * - PRBS-23: x^23 + x^18 + 1 (0x84001), Period 2^23 -1 (8388607) bits - TODO
 * - PRBS-32: x^31 + x^28 + 1 (0x900001), Period 2^31 - 1 (2147483647) bits - TODO
 *
 *
 * @ref ITU-T Recommendation O.150, 05/96
 * @ref http://pl.wikipedia.org/wiki/PRBS
 * @ref http://en.wikipedia.org/wiki/Pseudorandom_binary_sequence
 * LFSR (Linear feedback shift register)
 * @ref http://en.wikipedia.org/wiki/LFSR
 *
 * Author: Yongsen Chen, YongsenChen@gmail.com
 */



#define PRBS_POLYNOMIAL_DEFAULT                     (0)


enum prbs_type {
	PRBS7			= 7,
	PRBS9			= 9,
	PRBS11			= 11,
	PRBS15			= 15,
	PRBS17			= 17,
	PRBS23			= 23,
	PRBS31			= 31
};

#define PRBS11_CYCLE_BITS			((1 << 11) - 1)
#define PRBS11_CYCLE_ALIGNED_BYTES		((PRBS11_CYCLE_BITS + 8 - 1)/8)

#define PRBS15_CYCLE_BITS			((1 << 15) - 1)
#define PRBS15_CYCLE_ALIGNED_BYTES		((PRBS15_CYCLE_BITS + 8 - 1)/8)

#define PRBS17_CYCLE_BITS			((1 << 17) - 1)
#define PRBS17_CYCLE_ALIGNED_BYTES		((PRBS17_CYCLE_BITS + 8 - 1)/8)

#define PRBS23_CYCLE_BITS			((1 << 23) - 1)
#define PRBS23_CYCLE_ALIGNED_BYTES		((PRBS23_CYCLE_BITS + 8 - 1)/8)

#define PRBS31_CYCLE_BITS			((1 << 31) - 1)
#define PRBS31_CYCLE_ALIGNED_BYTES		((PRBS31_CYCLE_BITS + 8 - 1)/8)


#ifdef __cplusplus
extern "C"  {
#endif /* __cplusplus */

/** Generate PRBS-15 data by polynomial and seed.
 *
 * @param polynomial        PRBS polynomial, only support 0xC001 at present.
 * @param seed              Seed of the PRBS.
 * @param buf               Buffer to save the result.
 * @param length            Length in bytes to generate.
 * @param reverse_output    Whether to reverse the output bits.
 *
 * @return unsigned short   Next seed or last data in LFSR buffer.
 *
 */
unsigned short prbs15_gen(unsigned short polynomial,
			  unsigned short seed,
			  unsigned char *buf,
			  int            length,
			  int            reverse_output);

#ifdef __cplusplus
}      /* extern "C"  */
#endif /* __cplusplus */

#endif /* _PRBS_H_ */
