// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
#define UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_

#include "ui/compositor/compositor_export.h"

namespace switches {

COMPOSITOR_EXPORT extern const char kEnableHardwareOverlays[];
COMPOSITOR_EXPORT extern const char kEnablePixelOutputInTests[];
COMPOSITOR_EXPORT extern const char kUIDisablePartialSwap[];
COMPOSITOR_EXPORT extern const char kUIEnableRGBA4444Textures[];
COMPOSITOR_EXPORT extern const char kUIEnableZeroCopy[];
COMPOSITOR_EXPORT extern const char kUIShowPaintRects[];

}  // namespace switches

namespace ui {

bool IsUIZeroCopyEnabled();

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
