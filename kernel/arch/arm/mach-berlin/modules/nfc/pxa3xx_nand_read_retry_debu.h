#ifndef _PXA3XX_NAND_READ_RETRY_DEBU_
#define _PXA3XX_NAND_READ_RETRY_DEBU_
/** nand read retry API.
 *
 * all the read retry serial function here ,including various kinds of nand chip.
 * step 1, call read_rr function to read out the register parameter.
 * step 2, call set_rr to set these parameter to nand chip register when ecc fail in read operation
 *
 * Note:
 *
 * Author: Tianjun hu, hutj@marvell.com
 */

#include <linux/mtd/nand.h>

#define NAND_TIME_OUT	1000000
#define RR_CMD_LEN	5
#define RR_ADDR_LEN	5
#define RR_READ_LEN 2048


#define SLC_MODE	0x5aa5
#define MLC_MODE	0

enum {
	NO_READ_RETRY = 0,
	HYNIX_H27UCG8T_RR_MODE1,
	MICRON_MT29F32G08CBADA_RR,
} ;

int berlin_nand_rr_init(struct pxa3xx_nand *nand, struct nand_flash_dev *flash);
void berlin_nand_rr_exit(struct pxa3xx_nand *nand);


#endif /* _PXA3XX_NAND_READ_RETRY_DEBU_ */

