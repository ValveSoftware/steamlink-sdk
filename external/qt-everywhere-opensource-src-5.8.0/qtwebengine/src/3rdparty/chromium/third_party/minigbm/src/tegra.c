/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef GBM_TEGRA

#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <tegra_drm.h>

#include "gbm_priv.h"
#include "helpers.h"
#include "util.h"

/*
 * GOB (Group Of Bytes) is the basic unit of the blocklinear layout.
 * GOBs are arranged to blocks, where the height of the block (measured
 * in GOBs) is configurable.
 */
#define NV_BLOCKLINEAR_GOB_HEIGHT 8
#define NV_BLOCKLINEAR_GOB_WIDTH 64
#define NV_DEFAULT_BLOCK_HEIGHT_LOG2 4
#define NV_PREFERRED_PAGE_SIZE (128 * 1024)

enum nv_mem_kind
{
	NV_MEM_KIND_PITCH = 0,
	NV_MEM_KIND_GENERIC_16Bx2 = 0xfe,
};

static int compute_block_height_log2(int height)
{
	int block_height_log2 = NV_DEFAULT_BLOCK_HEIGHT_LOG2;

	if (block_height_log2 > 0) {
		/* Shrink, if a smaller block height could cover the whole
		 * surface height. */
		int proposed = NV_BLOCKLINEAR_GOB_HEIGHT << (block_height_log2 - 1);
		while (proposed >= height) {
			block_height_log2--;
			if (block_height_log2 == 0)
				break;
			proposed /= 2;
		}
	}
	return block_height_log2;
}

static void compute_layout_blocklinear(int width, int height, int format,
				       enum nv_mem_kind *kind,
				       uint32_t *block_height_log2,
				       uint32_t *stride, uint32_t *size)
{
	int pitch = gbm_stride_from_format(format, width);

	/* Align to blocklinear blocks. */
	pitch = ALIGN(pitch, NV_BLOCKLINEAR_GOB_WIDTH);

	/* Compute padded height. */
	*block_height_log2 = compute_block_height_log2(height);
	int block_height = 1 << *block_height_log2;
	int padded_height =
		ALIGN(height, NV_BLOCKLINEAR_GOB_HEIGHT * block_height);

	int bytes = pitch * padded_height;

	/* Pad the allocation to the preferred page size.
	 * This will reduce the required page table size (see discussion in NV
	 * bug 1321091), and also acts as a WAR for NV bug 1325421.
	 */
	bytes = ALIGN(bytes, NV_PREFERRED_PAGE_SIZE);

	*kind = NV_MEM_KIND_GENERIC_16Bx2;
	*stride = pitch;
	*size = bytes;
}

static void compute_layout_linear(int width, int height, int format,
				  uint32_t *stride, uint32_t *size)
{
	*stride = gbm_stride_from_format(format, width);
	*size = *stride * height;
}

static int gbm_tegra_bo_create(struct gbm_bo *bo, uint32_t width,
			       uint32_t height, uint32_t format, uint32_t flags)
{
	uint32_t size, stride, block_height_log2 = 0;
	enum nv_mem_kind kind = NV_MEM_KIND_PITCH;
	struct drm_tegra_gem_create gem_create;
	int ret;

	if (flags & GBM_BO_USE_RENDERING)
		compute_layout_blocklinear(width, height, format, &kind,
					   &block_height_log2, &stride, &size);
	else
		compute_layout_linear(width, height, format, &stride, &size);

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;
	gem_create.flags = 0;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_TEGRA_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_TEGRA_GEM_CREATE failed "
				"(size=%zu)\n", size);
		return ret;
	}

	bo->handles[0].u32 = gem_create.handle;
	bo->offsets[0] = 0;
	bo->sizes[0] = size;
	bo->strides[0] = stride;

	if (kind != NV_MEM_KIND_PITCH) {
		struct drm_tegra_gem_set_tiling gem_tile;

		memset(&gem_tile, 0, sizeof(gem_tile));
		gem_tile.handle = bo->handles[0].u32;
		gem_tile.mode = DRM_TEGRA_GEM_TILING_MODE_BLOCK;
		gem_tile.value = block_height_log2;

		ret = drmCommandWriteRead(bo->gbm->fd, DRM_TEGRA_GEM_SET_TILING,
					  &gem_tile, sizeof(gem_tile));
		if (ret < 0) {
			gbm_gem_bo_destroy(bo);
			return ret;
		}

		/* Encode blocklinear parameters for EGLImage creation. */
		bo->tiling = (kind & 0xff) |
			     ((block_height_log2 & 0xf) << 8);
		bo->format_modifiers[0] = gbm_fourcc_mod_code(NV, bo->tiling);
	}

	return 0;
}

const struct gbm_driver gbm_driver_tegra =
{
	.name = "tegra",
	.bo_create = gbm_tegra_bo_create,
	.bo_destroy = gbm_gem_bo_destroy,
	.format_list = {
		/* Linear support */
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_LINEAR},
		/* Blocklinear support */
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING},
	}
};

#endif
