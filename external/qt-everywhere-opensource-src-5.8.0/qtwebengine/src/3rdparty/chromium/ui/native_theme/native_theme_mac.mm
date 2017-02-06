// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

namespace {

// Values calculated by reading pixels and solving simultaneous equations
// derived from "A over B" alpha compositing. Steps: Sample the semi-transparent
// pixel over two backgrounds; P1, P2 over backgrounds B1, B2. Use the color
// value between 0.0 and 1.0 (i.e. divide by 255.0). Then,
// alpha = (P2 - P1 + B1 - B2) / (B1 - B2)
// color = (P1 - B1 + alpha * B1) / alpha.
const SkColor kMenuPopupBackgroundColor = SkColorSetARGB(245, 255, 255, 255);
const SkColor kMenuSeparatorColor = SkColorSetARGB(255, 217, 217, 217);
const SkColor kMenuBorderColor = SkColorSetARGB(60, 0, 0, 0);

const SkColor kMenuPopupBackgroundColorMavericks =
    SkColorSetARGB(255, 255, 255, 255);
const SkColor kMenuSeparatorColorMavericks = SkColorSetARGB(243, 228, 228, 228);

// Hardcoded color used for some existing dialogs in Chrome's Cocoa UI.
const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);

// Color for the highlighted text in a control when that control doesn't have
// keyboard focus.
const SkColor kUnfocusedSelectedTextBackgroundColor =
    SkColorSetRGB(220, 220, 220);

// Helper to make indexing an array by an enum class easier.
template <class KEY, class VALUE>
struct EnumArray {
  VALUE& operator[](const KEY& key) { return array[static_cast<size_t>(key)]; }
  VALUE array[static_cast<size_t>(KEY::COUNT)];
};

// NSColor has a number of methods that return system colors (i.e. controlled by
// user preferences). This function converts the color given by an NSColor class
// method to an SkColor. Official documentation suggests developers only rely on
// +[NSColor selectedTextBackgroundColor] and +[NSColor selectedControlColor],
// but other colors give a good baseline. For many, a gradient is involved; the
// palette chosen based on the enum value given by +[NSColor currentColorTint].
// Apple's documentation also suggests to use NSColorList, but the system color
// list is just populated with class methods on NSColor.
SkColor NSSystemColorToSkColor(NSColor* color) {
  // System colors use the an NSNamedColorSpace called "System", so first step
  // is to convert the color into something that can be worked with.
  NSColor* device_color =
      [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
  if (device_color)
    return skia::NSDeviceColorToSkColor(device_color);

  // Sometimes the conversion is not possible, but we can get an approximation
  // by going through a CGColorRef. Note that simply using NSColor methods for
  // accessing components for system colors results in exceptions like
  // "-numberOfComponents not valid for the NSColor NSNamedColorSpace System
  // windowBackgroundColor; need to first convert colorspace." Hence the
  // conversion first to CGColor.
  CGColorRef cg_color = [color CGColor];
  const size_t component_count = CGColorGetNumberOfComponents(cg_color);
  if (component_count == 4)
    return skia::CGColorRefToSkColor(cg_color);

  CHECK(component_count == 1 || component_count == 2);
  // 1-2 components means a grayscale channel and maybe an alpha channel, which
  // CGColorRefToSkColor will not like. But RGB is additive, so the conversion
  // is easy (RGB to grayscale is less easy).
  const CGFloat* components = CGColorGetComponents(cg_color);
  CGFloat alpha = component_count == 2 ? components[1] : 1.0;
  return SkColorSetARGB(SkScalarRoundToInt(255.0 * alpha),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]));
}

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeMac::instance();
}

// static
NativeThemeMac* NativeThemeMac::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeMac, s_native_theme, ());
  return &s_native_theme;
}

