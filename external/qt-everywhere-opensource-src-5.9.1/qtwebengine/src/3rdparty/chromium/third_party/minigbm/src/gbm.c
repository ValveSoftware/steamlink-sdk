/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>

#include "drv.h"
#include "gbm_priv.h"
#include "gbm_helpers.h"
#include "util.h"

PUBLIC int
gbm_device_get_fd(struct gbm_device *gbm)
{

	return drv_get_fd(gbm->drv);
}

PUBLIC const char *
gbm_device_get_backend_name(struct gbm_device *gbm)
{
	return drv_get_name(gbm->drv);
}

PUBLIC int
gbm_device_is_format_supported(struct gbm_device *gbm,
			       uint32_t format, uint32_t usage)
{
	uint32_t drv_format;
	uint64_t drv_usage;

	if (usage & GBM_BO_USE_CURSOR &&
		usage & GBM_BO_USE_RENDERING)
		return 0;

	drv_format = gbm_convert_format(format);
	drv_usage = gbm_convert_flags(usage);

	return drv_is_format_supported(gbm->drv, drv_format, drv_usage);
}

PUBLIC struct gbm_device *gbm_create_device(int fd)
{
	struct gbm_device *gbm;

	gbm = (struct gbm_device*) malloc(sizeof(*gbm));

	if (!gbm)
		return NULL;

	gbm->drv = drv_create(fd);
	if (!gbm->drv) {
		free(gbm);
		return NULL;
	}

	return gbm;
}

PUBLIC void gbm_device_destroy(struct gbm_device *gbm)
{
	drv_destroy(gbm->drv);
	free(gbm);
}

PUBLIC struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
					      uint32_t width, uint32_t height,
					      uint32_t format, uint32_t flags)
{
	struct gbm_surface *surface =
		(struct gbm_surface*) malloc(sizeof(*surface));

	if (!surface)
		return NULL;

	return surface;
}

PUBLIC void gbm_surface_destroy(struct gbm_surface *surface)
{
	free(surface);
}

PUBLIC struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface)
{
	return NULL;
}

PUBLIC void gbm_surface_release_buffer(struct gbm_surface *surface,
				       struct gbm_bo *bo)
{
}

static struct gbm_bo *gbm_bo_new(struct gbm_device *gbm, uint32_t format)
{
	struct gbm_bo *bo;

	bo = (struct gbm_bo*) calloc(1, sizeof(*bo));
	if (!bo)
		return NULL;

	bo->gbm = gbm;
	bo->gbm_format = format;

	return bo;
}

PUBLIC struct gbm_bo *gbm_bo_create(struct gbm_device *gbm, uint32_t width,
				    uint32_t height, uint32_t format,
				    uint32_t flags)
{
	struct gbm_bo *bo;

	if (!gbm_device_is_format_supported(gbm, format, flags))
		return NULL;

	bo = gbm_bo_new(gbm, format);

	if (!bo)
		return NULL;

	bo->bo = drv_bo_create(gbm->drv, width, height,
			       gbm_convert_format(format),
			       gbm_convert_flags(flags));

	if (!bo->bo) {
		free(bo);
		return NULL;
	}

	return bo;
}

PUBLIC void gbm_bo_destroy(struct gbm_bo *bo)
{
	if (bo->destroy_user_data) {
		bo->destroy_user_data(bo, bo->user_data);
		bo->destroy_user_data = NULL;
		bo->user_data = NULL;
	}

	drv_bo_destroy(bo->bo);
	free(bo);
}

PUBLIC struct gbm_bo *
gbm_bo_import(struct gbm_device *gbm, uint32_t type,
              void *buffer, uint32_t usage)
{
	struct gbm_bo *bo;
	struct drv_import_fd_data drv_data;
	struct gbm_import_fd_data *fd_data = buffer;
	struct gbm_import_fd_planar_data *fd_planar_data = buffer;
	uint32_t gbm_format;
	int i;

	memset(&drv_data, 0, sizeof(drv_data));

	switch (type) {
	case GBM_BO_IMPORT_FD:
		gbm_format = fd_data->format;
		drv_data.width = fd_data->width;
		drv_data.height = fd_data->height;
		drv_data.format = gbm_convert_format(fd_data->format);
		drv_data.fds[0] = fd_data->fd;
		drv_data.strides[0] = fd_data->stride;
		drv_data.sizes[0] = fd_data->height * fd_data->stride;
		break;
	case GBM_BO_IMPORT_FD_PLANAR:
		gbm_format = fd_planar_data->format;
		drv_data.width = fd_planar_data->width;
		drv_data.height = fd_planar_data->height;
		drv_data.format = gbm_convert_format(fd_planar_data->format);
		for (i = 0; i < GBM_MAX_PLANES; i++) {
			drv_data.fds[i] = fd_planar_data->fds[i];
			drv_data.offsets[i] = fd_planar_data->offsets[i];
			drv_data.strides[i] = fd_planar_data->strides[i];
			drv_data.sizes[i] = fd_planar_data->height *
					    fd_planar_data->strides[i];
			drv_data.format_modifiers[i] =
				fd_planar_data->format_modifiers[i];
		}
		break;
	default:
		return NULL;
	}

	if (!gbm_device_is_format_supported(gbm, gbm_format, usage))
		return NULL;

	bo = gbm_bo_new(gbm, gbm_format);

	if (!bo)
		return NULL;

	bo->bo = drv_bo_import(gbm->drv, &drv_data);

	if (!bo->bo) {
		free(bo);
		return NULL;
	}

	return bo;
}

