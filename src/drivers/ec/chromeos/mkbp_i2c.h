/*
 * Chromium OS mkbp driver - I2C interface
 *
 * Copyright 2013 Google Inc.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_EC_CHROMEOS_MKBP_I2C_H__
#define __DRIVERS_EC_CHROMEOS_MKBP_I2C_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/chromeos/message.h"
#include "drivers/ec/chromeos/mkbp.h"

struct I2cOps;
typedef struct I2cOps I2cOps;

typedef struct
{
	uint8_t din[MSG_BYTES];
	uint8_t dout[MSG_BYTES];

	MkbpBusOps ops;
	I2cOps *bus;
	uint8_t chip;

	int initialized;
} MkbpI2cBus;

MkbpI2cBus *new_mkbp_i2c_bus(I2cOps *bus, uint8_t chip);

#endif /* __DRIVERS_EC_CHROMEOS_MKBP_I2C_H__ */
