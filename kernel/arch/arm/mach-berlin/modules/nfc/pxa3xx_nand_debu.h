#ifndef __ASM_ARCH_PXA3XX_NAND_H
#define __ASM_ARCH_PXA3XX_NAND_H

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>

#include <linux/io.h>
#include <linux/mtd/nand.h>
#include "api_dhub.h"
#include "pBridge.h"
#include <mach/galois_platform.h>


#define READ_DATA_CHAN_CMD_BASE		(0)
#define READ_DATA_CHAN_CMD_SIZE		(2 << 3)
#define READ_DATA_CHAN_DATA_BASE	(READ_DATA_CHAN_CMD_BASE + READ_DATA_CHAN_CMD_SIZE)
#define READ_DATA_CHAN_DATA_SIZE	(32 << 3)

#define WRITE_DATA_CHAN_CMD_BASE	(READ_DATA_CHAN_DATA_BASE + READ_DATA_CHAN_DATA_SIZE)
#define WRITE_DATA_CHAN_CMD_SIZE	(2 << 3)
#define WRITE_DATA_CHAN_DATA_BASE	(WRITE_DATA_CHAN_CMD_BASE + WRITE_DATA_CHAN_CMD_SIZE)
#define WRITE_DATA_CHAN_DATA_SIZE	(32 << 3)

#define DESCRIPTOR_CHAN_CMD_BASE	(WRITE_DATA_CHAN_DATA_BASE + WRITE_DATA_CHAN_DATA_SIZE)
#define DESCRIPTOR_CHAN_CMD_SIZE	(2 << 3)
#define DESCRIPTOR_CHAN_DATA_BASE	(DESCRIPTOR_CHAN_CMD_BASE + DESCRIPTOR_CHAN_CMD_SIZE)
#define DESCRIPTOR_CHAN_DATA_SIZE	(32 << 3)

#define NFC_DEV_CTL_CHAN_CMD_BASE	(DESCRIPTOR_CHAN_DATA_BASE + DESCRIPTOR_CHAN_DATA_SIZE)
#define NFC_DEV_CTL_CHAN_CMD_SIZE	(2 << 3)
#define NFC_DEV_CTL_CHAN_DATA_BASE	(NFC_DEV_CTL_CHAN_CMD_BASE + NFC_DEV_CTL_CHAN_CMD_SIZE)
#define NFC_DEV_CTL_CHAN_DATA_SIZE	(32 << 3)

#define	CHIP_DELAY_TIMEOUT	(2 * HZ/10)
#define NAND_STOP_DELAY		(2 * HZ/50)
#define PAGE_CHUNK_SIZE		(2048)
#define OOB_CHUNK_SIZE		(64)
#define CMD_POOL_SIZE		(5)
#define BCH_THRESHOLD           (0)
#define BCH_STRENGTH		(4)
#define HAMMING_STRENGTH	(1)

#define DMA_H_SIZE	(32*1024)

/* registers and bit definitions */
#define NDCR		(0x00) /* Control register */
#define NDTR0CS0	(0x04) /* Timing Parameter 0 for CS0 */
#define NDTR1CS0	(0x0C) /* Timing Parameter 1 for CS0 */
#define NDSR		(0x14) /* Status Register */
#define NDPCR		(0x18) /* Page Count Register */
#define NDBDR0		(0x1C) /* Bad Block Register 0 */
#define NDBDR1		(0x20) /* Bad Block Register 1 */
#define NDECCCTRL	(0x28) /* ECC Control Register */
#define NDDB		(0x40) /* Data Buffer */
#define NDCB0		(0x48) /* Command Buffer0 */
#define NDCB1		(0x4C) /* Command Buffer1 */
#define NDCB2		(0x50) /* Command Buffer2 */
#define NDCB3		(0x54) /* Command Buffer3 */

