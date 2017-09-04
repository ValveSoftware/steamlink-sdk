// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer.h"

namespace ui {

MockScanoutBuffer::MockScanoutBuffer(const gfx::Size& size, uint32_t format)
    : size_(size), format_(format) {}

MockScanoutBuffer::~MockScanoutBuffer() {}

uint32_t MockScanoutBuffer::GetFramebufferId() const {
  return 1;
}

uint32_t MockScanoutBuffer::GetHandle() const {
  return 0;
}

gfx::Size MockScanoutBuffer::GetSize() const {
  return size_;
}

uint32_t MockScanoutBuffer::GetFramebufferPixelFormat() const {
  return format_;
}

const DrmDevice* MockScanoutBuffer::GetDrmDevice() const {
  return nullptr;
}

bool MockScanoutBuffer::RequiresGlFinish() const {
  return false;
}

}  // namespace ui
