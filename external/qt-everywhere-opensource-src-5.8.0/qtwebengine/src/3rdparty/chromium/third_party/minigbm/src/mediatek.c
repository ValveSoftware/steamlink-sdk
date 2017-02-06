/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef GBM_MEDIATEK

#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <mediatek_drm.h>
#include <stdio.h>
#include "gbm_priv.h"
#include "helpers.h"

static int gbm_mediatek_bo_create(struct gbm_bo *bo,
				  uint32_t width, uint32_t height,
				  uint32_t format, uint32_t flags)
{
	size_t size;
	struct drm_mtk_gem_create gem_create;
	int ret;

	bo->strides[0] = gbm_stride_from_format(format, width);
	size = height * bo->strides[0];

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_MTK_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_MTK_GEM_CREATE failed "
				"(size=%zu)\n", size);
		return ret;
	}

	bo->handles[0].u32 = gem_create.handle;
	bo->sizes[0] = size;
	bo->offsets[0] = 0;

	return 0;
}

const struct gbm_driver gbm_driver_mediatek =
{
	.name = "mediatek",
	.bo_create = gbm_mediatek_bo_create,
	.bo_destroy = gbm_gem_bo_destroy,
	.format_list = {
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
	}
};

#endif
