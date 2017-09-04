/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GBM_GRALLOC_H
#define GBM_GRALLOC_H

#include "cros_gralloc_helpers.h"

#include <mutex>
#include <unordered_set>
#include <unordered_map>

struct cros_gralloc_bo {
	struct bo *bo;
	int32_t refcount;
	struct cros_gralloc_handle *hnd;
	void *map_data;
};

struct cros_gralloc_module {
	gralloc_module_t base;
	struct driver *drv;
	std::mutex mutex;
	std::unordered_set<uint64_t> handles;
	std::unordered_map<uint32_t, cros_gralloc_bo*> buffers;
};

int cros_gralloc_open(const struct hw_module_t *mod, const char *name,
		      struct hw_device_t **dev);

int cros_gralloc_validate_reference(struct cros_gralloc_module *mod,
				    struct cros_gralloc_handle *hnd,
				    struct cros_gralloc_bo **obj);

int cros_gralloc_decrement_reference_count(struct cros_gralloc_module *mod,
					   struct cros_gralloc_bo *obj);

#endif
