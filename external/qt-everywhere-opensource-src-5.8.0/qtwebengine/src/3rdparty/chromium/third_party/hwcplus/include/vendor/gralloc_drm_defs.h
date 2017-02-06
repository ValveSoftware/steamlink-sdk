// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VENDOR_GRALLOCDRM_H
#define VENDOR_GRALLOCDRM_H

#include <sys/cdefs.h>

__BEGIN_DECLS

// As defined by the android-x86 implementation of gralloc_drm.

// pixel format definitions
enum {
    HAL_PIXEL_FORMAT_DRM_NV12 = 0x102,
};


// used with the 'perform' method of gralloc_module_t
enum {
    GRALLOC_MODULE_PERFORM_GET_DRM_FD                = 0x80000002,
    GRALLOC_MODULE_PERFORM_GET_DRM_MAGIC             = 0x80000003,
    GRALLOC_MODULE_PERFORM_AUTH_DRM_MAGIC            = 0x80000004,
    GRALLOC_MODULE_PERFORM_ENTER_VT                  = 0x80000005,
    GRALLOC_MODULE_PERFORM_LEAVE_VT                  = 0x80000006,
};

__END_DECLS

#endif /* VENDOR_GRALLOCDRM_H */
