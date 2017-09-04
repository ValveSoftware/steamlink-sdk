/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_TEGRA

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <tegra_drm.h>

#include "drv_priv.h"
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
	NV_MEM_KIND_C32_2CRA = 0xdb,
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
	int pitch = drv_stride_from_format(format, width, 0);

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

	*kind = NV_MEM_KIND_C32_2CRA;
	*stride = pitch;
	*size = bytes;
}

static void compute_layout_linear(int width, int height, int format,
				  uint32_t *stride, uint32_t *size)
{
	*stride = drv_stride_from_format(format, width, 0);
	*size = *stride * height;
}

static int tegra_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			   uint32_t format, uint32_t flags)
{
	uint32_t size, stride, block_height_log2 = 0;
	enum nv_mem_kind kind = NV_MEM_KIND_PITCH;
	struct drm_tegra_gem_create gem_create;
	int ret;

	if (flags & DRV_BO_USE_RENDERING)
		compute_layout_blocklinear(width, height, format, &kind,
					   &block_height_log2, &stride, &size);
	else
		compute_layout_linear(width, height, format, &stride, &size);

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;
	gem_create.flags = 0;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_TEGRA_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_TEGRA_GEM_CREATE failed "
				"(size=%zu)\n", size);
		return ret;
	}

	bo->handles[0].u32 = gem_create.handle;
	bo->offsets[0] = 0;
	bo->total_size = bo->sizes[0] = size;
	bo->strides[0] = stride;

	if (kind != NV_MEM_KIND_PITCH) {
		struct drm_tegra_gem_set_tiling gem_tile;

		memset(&gem_tile, 0, sizeof(gem_tile));
		gem_tile.handle = bo->handles[0].u32;
		gem_tile.mode = DRM_TEGRA_GEM_TILING_MODE_BLOCK;
		gem_tile.value = block_height_log2;

		ret = drmCommandWriteRead(bo->drv->fd, DRM_TEGRA_GEM_SET_TILING,
					  &gem_tile, sizeof(gem_tile));
		if (ret < 0) {
			drv_gem_bo_destroy(bo);
			return ret;
		}

		/* Encode blocklinear parameters for EGLImage creation. */
		bo->tiling = (kind & 0xff) |
			     ((block_height_log2 & 0xf) << 8);
		bo->format_modifiers[0] = drv_fourcc_mod_code(NV, bo->tiling);
	}

	return 0;
}

static void *tegra_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	struct drm_tegra_gem_mmap gem_map;

	memset(&gem_map, 0, sizeof(gem_map));
	gem_map.handle = bo->handles[0].u32;

	ret = drmCommandWriteRead(bo->drv->fd, DRM_TEGRA_GEM_MMAP, &gem_map,
				  sizeof(gem_map));
	if (ret < 0) {
		fprintf(stderr, "drv: DRM_TEGRA_GEM_MMAP failed\n");
		return MAP_FAILED;
	}

	data->length = bo->total_size;

	return mmap(0, bo->total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, gem_map.offset);
}

const struct backend backend_tegra =
{
	.name = "tegra",
	.bo_create = tegra_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = tegra_bo_map,
	.format_list = {
		/* Linear support */
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR
				      | DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR
				      | DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		/* Blocklinear support */
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
	}
};

#endif