#define NDCR_SPARE_EN		(0x1 << 31)
#define NDCR_ECC_EN		(0x1 << 30)
#define NDCR_DMA_EN		(0x1 << 29)
#define NDCR_ND_RUN		(0x1 << 28)
#define NDCR_DWIDTH_C		(0x1 << 27)
#define NDCR_DWIDTH_M		(0x1 << 26)
#define NDCR_PAGE_SZ		(0x1 << 24)
#define NDCR_NCSX		(0x1 << 23)
#define NDCR_FORCE_CSX          (0x1 << 21)
#define NDCR_CLR_PG_CNT		(0x1 << 20)
#define NDCR_STOP_ON_UNCOR	(0x1 << 19)
#define NDCR_RD_ID_CNT_MASK	(0x7 << 16)
#define NDCR_RD_ID_CNT(x)	(((x) << 16) & NDCR_RD_ID_CNT_MASK)

#define NDCR_RA_START		(0x1 << 15)
#define NDCR_PG_PER_BLK_MASK	(0x3 << 13)
#define NDCR_PG_PER_BLK(x)	(((x) << 13) & NDCR_PG_PER_BLK_MASK)
#define NDCR_ND_ARB_EN		(0x1 << 12)
#define NDCR_INT_MASK           (0xFFF)
#define NDCR_RDYM               (0x1 << 11)

#define NDSR_MASK		(0xffffffff)
#define NDSR_ERR_CNT_MASK       (0x1F << 16)
#define NDSR_ERR_CNT(x)         (((x) << 16) & NDSR_ERR_CNT_MASK)
#define NDSR_RDY                (0x1 << 12)
#define NDSR_FLASH_RDY          (0x1 << 11)
#define NDSR_CS0_PAGED		(0x1 << 10)
#define NDSR_CS1_PAGED		(0x1 << 9)
#define NDSR_CS0_CMDD		(0x1 << 8)
#define NDSR_CS1_CMDD		(0x1 << 7)
#define NDSR_CS0_BBD		(0x1 << 6)
#define NDSR_CS1_BBD		(0x1 << 5)
#define NDSR_DBERR		(0x1 << 4)
#define NDSR_SBERR		(0x1 << 3)
#define NDSR_WRDREQ		(0x1 << 2)
#define NDSR_RDDREQ		(0x1 << 1)
#define NDSR_WRCMDREQ		(0x1)

#define NDCB0_CMD_XTYPE_MASK	(0x7 << 29)
#define NDCB0_CMD_XTYPE(x)	(((x) << 29) & NDCB0_CMD_XTYPE_MASK)
#define NDCB0_LEN_OVRD		(0x1 << 28)
#define NDCB0_ST_ROW_EN         (0x1 << 26)
#define NDCB0_AUTO_RS		(0x1 << 25)
#define NDCB0_CSEL		(0x1 << 24)
#define NDCB0_CMD_TYPE_MASK	(0x7 << 21)
#define NDCB0_CMD_TYPE(x)	(((x) << 21) & NDCB0_CMD_TYPE_MASK)
#define NDCB0_NC		(0x1 << 20)
#define NDCB0_DBC		(0x1 << 19)
#define NDCB0_ADDR_CYC_MASK	(0x7 << 16)
#define NDCB0_ADDR_CYC(x)	(((x) << 16) & NDCB0_ADDR_CYC_MASK)
#define NDCB0_CMD2_MASK		(0xff << 8)
#define NDCB0_CMD1_MASK		(0xff)
#define NDCB0_ADDR_CYC_SHIFT	(16)

#define NDCB0_CMDX_MREAD	(0x0)
#define NDCB0_CMDX_MWRITE	(0x0)
#define NDCB0_CMDX_NWRITE_FINAL_CMD	(0x1)
#define NDCB0_CMDX_FINAL_CMD	(0x3)
#define NDCB0_CMDX_CMDP_WRITE	(0x4)
#define NDCB0_CMDX_NREAD	(0x5)
#define NDCB0_CMDX_NWRITE	(0x5)
#define NDCB0_CMDX_CMDP		(0x6)

