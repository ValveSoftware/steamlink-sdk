// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"

namespace views {

namespace {

// Minimum size to reserve for the button contents.
const int kMinWidth = 48;

LabelButton* CreateButton(ButtonListener* listener,
                          const base::string16& text,
                          bool md) {
  if (md)
    return MdTextButton::Create(listener, text);

  LabelButton* button = new LabelButton(listener, text);
  button->SetStyle(CustomButton::STYLE_BUTTON);
  return button;
}

const gfx::FontList& GetMdFontList() {
  static base::LazyInstance<gfx::FontList>::Leaky font_list =
      LAZY_INSTANCE_INITIALIZER;
  const gfx::Font::Weight min_weight = gfx::Font::Weight::MEDIUM;
  if (font_list.Get().GetFontWeight() < min_weight)
    font_list.Get() = font_list.Get().DeriveWithWeight(min_weight);
  return font_list.Get();
}

}  // namespace

// static
LabelButton* MdTextButton::CreateSecondaryUiButton(ButtonListener* listener,
                                                   const base::string16& text) {
  return CreateButton(listener, text,
                      ui::MaterialDesignController::IsSecondaryUiMaterial());
}

// static
LabelButton* MdTextButton::CreateSecondaryUiBlueButton(
    ButtonListener* listener,
    const base::string16& text) {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    MdTextButton* md_button = MdTextButton::Create(listener, text);
    md_button->SetProminent(true);
    return md_button;
  }

  return new BlueButton(listener, text);
}

// static
MdTextButton* MdTextButton::Create(ButtonListener* listener,
                                   const base::string16& text) {
  MdTextButton* button = new MdTextButton(listener);
  button->SetText(text);
  button->SetFocusForPlatform();
  return button;
}

MdTextButton::~MdTextButton() {}

void MdTextButton::SetProminent(bool is_prominent) {
  if (is_prominent_ == is_prominent)
    return;

  is_prominent_ = is_prominent;
  UpdateColors();
}

void MdTextButton::SetBgColorOverride(const base::Optional<SkColor>& color) {
  bg_color_override_ = color;
  UpdateColors();
}

void MdTextButton::OnPaintBackground(gfx::Canvas* canvas) {
  LabelButton::OnPaintBackground(canvas);
  if (hover_animation().is_animating() || state() == STATE_HOVERED) {
    const int kHoverAlpha = is_prominent_ ? 0x0c : 0x05;
    SkScalar alpha = hover_animation().CurrentValueBetween(0, kHoverAlpha);
    canvas->FillRect(GetLocalBounds(), SkColorSetA(SK_ColorBLACK, alpha));
  }
}

void MdTextButton::OnFocus() {
  LabelButton::OnFocus();
  FocusRing::Install(this);
}

void MdTextButton::OnBlur() {
  LabelButton::OnBlur();
  FocusRing::Uninstall(this);
}

void MdTextButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  LabelButton::OnNativeThemeChanged(theme);
  UpdateColors();
}

SkColor MdTextButton::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(label()->enabled_color());
}

std::unique_ptr<InkDrop> MdTextButton::CreateInkDrop() {
  return CreateDefaultFloodFillInkDropImpl();
}

std::unique_ptr<views::InkDropRipple> MdTextButton::CreateInkDropRipple()
    const {
  return std::unique_ptr<views::InkDropRipple>(
      new views::FloodFillInkDropRipple(
          size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
          ink_drop_visible_opacity()));
}

void MdTextButton::StateChanged() {
  LabelButton::StateChanged();
  UpdateColors();
}

std::unique_ptr<views::InkDropHighlight> MdTextButton::CreateInkDropHighlight()
    const {
  // The prominent button hover effect is a shadow.
  const int kYOffset = 1;
  const int kSkiaBlurRadius = 2;
  const int shadow_alpha = is_prominent_ ? 0x3D : 0x1A;
  std::vector<gfx::ShadowValue> shadows;
  // The notion of blur that gfx::ShadowValue uses is twice the Skia/CSS value.
  // Skia counts the number of pixels outside the mask area whereas
  // gfx::ShadowValue counts together the number of pixels inside and outside
  // the mask bounds.
  shadows.push_back(gfx::ShadowValue(gfx::Vector2d(0, kYOffset),
                                     2 * kSkiaBlurRadius,
                                     SkColorSetA(SK_ColorBLACK, shadow_alpha)));
  const SkColor fill_color =
      SkColorSetA(SK_ColorWHITE, is_prominent_ ? 0x0D : 0x05);
  return base::MakeUnique<InkDropHighlight>(
      gfx::RectF(GetLocalBounds()).CenterPoint(),
      base::WrapUnique(new BorderShadowLayerDelegate(
          shadows, GetLocalBounds(), fill_color, kInkDropSmallCornerRadius)));
}

void MdTextButton::SetEnabledTextColors(SkColor color) {
  LabelButton::SetEnabledTextColors(color);
  UpdateColors();
}

void MdTextButton::SetText(const base::string16& text) {
  LabelButton::SetText(text);
  UpdatePadding();
}

