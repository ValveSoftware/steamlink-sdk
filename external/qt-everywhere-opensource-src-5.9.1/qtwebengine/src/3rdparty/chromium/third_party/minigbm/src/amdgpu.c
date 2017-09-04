/*
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifdef DRV_AMDGPU
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#include <amdgpu_drm.h>
#include <amdgpu.h>

#include "addrinterface.h"
#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#ifndef CIASICIDGFXENGINE_SOUTHERNISLAND
#define CIASICIDGFXENGINE_SOUTHERNISLAND 0x0000000A
#endif

#define mmCC_RB_BACKEND_DISABLE		0x263d
#define mmGB_TILE_MODE0			0x2644
#define mmGB_MACROTILE_MODE0		0x2664
#define mmGB_ADDR_CONFIG		0x263e
#define mmMC_ARB_RAMCFG			0x9d8

enum {
	FAMILY_UNKNOWN,
	FAMILY_SI,
	FAMILY_CI,
	FAMILY_KV,
	FAMILY_VI,
	FAMILY_CZ,
	FAMILY_PI,
	FAMILY_LAST,
};

static int amdgpu_set_metadata(int fd, uint32_t handle,
			       struct amdgpu_bo_metadata *info)
{
	struct drm_amdgpu_gem_metadata args = {0};

	if (!info)
		return -EINVAL;

	args.handle = handle;
	args.op = AMDGPU_GEM_METADATA_OP_SET_METADATA;
	args.data.flags = info->flags;
	args.data.tiling_info = info->tiling_info;

	if (info->size_metadata > sizeof(args.data.data))
		return -EINVAL;

	if (info->size_metadata) {
		args.data.data_size_bytes = info->size_metadata;
		memcpy(args.data.data, info->umd_metadata, info->size_metadata);
	}

	return drmCommandWriteRead(fd, DRM_AMDGPU_GEM_METADATA, &args,
				   sizeof(args));
}

static int amdgpu_read_mm_regs(int fd, unsigned dword_offset,
			       unsigned count, uint32_t instance,
			       uint32_t flags, uint32_t *values)
{
	struct drm_amdgpu_info request;

	memset(&request, 0, sizeof(request));
	request.return_pointer = (uintptr_t) values;
	request.return_size = count * sizeof(uint32_t);
	request.query = AMDGPU_INFO_READ_MMR_REG;
	request.read_mmr_reg.dword_offset = dword_offset;
	request.read_mmr_reg.count = count;
	request.read_mmr_reg.instance = instance;
	request.read_mmr_reg.flags = flags;

	return drmCommandWrite(fd, DRM_AMDGPU_INFO, &request,
			       sizeof(struct drm_amdgpu_info));
}

static int amdgpu_query_gpu(int fd, struct amdgpu_gpu_info *gpu_info)
{
	int ret;
	uint32_t instance;

	if (!gpu_info)
		return -EINVAL;

	instance = AMDGPU_INFO_MMR_SH_INDEX_MASK <<
			AMDGPU_INFO_MMR_SH_INDEX_SHIFT;

	ret = amdgpu_read_mm_regs(fd, mmCC_RB_BACKEND_DISABLE, 1, instance, 0,
				  &gpu_info->backend_disable[0]);
	if (ret)
		return ret;
	/* extract bitfield CC_RB_BACKEND_DISABLE.BACKEND_DISABLE */
	gpu_info->backend_disable[0] =
		(gpu_info->backend_disable[0] >> 16) & 0xff;

	ret = amdgpu_read_mm_regs(fd, mmGB_TILE_MODE0, 32, 0xffffffff, 0,
				  gpu_info->gb_tile_mode);
	if (ret)
		return ret;

	ret = amdgpu_read_mm_regs(fd, mmGB_MACROTILE_MODE0, 16, 0xffffffff, 0,
				  gpu_info->gb_macro_tile_mode);
	if (ret)
		return ret;

	ret = amdgpu_read_mm_regs(fd, mmGB_ADDR_CONFIG, 1, 0xffffffff, 0,
				  &gpu_info->gb_addr_cfg);
	if (ret)
		return ret;

	ret = amdgpu_read_mm_regs(fd, mmMC_ARB_RAMCFG, 1, 0xffffffff, 0,
				  &gpu_info->mc_arb_ramcfg);
	if (ret)
		return ret;

	return 0;
}

static void *ADDR_API alloc_sys_mem(const ADDR_ALLOCSYSMEM_INPUT *in)
{
	return malloc(in->sizeInBytes);
}

static ADDR_E_RETURNCODE ADDR_API free_sys_mem(const ADDR_FREESYSMEM_INPUT *in)
{
	free(in->pVirtAddr);
	return ADDR_OK;
}

