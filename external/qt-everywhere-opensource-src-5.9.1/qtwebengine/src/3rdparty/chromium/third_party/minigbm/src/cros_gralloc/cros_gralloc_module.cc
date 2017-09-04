/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc.h"

#include <sys/mman.h>
#include <xf86drm.h>

int cros_gralloc_validate_reference(struct cros_gralloc_module *mod,
				    struct cros_gralloc_handle *hnd,
				    struct cros_gralloc_bo **bo)
{
	uint64_t key = reinterpret_cast<uint64_t>(&hnd->base);

	if (!mod->handles.count(key))
		return CROS_GRALLOC_ERROR_BAD_HANDLE;

	*bo = reinterpret_cast<cros_gralloc_bo*>(hnd->bo);
	return CROS_GRALLOC_ERROR_NONE;
}

int cros_gralloc_decrement_reference_count(struct cros_gralloc_module *mod,
					   struct cros_gralloc_bo *bo)
{
	if (bo->refcount <= 0) {
		cros_gralloc_error("The reference count is <= 0.");
		assert(0);
	}

	if (!--bo->refcount) {
		mod->buffers.erase(drv_bo_get_plane_handle(bo->bo, 0).u32);
		drv_bo_destroy(bo->bo);

		if (bo->hnd) {
			native_handle_close(&bo->hnd->base);
			delete bo->hnd;
		}

		delete bo;
	}

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_register_buffer(struct gralloc_module_t const* module,
				        buffer_handle_t handle)
{
	uint32_t id;
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *) handle;
	auto mod = (struct cros_gralloc_module *) module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (!mod->drv) {
		if (cros_gralloc_rendernode_open(&mod->drv)) {
			cros_gralloc_error("Failed to open render node.");
			return CROS_GRALLOC_ERROR_NO_RESOURCES;
		}
	}

	if (!cros_gralloc_validate_reference(mod, hnd, &bo)) {
		bo->refcount++;
		hnd->registrations++;
		return CROS_GRALLOC_ERROR_NONE;
	}

