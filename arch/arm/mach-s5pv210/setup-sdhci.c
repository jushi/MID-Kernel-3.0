/* linux/arch/arm/mach-s5pv210/setup-sdhci.c
 *
 * Copyright (c) 2009-2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV210 - Helper functions for settign up SDHCI device(s) (HSMMC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include <plat/regs-sdhci.h>
#include <plat/sdhci.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>

/* clock sources for the mmc bus clock, order as for the ctrl2[5..4] */

char *s5pv210_hsmmc_clksrcs[4] = {
	[0] = "hsmmc",		/* HCLK */
	[1] = NULL/*"hsmmc"*/,		/* duplicate HCLK entry */
	[2] = "sclk_mmc",	/* mmc_bus */
	[3] = NULL,			/* reserved */
};

#define S3C_SDHCI_CTRL3_FCSELTX_INVERT  (0)
#define S3C_SDHCI_CTRL3_FCSELTX_BASIC \
	(S3C_SDHCI_CTRL3_FCSEL3 | S3C_SDHCI_CTRL3_FCSEL2)
#define S3C_SDHCI_CTRL3_FCSELRX_INVERT  (0)
#define S3C_SDHCI_CTRL3_FCSELRX_BASIC \
	(S3C_SDHCI_CTRL3_FCSEL1 | S3C_SDHCI_CTRL3_FCSEL0)

void s5pv210_setup_sdhci_cfg_card(struct platform_device *dev,
				    void __iomem *r,
				    struct mmc_ios *ios,
				    struct mmc_card *card)
{
	u32 ctrl2, ctrl3;

	/* don't need to alter anything according to card-type */

	ctrl2 = readl(r + S3C_SDHCI_CONTROL2);

	/* select base clock source to HCLK */

	ctrl2 &= S3C_SDHCI_CTRL2_SELBASECLK_MASK;

	/*
	 * clear async mode, enable conflict mask, rx feedback ctrl, SD
	 * clk hold and no use debounce count
	 */

	ctrl2 |= (S3C64XX_SDHCI_CTRL2_ENSTAASYNCCLR |
		  S3C64XX_SDHCI_CTRL2_ENCMDCNFMSK |
		  S3C_SDHCI_CTRL2_ENFBCLKRX |
		  S3C_SDHCI_CTRL2_DFCNT_NONE |
		  S3C_SDHCI_CTRL2_ENCLKOUTHOLD);

	/* Tx and Rx feedback clock delay control */

	if (ios->clock < 25 * 1000000)
		ctrl3 = (S3C_SDHCI_CTRL3_FCSEL3 |
			 S3C_SDHCI_CTRL3_FCSEL2 |
			 S3C_SDHCI_CTRL3_FCSEL1 |
			 S3C_SDHCI_CTRL3_FCSEL0);
	else
		ctrl3 = (S3C_SDHCI_CTRL3_FCSEL1 | S3C_SDHCI_CTRL3_FCSEL0);

	writel(ctrl2, r + S3C_SDHCI_CONTROL2);
	writel(ctrl3, r + S3C_SDHCI_CONTROL3);

}

