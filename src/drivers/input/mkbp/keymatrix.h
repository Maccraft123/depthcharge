/*
 * Copyright 2013 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __DRIVERS_INPUT_MKBP_KEYMATRIX_H__
#define __DRIVERS_INPUT_MKBP_KEYMATRIX_H__

#include <stdint.h>

typedef struct MkbpKeymatrix {
	int rows;
	int cols;
	uint16_t **scancodes;
	uint16_t *button_scancodes;
} MkbpKeymatrix;

extern MkbpKeymatrix mkbp_keymatrix;

#endif /* __DRIVERS_INPUT_MKBP_KEYMATRIX_H__ */
