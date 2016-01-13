/*
 * Marvell SDHCI controller driver for the platform bus.
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "sdhci-pltfm.h"


// DDL seting value
#define SDHCI_MV_DLL_CTL	0x118
#define SDHCI_MV_DLL_ENABLE_MASK	0x80000000
#define SDHCI_MV_DLL_ENABLE 0x80000000

#define SDHCI_MV_DLL_VALUE_MASK	0x3FF
#define SDHCI_MV_DLL_HIGH_VALUE	0x1F
#define SDHCI_MV_DLL_LOW_VALUE	0x70

struct pltfm_mv_data {
	int flags;
};

static const struct of_device_id sdhci_mv_dt_ids[] = {
	{ .compatible = "mrvl,berlin-sdhci",},
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_mv_dt_ids);

static unsigned int sdhci_mv_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return clk_get_rate(pltfm_host->clk);
}
static void sdhci_mv_set_clock(struct sdhci_host *host, unsigned int clock)
{
  int dll_value = 0; /* Initialized for compiler warning */
  dll_value = sdhci_readl(host, SDHCI_MV_DLL_CTL);
  if((clock >= 100000000) && ((dll_value & SDHCI_MV_DLL_ENABLE_MASK) ==0))
  { //enable DLL
    dll_value &= (~SDHCI_MV_DLL_VALUE_MASK);
    dll_value |= (SDHCI_MV_DLL_HIGH_VALUE & SDHCI_MV_DLL_VALUE_MASK);
    dll_value |= SDHCI_MV_DLL_ENABLE;
    sdhci_writel(host, dll_value, SDHCI_MV_DLL_CTL);
  }
  else if(clock < 100000000 && (dll_value & SDHCI_MV_DLL_ENABLE_MASK))
  { //disable DLL
    dll_value &= (~SDHCI_MV_DLL_VALUE_MASK);
    dll_value &= (~SDHCI_MV_DLL_ENABLE_MASK);
    dll_value |= (SDHCI_MV_DLL_LOW_VALUE & SDHCI_MV_DLL_VALUE_MASK);
    sdhci_writel(host, dll_value, SDHCI_MV_DLL_CTL);
  }
}
static struct sdhci_ops sdhci_mv_ops = {
	.get_max_clock = sdhci_mv_get_max_clock,
	.set_clock = sdhci_mv_set_clock,
};

static struct sdhci_pltfm_data sdhci_mv_pdata = {
	.quirks	= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN |
			SDHCI_QUIRK_INVERTED_WRITE_PROTECT |
			SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.ops = &sdhci_mv_ops,
};

static void sdhci_mv_probe_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sdhci_host *host = platform_get_drvdata(pdev);

	if (of_get_property(np, "mrvl,card-wired", NULL)) {
		host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
		host->mmc->caps |= MMC_CAP_NONREMOVABLE;
	}

	if (of_get_property(np, "mrvl,8bit-data", NULL))
		host->mmc->caps |= MMC_CAP_8_BIT_DATA;

	if (of_get_property(np, "mrvl,host-off-card-on", NULL))
		host->quirks2 = SDHCI_QUIRK2_HOST_OFF_CARD_ON;

	if (of_get_property(np, "mrvl,no-hispd", NULL))
		host->quirks |= SDHCI_QUIRK_NO_HISPD_BIT;
}

static int sdhci_mv_probe(struct platform_device *pdev)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_host *host;
	struct clk *clk;
	int err;
	struct pltfm_mv_data *mv_data;

	host = sdhci_pltfm_init(pdev, &sdhci_mv_pdata);
	if (IS_ERR(host))
		return PTR_ERR(host);

	pltfm_host = sdhci_priv(host);

	mv_data = devm_kzalloc(&pdev->dev, sizeof(*mv_data), GFP_KERNEL);
	if (!mv_data) {
		err = -ENOMEM;
		goto err_mv_data;
	}

	pltfm_host->priv = mv_data;

	clk = clk_get(mmc_dev(host->mmc), NULL);
	if (IS_ERR(clk)) {
		dev_err(mmc_dev(host->mmc), "clk err\n");
		err = PTR_ERR(clk);
		goto err_mv_data;
	}
	clk_prepare_enable(clk);
	pltfm_host->clk = clk;

	host->quirks |= SDHCI_QUIRK_BROKEN_TIMEOUT_VAL;

	sdhci_mv_probe_dt(pdev);

	host->mmc->pm_caps = MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;

	err = sdhci_add_host(host);
	if (err)
		goto err_add_host;

	return 0;

err_add_host:
	clk_disable_unprepare(pltfm_host->clk);
	clk_put(pltfm_host->clk);
err_mv_data:
	sdhci_pltfm_free(pdev);
	return err;
}

static int sdhci_mv_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	int dead = (readl(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);

	sdhci_remove_host(host, dead);
	clk_disable_unprepare(pltfm_host->clk);
	clk_put(pltfm_host->clk);
	sdhci_pltfm_free(pdev);

	return 0;
}

static struct platform_driver sdhci_mv_driver = {
	.driver		= {
		.name	= "mv-sdhci",
		.owner	= THIS_MODULE,
		.of_match_table = sdhci_mv_dt_ids,
		.pm	= SDHCI_PLTFM_PMOPS,
	},
	.probe		= sdhci_mv_probe,
	.remove		= sdhci_mv_remove,
};

module_platform_driver(sdhci_mv_driver);

MODULE_DESCRIPTION("SDHCI driver for Marvell Berlin SoC");
MODULE_AUTHOR("Jisheng Zhang <jszhang@marvell.com>");
MODULE_LICENSE("GPL v2");
