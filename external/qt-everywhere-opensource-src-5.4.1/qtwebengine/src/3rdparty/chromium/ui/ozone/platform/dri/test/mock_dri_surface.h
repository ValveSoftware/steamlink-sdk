// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_SURFACE_H_
#define UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_SURFACE_H_

#include <vector>

#include "ui/ozone/platform/dri/dri_surface.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {

class DriBuffer;
class DriWrapper;

class MockDriSurface : public DriSurface {
 public:
  MockDriSurface(DriWrapper* dri, const gfx::Size& size);
  virtual ~MockDriSurface();

  const std::vector<ui::DriBuffer*>& bitmaps() const { return bitmaps_; }
  void set_initialize_expectation(bool state) {
    initialize_expectation_ = state;
  }

 private:
  // DriSurface:
  virtual ui::DriBuffer* CreateBuffer() OVERRIDE;

  DriWrapper* dri_;                  // Not owned.
  std::vector<DriBuffer*> bitmaps_;  // Not owned.

  bool initialize_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_SURFACE_H_
