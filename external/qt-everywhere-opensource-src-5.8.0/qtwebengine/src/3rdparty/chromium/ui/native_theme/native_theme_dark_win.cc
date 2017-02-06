// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_dark_win.h"

#include "ui/base/material_design/material_design_controller.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace ui {

NativeThemeDarkWin* NativeThemeDarkWin::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeDarkWin, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeDarkWin::NativeThemeDarkWin() {}

NativeThemeDarkWin::~NativeThemeDarkWin() {}

SkColor NativeThemeDarkWin::GetSystemColor(ColorId color_id) const {
  if (!ui::MaterialDesignController::IsModeMaterial())
    return NativeThemeWin::GetSystemColor(color_id);

  return NativeThemeDarkAura::instance()->GetSystemColor(color_id);
}

}  // namespace ui