SkColor NativeThemeMac::GetSystemColor(ColorId color_id) const {
  // TODO(tapted): Add caching for these, and listen for
  // NSSystemColorsDidChangeNotification.
  switch (color_id) {
    case kColorId_WindowBackground:
      return NSSystemColorToSkColor([NSColor windowBackgroundColor]);
    case kColorId_DialogBackground:
      return kDialogBackgroundColor;
    case kColorId_BubbleBackground:
      return SK_ColorWHITE;

    case kColorId_FocusedBorderColor:
    case kColorId_FocusedMenuButtonBorderColor:
      return NSSystemColorToSkColor([NSColor keyboardFocusIndicatorColor]);
    case kColorId_UnfocusedBorderColor:
      return NSSystemColorToSkColor([NSColor controlColor]);

    // Buttons and labels.
    case kColorId_ButtonBackgroundColor:
    case kColorId_ButtonHoverBackgroundColor:
    case kColorId_HoverMenuButtonBorderColor:
    case kColorId_LabelBackgroundColor:
      return NSSystemColorToSkColor([NSColor controlBackgroundColor]);
    case kColorId_ButtonEnabledColor:
    case kColorId_EnabledMenuButtonBorderColor:
    case kColorId_LabelEnabledColor:
      return NSSystemColorToSkColor([NSColor controlTextColor]);
    case kColorId_ButtonDisabledColor:
    case kColorId_LabelDisabledColor:
      return NSSystemColorToSkColor([NSColor disabledControlTextColor]);
    case kColorId_ButtonHighlightColor:
      // Although the NSColor documentation names "selectedControlTextColor" as
      // the color for a "text in a .. control being clicked or dragged", it
      // remains black, and text on Yosemite-style pressed buttons is white.
      return SK_ColorWHITE;
    case kColorId_ButtonHoverColor:
      return NSSystemColorToSkColor([NSColor selectedControlTextColor]);

    // Menus.
    case kColorId_EnabledMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor controlTextColor]);
    case kColorId_DisabledMenuItemForegroundColor:
    case kColorId_DisabledEmphasizedMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor disabledControlTextColor]);
    case kColorId_SelectedMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor selectedMenuItemTextColor]);
    case kColorId_FocusedMenuItemBackgroundColor:
    case kColorId_HoverMenuItemBackgroundColor:
      return NSSystemColorToSkColor([NSColor selectedMenuItemColor]);
    case kColorId_MenuBackgroundColor:
      return kMenuPopupBackgroundColor;
    case kColorId_MenuSeparatorColor:
      return base::mac::IsOSMavericks() ? kMenuSeparatorColorMavericks
                                        : kMenuSeparatorColor;
    case kColorId_MenuBorderColor:
      return kMenuBorderColor;

    // Link.
    case kColorId_LinkDisabled:
      return SK_ColorBLACK;
    case kColorId_LinkEnabled:
      return SK_ColorBLUE;
    case kColorId_LinkPressed:
      return SK_ColorRED;

    // Text fields.
    case kColorId_TextfieldDefaultColor:
    case kColorId_TextfieldReadOnlyColor:
      return NSSystemColorToSkColor([NSColor textColor]);
    case kColorId_TextfieldDefaultBackground:
    case kColorId_TextfieldReadOnlyBackground:
      return NSSystemColorToSkColor([NSColor textBackgroundColor]);
    case kColorId_TextfieldSelectionColor:
      return NSSystemColorToSkColor([NSColor selectedTextColor]);
    case kColorId_TextfieldSelectionBackgroundFocused:
      return NSSystemColorToSkColor([NSColor selectedTextBackgroundColor]);

    // Trees/Tables. For focused text, use the alternate* versions, which
    // NSColor documents as "the table and list view equivalent to the
    // selectedControlTextColor".
    case kColorId_TreeBackground:
    case kColorId_TableBackground:
      return NSSystemColorToSkColor([NSColor controlBackgroundColor]);
    case kColorId_TreeText:
    case kColorId_TableText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TreeSelectedTextUnfocused:
      return NSSystemColorToSkColor([NSColor textColor]);
    case kColorId_TreeSelectedText:
    case kColorId_TableSelectedText:
      return NSSystemColorToSkColor(
          [NSColor alternateSelectedControlTextColor]);
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundFocused:
      return NSSystemColorToSkColor([NSColor alternateSelectedControlColor]);
    case kColorId_TreeSelectionBackgroundUnfocused:
    case kColorId_TableSelectionBackgroundUnfocused:
      return kUnfocusedSelectedTextBackgroundColor;
    case kColorId_TreeArrow:
    case kColorId_TableGroupingIndicatorColor:
      return SkColorSetRGB(140, 140, 140);

    default:
      // TODO(tapted): Handle all values and remove the default case.
      return GetAuraColor(color_id, this);
  }
}

