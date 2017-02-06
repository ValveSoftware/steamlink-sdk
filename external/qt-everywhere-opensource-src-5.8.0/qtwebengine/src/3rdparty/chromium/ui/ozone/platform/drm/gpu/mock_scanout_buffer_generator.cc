// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer_generator.h"

#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer.h"

namespace ui {

MockScanoutBufferGenerator::MockScanoutBufferGenerator() {}

MockScanoutBufferGenerator::~MockScanoutBufferGenerator() {}

scoped_refptr<ScanoutBuffer> MockScanoutBufferGenerator::Create(
    const scoped_refptr<DrmDevice>& drm,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  scoped_refptr<MockScanoutBuffer> buffer(
      new MockScanoutBuffer(size, GetFourCCFormatForFramebuffer(format)));

  return buffer;
}

}  // namespace ui
