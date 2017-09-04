/*
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DRV_H_
#define DRV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DRV_MAX_PLANES 4

/* Vendor ids and mod_code fourcc function must match gbm.h */
#define DRV_FORMAT_MOD_NONE           0
#define DRV_FORMAT_MOD_VENDOR_INTEL   0x01
#define DRV_FORMAT_MOD_VENDOR_AMD     0x02
#define DRV_FORMAT_MOD_VENDOR_NV      0x03
#define DRV_FORMAT_MOD_VENDOR_SAMSUNG 0x04
#define DRV_FORMAT_MOD_VENDOR_QCOM    0x05

#define drv_fourcc_mod_code(vendor, val) \
	((((__u64)DRV_FORMAT_MOD_VENDOR_## vendor) << 56) | (val & 0x00ffffffffffffffULL))

/* Use flags */
#define DRV_BO_USE_NONE				 0
#define DRV_BO_USE_SCANOUT		(1ull << 0)
#define DRV_BO_USE_CURSOR		(1ull << 1)
#define DRV_BO_USE_CURSOR_64X64		DRV_BO_USE_CURSOR
#define DRV_BO_USE_RENDERING		(1ull << 2)
#define DRV_BO_USE_LINEAR		(1ull << 3)
#define DRV_BO_USE_SW_READ_NEVER	(1ull << 4)
#define DRV_BO_USE_SW_READ_RARELY	(1ull << 5)
#define DRV_BO_USE_SW_READ_OFTEN	(1ull << 6)
#define DRV_BO_USE_SW_WRITE_NEVER	(1ull << 7)
#define DRV_BO_USE_SW_WRITE_RARELY	(1ull << 8)
#define DRV_BO_USE_SW_WRITE_OFTEN	(1ull << 9)
#define DRV_BO_USE_EXTERNAL_DISP	(1ull << 10)
#define DRV_BO_USE_PROTECTED		(1ull << 11)
#define DRV_BO_USE_HW_VIDEO_ENCODER	(1ull << 12)
#define DRV_BO_USE_HW_CAMERA_WRITE	(1ull << 13)
#define DRV_BO_USE_HW_CAMERA_READ	(1ull << 14)
#define DRV_BO_USE_HW_CAMERA_ZSL	(1ull << 15)
#define DRV_BO_USE_RENDERSCRIPT		(1ull << 16)

typedef enum {
	DRV_FORMAT_NONE,
	DRV_FORMAT_C8,
	DRV_FORMAT_R8,
	DRV_FORMAT_RG88,
	DRV_FORMAT_GR88,
	DRV_FORMAT_RGB332,
	DRV_FORMAT_BGR233,
	DRV_FORMAT_XRGB4444,
	DRV_FORMAT_XBGR4444,
	DRV_FORMAT_RGBX4444,
	DRV_FORMAT_BGRX4444,
	DRV_FORMAT_ARGB4444,
	DRV_FORMAT_ABGR4444,
	DRV_FORMAT_RGBA4444,
	DRV_FORMAT_BGRA4444,
	DRV_FORMAT_XRGB1555,
	DRV_FORMAT_XBGR1555,
	DRV_FORMAT_RGBX5551,
	DRV_FORMAT_BGRX5551,
	DRV_FORMAT_ARGB1555,
	DRV_FORMAT_ABGR1555,
	DRV_FORMAT_RGBA5551,
	DRV_FORMAT_BGRA5551,
	DRV_FORMAT_RGB565,
	DRV_FORMAT_BGR565,
	DRV_FORMAT_RGB888,
	DRV_FORMAT_BGR888,
	DRV_FORMAT_XRGB8888,
	DRV_FORMAT_XBGR8888,
	DRV_FORMAT_RGBX8888,
	DRV_FORMAT_BGRX8888,
	DRV_FORMAT_ARGB8888,
	DRV_FORMAT_ABGR8888,
	DRV_FORMAT_RGBA8888,
	DRV_FORMAT_BGRA8888,
	DRV_FORMAT_XRGB2101010,
	DRV_FORMAT_XBGR2101010,
	DRV_FORMAT_RGBX1010102,
	DRV_FORMAT_BGRX1010102,
	DRV_FORMAT_ARGB2101010,
	DRV_FORMAT_ABGR2101010,
	DRV_FORMAT_RGBA1010102,
	DRV_FORMAT_BGRA1010102,
	DRV_FORMAT_YUYV,
	DRV_FORMAT_YVYU,
	DRV_FORMAT_UYVY,
	DRV_FORMAT_VYUY,
	DRV_FORMAT_AYUV,
	DRV_FORMAT_NV12,
	DRV_FORMAT_NV21,
	DRV_FORMAT_NV16,
	DRV_FORMAT_NV61,
	DRV_FORMAT_YUV410,
	DRV_FORMAT_YVU410,
	DRV_FORMAT_YUV411,
	DRV_FORMAT_YVU411,
	DRV_FORMAT_YUV420,
	DRV_FORMAT_YVU420,
	DRV_FORMAT_YUV422,
	DRV_FORMAT_YVU422,
	DRV_FORMAT_YUV444,
	DRV_FORMAT_YVU444,
	DRV_FORMAT_FLEX_IMPLEMENTATION_DEFINED,
	DRV_FORMAT_FLEX_YCbCr_420_888,
} drv_format_t;

struct driver;
struct bo;

union bo_handle {
	void *ptr;
	int32_t s32;
	uint32_t u32;
	int64_t s64;
	uint64_t u64;
};

struct drv_import_fd_data {
	int fds[DRV_MAX_PLANES];
	uint32_t strides[DRV_MAX_PLANES];
	uint32_t offsets[DRV_MAX_PLANES];
	uint32_t sizes[DRV_MAX_PLANES];
	uint64_t format_modifiers[DRV_MAX_PLANES];
	uint32_t width;
	uint32_t height;
	drv_format_t format;
};

struct driver *
drv_create(int fd);

void
drv_destroy(struct driver *drv);

int
drv_get_fd(struct driver *drv);

const char *
drv_get_name(struct driver *drv);

int
drv_is_format_supported(struct driver *drv, drv_format_t format,
			uint64_t usage);

struct bo *
drv_bo_new(struct driver *drv, uint32_t width, uint32_t height,
	   drv_format_t format);

struct bo *
drv_bo_create(struct driver *drv, uint32_t width, uint32_t height,
	      drv_format_t format, uint64_t flags);

void
drv_bo_destroy(struct bo *bo);

struct bo *
drv_bo_import(struct driver *drv, struct drv_import_fd_data *data);

void *
drv_bo_map(struct bo *bo, uint32_t x, uint32_t y, uint32_t width,
	   uint32_t height, uint32_t flags, void **map_data, size_t plane);

int
drv_bo_unmap(struct bo *bo, void *map_data);

uint32_t
drv_bo_get_width(struct bo *bo);

uint32_t
drv_bo_get_height(struct bo *bo);

uint32_t
drv_bo_get_stride_or_tiling(struct bo *bo);

size_t
drv_bo_get_num_planes(struct bo *bo);

union bo_handle
drv_bo_get_plane_handle(struct bo *bo, size_t plane);

int
drv_bo_get_plane_fd(struct bo *bo, size_t plane);

uint32_t
drv_bo_get_plane_offset(struct bo *bo, size_t plane);

uint32_t
drv_bo_get_plane_size(struct bo *bo, size_t plane);

uint32_t
drv_bo_get_plane_stride(struct bo *bo, size_t plane);

uint64_t
drv_bo_get_plane_format_modifier(struct bo *bo, size_t plane);

drv_format_t
drv_bo_get_format(struct bo *bo);

drv_format_t
drv_resolve_format(struct driver *drv, drv_format_t format);

int
drv_stride_from_format(uint32_t format, uint32_t width, size_t plane);

uint32_t
drv_num_buffers_per_bo(struct bo *bo);

#ifdef __cplusplus
}
#endif

#endif
