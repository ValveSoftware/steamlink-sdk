/** Randomize the data in NAND.
 *
 * At present, it only supports SAMSUNG NAND randomization way at present.
 *
 * Some key points.
 * 1. Don't randomize bad block marker.
 * 2. Seed can use randomized data.
 *
 * Important things for modifications, for the follow things would change
 * the random data, and it would make the data won't be binary compatible.
 * 1. Initial Seed.
 * 2. Page start positions.
 * 3. Randomize or unrandomized some special bytes, such as bad block marker.
 *
 * @warning: This file is used in other places, so don't inlcude any
 *           kernel related files to it.
 *
 * Author: Yongsen Chen, YongsenChen@gmail.com
 */

/*
 * CONFIGURATIONS
 */
#define xUNIT_TEST

#ifndef ASSERT
#   define ASSERT(x)                /* {if (!(x))  while (1);} */
#endif /* ASSERT */

#define DO_TRACE_LOG                (0)

#if DO_TRACE_LOG
int printf(const char *fmt, ...);
#define TRACE_LOG               printf
#else
#define TRACE_LOG(...)
#endif

/* We use random seed to make it work for all chip that requires randomization.
 */
#define RANDOM_SEED

/*
 * INCLUDES
 */
#include "prbs.h"
#include "nand_randomizer.h"

/*
 * CONSTANTS
 */
#ifndef TRUE
#define TRUE				(1)
#endif

#ifndef FALSE
#define FALSE				(0)
#endif

#ifndef NULL
#define NULL				(0)
#endif


#define NAND_ID_MAX_SIZE                (8)
#define NAND_DEFAULT_OOB_SIZE		(32)

#define IS_POWER_OF_2(x)                ( !( (x) & ((x)-1) ) )
#define ARRAYSIZE(a)                    (sizeof(a)/sizeof(*a))

#define PAGE_START_POS_ALIGNED_BYTES    (4)
#define PAGE_START_POS_MASK(cycle_len)	                                \
	((cycle_len) - 1) & (~(PAGE_START_POS_ALIGNED_BYTES - 1))

/** NAND randomizer types.
 *
 * We only support SAMSUNG PRBS-15 at present.
 * @sa SAMSUNG Recommendation for a randomizer v0.1
 */
enum mv_nand_randomizer_type_e {
	MV_NAND_RANDOMIZER_FROM_MEMORY,     /** Use the input buffer directly   */
	MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,  /** Generate by PRBS-15 with seeds
                                             *  defined by SAMSUNG.
                                             */
	MV_NAND_RANDOMIZER_TYPE_MAX
};

/*
 * TYPES
 */

/** NAND Randomizer chip information.
 */
struct nand_randomized_chip_info_s {
	int chip_id_len;
	unsigned char chip_id[NAND_ID_MAX_SIZE];
	unsigned int block_size;
	unsigned int page_size;
	unsigned int spare_size;
	enum mv_nand_randomizer_type_e randomizer_type;
	unsigned int randomizer_buffer_length;
};


/** NAND Randomizer.
 */
struct mv_nand_randomizer_s {
	int          chip_randomized;
	struct nand_randomized_chip_info_s *chip_info;

	unsigned int block_size;
	unsigned int page_size;
	unsigned int oob_size;

	unsigned int page_per_block;
	unsigned int block_shift;
	unsigned int page_mask;

	unsigned int random_data_buffer_length;
	unsigned char *p_random_data_buffer;
};

/*
 * VARIABLES
 */