PUBLIC void *
gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y, uint32_t width,
	   uint32_t height, uint32_t flags, uint32_t *stride, void **map_data,
	   size_t plane)
{
	if (!bo || width == 0 || height == 0 || !stride || !map_data)
		return NULL;

	*stride = gbm_bo_get_plane_stride(bo, plane);
	return drv_bo_map(bo->bo, x, y, width, height, 0, map_data, plane);
}

PUBLIC void
gbm_bo_unmap(struct gbm_bo *bo, void *map_data)
{
	assert(bo);
	drv_bo_unmap(bo->bo, map_data);
}

PUBLIC uint32_t
gbm_bo_get_width(struct gbm_bo *bo)
{
	return drv_bo_get_width(bo->bo);
}

PUBLIC uint32_t
gbm_bo_get_height(struct gbm_bo *bo)
{
	return drv_bo_get_height(bo->bo);
}

PUBLIC uint32_t
gbm_bo_get_stride(struct gbm_bo *bo)
{
	return gbm_bo_get_plane_stride(bo, 0);
}

PUBLIC uint32_t
gbm_bo_get_stride_or_tiling(struct gbm_bo *bo)
{
	return drv_bo_get_stride_or_tiling(bo->bo);
}

PUBLIC uint32_t
gbm_bo_get_format(struct gbm_bo *bo)
{
	return bo->gbm_format;
}

PUBLIC uint64_t
gbm_bo_get_format_modifier(struct gbm_bo *bo)
{
	return gbm_bo_get_plane_format_modifier(bo, 0);
}

PUBLIC struct gbm_device *
gbm_bo_get_device(struct gbm_bo *bo)
{
	return bo->gbm;
}

PUBLIC union gbm_bo_handle
gbm_bo_get_handle(struct gbm_bo *bo)
{
	return gbm_bo_get_plane_handle(bo, 0);
}

PUBLIC int
gbm_bo_get_fd(struct gbm_bo *bo)
{
	return gbm_bo_get_plane_fd(bo, 0);
}

PUBLIC size_t
gbm_bo_get_num_planes(struct gbm_bo *bo)
{
	return drv_bo_get_num_planes(bo->bo);
}

PUBLIC union gbm_bo_handle
gbm_bo_get_plane_handle(struct gbm_bo *bo, size_t plane)
{
	return (union gbm_bo_handle) drv_bo_get_plane_handle(bo->bo, plane).u64;
}

PUBLIC int
gbm_bo_get_plane_fd(struct gbm_bo *bo, size_t plane)
{
	return drv_bo_get_plane_fd(bo->bo, plane);
}

PUBLIC uint32_t
gbm_bo_get_plane_offset(struct gbm_bo *bo, size_t plane)
{
	return drv_bo_get_plane_offset(bo->bo, plane);
}

PUBLIC uint32_t
gbm_bo_get_plane_size(struct gbm_bo *bo, size_t plane)
{
	return drv_bo_get_plane_size(bo->bo, plane);
}

PUBLIC uint32_t
gbm_bo_get_plane_stride(struct gbm_bo *bo, size_t plane)
{
	return drv_bo_get_plane_stride(bo->bo, plane);
}

PUBLIC uint64_t
gbm_bo_get_plane_format_modifier(struct gbm_bo *bo, size_t plane)
{
	return drv_bo_get_plane_format_modifier(bo->bo, plane);
}

PUBLIC void
gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
		     void (*destroy_user_data)(struct gbm_bo *, void *))
{
	bo->user_data = data;
	bo->destroy_user_data = destroy_user_data;
}

PUBLIC void *
gbm_bo_get_user_data(struct gbm_bo *bo)
{
	return bo->user_data;
}
