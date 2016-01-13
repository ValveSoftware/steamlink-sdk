/*
 * Driver for Marvell Berlin SoC ethernet
 * Copyright (C) 2010-2013 Jisheng Zhang <jszhang@marvell.com>
 *
 * Based on the MV643XX driver from:
 * Copyright (C) 2002 Matthew Dharm <mdharm@momenco.com>
 * Which is Based on the 64360 driver from:
 * Copyright (C) 2002 Rabeeh Khoury <rabeeh@galileo.co.il>
 *		      Rabeeh Khoury <rabeeh@marvell.com>
 *
 * Copyright (C) 2003 PMC-Sierra, Inc.,
 *	written by Manish Lachwani
 *
 * Copyright (C) 2003 Ralf Baechle <ralf@linux-mips.org>
 *
 * Copyright (C) 2004-2006 MontaVista Software, Inc.
 *			   Dale Farnsworth <dale@farnsworth.org>
 *
 * Copyright (C) 2004 Steven J. Hill <sjhill1@rockwellcollins.com>
 *				     <sjhill@realitydiluted.com>
 *
 * Copyright (C) 2007-2008 Marvell Semiconductor
 *			   Lennert Buytenhek <buytenh@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/system.h>


#define CONFIG_BERLIN_ASIC
#ifdef CONFIG_BERLIN_ASIC
#define PHY_ADDR 0x00
#define PHY_NEGOTIATION_WORKAROUND
#else
#define PHY_ADDR 0x01
#endif

#define MARVELL_PHY_ID1			0x0141
#define NATIONAL_PHY_ID1		0x2000

/*
 * Main per-port registers.  These live at offset 0x0400 for
 * port #0, 0x0800 for port #1, and 0x0c00 for port #2.
 */
#define ETH_EPAR			0x000
#define ETH_ESMIR			0x010
#define  ETH_SMIR_PHYADDR_SET(a)	((a) << 16)
#define  ETH_SMIR_REGADDR_SET(a)	((a) << 21)
#define  ETH_SMIR_OP_READ		(1 << 26)
#define  ETH_SMIR_READ_VALID		(1 << 27)
#define  ETH_SMIR_BUSY			(1 << 28)
#define ETH_EPCR			0x400
#define  ETH_PROMISCUOUS_MODE		(1 << 0)
#define  ETH_BROADCAST_REJECT_MODE    	(1 << 1)
#define  ETH_PORT_ENABLE		(1 << 7)
#define  ETH_HASH_SIZE_HALF_K		(1 << 12)
#define  ETH_HASH_FUNCTION_1		(1 << 13)
#define  ETH_HASH_PASS_MODE		(1 << 14)
#define  ETH_HD_DISABLE			(1 << 15)
#define ETH_EPCXR			0x408
#define  ETH_DP_AN_DISABLE		(1 << 9)
#define  ETH_FC_AN_DISABLE		(1 << 10)
#define  ETH_FLP_DISABLE		(1 << 11)
#define  ETH_FC_ENABLE			(1 << 12)
#define  ETH_MRU_ALL_MASK		(3 << 14)
#define  ETH_MRU_1518			(0 << 14)
#define  ETH_MRU_1536			(1 << 14)
#define  ETH_MRU_2048			(2 << 14)
#define  ETH_MRU_64K			(3 << 14)
#define  ETH_MIB_CLEAR			(1 << 16)
#define  ETH_SPEED_100			(1 << 18)
#define  ETH_SPEED_AN_DISABLE		(1 << 19)
#define  ETH_MAC_RX_2BSTUFF		(1 << 28)
#define ETH_EPSR        		0x418
#define  PORT_SPEED_MASK		(1 << 0)
#define  PORT_SPEED_10			(0 << 0)
#define  PORT_SPEED_100			(1 << 0)
#define  FULL_DUPLEX			(1 << 1)
#define  FLOW_CONTROL_ENABLED		(1 << 2)
#define  LINK_UP			(1 << 3)
#define  TX_IN_PROGRESS			(1 << 7)
#define ETH_ESPR        		0x420
#define ETH_EHTPR       		0x428
#define ETH_EFCSAL      		0x430
#define ETH_EFCSAH      		0x438
#define ETH_ESDCR			0x440
#define  ETH_BURST_SIZE_8_64BIT		(3 << 12)
#define  BLM_TX_LE			(1 << 7)
#define  BLM_RX_LE			(1 << 6)
#define  ETH_RX_FRAME_INTERRUPT		(1 << 9)
#define ETH_ESDCMR      		0x448
#define  ETH_RX_ENABLE             	(1 << 7)
#define  ETH_RX_ABORT			(1 << 15)
#define  ETH_TX_STOP_0			(1 << 16)
#define  ETH_TX_START_0			(1 << 23)
#define ETH_EICR        		0x450
#define  INT_TX				(3 << 2)
#define  INT_TX_0			(1 << 2)
#define  INT_TX_END			(3 << 6)
#define  INT_TX_END_0			(1 << 6)
#define  INT_RX_OVERRUN			(1 << 12)
#define  INT_TX_UNDERRUN		(1 << 13)
#define  INT_RX				(0xf << 16)
#define  INT_RX_0			(1 << 16)
#define  INT_MIIPSTC			(1 << 28)
#define ETH_EIMR        		0x458
#define ETH_EDSCP2P0L   		0x460
#define ETH_EDSCP2P0H   		0x464
#define ETH_EDSCP2P1L   		0x468
#define ETH_EDSCP2P1H   		0x46C
#define ETH_EFRDP0			0x480
#define ETH_ECRDP0			0x4A0
#define ETH_ECTDP0      		0x4E0

#define RXQ_CURRENT_DESC_PTR(q)		(ETH_ECRDP0 + ((q) << 2))
#define RXQ_FIRST_DESC_PTR(q)		(ETH_EFRDP0 + ((q) << 2))
#define TXQ_CURRENT_DESC_PTR(q)		(ETH_ECTDP0 + ((1 - q) << 2))

#define ETH_HASH_TABLE_ENTRY_VALID	(1 << 0)
#define ETH_HASH_TABLE_ENTRY_SKIP	(1 << 1)
#define ETH_HASH_TABLE_ENTRY_RECEIVE	(1 << 2)

/*
 * Misc per-port registers.
 */
#define MIB_COUNTERS(p)			(0x500 + ((p) << 7))


/*
 * SDMA configuration register default value.
 */
#if defined(__BIG_ENDIAN)
#define PORT_SDMA_CONFIG_DEFAULT_VALUE		\
		(ETH_BURST_SIZE_8_64BIT	|	\
		 0x3c			|	\
		 ETH_RX_FRAME_INTERRUPT)
#elif defined(__LITTLE_ENDIAN)
#define PORT_SDMA_CONFIG_DEFAULT_VALUE		\
		(ETH_BURST_SIZE_8_64BIT	|	\
		 BLM_RX_LE		|	\
		 BLM_TX_LE		|	\
		 0x3c			|	\
		 ETH_RX_FRAME_INTERRUPT)
#else
#error One of __BIG_ENDIAN or __LITTLE_ENDIAN must be defined
#endif


/*
 * Misc definitions.
 */
#define DEFAULT_RX_QUEUE_SIZE	128
#define DEFAULT_TX_QUEUE_SIZE	256
#define SKB_DMA_REALIGN		((PAGE_SIZE - NET_SKB_PAD) % SMP_CACHE_BYTES)


/*
 * RX/TX descriptors.
 */
struct rx_desc {
	u32 cmd_sts;		/* Descriptor command status		*/
	u16 byte_cnt;		/* Descriptor buffer byte count		*/
	u16 buf_size;		/* Buffer size				*/
	u32 buf_ptr;		/* Descriptor buffer pointer		*/
	u32 next_desc_ptr;	/* Next descriptor pointer		*/
};

struct tx_desc {
	u32 cmd_sts;		/* Command/status field			*/
	u16 l4i_chk;		/* CPU provided TCP checksum		*/
	u16 byte_cnt;		/* buffer byte count			*/
	u32 buf_ptr;		/* pointer to buffer for this descriptor*/
	u32 next_desc_ptr;	/* Pointer to next descriptor		*/
};

/* RX & TX descriptor command */
#define BUFFER_OWNED_BY_DMA		0x80000000

/* RX & TX descriptor status */
#define ERROR_SUMMARY			(1 << 15)

