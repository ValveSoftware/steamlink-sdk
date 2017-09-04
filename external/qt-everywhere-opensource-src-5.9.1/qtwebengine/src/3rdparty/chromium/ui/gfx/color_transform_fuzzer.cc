// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/icc_profile.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::AtExitManager at_exit;
  gfx::ICCProfile icc =
      gfx::ICCProfile::FromData(reinterpret_cast<const char*>(data), size);
  gfx::ColorSpace bt709 = gfx::ColorSpace::CreateREC709();
  std::unique_ptr<gfx::ColorTransform> t(gfx::ColorTransform::NewColorTransform(
      bt709, icc.GetColorSpace(),
      gfx::ColorTransform::Intent::INTENT_ABSOLUTE));
  gfx::ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->transform(&tmp, 1);
  return 0;
}