void s5pv210_adjust_sdhci_cfg_card(struct s3c_sdhci_platdata *pdata,
				   void __iomem *r, int rw)
{
	u32 ctrl2, ctrl3;

	ctrl2 = readl(r + S3C_SDHCI_CONTROL2);
	ctrl3 = readl(r + S3C_SDHCI_CONTROL3);

	if (rw == 0) {
		pdata->rx_cfg++;
		if (pdata->rx_cfg == 1) {
			ctrl2 |= S3C_SDHCI_CTRL2_ENFBCLKRX;
			ctrl3 |= S3C_SDHCI_CTRL3_FCSELRX_BASIC;
		} else if (pdata->rx_cfg == 2) {
			ctrl2 |= S3C_SDHCI_CTRL2_ENFBCLKRX;
			ctrl3 &= ~S3C_SDHCI_CTRL3_FCSELRX_BASIC;
		} else if (pdata->rx_cfg == 3) {
			ctrl2 &= ~(S3C_SDHCI_CTRL2_ENFBCLKTX |
				   S3C_SDHCI_CTRL2_ENFBCLKRX);
			pdata->rx_cfg = 0;
		}
	} else if (rw == 1) {
		pdata->tx_cfg++;
		if (pdata->tx_cfg == 1) {
			if (ctrl2 & S3C_SDHCI_CTRL2_ENFBCLKRX) {
				ctrl2 |= S3C_SDHCI_CTRL2_ENFBCLKTX;
				ctrl3 |= S3C_SDHCI_CTRL3_FCSELTX_BASIC;
			} else {
				ctrl2 &= ~S3C_SDHCI_CTRL2_ENFBCLKTX;
			}
		} else if (pdata->tx_cfg == 2) {
			ctrl2 &= ~S3C_SDHCI_CTRL2_ENFBCLKTX;
			pdata->tx_cfg = 0;
		}
	} else {
		printk(KERN_ERR "%s, unknown value rw:%d\n", __func__, rw);
		return;
	}

	writel(ctrl2, r + S3C_SDHCI_CONTROL2);
	writel(ctrl3, r + S3C_SDHCI_CONTROL3);
}

void universal_sdhci2_cfg_ext_cd(void)
{
	printk(KERN_DEBUG "Universal :SD Detect configuration\n");
	s3c_gpio_setpull(S5PV210_GPH3(4), S3C_GPIO_PULL_NONE);
	irq_set_irq_type(IRQ_EINT(28), IRQ_TYPE_EDGE_BOTH);
}

static struct s3c_sdhci_platdata hsmmc0_platdata = {
	.cd_type = S3C_SDHCI_CD_PERMANENT,
#if defined(CONFIG_S5PV210_SD_CH0_8BIT)
	.max_width	= 8,
	.host_caps	= MMC_CAP_8_BIT_DATA,
#endif
};

#if defined(CONFIG_S3C_DEV_HSMMC1)
static struct s3c_sdhci_platdata hsmmc1_platdata = {
	.clk_type = S3C_SDHCI_CLK_DIV_EXTERNAL,
	.cd_type = S3C_SDHCI_CD_PERMANENT,
	.max_width	= 4
};
#endif

#if defined(CONFIG_S3C_DEV_HSMMC2)
static struct s3c_sdhci_platdata hsmmc2_platdata = {
#if defined(CONFIG_S5PV210_SD_CH2_8BIT)
	.max_width	= 8,
	.host_caps	= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#if defined(CONFIG_S3C_DEV_HSMMC3)
static struct s3c_sdhci_platdata hsmmc3_platdata = {
	.cd_type = S3C_SDHCI_CD_PERMANENT,
	.max_width	= 4
};
#endif

/*
 * call this when you need sd stack to recognize insertion or removal of card
 * that can't be told by SDHCI regs
 */
void sdhci_s3c_force_presence_change(struct platform_device *pdev)
{
	pr_warn("%s: called for device with no notifier\n", __func__);
}
EXPORT_SYMBOL_GPL(sdhci_s3c_force_presence_change);

void s3c_sdhci_set_platdata(void)
{
#if defined(CONFIG_S3C_DEV_HSMMC)
    // namko: MID: Internal SD
	s3c_sdhci0_set_platdata(&hsmmc0_platdata);
#endif
#if defined(CONFIG_S3C_DEV_HSMMC1)
    // namko: MID: Atheros WiFi
	s3c_sdhci1_set_platdata(&hsmmc1_platdata);
#endif
#if defined(CONFIG_S3C_DEV_HSMMC2)
    // namko: MID: External SD
	s3c_sdhci2_set_platdata(&hsmmc2_platdata);
#endif
#if defined(CONFIG_S3C_DEV_HSMMC3)
    // namko: MID: Broadcom WiFi
	s3c_sdhci3_set_platdata(&hsmmc3_platdata);
#endif
};