#define NDCB0_CMD_READ		(0x0)
#define NDCB0_CMD_PROGRAM	(0x1)
#define NDCB0_CMD_ERASE		(0x2)
#define NDCB0_CMD_READID	(0x3)
#define NDCB0_CMD_STATUSREAD	(0x4)
#define NDCB0_CMD_RESET		(0x5)
#define NDCB0_CMD_NAKED_CMD	(0x6)
#define NDCB0_CMD_NAKED_ADDR	(0x7)

#define NDCB3_NDLENCNT_MASK	(0xffff)
#define NDCB3_NDLENCNT(x)	((x) & NDCB3_NDLENCNT_MASK)

/* ECC Control Register */
#define NDECCCTRL_ECC_SPARE_MSK (0xFF << 7)
#define NDECCCTRL_ECC_SPARE(x)  (((x) << 7) & NDECCCTRL_ECC_SPARE_MSK)
#define NDECCCTRL_ECC_THR_MSK   (0x3F << 1)
#define NDECCCTRL_ECC_THRESH(x) (((x) << 1) & NDECCCTRL_ECC_THR_MSK)
#define NDECCCTRL_BCH_EN        (0x1)
#define NDECCCTRL_ECC_STRENGTH_SHIFT (24)
#define NDECCCTRL_ECC_STRENGTH_MSK	(0xFF << 24)
#define NDECCCTRL_ECC_STRENGTH(x)	(((x) << 24) & NDECCCTRL_ECC_STRENGTH_MSK)


enum {
	MV_MTU_8_BYTE = 0,
	MV_MTU_32_BYTE,
	MV_MTU_128_BYTE,
	MV_MTU_1024_BYTE
};

/* error code and state */
enum {
	ERR_NONE	= 0,
	ERR_DMABUSERR	= -1,
	ERR_SENDCMD	= -2,
	ERR_DBERR	= -3,
	ERR_BBERR	= -4,
	ERR_SBERR	= -5,
	ERR_DMAMAPERR	= -6,
};

enum {
	STATE_IDLE = 0,
	STATE_PREPARED,
	STATE_CMD_HANDLE,
	STATE_DMA_READING,
	STATE_DMA_WRITING,
	STATE_DMA_DONE,
	STATE_PIO_READING,
	STATE_PIO_WRITING,
	STATE_CMD_DONE,
	STATE_READY,
};

enum {
	TYPE_SINGLE_PLANE = 0,
	TYPE_DUAL_PLANE,
	TYPE_E_SLC,
	TYPE_SLC,
	TYPE_UNKNOWN,
};


struct pxa3xx_nand_timing {
	unsigned int	tCH;  /* Enable signal hold time */
	unsigned int	tCS;  /* Enable signal setup time */
	unsigned int	tWH;  /* ND_nWE high duration */
	unsigned int	tWP;  /* ND_nWE pulse time */
	unsigned int	tRH;  /* ND_nRE high duration */
	unsigned int	tRP;  /* ND_nRE pulse width */
	unsigned int	tR;   /* ND_nWE high to ND_nRE low for read */
	unsigned int	tWHR; /* ND_nWE high to ND_nRE low for status read */
	unsigned int	tAR;  /* ND_ALE low to ND_nRE low delay */
};

struct pxa3xx_nand_cmdset {
	uint16_t	read1;
	uint16_t	read2;
	uint16_t	program;
	uint16_t	read_status;
	uint16_t	read_id;
	uint16_t	erase;
	uint16_t	reset;
	uint16_t	lock;
	uint16_t	unlock;
	uint16_t	lock_status;
	uint32_t	dual_plane_read;
	uint32_t	dual_plane_write;
	uint32_t	dual_plane_erase;
	uint32_t	dual_plane_randout;
};