void NativeThemeMac::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  SkPaint paint;
  paint.setAntiAlias(true);
  if (base::mac::IsOSMavericks())
    paint.setColor(kMenuPopupBackgroundColorMavericks);
  else
    paint.setColor(kMenuPopupBackgroundColor);
  const SkScalar radius = SkIntToScalar(menu_background.corner_radius);
  SkRect rect = gfx::RectToSkRect(gfx::Rect(size));
  canvas->drawRoundRect(rect, radius, radius, paint);
}

void NativeThemeMac::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  SkPaint paint;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      // Draw nothing over the regular background.
      break;
    case NativeTheme::kHovered:
      // TODO(tapted): Draw a gradient, and use [NSColor currentControlTint] to
      // pick colors. The System color "selectedMenuItemColor" is actually still
      // blue for Graphite. And while "keyboardFocusIndicatorColor" does change,
      // and is a good shade of gray, it's not blue enough for the Blue theme.
      paint.setColor(GetSystemColor(kColorId_HoverMenuItemBackgroundColor));
      canvas->drawRect(gfx::RectToSkRect(rect), paint);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// static
sk_sp<SkShader> NativeThemeMac::GetButtonBackgroundShader(
    ButtonBackgroundType type,
    int height) {
  using ColorByState = EnumArray<ButtonBackgroundType, SkColor>;
  SkPoint gradient_points[2];
  gradient_points[0].iset(0, 0);
  gradient_points[1].iset(0, height);

  SkScalar gradient_positions[] = { 0.0, 1.0 };

  // These hex values are directly from the detailed specs in
  // https://crbug.com/543683.
  const SkColor kGrey = SkColorSetRGB(0xf6, 0xf6, 0xf6);
  const SkColor kBlueStart = SkColorSetRGB(0x6b, 0xb3, 0xfa);
  const SkColor kBlueEnd = SkColorSetRGB(0x07, 0x7d, 0xff);
  const SkColor kPressedBlueStart = SkColorSetRGB(0x3e, 0x8b, 0xf6);
  const SkColor kPressedBlueEnd = SkColorSetRGB(0x03, 0x51, 0xff);

  ColorByState start_colors;
  start_colors[ButtonBackgroundType::DISABLED] = kGrey;
  start_colors[ButtonBackgroundType::HIGHLIGHTED] = kBlueStart;
  start_colors[ButtonBackgroundType::NORMAL] = SK_ColorWHITE;
  start_colors[ButtonBackgroundType::PRESSED] = kPressedBlueStart;

  ColorByState end_colors;
  end_colors[ButtonBackgroundType::DISABLED] = kGrey;
  end_colors[ButtonBackgroundType::HIGHLIGHTED] = kBlueEnd;
  end_colors[ButtonBackgroundType::NORMAL] = SK_ColorWHITE;
  end_colors[ButtonBackgroundType::PRESSED] = kPressedBlueEnd;

  SkColor gradient_colors[] = {start_colors[type], end_colors[type]};

  return SkGradientShader::MakeLinear(
      gradient_points, gradient_colors, gradient_positions,
      arraysize(gradient_positions),
      SkShader::kClamp_TileMode);
}

