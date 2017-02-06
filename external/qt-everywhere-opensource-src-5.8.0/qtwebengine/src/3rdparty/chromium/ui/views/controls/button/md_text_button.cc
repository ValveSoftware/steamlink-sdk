// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Inset between clickable region border and button contents (text).
const int kHorizontalPadding = 16;

// Minimum size to reserve for the button contents.
const int kMinWidth = 48;

// The stroke width of the focus border in normal and call to action mode.
const int kFocusBorderThickness = 1;
const int kFocusBorderThicknessCta = 2;

// The corner radius of the focus border roundrect.
const int kFocusBorderCornerRadius = 3;

LabelButton* CreateButton(ButtonListener* listener,
                          const base::string16& text,
                          bool md) {
  if (md)
    return MdTextButton::CreateMdButton(listener, text);

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

namespace internal {

class MdFocusRing : public View {
 public:
  MdFocusRing() : thickness_(kFocusBorderThickness) {
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
  }
  ~MdFocusRing() override {}

  int thickness() const { return thickness_; }
  void set_thickness(int thickness) { thickness_ = thickness; }

  // View:
  bool CanProcessEventsWithinSubtree() const override { return false; }

  void OnPaint(gfx::Canvas* canvas) override {
    MdTextButton::PaintMdFocusRing(canvas, this, thickness_, 0x33);
  }

 private:
  int thickness_;

  DISALLOW_COPY_AND_ASSIGN(MdFocusRing);
};

}  // namespace internal

// static
LabelButton* MdTextButton::CreateStandardButton(ButtonListener* listener,
                                                const base::string16& text) {
  return CreateButton(listener, text,
                      ui::MaterialDesignController::IsModeMaterial());
}

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
    MdTextButton* md_button = MdTextButton::CreateMdButton(listener, text);
    md_button->SetCallToAction(true);
    return md_button;
  }

  return new BlueButton(listener, text);
}

// static
MdTextButton* MdTextButton::CreateMdButton(ButtonListener* listener,
                                           const base::string16& text) {
  MdTextButton* button = new MdTextButton(listener);
  button->SetText(text);
  button->SetFocusForPlatform();
  return button;
}

// static
void MdTextButton::PaintMdFocusRing(gfx::Canvas* canvas,
                                    views::View* view,
                                    int thickness,
                                    SkAlpha alpha) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SkColorSetA(view->GetNativeTheme()->GetSystemColor(
                                 ui::NativeTheme::kColorId_CallToActionColor),
                             alpha));
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(thickness);
  gfx::RectF rect(view->GetLocalBounds());
  rect.Inset(gfx::InsetsF(thickness / 2.f));
  canvas->DrawRoundRect(rect, kFocusBorderCornerRadius, paint);
}

void MdTextButton::SetCallToAction(bool cta) {
  if (is_cta_ == cta)
    return;

  is_cta_ = cta;
  focus_ring_->set_thickness(cta ? kFocusBorderThicknessCta
                                 : kFocusBorderThickness);
  UpdateColors();
}

void MdTextButton::Layout() {
  LabelButton::Layout();
  gfx::Rect focus_bounds = GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(-focus_ring_->thickness()));
  focus_ring_->SetBoundsRect(focus_bounds);
}

void MdTextButton::OnFocus() {
  LabelButton::OnFocus();
  focus_ring_->SetVisible(true);
}

void MdTextButton::OnBlur() {
  LabelButton::OnBlur();
  focus_ring_->SetVisible(false);
}

void MdTextButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  LabelButton::OnNativeThemeChanged(theme);
  UpdateColors();
}

SkColor MdTextButton::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(label()->enabled_color());
}

