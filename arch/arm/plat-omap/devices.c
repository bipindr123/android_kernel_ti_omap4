/*
 * linux/arch/arm/plat-omap/devices.c
 *
 * Common platform device setup/initialization for OMAP1 and OMAP2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/memblock.h>
#include <linux/err.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/tc.h>
#include <plat/board.h>
#include <plat/mmc.h>
#include <mach/gpio.h>
#include <plat/menelaus.h>
#include <plat/mcbsp.h>
#include <plat/remoteproc.h>
#include <plat/omap44xx.h>

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_OMAP_MCBSP) || defined(CONFIG_OMAP_MCBSP_MODULE)

static struct platform_device **omap_mcbsp_devices;

void omap_mcbsp_register_board_cfg(struct resource *res, int res_count,
			struct omap_mcbsp_platform_data *config, int size)
{
	int i;

	omap_mcbsp_devices = kzalloc(size * sizeof(struct platform_device *),
				     GFP_KERNEL);
	if (!omap_mcbsp_devices) {
		printk(KERN_ERR "Could not register McBSP devices\n");
		return;
	}

	for (i = 0; i < size; i++) {
		struct platform_device *new_mcbsp;
		int ret;

		new_mcbsp = platform_device_alloc("omap-mcbsp", i + 1);
		if (!new_mcbsp)
			continue;
		platform_device_add_resources(new_mcbsp, &res[i * res_count],
					res_count);
		new_mcbsp->dev.platform_data = &config[i];
		ret = platform_device_add(new_mcbsp);
		if (ret) {
			platform_device_put(new_mcbsp);
			continue;
		}
		omap_mcbsp_devices[i] = new_mcbsp;
	}
}

#else
void omap_mcbsp_register_board_cfg(struct resource *res, int res_count,
			struct omap_mcbsp_platform_data *config, int size)
{  }
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_SND_OMAP_SOC_DMIC) || \
    defined(CONFIG_SND_OMAP_SOC_DMIC_MODULE)

static struct omap_device_pm_latency omap_dmic_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_dmic(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;

	oh = omap_hwmod_lookup("dmic");
	if (!oh) {
		printk(KERN_ERR "Could not look up dmic hw_mod\n");
		return;
	}

	od = omap_device_build("omap-dmic-dai", -1, oh, NULL, 0,
				omap_dmic_latency,
				ARRAY_SIZE(omap_dmic_latency), 0);
	if (IS_ERR(od))
		printk(KERN_ERR "Could not build omap_device for omap-dmic-dai\n");
}
#else
static inline void omap_init_dmic(void) {}
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE) || \
	defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)

#define OMAP_MMC_NR_RES		2

/*
 * Register MMC devices. Called from mach-omap1 and mach-omap2 device init.
 */
int __init omap_mmc_add(const char *name, int id, unsigned long base,
				unsigned long size, unsigned int irq,
				struct omap_mmc_platform_data *data)
{
	struct platform_device *pdev;
	struct resource res[OMAP_MMC_NR_RES];
	int ret;

	pdev = platform_device_alloc(name, id);
	if (!pdev)
		return -ENOMEM;

	memset(res, 0, OMAP_MMC_NR_RES * sizeof(struct resource));
	res[0].start = base;
	res[0].end = base + size - 1;
	res[0].flags = IORESOURCE_MEM;
	res[1].start = res[1].end = irq;
	res[1].flags = IORESOURCE_IRQ;

	ret = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
	if (ret == 0)
		ret = platform_device_add_data(pdev, data, sizeof(*data));
	if (ret)
		goto fail;

	ret = platform_device_add(pdev);
	if (ret)
		goto fail;

	/* return device handle to board setup code */
	data->dev = &pdev->dev;
	return 0;

fail:
	platform_device_put(pdev);
	return ret;
}

#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_HW_RANDOM_OMAP) || defined(CONFIG_HW_RANDOM_OMAP_MODULE)

#ifdef CONFIG_ARCH_OMAP2
#define	OMAP_RNG_BASE		0x480A0000
#else
#define	OMAP_RNG_BASE		0xfffe5000
#endif

