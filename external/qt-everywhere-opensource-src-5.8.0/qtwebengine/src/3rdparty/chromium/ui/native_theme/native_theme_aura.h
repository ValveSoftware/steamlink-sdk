// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
#define UI_NATIVE_THEME_NATIVE_THEME_AURA_H_

#include "base/macros.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// Aura implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeAura : public NativeThemeBase {
 public:
  static NativeThemeAura* instance();

 protected:
  NativeThemeAura();
  ~NativeThemeAura() override;

  // Overridden from NativeThemeBase:
  SkColor GetSystemColor(ColorId color_id) const override;
  void PaintMenuPopupBackground(
      SkCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const override;
  void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& menu_item) const override;
  void PaintArrowButton(SkCanvas* gc,
                        const gfx::Rect& rect,
                        Part direction,
                        State state) const override;
  void PaintScrollbarTrack(SkCanvas* canvas,
                           Part part,
                           State state,
                           const ScrollbarTrackExtraParams& extra_params,
                           const gfx::Rect& rect) const override;
  void PaintScrollbarThumb(SkCanvas* canvas,
                           Part part,
                           State state,
                           const gfx::Rect& rect) const override;
  void PaintScrollbarCorner(SkCanvas* canvas,
                            State state,
                            const gfx::Rect& rect) const override;

  void PaintScrollbarThumbStateTransition(SkCanvas* canvas,
                                          Part part,
                                          State startState,
                                          State endState,
                                          double progress,
                                          const gfx::Rect& rect) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeThemeAura);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
