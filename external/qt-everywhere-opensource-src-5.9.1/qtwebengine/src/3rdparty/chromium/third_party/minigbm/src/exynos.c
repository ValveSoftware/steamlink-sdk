/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_EXYNOS

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <exynos_drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static int exynos_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			    uint32_t format, uint32_t flags)
{
	size_t plane;

	if (format == DRV_FORMAT_NV12) {
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
		bo->total_size = bo->sizes[0] + bo->sizes[1];
	} else if (format == DRV_FORMAT_XRGB8888 || format == DRV_FORMAT_ARGB8888) {
		bo->strides[0] = drv_stride_from_format(format, width, 0);
		bo->total_size = bo->sizes[0] = height * bo->strides[0];
		bo->offsets[0] = 0;
	} else {
		fprintf(stderr, "drv: unsupported format %X\n", format);
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

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_EXYNOS_GEM_CREATE, &gem_create);
		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_EXYNOS_GEM_CREATE failed "
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
		int gem_close_ret = drmIoctl(bo->drv->fd, DRM_IOCTL_GEM_CLOSE,
					     &gem_close);
		if (gem_close_ret) {
			fprintf(stderr,
				"drv: DRM_IOCTL_GEM_CLOSE failed: %d\n",
				gem_close_ret);
		}
	}

	return ret;
}

/*
 * Use dumb mapping with exynos even though a GEM buffer is created.
 * libdrm does the same thing in exynos_drm.c
 */
const struct backend backend_exynos =
{
	.name = "exynos",
	.bo_create = exynos_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = drv_dumb_bo_map,
	.format_list = {
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
		{DRV_FORMAT_NV12, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING},
	}
};

#endif
