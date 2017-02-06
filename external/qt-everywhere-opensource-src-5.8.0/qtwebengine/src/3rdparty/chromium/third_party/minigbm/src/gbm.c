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

#include "gbm_priv.h"
#include "helpers.h"
#include "util.h"

extern struct gbm_driver gbm_driver_cirrus;
extern struct gbm_driver gbm_driver_evdi;
#ifdef GBM_EXYNOS
extern struct gbm_driver gbm_driver_exynos;
#endif
extern struct gbm_driver gbm_driver_gma500;
#ifdef GBM_I915
extern struct gbm_driver gbm_driver_i915;
#endif
#ifdef GBM_MARVELL
extern struct gbm_driver gbm_driver_marvell;
#endif
#ifdef GBM_MEDIATEK
extern struct gbm_driver gbm_driver_mediatek;
#endif
#ifdef GBM_ROCKCHIP
extern struct gbm_driver gbm_driver_rockchip;
#endif
#ifdef GBM_TEGRA
extern struct gbm_driver gbm_driver_tegra;
#endif
extern struct gbm_driver gbm_driver_udl;
extern struct gbm_driver gbm_driver_virtio_gpu;

static struct gbm_driver *gbm_get_driver(int fd)
{
	drmVersionPtr drm_version;
	unsigned int i;

	drm_version = drmGetVersion(fd);

	if (!drm_version)
		return NULL;

	struct gbm_driver *driver_list[] = {
		&gbm_driver_cirrus,
		&gbm_driver_evdi,
#ifdef GBM_EXYNOS
		&gbm_driver_exynos,
#endif
		&gbm_driver_gma500,
#ifdef GBM_I915
		&gbm_driver_i915,
#endif
#ifdef GBM_MARVELL
		&gbm_driver_marvell,
#endif
#ifdef GBM_MEDIATEK
		&gbm_driver_mediatek,
#endif
#ifdef GBM_ROCKCHIP
		&gbm_driver_rockchip,
#endif
#ifdef GBM_TEGRA
		&gbm_driver_tegra,
#endif
		&gbm_driver_udl,
		&gbm_driver_virtio_gpu,
	};

	for(i = 0; i < ARRAY_SIZE(driver_list); i++)
		if (!strcmp(drm_version->name, driver_list[i]->name))
		{
			drmFreeVersion(drm_version);
			return driver_list[i];
		}

	drmFreeVersion(drm_version);
	return NULL;
}

PUBLIC int
gbm_device_get_fd(struct gbm_device *gbm)
{
	return gbm->fd;
}

PUBLIC const char *
gbm_device_get_backend_name(struct gbm_device *gbm)
{
	return gbm->driver->name;
}

PUBLIC int
gbm_device_is_format_supported(struct gbm_device *gbm,
			       uint32_t format, uint32_t usage)
{
	unsigned i;

	if (usage & GBM_BO_USE_CURSOR &&
		usage & GBM_BO_USE_RENDERING)
		return 0;

	for(i = 0 ; i < ARRAY_SIZE(gbm->driver->format_list); i++)
	{
		if (!gbm->driver->format_list[i].format)
			break;

		if (gbm->driver->format_list[i].format == format &&
			(gbm->driver->format_list[i].usage & usage) == usage)
			return 1;
	}

	return 0;
}

PUBLIC struct gbm_device *gbm_create_device(int fd)
{
	struct gbm_device *gbm;
	int ret;

	gbm = (struct gbm_device*) malloc(sizeof(*gbm));
	if (!gbm)
		return NULL;

	gbm->fd = fd;

	gbm->driver = gbm_get_driver(fd);
	if (!gbm->driver) {
		free(gbm);
		return NULL;
	}

	if (gbm->driver->init) {
		ret = gbm->driver->init(gbm);
		if (ret) {
			free(gbm);
			return NULL;
		}
	}

	return gbm;
}

