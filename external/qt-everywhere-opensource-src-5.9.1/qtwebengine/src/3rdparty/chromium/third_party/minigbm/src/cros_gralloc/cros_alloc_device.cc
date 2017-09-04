/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc.h"

static struct cros_gralloc_bo *cros_gralloc_bo_create(struct driver *drv,
						      int width, int height,
						      int format, int usage)
{
	uint64_t drv_usage;
	drv_format_t drv_format;
	struct cros_gralloc_bo *bo;

	drv_format = cros_gralloc_convert_format(format);
	drv_format = drv_resolve_format(drv, drv_format);
	drv_usage = cros_gralloc_convert_flags(usage);

	if (!drv_is_format_supported(drv, drv_format, drv_usage)) {
		cros_gralloc_error("Unsupported combination -- HAL format: %u, "
				    "HAL flags: %u, drv_format: %u, "
				    "drv_flags: %llu", format, usage,
				    drv_format, drv_usage);
		return NULL;
	}

	bo = new cros_gralloc_bo();
	memset(bo, 0, sizeof(*bo));

	bo->bo = drv_bo_create(drv, width, height, drv_format, drv_usage);
	if (!bo->bo) {
		delete bo;
		cros_gralloc_error("Failed to create bo.");
		return NULL;
	}

	if (drv_num_buffers_per_bo(bo->bo) != 1) {
		drv_bo_destroy(bo->bo);
		delete bo;
		cros_gralloc_error("Can only support one buffer per bo.");
		return NULL;
	}

	bo->refcount = 1;

	return bo;
}

static struct cros_gralloc_handle *cros_gralloc_handle_from_bo(struct bo *bo)
{
	struct cros_gralloc_handle *hnd;

	hnd = new cros_gralloc_handle();
	memset(hnd, 0, sizeof(*hnd));

	hnd->base.version = sizeof(hnd->base);
	hnd->base.numFds = 1;
	hnd->base.numInts = num_ints();

	for (size_t p = 0; p < drv_bo_get_num_planes(bo); p++) {
		hnd->data.strides[p] = drv_bo_get_plane_stride(bo, p);
		hnd->data.offsets[p] = drv_bo_get_plane_offset(bo, p);
		hnd->data.sizes[p] = drv_bo_get_plane_size(bo, p);
	}

	hnd->data.fds[0] = drv_bo_get_plane_fd(bo, 0);
	hnd->data.width = drv_bo_get_width(bo);
	hnd->data.height = drv_bo_get_height(bo);
	hnd->data.format = drv_bo_get_format(bo);

	hnd->magic = cros_gralloc_magic();
	hnd->registrations = 0;

	hnd->pixel_stride = hnd->data.strides[0];
	hnd->pixel_stride /= drv_stride_from_format(hnd->data.format, 1, 0);

	return hnd;
}

static int cros_gralloc_alloc(alloc_device_t *dev, int w, int h, int format,
			      int usage, buffer_handle_t *handle, int *stride)
{
	auto mod = (struct cros_gralloc_module *) dev->common.module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	auto bo = cros_gralloc_bo_create(mod->drv, w, h, format, usage);
	if (!bo)
		return CROS_GRALLOC_ERROR_NO_RESOURCES;

	auto hnd = cros_gralloc_handle_from_bo(bo->bo);
	hnd->format = static_cast<int32_t>(format);
	hnd->usage = static_cast<int32_t>(usage);

	hnd->bo = reinterpret_cast<uint64_t>(bo);
	bo->hnd = hnd;

	mod->handles.insert(reinterpret_cast<uint64_t>(&hnd->base));
	mod->buffers[drv_bo_get_plane_handle(bo->bo, 0).u32] = bo;

	*stride = static_cast<int>(hnd->pixel_stride);
	*handle = &hnd->base;

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_free(alloc_device_t *dev, buffer_handle_t handle)
{
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *) handle;
	auto mod = (struct cros_gralloc_module *) dev->common.module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (hnd->registrations > 0) {
		cros_gralloc_error("Deallocating before unregistering.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	return cros_gralloc_decrement_reference_count(mod, bo);
}

static int cros_gralloc_close(struct hw_device_t *dev)
{
	auto mod = (struct cros_gralloc_module *) dev->module;
	auto alloc = (struct alloc_device_t *) dev;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (mod->drv) {
		drv_destroy(mod->drv);
		mod->drv = NULL;
	}

	mod->buffers.clear();
	mod->handles.clear();

	delete alloc;

	return CROS_GRALLOC_ERROR_NONE;
}

int cros_gralloc_open(const struct hw_module_t *mod, const char *name,
		      struct hw_device_t **dev)
{
	auto module = (struct cros_gralloc_module *) mod;
	std::lock_guard<std::mutex> lock(module->mutex);

	if (module->drv)
		return CROS_GRALLOC_ERROR_NONE;

	if (strcmp(name, GRALLOC_HARDWARE_GPU0)) {
		cros_gralloc_error("Incorrect device name - %s.", name);
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	if (cros_gralloc_rendernode_open(&module->drv)) {
		cros_gralloc_error("Failed to open render node.");
		return CROS_GRALLOC_ERROR_NO_RESOURCES;
	}

	auto alloc = new alloc_device_t();
	memset(alloc, 0, sizeof(*alloc));

	alloc->alloc = cros_gralloc_alloc;
	alloc->free = cros_gralloc_free;
	alloc->common.tag = HARDWARE_DEVICE_TAG;
	alloc->common.version = 0;
	alloc->common.module = (hw_module_t*) mod;
	alloc->common.close = cros_gralloc_close;

	*dev = &alloc->common;

	return CROS_GRALLOC_ERROR_NONE;
}
