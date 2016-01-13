// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/text_button.h"

#include <algorithm>

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/native_theme/native_theme_win.h"
#endif

namespace views {

namespace {

// Default space between the icon and text.
const int kDefaultIconTextSpacing = 5;

// Preferred padding between text and edge.
const int kPreferredPaddingHorizontal = 6;
const int kPreferredPaddingVertical = 5;

// Preferred padding between text and edge for NativeTheme border.
const int kPreferredNativeThemePaddingHorizontal = 12;
const int kPreferredNativeThemePaddingVertical = 5;

// By default the focus rect is drawn at the border of the view.  For a button,
// we inset the focus rect by 3 pixels so that it doesn't draw on top of the
// button's border. This roughly matches how the Windows native focus rect for
// buttons looks. A subclass that draws a button with different padding may need
// to provide a different focus painter and do something different.
const int kFocusRectInset = 3;

// How long the hover fade animation should last.
const int kHoverAnimationDurationMs = 170;

#if defined(OS_WIN)
// These sizes are from http://msdn.microsoft.com/en-us/library/aa511279.aspx
const int kMinWidthDLUs = 50;
const int kMinHeightDLUs = 14;
#endif

// The default hot and pushed button image IDs; normal has none by default.
const int kHotImages[] = IMAGE_GRID(IDR_TEXTBUTTON_HOVER);
const int kPushedImages[] = IMAGE_GRID(IDR_TEXTBUTTON_PRESSED);

}  // namespace

// static
const char TextButtonBase::kViewClassName[] = "TextButtonBase";
// static
const char TextButton::kViewClassName[] = "TextButton";


// TextButtonBorder -----------------------------------------------------------

TextButtonBorder::TextButtonBorder() {
}

TextButtonBorder::~TextButtonBorder() {
}

void TextButtonBorder::Paint(const View& view, gfx::Canvas* canvas) {
}

gfx::Insets TextButtonBorder::GetInsets() const {
  return insets_;
}

gfx::Size TextButtonBorder::GetMinimumSize() const {
  return gfx::Size();
}

void TextButtonBorder::SetInsets(const gfx::Insets& insets) {
  insets_ = insets;
}


// TextButtonDefaultBorder ----------------------------------------------------

TextButtonDefaultBorder::TextButtonDefaultBorder()
    : vertical_padding_(kPreferredPaddingVertical) {
  set_hot_painter(Painter::CreateImageGridPainter(kHotImages));
  set_pushed_painter(Painter::CreateImageGridPainter(kPushedImages));
  SetInsets(gfx::Insets(vertical_padding_, kPreferredPaddingHorizontal,
                        vertical_padding_, kPreferredPaddingHorizontal));
}

TextButtonDefaultBorder::~TextButtonDefaultBorder() {
}

void TextButtonDefaultBorder::Paint(const View& view, gfx::Canvas* canvas) {
  const TextButton* button = static_cast<const TextButton*>(&view);
  int state = button->state();
  bool animating = button->GetAnimation()->is_animating();

  Painter* painter = normal_painter_.get();
  // Use the hot painter when we're hovered. Also use the hot painter when we're
  // STATE_NORMAL and |animating| so that we show throb animations started from
  // CustomButton::StartThrobbing which should start throbbing the button
  // regardless of whether it is hovered.
  if (button->show_multiple_icon_states() &&
      ((state == TextButton::STATE_HOVERED) ||
       (state == TextButton::STATE_PRESSED) ||
       ((state == TextButton::STATE_NORMAL) && animating))) {
    painter = (state == TextButton::STATE_PRESSED) ?
        pushed_painter_.get() : hot_painter_.get();
  }
  if (painter) {
    if (animating) {
      // TODO(pkasting): Really this should crossfade between states so it could
      // handle the case of having a non-NULL |normal_painter_|.
      canvas->SaveLayerAlpha(static_cast<uint8>(
          button->GetAnimation()->CurrentValueBetween(0, 255)));
      painter->Paint(canvas, view.size());
      canvas->Restore();
    } else {
      painter->Paint(canvas, view.size());
    }
  }
}

gfx::Size TextButtonDefaultBorder::GetMinimumSize() const {
  gfx::Size size;
  if (normal_painter_)
    size.SetToMax(normal_painter_->GetMinimumSize());
  if (hot_painter_)
    size.SetToMax(hot_painter_->GetMinimumSize());
  if (pushed_painter_)
    size.SetToMax(pushed_painter_->GetMinimumSize());
  return size;
}


// TextButtonNativeThemeBorder ------------------------------------------------

TextButtonNativeThemeBorder::TextButtonNativeThemeBorder(
    NativeThemeDelegate* delegate)
    : delegate_(delegate) {
  SetInsets(gfx::Insets(kPreferredNativeThemePaddingVertical,
                        kPreferredNativeThemePaddingHorizontal,
                        kPreferredNativeThemePaddingVertical,
                        kPreferredNativeThemePaddingHorizontal));
}

TextButtonNativeThemeBorder::~TextButtonNativeThemeBorder() {
}

void TextButtonNativeThemeBorder::Paint(const View& view, gfx::Canvas* canvas) {
  const ui::NativeTheme* theme = view.GetNativeTheme();
  const TextButtonBase* tb = static_cast<const TextButton*>(&view);
  ui::NativeTheme::Part part = delegate_->GetThemePart();
  gfx::Rect rect(delegate_->GetThemePaintRect());

  if (tb->show_multiple_icon_states() &&
      delegate_->GetThemeAnimation() != NULL &&
      delegate_->GetThemeAnimation()->is_animating()) {
    // Paint background state.
    ui::NativeTheme::ExtraParams prev_extra;
    ui::NativeTheme::State prev_state =
        delegate_->GetBackgroundThemeState(&prev_extra);
    theme->Paint(canvas->sk_canvas(), part, prev_state, rect, prev_extra);

    // Composite foreground state above it.
    ui::NativeTheme::ExtraParams extra;
    ui::NativeTheme::State state = delegate_->GetForegroundThemeState(&extra);
    int alpha = delegate_->GetThemeAnimation()->CurrentValueBetween(0, 255);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    canvas->Restore();
  } else {
    ui::NativeTheme::ExtraParams extra;
    ui::NativeTheme::State state = delegate_->GetThemeState(&extra);
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
  }
}


// TextButtonBase -------------------------------------------------------------

TextButtonBase::TextButtonBase(ButtonListener* listener,
                               const base::string16& text)
    : CustomButton(listener),
      alignment_(ALIGN_LEFT),
      min_width_(0),
      min_height_(0),
      max_width_(0),
      show_multiple_icon_states_(true),
      is_default_(false),
      multi_line_(false),
      use_enabled_color_from_theme_(true),
      use_disabled_color_from_theme_(true),
      use_highlight_color_from_theme_(true),
      use_hover_color_from_theme_(true),
      focus_painter_(Painter::CreateDashedFocusPainter()) {
  SetText(text);
  SetAnimationDuration(kHoverAnimationDurationMs);
}

TextButtonBase::~TextButtonBase() {
}

void TextButtonBase::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  if (is_default_)
    AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  else
    RemoveAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  SchedulePaint();
}

