/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc_helpers.h"

#include <cstdlib>
#include <cutils/log.h>
#include <fcntl.h>
#include <xf86drm.h>

uint64_t cros_gralloc_convert_flags(int flags)
{
	uint64_t usage = DRV_BO_USE_NONE;

	if (flags & GRALLOC_USAGE_CURSOR)
		usage |= DRV_BO_USE_CURSOR;
	if ((flags & sw_read()) == GRALLOC_USAGE_SW_READ_RARELY)
		usage |= DRV_BO_USE_SW_READ_RARELY;
	if ((flags & sw_read()) == GRALLOC_USAGE_SW_READ_OFTEN)
		usage |= DRV_BO_USE_SW_READ_OFTEN;
	if ((flags & sw_write()) == GRALLOC_USAGE_SW_WRITE_RARELY)
		usage |= DRV_BO_USE_SW_WRITE_RARELY;
	if ((flags & sw_write()) == GRALLOC_USAGE_SW_WRITE_OFTEN)
		usage |= DRV_BO_USE_SW_WRITE_OFTEN;
	if (flags & GRALLOC_USAGE_HW_TEXTURE)
		usage |= DRV_BO_USE_RENDERING;
	if (flags & GRALLOC_USAGE_HW_RENDER)
		usage |= DRV_BO_USE_RENDERING;
	if (flags & GRALLOC_USAGE_HW_2D)
		usage |= DRV_BO_USE_RENDERING;
	if (flags & GRALLOC_USAGE_HW_COMPOSER)
	/* HWC wants to use display hardware, but can defer to OpenGL. */
		usage |= DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING;
	if (flags & GRALLOC_USAGE_HW_FB)
		usage |= DRV_BO_USE_SCANOUT;
	if (flags & GRALLOC_USAGE_EXTERNAL_DISP)
	/* We're ignoring this flag until we decide what to with display link */
		usage |= DRV_BO_USE_NONE;
	if (flags & GRALLOC_USAGE_PROTECTED)
		usage |= DRV_BO_USE_PROTECTED;
	if (flags & GRALLOC_USAGE_HW_VIDEO_ENCODER)
		/*HACK: See b/30054495 */
		usage |= DRV_BO_USE_SW_READ_OFTEN;
	if (flags & GRALLOC_USAGE_HW_CAMERA_WRITE)
		usage |= DRV_BO_USE_HW_CAMERA_WRITE;
	if (flags & GRALLOC_USAGE_HW_CAMERA_READ)
		usage |= DRV_BO_USE_HW_CAMERA_READ;
	if (flags & GRALLOC_USAGE_HW_CAMERA_ZSL)
		usage |= DRV_BO_USE_HW_CAMERA_ZSL;
	if (flags & GRALLOC_USAGE_RENDERSCRIPT)
		usage |= DRV_BO_USE_RENDERSCRIPT;

	return usage;
}

drv_format_t cros_gralloc_convert_format(int format)
{
	/*
	 * Conversion from HAL to fourcc-based DRV formats based on
	 * platform_android.c in mesa.
	 */

	switch (format) {
	case HAL_PIXEL_FORMAT_BGRA_8888:
		return DRV_FORMAT_ARGB8888;
	case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
		return DRV_FORMAT_FLEX_IMPLEMENTATION_DEFINED;
	case HAL_PIXEL_FORMAT_RGB_565:
		return DRV_FORMAT_RGB565;
	case HAL_PIXEL_FORMAT_RGB_888:
		return DRV_FORMAT_RGB888;
	case HAL_PIXEL_FORMAT_RGBA_8888:
		return DRV_FORMAT_ABGR8888;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		return DRV_FORMAT_XBGR8888;
	case HAL_PIXEL_FORMAT_YCbCr_420_888:
		return DRV_FORMAT_FLEX_YCbCr_420_888;
	case HAL_PIXEL_FORMAT_YV12:
		return DRV_FORMAT_YVU420;
	}

	return DRV_FORMAT_NONE;
}

static int32_t cros_gralloc_query_rendernode(struct driver **drv,
					     const char *name)
{
	/* TODO(gsingh): Enable render nodes on udl/evdi. */
	int fd;
	drmVersionPtr version;
	char const *str = "%s/renderD%d";
	int32_t num_nodes = 63;
	int32_t min_node = 128;
	int32_t max_node = (min_node + num_nodes);

	for (int i = min_node; i < max_node; i++) {
		char *node;

		if (asprintf(&node, str, DRM_DIR_NAME, i) < 0)
			continue;

		fd = open(node, O_RDWR, 0);
		free(node);

		if (fd < 0)
			continue;

		version = drmGetVersion(fd);

		if (version && name && !strcmp(version->name, name)) {
			drmFreeVersion(version);
			continue;
		}

		drmFreeVersion(version);
		*drv = drv_create(fd);

		if (*drv)
			return CROS_GRALLOC_ERROR_NONE;
	}

	return CROS_GRALLOC_ERROR_NO_RESOURCES;
}

int32_t cros_gralloc_rendernode_open(struct driver **drv)
{
	int32_t ret;
	ret = cros_gralloc_query_rendernode(drv, NULL);

	/* Look for vgem driver if no hardware is found. */
	if (ret)
		ret = cros_gralloc_query_rendernode(drv, "vgem");

	return ret;
}

int32_t cros_gralloc_validate_handle(struct cros_gralloc_handle *hnd)
{
	if (!hnd || hnd->magic != cros_gralloc_magic())
		return CROS_GRALLOC_ERROR_BAD_HANDLE;

	return CROS_GRALLOC_ERROR_NONE;
}

void cros_gralloc_log(const char *prefix, const char *file, int line,
		      const char *format, ...)
{
	va_list args;
	va_start(args, format);
	ALOGE("%s - [%s(%d)]", prefix, basename(file), line);
	__android_log_vprint(ANDROID_LOG_ERROR, prefix, format, args);
	va_end(args);
}
