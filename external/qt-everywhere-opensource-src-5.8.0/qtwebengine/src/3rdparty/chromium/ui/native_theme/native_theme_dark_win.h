// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_DARK_WIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_DARK_WIN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/native_theme/native_theme_win.h"

namespace ui {

class NATIVE_THEME_EXPORT NativeThemeDarkWin : public NativeThemeWin {
 public:
  static NativeThemeDarkWin* instance();

 private:
  NativeThemeDarkWin();
  ~NativeThemeDarkWin() override;

  // Overridden from NativeThemeBase:
  SkColor GetSystemColor(ColorId color_id) const override;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeDarkWin);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_DARK_WIN_H_