	if (drmPrimeFDToHandle(drv_get_fd(mod->drv), hnd->data.fds[0], &id)) {
		cros_gralloc_error("drmPrimeFDToHandle failed.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (mod->buffers.count(id)) {
		bo = mod->buffers[id];
		bo->refcount++;
		hnd->bo = reinterpret_cast<uint64_t>(bo);
	} else {
		hnd->data.fds[1] = hnd->data.fds[2] = hnd->data.fds[3]
				 = hnd->data.fds[0];
		bo = new cros_gralloc_bo();

		bo->bo = drv_bo_import(mod->drv, &hnd->data);
		if (!bo->bo) {
			delete bo;
			return CROS_GRALLOC_ERROR_NO_RESOURCES;
		}

		id = drv_bo_get_plane_handle(bo->bo, 0).u32;
		mod->buffers[id] = bo;

		bo->refcount = 1;
		hnd->bo = reinterpret_cast<uint64_t>(bo);
	}

	hnd->registrations = 1;
	mod->handles.insert(reinterpret_cast<uint64_t>(&hnd->base));

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_unregister_buffer(struct gralloc_module_t const* module,
					  buffer_handle_t handle)
{
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *) handle;
	auto mod = (struct cros_gralloc_module *) module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (hnd->registrations <= 0) {
		cros_gralloc_error("Handle not registered.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	hnd->registrations--;

	if (!hnd->registrations)
		mod->handles.erase(reinterpret_cast<uint64_t>(&hnd->base));

	return cros_gralloc_decrement_reference_count(mod, bo);
}

static int cros_gralloc_lock(struct gralloc_module_t const* module,
			     buffer_handle_t handle, int usage, int l, int t,
			     int w, int h, void** vaddr)
{
	struct cros_gralloc_bo *bo;
	auto mod = (struct cros_gralloc_module *) module;
	auto hnd = (struct cros_gralloc_handle *) handle;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (hnd->format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
		cros_gralloc_error("YUV format not compatible.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (sw_access() & usage) {
		*vaddr = drv_bo_map(bo->bo, 0, 0, drv_bo_get_width(bo->bo),
				   drv_bo_get_height(bo->bo), 0, &bo->map_data,
				   0);

		if (*vaddr == MAP_FAILED) {
			cros_gralloc_error("Mapping failed.");
			return CROS_GRALLOC_ERROR_UNSUPPORTED;
		}
	}

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_unlock(struct gralloc_module_t const* module,
			       buffer_handle_t handle)
{
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *) handle;
	auto mod = (struct cros_gralloc_module *) module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (bo->map_data)
		drv_bo_unmap(bo->bo, bo->map_data);

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_perform(struct gralloc_module_t const* module,
				int op, ... )
{
	va_list args;
	struct cros_gralloc_bo *bo;
	int32_t *out_format;
	uint64_t *out_store;
	buffer_handle_t handle;
	uint32_t *out_width, *out_height, *out_stride;
	auto mod = (struct cros_gralloc_module *) module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	switch (op) {
	case GRALLOC_DRM_GET_STRIDE:
	case GRALLOC_DRM_GET_FORMAT:
	case GRALLOC_DRM_GET_DIMENSIONS:
	case GRALLOC_DRM_GET_BACKING_STORE:
		break;
	default:
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	va_start(args, op);
	handle = va_arg(args, buffer_handle_t);
	auto hnd = (struct cros_gralloc_handle *) handle;

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	switch (op) {
	case GRALLOC_DRM_GET_STRIDE:
		out_stride = va_arg(args, uint32_t *);
		*out_stride = hnd->pixel_stride;
		break;
	case GRALLOC_DRM_GET_FORMAT:
		out_format = va_arg(args, int32_t *);
		*out_format = hnd->format;
		break;
	case GRALLOC_DRM_GET_DIMENSIONS:
		out_width = va_arg(args, uint32_t *);
		out_height = va_arg(args, uint32_t *);
		*out_width = hnd->data.width;
		*out_height = hnd->data.height;
		break;
	case GRALLOC_DRM_GET_BACKING_STORE:
		out_store = va_arg(args, uint64_t *);
		*out_store = drv_bo_get_plane_handle(bo->bo, 0).u64;
		break;
	default:
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	va_end(args);

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_lock_ycbcr(struct gralloc_module_t const* module,
				   buffer_handle_t handle, int usage, int l,
				   int t, int w, int h,
				   struct android_ycbcr *ycbcr)
{
	uint8_t *addr = NULL;
	size_t offsets[DRV_MAX_PLANES];
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *) handle;
	auto mod = (struct cros_gralloc_module *) module;
	std::lock_guard<std::mutex> lock(mod->mutex);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if ((hnd->format != HAL_PIXEL_FORMAT_YCbCr_420_888)) {
		cros_gralloc_error("Non-YUV format not compatible.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (sw_access() & usage) {
		void *vaddr = drv_bo_map(bo->bo, 0, 0, drv_bo_get_width(bo->bo),
					 drv_bo_get_height(bo->bo), 0,
					 &bo->map_data, 0);

		if (vaddr == MAP_FAILED) {
			cros_gralloc_error("Mapping failed.");
			return CROS_GRALLOC_ERROR_UNSUPPORTED;
		}

		addr = static_cast<uint8_t*>(vaddr);
	}

	for (size_t p = 0; p < drv_bo_get_num_planes(bo->bo); p++)
		offsets[p] = drv_bo_get_plane_offset(bo->bo, p);

	switch (hnd->data.format) {
	case DRV_FORMAT_NV12:
		ycbcr->y = addr;
		ycbcr->cb = addr + offsets[1];
		ycbcr->cr = addr + offsets[1] + 1;
		ycbcr->ystride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->cstride = drv_bo_get_plane_stride(bo->bo, 1);
		ycbcr->chroma_step = 2;
		break;
	case DRV_FORMAT_YVU420:
		ycbcr->y = addr;
		ycbcr->cb = addr + offsets[2];
		ycbcr->cr = addr + offsets[1];
		ycbcr->ystride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->cstride = drv_bo_get_plane_stride(bo->bo, 1);
		ycbcr->chroma_step = 1;
		break;
	case DRV_FORMAT_UYVY:
		ycbcr->y = addr + 1;
		ycbcr->cb = addr;
		ycbcr->cr = addr + 2;
		ycbcr->ystride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->cstride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->chroma_step = 2;
		break;
	default:
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	return CROS_GRALLOC_ERROR_NONE;
}

static struct hw_module_methods_t cros_gralloc_module_methods = {
	.open = cros_gralloc_open
};

struct cros_gralloc_module HAL_MODULE_INFO_SYM = {
	.base = {
		.common = {
			.tag = HARDWARE_MODULE_TAG,
			.module_api_version = GRALLOC_MODULE_API_VERSION_0_2,
			.hal_api_version = 0,
			.id = GRALLOC_HARDWARE_MODULE_ID,
			.name = "CrOS Gralloc",
			.author = "Chrome OS",
			.methods = &cros_gralloc_module_methods,
		},
		.registerBuffer = cros_gralloc_register_buffer,
		.unregisterBuffer = cros_gralloc_unregister_buffer,
		.lock = cros_gralloc_lock,
		.unlock = cros_gralloc_unlock,
		.perform = cros_gralloc_perform,
		.lock_ycbcr = cros_gralloc_lock_ycbcr,
	},

	.drv = NULL,
};