/* RX descriptor status */
#define RX_ENABLE_INTERRUPT		(1 << 23)
#define RX_FIRST_DESC			(1 << 17)
#define RX_LAST_DESC			(1 << 16)

/* TX descriptor command */
#define TX_ENABLE_INTERRUPT		(1 << 23)
#define GEN_CRC				(1 << 22)
#define ZERO_PADDING			(1 << 18)
#define TX_FIRST_DESC			(1 << 17)
#define TX_LAST_DESC			(1 << 16)


/* global *******************************************************************/
static int geth_open(struct net_device *dev);
static int geth_stop(struct net_device *dev);


/* per-port *****************************************************************/
struct rx_queue {
	int index;

	int rx_ring_size;

	int rx_desc_count;
	int rx_curr_desc;
	int rx_used_desc;

	struct rx_desc *rx_desc_area;
	dma_addr_t rx_desc_dma;
	int rx_desc_area_size;
	struct sk_buff **rx_skb;
};

struct tx_queue {
	int index;

	int tx_ring_size;

	int tx_desc_count;
	int tx_curr_desc;
	int tx_used_desc;

	struct tx_desc *tx_desc_area;
	dma_addr_t tx_desc_dma;
	int tx_desc_area_size;

	struct sk_buff_head tx_skb;

	unsigned long tx_packets;
	unsigned long tx_bytes;
	unsigned long tx_dropped;
};

/* Currently just put here and use dummy geth_platform_data, move
 * to .h if we need different configuration for different board
 */
struct geth_platform_data {
	/*
	 * Pointer back to our parent instance, and our port number.
	 */
	int			port_number;

	/*
	 * Whether a PHY is present, and if yes, at which address.
	 */
	int			phy_addr;

	/*
	 * Use this MAC address if it is valid, overriding the
	 * address that is already in the hardware.
	 */
	u8			mac_addr[6];

	/*
	 * If speed is 0, autonegotiation is enabled.
	 *   Valid values for speed: 0, SPEED_10, SPEED_100, SPEED_1000.
	 *   Valid values for duplex: DUPLEX_HALF, DUPLEX_FULL.
	 */
	int			speed;
	int			duplex;

	/*
	 * How many RX/TX queues to use.
	 */
	int			rx_queue_count;
	int			tx_queue_count;

	/*
	 * Override default RX/TX queue sizes if nonzero.
	 */
	int			rx_queue_size;
	int			tx_queue_size;
};

struct geth_private {
	void __iomem *base;
	int port_num;

	struct net_device *dev;

	spinlock_t lock;

	void *hash_tbl;
	dma_addr_t hash_dma;
	struct mii_if_info mii;

	struct phy_device *phy;

	struct work_struct tx_timeout_task;

	struct napi_struct napi;
	u32 int_mask;
	u8 oom;
	u8 work_rx_refill;

	int skb_size;

	/*
	 * RX state.
	 */
	int rx_ring_size;
	int rxq_count;
	struct timer_list rx_oom;
	struct rx_queue rxq[4];

	/*
	 * TX state.
	 */
	int tx_ring_size;
	int txq_count;
	struct tx_queue txq[2];

	/* power saved state */
	u32 saved_config_space[12];
};


/* port register accessors **************************************************/
static inline u32 rdlp(struct geth_private *mp, int offset)
{
	return __raw_readl(mp->base + offset);
}

static inline void wrlp(struct geth_private *mp, int offset, u32 data)
{
	__raw_writel(data, mp->base + offset);
}

/*
 * early parameter for setting up MAC address
 */
static unsigned char galois_mac_addr[6] = {0x00, 0x18, 0x8b, 0x7b, 0x6e, 0x77};
static int __init macaddr_setup(char *bufp)
{
	int i;
	unsigned int m[6];

	if (sscanf(bufp, "%02x:%02x:%02x:%02x:%02x:%02x",
		   &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
		printk("Failed to parse mac address '%s'", bufp);
		return 1;
	}
	for (i = 0; i < 6; i++)
		galois_mac_addr[i] = m[i];

	return 1;
}
__setup("macaddr=", macaddr_setup);

static inline unsigned int cal_mfl(int size)
{
	unsigned int pcxr;

	if(size > 2048)
		pcxr = ETH_MRU_64K;
	else if(size > 1536)
		pcxr = ETH_MRU_2048;
	else if(size > 1518)
		pcxr = ETH_MRU_1536;
	else
		pcxr = ETH_MRU_1518;

	return pcxr;
}

/*
 * Read PHY register(addr) contents via SMI interface.
 */
int mvEthPhyRead(struct geth_private *mp, int phy_id, int addr)
{
	int regData;

	do {
		regData = rdlp(mp, ETH_ESMIR);
	} while (regData & ETH_SMIR_BUSY);

	regData = ETH_SMIR_OP_READ | ETH_SMIR_REGADDR_SET(addr) | ETH_SMIR_PHYADDR_SET(phy_id);
	wrlp(mp, ETH_ESMIR, regData);

	do {
		regData = rdlp(mp, ETH_ESMIR);
	} while (!(regData & ETH_SMIR_READ_VALID));

	return (regData & 0x00ffff);
}

/*
 * Write PHY register(addr) contents to data via SMI interface.
 * NOTE: some PHY chip needs to be reset after writing.
 */
void mvEthPhyWrite(struct geth_private *mp, int phy_id, int addr, int data)
{
	int regData;

	do {
		regData = rdlp(mp, ETH_ESMIR);
	} while (regData & ETH_SMIR_BUSY);

	data &= 0x00ffff;
	regData = ETH_SMIR_REGADDR_SET(addr) | ETH_SMIR_PHYADDR_SET(phy_id) | data;
	wrlp(mp, ETH_ESMIR, regData);
	return;
}

/*
 * Read out @reg of PHY chip
 * NOTE: PHY's address @addr is always PHY_ADDR(0x01)
 */
static int geth_mdio_read(struct net_device *dev, int phy_id, int reg)
{
	struct geth_private *mp = netdev_priv(dev);

	return mvEthPhyRead(mp, phy_id, reg);
}

/*
 * Write @data to @reg of PHY chip
 * NOTE: PHY's address @addr is always PHY_ADDR(0x01)
 */
static void geth_mdio_write(struct net_device *dev, int phy_id, int reg, int data)
{
	struct geth_private *mp = netdev_priv(dev);

	mvEthPhyWrite(mp, phy_id, reg, data);
}

static void phy_reset(struct geth_private *mp)
{
	unsigned int data;

	data = mvEthPhyRead(mp, mp->mii.phy_id, MII_BMCR);
	data |= BMCR_RESET|BMCR_SPEED100|BMCR_ANENABLE|BMCR_FULLDPLX;
	mvEthPhyWrite(mp, mp->mii.phy_id, MII_BMCR, data);
}

#ifdef PHY_NEGOTIATION_WORKAROUND
static inline void ethSetTxClock(int clock_25_Mhz)
{
	unsigned int val;
	void __iomem *reg = IOMEM(0xF7FCD038);

	val = readl(reg);

	if (clock_25_Mhz)
		val |= 4; // select 25 Mhz
	else
		val &=~4; // select 2.5 Mhz

	writel(val, reg);
}
#else
static inline void ethSetTxClock(int clock_25_Mhz) {}
#endif

/*
 * Initialize MII library setting
 */
static void phy_init(struct geth_private *priv)
{
	unsigned int value;
	unsigned int phy_id;
	void __iomem *reg = IOMEM(0xF7FE1404);

	priv->mii.phy_id = PHY_ADDR;
	priv->mii.phy_id_mask = 0x1F;
	priv->mii.reg_num_mask = 0x1F;
	priv->mii.dev = priv->dev;
	priv->mii.mdio_read = geth_mdio_read;
	priv->mii.mdio_write = geth_mdio_write;

	value = __raw_readl(reg);
	value &= ~(0x1);
	__raw_writel(value, reg);

	value = mvEthPhyRead(priv, priv->mii.phy_id, MII_PHYSID2);
	if ((value & 0xfff0) == 0x0e60) {
		/* configure Marvell fast ethernet phy */
		value = mvEthPhyRead(priv, priv->mii.phy_id, 22);
		value &= 0xf0f0;
		value |= (0x050a);
		mvEthPhyWrite(priv, priv->mii.phy_id, 22, value);
		printk(KERN_INFO "Marvell PHY, LED2:link, LED0:link/act.\n");
		value = mvEthPhyRead(priv, priv->mii.phy_id, 24);
		value &= ~(0x7 << 9);
		value |= (0x4 << 9);
		mvEthPhyWrite(priv, priv->mii.phy_id, 24, value);
	} else if ((value & 0xfff0) == 0x0db0) {
		value = mvEthPhyRead(priv, priv->mii.phy_id, 22);
		value &= 0xf000;
		value |= (0x0585);  //LED0: LINK LED1: ACT  LED2: Link
//		value |= (0x05b5);  //LED0: LINK LED1: ACT (blink)  LED2 Link
		mvEthPhyWrite(priv, priv->mii.phy_id, 22, value);
		printk(KERN_INFO "Marvell PHY, LED2:link, LED1 ACT LED0:link.\n");
		value = mvEthPhyRead(priv, priv->mii.phy_id, 24);
		value &= ~(0x7 << 9);
		value |= (0x4 << 9);
		mvEthPhyWrite(priv, priv->mii.phy_id, 24, value);
		// override to active high for LED0 LED1
		value = mvEthPhyRead(priv, priv->mii.phy_id, 25);
		value &= ~(0x3 << 12);
		value |= (0x3 << 12);
		mvEthPhyWrite(priv, priv->mii.phy_id, 25, value);
		/* Disable 1000Base-T advertisement */
		value = mvEthPhyRead(priv, priv->mii.phy_id, MII_CTRL1000);
		value &= ((~ADVERTISE_1000HALF) & (~ADVERTISE_1000FULL));
		mvEthPhyWrite(priv, priv->mii.phy_id, MII_CTRL1000, value);
	} else {
		/* Disable 1000Base-T advertisement */
		value = mvEthPhyRead(priv, priv->mii.phy_id, MII_CTRL1000);
		value &= ((~ADVERTISE_1000HALF) & (~ADVERTISE_1000FULL));
		mvEthPhyWrite(priv, priv->mii.phy_id, MII_CTRL1000, value);
	}

	value = mvEthPhyRead(priv, priv->mii.phy_id, MII_ADVERTISE);
	value &= ~(ADVERTISE_PAUSE_CAP | ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF);
#ifdef CONFIG_BERLIN_ASIC
	/* Enable 100Base-TX, 10Base-TX advertisement */
	value |= (ADVERTISE_PAUSE_CAP | ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF);
#else
	/* Enable 10Base-TX advertisement only */
	value |= (ADVERTISE_10FULL | ADVERTISE_10HALF);
#endif
	mvEthPhyWrite(priv, priv->mii.phy_id, MII_ADVERTISE, value);

#ifdef PHY_NEGOTIATION_WORKAROUND
	//enable crossover & scrambler
	ethSetTxClock(1);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x10, 0x138);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1D, 4);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1E, 0x39C);//Set ADC bias current
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1D, 5);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1E, 0x2100);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1D, 9);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1E, 0x2081);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1D, 8);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1E, 0xE42D);
#ifdef CONFIG_BERLIN2CD
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1D, 16);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1E, 0x2025);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1C, 0xc83);
#if 0
    //use default setting, this is for low core voltage.
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1D, 9);
	mvEthPhyWrite(priv, priv->mii.phy_id, 0x1F, 0x17C6); //for low voltage only
