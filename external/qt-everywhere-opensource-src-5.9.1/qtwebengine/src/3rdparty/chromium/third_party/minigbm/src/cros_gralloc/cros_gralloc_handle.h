/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROS_GRALLOC_HANDLE_H
#define CROS_GRALLOC_HANDLE_H

#include <cutils/native_handle.h>
#include <cstdint>

#include "../drv.h"

struct cros_gralloc_handle {
	native_handle_t base;
	drv_import_fd_data data;
	uint32_t magic;
	uint32_t pixel_stride;
	int32_t format, usage;     /* Android format and usage. */
	uint64_t bo;		   /* Pointer to cros_gralloc_bo. */
	int32_t registrations;     /*
				    * Number of times (*register)() has been
				    * called on this handle.
				    */
};

#endif
