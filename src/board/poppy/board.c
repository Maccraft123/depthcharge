/*
 * Copyright (C) 2016 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "boot/commandline.h"
#include "config.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/skylake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/skylake.h"
#include "drivers/sound/gpio_i2s.h"
#include "drivers/sound/max98927.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

#define SORAKA_AUX_CTRL_MIN_HW_VERSION		5

/*
 * On soraka, we need to pass an additional kernel command line parameter if
 * board version >= 5. This is required to enable proper backlight control in
 * the kernel. However, if this command line parameter is set by default on the
 * board, then backlight control will be broken on older boards. So, until we
 * phase out older boards, this hack is required to ensure that backlight
 * control works properly on all HW versions.
 *
 * TODO(furquan@): Remove this once older boards are phased out.
 */
const char *mainboard_commandline(void)
{
	if (strcmp(CONFIG_BOARD, "soraka"))
		return NULL;

	if (lib_sysinfo.board_id < SORAKA_AUX_CTRL_MIN_HW_VERSION)
		return NULL;

	return " i915.enable_dpcd_backlight=1 ";
}

static int cr50_irq_status(void)
{
	return skylake_get_gpe(GPE0_DW2_00);
}

static void poppy_setup_tpm(void)
{
	if (IS_ENABLED(CONFIG_DRIVER_TPM_SPI)) {
		/* SPI TPM */
		const IntelGspiSetupParams gspi0_params = {
			.dev = PCI_DEV(0, 0x1e, 2),
			.cs_polarity = SPI_POLARITY_LOW,
			.clk_phase = SPI_CLOCK_PHASE_FIRST,
			.clk_polarity = SPI_POLARITY_LOW,
			.ref_clk_mhz = 120,
			.gspi_clk_mhz = 1,
		};
		tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0_params),
					 cr50_irq_status)->ops);
	} else if (IS_ENABLED(CONFIG_DRIVER_TPM_CR50_I2C)) {
		DesignwareI2c *i2c1 = new_pci_designware_i2c(
			PCI_DEV(0, 0x15, 1), 400000, SKYLAKE_DW_I2C_MHZ);
		tpm_set_ops(&new_cr50_i2c(&i2c1->ops, 0x50,
					  &cr50_irq_status)->base.ops);
	}
}

static void board_audio_init(SoundRoute *sound)
{
	if (IS_ENABLED(CONFIG_DRIVER_SOUND_MAX98927)) {
		/* Speaker amp is Maxim 98927 codec on I2C5 */
		DesignwareI2c *i2c5 =
			new_pci_designware_i2c(PCI_DEV(0, 0x19, 1), 400000, 120);
		Max98927Codec *speaker_amp_l =
			new_max98927_codec(&i2c5->ops, 0x3a, 16, 16000, 64, 2000);
		Max98927Codec *speaker_amp_r =
			new_max98927_codec(&i2c5->ops, 0x39, 16, 16000, 64, 2000);
		list_insert_after(&speaker_amp_l->component.list_node,
			&sound->components);
		list_insert_after(&speaker_amp_r->component.list_node,
			&sound->components);
	} else if (IS_ENABLED(CONFIG_DRIVER_SOUND_MAX98357A)) {
		/* Speaker amp is Maxim 98357a codec*/
		GpioOps *sdmode_gpio = &new_skylake_gpio_output(GPP_A23, 0)->ops;
		max98357aCodec *speaker_amp =
				new_max98357a_codec(sdmode_gpio);
		list_insert_after(&speaker_amp->component.list_node,
				&sound->components);
	}
}

static int board_setup(void)
{
	sysinfo_install_flags(new_skylake_gpio_input_from_coreboot);

	/* TPM */
	poppy_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 16MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	/* PCH Power */
	power_set_ops(&skylake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1e, 4),
			SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SSD if config enabled */
	if (IS_ENABLED(CONFIG_DRIVER_AHCI)) {
		AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
		list_insert_after(&ahci->ctrlr.list_node,
				  &fixed_block_dev_controllers);
	}

	/* NVMe if config enabled */
	if (IS_ENABLED(CONFIG_DRIVER_STORAGE_NVME)) {
		/* Root port 5 */
		NvmeCtrlr *nvme1 = new_nvme_ctrlr(PCI_DEV(0, 0x1c, 4));
		list_insert_after(&nvme1->ctrlr.list_node,
				  &fixed_block_dev_controllers);

		/* Root port 9 */
		NvmeCtrlr *nvme2 = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
		list_insert_after(&nvme2->ctrlr.list_node,
				  &fixed_block_dev_controllers);
	}

	/* SD Card (if present) */
	pcidev_t sd_pci_dev = PCI_DEV(0, 0x1e, 6);
	uint16_t sd_vendor_id = pci_read_config32(sd_pci_dev, REG_VENDOR_ID);
	if (sd_vendor_id == PCI_VENDOR_ID_INTEL) {
		SdhciHost *sd = new_pci_sdhci_host(sd_pci_dev, 1,
					EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
					&removable_block_dev_controllers);
	}
	/* Activate buffer to disconnect I2S from PCH and allow GPIO */
	GpioCfg *i2s2_buffer_enable = new_skylake_gpio_output(GPP_D22, 1);
	gpio_set(&i2s2_buffer_enable->ops, 0);

	/* Use GPIO to provide PCM clock to the codec */
	GpioCfg *i2s2_bclk = new_skylake_gpio_output(GPP_F0, 0);
	GpioCfg *i2s2_sfrm = new_skylake_gpio_output(GPP_F1, 0);
	GpioCfg *i2s2_txd  = new_skylake_gpio_output(GPP_F2, 0);
	GpioI2s *i2s = new_gpio_i2s(
			&i2s2_bclk->ops,    /* I2S Bit Clock GPIO */
			&i2s2_sfrm->ops,    /* I2S Frame Sync GPIO */
			&i2s2_txd->ops,     /* I2S Data GPIO */
			16000,              /* Sample rate */
			2,                  /* Channels */
			0x1FFF);            /* Volume */
	SoundRoute *sound = new_sound_route(&i2s->ops);
	board_audio_init(sound);
	sound_set_ops(&sound->ops);

	return 0;
}

INIT_FUNC(board_setup);