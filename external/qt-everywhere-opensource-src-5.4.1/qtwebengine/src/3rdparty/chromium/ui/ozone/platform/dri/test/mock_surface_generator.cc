// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/test/mock_surface_generator.h"

namespace ui {

MockSurfaceGenerator::MockSurfaceGenerator(DriWrapper* dri) : dri_(dri) {}

MockSurfaceGenerator::~MockSurfaceGenerator() {}

ScanoutSurface* MockSurfaceGenerator::Create(const gfx::Size& size) {
  surfaces_.push_back(new MockDriSurface(dri_, size));
  return surfaces_.back();
}

}  // namespace ui