#endif
#endif
	return;
#endif
	printk(KERN_INFO "Setting ethernet to auto-negotiation mode.\n");

	/* Sw-reset PHY to restart AN */
	phy_id = mvEthPhyRead(priv, priv->mii.phy_id, MII_PHYSID1);
	if (phy_id == MARVELL_PHY_ID1) {
		value = BMCR_RESET | BMCR_ANENABLE;	/* SW-Reset PHY */
		mvEthPhyWrite(priv, priv->mii.phy_id, MII_BMCR, value);
	} else {
		value = BMCR_ANRESTART | BMCR_ANENABLE;	/* restart AN */
		mvEthPhyWrite(priv, priv->mii.phy_id, MII_BMCR, value);
	}
}

/*
 * Hash function macroes
 */
#define NIBBLE_SWAPPING_16_BIT(X)   \
        (((X&0xf) << 4) |     \
         ((X&0xf0) >> 4) |    \
         ((X&0xf00) << 4) |   \
         ((X&0xf000) >> 4))

#define NIBBLE_SWAPPING_32_BIT(X)   \
        (((X&0xf) << 4) |       \
         ((X&0xf0) >> 4) |      \
         ((X&0xf00) << 4) |     \
         ((X&0xf000) >> 4) |    \
         ((X&0xf0000) << 4) |   \
         ((X&0xf00000) >> 4) |  \
         ((X&0xf000000) << 4) | \
         ((X&0xf0000000) >> 4))

#define GT_NIBBLE(X)          \
        (((X&0x1) << 3 ) +    \
         ((X&0x2) << 1 ) +    \
         ((X&0x4) >> 1 ) +    \
         ((X&0x8) >> 3 ) )

/*
 * mvHashTableFunction - Hash calculation function
 */
static unsigned int mvHashTableFunction(unsigned int macH, unsigned int macL)
{
	unsigned int hashResult;
	unsigned int ethernetAddH;
	unsigned int ethernetAddL;
	unsigned int ethernetAdd0;
	unsigned int ethernetAdd1;
	unsigned int ethernetAdd2;
	unsigned int ethernetAdd3;
	unsigned int ethernetAddHSwapped = 0;
	unsigned int ethernetAddLSwapped = 0;

	ethernetAddH = NIBBLE_SWAPPING_16_BIT(macH);
	ethernetAddL = NIBBLE_SWAPPING_32_BIT(macL);

	ethernetAddHSwapped = GT_NIBBLE(ethernetAddH&0xf)+
		((GT_NIBBLE((ethernetAddH>>4)&0xf))<<4)+
		((GT_NIBBLE((ethernetAddH>>8)&0xf))<<8)+
		((GT_NIBBLE((ethernetAddH>>12)&0xf))<<12);

	ethernetAddLSwapped = GT_NIBBLE(ethernetAddL&0xf)+
		((GT_NIBBLE((ethernetAddL>>4)&0xf))<<4)+
		((GT_NIBBLE((ethernetAddL>>8)&0xf))<<8)+
		((GT_NIBBLE((ethernetAddL>>12)&0xf))<<12)+
		((GT_NIBBLE((ethernetAddL>>16)&0xf))<<16)+
		((GT_NIBBLE((ethernetAddL>>20)&0xf))<<20)+
		((GT_NIBBLE((ethernetAddL>>24)&0xf))<<24)+
		((GT_NIBBLE((ethernetAddL>>28)&0xf))<<28);

	ethernetAddH = ethernetAddHSwapped;
	ethernetAddL = ethernetAddLSwapped;

	/* hash mode 0 */
	ethernetAdd0 = (ethernetAddL>>2) & 0x3f;
	ethernetAdd1 = (ethernetAddL & 0x3) | ((ethernetAddL>>8) & 0x7f)<<2;
	ethernetAdd2 = (ethernetAddL>>15) & 0x1ff;
	ethernetAdd3 = ((ethernetAddL>>24) & 0xff) | ((ethernetAddH & 0x1)<<8);

	hashResult = (ethernetAdd0<<9) | (ethernetAdd1^ethernetAdd2^ethernetAdd3);

	hashResult = hashResult & 0x7ff; /* half-k */

	return hashResult;
}

/*
 * Calculate the hash value of a MAC address and then add into hash table
 */
static void mvAddAddressTableEntry(void *ptr, unsigned char *addr,
			    int rd, int skip)
{
	unsigned int addressHighValue, addressLowValue, addressLowRead, addressHighRead;
	unsigned int macH, macL, *entry;
	int i;

	macH = (addr[0] << 8) | (addr[1]);
	macL = (addr[2] << 24) | (addr[3] << 16) | (addr[4] << 8) | (addr[5]);
	entry = (unsigned int *)(ptr + (8 * mvHashTableFunction(macH, macL)));

	addressLowValue = ETH_HASH_TABLE_ENTRY_VALID | (rd<<2) |
		(((macH>>8)&0xf)<<3) | (((macH>>12)&0xf)<<7) |
		(((macH>>0)&0xf)<<11) | (((macH>>4)&0xf)<<15) |
		(((macL>>24)&0xf)<<19) | (((macL>>28)&0xf)<<23) |
		(((macL>>16)&0xf)<<27) | ((((macL>>20)&0x1)<<31));

	addressHighValue = ((macL>>21)&0x7) | (((macL>>8)&0xf)<<3) |
		(((macL>>12)&0xf)<<7) |	(((macL>>0)&0xf)<<11) | (((macL>>4)&0xf)<<15);

	if (skip)
		addressLowValue |= ETH_HASH_TABLE_ENTRY_SKIP;

	/* find a free place */
	for (i = 0 ; i < 12 ; i++) {
		addressLowRead = *(entry + (i*2));
		if (!(addressLowRead & ETH_HASH_TABLE_ENTRY_VALID) ||
		    (addressLowRead & ETH_HASH_TABLE_ENTRY_SKIP)) {
			entry = entry + (i*2);
			break;
		} else {
			/* if the address is the same locate it at the same position */
			addressHighRead = *(entry + (i*2) + 1);
			if (((addressLowRead>>3)&0x1fffffff) == ((addressLowValue>>3)&0x1fffffff) && (addressHighRead==addressHighValue)) {
				entry = entry + (i*2);
				break;
			}
		}

	}

	if (i == 12) {
		printk("error fill hash table\n");
		return; /* TO DO DEFRAGMENT */
	}

	/* update address entry */
	*entry = addressLowValue;
	*(entry + 1) = addressHighValue;
}