void TextButtonBase::SetText(const base::string16& text) {
  if (text == text_)
    return;
  text_ = text;
  SetAccessibleName(text);
  UpdateTextSize();
}

void TextButtonBase::SetFontList(const gfx::FontList& font_list) {
  font_list_ = font_list;
  UpdateTextSize();
}

void TextButtonBase::SetEnabledColor(SkColor color) {
  color_enabled_ = color;
  use_enabled_color_from_theme_ = false;
  UpdateColor();
}

void TextButtonBase::SetDisabledColor(SkColor color) {
  color_disabled_ = color;
  use_disabled_color_from_theme_ = false;
  UpdateColor();
}

void TextButtonBase::SetHighlightColor(SkColor color) {
  color_highlight_ = color;
  use_highlight_color_from_theme_ = false;
}

void TextButtonBase::SetHoverColor(SkColor color) {
  color_hover_ = color;
  use_hover_color_from_theme_ = false;
}

void TextButtonBase::ClearMaxTextSize() {
  max_text_size_ = text_size_;
}

void TextButtonBase::SetShowMultipleIconStates(bool show_multiple_icon_states) {
  show_multiple_icon_states_ = show_multiple_icon_states;
}

void TextButtonBase::SetMultiLine(bool multi_line) {
  if (multi_line != multi_line_) {
    multi_line_ = multi_line;
    max_text_size_.SetSize(0, 0);
    UpdateTextSize();
    SchedulePaint();
  }
}

gfx::Size TextButtonBase::GetPreferredSize() const {
  gfx::Insets insets = GetInsets();

  // Use the max size to set the button boundaries.
  // In multiline mode max size can be undefined while
  // width() is 0, so max it out with current text size.
  gfx::Size prefsize(std::max(max_text_size_.width(),
                              text_size_.width()) + insets.width(),
                     std::max(max_text_size_.height(),
                              text_size_.height()) + insets.height());

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  prefsize.set_width(std::max(prefsize.width(), min_width_));
  prefsize.set_height(std::max(prefsize.height(), min_height_));

  return prefsize;
}

