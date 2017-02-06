/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>

#include "gbm_priv.h"
#include "helpers.h"
#include "util.h"

size_t gbm_num_planes_from_format(uint32_t format)
{
	switch(format)
	{
		case GBM_FORMAT_C8:
		case GBM_FORMAT_R8:
		case GBM_FORMAT_RG88:
		case GBM_FORMAT_GR88:
		case GBM_FORMAT_RGB332:
		case GBM_FORMAT_BGR233:
		case GBM_FORMAT_XRGB4444:
		case GBM_FORMAT_XBGR4444:
		case GBM_FORMAT_RGBX4444:
		case GBM_FORMAT_BGRX4444:
		case GBM_FORMAT_ARGB4444:
		case GBM_FORMAT_ABGR4444:
		case GBM_FORMAT_RGBA4444:
		case GBM_FORMAT_BGRA4444:
		case GBM_FORMAT_XRGB1555:
		case GBM_FORMAT_XBGR1555:
		case GBM_FORMAT_RGBX5551:
		case GBM_FORMAT_BGRX5551:
		case GBM_FORMAT_ARGB1555:
		case GBM_FORMAT_ABGR1555:
		case GBM_FORMAT_RGBA5551:
		case GBM_FORMAT_BGRA5551:
		case GBM_FORMAT_RGB565:
		case GBM_FORMAT_BGR565:
		case GBM_FORMAT_YUYV:
		case GBM_FORMAT_YVYU:
		case GBM_FORMAT_UYVY:
		case GBM_FORMAT_VYUY:
		case GBM_FORMAT_RGB888:
		case GBM_FORMAT_BGR888:
		case GBM_FORMAT_XRGB8888:
		case GBM_FORMAT_XBGR8888:
		case GBM_FORMAT_RGBX8888:
		case GBM_FORMAT_BGRX8888:
		case GBM_FORMAT_ARGB8888:
		case GBM_FORMAT_ABGR8888:
		case GBM_FORMAT_RGBA8888:
		case GBM_FORMAT_BGRA8888:
		case GBM_FORMAT_XRGB2101010:
		case GBM_FORMAT_XBGR2101010:
		case GBM_FORMAT_RGBX1010102:
		case GBM_FORMAT_BGRX1010102:
		case GBM_FORMAT_ARGB2101010:
		case GBM_FORMAT_ABGR2101010:
		case GBM_FORMAT_RGBA1010102:
		case GBM_FORMAT_BGRA1010102:
		case GBM_FORMAT_AYUV:
			return 1;
		case GBM_FORMAT_NV12:
			return 2;
	}

	fprintf(stderr, "minigbm: UNKNOWN FORMAT %d\n", format);
	return 0;
}