static void init_hash_table(struct geth_private *mp)
{
	u32 epcr;

	epcr = rdlp(mp, ETH_EPCR);
	epcr |= ETH_HASH_SIZE_HALF_K;
	epcr &= ~ETH_HASH_FUNCTION_1;
	/* reset HDM to 0: discard addresses not found in hash table */
	epcr &= ~ETH_HASH_PASS_MODE;
	wrlp(mp, ETH_EPCR, epcr);
	mp->hash_tbl = dma_alloc_coherent(mp->dev->dev.parent, 0x200*4*8,
					  &mp->hash_dma, GFP_KERNEL);
	wrlp(mp, ETH_EHTPR, mp->hash_dma);

}

/* rxq/txq helper functions *************************************************/
static struct geth_private *rxq_to_mp(struct rx_queue *rxq)
{
	return container_of(rxq, struct geth_private, rxq[rxq->index]);
}

static struct geth_private *txq_to_mp(struct tx_queue *txq)
{
	return container_of(txq, struct geth_private, txq[txq->index]);
}

static void rxq_enable(struct rx_queue *rxq)
{
	struct geth_private *mp = rxq_to_mp(rxq);

	wrlp(mp, ETH_ESDCMR, ETH_RX_ENABLE);
}

static void rxq_disable(struct rx_queue *rxq)
{
	struct geth_private *mp = rxq_to_mp(rxq);

	wrlp(mp, ETH_ESDCMR, ETH_RX_ABORT);
	while (rdlp(mp, ETH_ESDCMR) & ETH_RX_ABORT)
		udelay(10);
}

static void txq_reset_hw_ptr(struct tx_queue *txq)
{
	struct geth_private *mp = txq_to_mp(txq);
	u32 addr;

	addr = (u32)txq->tx_desc_dma;
	addr += txq->tx_curr_desc * sizeof(struct tx_desc);
	wrlp(mp, TXQ_CURRENT_DESC_PTR(txq->index), addr);
}

static void txq_enable(struct tx_queue *txq)
{
	struct geth_private *mp = txq_to_mp(txq);

	wrlp(mp, ETH_ESDCMR, (ETH_TX_START_0 << txq->index));
}

static void txq_disable(struct tx_queue *txq)
{
	struct geth_private *mp = txq_to_mp(txq);

	wrlp(mp, ETH_ESDCMR, (ETH_TX_STOP_0 << txq->index));
}

static void txq_maybe_wake(struct tx_queue *txq)
{
	struct geth_private *mp = txq_to_mp(txq);
	struct netdev_queue *nq = netdev_get_tx_queue(mp->dev, txq->index);

	if (netif_tx_queue_stopped(nq)) {
		if (txq->tx_ring_size - txq->tx_desc_count >= MAX_SKB_FRAGS + 1)
			netif_tx_wake_queue(nq);
	}
}


/* rx napi ******************************************************************/
static int rxq_process(struct rx_queue *rxq, int budget)
{
	struct geth_private *mp = rxq_to_mp(rxq);
	struct net_device_stats *stats = &mp->dev->stats;
	int rx;

	rx = 0;
	while (rx < budget && rxq->rx_desc_count) {
		struct rx_desc *rx_desc;
		unsigned int cmd_sts;
		struct sk_buff *skb;
		u16 byte_cnt;

		rx_desc = &rxq->rx_desc_area[rxq->rx_curr_desc];

		cmd_sts = rx_desc->cmd_sts;
		if (cmd_sts & BUFFER_OWNED_BY_DMA)
			break;
		rmb();

		skb = rxq->rx_skb[rxq->rx_curr_desc];
		rxq->rx_skb[rxq->rx_curr_desc] = NULL;

		rxq->rx_curr_desc++;
		if (rxq->rx_curr_desc == rxq->rx_ring_size)
			rxq->rx_curr_desc = 0;

		dma_unmap_single(mp->dev->dev.parent, rx_desc->buf_ptr,
				 rx_desc->buf_size, DMA_FROM_DEVICE);
		rxq->rx_desc_count--;
		rx++;

		mp->work_rx_refill |= 1 << rxq->index;

		byte_cnt = rx_desc->byte_cnt;

		/*
		 * Update statistics.
		 *
		 * Note that the descriptor byte count includes 2 dummy
		 * bytes automatically inserted by the hardware at the
		 * start of the packet (which we don't count), and a 4
		 * byte CRC at the end of the packet (which we do count).
		 */
		stats->rx_packets++;
		stats->rx_bytes += byte_cnt - 2;

		/*
		 * In case we received a packet without first / last bits
		 * on, or the error summary bit is set, the packet needs
		 * to be dropped.
		 */
		if ((cmd_sts & (RX_FIRST_DESC | RX_LAST_DESC | ERROR_SUMMARY))
			!= (RX_FIRST_DESC | RX_LAST_DESC))
			goto err;

		/*
		 * The -4 is for the CRC in the trailer of the
		 * received packet
		 */
		skb_put(skb, byte_cnt - 2 - 4);

		skb->protocol = eth_type_trans(skb, mp->dev);

		netif_receive_skb(skb);

		continue;

err:
		stats->rx_dropped++;

		if ((cmd_sts & (RX_FIRST_DESC | RX_LAST_DESC)) !=
			(RX_FIRST_DESC | RX_LAST_DESC)) {
			if (net_ratelimit())
				dev_printk(KERN_ERR, &mp->dev->dev,
					   "received packet spanning "
					   "multiple descriptors\n");
		}

		if (cmd_sts & ERROR_SUMMARY)
			stats->rx_errors++;

		dev_kfree_skb(skb);
	}

	return rx;
}

static int rxq_refill(struct rx_queue *rxq, int budget)
{
	struct geth_private *mp = rxq_to_mp(rxq);
	int refilled;

	refilled = 0;
	while (refilled < budget && rxq->rx_desc_count < rxq->rx_ring_size) {
		struct sk_buff *skb;
		int rx;
		struct rx_desc *rx_desc;
		int size;

		skb = netdev_alloc_skb(mp->dev, mp->skb_size);

		if (skb == NULL) {
			mp->oom = 1;
			goto oom;
		}

		if (SKB_DMA_REALIGN)
			skb_reserve(skb, SKB_DMA_REALIGN);

		refilled++;
		rxq->rx_desc_count++;

		rx = rxq->rx_used_desc++;
		if (rxq->rx_used_desc == rxq->rx_ring_size)
			rxq->rx_used_desc = 0;

		rx_desc = rxq->rx_desc_area + rx;

		size = skb->end - skb->data;
		rx_desc->buf_ptr = dma_map_single(mp->dev->dev.parent,
						  skb->data, size,
						  DMA_FROM_DEVICE);
		rx_desc->buf_size = size;
		rxq->rx_skb[rx] = skb;
		wmb();
		rx_desc->cmd_sts = BUFFER_OWNED_BY_DMA | RX_ENABLE_INTERRUPT;
		wmb();

		/*
		 * The hardware automatically prepends 2 bytes of
		 * dummy data to each received packet, so that the
		 * IP header ends up 16-byte aligned.
		 */
		skb_reserve(skb, 2);
	}

	if (refilled < budget)
		mp->work_rx_refill &= ~(1 << rxq->index);

oom:
	return refilled;
}