int TextButtonBase::GetHeightForWidth(int w) const {
  if (!multi_line_)
    return View::GetHeightForWidth(w);

  if (max_width_ > 0)
    w = std::min(max_width_, w);

  gfx::Size text_size;
  CalculateTextSize(&text_size, w);
  int height = text_size.height() + GetInsets().height();

  return std::max(height, min_height_);
}

void TextButtonBase::OnPaint(gfx::Canvas* canvas) {
  PaintButton(canvas, PB_NORMAL);
}

void TextButtonBase::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (multi_line_)
    UpdateTextSize();
}

const gfx::Animation* TextButtonBase::GetAnimation() const {
  return hover_animation_.get();
}

void TextButtonBase::UpdateColor() {
  color_ = enabled() ? color_enabled_ : color_disabled_;
}

void TextButtonBase::UpdateTextSize() {
  int text_width = width();
  // If width is defined, use GetTextBounds.width() for maximum text width,
  // as it will take size of checkbox/radiobutton into account.
  if (text_width != 0) {
    gfx::Rect text_bounds = GetTextBounds();
    text_width = text_bounds.width();
  }
  CalculateTextSize(&text_size_, text_width);
  // Before layout width() is 0, and multiline text will be treated as one line.
  // Do not store max_text_size in this case. UpdateTextSize will be called
  // again once width() changes.
  if (!multi_line_ || text_width != 0) {
    max_text_size_.SetSize(std::max(max_text_size_.width(), text_size_.width()),
                           std::max(max_text_size_.height(),
                                    text_size_.height()));
    PreferredSizeChanged();
  }
}

void TextButtonBase::CalculateTextSize(gfx::Size* text_size,
                                       int max_width) const {
  int h = font_list_.GetHeight();
  int w = multi_line_ ? max_width : 0;
  int flags = ComputeCanvasStringFlags();
  if (!multi_line_)
    flags |= gfx::Canvas::NO_ELLIPSIS;

  gfx::Canvas::SizeStringInt(text_, font_list_, &w, &h, 0, flags);
  text_size->SetSize(w, h);
}

void TextButtonBase::OnPaintText(gfx::Canvas* canvas, PaintButtonMode mode) {
  gfx::Rect text_bounds(GetTextBounds());
  if (text_bounds.width() > 0) {
    // Because the text button can (at times) draw multiple elements on the
    // canvas, we can not mirror the button by simply flipping the canvas as
    // doing this will mirror the text itself. Flipping the canvas will also
    // make the icons look wrong because icons are almost always represented as
    // direction-insensitive images and such images should never be flipped
    // horizontally.
    //
    // Due to the above, we must perform the flipping manually for RTL UIs.
    text_bounds.set_x(GetMirroredXForRect(text_bounds));

    SkColor text_color = (show_multiple_icon_states_ &&
        (state() == STATE_HOVERED || state() == STATE_PRESSED)) ?
            color_hover_ : color_;

    int draw_string_flags = gfx::Canvas::DefaultCanvasTextAlignment() |
        ComputeCanvasStringFlags();

    if (mode == PB_FOR_DRAG) {
      // Disable sub-pixel rendering as background is transparent.
      draw_string_flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;
      canvas->DrawStringRectWithHalo(text_, font_list_,
                                     SK_ColorBLACK, SK_ColorWHITE,
                                     text_bounds, draw_string_flags);
    } else {
      canvas->DrawStringRectWithFlags(text_, font_list_, text_color,
                                      text_bounds, draw_string_flags);
    }
  }
}

int TextButtonBase::ComputeCanvasStringFlags() const {
  if (!multi_line_)
    return 0;

  int flags = gfx::Canvas::MULTI_LINE;
  switch (alignment_) {
    case ALIGN_LEFT:
      flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
      break;
    case ALIGN_RIGHT:
      flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
      break;
    case ALIGN_CENTER:
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
      break;
  }
  return flags;
}

void TextButtonBase::OnFocus() {
  View::OnFocus();
  if (focus_painter_)
    SchedulePaint();
}

void TextButtonBase::OnBlur() {
  View::OnBlur();
  if (focus_painter_)
    SchedulePaint();
}

