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
#include <sys/mman.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

size_t drv_num_planes_from_format(uint32_t format)
{
	switch (format) {
	case DRV_FORMAT_C8:
	case DRV_FORMAT_R8:
	case DRV_FORMAT_RG88:
	case DRV_FORMAT_GR88:
	case DRV_FORMAT_RGB332:
	case DRV_FORMAT_BGR233:
	case DRV_FORMAT_XRGB4444:
	case DRV_FORMAT_XBGR4444:
	case DRV_FORMAT_RGBX4444:
	case DRV_FORMAT_BGRX4444:
	case DRV_FORMAT_ARGB4444:
	case DRV_FORMAT_ABGR4444:
	case DRV_FORMAT_RGBA4444:
	case DRV_FORMAT_BGRA4444:
	case DRV_FORMAT_XRGB1555:
	case DRV_FORMAT_XBGR1555:
	case DRV_FORMAT_RGBX5551:
	case DRV_FORMAT_BGRX5551:
	case DRV_FORMAT_ARGB1555:
	case DRV_FORMAT_ABGR1555:
	case DRV_FORMAT_RGBA5551:
	case DRV_FORMAT_BGRA5551:
	case DRV_FORMAT_RGB565:
	case DRV_FORMAT_BGR565:
	case DRV_FORMAT_YUYV:
	case DRV_FORMAT_YVYU:
	case DRV_FORMAT_UYVY:
	case DRV_FORMAT_VYUY:
	case DRV_FORMAT_RGB888:
	case DRV_FORMAT_BGR888:
	case DRV_FORMAT_XRGB8888:
	case DRV_FORMAT_XBGR8888:
	case DRV_FORMAT_RGBX8888:
	case DRV_FORMAT_BGRX8888:
	case DRV_FORMAT_ARGB8888:
	case DRV_FORMAT_ABGR8888:
	case DRV_FORMAT_RGBA8888:
	case DRV_FORMAT_BGRA8888:
	case DRV_FORMAT_XRGB2101010:
	case DRV_FORMAT_XBGR2101010:
	case DRV_FORMAT_RGBX1010102:
	case DRV_FORMAT_BGRX1010102:
	case DRV_FORMAT_ARGB2101010:
	case DRV_FORMAT_ABGR2101010:
	case DRV_FORMAT_RGBA1010102:
	case DRV_FORMAT_BGRA1010102:
	case DRV_FORMAT_AYUV:
		return 1;
	case DRV_FORMAT_NV12:
		return 2;
	case DRV_FORMAT_YVU420:
		return 3;
	}

	fprintf(stderr, "drv: UNKNOWN FORMAT %d\n", format);
	return 0;
}

int drv_bpp_from_format(uint32_t format, size_t plane)
{
	assert(plane < drv_num_planes_from_format(format));

	switch (format) {
	case DRV_FORMAT_C8:
	case DRV_FORMAT_R8:
	case DRV_FORMAT_RGB332:
	case DRV_FORMAT_BGR233:
		return 8;

	case DRV_FORMAT_NV12:
		return (plane == 0) ? 8 : 4;
	case DRV_FORMAT_YVU420:
		return (plane == 0) ? 8 : 2;

	case DRV_FORMAT_RG88:
	case DRV_FORMAT_GR88:
	case DRV_FORMAT_XRGB4444:
	case DRV_FORMAT_XBGR4444:
	case DRV_FORMAT_RGBX4444:
	case DRV_FORMAT_BGRX4444:
	case DRV_FORMAT_ARGB4444:
	case DRV_FORMAT_ABGR4444:
	case DRV_FORMAT_RGBA4444:
	case DRV_FORMAT_BGRA4444:
	case DRV_FORMAT_XRGB1555:
	case DRV_FORMAT_XBGR1555:
	case DRV_FORMAT_RGBX5551:
	case DRV_FORMAT_BGRX5551:
	case DRV_FORMAT_ARGB1555:
	case DRV_FORMAT_ABGR1555:
	case DRV_FORMAT_RGBA5551:
	case DRV_FORMAT_BGRA5551:
	case DRV_FORMAT_RGB565:
	case DRV_FORMAT_BGR565:
	case DRV_FORMAT_YUYV:
	case DRV_FORMAT_YVYU:
	case DRV_FORMAT_UYVY:
	case DRV_FORMAT_VYUY:
		return 16;

	case DRV_FORMAT_RGB888:
	case DRV_FORMAT_BGR888:
		return 24;

	case DRV_FORMAT_XRGB8888:
	case DRV_FORMAT_XBGR8888:
	case DRV_FORMAT_RGBX8888:
	case DRV_FORMAT_BGRX8888:
	case DRV_FORMAT_ARGB8888:
	case DRV_FORMAT_ABGR8888:
	case DRV_FORMAT_RGBA8888:
	case DRV_FORMAT_BGRA8888:
	case DRV_FORMAT_XRGB2101010:
	case DRV_FORMAT_XBGR2101010:
	case DRV_FORMAT_RGBX1010102:
	case DRV_FORMAT_BGRX1010102:
	case DRV_FORMAT_ARGB2101010:
	case DRV_FORMAT_ABGR2101010:
	case DRV_FORMAT_RGBA1010102:
	case DRV_FORMAT_BGRA1010102:
	case DRV_FORMAT_AYUV:
		return 32;
	}

	fprintf(stderr, "drv: UNKNOWN FORMAT %d\n", format);
	return 0;
}

