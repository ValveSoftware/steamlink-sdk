// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aurawin.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_win.h"

namespace ui {

namespace {

bool IsScrollbarPart(NativeTheme::Part part) {
  switch (part) {
    case NativeTheme::kScrollbarDownArrow:
    case NativeTheme::kScrollbarLeftArrow:
    case NativeTheme::kScrollbarRightArrow:
    case NativeTheme::kScrollbarUpArrow:
    case NativeTheme::kScrollbarHorizontalThumb:
    case NativeTheme::kScrollbarVerticalThumb:
    case NativeTheme::kScrollbarHorizontalTrack:
    case NativeTheme::kScrollbarVerticalTrack:
    case NativeTheme::kScrollbarHorizontalGripper:
    case NativeTheme::kScrollbarVerticalGripper:
    case NativeTheme::kScrollbarCorner:
      return true;
    default:
      return false;
  }
}

}  // namespace

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeAuraWin::instance();
}

// static
NativeThemeAura* NativeThemeAura::instance() {
  return NativeThemeAuraWin::instance();
}

// static
NativeThemeAuraWin* NativeThemeAuraWin::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAuraWin, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeAuraWin::NativeThemeAuraWin() {
}

NativeThemeAuraWin::~NativeThemeAuraWin() {
}

void NativeThemeAuraWin::Paint(SkCanvas* canvas,
                               Part part,
                               State state,
                               const gfx::Rect& rect,
                               const ExtraParams& extra) const {
  if (IsScrollbarPart(part) &&
      NativeThemeWin::instance()->IsUsingHighContrastTheme()) {
    NativeThemeWin::instance()->Paint(canvas, part, state, rect, extra);
    return;
  }

  NativeThemeAura::Paint(canvas, part, state, rect, extra);
}

gfx::Size NativeThemeAuraWin::GetPartSize(Part part,
                                          State state,
                                          const ExtraParams& extra) const {
  // We want aura on windows to use the same size for scrollbars as we would in
  // the native theme.
  if (IsScrollbarPart(part))
    return NativeThemeWin::instance()->GetPartSize(part, state, extra);

  return NativeThemeAura::GetPartSize(part, state, extra);
}

}  // namespace ui