/* tx ***********************************************************************/
static inline unsigned int has_tiny_unaligned_frags(struct sk_buff *skb)
{
	int frag;

	for (frag = 0; frag < skb_shinfo(skb)->nr_frags; frag++) {
		const skb_frag_t *fragp = &skb_shinfo(skb)->frags[frag];

		if (skb_frag_size(fragp) <= 8 && fragp->page_offset & 7)
			return 1;
	}

	return 0;
}

static void txq_submit_frag_skb(struct tx_queue *txq, struct sk_buff *skb)
{
	struct geth_private *mp = txq_to_mp(txq);
	int nr_frags = skb_shinfo(skb)->nr_frags;
	int frag;

	for (frag = 0; frag < nr_frags; frag++) {
		skb_frag_t *this_frag;
		int tx_index;
		struct tx_desc *desc;

		this_frag = &skb_shinfo(skb)->frags[frag];
		tx_index = txq->tx_curr_desc++;
		if (txq->tx_curr_desc == txq->tx_ring_size)
			txq->tx_curr_desc = 0;
		desc = &txq->tx_desc_area[tx_index];

		/*
		 * The last fragment will generate an interrupt
		 * which will free the skb on TX completion.
		 */
		if (frag == nr_frags - 1) {
			desc->cmd_sts = BUFFER_OWNED_BY_DMA | GEN_CRC |
					ZERO_PADDING | TX_LAST_DESC |
					TX_ENABLE_INTERRUPT;
		} else {
			desc->cmd_sts = BUFFER_OWNED_BY_DMA | GEN_CRC;
		}

		desc->l4i_chk = 0;
		desc->byte_cnt = skb_frag_size(this_frag);
		desc->buf_ptr = skb_frag_dma_map(mp->dev->dev.parent,
						 this_frag, 0,
						 skb_frag_size(this_frag),
						 DMA_TO_DEVICE);
	}
}

static int txq_submit_skb(struct tx_queue *txq, struct sk_buff *skb)
{
	struct geth_private *mp = txq_to_mp(txq);
	int nr_frags = skb_shinfo(skb)->nr_frags;
	int tx_index;
	struct tx_desc *desc;
	u32 cmd_sts;
	int length;

	cmd_sts = TX_FIRST_DESC | GEN_CRC | BUFFER_OWNED_BY_DMA;

	tx_index = txq->tx_curr_desc++;
	if (txq->tx_curr_desc == txq->tx_ring_size)
		txq->tx_curr_desc = 0;
	desc = &txq->tx_desc_area[tx_index];

	if (nr_frags) {
		txq_submit_frag_skb(txq, skb);
		length = skb_headlen(skb);
	} else {
		cmd_sts |= ZERO_PADDING | TX_LAST_DESC | TX_ENABLE_INTERRUPT;
		length = skb->len;
	}

	desc->l4i_chk = 0;
	desc->byte_cnt = length;
	desc->buf_ptr = dma_map_single(mp->dev->dev.parent, skb->data,
				       length, DMA_TO_DEVICE);

	__skb_queue_tail(&txq->tx_skb, skb);

	/* ensure all other descriptors are written before first cmd_sts */
	wmb();
	desc->cmd_sts = cmd_sts;

	/* ensure all descriptors are written before poking hardware */
	wmb();
	txq_enable(txq);

	txq->tx_desc_count += nr_frags + 1;

	return 0;
}

static netdev_tx_t geth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);
	int queue;
	struct tx_queue *txq;
	struct netdev_queue *nq;
	unsigned long flags;

	queue = skb_get_queue_mapping(skb);
	txq = mp->txq + queue;
	nq = netdev_get_tx_queue(dev, queue);

	if (has_tiny_unaligned_frags(skb) && __skb_linearize(skb)) {
		txq->tx_dropped++;
		dev_printk(KERN_DEBUG, &dev->dev,
			   "failed to linearize skb with tiny "
			   "unaligned fragment\n");
		return NETDEV_TX_BUSY;
	}

	spin_lock_irqsave(&mp->lock, flags);
	if (txq->tx_ring_size - txq->tx_desc_count < MAX_SKB_FRAGS + 1) {
		spin_unlock_irqrestore(&mp->lock, flags);
		if (net_ratelimit())
			dev_printk(KERN_ERR, &dev->dev, "tx queue full?!\n");
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	if (!txq_submit_skb(txq, skb)) {
		int entries_left;

		txq->tx_bytes += skb->len;
		txq->tx_packets++;

		entries_left = txq->tx_ring_size - txq->tx_desc_count;
		if (entries_left < MAX_SKB_FRAGS + 1)
			netif_tx_stop_queue(nq);
	}

	spin_unlock_irqrestore(&mp->lock, flags);

	return NETDEV_TX_OK;
}

static void txq_kick(struct tx_queue *txq)
{
	struct geth_private *mp = txq_to_mp(txq);
	u32 hw_desc_ptr;
	u32 expected_ptr;

	spin_lock(&mp->lock);

	if (rdlp(mp, ETH_ESDCMR) & (ETH_TX_START_0 << txq->index));
		goto out;

	hw_desc_ptr = rdlp(mp, TXQ_CURRENT_DESC_PTR(txq->index));
	expected_ptr = (u32)txq->tx_desc_dma +
				txq->tx_curr_desc * sizeof(struct tx_desc);

	if (hw_desc_ptr != expected_ptr)
		txq_enable(txq);

out:
	spin_unlock(&mp->lock);
}

static int txq_reclaim(struct tx_queue *txq, int budget, int force)
{
	struct geth_private *mp = txq_to_mp(txq);
	int reclaimed;

	reclaimed = 0;
	while (reclaimed < budget && txq->tx_desc_count > 0) {
		int tx_index;
		struct tx_desc *desc;
		u32 cmd_sts;
		struct sk_buff *skb;

		tx_index = txq->tx_used_desc;
		desc = &txq->tx_desc_area[tx_index];
		cmd_sts = desc->cmd_sts;

		if (cmd_sts & BUFFER_OWNED_BY_DMA) {
			if (!force)
				break;
			desc->cmd_sts = cmd_sts & ~BUFFER_OWNED_BY_DMA;
		}

		txq->tx_used_desc = tx_index + 1;
		if (txq->tx_used_desc == txq->tx_ring_size)
			txq->tx_used_desc = 0;

		reclaimed++;
		txq->tx_desc_count--;

		skb = NULL;
		if (cmd_sts & TX_LAST_DESC)
			skb = __skb_dequeue(&txq->tx_skb);

		if (cmd_sts & ERROR_SUMMARY) {
			dev_printk(KERN_INFO, &mp->dev->dev, "tx error\n");
			mp->dev->stats.tx_errors++;
		}


		if (cmd_sts & TX_FIRST_DESC) {
			dma_unmap_single(mp->dev->dev.parent, desc->buf_ptr,
					 desc->byte_cnt, DMA_TO_DEVICE);
		} else {
			dma_unmap_page(mp->dev->dev.parent, desc->buf_ptr,
				       desc->byte_cnt, DMA_TO_DEVICE);
		}

		dev_kfree_skb_irq(skb);
	}

	return reclaimed;
}

/* statistics ***************************************************************/
static struct net_device_stats *geth_get_stats(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	unsigned long tx_packets = 0;
	unsigned long tx_bytes = 0;
	unsigned long tx_dropped = 0;
	int i;

	for (i = 0; i < mp->txq_count; i++) {
		struct tx_queue *txq = mp->txq + i;

		tx_packets += txq->tx_packets;
		tx_bytes += txq->tx_bytes;
		tx_dropped += txq->tx_dropped;
	}

	stats->tx_packets = tx_packets;
	stats->tx_bytes = tx_bytes;
	stats->tx_dropped = tx_dropped;

	return stats;
}

static inline u32 mib_read(struct geth_private *mp, int offset)
{
	return rdlp(mp, MIB_COUNTERS(mp->port_num) + offset);
}

static void mib_counters_clear(struct geth_private *mp)
{
	int i;
	u32 data;

	data = rdlp(mp, ETH_EPCXR);
	data &= ~ETH_MIB_CLEAR; /* 0 to read-clear */
	wrlp(mp, ETH_EPCXR, data);
	for (i = 0; i < 0x60; i += 4)
		mib_read(mp, i);
	data |= ETH_MIB_CLEAR; /* 1 to read-no-effect */
	wrlp(mp, ETH_EPCXR, data);
}

/* address handling *********************************************************/
static void uc_addr_set(struct geth_private *mp, unsigned char *addr)
{
	wrlp(mp, ETH_EFCSAH,
		(addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3]);
	wrlp(mp, ETH_EFCSAL, (addr[4] << 8) | addr[5]);
}