int gbm_bpp_from_format(uint32_t format)
{
	switch(format)
	{
		case GBM_FORMAT_C8:
		case GBM_FORMAT_R8:
		case GBM_FORMAT_RGB332:
		case GBM_FORMAT_BGR233:
			return 8;

		case GBM_FORMAT_NV12:
			return 12;

		case GBM_FORMAT_RG88:
		case GBM_FORMAT_GR88:
		case GBM_FORMAT_XRGB4444:
		case GBM_FORMAT_XBGR4444:
		case GBM_FORMAT_RGBX4444:
		case GBM_FORMAT_BGRX4444:
		case GBM_FORMAT_ARGB4444:
		case GBM_FORMAT_ABGR4444:
		case GBM_FORMAT_RGBA4444:
		case GBM_FORMAT_BGRA4444:
		case GBM_FORMAT_XRGB1555:
		case GBM_FORMAT_XBGR1555:
		case GBM_FORMAT_RGBX5551:
		case GBM_FORMAT_BGRX5551:
		case GBM_FORMAT_ARGB1555:
		case GBM_FORMAT_ABGR1555:
		case GBM_FORMAT_RGBA5551:
		case GBM_FORMAT_BGRA5551:
		case GBM_FORMAT_RGB565:
		case GBM_FORMAT_BGR565:
		case GBM_FORMAT_YUYV:
		case GBM_FORMAT_YVYU:
		case GBM_FORMAT_UYVY:
		case GBM_FORMAT_VYUY:
			return 16;

		case GBM_FORMAT_RGB888:
		case GBM_FORMAT_BGR888:
			return 24;

		case GBM_FORMAT_XRGB8888:
		case GBM_FORMAT_XBGR8888:
		case GBM_FORMAT_RGBX8888:
		case GBM_FORMAT_BGRX8888:
		case GBM_FORMAT_ARGB8888:
		case GBM_FORMAT_ABGR8888:
		case GBM_FORMAT_RGBA8888:
		case GBM_FORMAT_BGRA8888:
		case GBM_FORMAT_XRGB2101010:
		case GBM_FORMAT_XBGR2101010:
		case GBM_FORMAT_RGBX1010102:
		case GBM_FORMAT_BGRX1010102:
		case GBM_FORMAT_ARGB2101010:
		case GBM_FORMAT_ABGR2101010:
		case GBM_FORMAT_RGBA1010102:
		case GBM_FORMAT_BGRA1010102:
		case GBM_FORMAT_AYUV:
			return 32;
	}

	fprintf(stderr, "minigbm: UNKNOWN FORMAT %d\n", format);
	return 0;
}

int gbm_stride_from_format(uint32_t format, uint32_t width)
{
	/* Only single-plane formats are supported */
	assert(gbm_num_planes_from_format(format) == 1);
	return DIV_ROUND_UP(width * gbm_bpp_from_format(format), 8);
}

int gbm_is_format_supported(struct gbm_bo *bo)
{
	return 1;
}

int gbm_dumb_bo_create(struct gbm_bo *bo, uint32_t width, uint32_t height,
		       uint32_t format, uint32_t flags)
{
	struct drm_mode_create_dumb create_dumb;
	int ret;

	/* Only single-plane formats are supported */
	assert(gbm_num_planes_from_format(format) == 1);

	memset(&create_dumb, 0, sizeof(create_dumb));
	create_dumb.height = height;
	create_dumb.width = width;
	create_dumb.bpp = gbm_bpp_from_format(format);
	create_dumb.flags = 0;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_MODE_CREATE_DUMB failed\n");
		return ret;
	}

	bo->handles[0].u32 = create_dumb.handle;
	bo->offsets[0] = 0;
	bo->sizes[0] = create_dumb.size;
	bo->strides[0] = create_dumb.pitch;

	return 0;
}

int gbm_dumb_bo_destroy(struct gbm_bo *bo)
{
	struct drm_mode_destroy_dumb destroy_dumb;
	int ret;

	memset(&destroy_dumb, 0, sizeof(destroy_dumb));
	destroy_dumb.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_MODE_DESTROY_DUMB failed "
				"(handle=%x)\n", bo->handles[0].u32);
		return ret;
	}

	return 0;
}

int gbm_gem_bo_destroy(struct gbm_bo *bo)
{
	struct drm_gem_close gem_close;
	int ret, error = 0;
	size_t plane, i;

	for (plane = 0; plane < bo->num_planes; plane++) {
		bool already_closed = false;
		for (i = 1; i < plane && !already_closed; i++)
			if (bo->handles[i-1].u32 == bo->handles[plane].u32)
				already_closed = true;
		if (already_closed)
			continue;

		memset(&gem_close, 0, sizeof(gem_close));
		gem_close.handle = bo->handles[plane].u32;

		ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
		if (ret) {
			fprintf(stderr, "minigbm: DRM_IOCTL_GEM_CLOSE failed "
					"(handle=%x) error %d\n",
					bo->handles[plane].u32, ret);
			error = ret;
		}
	}

	return error;
}