static int amdgpu_addrlib_compute(void *addrlib, uint32_t width,
				  uint32_t height, uint32_t format,
				  uint32_t usage, uint32_t *tiling_flags,
				  ADDR_COMPUTE_SURFACE_INFO_OUTPUT *addr_out)
{
	ADDR_COMPUTE_SURFACE_INFO_INPUT addr_surf_info_in = {0};
	ADDR_TILEINFO addr_tile_info = {0};
	ADDR_TILEINFO addr_tile_info_out = {0};

	addr_surf_info_in.size = sizeof(ADDR_COMPUTE_SURFACE_INFO_INPUT);

	/* Set the requested tiling mode. */
	addr_surf_info_in.tileMode = ADDR_TM_2D_TILED_THIN1;
	if (usage & (DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR))
		addr_surf_info_in.tileMode = ADDR_TM_LINEAR_ALIGNED;
	if (width <= 16 || height <= 16)
		addr_surf_info_in.tileMode = ADDR_TM_1D_TILED_THIN1;

	/* Bits per pixel should be calculated from format*/
	addr_surf_info_in.bpp = drv_bpp_from_format(format, 0);
	addr_surf_info_in.numSamples = 1;
	addr_surf_info_in.width = width;
	addr_surf_info_in.height = height;
	addr_surf_info_in.numSlices = 1;
	addr_surf_info_in.pTileInfo = &addr_tile_info;
	addr_surf_info_in.tileIndex = -1;

	/* This disables incorrect calculations (hacks) in addrlib. */
	addr_surf_info_in.flags.noStencil = 1;

	/* Set the micro tile type. */
	if (usage & DRV_BO_USE_SCANOUT)
		addr_surf_info_in.tileType = ADDR_DISPLAYABLE;
	else
		addr_surf_info_in.tileType = ADDR_NON_DISPLAYABLE;

	addr_out->size = sizeof(ADDR_COMPUTE_SURFACE_INFO_OUTPUT);
	addr_out->pTileInfo = &addr_tile_info_out;

	if (AddrComputeSurfaceInfo(addrlib, &addr_surf_info_in,
				   addr_out) != ADDR_OK)
		return -EINVAL;

	ADDR_CONVERT_TILEINFOTOHW_INPUT s_in = {0};
	ADDR_CONVERT_TILEINFOTOHW_OUTPUT s_out = {0};
	ADDR_TILEINFO s_tile_hw_info_out = {0};

	s_in.size = sizeof(ADDR_CONVERT_TILEINFOTOHW_INPUT);
	/* Convert from real value to HW value */
	s_in.reverse = 0;
	s_in.pTileInfo = &addr_tile_info_out;
	s_in.tileIndex = -1;

	s_out.size = sizeof(ADDR_CONVERT_TILEINFOTOHW_OUTPUT);
	s_out.pTileInfo = &s_tile_hw_info_out;

	if (AddrConvertTileInfoToHW(addrlib, &s_in, &s_out) != ADDR_OK)
		return -EINVAL;

	if (addr_out->tileMode >= ADDR_TM_2D_TILED_THIN1)
		/* 2D_TILED_THIN1 */
		*tiling_flags |= AMDGPU_TILING_SET(ARRAY_MODE, 4);
	else if (addr_out->tileMode >= ADDR_TM_1D_TILED_THIN1)
		/* 1D_TILED_THIN1 */
		*tiling_flags |= AMDGPU_TILING_SET(ARRAY_MODE, 2);
	else
		/* LINEAR_ALIGNED */
		*tiling_flags |= AMDGPU_TILING_SET(ARRAY_MODE, 1);

	*tiling_flags |= AMDGPU_TILING_SET(BANK_WIDTH,
			drv_log_base2(addr_tile_info_out.bankWidth));
	*tiling_flags |= AMDGPU_TILING_SET(BANK_HEIGHT,
			drv_log_base2(addr_tile_info_out.bankHeight));
	*tiling_flags |= AMDGPU_TILING_SET(TILE_SPLIT,
			s_tile_hw_info_out.tileSplitBytes);
	*tiling_flags |= AMDGPU_TILING_SET(MACRO_TILE_ASPECT,
			drv_log_base2(addr_tile_info_out.macroAspectRatio));
	*tiling_flags |= AMDGPU_TILING_SET(PIPE_CONFIG,
				s_tile_hw_info_out.pipeConfig);
	*tiling_flags |= AMDGPU_TILING_SET(NUM_BANKS, s_tile_hw_info_out.banks);

	return 0;
}

