#ifndef _NAND_RANDOMIZER_H_
#define _NAND_RANDOMIZER_H_
/** Randomizer API.
 *
 * Follow the steps to use it.
 * step 1, call mv_nand_randomizer_init() to initialize the randomizer.
 * step 2, loop calling mv_nand_randomizer_randomize_page() or
 *         mv_nand_randomizer_randomize() to randomize the pages.
 *
 * Take the unit test in nand_randomize.c as an example.
 *
 * Author: Yongsen Chen, YongsenChen@gmail.com
 */

#define MV_NAND_RANDOMIZER_BUFFER_SIZE_MAX      (4096)


#ifdef __cplusplus
extern "C"  {
#endif /* __cplusplus */

/** Check whether the NAND chip is randomized.
 *
 * @param p_chip_id                 Chip ID data.
 * @param chip_id_len               Chip ID length.
 * @param p_randomizer_buffer_length Field to return the buffer length required
 *                                  for the randomizer. NULL for not return.
 *
 * @retval 0                        Chip is not randomized.
 * @retval !0                       Chip is randomized.
 *
 * @sa mv_nand_randomizer_init()
 */
int mv_nand_chip_randomized(const unsigned char *p_chip_id,
			    int                  chip_id_len,
			    unsigned int        *p_randomizer_buffer_length);

/** Initialize the global NAND randomizer.
 *
 * @param p_chip_id                 Chip ID data. If it's NULL, then it will
 *                                  use a default randomization pattern.
 * @param chip_id_len               Chip ID length.
 * @param block_size                Block size of the chip, must be power of 2.
 * @param page_size                 Page size of the chip, must be power of 2.
 * @param oob_size                  OOB size.
 * @param p_randomizer_buffer       Randomizer buffer. If it's NULL, it will
 *                                  only return the randomizer buffer length.
 * @param randomizer_buffer_length  Length of the randomizer buffer.
 *
 * @retval <0                       Invalid inputs.
 * @retval 0                        The chip is not randomized.
 * @retval >0                       Size of real valid data in random buffer.
 *
 * @sa mv_nand_chip_randomized(), mv_nand_randomizer_get_page()
 */
int mv_nand_randomizer_init(const unsigned char *p_chip_id,
			    int                  chip_id_len,
			    unsigned int         block_size,
			    unsigned int         page_size,
			    unsigned int         oob_size,
			    unsigned char       *p_randomizer_buffer,
			    unsigned int         randomizer_buffer_length);

/** Randomize a page (with OOB).
 *
 * Before calling it, the global randomizer must be initialized by
 * mv_nand_randomizer_init() first.
 *
 * @param page_addr                 Address of the page (start from 0).
 * @param p_data_src                Page data source, size must be page_size.
 *                                  NULL for not randomize page data.
 * @param p_oob_src                 OOB data source, size must be oob_size.
 *                                  NULL for not randomize OOB data.
 * @param p_data_dst                Randomized page data, size must be page_size.
 * @param p_oob_dst                 Randomized OOB data, size must be oob_size.
 *
 * @retval =0                       The page is not randomized.
 * @retval >0                       Size of data has been randomized, including
 *                                  both page and OOB data.
 *
 * @note If you want to randomize data in any position in a page, you should use
 *       mv_nand_randomizer_randomize().
 *
 * @sa mv_nand_randomizer_init(), mv_nand_randomizer_randomize()
 */
unsigned int mv_nand_randomizer_randomize_page(unsigned int         page_addr,
					       const unsigned char *p_data_src,
					       const unsigned char *p_oob_src,
					       unsigned char       *p_data_dst,
					       unsigned char       *p_oob_dst);

/** Randomize data in a page.
 *
 * Before calling it, the global randomizer must be initialized by
 * mv_nand_randomizer_init() first.
 *
 * @param page_addr                 Address of the page (start from 0).
 * @param p_src                     Source data.
 * @param p_dst                     Buffer to save the randomized data.
 * @param length                    Length of the data to randomize, must not
 *                                  be larger than a page_random_data_length.
 * @param offset_in_page            Start position in the page to randomize.
 *
 * @retval <0                       Parameter input error.
 * @retval =0                       The page is not randomized.
 * @retval >0                       Size of data has been randomized.
 *
 * @sa mv_nand_randomizer_init(), mv_nand_randomizer_randomize_page()
 */
unsigned int mv_nand_randomizer_randomize(unsigned int         page_addr,
					  const unsigned char *p_src,
					  unsigned char       *p_dst,
					  unsigned int         length,
					  unsigned int         offset_in_page);

#ifdef __cplusplus
}      /* extern "C"  */
#endif /* __cplusplus */

#endif /* _NAND_RANDOMIZER_H_ */