void MdTextButton::AdjustFontSize(int size_delta) {
  LabelButton::AdjustFontSize(size_delta);
  UpdatePadding();
}

void MdTextButton::UpdateStyleToIndicateDefaultStatus() {
  is_prominent_ = is_prominent_ || is_default();
  UpdateColors();
}

void MdTextButton::SetFontList(const gfx::FontList& font_list) {
  NOTREACHED()
      << "Don't call MdTextButton::SetFontList (it will soon be protected)";
}

MdTextButton::MdTextButton(ButtonListener* listener)
    : LabelButton(listener, base::string16()),
      is_prominent_(false) {
  SetInkDropMode(PlatformStyle::kUseRipples ? InkDropMode::ON
                                            : InkDropMode::OFF);
  set_has_ink_drop_action_on_click(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusForPlatform();
  SetMinSize(gfx::Size(kMinWidth, 0));
  SetFocusPainter(nullptr);
  label()->SetAutoColorReadabilityEnabled(false);
  set_request_focus_on_press(false);
  LabelButton::SetFontList(GetMdFontList());

  set_animate_on_state_change(true);

  // Paint to a layer so that the canvas is snapped to pixel boundaries (useful
  // for fractional DSF).
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
}

void MdTextButton::UpdatePadding() {
  // Don't use font-based padding when there's no text visible.
  if (GetText().empty()) {
    SetBorder(NullBorder());
    return;
  }

  // Text buttons default to 28dp in height on all platforms when the base font
  // is in use, but should grow or shrink if the font size is adjusted up or
  // down. When the system font size has been adjusted, the base font will be
  // larger than normal such that 28dp might not be enough, so also enforce a
  // minimum height of twice the font size.
  // Example 1:
  // * Normal button on ChromeOS, 12pt Roboto. Button height of 28dp.
  // * Button on ChromeOS that has been adjusted to 14pt Roboto. Button height
  // of 28 + 2 * 2 = 32dp.
  // * Linux user sets base system font size to 17dp. For a normal button, the
  // |size_delta| will be zero, so to adjust upwards we double 17 to get 34.
  int size_delta =
      label()->font_list().GetFontSize() - GetMdFontList().GetFontSize();
  const int kBaseHeight = 28;
  int target_height = std::max(kBaseHeight + size_delta * 2,
                               label()->font_list().GetFontSize() * 2);

  int label_height = label()->GetPreferredSize().height();
  int top_padding = (target_height - label_height) / 2;
  int bottom_padding = (target_height - label_height + 1) / 2;
  DCHECK_EQ(target_height, label_height + top_padding + bottom_padding);

  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  const int kHorizontalPadding = 16;
  SetBorder(CreateEmptyBorder(top_padding, kHorizontalPadding, bottom_padding,
                              kHorizontalPadding));
}

void MdTextButton::UpdateColors() {
  ui::NativeTheme::ColorId fg_color_id =
      is_prominent_ ? ui::NativeTheme::kColorId_TextOnProminentButtonColor
                    : ui::NativeTheme::kColorId_ButtonEnabledColor;

  ui::NativeTheme* theme = GetNativeTheme();
  if (!explicitly_set_normal_color())
    LabelButton::SetEnabledTextColors(theme->GetSystemColor(fg_color_id));

  // Prominent buttons keep their enabled text color; disabled state is conveyed
  // by shading the background instead.
  if (is_prominent_)
    SetTextColor(STATE_DISABLED, theme->GetSystemColor(fg_color_id));

  SkColor text_color = label()->enabled_color();
  SkColor bg_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);

  if (bg_color_override_) {
    bg_color = *bg_color_override_;
  } else if (is_prominent_) {
    bg_color = theme->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    if (state() == STATE_DISABLED)
      bg_color = color_utils::BlendTowardOppositeLuma(
          bg_color, gfx::kDisabledControlAlpha);
  }

  if (state() == STATE_PRESSED) {
    SkColor shade =
        theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonPressedShade);
    bg_color = color_utils::GetResultingPaintColor(shade, bg_color);
  }

  // Specified text color: 5a5a5a @ 1.0 alpha
  // Specified stroke color: 000000 @ 0.2 alpha
  // 000000 @ 0.2 is very close to 5a5a5a @ 0.308 (== 0x4e); both are cccccc @
  // 1.0, and this way if NativeTheme changes the button color, the button
  // stroke will also change colors to match.
  SkColor stroke_color =
      is_prominent_ ? SK_ColorTRANSPARENT : SkColorSetA(text_color, 0x4e);

  // Disabled, non-prominent buttons need their stroke lightened. Prominent
  // buttons need it left at SK_ColorTRANSPARENT from above.
  if (state() == STATE_DISABLED && !is_prominent_) {
    stroke_color = color_utils::BlendTowardOppositeLuma(
        stroke_color, gfx::kDisabledControlAlpha);
  }

  DCHECK_EQ(SK_AlphaOPAQUE, static_cast<int>(SkColorGetA(bg_color)));
  set_background(Background::CreateBackgroundPainter(
      true, Painter::CreateRoundRectWith1PxBorderPainter(
                bg_color, stroke_color, kInkDropSmallCornerRadius)));
}

}  // namespace views