static void *amdgpu_addrlib_init(int fd)
{
	int ret;
	ADDR_CREATE_INPUT addr_create_input = {0};
	ADDR_CREATE_OUTPUT addr_create_output = {0};
	ADDR_REGISTER_VALUE reg_value = {0};
	ADDR_CREATE_FLAGS create_flags = { {0} };
	ADDR_E_RETURNCODE addr_ret;

	addr_create_input.size = sizeof(ADDR_CREATE_INPUT);
	addr_create_output.size = sizeof(ADDR_CREATE_OUTPUT);

	struct amdgpu_gpu_info gpu_info = {0};

	ret = amdgpu_query_gpu(fd, &gpu_info);

	if (ret) {
		fprintf(stderr, "[%s]failed with error =%d\n", __func__, ret);
		return NULL;
	}

	reg_value.noOfBanks = gpu_info.mc_arb_ramcfg & 0x3;
	reg_value.gbAddrConfig = gpu_info.gb_addr_cfg;
	reg_value.noOfRanks = (gpu_info.mc_arb_ramcfg & 0x4) >> 2;

	reg_value.backendDisables = gpu_info.backend_disable[0];
	reg_value.pTileConfig = gpu_info.gb_tile_mode;
	reg_value.noOfEntries = sizeof(gpu_info.gb_tile_mode)
			/ sizeof(gpu_info.gb_tile_mode[0]);
	reg_value.pMacroTileConfig = gpu_info.gb_macro_tile_mode;
	reg_value.noOfMacroEntries = sizeof(gpu_info.gb_macro_tile_mode)
			/ sizeof(gpu_info.gb_macro_tile_mode[0]);
	create_flags.value = 0;
	create_flags.useTileIndex = 1;

	addr_create_input.chipEngine = CIASICIDGFXENGINE_SOUTHERNISLAND;

	addr_create_input.chipFamily = FAMILY_CZ;
	addr_create_input.createFlags = create_flags;
	addr_create_input.callbacks.allocSysMem = alloc_sys_mem;
	addr_create_input.callbacks.freeSysMem = free_sys_mem;
	addr_create_input.callbacks.debugPrint = 0;
	addr_create_input.regValue = reg_value;

	addr_ret = AddrCreate(&addr_create_input, &addr_create_output);

	if (addr_ret != ADDR_OK) {
		fprintf(stderr, "[%s]failed error =%d\n", __func__, addr_ret);
		return NULL;
	}

	return addr_create_output.hLib;
}

static int amdgpu_init(struct driver *drv)
{
	void *addrlib;

	addrlib = amdgpu_addrlib_init(drv_get_fd(drv));
	if (!addrlib)
		return -1;

	drv->priv = addrlib;

	return 0;
}

static void amdgpu_close(struct driver *drv)
{
	AddrDestroy(drv->priv);
	drv->priv = NULL;
}

static int amdgpu_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			    uint32_t format, uint32_t usage)
{
	void *addrlib =	bo->drv->priv;
	union drm_amdgpu_gem_create gem_create;
	struct amdgpu_bo_metadata metadata = {0};
	ADDR_COMPUTE_SURFACE_INFO_OUTPUT addr_out = {0};
	uint32_t tiling_flags = 0;
	int ret;

	if (amdgpu_addrlib_compute(addrlib, width,
				   height, format, usage,
				   &tiling_flags,
				   &addr_out) < 0)
		return -EINVAL;

	bo->tiling = tiling_flags;
	bo->offsets[0] = 0;
	bo->sizes[0] = addr_out.surfSize;
	bo->strides[0] = addr_out.pixelPitch
		* DIV_ROUND_UP(addr_out.pixelBits, 8);

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.in.bo_size = bo->sizes[0];
	gem_create.in.alignment = addr_out.baseAlign;
	/* Set the placement. */
	gem_create.in.domains = AMDGPU_GEM_DOMAIN_VRAM;
	gem_create.in.domain_flags = usage;

	/* Allocate the buffer with the preferred heap. */
	ret = drmCommandWriteRead(drv_get_fd(bo->drv), DRM_AMDGPU_GEM_CREATE,
				  &gem_create, sizeof(gem_create));

	if (ret < 0)
		return ret;

	bo->handles[0].u32 = gem_create.out.handle;

	metadata.tiling_info = tiling_flags;

	ret = amdgpu_set_metadata(drv_get_fd(bo->drv),
				  bo->handles[0].u32, &metadata);

	return ret;
}

const struct backend backend_amdgpu = {
	.name = "amdgpu",
	.init = amdgpu_init,
	.close = amdgpu_close,
	.bo_create = amdgpu_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.format_list = {
		/* Linear support */
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_LINEAR},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
		/* Blocklinear support */
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_XBGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING},
	}
};

#endif

