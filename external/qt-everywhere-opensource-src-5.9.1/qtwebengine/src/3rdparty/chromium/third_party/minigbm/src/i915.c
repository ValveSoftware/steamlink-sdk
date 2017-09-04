/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <i915_drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

struct i915_device
{
	int gen;
};


static int get_gen(int device_id)
{
	const uint16_t gen3_ids[] = {0x2582, 0x2592, 0x2772, 0x27A2, 0x27AE,
				     0x29C2, 0x29B2, 0x29D2, 0xA001, 0xA011};
	unsigned i;
	for(i = 0; i < ARRAY_SIZE(gen3_ids); i++)
		if (gen3_ids[i] == device_id)
			return 3;

	return 4;
}

static int i915_init(struct driver *drv)
{
	struct i915_device *i915_drv;
	drm_i915_getparam_t get_param;
	int device_id;
	int ret;

	i915_drv = (struct i915_device*)malloc(sizeof(*i915_drv));
	if (!i915_drv)
		return -1;

	memset(&get_param, 0, sizeof(get_param));
	get_param.param = I915_PARAM_CHIPSET_ID;
	get_param.value = &device_id;
	ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GETPARAM, &get_param);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GETPARAM failed\n");
		free(i915_drv);
		return -1;
	}

	i915_drv->gen = get_gen(device_id);

	drv->priv = i915_drv;

	return 0;
}

static void i915_close(struct driver *drv)
{
	free(drv->priv);
	drv->priv = NULL;
}

static void i915_align_dimensions(struct driver *drv, uint32_t tiling_mode,
				  uint32_t *width, uint32_t *height, int bpp)
{
	struct i915_device *i915_drv = (struct i915_device *)drv->priv;
	uint32_t width_alignment = 4, height_alignment = 4;

	switch (tiling_mode) {
	default:
	case I915_TILING_NONE:
		width_alignment = 64 / bpp;
		break;

	case I915_TILING_X:
		width_alignment = 512 / bpp;
		height_alignment = 8;
		break;

	case I915_TILING_Y:
		if (i915_drv->gen == 3) {
			width_alignment = 512 / bpp;
			height_alignment = 8;
		} else  {
			width_alignment = 128 / bpp;
			height_alignment = 32;
		}
		break;
	}

	if (i915_drv->gen > 3) {
		*width = ALIGN(*width, width_alignment);
		*height = ALIGN(*height, height_alignment);
	} else {
		uint32_t w;
		for (w = width_alignment; w < *width;  w <<= 1)
			;
		*width = w;
		*height = ALIGN(*height, height_alignment);
	}
}

static int i915_verify_dimensions(struct driver *drv, uint32_t stride,
				  uint32_t height)
{
	struct i915_device *i915_drv = (struct i915_device *)drv->priv;
	if (i915_drv->gen <= 3 && stride > 8192)
		return 0;

	return 1;
}

static int i915_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			  uint32_t format, uint32_t flags)
{
	struct driver *drv = bo->drv;
	int bpp = drv_stride_from_format(format, 1, 0);
	struct drm_i915_gem_create gem_create;
	struct drm_i915_gem_set_tiling gem_set_tiling;
	uint32_t tiling_mode = I915_TILING_NONE;
	size_t plane;
	int ret;

	if (flags & (DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
		     DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN))
		tiling_mode = I915_TILING_NONE;
	else if (flags & DRV_BO_USE_SCANOUT)
		tiling_mode = I915_TILING_X;
	else
		tiling_mode = I915_TILING_Y;

	i915_align_dimensions(drv, tiling_mode, &width, &height, bpp);

	drv_bo_from_format(bo, width, height, format);

	if (!i915_verify_dimensions(drv, bo->strides[0], height))
		return EINVAL;

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = bo->total_size;

	ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_CREATE failed "
				"(size=%llu)\n", gem_create.size);
		return ret;
	}

	for (plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = gem_create.handle;

	memset(&gem_set_tiling, 0, sizeof(gem_set_tiling));
	do {
		gem_set_tiling.handle = bo->handles[0].u32;
		gem_set_tiling.tiling_mode = tiling_mode;
		gem_set_tiling.stride = bo->strides[0];
		ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GEM_SET_TILING,
			       &gem_set_tiling);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	if (ret == -1) {
		struct drm_gem_close gem_close;
		gem_close.handle = bo->handles[0].u32;
		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_SET_TILING failed "
				"errno=%x (handle=%x, tiling=%x, stride=%x)\n",
				errno,
				gem_set_tiling.handle,
				gem_set_tiling.tiling_mode,
				gem_set_tiling.stride);
		drmIoctl(drv->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
		return -errno;
	}

	return 0;
}

static void *i915_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	struct drm_i915_gem_mmap_gtt gem_map;

	memset(&gem_map, 0, sizeof(gem_map));
	gem_map.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &gem_map);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_MMAP_GTT failed\n");
		return MAP_FAILED;
	}

	data->length = bo->total_size;

	return mmap(0, bo->total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, gem_map.offset);
}

static drv_format_t i915_resolve_format(drv_format_t format)
{
	switch (format) {
	case DRV_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRV_FORMAT_XBGR8888;
	case DRV_FORMAT_FLEX_YCbCr_420_888:
		return DRV_FORMAT_YVU420;
	default:
		return format;
	}
}

const struct backend backend_i915 =
{
	.name = "i915",
	.init = i915_init,
	.close = i915_close,
	.bo_create = i915_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = i915_bo_map,
	.resolve_format = i915_resolve_format,
	.format_list = {
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_XBGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_XBGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_ABGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_ABGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING | DRV_BO_USE_CURSOR
				      | DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_XRGB1555, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_ARGB1555, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_RGB565,   DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_UYVY,     DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_UYVY,     DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_YUYV,     DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING
				      | DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_YUYV,     DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_R8,	      DRV_BO_USE_SCANOUT | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_GR88,     DRV_BO_USE_SCANOUT | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_YVU420,   DRV_BO_USE_RENDERING | DRV_BO_USE_SW_READ_RARELY |
				      DRV_BO_USE_SW_WRITE_RARELY},
	}
};

#endif