static struct resource rng_resources[] = {
	{
		.start		= OMAP_RNG_BASE,
		.end		= OMAP_RNG_BASE + 0x4f,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device omap_rng_device = {
	.name		= "omap_rng",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rng_resources),
	.resource	= rng_resources,
};

static void omap_init_rng(void)
{
	(void) platform_device_register(&omap_rng_device);
}
#else
static inline void omap_init_rng(void) {}
#endif

/* Numbering for the SPI-capable controllers when used for SPI:
 * spi		= 1
 * uwire	= 2
 * mmc1..2	= 3..4
 * mcbsp1..3	= 5..7
 */

#if defined(CONFIG_SPI_OMAP_UWIRE) || defined(CONFIG_SPI_OMAP_UWIRE_MODULE)

#define	OMAP_UWIRE_BASE		0xfffb3000

static struct resource uwire_resources[] = {
	{
		.start		= OMAP_UWIRE_BASE,
		.end		= OMAP_UWIRE_BASE + 0x20,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device omap_uwire_device = {
	.name	   = "omap_uwire",
	.id	     = -1,
	.num_resources	= ARRAY_SIZE(uwire_resources),
	.resource	= uwire_resources,
};

static void omap_init_uwire(void)
{
	/* FIXME define and use a boot tag; not all boards will be hooking
	 * up devices to the microwire controller, and multi-board configs
	 * mean that CONFIG_SPI_OMAP_UWIRE may be configured anyway...
	 */

	/* board-specific code must configure chipselects (only a few
	 * are normally used) and SCLK/SDI/SDO (each has two choices).
	 */
	(void) platform_device_register(&omap_uwire_device);
}
#else
static inline void omap_init_uwire(void) {}
#endif

#if defined(CONFIG_TIDSPBRIDGE) || defined(CONFIG_TIDSPBRIDGE_MODULE) \
				|| defined(CONFIG_OMAP_REMOTE_PROC_DSP)

static phys_addr_t omap_dsp_phys_mempool_base;
static phys_addr_t omap_dsp_phys_mempool_size;

void __init omap_dsp_reserve_sdram_memblock(void)
{
#if defined(CONFIG_OMAP_REMOTE_PROC_DSP)
	phys_addr_t size = CONFIG_OMAP_REMOTEPROC_MEMPOOL_SIZE_DSP;
#else
	phys_addr_t size = CONFIG_TIDSPBRIDGE_MEMPOOL_SIZE;
#endif
	phys_addr_t paddr;

	if (!size)
		return;

	paddr = memblock_alloc(size, SZ_1M);
	if (!paddr) {
		pr_err("%s: failed to reserve %x bytes\n",
				__func__, size);
		return;
	}
	memblock_free(paddr, size);
	memblock_remove(paddr, size);

	omap_dsp_phys_mempool_base = paddr;
	omap_dsp_phys_mempool_size = size;
}
#endif

#if defined(CONFIG_TIDSPBRIDGE) || defined(CONFIG_TIDSPBRIDGE_MODULE)
phys_addr_t omap_dsp_get_mempool_base(void)
{
	return omap_dsp_phys_mempool_base;
}
EXPORT_SYMBOL(omap_dsp_get_mempool_base);

phys_addr_t omap_dsp_get_mempool_size(void)
{
	return omap_dsp_phys_mempool_size;
}
EXPORT_SYMBOL(omap_dsp_get_mempool_size);
#endif

#if defined(CONFIG_OMAP_REMOTE_PROC_DSP) && !defined(CONFIG_MACH_TUNA)
static phys_addr_t omap_dsp_phys_st_mempool_base;
static phys_addr_t omap_dsp_phys_st_mempool_size;

void __init omap_dsp_set_static_mempool(u32 start, u32 size)
{
	omap_dsp_phys_st_mempool_base = start;
	omap_dsp_phys_st_mempool_size = size;
}

phys_addr_t omap_dsp_get_mempool_tbase(enum omap_rproc_mempool_type type)
{
	switch (type) {
	case OMAP_RPROC_MEMPOOL_STATIC:
		return omap_dsp_phys_st_mempool_base;
	case OMAP_RPROC_MEMPOOL_DYNAMIC:
		return omap_dsp_phys_mempool_base;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(omap_dsp_get_mempool_tbase);

phys_addr_t omap_dsp_get_mempool_tsize(enum omap_rproc_mempool_type type)
{
	switch (type) {
	case OMAP_RPROC_MEMPOOL_STATIC:
		return omap_dsp_phys_st_mempool_size;
	case OMAP_RPROC_MEMPOOL_DYNAMIC:
		return omap_dsp_phys_mempool_size;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(omap_dsp_get_mempool_tsize);
#endif

#if defined(CONFIG_OMAP_REMOTE_PROC_IPU) || defined(CONFIG_MACH_TUNA) && defined(CONFIG_OMAP_REMOTE_PROC)
static phys_addr_t omap_ipu_phys_mempool_base;
static u32 omap_ipu_phys_mempool_size;
static phys_addr_t omap_ipu_phys_st_mempool_base;
static u32 omap_ipu_phys_st_mempool_size;

void __init omap_ipu_reserve_sdram_memblock(void)
{
	/* currently handles only ipu. dsp will be handled later...*/
	u32 size = CONFIG_OMAP_REMOTEPROC_MEMPOOL_SIZE;
	phys_addr_t paddr;

	if (!size)
		return;

	paddr = memblock_alloc(size, SZ_1M);
	if (!paddr) {
		pr_err("%s: failed to reserve %x bytes\n",
				__func__, size);
		return;
	}
	memblock_free(paddr, size);
	memblock_remove(paddr, size);

	omap_ipu_phys_mempool_base = paddr;
	omap_ipu_phys_mempool_size = size;
}

void __init omap_ipu_set_static_mempool(u32 start, u32 size)
{
	omap_ipu_phys_st_mempool_base = start;
	omap_ipu_phys_st_mempool_size = size;
}

phys_addr_t omap_ipu_get_mempool_base(enum omap_rproc_mempool_type type)
{
	switch (type) {
	case OMAP_RPROC_MEMPOOL_STATIC:
		return omap_ipu_phys_st_mempool_base;
	case OMAP_RPROC_MEMPOOL_DYNAMIC:
		return omap_ipu_phys_mempool_base;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(omap_ipu_get_mempool_base);

u32 omap_ipu_get_mempool_size(enum omap_rproc_mempool_type type)
{
	switch (type) {
	case OMAP_RPROC_MEMPOOL_STATIC:
		return omap_ipu_phys_st_mempool_size;
	case OMAP_RPROC_MEMPOOL_DYNAMIC:
		return omap_ipu_phys_mempool_size;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(omap_ipu_get_mempool_size);
#endif

/*
 * This gets called after board-specific INIT_MACHINE, and initializes most
 * on-chip peripherals accessible on this board (except for few like USB):
 *
 *  (a) Does any "standard config" pin muxing needed.  Board-specific
 *	code will have muxed GPIO pins and done "nonstandard" setup;
 *	that code could live in the boot loader.
 *  (b) Populating board-specific platform_data with the data drivers
 *	rely on to handle wiring variations.
 *  (c) Creating platform devices as meaningful on this board and
 *	with this kernel configuration.
 *
 * Claiming GPIOs, and setting their direction and initial values, is the
 * responsibility of the device drivers.  So is responding to probe().
 *
 * Board-specific knowledge like creating devices or pin setup is to be
 * kept out of drivers as much as possible.  In particular, pin setup
 * may be handled by the boot loader, and drivers should expect it will
 * normally have been done by the time they're probed.
 */
static int __init omap_init_devices(void)
{
	/* please keep these calls, and their implementations above,
	 * in alphabetical order so they're easier to sort through.
	 */
	omap_init_rng();
	omap_init_dmic();
	omap_init_uwire();
	return 0;
}
arch_initcall(omap_init_devices);