static void geth_program_unicast_filter(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);

	uc_addr_set(mp, dev->dev_addr);

	/* add unicast address to filter table, and accept frames of this address */
	mvAddAddressTableEntry(mp->hash_tbl, dev->dev_addr, 1/*receive*/, 0/*not skip*/);
}

static void geth_set_multicast_list(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);
	u32 epcr;

	epcr = rdlp(mp, ETH_EPCR);
	if (dev->flags & (IFF_PROMISC | IFF_ALLMULTI)) {
		epcr |= ETH_PROMISCUOUS_MODE;
		wrlp(mp, ETH_EPCR, epcr);
	} else {
		struct netdev_hw_addr *ha;

		epcr &= ~(ETH_PROMISCUOUS_MODE | ETH_BROADCAST_REJECT_MODE);
		wrlp(mp, ETH_EPCR, epcr);
		netdev_for_each_mc_addr(ha, dev) {
			mvAddAddressTableEntry(mp->hash_tbl, ha->addr, 1, 0);
		}
	}
	/*printk("EPCR %x\n", rdlp(mp, ETH_EPCR));
	printk("EPCXR %x\n", rdlp(mp, ETH_EPCXR));
	printk("EPSR %x\n", rdlp(mp, ETH_EPSR));
	printk("EFCSAL %x\n", rdlp(mp, ETH_EFCSAL));
	printk("EFCSAH %x\n", rdlp(mp, ETH_EFCSAH));
	printk("ESDCR %x\n", rdlp(mp, ETH_ESDCR));
	printk("ESDCMR %x\n", rdlp(mp, ETH_ESDCMR));
	printk("EIMR %x\n", rdlp(mp, ETH_EIMR));*/
}

static int geth_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = addr;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EINVAL;

	memcpy(dev->dev_addr, sa->sa_data, ETH_ALEN);

	netif_addr_lock_bh(dev);
	geth_program_unicast_filter(dev);
	netif_addr_unlock_bh(dev);

	return 0;
}

/* rx/tx queue initialisation ***********************************************/
static int rxq_init(struct geth_private *mp, int index)
{
	struct rx_queue *rxq = mp->rxq + index;
	struct rx_desc *rx_desc;
	int size;
	int i;

	rxq->index = index;

	rxq->rx_ring_size = mp->rx_ring_size;

	rxq->rx_desc_count = 0;
	rxq->rx_curr_desc = 0;
	rxq->rx_used_desc = 0;

	size = rxq->rx_ring_size * sizeof(struct rx_desc);

	rxq->rx_desc_area = dma_alloc_coherent(mp->dev->dev.parent,
					       size, &rxq->rx_desc_dma,
					       GFP_KERNEL);

	if (rxq->rx_desc_area == NULL) {
		dev_printk(KERN_ERR, &mp->dev->dev,
			   "can't allocate rx ring (%d bytes)\n", size);
		goto out;
	}
	memset(rxq->rx_desc_area, 0, size);

	rxq->rx_desc_area_size = size;
	rxq->rx_skb = kmalloc(rxq->rx_ring_size * sizeof(*rxq->rx_skb),
								GFP_KERNEL);
	if (rxq->rx_skb == NULL) {
		dev_printk(KERN_ERR, &mp->dev->dev,
			   "can't allocate rx skb ring\n");
		goto out_free;
	}

	rx_desc = (struct rx_desc *)rxq->rx_desc_area;
	for (i = 0; i < rxq->rx_ring_size; i++) {
		int nexti;

		nexti = i + 1;
		if (nexti == rxq->rx_ring_size)
			nexti = 0;

		rx_desc[i].next_desc_ptr = rxq->rx_desc_dma +
					nexti * sizeof(struct rx_desc);
	}

	return 0;

out_free:
	dma_free_coherent(mp->dev->dev.parent, size,
			  rxq->rx_desc_area,
			  rxq->rx_desc_dma);

out:
	return -ENOMEM;
}

static void rxq_deinit(struct rx_queue *rxq)
{
	struct geth_private *mp = rxq_to_mp(rxq);
	int i;

	rxq_disable(rxq);

	for (i = 0; i < rxq->rx_ring_size; i++) {
		if (rxq->rx_skb[i]) {
			dev_kfree_skb(rxq->rx_skb[i]);
			rxq->rx_desc_count--;
		}
	}

	if (rxq->rx_desc_count) {
		dev_printk(KERN_ERR, &mp->dev->dev,
			   "error freeing rx ring -- %d skbs stuck\n",
			   rxq->rx_desc_count);
	}

	dma_free_coherent(mp->dev->dev.parent, rxq->rx_desc_area_size,
			  rxq->rx_desc_area, rxq->rx_desc_dma);

	kfree(rxq->rx_skb);
}

static int txq_init(struct geth_private *mp, int index)
{
	struct tx_queue *txq = mp->txq + index;
	struct tx_desc *tx_desc;
	int size;
	int i;

	txq->index = index;

	txq->tx_ring_size = mp->tx_ring_size;

	txq->tx_desc_count = 0;
	txq->tx_curr_desc = 0;
	txq->tx_used_desc = 0;

	size = txq->tx_ring_size * sizeof(struct tx_desc);

	txq->tx_desc_area = dma_alloc_coherent(mp->dev->dev.parent,
					       size, &txq->tx_desc_dma,
					       GFP_KERNEL);

	if (txq->tx_desc_area == NULL) {
		dev_printk(KERN_ERR, &mp->dev->dev,
			   "can't allocate tx ring (%d bytes)\n", size);
		return -ENOMEM;
	}
	memset(txq->tx_desc_area, 0, size);

	txq->tx_desc_area_size = size;

	tx_desc = (struct tx_desc *)txq->tx_desc_area;
	for (i = 0; i < txq->tx_ring_size; i++) {
		struct tx_desc *txd = tx_desc + i;
		int nexti;

		nexti = i + 1;
		if (nexti == txq->tx_ring_size)
			nexti = 0;

		txd->cmd_sts = 0;
		txd->next_desc_ptr = txq->tx_desc_dma +
					nexti * sizeof(struct tx_desc);
	}

	skb_queue_head_init(&txq->tx_skb);

	return 0;
}

static void txq_deinit(struct tx_queue *txq)
{
	struct geth_private *mp = txq_to_mp(txq);
	unsigned long flags;

	txq_disable(txq);
	spin_lock_irqsave(&mp->lock, flags);
	txq_reclaim(txq, txq->tx_ring_size, 1);
	spin_unlock_irqrestore(&mp->lock, flags);

	BUG_ON(txq->tx_used_desc != txq->tx_curr_desc);

	dma_free_coherent(mp->dev->dev.parent, txq->tx_desc_area_size,
			  txq->tx_desc_area, txq->tx_desc_dma);
}


/* netdev ops and related ***************************************************/
static void handle_link_event(struct geth_private *mp)
{
	struct net_device *dev = mp->dev;
	u32 port_status;
	int speed;
	int duplex;
	int fc;

	port_status = rdlp(mp, ETH_EPSR);
	if (!(port_status & LINK_UP)) {
		if (netif_carrier_ok(dev)) {
			int i;

			printk(KERN_INFO "%s: link down\n", dev->name);

			netif_carrier_off(dev);

			for (i = 0; i < mp->txq_count; i++) {
				struct tx_queue *txq = mp->txq + i;

				spin_lock(&mp->lock);
				txq_reclaim(txq, txq->tx_ring_size, 1);
				spin_unlock(&mp->lock);
				txq_reset_hw_ptr(txq);
			}
		}
		return;
	}

	switch (port_status & PORT_SPEED_MASK) {
	case PORT_SPEED_10:
		speed = 10;
		ethSetTxClock(0);
		break;
	case PORT_SPEED_100:
		speed = 100;
		ethSetTxClock(1);
		break;
	default:
		speed = -1;
		break;
	}
	duplex = (port_status & FULL_DUPLEX) ? 1 : 0;
	fc = (port_status & FLOW_CONTROL_ENABLED) ? 1 : 0;

	printk(KERN_INFO "%s: link up, %d Mb/s, %s duplex, "
			 "flow control %sabled\n", dev->name,
			 speed, duplex ? "full" : "half",
			 fc ? "en" : "dis");

	if (!netif_carrier_ok(dev))
		netif_carrier_on(dev);
}

