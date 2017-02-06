/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef GBM_EXYNOS

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <exynos_drm.h>

#include "gbm_priv.h"
#include "helpers.h"
#include "util.h"

static int gbm_exynos_bo_create(struct gbm_bo *bo,
				uint32_t width, uint32_t height,
				uint32_t format, uint32_t flags)
{
	size_t plane;

	if (format == GBM_FORMAT_NV12) {
		uint32_t chroma_height;
		/* V4L2 s5p-mfc requires width to be 16 byte aligned and height 32. */
		width = ALIGN(width, 16);
		height = ALIGN(height, 32);
		chroma_height = ALIGN(height / 2, 32);
		bo->strides[0] = bo->strides[1] = width;
		/* MFC v8+ requires 64 byte padding in the end of luma and chroma buffers. */
		bo->sizes[0] = bo->strides[0] * height + 64;
		bo->sizes[1] = bo->strides[1] * chroma_height + 64;
		bo->offsets[0] = bo->offsets[1] = 0;
	} else if (format == GBM_FORMAT_XRGB8888 || format == GBM_FORMAT_ARGB8888) {
		bo->strides[0] = gbm_stride_from_format(format, width);
		bo->sizes[0] = height * bo->strides[0];
		bo->offsets[0] = 0;
	} else {
		fprintf(stderr, "minigbm: unsupported format %X\n", format);
		assert(0);
		return -EINVAL;
	}

	int ret;
	for (plane = 0; plane < bo->num_planes; plane++) {
		size_t size = bo->sizes[plane];
		struct drm_exynos_gem_create gem_create;

		memset(&gem_create, 0, sizeof(gem_create));
		gem_create.size = size;
		gem_create.flags = EXYNOS_BO_NONCONTIG;

		ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_EXYNOS_GEM_CREATE, &gem_create);
		if (ret) {
			fprintf(stderr, "minigbm: DRM_IOCTL_EXYNOS_GEM_CREATE failed "
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

const struct gbm_driver gbm_driver_exynos =
{
	.name = "exynos",
	.bo_create = gbm_exynos_bo_create,
	.bo_destroy = gbm_gem_bo_destroy,
	.format_list = {
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		{GBM_FORMAT_NV12, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING},
	}
};

#endif
