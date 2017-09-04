// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ui/gfx/native_pixmap_handle.h"

namespace gfx {

NativePixmapPlane::NativePixmapPlane()
    : stride(0), offset(0), size(0), modifier(0) {}

NativePixmapPlane::NativePixmapPlane(int stride,
                                     int offset,
                                     uint64_t size,
                                     uint64_t modifier)
    : stride(stride), offset(offset), size(size), modifier(modifier) {}

NativePixmapPlane::NativePixmapPlane(const NativePixmapPlane& other) = default;

NativePixmapPlane::~NativePixmapPlane() {}

NativePixmapHandle::NativePixmapHandle() {}
NativePixmapHandle::NativePixmapHandle(const NativePixmapHandle& other) =
    default;

NativePixmapHandle::~NativePixmapHandle() {}

}  // namespace gfx
