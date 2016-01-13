// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

// The spacing between the icon and text.
const int kSpacing = 5;

#if !(defined(OS_LINUX) && !defined(OS_CHROMEOS))
// Default text and shadow colors for STYLE_BUTTON.
const SkColor kStyleButtonTextColor = SK_ColorBLACK;
const SkColor kStyleButtonShadowColor = SK_ColorWHITE;
#endif

}  // namespace

namespace views {

// static
const int LabelButton::kHoverAnimationDurationMs = 170;

// static
const char LabelButton::kViewClassName[] = "LabelButton";

LabelButton::LabelButton(ButtonListener* listener, const base::string16& text)
    : CustomButton(listener),
      image_(new ImageView()),
      label_(new Label()),
      button_state_images_(),
      button_state_colors_(),
      explicitly_set_colors_(),
      is_default_(false),
      style_(STYLE_TEXTBUTTON),
      border_is_themed_border_(true) {
  SetAnimationDuration(kHoverAnimationDurationMs);
  SetText(text);
  SetFontList(gfx::FontList());

  AddChildView(image_);
  image_->set_interactive(false);

  AddChildView(label_);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Initialize the colors, border, and layout.
  SetStyle(style_);

  SetAccessibleName(text);
}

LabelButton::~LabelButton() {}

const gfx::ImageSkia& LabelButton::GetImage(ButtonState for_state) {
  if (for_state != STATE_NORMAL && button_state_images_[for_state].isNull())
    return button_state_images_[STATE_NORMAL];
  return button_state_images_[for_state];
}

void LabelButton::SetImage(ButtonState for_state, const gfx::ImageSkia& image) {
  button_state_images_[for_state] = image;
  UpdateImage();
}

const base::string16& LabelButton::GetText() const {
  return label_->text();
}

void LabelButton::SetText(const base::string16& text) {
  SetAccessibleName(text);
  label_->SetText(text);
}

void LabelButton::SetTextColor(ButtonState for_state, SkColor color) {
  button_state_colors_[for_state] = color;
  if (for_state == STATE_DISABLED)
    label_->SetDisabledColor(color);
  else if (for_state == state())
    label_->SetEnabledColor(color);
  explicitly_set_colors_[for_state] = true;
}

void LabelButton::SetTextShadows(const gfx::ShadowValues& shadows) {
  label_->set_shadows(shadows);
}

void LabelButton::SetTextSubpixelRenderingEnabled(bool enabled) {
  label_->set_subpixel_rendering_enabled(enabled);
}

bool LabelButton::GetTextMultiLine() const {
  return label_->is_multi_line();
}

void LabelButton::SetTextMultiLine(bool text_multi_line) {
  label_->SetMultiLine(text_multi_line);
}

const gfx::FontList& LabelButton::GetFontList() const {
  return label_->font_list();
}

void LabelButton::SetFontList(const gfx::FontList& font_list) {
  cached_normal_font_list_ = font_list;
  cached_bold_font_list_ = font_list.DeriveWithStyle(
      font_list.GetFontStyle() | gfx::Font::BOLD);

  // STYLE_BUTTON uses bold text to indicate default buttons.
  label_->SetFontList(
      style_ == STYLE_BUTTON && is_default_ ?
      cached_bold_font_list_ : cached_normal_font_list_);
}

void LabelButton::SetElideBehavior(gfx::ElideBehavior elide_behavior) {
  label_->SetElideBehavior(elide_behavior);
}

gfx::HorizontalAlignment LabelButton::GetHorizontalAlignment() const {
  return label_->GetHorizontalAlignment();
}

void LabelButton::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  label_->SetHorizontalAlignment(alignment);
  InvalidateLayout();
}

void LabelButton::SetDirectionalityMode(gfx::DirectionalityMode mode) {
  label_->set_directionality_mode(mode);
}

void LabelButton::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  ui::Accelerator accel(ui::VKEY_RETURN, ui::EF_NONE);
  is_default_ ? AddAccelerator(accel) : RemoveAccelerator(accel);

  // STYLE_BUTTON uses bold text to indicate default buttons.
  if (style_ == STYLE_BUTTON) {
    label_->SetFontList(
        is_default ? cached_bold_font_list_ : cached_normal_font_list_);
  }
}

void LabelButton::SetStyle(ButtonStyle style) {
  style_ = style;
  // Inset the button focus rect from the actual border; roughly match Windows.
  if (style == STYLE_BUTTON) {
    SetFocusPainter(scoped_ptr<Painter>());
  } else {
    SetFocusPainter(Painter::CreateDashedFocusPainterWithInsets(
                        gfx::Insets(3, 3, 3, 3)));
  }
  if (style == STYLE_BUTTON) {
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetFocusable(true);
  }
  if (style == STYLE_BUTTON)
    set_min_size(gfx::Size(70, 33));

  OnNativeThemeChanged(GetNativeTheme());
}