struct pxa3xx_nand_flash {
	char		*name;
	uint16_t	chip_id;	/* chip id */
	uint16_t	ext_id;		/* Extend id */
	unsigned int	page_per_block; /* Pages per block (PG_PER_BLK) */
	unsigned int	page_size;	/* Page size in bytes (PAGE_SZ) */
	unsigned int	flash_width;	/* Width of Flash memory (DWIDTH_M) */
	unsigned int	dfc_width;	/* Width of flash controller(DWIDTH_C) */
	uint8_t         ecc_strength;   /* How strong ecc should be applied */
	unsigned int	num_blocks;	/* Number of physical blocks in Flash */

	struct pxa3xx_nand_timing *timing;	/* NAND Flash timing */

	uint8_t		has_dual_plane; /* Number of planes in Flash*/
};

/* The max num of chip select current support */
#define NUM_CHIP_SELECT		(2)
/*
 * The data flash bus is shared between the Static Memory
 * Controller and the Data Flash Controller,  the arbiter
 * controls the ownership of the bus
 */
#define PXA3XX_ARBI_EN		(1)
/* Newer controller support sending command in sperated stage */
#define PXA3XX_NAKED_CMD_EN	(1 << 1)
/* Tell driver whether enable DMA */
#define PXA3XX_DMA_EN		(1 << 2)
/* Tell driver whether keep configuration set in the boot loader */
#define PXA3XX_KEEP_CONFIG	(1 << 3)

#define PXA3XX_PDATA_MALLOC	(1 << 4)

#define HY_ESLC_REG_NUM	4

struct eslc_part {
	struct mtd_partition *part;
	struct list_head	list;
};

struct hynix_eslc_data {
	struct pxa3xx_nand *nand;
	uint8_t		eslc_def[HY_ESLC_REG_NUM];
	struct list_head	eslc_parts_qh;
	struct list_head	slc_parts_qh;
	const unsigned int *reg_addr;
	int page_addr;
};

struct pxa3xx_nand_slc {
	int	(*eslc_part_add)(void *priv, struct mtd_partition *part);
	int	(*eslc_start)(void *priv, uint cmd, int page);
	void	(*eslc_stop)(void *priv, uint cmd);
	loff_t	(*eslc_get_phy_off)(void *priv, loff_t off);

	int	(*slc_part_add)(void *priv, struct mtd_partition *part);
	int	(*slc_start)(void *priv, uint cmd, int page);
	void	(*slc_stop)(void *priv, uint cmd);
	loff_t	(*slc_get_phy_off)(void *priv, loff_t off);

	void	*priv;
};


struct pxa3xx_nand_info {
	struct nand_chip	nand_chip;

	struct pxa3xx_nand_cmdset *cmdset;
	/* page size of attached chip */
	uint16_t		page_size;
	uint8_t			chip_select;
	uint8_t			ecc_strength;

	int			page_shift;
	int			erase_shift;

	uint8_t			has_dual_plane;
	uint8_t			n_planes;
	uint8_t			eslc_mode;
	uint32_t		erase_size;

	/* calculated from pxa3xx_nand_flash data */
	uint8_t			col_addr_cycles;
	uint8_t			row_addr_cycles;
	uint8_t			read_id_bytes;

	/* cached register value */
	uint32_t		reg_ndcr;
	uint32_t		ndtr0cs0;
	uint32_t		ndtr1cs0;

	void			*nand_data;
};

struct pxa3xx_nand {
	void __iomem		*mmio_base;
	void __iomem            *pb_base;
	unsigned long		mmio_phys;
	struct nand_hw_control	controller;
	struct completion 	cmd_complete;
	struct platform_device	 *pdev;

	/* DMA information */
	HDL_dhub		PB_dhubHandle;
	unsigned int		mtu_size;
	unsigned int		mtu_type;
	dma_addr_t 		data_buff_phys;
	dma_addr_t		oob_buff_phys;
	dma_addr_t		drd_dma_addr;
	unsigned char		*drd_buf;

	unsigned char		*bounce_buffer;