static const struct nand_randomized_chip_info_s g_nand_randomized_chip_list[] = {
	/* SAMSUNG K9GBG08U0A, 32Gb */
	{
		6,
		{0xEC, 0xD7, 0x94, 0x7A, 0x54, 0x43},
		8192 * 128,
		8192,
		640,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* SAMSUNG K9GBG08U0B, 32Gb */
	,{
		6,
		{0xEC, 0xD7, 0x94, 0x7E, 0x64, 0x44},
		8192 * 128,
		8192,
		1024,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* TOSHIBA TC58NVG4D2HTA00, 16Gb */
	,{
		6,
		{0x98, 0xD5, 0x84, 0x32, 0x72, 0x56},
		8192 * 128,
		8192,
		640,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* SAMSUNG K9LCG08U0A/K9HDG08U1A, 64Gb/128Gb
	 * K9HDG08U1A consist 2 pieces of K9LCG08U0A, 1 CE(Chip Enable) pin
	 * for 1 piece.
	 */
	,{
		6,
		{0xEC, 0xDE, 0xD5, 0x7A, 0x58, 0x43},
		8192 * 128,
		8192,
		640,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* MICRON MT29F32G08CBxBx/MT29F64G08CxxBx, 32Gb/64Gb */
	,{
		5,
		{0x2C, 0x68, 0x04, 0x46, 0x89},
		4096 * 256,
		4096,
		224,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* MICRON MT29F16G08CBACA, 16Gb */
	,{
		5,
		{0x2C, 0x48, 0x04, 0x4A, 0xA5},
		4096 * 256,
		4096,
		224,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* HYNIX H27UBG8T2BTR, 32Gb */
	,{
		6,
		{0xAD, 0xDE, 0x94, 0xD2, 0x04, 0x43},
		8192 * 256,
		8192,
		448,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* HYNIX H27UCG8T2ATR, 64Gb */
	,{
		6,
		{0xAD, 0xDE, 0x94, 0xDA, 0x74, 0xC4},
		8192 * 256,
		8192,
		640,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}
	/* HYNIX H27UAG8T2CTR, 16Gb */
	,{
		6,
		{0xAD, 0xD5, 0x94, 0xDA, 0x74, 0xC4},
		8192 * 256,
		8192,
		640,
		MV_NAND_RANDOMIZER_SAMSUNG_PRBS15,
		4096
	}

};

/* Samsung randomizer.
 */
#ifndef RANDOM_SEED
static const unsigned short g_nand_randomozer_seed_start_pos_table_samsung [] = {
	   0,   32, 1020, 2564, 2664, 3408, 1300, 3048,
	 848,  332,  860,  804,  800, 3816, 3064, 3096,
	1136, 3208,  304, 3264,  500, 3944, 2348, 1260,
	3884,  516, 1280, 3980,  176, 2552, 2584,  648,
	1832, 1928,  656,  224, 1848,  340, 2544,  988,
	2468,  292,  224, 2968, 3000, 1168, 2000,   36,
	 792, 1972, 3684,  928, 4052, 2752, 1016,  440,
	2412,  176, 2184, 2216, 1824, 2536, 2456, 2992,
	1848, 2120,  184, 2484, 2220, 1324, 2056, 3472,
	3544, 3576, 3600,  556, 3428, 2936,  336, 2284,
	 804, 3620, 3140, 1660, 2888, 3476,   44, 1816,
	1848,  408, 3864, 1592, 2896,  452, 3564, 1916,
	3300, 1852,  172,  964,  804,  296,  328, 1052,
	2168, 2180,  356, 4064, 2096, 1228, 2484, 2204,
	1996, 1192,  984, 1052, 3356, 3388, 2368, 3260,
	2308, 1920, 1304, 2732, 3744, 3112, 3256,  732
};

#define NAND_RANDOMIZER_SEED_COUNT_SAMSUNG                                  \
	ARRAYSIZE(g_nand_randomozer_seed_start_pos_table_samsung)

#endif /* RANDOM_SEED */

#define NAND_RANDOMIZER_SEED_SAMSUNG                (0x576A)

/* Global randomizer. */
static struct mv_nand_randomizer_s g_nand_randomizer;


/*
 * PRIVATE FUNCTIONS
 */

#define DUMP_8_BYTES(name, b)							\
	if (b) {                                                                \
		TRACE_LOG("%s(0x%08X): %02X %02X %02X %02X %02X %02X %02X %02X\n", \
		name, (int)(b),                                                 \
		(b)[0], (b)[1], (b)[2], (b)[3], (b)[4], (b)[5], (b)[6], (b)[7]);\
	}


/** Get shift of x.
 */
static unsigned int get_shift(unsigned int x)
{
	unsigned int s = 0;
	ASSERT(x > 0 && IS_POWER_OF_2(x));

	for (s=0; s<sizeof(x)*8; s++) {
		if ((1 << s) == x) {
			break;
		}
	}
	return s;
}

/** Get the random data by PRBS generator
 *
 */
static unsigned int gen_prbs15_random_data_samsung(unsigned char *p_random_data_buffer,
						   unsigned int   random_data_length)
{
	unsigned short seed = NAND_RANDOMIZER_SEED_SAMSUNG;

	ASSERT(p_random_data_buffer);
	ASSERT(random_data_length);

	prbs15_gen(PRBS_POLYNOMIAL_DEFAULT,
		   seed,
		   p_random_data_buffer,
		   random_data_length,
		   FALSE);
	return random_data_length;
}

/** Randomize data by XOR way.
 *
 * For each byte, *p_dest = *p_random_data ^ *p_src.
 *
 * @param p_random_data             Random data.
 * @param p_src                     Source data.
 * @param p_dst                     Buffer to save the randomized data.
 * @param length                    Length of the data to randomize.
 *
 * @return void
 *
 * @sa mv_nand_randomizer_init(), mv_nand_randomizer_get_page().
 */
static void randomize_by_xor(const unsigned char *p_random_data,
			     const unsigned char *p_src,
			     unsigned char       *p_dst,
			     int                  length)
{
	int i;
	const int aligned_mask = sizeof(unsigned int) - 1;
	ASSERT(p_random_data);
	ASSERT(p_src);
	ASSERT(p_dst);
	ASSERT(length > 0);

	/* if it's 4-byte aligned, then we use a faster way to do so            */
	if ((0 == ((int)p_random_data & aligned_mask)) &&
	    (0 == ((int)p_src & aligned_mask)) &&
	    (0 == ((int)p_dst & aligned_mask)) &&
	    (0 == (length & aligned_mask))) {
		const unsigned int *p_random_data_32 = (const unsigned int *)p_random_data;
		const unsigned int *p_src_32 = (const unsigned int *)p_src;
		unsigned int *p_dst_32 = (unsigned int *)p_dst;
		int length_32 = length/sizeof(*p_random_data_32);

		for (i=0; i<length_32; i++) {
			p_dst_32[i] = p_src_32[i] ^ p_random_data_32[i];
		}
	} else {
		for (i=0; i<length; i++) {
			p_dst[i] = p_src[i] ^ p_random_data[i];
		}
	}
}

/** It will randomize the data in ring.
 *
 * @param p_random_data             Random data.
 * @param random_data_length        Length of the random data.
 * @param p_src                     Source data.
 * @param p_dst                     Buffer to save the randomized data.
 * @param length                    Length of the data to randomize, must not
 *                                  be larger than a page_random_data_length.
 * @param start                     Start position in the page to randomize.
 *
 * @return void
 *
 * @sa randomize_by_xor()
 */
static void randomize_by_xor_ring(const unsigned char *p_random_data,
				  unsigned int         random_data_length,
				  const unsigned char *p_src,
				  unsigned char       *p_dst,
				  unsigned int         length,
				  unsigned int         start)
{
	unsigned int randomized_length = 0;
	unsigned int offset = start;

	/* make sure offset is in a page */
	while (offset >= random_data_length) {
		offset -= random_data_length;
	}

	while (randomized_length < length) {
		unsigned int processing_length = random_data_length - offset;
		if (processing_length > length - randomized_length) {
			processing_length = length - randomized_length;
		}

		if (offset != 0) {
			DUMP_8_BYTES("RANDOM ", p_random_data + offset);
		}

		randomize_by_xor(p_random_data + offset,
				 p_src + randomized_length,
				 p_dst + randomized_length,
				 processing_length);
		offset = 0;
		randomized_length += processing_length;
	}
}

/** Get the randomized chip info.
 *
 * @param p_chip_id             Chip ID data.
 * @param chip_id_len           Chip ID length.
 *
 * @retval  NULL                Chip is unrandomized.
 * @retval !NULL                Chip is randomized, return the chip info.
 *
 * @sa
 */
static const struct nand_randomized_chip_info_s*
mv_nand_get_chip_randomized_info(const unsigned char *p_chip_id,
				 int                  chip_id_len)
{
	const struct nand_randomized_chip_info_s *p_found_chip_info = NULL;
	int chip_index;

	/* If no chip_id input, then default is enabled	*/
	if (NULL == p_chip_id || chip_id_len <= 0) {
		TRACE_LOG("no chip_id input, use default randomizer.......\n");
		return &g_nand_randomized_chip_list[0];
	}

	for (chip_index=0; chip_index < ARRAYSIZE(g_nand_randomized_chip_list); chip_index++) {
		const struct nand_randomized_chip_info_s * p_chip_info =
			&g_nand_randomized_chip_list[chip_index];
		int compare_len = chip_id_len;
		int found = TRUE;
		int i;

		if ((0 == compare_len) ||
		    (compare_len > p_chip_info->chip_id_len)) {
			compare_len = p_chip_info->chip_id_len;
		}

		/* we don't use memcmp() or strncmp() to make the function get better
		 * compatibility.
		 */
		for (i=0; i<compare_len; i++) {
			if (p_chip_id[i] != p_chip_info->chip_id[i]) {
				found = FALSE;
				break;
			}
		}

		if (found) {
			p_found_chip_info = p_chip_info;
			break;
		}
	}

	TRACE_LOG("mv_nand_chip_randomized(%02X %02X %02X %02X %02X %02X): !!!%s!!!\n",
		  p_chip_id[0], p_chip_id[1], p_chip_id[2], p_chip_id[3],
		  p_chip_id[4], p_chip_id[5],
		  p_found_chip_info ? "RANDOMIZED" : "UNRANDOMIZED");

	return p_found_chip_info;
}

/** Check whether a block is randomized.
 *
 * @param block_index               Block index to check.
 *
 * @retval 0                        Chip is not randomized.
 * @retval !0                       Chip is randomized.
 *
 * @sa
 */
static int mv_nand_block_randomized(unsigned int block_index)
{
	/* Modify it to support other blocks */
	const unsigned int unrandomized_block_table[] = {
		/* block 0    */0,
		/* bootloader */1, 2, 3, 4, 5, 6, 7, 8
	};
	int i;
	int randomized = TRUE;

	for (i=0; i<ARRAYSIZE(unrandomized_block_table); i++) {
		if (block_index == unrandomized_block_table[i]) {
			randomized = FALSE;
			break;
		}
	}
	return randomized;
}

/** Get the start random data pos for a page.
 *
 * @param page_addr                 Address of a page.
 *
 * @return unsigned int             Start pos in random data for the page.
 *
 * @sa
 */
static unsigned int mv_nand_get_page_start(unsigned int page_addr)
{
	struct mv_nand_randomizer_s * p_nr = &g_nand_randomizer;
	unsigned int page_index_in_block = page_addr & p_nr->page_mask;

#ifdef RANDOM_SEED
	unsigned int start;
	/* 2 for each pos need 2 bytes
	 * we don't use pos = page_addr * 2; because LEAPimg need repeat the seed
	 * in each block, for the which block is bad is unexpected.
	 */
	unsigned int pos = page_index_in_block * 2;
	pos = pos & (p_nr->random_data_buffer_length - 1);

	start = (p_nr->p_random_data_buffer[pos + 1] << 8) +
		p_nr->p_random_data_buffer[pos];

	start &= PAGE_START_POS_MASK(p_nr->random_data_buffer_length);
	/* & p_nr->random_data_buffer_length is to make sure it won't exceed random
	 * data length.
	 * & (~0x3) is to make sure it's 32-bit aligned
	 */

	return start;
#else /* !RANDOM_SEED */
	while (page_index_in_block >= NAND_RANDOMIZER_SEED_COUNT_SAMSUNG) {
		page_index_in_block -= NAND_RANDOMIZER_SEED_COUNT_SAMSUNG;
	}

	return g_nand_randomozer_seed_start_pos_table_samsung[page_index_in_block];
#endif /* RANDOM_SEED */
}

/** The function to do real randomization.
 *
 * It will be called by all randomization functions.
 *
 * @param page_addr                 Address of the page (start from 0).
 * @param p_src                     Source data.
 * @param p_dst                     Buffer to save the randomized data.
 * @param length                    Length of the data to randomize, must not
 *                                  be larger than a page_random_data_length.
 * @param offset_in_page            Start position in the page to randomize.
 *
 * @return void
 *
 * @sa
 */
static void mv_nand_do_randomize(unsigned int         page_addr,
				 const unsigned char *p_src,
				 unsigned char       *p_dst,
				 unsigned int         length,
				 unsigned int         offset_in_page)
{
	struct mv_nand_randomizer_s * p_nr = &g_nand_randomizer;

	int start = mv_nand_get_page_start(page_addr);
	randomize_by_xor_ring(p_nr->p_random_data_buffer,
			      p_nr->random_data_buffer_length,
			      p_src,
			      p_dst,
			      length,
			      start + offset_in_page);
}
/*
 * PUBLIC FUNCTIONS
 */

int mv_nand_chip_randomized(const unsigned char *p_chip_id,
			    int                  chip_id_len,
			    unsigned int        *p_randomizer_buffer_length)
{
	const struct nand_randomized_chip_info_s *p_randomized_chip_info = NULL;
	p_randomized_chip_info = mv_nand_get_chip_randomized_info(p_chip_id, chip_id_len);

	if (p_randomizer_buffer_length) {
		*p_randomizer_buffer_length = 0;
	}

	if (!p_randomized_chip_info) {
		return FALSE;
	}

	if (p_randomizer_buffer_length) {
		*p_randomizer_buffer_length = p_randomized_chip_info->randomizer_buffer_length;
	}
	return TRUE;
}

int mv_nand_randomizer_init(const unsigned char *p_chip_id,
			    int                  chip_id_len,
			    unsigned int         block_size,
			    unsigned int         page_size,
			    unsigned int         oob_size,
			    unsigned char       *p_randomizer_buffer,
			    unsigned int         randomizer_buffer_length)
{
	struct mv_nand_randomizer_s *p_nr = &g_nand_randomizer;
	const struct nand_randomized_chip_info_s *p_randomized_chip_info = NULL;

	ASSERT(IS_POWER_OF_2(page_size));
	ASSERT(IS_POWER_OF_2(block_size));

	TRACE_LOG("mv_nand_randomizer_init(block_size=%d, page_size=%d, oob_size=%d, buf_len=%d)\n",
		  block_size, page_size, oob_size, randomizer_buffer_length);

	if (NULL == p_randomizer_buffer || 0 == randomizer_buffer_length) {
		mv_nand_chip_randomized(p_chip_id, chip_id_len, &randomizer_buffer_length);
		return randomizer_buffer_length;
	}

	p_nr->p_random_data_buffer      = p_randomizer_buffer;

	/* we initialize randomizer ASAP, to make sure the the randomizer can be initialized
	 * as unrandomized first.
	 */
	/*memset(p_nr, 0, sizeof(*p_nr));*/
	p_nr->chip_randomized           = FALSE;
	p_nr->chip_info                 = NULL;

	p_randomized_chip_info = mv_nand_get_chip_randomized_info(p_chip_id, chip_id_len);

	if (NULL == p_randomized_chip_info) {
		return 0;
	}

	if (0 == block_size) {
		block_size = p_randomized_chip_info->block_size;
	}

	if (0 == page_size) {
		page_size = p_randomized_chip_info->page_size;
	}

	if (0 == oob_size) {
		oob_size = NAND_DEFAULT_OOB_SIZE;
	}

	p_nr->block_size                = block_size;
	p_nr->page_size                 = page_size;
	p_nr->oob_size                  = oob_size;

	/* calculate the masks and shifts */
	p_nr->page_per_block = block_size / page_size;
	p_nr->page_mask = p_nr->page_per_block - 1;
	p_nr->block_shift = get_shift(p_nr->page_per_block);

	if (randomizer_buffer_length < p_randomized_chip_info->randomizer_buffer_length) {
		return -1;
	} else {
		randomizer_buffer_length = p_randomized_chip_info->randomizer_buffer_length;
	}

	if (MV_NAND_RANDOMIZER_SAMSUNG_PRBS15 == p_randomized_chip_info->randomizer_type) {
		gen_prbs15_random_data_samsung(p_randomizer_buffer,
					       randomizer_buffer_length);
	}

	ASSERT(IS_POWER_OF_2(randomizer_buffer_length));
	p_nr->chip_randomized = TRUE;
	p_nr->random_data_buffer_length = randomizer_buffer_length;
	return randomizer_buffer_length;
}

unsigned int mv_nand_randomizer_randomize(unsigned int         page_addr,
					  const unsigned char *p_src,
					  unsigned char       *p_dst,
					  unsigned int         length,
					  unsigned int         offset_in_page)
{
	const struct mv_nand_randomizer_s *p_nr = &g_nand_randomizer;
	unsigned int block_index = page_addr >> p_nr->block_shift;
	/*unsigned int page_index = page_addr & p_nr->page_mask;*/

	int need_set_bad_marker = FALSE;
	unsigned char bad_marker = 0;
	unsigned int bad_marker_pos = 0;
	ASSERT(p_src);
	ASSERT(p_dst);
	ASSERT(length > 0);

	if (!p_nr->chip_randomized || !mv_nand_block_randomized(block_index)) {
		return 0;
	}

	if ((offset_in_page <= p_nr->page_size) &&
	    ((offset_in_page + length) > p_nr->page_size)) {
		need_set_bad_marker = TRUE;
		bad_marker_pos = p_nr->page_size - offset_in_page;
		bad_marker = p_src[bad_marker_pos];
	}

	mv_nand_do_randomize(page_addr, p_src, p_dst, length, offset_in_page);

	if (need_set_bad_marker) {
		p_dst[bad_marker_pos] = bad_marker;
	}

	return length;
}

unsigned int mv_nand_randomizer_randomize_page(unsigned int         page_addr,
					       const unsigned char *p_data_src,
					       const unsigned char *p_oob_src,
					       unsigned char       *p_data_dst,
					       unsigned char       *p_oob_dst)
{
	const struct mv_nand_randomizer_s *p_nr = &g_nand_randomizer;
	unsigned int block_index = page_addr >> p_nr->block_shift;
#if DO_TRACE_LOG
	unsigned int page_index = page_addr & p_nr->page_mask;
#endif /* DO_TRACE_LOG */
	unsigned int randomized_length = 0;

	if (!p_nr->chip_randomized || !mv_nand_block_randomized(block_index)) {
		TRACE_LOG("nand_randomize_page(0x%08X): UNRONDOMIZED\n", page_addr);
		return 0;
	}

	TRACE_LOG("mv_nand_randomizer_randomize_page(0x%08X): block %d, page %d, !!! RANDOMIZED !!!.\n",
		  page_addr, block_index, page_index);

	if (p_data_src) {
		/* randomize page data  */
		DUMP_8_BYTES("DATA_SRC", p_data_src);
		mv_nand_do_randomize(page_addr, p_data_src, p_data_dst, p_nr->page_size, 0);
		randomized_length += p_nr->page_size;
		DUMP_8_BYTES("DATA_DST", p_data_dst);
	}

	if (p_oob_src) {
		unsigned char bad_marker = p_oob_src[0];
		/* randomize oob data   */
		DUMP_8_BYTES("OOB_SRC", p_oob_src);

		mv_nand_do_randomize(page_addr, p_oob_src, p_oob_dst, p_nr->oob_size, p_nr->page_size);
		/* we don't randomize the bad block marker  */
		p_oob_dst[0] = bad_marker;
		randomized_length += p_nr->oob_size;
		DUMP_8_BYTES("OOB_DST", p_oob_dst);
	}

	return randomized_length;
}

/*
 * UNIT TEST
 */

#ifdef UNIT_TEST

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>

#define BLOCK_SIZE                          (1024*1024)
#define PAGE_SIZE                           (8192)
#define OOB_SIZE                            (32)
#define TOTAL_RANDOM_DATA_LENGTH            (4096)

static unsigned char g_random_buffer[TOTAL_RANDOM_DATA_LENGTH];

void mv_nand_randomizer_ut(void)
{
#define TEST_DATA_LENGTH                    (8)
	const char chip_id[] = {0xEC, 0xD7, 0x94, 0x7A, 0x54, 0x43};
	unsigned int block_size = BLOCK_SIZE;
	unsigned int page_size = PAGE_SIZE;
	unsigned int oob_size = OOB_SIZE;

	unsigned int random_data_size;
	const unsigned char *p_page0_random_data = NULL;
	const unsigned char randomizer_buffer_date_expect[] = {
		0xCF, 0x7E, 0xD4, 0x20, 0x5F, 0x58, 0x38, 0x3A
	};
	const unsigned char page0_random_data_expect[PAGE_SIZE] = {
#ifdef RANDOM_SEED
		0xD4, 0x3F, 0x1F, 0x50, 0x08, 0x3C, 0x06, 0x91
#else /* !RANDOM_SEED */
		0xCF, 0x7E, 0xD4, 0x20, 0x5F, 0x58, 0x38, 0x3A
#endif /* RANDOM_SEED */
	};
	unsigned char randomized_data[PAGE_SIZE];
	const unsigned char randomized_data_expect[PAGE_SIZE] = {0};
	const unsigned char unrandomized_data_expect[PAGE_SIZE] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	unsigned int unrandomized_page_addr = 0;
	unsigned int randomized_page_addr = 0x1000;
	int randomized = FALSE;

	random_data_size = mv_nand_randomizer_init(
				   chip_id,
				   sizeof(chip_id),
				   0,
				   0,
				   oob_size,
				   g_random_buffer,
				   TOTAL_RANDOM_DATA_LENGTH
			   );
	assert(TOTAL_RANDOM_DATA_LENGTH == random_data_size);

	assert(0 == memcmp(g_random_buffer, randomizer_buffer_date_expect,
			   TEST_DATA_LENGTH));

	p_page0_random_data = page0_random_data_expect;
	memcpy(randomized_data, unrandomized_data_expect, TEST_DATA_LENGTH);
	randomize_by_xor(p_page0_random_data,
			 page0_random_data_expect, randomized_data, TEST_DATA_LENGTH);
	assert(0 == memcmp(randomized_data, randomized_data_expect, TEST_DATA_LENGTH));

	/* unrandomized page test */
	memcpy(randomized_data, unrandomized_data_expect, TEST_DATA_LENGTH);
	randomized = mv_nand_randomizer_randomize(unrandomized_page_addr,
			page0_random_data_expect, randomized_data, TEST_DATA_LENGTH, 0);
	assert(!randomized && 0 == memcmp(randomized_data, unrandomized_data_expect, TEST_DATA_LENGTH));

	memcpy(randomized_data, unrandomized_data_expect, TEST_DATA_LENGTH);
	randomized = mv_nand_randomizer_randomize_page(unrandomized_page_addr,
			page0_random_data_expect, NULL, randomized_data, NULL);
	assert(!randomized && 0 == memcmp(randomized_data, unrandomized_data_expect, TEST_DATA_LENGTH));

	/* randomized page test  */
	memcpy(randomized_data, unrandomized_data_expect, TEST_DATA_LENGTH);
	randomized = mv_nand_randomizer_randomize(randomized_page_addr,
			page0_random_data_expect, randomized_data, TEST_DATA_LENGTH, 0);
	assert(randomized && 0 == memcmp(randomized_data, randomized_data_expect, TEST_DATA_LENGTH));

	memcpy(randomized_data, randomized_data_expect, TEST_DATA_LENGTH);
	randomized = mv_nand_randomizer_randomize_page(randomized_page_addr,
			page0_random_data_expect, NULL, randomized_data, NULL);
	assert(randomized && 0 == memcmp(randomized_data, randomized_data_expect, TEST_DATA_LENGTH));

}


#define FILL_BYTE                           (0xAA)
static unsigned char g_data_src [PAGE_SIZE + OOB_SIZE];
static unsigned char g_data_dst [PAGE_SIZE + OOB_SIZE];
static unsigned char g_data_dst_prev [PAGE_SIZE + OOB_SIZE];
void mv_nand_randomizer_page_ut(unsigned int page_addr)
{
	/* randomized start from block 9 */
#define RANDOMIZED_START_PAGE_ADDR          (9 * BLOCK_SIZE / PAGE_SIZE)
	int randomized = FALSE;

	memset(g_data_src, FILL_BYTE, sizeof(g_data_src));

	randomized = mv_nand_randomizer_randomize_page(page_addr,
			g_data_src, g_data_src + PAGE_SIZE, g_data_dst, g_data_dst + PAGE_SIZE);

	if (page_addr < RANDOMIZED_START_PAGE_ADDR) {
		assert(!randomized);
	} else {
		assert(randomized
		       /* If the source fill with same data, then the randomized data would
		        * repeat in cycle of 4096 bytes, for the random data repeats in that
		        * cycle.
		        */
		       && 0 == memcmp(g_data_dst, g_data_dst + 4096, 4096)
		       /* The bad block marker should not be randomized
		        */
		       && FILL_BYTE == g_data_dst[PAGE_SIZE]
		       /* OOB data should repeat too (except the bad block marker).
		        */
		       && 0 == memcmp(g_data_dst + 1, g_data_dst + PAGE_SIZE + 1, OOB_SIZE - 1)
		       /* The random start position should change every time, so the randomized
		        * data should change too.
		        */
		       && 0 != memcmp(g_data_dst, g_data_dst_prev, PAGE_SIZE + OOB_SIZE)
		      );
	}
}

void mv_nand_randomizer_full_page_ut(void)
{
	unsigned int page_addr = 0;
	for (page_addr = 0; page_addr < 512*4096; page_addr++) {
		mv_nand_randomizer_page_ut(page_addr);
	}
}

void mv_nand_randomizer_dump_seed_pos_ut(void)
{
	unsigned int page_addr = 0;
	unsigned short *random_pos = (unsigned short *)g_random_buffer;

	for (page_addr = 0; page_addr < 8192; page_addr++) {
		unsigned int random_start = mv_nand_get_page_start(page_addr);
		unsigned int except_random_start = random_pos[page_addr % 128] & PAGE_START_POS_MASK(4096);
		printf("0x%08X: %d \t %d\n", page_addr, random_start, except_random_start);
		assert(random_start == except_random_start);
	}
}

int main(int argc, const char* argv[])
{
	mv_nand_randomizer_ut();
	mv_nand_randomizer_full_page_ut();
	mv_nand_randomizer_dump_seed_pos_ut();

	return 0;
}

#endif /* UNIT_TEST */
