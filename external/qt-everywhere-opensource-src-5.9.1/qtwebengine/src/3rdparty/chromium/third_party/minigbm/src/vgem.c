/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drv_priv.h"
#include "helpers.h"

static drv_format_t vgem_resolve_format(drv_format_t format)
{
	switch (format) {
	case DRV_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRV_FORMAT_XBGR8888;
	case DRV_FORMAT_FLEX_YCbCr_420_888:
		return DRV_FORMAT_YVU420;
	default:
		return format;
	}
}

const struct backend backend_vgem =
{
	.name = "vgem",
	.bo_create = drv_dumb_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_map = drv_dumb_bo_map,
	.resolve_format = vgem_resolve_format,
	.format_list = {
		{DRV_FORMAT_ABGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING | DRV_BO_USE_CURSOR
				      | DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_YVU420,   DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
	}
};