void TextButtonBase::GetExtraParams(
    ui::NativeTheme::ExtraParams* params) const {
  params->button.checked = false;
  params->button.indeterminate = false;
  params->button.is_default = false;
  params->button.is_focused = false;
  params->button.has_border = false;
  params->button.classic_state = 0;
  params->button.background_color =
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_ButtonBackgroundColor);
}

gfx::Rect TextButtonBase::GetContentBounds(int extra_width) const {
  gfx::Insets insets = GetInsets();
  int available_width = width() - insets.width();
  int content_width = text_size_.width() + extra_width;
  int content_x = 0;
  switch(alignment_) {
    case ALIGN_LEFT:
      content_x = insets.left();
      break;
    case ALIGN_RIGHT:
      content_x = width() - insets.right() - content_width;
      if (content_x < insets.left())
        content_x = insets.left();
      break;
    case ALIGN_CENTER:
      content_x = insets.left() + std::max(0,
          (available_width - content_width) / 2);
      break;
  }
  content_width = std::min(content_width,
                           width() - insets.right() - content_x);
  int available_height = height() - insets.height();
  int content_y = (available_height - text_size_.height()) / 2 + insets.top();

  gfx::Rect bounds(content_x, content_y, content_width, text_size_.height());
  return bounds;
}

gfx::Rect TextButtonBase::GetTextBounds() const {
  return GetContentBounds(0);
}

void TextButtonBase::SetFocusPainter(scoped_ptr<Painter> focus_painter) {
  focus_painter_ = focus_painter.Pass();
}

void TextButtonBase::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  if (mode == PB_NORMAL) {
    OnPaintBackground(canvas);
    OnPaintBorder(canvas);
    Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
  }

  OnPaintText(canvas, mode);
}

gfx::Size TextButtonBase::GetMinimumSize() const {
  return max_text_size_;
}

void TextButtonBase::OnEnabledChanged() {
  // We should always call UpdateColor() since the state of the button might be
  // changed by other functions like CustomButton::SetState().
  UpdateColor();
  CustomButton::OnEnabledChanged();
}

const char* TextButtonBase::GetClassName() const {
  return kViewClassName;
}

void TextButtonBase::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (use_enabled_color_from_theme_) {
    color_enabled_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_ButtonEnabledColor);
  }
  if (use_disabled_color_from_theme_) {
    color_disabled_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_ButtonDisabledColor);
  }
  if (use_highlight_color_from_theme_) {
    color_highlight_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_ButtonHighlightColor);
  }
  if (use_hover_color_from_theme_) {
    color_hover_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_ButtonHoverColor);
  }
  UpdateColor();
}

gfx::Rect TextButtonBase::GetThemePaintRect() const {
  return GetLocalBounds();
}

ui::NativeTheme::State TextButtonBase::GetThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  switch(state()) {
    case STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    default:
      NOTREACHED() << "Unknown state: " << state();
      return ui::NativeTheme::kNormal;
  }
}

const gfx::Animation* TextButtonBase::GetThemeAnimation() const {
#if defined(OS_WIN)
  if (GetNativeTheme() == ui::NativeThemeWin::instance()) {
    return ui::NativeThemeWin::instance()->IsThemingActive() ?
        hover_animation_.get() : NULL;
  }
#endif
  return hover_animation_.get();
}

ui::NativeTheme::State TextButtonBase::GetBackgroundThemeState(
  ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kNormal;
}

ui::NativeTheme::State TextButtonBase::GetForegroundThemeState(
  ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kHovered;
}


// TextButton -----------------------------------------------------------------

TextButton::TextButton(ButtonListener* listener, const base::string16& text)
    : TextButtonBase(listener, text),
      icon_placement_(ICON_ON_LEFT),
      has_hover_icon_(false),
      has_pushed_icon_(false),
      icon_text_spacing_(kDefaultIconTextSpacing),
      ignore_minimum_size_(true),
      full_justification_(false) {
  SetBorder(scoped_ptr<Border>(new TextButtonDefaultBorder));
  SetFocusPainter(Painter::CreateDashedFocusPainterWithInsets(
                      gfx::Insets(kFocusRectInset, kFocusRectInset,
                                  kFocusRectInset, kFocusRectInset)));
}

TextButton::~TextButton() {
}

void TextButton::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  SchedulePaint();
}

void TextButton::SetHoverIcon(const gfx::ImageSkia& icon) {
  icon_hover_ = icon;
  has_hover_icon_ = true;
  SchedulePaint();
}

void TextButton::SetPushedIcon(const gfx::ImageSkia& icon) {
  icon_pushed_ = icon;
  has_pushed_icon_ = true;
  SchedulePaint();
}

