// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <limits>

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_switches.h"

using gfx::NineImagePainter;

#define EMPTY_IMAGE_GRID { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

namespace ui {

namespace {

const int kScrollbarThumbImages[NativeTheme::kMaxState][9] = {
  EMPTY_IMAGE_GRID,
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_HOVER),
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_NORMAL),
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_PRESSED)
};

const int kScrollbarArrowButtonImages[NativeTheme::kMaxState][9] = {
  EMPTY_IMAGE_GRID,
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_HOVER),
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_NORMAL),
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_PRESSED)
};

const uint8 kScrollbarOverlayThumbFillAlphas[NativeTheme::kMaxState] = {
  0,    // Does not matter, will not paint for disabled state.
  178,  // Hover state, opacity 70%, alpha would be 0.7 * 255.
  140,  // Normal state, opacity 55%, alpha would be 0.55 * 255.
  178   // Pressed state, opacity 70%, alpha would be 0.7 * 255.
};

const uint8 kScrollbarOverlayThumbStrokeAlphas[NativeTheme::kMaxState] = {
  0,   // Does not matter, will not paint for disabled state.
  51,  // Hover state, opacity 20%, alpha would be 0.2 * 255.
  38,  // Normal state, opacity 15%, alpha would be 0.15 * 255.
  51   // Pressed state, opacity 20%, alpha would be 0.2 * 255.
};

const int kScrollbarOverlayThumbStrokeImages[9] =
    IMAGE_GRID_NO_CENTER(IDR_SCROLLBAR_OVERLAY_THUMB_STROKE);

const int kScrollbarOverlayThumbFillImages[9] =
    IMAGE_GRID(IDR_SCROLLBAR_OVERLAY_THUMB_FILL);

const int kScrollbarTrackImages[9] = IMAGE_GRID(IDR_SCROLLBAR_BASE);

}  // namespace

#if !defined(OS_WIN)
// static
NativeTheme* NativeTheme::instance() {
  return NativeThemeAura::instance();
}

// static
NativeThemeAura* NativeThemeAura::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAura, s_native_theme, ());
  return &s_native_theme;
}
#endif

NativeThemeAura::NativeThemeAura() {
  // We don't draw scrollbar buttons.
#if defined(OS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif

  // Images and alphas declarations assume the following order.
  COMPILE_ASSERT(kDisabled == 0, states_unexepctedly_changed);
  COMPILE_ASSERT(kHovered == 1, states_unexepctedly_changed);
  COMPILE_ASSERT(kNormal == 2, states_unexepctedly_changed);
  COMPILE_ASSERT(kPressed == 3, states_unexepctedly_changed);
  COMPILE_ASSERT(kMaxState == 4, states_unexepctedly_changed);
}

NativeThemeAura::~NativeThemeAura() {
}

void NativeThemeAura::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  SkColor color = GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor);
  if (menu_background.corner_radius > 0) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(color);

    gfx::Path path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, paint);
  } else {
    canvas->drawColor(color, SkXfermode::kSrc_Mode);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  CommonThemePaintMenuItemBackground(canvas, state, rect);
}

void NativeThemeAura::PaintArrowButton(
      SkCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state) const {
  if (direction == kInnerSpinButton) {
    FallbackTheme::PaintArrowButton(gc, rect, direction, state);
    return;
  }
  PaintPainter(GetOrCreatePainter(
                   kScrollbarArrowButtonImages, state,
                   scrollbar_arrow_button_painters_),
               gc, rect);

  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = GetArrowColor(state);
  switch (state) {
    case kHovered:
    case kNormal:
      arrow_color = SkColorSetRGB(0x50, 0x50, 0x50);
      break;
    case kPressed:
      arrow_color = SK_ColorWHITE;
    default:
      break;
  }
  PaintArrow(gc, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    SkCanvas* sk_canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!IsOverlayScrollbarEnabled());
  if (!scrollbar_track_painter_)
    scrollbar_track_painter_ = CreateNineImagePainter(kScrollbarTrackImages);
  PaintPainter(scrollbar_track_painter_.get(), sk_canvas, rect);
}

void NativeThemeAura::PaintScrollbarThumb(SkCanvas* sk_canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect) const {
  gfx::Rect thumb_rect(rect);
  if (IsOverlayScrollbarEnabled()) {
    // Overlay scrollbar has no track, just paint thumb directly.
    // Do not paint if state is disabled.
    if (state == kDisabled)
      return;

    if (!scrollbar_overlay_thumb_painter_) {
      scrollbar_overlay_thumb_painter_ =
          CreateDualPainter(kScrollbarOverlayThumbFillImages,
                            kScrollbarOverlayThumbFillAlphas,
                            kScrollbarOverlayThumbStrokeImages,
                            kScrollbarOverlayThumbStrokeAlphas);
    }

    PaintDualPainter(
        scrollbar_overlay_thumb_painter_.get(), sk_canvas, thumb_rect, state);
    return;
  }
  // If there are no scrollbuttons then provide some padding so that thumb
  // doesn't touch the top of the track.
  const int extra_padding = (scrollbar_button_length() == 0) ? 2 : 0;
  if (part == NativeTheme::kScrollbarVerticalThumb)
    thumb_rect.Inset(2, extra_padding, 2, extra_padding);
  else
    thumb_rect.Inset(extra_padding, 2, extra_padding, 2);
  PaintPainter(GetOrCreatePainter(
                   kScrollbarThumbImages, state, scrollbar_thumb_painters_),
               sk_canvas,
               thumb_rect);
}

