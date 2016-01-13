// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#include "base/basictypes.h"
#include "ui/native_theme/common_theme.h"

namespace {

const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::instance() {
  return NativeThemeMac::instance();
}

// static
NativeThemeMac* NativeThemeMac::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeMac, s_native_theme, ());
  return &s_native_theme;
}

SkColor NativeThemeMac::GetSystemColor(ColorId color_id) const {
  SkColor color;
  if (CommonThemeGetSystemColor(color_id, &color))
    return color;

  switch (color_id) {
    case kColorId_DialogBackground:
      return kDialogBackgroundColor;
    default:
      NOTIMPLEMENTED() << " Invalid color_id: " << color_id;
      return FallbackTheme::GetSystemColor(color_id);
  }

  return kInvalidColorIdColor;
}

NativeThemeMac::NativeThemeMac() {
}

NativeThemeMac::~NativeThemeMac() {
}

}  // namespace ui