static irqreturn_t geth_irq(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct geth_private *mp = netdev_priv(dev);
	u32 int_cause, txstatus;
	int i;

	int_cause = rdlp(mp, ETH_EICR) & mp->int_mask;
	if (int_cause == 0)
		return IRQ_NONE;
	wrlp(mp, ETH_EICR, ~int_cause);

	if (int_cause & INT_RX) {
		wrlp(mp, ETH_EIMR, mp->int_mask & ~INT_RX);
		napi_schedule(&mp->napi);
	}

	if (int_cause & INT_MIIPSTC)
		handle_link_event(mp);

	txstatus = int_cause & INT_TX;
	for (i = 0; i < mp->txq_count; ++i) {
		if (txstatus & INT_TX_0 << i) {
			spin_lock(&mp->lock);
			txq_reclaim(mp->txq + i, 16, 0);
			txq_maybe_wake(mp->txq + i);
			spin_unlock(&mp->lock);
		}
	}

	txstatus = ((int_cause & INT_TX_END) >> 6) &
			~((rdlp(mp, ETH_ESDCMR) >> 16) & 0x3);
	for (i = 0; i < mp->txq_count; ++i) {
		if (txstatus & 1 << i)
			txq_kick(mp->txq + i);
	}

	return IRQ_HANDLED;
}

static int geth_poll(struct napi_struct *napi, int budget)
{
	struct geth_private *mp;
	int i, work_done;

	mp = container_of(napi, struct geth_private, napi);

	if (unlikely(mp->oom)) {
		mp->oom = 0;
		del_timer(&mp->rx_oom);
	}

	work_done = 0;
	for (i = mp->rxq_count - 1; work_done < budget && i >= 0; i--) {
		struct rx_queue *rxq = mp->rxq + i;

		int work_tbd = budget - work_done;
		if (work_tbd > 16)
			work_tbd = 16;
		work_done += rxq_process(rxq, work_tbd);
		wrlp(mp, ETH_EICR, ~(INT_RX_0 << i));
		if (likely(!mp->oom))
			if (mp->work_rx_refill & 1 << i)
				work_done += rxq_refill(rxq, work_tbd);
	}

	if (work_done < budget) {
		if (mp->oom)
			mod_timer(&mp->rx_oom, jiffies + (HZ / 10));
		napi_complete(napi);
		wrlp(mp, ETH_EIMR, mp->int_mask);
	}

	return work_done;
}

static inline void oom_timer_wrapper(unsigned long data)
{
	struct geth_private *mp = (void *)data;

	napi_schedule(&mp->napi);
}

static void port_start(struct geth_private *mp)
{
	u32 epcr, epcxr;
	int i;

	/*
	 * Configure basic link parameters.
	 */
	phy_reset(mp);
	epcxr = rdlp(mp, ETH_EPCXR);
	epcxr &= ~ETH_MRU_ALL_MASK;
	epcxr |= cal_mfl(mp->skb_size);
	wrlp(mp, ETH_EPCXR, epcxr);

	epcr = rdlp(mp, ETH_EPCR);
	epcr |= ETH_PORT_ENABLE;
	wrlp(mp, ETH_EPCR, epcr);

	/*
	 * Configure TX path and queues.
	 */
	for (i = 0; i < mp->txq_count; i++) {
		struct tx_queue *txq = mp->txq + i;

		txq_reset_hw_ptr(txq);
	}

	/*
	 * Enable the receive queues.
	 */
	for (i = 0; i < mp->rxq_count; i++) {
		struct rx_queue *rxq = mp->rxq + i;
		u32 addr;

		addr = (u32)rxq->rx_desc_dma;
		addr += rxq->rx_curr_desc * sizeof(struct rx_desc);
		wrlp(mp, RXQ_CURRENT_DESC_PTR(i), addr);
		wrlp(mp, RXQ_FIRST_DESC_PTR(i), addr);

		rxq_enable(rxq);
	}
}

static void geth_recalc_skb_size(struct geth_private *mp)
{
	int skb_size;

	/*
	 * Reserve 2+14 bytes for an ethernet header (the hardware
	 * automatically prepends 2 bytes of dummy data to each
	 * received packet), and 4 bytes for the trailing FCS -- 20 bytes total.
	 */
	skb_size = mp->dev->mtu + 20;

	/*
	 * Make sure that the skb size is a multiple of 8 bytes, as
	 * the lower three bits of the receive descriptor's buffer
	 * size field are ignored by the hardware.
	 */
	mp->skb_size = (skb_size + 7) & ~7;

	/*
	 * If NET_SKB_PAD is smaller than a cache line,
	 * netdev_alloc_skb() will cause skb->data to be misaligned
	 * to a cache line boundary.  If this is the case, include
	 * some extra space to allow re-aligning the data area.
	 */
	mp->skb_size += SKB_DMA_REALIGN;
}

static int geth_open(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);
	int err;
	int i;

	/* Mask all interrupts */
	wrlp(mp, ETH_EIMR, 0);
	/* Clear Cause registers */
	wrlp(mp, ETH_EICR, 0);

	err = request_irq(dev->irq, geth_irq, 0, dev->name, dev);
	if (err) {
		dev_printk(KERN_ERR, &dev->dev, "can't assign irq\n");
		return -EAGAIN;
	}

	geth_recalc_skb_size(mp);

	napi_enable(&mp->napi);

	mp->int_mask = INT_MIIPSTC;

	for (i = 0; i < mp->rxq_count; i++) {
		err = rxq_init(mp, i);
		if (err) {
			while (--i >= 0)
				rxq_deinit(mp->rxq + i);
			goto out;
		}

		rxq_refill(mp->rxq + i, INT_MAX);
		mp->int_mask |= INT_RX_0 << i;
	}

	if (mp->oom) {
		mp->rx_oom.expires = jiffies + (HZ / 10);
		add_timer(&mp->rx_oom);
	}

	for (i = 0; i < mp->txq_count; i++) {
		err = txq_init(mp, i);
		if (err) {
			while (--i >= 0)
				txq_deinit(mp->txq + i);
			goto out_free;
		}
		mp->int_mask |= INT_TX_0 << i;
		mp->int_mask |= INT_TX_END_0 << i;
	}

	port_start(mp);

	wrlp(mp, ETH_EIMR, mp->int_mask);

	return 0;

out_free:
	for (i = 0; i < mp->rxq_count; i++)
		rxq_deinit(mp->rxq + i);
out:
	free_irq(dev->irq, dev);

	return err;
}

static void port_reset(struct geth_private *mp)
{
	unsigned int data;
	int i;

	for (i = 0; i < mp->rxq_count; i++)
		rxq_disable(mp->rxq + i);
	for (i = 0; i < mp->txq_count; i++)
		txq_disable(mp->txq + i);

	/*while (1) {
		u32 ps = rdlp(mp, ETH_EPSR);

		if (ps & TX_IN_PROGRESS)
			break;
		udelay(10);
	}*/

	/* Reset the Enable bit in the Configuration Register */
	data = rdlp(mp, ETH_EPCR);
	data &= ~ETH_PORT_ENABLE;
	wrlp(mp, ETH_EPCR, data);
}

static int geth_stop(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);
	int i;

	wrlp(mp, ETH_EIMR, 0x00000000);

	napi_disable(&mp->napi);

	del_timer_sync(&mp->rx_oom);

	netif_carrier_off(dev);

	free_irq(dev->irq, dev);

	port_reset(mp);
	geth_get_stats(dev);

	for (i = 0; i < mp->rxq_count; i++)
		rxq_deinit(mp->rxq + i);
	for (i = 0; i < mp->txq_count; i++)
		txq_deinit(mp->txq + i);

	return 0;
}

static int geth_change_mtu(struct net_device *dev, int new_mtu)
{
	struct geth_private *mp = netdev_priv(dev);

	if (new_mtu < 64 || new_mtu > 9500)
		return -EINVAL;

	dev->mtu = new_mtu;
	geth_recalc_skb_size(mp);

	if (!netif_running(dev))
		return 0;

	/*
	 * Stop and then re-open the interface. This will allocate RX
	 * skbs of the new MTU.
	 * There is a possible danger that the open will not succeed,
	 * due to memory being full.
	 */
	geth_stop(dev);
	if (geth_open(dev)) {
		dev_printk(KERN_ERR, &dev->dev,
			   "fatal error on re-opening device after "
			   "MTU change\n");
	}

	return 0;
}

