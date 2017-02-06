// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_AURAWIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_AURAWIN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/native_theme/native_theme_aura.h"

namespace ui {

// Aura implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeAuraWin : public NativeThemeAura {
 public:
  static NativeThemeAuraWin* instance();

 private:
  NativeThemeAuraWin();
  ~NativeThemeAuraWin() override;

  // Overridden from NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(SkCanvas* canvas,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra) const override;
  DISALLOW_COPY_AND_ASSIGN(NativeThemeAuraWin);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_AURAWIN_H_
