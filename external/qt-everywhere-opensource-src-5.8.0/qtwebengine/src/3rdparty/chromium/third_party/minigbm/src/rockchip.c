/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef GBM_ROCKCHIP

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <rockchip_drm.h>

#include "gbm_priv.h"
#include "helpers.h"
#include "util.h"

static int gbm_rockchip_bo_create(struct gbm_bo *bo,
				  uint32_t width, uint32_t height,
				  uint32_t format, uint32_t flags)
{
	size_t plane;

	switch (format) {
	case GBM_FORMAT_NV12:
		width = ALIGN(width, 4);
		height = ALIGN(height, 4);
		bo->strides[0] = bo->strides[1] = width;
		bo->sizes[0] = height * bo->strides[0];
		bo->sizes[1] = height * bo->strides[1] / 2;
		bo->offsets[0] = bo->offsets[1] = 0;
		break;
	case GBM_FORMAT_XRGB8888:
	case GBM_FORMAT_ARGB8888:
	case GBM_FORMAT_ABGR8888:
		bo->strides[0] = gbm_stride_from_format(format, width);
		bo->sizes[0] = height * bo->strides[0];
		bo->offsets[0] = 0;
		break;
	default:
		fprintf(stderr, "minigbm: rockchip: unsupported format %4.4s\n",
			(char*)&format);
		assert(0);
		return -EINVAL;
	}

	int ret;
	for (plane = 0; plane < bo->num_planes; plane++) {
		size_t size = bo->sizes[plane];
		struct drm_rockchip_gem_create gem_create;

		memset(&gem_create, 0, sizeof(gem_create));
		gem_create.size = size;

		ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_ROCKCHIP_GEM_CREATE,
			       &gem_create);
		if (ret) {
			fprintf(stderr, "minigbm: DRM_IOCTL_ROCKCHIP_GEM_CREATE failed "
					"(size=%zu)\n", size);
			goto cleanup_planes;
		}

		bo->handles[plane].u32 = gem_create.handle;
	}

	return 0;

cleanup_planes:
	for ( ; plane != 0; plane--) {
		struct drm_gem_close gem_close;
		memset(&gem_close, 0, sizeof(gem_close));
		gem_close.handle = bo->handles[plane - 1].u32;
		int gem_close_ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_GEM_CLOSE,
					     &gem_close);
		if (gem_close_ret) {
			fprintf(stderr,
				"minigbm: DRM_IOCTL_GEM_CLOSE failed: %d\n",
				gem_close_ret);
		}
	}

	return ret;
}

const struct gbm_driver gbm_driver_rockchip =
{
	.name = "rockchip",
	.bo_create = gbm_rockchip_bo_create,
	.bo_destroy = gbm_gem_bo_destroy,
	.format_list = {
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		{GBM_FORMAT_ABGR8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_NV12, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_NV12, GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR},
	}
};

#endif
