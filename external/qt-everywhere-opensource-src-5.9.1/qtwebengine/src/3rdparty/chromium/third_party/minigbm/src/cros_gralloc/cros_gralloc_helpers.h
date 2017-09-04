/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROS_GRALLOC_HELPERS_H
#define CROS_GRALLOC_HELPERS_H

#include "cros_gralloc_handle.h"

#include <hardware/gralloc.h>
#include <system/graphics.h>

/* Use these error codes derived from gralloc1 to make transition easier when
 * it happens
 */
typedef enum {
	CROS_GRALLOC_ERROR_NONE = 0,
	CROS_GRALLOC_ERROR_BAD_DESCRIPTOR = 1,
	CROS_GRALLOC_ERROR_BAD_HANDLE = 2,
	CROS_GRALLOC_ERROR_BAD_VALUE = 3,
	CROS_GRALLOC_ERROR_NOT_SHARED = 4,
	CROS_GRALLOC_ERROR_NO_RESOURCES = 5,
	CROS_GRALLOC_ERROR_UNDEFINED = 6,
	CROS_GRALLOC_ERROR_UNSUPPORTED = 7,
} cros_gralloc_error_t;

/* This enumeration must match the one in <gralloc_drm.h>.
 * The functions supported by this gralloc's temporary private API are listed
 * below. Use of these functions is highly discouraged and should only be
 * reserved for cases where no alternative to get same information (such as
 * querying ANativeWindow) exists.
 */
enum {
	GRALLOC_DRM_GET_STRIDE,
	GRALLOC_DRM_GET_FORMAT,
	GRALLOC_DRM_GET_DIMENSIONS,
	GRALLOC_DRM_GET_BACKING_STORE,
};

constexpr uint32_t cros_gralloc_magic(void)
{
	return 0xABCDDCBA;
}

constexpr uint32_t num_ints(void)
{
	/*
	 * numFds in our case is one. Subtract that from our numInts
	 * calculation.
	 */
	return ((sizeof(struct cros_gralloc_handle)
		- offsetof(cros_gralloc_handle, data)) / sizeof(int)) - 1;
}

constexpr uint32_t sw_access(void)
{
	return GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK;
}

constexpr uint32_t sw_read(void)
{
	return GRALLOC_USAGE_SW_READ_MASK;
}

constexpr uint32_t sw_write(void)
{
	return GRALLOC_USAGE_SW_WRITE_MASK;
}

uint64_t cros_gralloc_convert_flags(int flags);

drv_format_t cros_gralloc_convert_format(int format);

int32_t cros_gralloc_rendernode_open(struct driver **drv);

int32_t cros_gralloc_validate_handle(struct cros_gralloc_handle *hnd);

/* Logging code adapted from bsdrm */
__attribute__((format(printf, 4, 5)))
void cros_gralloc_log(const char *prefix, const char *file, int line,
		      const char *format, ...);

#define cros_gralloc_error(...)                                     \
	do {                                                        \
		cros_gralloc_log("cros_gralloc_error", __FILE__,    \
				 __LINE__, __VA_ARGS__);            \
	} while (0)

#endif