gfx::Size TextButton::GetPreferredSize() const {
  gfx::Size prefsize(TextButtonBase::GetPreferredSize());
  prefsize.Enlarge(icon_.width(), 0);
  prefsize.set_height(std::max(prefsize.height(), icon_.height()));

  // Use the max size to set the button boundaries.
  if (icon_.width() > 0 && !text_.empty())
    prefsize.Enlarge(icon_text_spacing_, 0);

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

#if defined(OS_WIN)
  // Clamp the size returned to at least the minimum size.
  if (!ignore_minimum_size_) {
    gfx::PlatformFontWin* platform_font = static_cast<gfx::PlatformFontWin*>(
        font_list_.GetPrimaryFont().platform_font());
    prefsize.set_width(std::max(
        prefsize.width(),
        platform_font->horizontal_dlus_to_pixels(kMinWidthDLUs)));
    prefsize.set_height(std::max(
        prefsize.height(),
        platform_font->vertical_dlus_to_pixels(kMinHeightDLUs)));
  }
#endif

  prefsize.set_width(std::max(prefsize.width(), min_width_));
  prefsize.set_height(std::max(prefsize.height(), min_height_));

  return prefsize;
}

void TextButton::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  if (full_justification_ && icon_placement_ == ICON_ON_LEFT)
      set_alignment(ALIGN_RIGHT);

  TextButtonBase::PaintButton(canvas, mode);
  OnPaintIcon(canvas, mode);
}

void TextButton::OnPaintIcon(gfx::Canvas* canvas, PaintButtonMode mode) {
  const gfx::ImageSkia& icon = GetImageToPaint();

  if (icon.width() > 0) {
    gfx::Rect text_bounds = GetTextBounds();
    int icon_x = 0;
    int spacing = text_.empty() ? 0 : icon_text_spacing_;
    gfx::Insets insets = GetInsets();
    switch (icon_placement_) {
      case ICON_ON_LEFT:
        icon_x = full_justification_ ? insets.left()
                                     : text_bounds.x() - icon.width() - spacing;
        break;
      case ICON_ON_RIGHT:
        icon_x = full_justification_ ? width() - insets.right() - icon.width()
                                     : text_bounds.right() + spacing;
        break;
      case ICON_CENTERED:
        DCHECK(text_.empty());
        icon_x = (width() - insets.width() - icon.width()) / 2 + insets.left();
        break;
      default:
        NOTREACHED();
        break;
    }

    int available_height = height() - insets.height();
    int icon_y = (available_height - icon.height()) / 2 + insets.top();

    // Mirroring the icon position if necessary.
    gfx::Rect icon_bounds(icon_x, icon_y, icon.width(), icon.height());
    icon_bounds.set_x(GetMirroredXForRect(icon_bounds));
    canvas->DrawImageInt(icon, icon_bounds.x(), icon_bounds.y());
  }
}

void TextButton::set_ignore_minimum_size(bool ignore_minimum_size) {
  ignore_minimum_size_ = ignore_minimum_size;
}

void TextButton::set_full_justification(bool full_justification) {
  full_justification_ = full_justification;
}

const char* TextButton::GetClassName() const {
  return kViewClassName;
}

ui::NativeTheme::Part TextButton::GetThemePart() const {
  return ui::NativeTheme::kPushButton;
}

void TextButton::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  TextButtonBase::GetExtraParams(params);
  params->button.is_default = is_default_;
}

gfx::Rect TextButton::GetTextBounds() const {
  int extra_width = 0;

  const gfx::ImageSkia& icon = GetImageToPaint();
  if (icon.width() > 0)
    extra_width = icon.width() + (text_.empty() ? 0 : icon_text_spacing_);

  gfx::Rect bounds(GetContentBounds(extra_width));

  if (extra_width > 0) {
    // Make sure the icon is always fully visible.
    if (icon_placement_ == ICON_ON_LEFT) {
      bounds.Inset(extra_width, 0, 0, 0);
    } else if (icon_placement_ == ICON_ON_RIGHT) {
      bounds.Inset(0, 0, extra_width, 0);
    }
  }

  return bounds;
}

const gfx::ImageSkia& TextButton::GetImageToPaint() const {
  if (show_multiple_icon_states_) {
    if (has_hover_icon_ && (state() == STATE_HOVERED))
      return icon_hover_;
    if (has_pushed_icon_ && (state() == STATE_PRESSED))
      return icon_pushed_;
  }
  return icon_;
}

}  // namespace views