static void tx_timeout_task(struct work_struct *ugly)
{
	struct geth_private *mp;

	mp = container_of(ugly, struct geth_private, tx_timeout_task);
	if (netif_running(mp->dev)) {
		netif_tx_stop_all_queues(mp->dev);
		port_reset(mp);
		port_start(mp);
		netif_tx_wake_all_queues(mp->dev);
	}
}

static void geth_tx_timeout(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);

	dev_printk(KERN_INFO, &dev->dev, "tx timeout\n");

	schedule_work(&mp->tx_timeout_task);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void geth_netpoll(struct net_device *dev)
{
	struct geth_private *mp = netdev_priv(dev);

	wrlp(mp, ETH_EIMR, 0);

	geth_irq(dev->irq, dev);

	wrlp(mp, ETH_EIMR, mp->int_mask);
}
#endif


/* platform glue ************************************************************/
static void phy_addr_set(struct geth_private *mp, int phy_addr)
{
	u32 data;

	data = rdlp(mp, ETH_EPCXR);
	/* FC_AN must be disabled before changing PHY address: page302 */
	wrlp(mp, ETH_EPCXR, data | ETH_FC_AN_DISABLE);
	/* change PHY address */
	wrlp(mp, ETH_EPAR, phy_addr & 0x1f);
	/* recover EPCXR register setting */
	wrlp(mp, ETH_EPCXR, data);
}

static void set_params(struct geth_private *mp,
		       struct geth_platform_data *pd)
{
	struct net_device *dev = mp->dev;

	memcpy(dev->dev_addr, galois_mac_addr, ETH_ALEN);
	mp->port_num = pd->port_number;

	mp->rx_ring_size = DEFAULT_RX_QUEUE_SIZE;
	if (pd->rx_queue_size)
		mp->rx_ring_size = pd->rx_queue_size;

	mp->rxq_count = pd->rx_queue_count ? : 1;

	mp->tx_ring_size = DEFAULT_TX_QUEUE_SIZE;
	if (pd->tx_queue_size)
		mp->tx_ring_size = pd->tx_queue_size;

	mp->txq_count = pd->tx_queue_count ? : 1;
}

static void init_pscr(struct geth_private *mp)
{
	u32 pcxr;

	wrlp(mp, ETH_EPCR, 0);
	pcxr = ETH_FC_AN_DISABLE | ETH_FLP_DISABLE | ETH_FC_ENABLE | ETH_MAC_RX_2BSTUFF;

	/* Only use HIGH TXQ when only one TXQ, so set all pkts are from HIGH*/
	if (mp->txq_count == 1)
		pcxr |= (7 << 3);
	wrlp(mp, ETH_EPCXR, pcxr);
}

static int geth_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct geth_private *mp = netdev_priv(dev);

	spin_lock_irq(&mp->lock);
	mii_ethtool_gset(&mp->mii, cmd);
	spin_unlock_irq(&mp->lock);

	return 0;
}

static void geth_get_drvinfo(struct net_device *dev,
			     struct ethtool_drvinfo *drvinfo)
{
	strncpy(drvinfo->driver,  "geth", 32);
	strncpy(drvinfo->version, "1.0", 32);
	strncpy(drvinfo->bus_info, "platform", 32);
}

static u32 geth_get_link(struct net_device *dev)
{
	return !!netif_carrier_ok(dev);
}

static const struct ethtool_ops geth_ethtool_ops = {
	.get_drvinfo		= geth_get_drvinfo,
	.get_settings		= geth_get_settings,
	.get_link		= geth_get_link,
};

static const struct net_device_ops geth_netdev_ops = {
	.ndo_open		= geth_open,
	.ndo_stop		= geth_stop,
	.ndo_start_xmit		= geth_xmit,
	.ndo_set_rx_mode	= geth_set_multicast_list,
	.ndo_set_mac_address	= geth_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= geth_change_mtu,
	.ndo_tx_timeout		= geth_tx_timeout,
	.ndo_get_stats		= geth_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= geth_netpoll,
#endif
};

static int geth_probe(struct platform_device *pdev)
{
	struct geth_platform_data dummy, *pd;
	struct geth_private *mp;
	struct net_device *dev;
	struct resource *res;
	int err;

	dev_printk(KERN_WARNING, &pdev->dev,
		   "use dummy geth_platform_data\n");
	memset(&dummy, 0, sizeof(dummy));
	pd = &dummy;

	dev = alloc_etherdev_mq(sizeof(struct geth_private), 4);
	if (!dev)
		return -ENOMEM;

	mp = netdev_priv(dev);
	platform_set_drvdata(pdev, mp);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	BUG_ON(!res);
	mp->base = (void __iomem *)res->start;

	mp->dev = dev;

	set_params(mp, pd);
	dev->real_num_tx_queues = mp->txq_count;

	init_pscr(mp);
	phy_addr_set(mp, PHY_ADDR);
	phy_init(mp);

	SET_ETHTOOL_OPS(dev, &geth_ethtool_ops);

	init_hash_table(mp);
	geth_program_unicast_filter(dev);

	spin_lock_init(&mp->lock);

	mib_counters_clear(mp);

	INIT_WORK(&mp->tx_timeout_task, tx_timeout_task);

	netif_napi_add(dev, &mp->napi, geth_poll, 128);

	init_timer(&mp->rx_oom);
	mp->rx_oom.data = (unsigned long)mp;
	mp->rx_oom.function = oom_timer_wrapper;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	BUG_ON(!res);
	dev->irq = res->start;

	dev->netdev_ops = &geth_netdev_ops;

	dev->watchdog_timeo = 2 * HZ;
	dev->base_addr = 0;

	SET_NETDEV_DEV(dev, &pdev->dev);

	wrlp(mp, ETH_ESDCR, PORT_SDMA_CONFIG_DEFAULT_VALUE);

	err = register_netdev(dev);
	if (err)
		goto out;

	netif_carrier_off(dev);

	dev_printk(KERN_NOTICE, &dev->dev, "port %d with MAC address %pM\n",
		   mp->port_num, dev->dev_addr);

	return 0;

out:
	free_netdev(dev);

	return err;
}

static int geth_remove(struct platform_device *pdev)
{
	struct geth_private *mp = platform_get_drvdata(pdev);

	unregister_netdev(mp->dev);
	dma_free_coherent(mp->dev->dev.parent, 0x200*4*8,
			  mp->hash_tbl, mp->hash_dma);
	flush_scheduled_work();
	free_netdev(mp->dev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void geth_shutdown(struct platform_device *pdev)
{
	struct geth_private *mp = platform_get_drvdata(pdev);

	/* Mask all interrupts on ethernet port */
	wrlp(mp, ETH_EIMR, 0);

	if (netif_running(mp->dev))
		port_reset(mp);
}

#ifdef CONFIG_PM
static int geth_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i;
	struct geth_private *mp = platform_get_drvdata(pdev);

	if (netif_running(mp->dev))
		geth_stop(mp->dev);
	netif_device_detach(mp->dev);

	/* save configuration space */
	for (i = 0; i < 12; i++)
		mp->saved_config_space[i] = rdlp(mp, ETH_EPCR + i*8);

	return 0;
}

static int geth_resume(struct platform_device *pdev)
{
	int i;
	struct geth_private *mp = platform_get_drvdata(pdev);

	/* restore configuration space */
	for (i = 0; i < 12; i++)
		wrlp(mp, ETH_EPCR + i*8, mp->saved_config_space[i]);

	/* restore phy state, including autoneg */
	phy_addr_set(mp, PHY_ADDR);
	phy_init(mp);

	netif_device_attach(mp->dev);
	if (netif_running(mp->dev))
		geth_open(mp->dev);

	return 0;
}
#endif

static struct of_device_id geth_match[] = {
	{ .compatible = "mrvl,fastethernet", },
	{},
};
MODULE_DEVICE_TABLE(of, geth_match);

static struct platform_driver geth_driver = {
	.probe		= geth_probe,
	.remove		= geth_remove,
	.shutdown	= geth_shutdown,
	.driver = {
		.name	= "mrvl_fe",
		.owner	= THIS_MODULE,
		.of_match_table = geth_match,
	},
#ifdef CONFIG_PM
	.suspend	= geth_suspend,
	.resume		= geth_resume,
#endif
};

module_platform_driver(geth_driver);

MODULE_AUTHOR("Jisheng Zhang  <jszhang@marvell.com>");
MODULE_DESCRIPTION("Driver for Marvell Berlin SoC Fast Ethernet");