void LabelButton::SetFocusPainter(scoped_ptr<Painter> focus_painter) {
  focus_painter_ = focus_painter.Pass();
}

gfx::Size LabelButton::GetPreferredSize() const {
  // Use a temporary label copy for sizing to avoid calculation side-effects.
  Label label(GetText(), cached_normal_font_list_);
  label.set_shadows(label_->shadows());
  label.SetMultiLine(GetTextMultiLine());

  if (style() == STYLE_BUTTON) {
    // Some text appears wider when rendered normally than when rendered bold.
    // Accommodate the widest, as buttons may show bold and shouldn't resize.
    const int current_width = label.GetPreferredSize().width();
    label.SetFontList(cached_bold_font_list_);
    if (label.GetPreferredSize().width() < current_width)
      label.SetFontList(cached_normal_font_list_);
  }

  // Resize multi-line labels given the current limited available width.
  const gfx::Size image_size(image_->GetPreferredSize());
  const int image_width = image_size.width();
  if (GetTextMultiLine() && (width() > image_width + kSpacing))
    label.SizeToFit(width() - image_width - (image_width > 0 ? kSpacing : 0));

  // Calculate the required size.
  gfx::Size size(label.GetPreferredSize());
  if (image_width > 0 && size.width() > 0)
    size.Enlarge(kSpacing, 0);
  size.SetToMax(gfx::Size(0, image_size.height()));
  const gfx::Insets insets(GetInsets());
  size.Enlarge(image_size.width() + insets.width(), insets.height());

  // Make the size at least as large as the minimum size needed by the border.
  size.SetToMax(border() ? border()->GetMinimumSize() : gfx::Size());

  // Increase the minimum size monotonically with the preferred size.
  size.SetToMax(min_size_);
  min_size_ = size;

  // Return the largest known size clamped to the maximum size (if valid).
  if (max_size_.width() > 0)
    size.set_width(std::min(max_size_.width(), size.width()));
  if (max_size_.height() > 0)
    size.set_height(std::min(max_size_.height(), size.height()));
  return size;
}

void LabelButton::Layout() {
  gfx::HorizontalAlignment adjusted_alignment = GetHorizontalAlignment();
  if (base::i18n::IsRTL() && adjusted_alignment != gfx::ALIGN_CENTER)
    adjusted_alignment = (adjusted_alignment == gfx::ALIGN_LEFT) ?
        gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;

  gfx::Rect child_area(GetChildAreaBounds());
  child_area.Inset(GetInsets());

  gfx::Size image_size(image_->GetPreferredSize());
  image_size.SetToMin(child_area.size());

  // The label takes any remaining width after sizing the image, unless both
  // views are centered. In that case, using the tighter preferred label width
  // avoids wasted space within the label that would look like awkward padding.
  // Labels can paint over the full button height, including the border height.
  gfx::Size label_size(child_area.width(), height());
  if (!image_size.IsEmpty() && !label_size.IsEmpty()) {
    label_size.set_width(
        std::max(child_area.width() - image_size.width() - kSpacing, 0));
    if (adjusted_alignment == gfx::ALIGN_CENTER) {
      // Ensure multi-line labels paired with images use their available width.
      if (GetTextMultiLine())
        label_->SizeToFit(label_size.width());
      label_size.set_width(
          std::min(label_size.width(), label_->GetPreferredSize().width()));
    }
  }

  gfx::Point image_origin(child_area.origin());
  image_origin.Offset(0, (child_area.height() - image_size.height()) / 2);
  if (adjusted_alignment == gfx::ALIGN_CENTER) {
    const int total_width = image_size.width() + label_size.width() +
        ((image_size.width() > 0 && label_size.width() > 0) ? kSpacing : 0);
    image_origin.Offset((child_area.width() - total_width) / 2, 0);
  } else if (adjusted_alignment == gfx::ALIGN_RIGHT) {
    image_origin.Offset(child_area.width() - image_size.width(), 0);
  }

  gfx::Point label_origin(child_area.x(), 0);
  if (!image_size.IsEmpty() && adjusted_alignment != gfx::ALIGN_RIGHT)
    label_origin.set_x(image_origin.x() + image_size.width() + kSpacing);

  image_->SetBoundsRect(gfx::Rect(image_origin, image_size));
  label_->SetBoundsRect(gfx::Rect(label_origin, label_size));
}

const char* LabelButton::GetClassName() const {
  return kViewClassName;
}

scoped_ptr<LabelButtonBorder> LabelButton::CreateDefaultBorder() const {
  return scoped_ptr<LabelButtonBorder>(new LabelButtonBorder(style_));
}

void LabelButton::SetBorder(scoped_ptr<Border> border) {
  border_is_themed_border_ = false;
  View::SetBorder(border.Pass());
}

gfx::Rect LabelButton::GetChildAreaBounds() {
  return GetLocalBounds();
}

void LabelButton::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void LabelButton::OnFocus() {
  View::OnFocus();
  // Typically the border renders differently when focused.
  SchedulePaint();
}