/*
 * This function fills in the buffer object given driver aligned dimensions
 * and a format.  This function assumes there is just one kernel buffer per
 * buffer object.
 */
int drv_bo_from_format(struct bo *bo, uint32_t width, uint32_t height,
		       drv_format_t format)
{

	switch (format) {
	case DRV_FORMAT_YVU420:
		bo->strides[0] = drv_stride_from_format(format, width, 0);
		bo->strides[1] = drv_stride_from_format(format, width, 1);
		bo->strides[2] = drv_stride_from_format(format, width, 2);
		bo->sizes[0] = height * bo->strides[0];
		bo->sizes[1] = bo->sizes[2] = (height / 2) * bo->strides[1];
		bo->offsets[0] = 0;
		bo->offsets[1] = bo->sizes[0];
		bo->offsets[2] = bo->offsets[1] + bo->sizes[1];
		break;
	case DRV_FORMAT_NV12:
		bo->strides[0] = drv_stride_from_format(format, width, 0);
		bo->strides[1] = drv_stride_from_format(format, width, 1);
		bo->sizes[0] = height * bo->strides[0];
		bo->sizes[1] = height * bo->strides[1] / 2;
		bo->offsets[0] = 0;
		bo->offsets[1] = height * bo->strides[0];
		break;
	default:
		bo->strides[0] = drv_stride_from_format(format, width, 0);
		bo->sizes[0] = height * bo->strides[0];
		bo->offsets[0] = 0;
	}

	bo->total_size = bo->offsets[bo->num_planes - 1] +
			 bo->sizes[bo->num_planes - 1];

	return 0;
}

int drv_dumb_bo_create(struct bo *bo, uint32_t width, uint32_t height,
		       uint32_t format, uint32_t flags)
{
	struct drm_mode_create_dumb create_dumb;
	int ret;

	/* Only single-plane formats are supported */
	assert(drv_num_planes_from_format(format) == 1);

	memset(&create_dumb, 0, sizeof(create_dumb));
	create_dumb.height = height;
	create_dumb.width = width;
	create_dumb.bpp = drv_bpp_from_format(format, 0);
	create_dumb.flags = 0;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_CREATE_DUMB failed\n");
		return ret;
	}

	bo->handles[0].u32 = create_dumb.handle;
	bo->offsets[0] = 0;
	bo->total_size = bo->sizes[0] = create_dumb.size;
	bo->strides[0] = create_dumb.pitch;

	return 0;
}

int drv_dumb_bo_destroy(struct bo *bo)
{
	struct drm_mode_destroy_dumb destroy_dumb;
	int ret;

	memset(&destroy_dumb, 0, sizeof(destroy_dumb));
	destroy_dumb.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_DESTROY_DUMB failed "
				"(handle=%x)\n", bo->handles[0].u32);
		return ret;
	}

	return 0;
}

int drv_gem_bo_destroy(struct bo *bo)
{
	struct drm_gem_close gem_close;
	int ret, error = 0;
	size_t plane, i;

	for (plane = 0; plane < bo->num_planes; plane++) {
		for (i = 0; i < plane; i++)
			if (bo->handles[i].u32 == bo->handles[plane].u32)
				break;
		/* Make sure close hasn't already been called on this handle */
		if (i != plane)
			continue;

		memset(&gem_close, 0, sizeof(gem_close));
		gem_close.handle = bo->handles[plane].u32;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_GEM_CLOSE failed "
					"(handle=%x) error %d\n",
					bo->handles[plane].u32, ret);
			error = ret;
		}
	}

	return error;
}

void *drv_dumb_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	size_t i;
	struct drm_mode_map_dumb map_dumb;

	memset(&map_dumb, 0, sizeof(map_dumb));
	map_dumb.handle = bo->handles[plane].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_MAP_DUMB failed \n");
		return MAP_FAILED;
	}

	for (i = 0; i < bo->num_planes; i++)
		if (bo->handles[i].u32 == bo->handles[plane].u32)
			data->length += bo->sizes[i];

	return mmap(0, data->length, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, map_dumb.offset);
}

uintptr_t drv_get_reference_count(struct driver *drv, struct bo *bo,
				  size_t plane)
{
	void *count;
	uintptr_t num = 0;

	if (!drmHashLookup(drv->buffer_table, bo->handles[plane].u32, &count))
		num = (uintptr_t) (count);

	return num;
}

void drv_increment_reference_count(struct driver *drv, struct bo *bo,
				   size_t plane)
{
	uintptr_t num = drv_get_reference_count(drv, bo, plane);

	/* If a value isn't in the table, drmHashDelete is a no-op */
	drmHashDelete(drv->buffer_table, bo->handles[plane].u32);
	drmHashInsert(drv->buffer_table, bo->handles[plane].u32,
		      (void *) (num + 1));
}

void drv_decrement_reference_count(struct driver *drv, struct bo *bo,
				   size_t plane)
{
	uintptr_t num = drv_get_reference_count(drv, bo, plane);

	drmHashDelete(drv->buffer_table, bo->handles[plane].u32);

	if (num > 0)
		drmHashInsert(drv->buffer_table, bo->handles[plane].u32,
			      (void *) (num - 1));
}

uint32_t drv_log_base2(uint32_t value)
{
	int ret = 0;

	while (value >>= 1)
		++ret;

	return ret;
}