	dma_addr_t 		data_desc_addr;
	unsigned char		*data_desc;
	int			data_desc_len;

	dma_addr_t 		read_desc_addr;
	unsigned char		*read_desc;
	int			read_desc_len[NUM_CHIP_SELECT];

	dma_addr_t 		write_desc_addr;
	unsigned char		*write_desc;
	int			write_desc_len[NUM_CHIP_SELECT];

	dma_addr_t		dp_read_desc_addr;
	unsigned char		*dp_read_desc;
	int			dp_read_desc_len[NUM_CHIP_SELECT];

	dma_addr_t 		dp_write_desc_addr;
	unsigned char		*dp_write_desc;
	int			dp_write_desc_len[NUM_CHIP_SELECT];

	int			transfer_units[NUM_CHIP_SELECT];
	int			transfer_units_last_trunk[NUM_CHIP_SELECT];
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	struct pxa3xx_nand_read_retry *retry;
#endif
	struct pxa3xx_nand_slc  *p_slc;
	struct pxa3xx_nand_info *info[NUM_CHIP_SELECT];
	struct pxa3xx_nand_info *info_dp[NUM_CHIP_SELECT];
	struct pxa3xx_nand_info *info_eslc[NUM_CHIP_SELECT];
	struct pxa3xx_nand_info *info_slc[NUM_CHIP_SELECT];

	uint8_t			chip_select;
	uint32_t		command;
	uint16_t		data_size;	/* data size in FIFO */
	uint16_t		oob_size;
	unsigned char		*data_buff;
	unsigned char		*oob_buff;
	/************
	 * following members will be reset as 0 before real IO
	 ************/
	uint32_t		buf_start;
	uint32_t		buf_count;
	uint16_t		data_column;
	uint16_t		oob_column;

	/* relate to the command */
	unsigned int		state;
	int			page_addr;
	uint16_t		column;
	unsigned int		ecc_strength;
	unsigned int		bad_count;
	int			use_mapped_buf;
	int			is_ready;
	int 			retcode;

	uint32_t		ndeccctrl;
	uint32_t		ndcr;

	/* generated NDCBx register values */
	uint8_t			total_cmds;
	uint8_t			cmd_seqs;
	uint8_t			wait_ready[CMD_POOL_SIZE];
	uint32_t		ndcb0[CMD_POOL_SIZE];
	uint32_t		ndcb1;
	uint32_t		ndcb2;
	uint32_t		chunk_oob_size[CMD_POOL_SIZE];
	uint32_t		curr_chunk;
};

struct pxa3xx_nand_platform_data {
	unsigned int				controller_attrs;

	/* indicate how many chip select would be used for this platform */
	int	cs_num;
	const struct mtd_partition		*parts[NUM_CHIP_SELECT];
	unsigned int				nr_parts[NUM_CHIP_SELECT];

	const struct pxa3xx_nand_flash * 	flash;
	size_t					num_flash;
};

#ifdef CONFIG_BERLIN_NAND_READ_RETRY
struct pxa3xx_nand_read_retry {
	uint8_t		rr_type;
	uint8_t 		rr_cycles;
	uint8_t 		rr_reg_nr;
	uint8_t 		rr_current_cycle;
	uint8_t		rr_table_ready;
	uint8_t 		*rr_table;
	int		(*set_rr)(struct pxa3xx_nand *nand, int rr_current_cycle);
	int		(*verify_rr)(struct pxa3xx_nand *nand);
};
#endif

enum {
	READ_DATA_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh0_intr,
	WRITE_DATA_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh1_intr,
	DESCRIPTOR_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh2_intr,
	NFC_DEV_CTL_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh3_intr,
	SPI_DEV_CTL_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh4_intr
};

#define pb_get_phys_offset(nand, off)	\
	((nand->mmio_phys + off) & 0x0FFFFFFF)