PUBLIC void gbm_device_destroy(struct gbm_device *gbm)
{
	if (gbm->driver->close)
		gbm->driver->close(gbm);
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

static struct gbm_bo *gbm_bo_new(struct gbm_device *gbm,
				 uint32_t width, uint32_t height,
				 uint32_t format)
{
	struct gbm_bo *bo;

	bo = (struct gbm_bo*) calloc(1, sizeof(*bo));
	if (!bo)
		return NULL;

	bo->gbm = gbm;
	bo->width = width;
	bo->height = height;
	bo->format = format;
	bo->num_planes = gbm_num_planes_from_format(format);
	if (!bo->num_planes) {
		free(bo);
		return NULL;
	}

	return bo;
}

PUBLIC struct gbm_bo *gbm_bo_create(struct gbm_device *gbm, uint32_t width,
				    uint32_t height, uint32_t format,
				    uint32_t flags)
{
	struct gbm_bo *bo;
	int ret;

	if (!gbm_device_is_format_supported(gbm, format, flags))
		return NULL;

	bo = gbm_bo_new(gbm, width, height, format);
	if (!bo)
		return NULL;

	ret = gbm->driver->bo_create(bo, width, height, format, flags);
	if (ret) {
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

	bo->gbm->driver->bo_destroy(bo);
	free(bo);
}

PUBLIC struct gbm_bo *
gbm_bo_import(struct gbm_device *gbm, uint32_t type,
              void *buffer, uint32_t usage)
{
	struct gbm_import_fd_data *fd_data = buffer;
	struct gbm_bo *bo;
	struct drm_prime_handle prime_handle;
	int ret;

	if (type != GBM_BO_IMPORT_FD)
		return NULL;

	if (!gbm_device_is_format_supported(gbm, fd_data->format, usage))
		return NULL;

	/* This function can support only single plane formats. */
	/* If multi-plane import is desired, new function should be added. */
	if (gbm_num_planes_from_format(fd_data->format) != 1)
		return NULL;

	bo = gbm_bo_new(gbm, fd_data->width, fd_data->height, fd_data->format);
	if (!bo)
		return NULL;

	bo->strides[0] = fd_data->stride;
	bo->sizes[0] = fd_data->height * fd_data->stride;

	memset(&prime_handle, 0, sizeof(prime_handle));
	prime_handle.fd = fd_data->fd;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_PRIME_FD_TO_HANDLE failed "
				"(fd=%u)\n", prime_handle.fd);
		free(bo);
		return NULL;
	}

	bo->handles[0].u32 = prime_handle.handle;

	return bo;
}

PUBLIC uint32_t
gbm_bo_get_width(struct gbm_bo *bo)
{
	return bo->width;
}

PUBLIC uint32_t
gbm_bo_get_height(struct gbm_bo *bo)
{
	return bo->height;
}

PUBLIC uint32_t
gbm_bo_get_stride(struct gbm_bo *bo)
{
	return gbm_bo_get_plane_stride(bo, 0);
}

PUBLIC uint32_t
gbm_bo_get_stride_or_tiling(struct gbm_bo *bo)
{
	return bo->tiling ? bo->tiling : gbm_bo_get_stride(bo);
}

PUBLIC uint32_t
gbm_bo_get_format(struct gbm_bo *bo)
{
	return bo->format;
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
	return bo->num_planes;
}

PUBLIC union gbm_bo_handle
gbm_bo_get_plane_handle(struct gbm_bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->handles[plane];
}

#ifndef DRM_RDWR
#define DRM_RDWR O_RDWR
#endif

PUBLIC int
gbm_bo_get_plane_fd(struct gbm_bo *bo, size_t plane)
{
	int fd;
	assert(plane < bo->num_planes);

	if (drmPrimeHandleToFD(
			gbm_device_get_fd(bo->gbm),
			gbm_bo_get_plane_handle(bo, plane).u32,
			DRM_CLOEXEC | DRM_RDWR,
			&fd))
		return -1;
	else
		return fd;
}

PUBLIC uint32_t
gbm_bo_get_plane_offset(struct gbm_bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->offsets[plane];
}

PUBLIC uint32_t
gbm_bo_get_plane_size(struct gbm_bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->sizes[plane];
}

PUBLIC uint32_t
gbm_bo_get_plane_stride(struct gbm_bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->strides[plane];
}

PUBLIC uint64_t
gbm_bo_get_plane_format_modifier(struct gbm_bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->format_modifiers[plane];
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
