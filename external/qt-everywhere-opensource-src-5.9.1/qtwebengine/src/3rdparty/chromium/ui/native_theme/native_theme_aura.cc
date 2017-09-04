// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {

// Constants for painting overlay scrollbars. Other properties needed outside
// this painting code are defined in overlay_scrollbar_constants_aura.h.
constexpr int kOverlayScrollbarStrokeWidth = 1;
constexpr int kOverlayScrollbarMinimumLength = 12;
constexpr SkAlpha kOverlayScrollbarAlphaNormal = 0x4D;
constexpr SkAlpha kOverlayScrollbarAlphaHovered = 0x80;
constexpr SkAlpha kOverlayScrollbarAlphaPressed = 0x80;

// Indexed by ScrollbarOverlayColorTheme.
constexpr SkColor kOverlayScrollbarThumbColor[] = {SK_ColorBLACK,
                                                   SK_ColorWHITE};
constexpr SkColor kOverlayScrollbarStrokeColor[] = {SK_ColorWHITE,
                                                    SK_ColorBLACK};

const SkColor kTrackColor = SkColorSetRGB(0xF1, 0xF1, 0xF1);

}  // namespace

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeAura::web_instance();
}

#if !defined(OS_WIN)
// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeAura::instance();
}
#endif

// static
NativeThemeAura* NativeThemeAura::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAura, s_native_theme, (false));
  return &s_native_theme;
}

// static
NativeThemeAura* NativeThemeAura::web_instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAura, s_native_theme_for_web,
                         (IsOverlayScrollbarEnabled()));
  return &s_native_theme_for_web;
}

NativeThemeAura::NativeThemeAura(bool use_overlay_scrollbars)
    : use_overlay_scrollbars_(use_overlay_scrollbars) {
// We don't draw scrollbar buttons.
#if defined(OS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif

  if (use_overlay_scrollbars_) {
    scrollbar_width_ =
        kOverlayScrollbarThumbWidthPressed + kOverlayScrollbarStrokeWidth * 2;
  }

  // Images and alphas declarations assume the following order.
  static_assert(kDisabled == 0, "states unexpectedly changed");
  static_assert(kHovered == 1, "states unexpectedly changed");
  static_assert(kNormal == 2, "states unexpectedly changed");
  static_assert(kPressed == 3, "states unexpectedly changed");
}

NativeThemeAura::~NativeThemeAura() {}

// This implementation returns hardcoded colors.
SkColor NativeThemeAura::GetSystemColor(ColorId color_id) const {
  return GetAuraColor(color_id, this);
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
    canvas->drawColor(color, SkBlendMode::kSrc);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  CommonThemePaintMenuItemBackground(this, canvas, state, rect, menu_item);
}

void NativeThemeAura::PaintArrowButton(SkCanvas* canvas,
                                       const gfx::Rect& rect,
                                       Part direction,
                                       State state) const {
  SkColor bg_color = kTrackColor;
  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = gfx::kPlaceholderColor;
  switch (state) {
    case kDisabled:
      arrow_color = GetArrowColor(state);
      break;
    case kHovered:
      bg_color = SkColorSetRGB(0xD2, 0xD2, 0xD2);
    // Fall through.
    case kNormal:
      arrow_color = SkColorSetRGB(0x50, 0x50, 0x50);
      break;
    case kPressed:
      bg_color = SkColorSetRGB(0x78, 0x78, 0x78);
      arrow_color = SK_ColorWHITE;
      break;
    case kNumStates:
      break;
  }
  DCHECK_NE(arrow_color, gfx::kPlaceholderColor);

  SkPaint paint;
  paint.setColor(bg_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), paint);

  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    SkCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!use_overlay_scrollbars_);
  SkPaint paint;
  paint.setColor(kTrackColor);
  canvas->drawIRect(gfx::RectToSkIRect(rect), paint);
}

void NativeThemeAura::PaintScrollbarThumb(
    SkCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    ScrollbarOverlayColorTheme theme) const {
  // Do not paint if state is disabled.
  if (state == kDisabled)
    return;

  TRACE_EVENT0("blink", "NativeThemeAura::PaintScrollbarThumb");

  SkAlpha thumb_alpha = SK_AlphaTRANSPARENT;
  const bool overlay = use_overlay_scrollbars_;
  switch (state) {
    case NativeTheme::kDisabled:
      thumb_alpha = SK_AlphaTRANSPARENT;
      break;
    case NativeTheme::kHovered:
      thumb_alpha = overlay ? kOverlayScrollbarAlphaHovered : 0x4D;
      break;
    case NativeTheme::kNormal:
      thumb_alpha = overlay ? kOverlayScrollbarAlphaNormal : 0x33;
      break;
    case NativeTheme::kPressed:
      thumb_alpha = overlay ? kOverlayScrollbarAlphaPressed : 0x80;
      break;
    case NativeTheme::kNumStates:
      NOTREACHED();
      break;
  }

  gfx::Rect thumb_rect(rect);
  SkColor thumb_color;
  if (overlay) {
    thumb_color = kOverlayScrollbarThumbColor[theme];

    // In overlay mode, draw a stroke (border).
    constexpr int kStrokeWidth = kOverlayScrollbarStrokeWidth;
    SkPaint paint;
    paint.setColor(
        SkColorSetA(kOverlayScrollbarStrokeColor[theme], thumb_alpha));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(kStrokeWidth);

    gfx::RectF stroke_rect(thumb_rect);
    constexpr float kHalfStrokeWidth = kStrokeWidth / 2.f;
    stroke_rect.Inset(kHalfStrokeWidth, kHalfStrokeWidth);
    canvas->drawRect(gfx::RectFToSkRect(stroke_rect), paint);

    // Inset the all the edges edges so we fill-in the stroke below.
    thumb_rect.Inset(kStrokeWidth, kStrokeWidth);
  } else {
    // If there are no scrollbuttons then provide some padding so that the thumb
    // doesn't touch the top of the track.
    const int kThumbPadding = 2;
    const int extra_padding =
        (scrollbar_button_length() == 0) ? kThumbPadding : 0;
    if (part == NativeTheme::kScrollbarVerticalThumb)
      thumb_rect.Inset(kThumbPadding, extra_padding);
    else
      thumb_rect.Inset(extra_padding, kThumbPadding);

    thumb_color = SK_ColorBLACK;
  }

  SkPaint paint;
  paint.setColor(SkColorSetA(thumb_color, thumb_alpha));
  canvas->drawIRect(gfx::RectToSkIRect(thumb_rect), paint);
}

void NativeThemeAura::PaintScrollbarCorner(SkCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!use_overlay_scrollbars_);
  SkPaint paint;
  paint.setColor(SkColorSetRGB(0xDC, 0xDC, 0xDC));
  canvas->drawIRect(RectToSkIRect(rect), paint);
}

gfx::Size NativeThemeAura::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  if (use_overlay_scrollbars_) {
    constexpr int minimum_length =
        kOverlayScrollbarMinimumLength + 2 * kOverlayScrollbarStrokeWidth;

    // Aura overlay scrollbars need a slight tweak from the base sizes.
    switch (part) {
      case kScrollbarHorizontalThumb:
        return gfx::Size(minimum_length, scrollbar_width_);
      case kScrollbarVerticalThumb:
        return gfx::Size(scrollbar_width_, minimum_length);

      default:
        // TODO(bokan): We should probably make sure code using overlay
        // scrollbars isn't asking for part sizes that don't exist.
        // crbug.com/657159.
        break;
    }
  }

  return NativeThemeBase::GetPartSize(part, state, extra);
}

}  // namespace ui
