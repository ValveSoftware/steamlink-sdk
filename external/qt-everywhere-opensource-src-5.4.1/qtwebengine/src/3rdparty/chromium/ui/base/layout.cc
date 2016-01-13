// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "ui/gfx/win/dpi.h"
#include <Windows.h>
#endif  // defined(OS_WIN)

namespace ui {

namespace {

bool ScaleFactorComparator(const ScaleFactor& lhs, const ScaleFactor& rhs){
  return GetScaleForScaleFactor(lhs) < GetScaleForScaleFactor(rhs);
}

std::vector<ScaleFactor>* g_supported_scale_factors = NULL;

const float kScaleFactorScales[] = {1.0f, 1.0f, 1.25f, 1.33f, 1.4f, 1.5f, 1.8f,
                                    2.0f, 2.5f, 3.0f};
COMPILE_ASSERT(NUM_SCALE_FACTORS == arraysize(kScaleFactorScales),
               kScaleFactorScales_incorrect_size);

}  // namespace

void SetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  if (g_supported_scale_factors != NULL)
    delete g_supported_scale_factors;

  g_supported_scale_factors = new std::vector<ScaleFactor>(scale_factors);
  std::sort(g_supported_scale_factors->begin(),
            g_supported_scale_factors->end(),
            ScaleFactorComparator);

  // Set ImageSkia's supported scales.
  std::vector<float> scales;
  for (std::vector<ScaleFactor>::const_iterator it =
          g_supported_scale_factors->begin();
       it != g_supported_scale_factors->end(); ++it) {
    scales.push_back(kScaleFactorScales[*it]);
  }
  gfx::ImageSkia::SetSupportedScales(scales);
}

const std::vector<ScaleFactor>& GetSupportedScaleFactors() {
  DCHECK(g_supported_scale_factors != NULL);
  return *g_supported_scale_factors;
}

ScaleFactor GetSupportedScaleFactor(float scale) {
  DCHECK(g_supported_scale_factors != NULL);
  ScaleFactor closest_match = SCALE_FACTOR_100P;
  float smallest_diff =  std::numeric_limits<float>::max();
  for (size_t i = 0; i < g_supported_scale_factors->size(); ++i) {
    ScaleFactor scale_factor = (*g_supported_scale_factors)[i];
    float diff = std::abs(kScaleFactorScales[scale_factor] - scale);
    if (diff < smallest_diff) {
      closest_match = scale_factor;
      smallest_diff = diff;
    }
  }
  DCHECK_NE(closest_match, SCALE_FACTOR_NONE);
  return closest_match;
}

float GetImageScale(ScaleFactor scale_factor) {
#if defined(OS_WIN)
  if (gfx::IsHighDPIEnabled())
    return gfx::win::GetDeviceScaleFactor();
#endif
  return GetScaleForScaleFactor(scale_factor);
}

float GetScaleForScaleFactor(ScaleFactor scale_factor) {
  return kScaleFactorScales[scale_factor];
}

namespace test {

ScopedSetSupportedScaleFactors::ScopedSetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& new_scale_factors) {
  if (g_supported_scale_factors) {
    original_scale_factors_ =
        new std::vector<ScaleFactor>(*g_supported_scale_factors);
  } else {
    original_scale_factors_ = NULL;
  }
  SetSupportedScaleFactors(new_scale_factors);
}

ScopedSetSupportedScaleFactors::~ScopedSetSupportedScaleFactors() {
  if (original_scale_factors_) {
    SetSupportedScaleFactors(*original_scale_factors_);
    delete original_scale_factors_;
  } else {
    delete g_supported_scale_factors;
    g_supported_scale_factors = NULL;
  }
}

}  // namespace test

#if !defined(OS_MACOSX)
float GetScaleFactorForNativeView(gfx::NativeView view) {
  gfx::Screen* screen = gfx::Screen::GetScreenFor(view);
  if (screen->IsDIPEnabled()) {
    gfx::Display display = screen->GetDisplayNearestWindow(view);
    return display.device_scale_factor();
  }
  return 1.0f;
}
#endif  // !defined(OS_MACOSX)

}  // namespace ui
