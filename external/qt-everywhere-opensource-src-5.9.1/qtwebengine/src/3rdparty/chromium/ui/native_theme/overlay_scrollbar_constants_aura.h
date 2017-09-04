// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_AURA_H_
#define UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_AURA_H_

#include "base/time/time.h"
#include "ui/gfx/skia_util.h"

namespace ui {

constexpr int kOverlayScrollbarThumbWidthNormal = 6;
constexpr int kOverlayScrollbarThumbWidthHovered = 10;
constexpr int kOverlayScrollbarThumbWidthPressed = 10;

constexpr base::TimeDelta kOverlayScrollbarFadeOutDelay =
    base::TimeDelta::FromMilliseconds(1000);
constexpr base::TimeDelta kOverlayScrollbarFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);
// TODO(bokan): This is still undetermined. crbug.com/652520.
constexpr base::TimeDelta kOverlayScrollbarThinningDuration =
    base::TimeDelta::FromMilliseconds(200);

} // namespace ui

#endif // UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_AURA_H_
