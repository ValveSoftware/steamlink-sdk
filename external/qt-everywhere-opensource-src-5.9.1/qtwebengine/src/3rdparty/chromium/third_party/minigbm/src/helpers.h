/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HELPERS_H
#define HELPERS_H

#include "drv.h"

size_t drv_num_planes_from_format(uint32_t format);
int drv_bpp_from_format(uint32_t format, size_t plane);
int drv_bo_from_format(struct bo *bo, uint32_t width, uint32_t height,
		       drv_format_t format);
int drv_dumb_bo_create(struct bo *bo, uint32_t width, uint32_t height,
		       uint32_t format, uint32_t flags);
int drv_dumb_bo_destroy(struct bo *bo);
int drv_gem_bo_destroy(struct bo *bo);
void *drv_dumb_bo_map(struct bo *bo, struct map_info *data, size_t plane);
uintptr_t drv_get_reference_count(struct driver *drv, struct bo *bo,
				  size_t plane);
void drv_increment_reference_count(struct driver *drv, struct bo *bo,
				   size_t plane);
void drv_decrement_reference_count(struct driver *drv, struct bo *bo,
				   size_t plane);
uint32_t drv_log_base2(uint32_t value);
#endif