/* structure for DMA command buffer */
struct cmd_3_desc {
	SIE_BCMCFGW cfgw_ndcb0[3];
};
struct cmd_4_desc {
	SIE_BCMCFGW cfgw_ndcb0[4];
};

struct start_desc {
	SIE_BCMCFGW     ndcr_stop;
	SIE_BCMCFGW     ndeccctrl;
	SIE_BCMCFGW     ndsr_clear;
	SIE_BCMCFGW     ndcr_go;
};

struct reset_desc {
	SIE_BCMSEMA             sema_cmd;
	struct cmd_3_desc       cmd;
};

struct read_status_desc {
	SIE_BCMSEMA             sema_cmd;
	struct cmd_3_desc       cmd;
	SIE_BCMSEMA             sema_data;
	SIE_BCMWCMD             wcmd;
	SIE_BCMWDAT             wdat;
};

struct read_desc {
	SIE_BCMSEMA             sema_cmd1;
	struct cmd_3_desc       cmd1;
	SIE_BCMSEMA             sema_addr;
	struct cmd_3_desc       addr;
	SIE_BCMSEMA             sema_cmd2;
	struct cmd_3_desc       cmd2;
	/* chunks * (sema + cmd4 + units * (sema + data)) */
	unsigned char data_cmd[0];
};

struct dp_read_desc {
	SIE_BCMSEMA		sema_cmd1;
	struct cmd_3_desc	plane_1_cmd;
	SIE_BCMSEMA		sema_addr1;
	struct cmd_3_desc	plane_1_addr;
	SIE_BCMSEMA		sema_cmd2;
	struct cmd_3_desc	plane_2_cmd;
	SIE_BCMSEMA		sema_addr2;
	struct cmd_3_desc	plane_2_addr;
	SIE_BCMSEMA		sema_conf_cmd;
	struct cmd_3_desc	confirm_cmd;
	/* planes *   ( sema + cmd3 +
	 * 		sema + cmd3 +
	 * 		sema + cmd3 +
	 * 		sema + cmd3 +
	 * 		sema + cmd3 +
	 * 		chunk *	( sema + cmd4 +
	 * 			units * (sema + wcmd + wdat)))*/
	unsigned char		data_cmd[0];
};

/* macros for registers read/write */
#define nand_writel(nand, off, val)	\
	__raw_writel((val), (nand)->mmio_base + (off))

#define nand_readl(nand, off)		\
	__raw_readl((nand)->mmio_base + (off))
#define get_mtd_by_info(info)		\
	(struct mtd_info *)((void *)info - sizeof(struct mtd_info))
extern void pxa3xx_set_nand_info(struct pxa3xx_nand_platform_data *info);
extern int pxa3xx_read_page(struct mtd_info *mtd, uint8_t *buf, int page);
extern int pxa3xx_nand_intern_read(struct pxa3xx_nand *nand, uint8_t *buf, int page);
extern int pxa3xx_dummy_read(struct mtd_info *mtd, int page);

void mv88dexx_nand_dma_start(struct pxa3xx_nand_info *info, int rw);
void init_cmd_3(struct pxa3xx_nand *nand, struct cmd_3_desc *cmd);
void init_cmd_4(struct pxa3xx_nand *nand, struct cmd_4_desc *cmd);
void init_wcmd(struct pxa3xx_nand *nand, SIE_BCMWCMD *wcmd);
void init_wdat(struct pxa3xx_nand *nand, SIE_BCMWDAT *wdat);
void init_rcmd(struct pxa3xx_nand *nand, SIE_BCMRCMD *rcmd);
void init_rdat(struct pxa3xx_nand *nand, SIE_BCMRDAT *rdat);

void wait_dhub_ready(struct pxa3xx_nand *nand);
void init_sema_nfccmd(struct pxa3xx_nand *nand, SIE_BCMSEMA *sema);
void init_sema_nfcdat(struct pxa3xx_nand *nand, SIE_BCMSEMA *sema);

#endif /* __ASM_ARCH_PXA3XX_NAND_H */
