// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/test/mock_dri_surface.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {

class MockDriBuffer : public DriBuffer {
 public:
  MockDriBuffer(DriWrapper* dri, bool initialize_expectation)
      : DriBuffer(dri), initialize_expectation_(initialize_expectation) {}
  virtual ~MockDriBuffer() { surface_.clear(); }

  virtual bool Initialize(const SkImageInfo& info) OVERRIDE {
    if (!initialize_expectation_)
      return false;

    dri_->AddFramebuffer(
        info.width(), info.height(), 24, 32, stride_, handle_, &framebuffer_);
    surface_ = skia::AdoptRef(SkSurface::NewRaster(info));
    surface_->getCanvas()->clear(SK_ColorBLACK);

    return true;
  }

 private:
  bool initialize_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriBuffer);
};

}  // namespace

MockDriSurface::MockDriSurface(DriWrapper* dri, const gfx::Size& size)
    : DriSurface(dri, size), dri_(dri), initialize_expectation_(true) {}

MockDriSurface::~MockDriSurface() {}

DriBuffer* MockDriSurface::CreateBuffer() {
  MockDriBuffer* bitmap = new MockDriBuffer(dri_, initialize_expectation_);
  bitmaps_.push_back(bitmap);

  return bitmap;
}

}  // namespace ui