std::unique_ptr<views::InkDropHighlight> MdTextButton::CreateInkDropHighlight()
    const {
  if (!ShouldShowInkDropHighlight())
    return nullptr;
  if (!is_cta_)
    return LabelButton::CreateInkDropHighlight();

  // The call to action hover effect is a shadow.
  const int kYOffset = 1;
  const int kSkiaBlurRadius = 1;
  std::vector<gfx::ShadowValue> shadows;
  // The notion of blur that gfx::ShadowValue uses is twice the Skia/CSS value.
  // Skia counts the number of pixels outside the mask area whereas
  // gfx::ShadowValue counts together the number of pixels inside and outside
  // the mask bounds.
  shadows.push_back(gfx::ShadowValue(gfx::Vector2d(0, kYOffset),
                                     2 * kSkiaBlurRadius,
                                     SkColorSetA(SK_ColorBLACK, 0x3D)));
  return base::WrapUnique(new InkDropHighlight(
      gfx::RectF(GetLocalBounds()).CenterPoint(),
      base::WrapUnique(new BorderShadowLayerDelegate(
          shadows, GetLocalBounds(), kInkDropSmallCornerRadius))));
}

bool MdTextButton::ShouldShowInkDropForFocus() const {
  // These types of button use |focus_ring_|.
  return false;
}

void MdTextButton::SetEnabledTextColors(SkColor color) {
  LabelButton::SetEnabledTextColors(color);
  UpdateColors();
}

void MdTextButton::UpdateStyleToIndicateDefaultStatus() {
  UpdateColors();
}

MdTextButton::MdTextButton(ButtonListener* listener)
    : LabelButton(listener, base::string16()),
      focus_ring_(new internal::MdFocusRing()),
      is_cta_(false) {
  SetHasInkDrop(true);
  set_has_ink_drop_action_on_click(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusForPlatform();
  SetMinSize(gfx::Size(kMinWidth, 0));
  SetFocusPainter(nullptr);
  label()->SetAutoColorReadabilityEnabled(false);
  SetFontList(GetMdFontList());

  AddChildView(focus_ring_);
  focus_ring_->SetVisible(false);
  set_request_focus_on_press(false);

  // Top and bottom padding depend on the font. Example: if font cap height is
  // 9dp, use 8dp bottom padding and 7dp top padding to total 24dp.
  const gfx::FontList& font = label()->font_list();
  int text_height = font.GetCapHeight();
  int even_text_height = text_height - (text_height % 2);
  const int top_padding = even_text_height - (text_height - even_text_height);
  const int bottom_padding = even_text_height;
  DCHECK_EQ(3 * even_text_height, top_padding + text_height + bottom_padding);

  const int inbuilt_top_padding = font.GetBaseline() - font.GetCapHeight();
  const int inbuilt_bottom_padding =
      font.GetHeight() - label()->font_list().GetBaseline();

  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  SetBorder(Border::CreateEmptyBorder(
      top_padding - inbuilt_top_padding, kHorizontalPadding,
      bottom_padding - inbuilt_bottom_padding, kHorizontalPadding));
}

MdTextButton::~MdTextButton() {}

void MdTextButton::UpdateColors() {
  ui::NativeTheme::ColorId fg_color_id =
      is_cta_ ? ui::NativeTheme::kColorId_TextOnCallToActionColor
              : ui::NativeTheme::kColorId_ButtonEnabledColor;

  // When there's no call to action, respect a color override if one has
  // been set. For call to action styling, don't let individual buttons
  // specify a color.
  ui::NativeTheme* theme = GetNativeTheme();
  if (is_cta_ || !explicitly_set_normal_color())
    LabelButton::SetEnabledTextColors(theme->GetSystemColor(fg_color_id));

  SkColor text_color = label()->enabled_color();
  SkColor bg_color =
      is_cta_
          ? theme->GetSystemColor(ui::NativeTheme::kColorId_CallToActionColor)
          : is_default()
                ? color_utils::BlendTowardOppositeLuma(text_color, 0xD8)
                : SK_ColorTRANSPARENT;

  const SkAlpha kStrokeOpacity = 0x1A;
  SkColor stroke_color = (is_cta_ || color_utils::IsDark(text_color))
                             ? SkColorSetA(SK_ColorBLACK, kStrokeOpacity)
                             : SkColorSetA(SK_ColorWHITE, 2 * kStrokeOpacity);

  set_background(Background::CreateBackgroundPainter(
      true, Painter::CreateRoundRectWith1PxBorderPainter(
                bg_color, stroke_color, kInkDropSmallCornerRadius)));
}

}  // namespace views
