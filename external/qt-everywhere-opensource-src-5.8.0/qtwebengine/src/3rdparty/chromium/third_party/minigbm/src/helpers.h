/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HELPERS_H
#define HELPERS_H

size_t gbm_num_planes_from_format(uint32_t format);
int gbm_bpp_from_format(uint32_t format);
int gbm_stride_from_format(uint32_t format, uint32_t width);
int gbm_is_format_supported(struct gbm_bo *bo);
int gbm_dumb_bo_create(struct gbm_bo *bo, uint32_t width, uint32_t height,
		       uint32_t format, uint32_t flags);
int gbm_dumb_bo_destroy(struct gbm_bo *bo);
int gbm_gem_bo_destroy(struct gbm_bo *bo);

#endif
