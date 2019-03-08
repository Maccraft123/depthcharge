/*
 * Copyright 2012 Google Inc.
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

#include <image/fmap.h>
#include <libpayload.h>
#include <lzma.h>
#include <vboot_api.h>

#include "config.h"
#include "base/vpd_util.h"
#include "drivers/flash/flash.h"
#include "vboot/util/flag.h"

uint32_t VbExIsShutdownRequested(void)
{
	int lidsw = flag_fetch(FLAG_LIDSW);
	int pwrsw = 0;
	uint32_t shutdown_request = 0;

	// Power button is always handled as a keyboard key in detachable UI.
	if (!IS_ENABLED(CONFIG_DETACHABLE_UI))
		pwrsw = flag_fetch(FLAG_PWRSW);

	if (lidsw < 0 || pwrsw < 0) {
		// There isn't any way to return an error, so just hang.
		printf("Failed to fetch lid or power switch flag.\n");
		halt();
	}

	if (!lidsw) {
		printf("Lid is closed.\n");
		shutdown_request |= VB_SHUTDOWN_REQUEST_LID_CLOSED;
	}
	if (pwrsw) {
		printf("Power key pressed.\n");
		shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;
	}

	return shutdown_request;
}

VbError_t VbExSetVendorData(const char *vendor_data_value)
{
	FmapArea vpd_area_descriptor;
	uint8_t *ro_vpd;
	int size, offset;

	printf("%s: Setting %s to '%s'\n", __func__,
	       CONFIG_VENDOR_DATA_KEY, vendor_data_value);
	if (strnlen(vendor_data_value, CONFIG_VENDOR_DATA_LENGTH + 1) !=
		CONFIG_VENDOR_DATA_LENGTH)
		return VBERROR_INVALID_PARAMETER;

	if (fmap_find_area("RO_VPD", &vpd_area_descriptor)) {
		printf("%s: failed to find RO_VPD area\n", __func__);
		return VBERROR_VPD_WRITE;
	}

	/* Read RO VPD from flash */
	ro_vpd = flash_read(vpd_area_descriptor.offset,
			    vpd_area_descriptor.size);

	if (!ro_vpd) {
		printf("%s: failed to read RO_VPD area\n", __func__);
		return VBERROR_VPD_WRITE;
	}

	/* Find the vendor data value */
	if (!vpd_find(CONFIG_VENDOR_DATA_KEY, ro_vpd, &offset, &size) ||
	    size != CONFIG_VENDOR_DATA_LENGTH)
		return VBERROR_VPD_WRITE;

	/* Set vendor data to new value */
	if (flash_rewrite(vpd_area_descriptor.offset + offset,
			  CONFIG_VENDOR_DATA_LENGTH, vendor_data_value) !=
			  CONFIG_VENDOR_DATA_LENGTH) {
		printf("%s: failed to rewrite RO_VPD area\n", __func__);
		return VBERROR_VPD_WRITE;
	}

	/* TODO: set SRWD and block protection bits */

	return VBERROR_SUCCESS;
}