void LabelButton::OnBlur() {
  View::OnBlur();
  // Typically the border renders differently when focused.
  SchedulePaint();
}

void LabelButton::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  params->button.checked = false;
  params->button.indeterminate = false;
  params->button.is_default = is_default_;
  params->button.is_focused = HasFocus() && IsAccessibilityFocusable();
  params->button.has_border = false;
  params->button.classic_state = 0;
  params->button.background_color = label_->background_color();
}

void LabelButton::ResetColorsFromNativeTheme() {
  const ui::NativeTheme* theme = GetNativeTheme();
  SkColor colors[STATE_COUNT] = {
    theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonEnabledColor),
    theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonHoverColor),
    theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonHoverColor),
    theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonDisabledColor),
  };

  // Certain styles do not change text color when hovered or pressed.
  bool constant_text_color = false;
  // Use hardcoded colors for inverted color scheme support and STYLE_BUTTON.
  if (gfx::IsInvertedColorScheme()) {
    constant_text_color = true;
    colors[STATE_NORMAL] = SK_ColorWHITE;
    label_->SetBackgroundColor(SK_ColorBLACK);
    label_->set_background(Background::CreateSolidBackground(SK_ColorBLACK));
    label_->SetAutoColorReadabilityEnabled(true);
    label_->set_shadows(gfx::ShadowValues());
  } else if (style() == STYLE_BUTTON) {
    // TODO(erg): This is disabled on desktop linux because of the binary asset
    // confusion. These details should either be pushed into ui::NativeThemeWin
    // or should be obsoleted by rendering buttons with paint calls instead of
    // with static assets. http://crbug.com/350498
#if !(defined(OS_LINUX) && !defined(OS_CHROMEOS))
    constant_text_color = true;
    colors[STATE_NORMAL] = kStyleButtonTextColor;
    label_->SetBackgroundColor(theme->GetSystemColor(
        ui::NativeTheme::kColorId_ButtonBackgroundColor));
    label_->SetAutoColorReadabilityEnabled(false);
    label_->set_shadows(gfx::ShadowValues(1,
        gfx::ShadowValue(gfx::Point(0, 1), 0, kStyleButtonShadowColor)));
#endif
    label_->set_background(NULL);
  } else {
    label_->set_background(NULL);
  }

  if (constant_text_color)
    colors[STATE_HOVERED] = colors[STATE_PRESSED] = colors[STATE_NORMAL];

  for (size_t state = STATE_NORMAL; state < STATE_COUNT; ++state) {
    if (!explicitly_set_colors_[state]) {
      SetTextColor(static_cast<ButtonState>(state), colors[state]);
      explicitly_set_colors_[state] = false;
    }
  }
}

void LabelButton::UpdateImage() {
  image_->SetImage(GetImage(state()));
}

void LabelButton::UpdateThemedBorder() {
  // Don't override borders set by others.
  if (!border_is_themed_border_)
    return;

  scoped_ptr<LabelButtonBorder> label_button_border = CreateDefaultBorder();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    SetBorder(linux_ui->CreateNativeBorder(
        this, label_button_border.Pass()));
  } else
#endif
  {
    SetBorder(label_button_border.PassAs<Border>());
  }

  border_is_themed_border_ = true;
}

void LabelButton::StateChanged() {
  const gfx::Size previous_image_size(image_->GetPreferredSize());
  UpdateImage();
  const SkColor color = button_state_colors_[state()];
  if (state() != STATE_DISABLED && label_->enabled_color() != color)
    label_->SetEnabledColor(color);
  label_->SetEnabled(state() != STATE_DISABLED);
  if (image_->GetPreferredSize() != previous_image_size)
    Layout();
}

void LabelButton::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void LabelButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  ResetColorsFromNativeTheme();
  UpdateThemedBorder();
  // Invalidate the layout to pickup the new insets from the border.
  InvalidateLayout();
}

ui::NativeTheme::Part LabelButton::GetThemePart() const {
  return ui::NativeTheme::kPushButton;
}

gfx::Rect LabelButton::GetThemePaintRect() const {
  return GetLocalBounds();
}

ui::NativeTheme::State LabelButton::GetThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  switch (state()) {
    case STATE_NORMAL:   return ui::NativeTheme::kNormal;
    case STATE_HOVERED:  return ui::NativeTheme::kHovered;
    case STATE_PRESSED:  return ui::NativeTheme::kPressed;
    case STATE_DISABLED: return ui::NativeTheme::kDisabled;
    case STATE_COUNT:    NOTREACHED() << "Unknown state: " << state();
  }
  return ui::NativeTheme::kNormal;
}

const gfx::Animation* LabelButton::GetThemeAnimation() const {
  return hover_animation_.get();
}

ui::NativeTheme::State LabelButton::GetBackgroundThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kNormal;
}

ui::NativeTheme::State LabelButton::GetForegroundThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kHovered;
}

}  // namespace views
