// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
#define UI_NATIVE_THEME_NATIVE_THEME_AURA_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/native_theme/fallback_theme.h"

namespace gfx {
class NineImagePainter;
}

namespace ui {

// Aura implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeAura : public FallbackTheme {
 public:
  static NativeThemeAura* instance();

 protected:
  NativeThemeAura();
  virtual ~NativeThemeAura();

  // Overridden from NativeThemeBase:
  virtual void PaintMenuPopupBackground(
      SkCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const OVERRIDE;
  virtual void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const OVERRIDE;
  virtual void PaintArrowButton(SkCanvas* gc,
                                const gfx::Rect& rect,
                                Part direction,
                                State state) const OVERRIDE;
  virtual void PaintScrollbarTrack(
      SkCanvas* sk_canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const OVERRIDE;
  virtual void PaintScrollbarThumb(SkCanvas* sk_canvas,
                                   Part part,
                                   State state,
                                   const gfx::Rect& rect) const OVERRIDE;
  virtual void PaintScrollbarCorner(SkCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect) const OVERRIDE;

  virtual void PaintScrollbarThumbStateTransition(SkCanvas* canvas,
                                                  State startState,
                                                  State endState,
                                                  double progress,
                                                  const gfx::Rect& rect) const
      OVERRIDE;

  // Returns the NineImagePainter used to paint the specified state, creating if
  // necessary. If no image is provided for the specified state the normal state
  // images are used.
  gfx::NineImagePainter* GetOrCreatePainter(
      const int image_ids[kMaxState][9],
      State state,
      scoped_ptr<gfx::NineImagePainter> painters[kMaxState]) const;

  // Paints |painter| into the canvas using |rect|.
  void PaintPainter(gfx::NineImagePainter* painter,
                    SkCanvas* sk_canvas,
                    const gfx::Rect& rect) const;

  mutable scoped_ptr<gfx::NineImagePainter> scrollbar_track_painter_;

  mutable scoped_ptr<gfx::NineImagePainter>
      scrollbar_thumb_painters_[kMaxState];

  mutable scoped_ptr<gfx::NineImagePainter>
      scrollbar_arrow_button_painters_[kMaxState];

 private:
  struct DualPainter {
    // For overlay scrollbar thumbs, fill and stroke are controlled separately,
    // and each state is achieved by painting with different opacity. This
    // struct bundles information of painter generated using assets and alpha
    // value associated with each state, so that a DualPainter for overlay
    // scrollbar thumb would only need state as input to paint correctly.
    DualPainter(scoped_ptr<gfx::NineImagePainter> fill_painter,
                const uint8 fill_alphas[kMaxState],
                scoped_ptr<gfx::NineImagePainter> stroke_painter,
                const uint8 stroke_alphas[kMaxState]);
    ~DualPainter();

    scoped_ptr<gfx::NineImagePainter> fill_painter;
    const uint8* const fill_alphas;
    scoped_ptr<gfx::NineImagePainter> stroke_painter;
    const uint8* const stroke_alphas;
  };

  // Returns DualPainter from specific fill and stroke, creating if necessary.
  scoped_ptr<DualPainter> CreateDualPainter(
      const int fill_image_ids[9],
      const uint8 fill_alphas[kMaxState],
      const int stroke_image_ids[9],
      const uint8 stroke_alphas[kMaxState]) const;

  // Paints |dualPainter| into the canvas using |rect| and specific alpha.
  void PaintDualPainter(DualPainter* dual_painter,
                        SkCanvas* sk_canvas,
                        const gfx::Rect& rect,
                        State state) const;

  void PaintDualPainterTransition(DualPainter* dual_painter,
                                  SkCanvas* sk_canvas,
                                  const gfx::Rect& rect,
                                  State startState,
                                  State endState,
                                  double progress) const;

  mutable scoped_ptr<DualPainter> scrollbar_overlay_thumb_painter_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeAura);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