// static
sk_sp<SkShader> NativeThemeMac::GetButtonBorderShader(ButtonBackgroundType type,
                                                      int height) {
  using ColorByState = EnumArray<ButtonBackgroundType, SkColor>;
  SkPoint gradient_points[2];
  gradient_points[0].iset(0, 0);
  gradient_points[1].iset(0, height);

  // Two positions works well for pressed and highlighted, but the side edges of
  // disabled and normal are more heavily weighted at the top and bottom.
  // TODO(tapted): Use more positions for normal and disabled.
  SkScalar gradient_positions[] = {0.0, 1.0};

  ColorByState top_edge;
  top_edge[ButtonBackgroundType::DISABLED] = SkColorSetRGB(0xd2, 0xc2, 0xc2);
  top_edge[ButtonBackgroundType::HIGHLIGHTED] = SkColorSetRGB(0x6a, 0x9f, 0xff);
  top_edge[ButtonBackgroundType::NORMAL] = SkColorSetRGB(0xcc, 0xcc, 0xcc);
  top_edge[ButtonBackgroundType::PRESSED] = SkColorSetRGB(0x4f, 0x72, 0xfb);
  ColorByState bottom_edge;
  bottom_edge[ButtonBackgroundType::DISABLED] = SkColorSetRGB(0xbe, 0xbe, 0xbe);
  bottom_edge[ButtonBackgroundType::HIGHLIGHTED] =
      SkColorSetRGB(0x43, 0x52, 0xff);
  bottom_edge[ButtonBackgroundType::NORMAL] = SkColorSetRGB(0x9d, 0x9d, 0x9d);
  bottom_edge[ButtonBackgroundType::PRESSED] = SkColorSetRGB(0x3e, 0x12, 0xff);

  SkColor gradient_colors[] = {top_edge[type], bottom_edge[type]};

  return SkGradientShader::MakeLinear(
      gradient_points, gradient_colors, gradient_positions,
      arraysize(gradient_positions), SkShader::kClamp_TileMode);
}

// static
void NativeThemeMac::PaintStyledGradientButton(SkCanvas* canvas,
                                               const gfx::Rect& integer_bounds,
                                               ButtonBackgroundType type,
                                               bool round_left,
                                               bool round_right,
                                               bool focus) {
  const SkScalar kBorderThickness = 1;
  const SkScalar kFocusRingThickness = 4;
  const SkColor kFocusRingColor = SkColorSetARGB(0x94, 0x79, 0xa7, 0xe9);

  const SkVector kNoCurve = {0, 0};
  const SkVector kCurve = {kButtonCornerRadius, kButtonCornerRadius};
  const SkVector kLeftCurves[4] = {kCurve, kNoCurve, kNoCurve, kCurve};
  const SkVector kRightCurves[4] = {kNoCurve, kCurve, kCurve, kNoCurve};

  const SkScalar kShadowOffsetY = 1;
  const SkColor kShadowColor = SkColorSetA(SK_ColorBLACK, 0x05);
  const double kShadowBlur = 0.0;
  const std::vector<gfx::ShadowValue> shadows(
      1, gfx::ShadowValue(gfx::Vector2d(0, kShadowOffsetY), kShadowBlur,
                          kShadowColor));

  SkRect bounds = gfx::RectToSkRect(integer_bounds);

  // Inset to account for the focus ring. Note it draws over the border stroke.
  bounds.inset(kFocusRingThickness - kBorderThickness,
               kFocusRingThickness - kBorderThickness);

  SkRRect shape;
  if (round_left && round_right)
    shape.setRectXY(bounds, kButtonCornerRadius, kButtonCornerRadius);
  else if (round_left)
    shape.setRectRadii(bounds, kLeftCurves);
  else if (round_right)
    shape.setRectRadii(bounds, kRightCurves);
  else
    shape.setRect(bounds);

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  // First draw the darker "outer" border, with its gradient and shadow. Inside
  // a tab strip, this will draw over the outer border and inner separator.
  paint.setLooper(gfx::CreateShadowDrawLooper(shadows));
  paint.setShader(GetButtonBorderShader(type, shape.height()));
  canvas->drawRRect(shape, paint);

  // Then, inset the rounded rect and draw over that with the inner gradient.
  shape.inset(kBorderThickness, kBorderThickness);
  paint.setLooper(nullptr);
  paint.setShader(GetButtonBackgroundShader(type, shape.height()));
  canvas->drawRRect(shape, paint);

  if (!focus)
    return;

  SkRRect outer_shape;
  shape.outset(kFocusRingThickness, kFocusRingThickness, &outer_shape);
  paint.setShader(nullptr);
  paint.setColor(kFocusRingColor);
  canvas->drawDRRect(outer_shape, shape, paint);
}

NativeThemeMac::NativeThemeMac() {
}

NativeThemeMac::~NativeThemeMac() {
}

}  // namespace ui