void NativeThemeAura::PaintScrollbarThumbStateTransition(
    SkCanvas* canvas,
    State startState,
    State endState,
    double progress,
    const gfx::Rect& rect) const {
  // Only Overlay scrollbars should have state transition animation.
  DCHECK(IsOverlayScrollbarEnabled());
  if (!scrollbar_overlay_thumb_painter_) {
    scrollbar_overlay_thumb_painter_ =
        CreateDualPainter(kScrollbarOverlayThumbFillImages,
                          kScrollbarOverlayThumbFillAlphas,
                          kScrollbarOverlayThumbStrokeImages,
                          kScrollbarOverlayThumbStrokeAlphas);
  }

  PaintDualPainterTransition(scrollbar_overlay_thumb_painter_.get(),
                             canvas,
                             rect,
                             startState,
                             endState,
                             progress);
}

void NativeThemeAura::PaintScrollbarCorner(SkCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!IsOverlayScrollbarEnabled());
  SkPaint paint;
  paint.setColor(SkColorSetRGB(0xF1, 0xF1, 0xF1));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  canvas->drawIRect(RectToSkIRect(rect), paint);
}

NineImagePainter* NativeThemeAura::GetOrCreatePainter(
    const int images[kMaxState][9],
    State state,
    scoped_ptr<NineImagePainter> painters[kMaxState]) const {
  if (painters[state])
    return painters[state].get();
  if (images[state][0] == 0) {
    // Must always provide normal state images.
    DCHECK_NE(kNormal, state);
    return GetOrCreatePainter(images, kNormal, painters);
  }
  painters[state] = CreateNineImagePainter(images[state]);
  return painters[state].get();
}

void NativeThemeAura::PaintPainter(NineImagePainter* painter,
                                   SkCanvas* sk_canvas,
                                   const gfx::Rect& rect) const {
  DCHECK(painter);
  scoped_ptr<gfx::Canvas> canvas(CommonThemeCreateCanvas(sk_canvas));
  painter->Paint(canvas.get(), rect);
}

scoped_ptr<NativeThemeAura::DualPainter> NativeThemeAura::CreateDualPainter(
    const int fill_image_ids[9],
    const uint8 fill_alphas[kMaxState],
    const int stroke_image_ids[9],
    const uint8 stroke_alphas[kMaxState]) const {
  scoped_ptr<NativeThemeAura::DualPainter> dual_painter(
      new NativeThemeAura::DualPainter(CreateNineImagePainter(fill_image_ids),
                                       fill_alphas,
                                       CreateNineImagePainter(stroke_image_ids),
                                       stroke_alphas));
  return dual_painter.Pass();
}

void NativeThemeAura::PaintDualPainter(
    NativeThemeAura::DualPainter* dual_painter,
    SkCanvas* sk_canvas,
    const gfx::Rect& rect,
    State state) const {
  DCHECK(dual_painter);
  scoped_ptr<gfx::Canvas> canvas(CommonThemeCreateCanvas(sk_canvas));
  dual_painter->fill_painter->Paint(
      canvas.get(), rect, dual_painter->fill_alphas[state]);
  dual_painter->stroke_painter->Paint(
      canvas.get(), rect, dual_painter->stroke_alphas[state]);
}

void NativeThemeAura::PaintDualPainterTransition(
    NativeThemeAura::DualPainter* dual_painter,
    SkCanvas* sk_canvas,
    const gfx::Rect& rect,
    State startState,
    State endState,
    double progress) const {
  DCHECK(dual_painter);
  scoped_ptr<gfx::Canvas> canvas(CommonThemeCreateCanvas(sk_canvas));
  uint8 fill_alpha = dual_painter->fill_alphas[startState] +
                     (dual_painter->fill_alphas[endState] -
                      dual_painter->fill_alphas[startState]) *
                         progress;
  uint8 stroke_alpha = dual_painter->stroke_alphas[startState] +
                       (dual_painter->stroke_alphas[endState] -
                        dual_painter->stroke_alphas[startState]) *
                           progress;

  dual_painter->fill_painter->Paint(canvas.get(), rect, fill_alpha);
  dual_painter->stroke_painter->Paint(canvas.get(), rect, stroke_alpha);
}

NativeThemeAura::DualPainter::DualPainter(
    scoped_ptr<NineImagePainter> fill_painter,
    const uint8 fill_alphas[kMaxState],
    scoped_ptr<NineImagePainter> stroke_painter,
    const uint8 stroke_alphas[kMaxState])
    : fill_painter(fill_painter.Pass()),
      fill_alphas(fill_alphas),
      stroke_painter(stroke_painter.Pass()),
      stroke_alphas(stroke_alphas) {}

NativeThemeAura::DualPainter::~DualPainter() {}

}  // namespace ui
