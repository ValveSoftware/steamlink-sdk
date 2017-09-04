/*
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GBM_HELPERS_H
#define GBM_HELPERS_H

drv_format_t gbm_convert_format(uint32_t format);
uint64_t gbm_convert_flags(uint32_t flags);

#endif
