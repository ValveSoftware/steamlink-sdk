// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/label.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/utf16_indexing.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"

namespace {

const int kCachedSizeLimit = 10;
const base::char16 kPasswordReplacementChar = '*';

}  // namespace

namespace views {

// static
const char Label::kViewClassName[] = "Label";
const int Label::kFocusBorderPadding = 1;

Label::Label() {
  Init(base::string16(), gfx::FontList());
}

Label::Label(const base::string16& text) {
  Init(text, gfx::FontList());
}

Label::Label(const base::string16& text, const gfx::FontList& font_list) {
  Init(text, font_list);
}

Label::~Label() {
}

void Label::SetFontList(const gfx::FontList& font_list) {
  font_list_ = font_list;
  ResetCachedSize();
  PreferredSizeChanged();
  SchedulePaint();
}

void Label::SetText(const base::string16& text) {
  if (text != text_)
    SetTextInternal(text);
}

void Label::SetTextInternal(const base::string16& text) {
  text_ = text;

  if (is_obscured_) {
    size_t obscured_text_length =
        static_cast<size_t>(gfx::UTF16IndexToOffset(text_, 0, text_.length()));
    layout_text_.assign(obscured_text_length, kPasswordReplacementChar);
  } else {
    layout_text_ = text_;
  }

  ResetCachedSize();
  PreferredSizeChanged();
  SchedulePaint();
}

void Label::SetAutoColorReadabilityEnabled(bool enabled) {
  auto_color_readability_ = enabled;
  RecalculateColors();
}

void Label::SetEnabledColor(SkColor color) {
  requested_enabled_color_ = color;
  enabled_color_set_ = true;
  RecalculateColors();
}

void Label::SetDisabledColor(SkColor color) {
  requested_disabled_color_ = color;
  disabled_color_set_ = true;
  RecalculateColors();
}

void Label::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  background_color_set_ = true;
  RecalculateColors();
}

void Label::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  // If the UI layout is right-to-left, flip the alignment direction.
  if (base::i18n::IsRTL() &&
      (alignment == gfx::ALIGN_LEFT || alignment == gfx::ALIGN_RIGHT)) {
    alignment = (alignment == gfx::ALIGN_LEFT) ?
        gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
  }
  if (horizontal_alignment_ != alignment) {
    horizontal_alignment_ = alignment;
    SchedulePaint();
  }
}

gfx::HorizontalAlignment Label::GetHorizontalAlignment() const {
  if (horizontal_alignment_ != gfx::ALIGN_TO_HEAD)
    return horizontal_alignment_;

  const base::i18n::TextDirection dir =
      base::i18n::GetFirstStrongCharacterDirection(layout_text());
  return dir == base::i18n::RIGHT_TO_LEFT ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
}

void Label::SetLineHeight(int height) {
  if (height != line_height_) {
    line_height_ = height;
    ResetCachedSize();
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void Label::SetMultiLine(bool multi_line) {
  DCHECK(!multi_line || (elide_behavior_ == gfx::ELIDE_TAIL ||
                         elide_behavior_ == gfx::TRUNCATE));
  if (multi_line != is_multi_line_) {
    is_multi_line_ = multi_line;
    ResetCachedSize();
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void Label::SetObscured(bool obscured) {
  if (obscured != is_obscured_) {
    is_obscured_ = obscured;
    SetTextInternal(text_);
  }
}

void Label::SetAllowCharacterBreak(bool allow_character_break) {
  if (allow_character_break != allow_character_break_) {
    allow_character_break_ = allow_character_break;
    ResetCachedSize();
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void Label::SetElideBehavior(gfx::ElideBehavior elide_behavior) {
  DCHECK(!is_multi_line_ || (elide_behavior_ == gfx::ELIDE_TAIL ||
                             elide_behavior_ == gfx::TRUNCATE));
  if (elide_behavior != elide_behavior_) {
    elide_behavior_ = elide_behavior;
    ResetCachedSize();
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void Label::SetTooltipText(const base::string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

void Label::SizeToFit(int max_width) {
  DCHECK(is_multi_line_);

  std::vector<base::string16> lines;
  base::SplitString(layout_text(), '\n', &lines);

  int label_width = 0;
  for (std::vector<base::string16>::const_iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    label_width = std::max(label_width, gfx::GetStringWidth(*iter, font_list_));
  }

  label_width += GetInsets().width();

  if (max_width > 0)
    label_width = std::min(label_width, max_width);

  SetBounds(x(), y(), label_width, 0);
  SizeToPreferredSize();
}

gfx::Insets Label::GetInsets() const {
  gfx::Insets insets = View::GetInsets();
  if (focusable()) {
    insets += gfx::Insets(kFocusBorderPadding, kFocusBorderPadding,
                          kFocusBorderPadding, kFocusBorderPadding);
  }
  return insets;
}

int Label::GetBaseline() const {
  return GetInsets().top() + font_list_.GetBaseline();
}

gfx::Size Label::GetPreferredSize() const {
  // Return a size of (0, 0) if the label is not visible and if the
  // collapse_when_hidden_ flag is set.
  // TODO(munjal): This logic probably belongs to the View class. But for now,
  // put it here since putting it in View class means all inheriting classes
  // need ot respect the collapse_when_hidden_ flag.
  if (!visible() && collapse_when_hidden_)
    return gfx::Size();

  gfx::Size size(GetTextSize());
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

gfx::Size Label::GetMinimumSize() const {
  gfx::Size text_size(GetTextSize());
  if ((!visible() && collapse_when_hidden_) || text_size.IsEmpty())
    return gfx::Size();

  gfx::Size size(gfx::GetStringWidth(base::string16(gfx::kEllipsisUTF16),
                                     font_list_),
                 font_list_.GetHeight());
  size.SetToMin(text_size);  // The actual text may be shorter than an ellipsis.
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

int Label::GetHeightForWidth(int w) const {
  if (!is_multi_line_)
    return View::GetHeightForWidth(w);

  w = std::max(0, w - GetInsets().width());

  for (size_t i = 0; i < cached_heights_.size(); ++i) {
    const gfx::Size& s = cached_heights_[i];
    if (s.width() == w)
      return s.height() + GetInsets().height();
  }

  int cache_width = w;

  int h = font_list_.GetHeight();
  const int flags = ComputeDrawStringFlags();
  gfx::Canvas::SizeStringInt(
      layout_text(), font_list_, &w, &h, line_height_, flags);
  cached_heights_[cached_heights_cursor_] = gfx::Size(cache_width, h);
  cached_heights_cursor_ = (cached_heights_cursor_ + 1) % kCachedSizeLimit;
  return h + GetInsets().height();
}

const char* Label::GetClassName() const {
  return kViewClassName;
}

View* Label::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // Bail out if the label does not contain the point.
  // Note that HitTestPoint() cannot be used here as it uses
  // Label::HitTestRect() to determine if the point hits the label; and
  // Label::HitTestRect() always fails. Instead, default HitTestRect()
  // implementation should be used.
  if (!View::HitTestRect(gfx::Rect(point, gfx::Size(1, 1))))
    return NULL;

  if (tooltip_text_.empty() && !ShouldShowDefaultTooltip())
    return NULL;

  return this;
}

bool Label::CanProcessEventsWithinSubtree() const {
  // Send events to the parent view for handling.
  return false;
}

bool Label::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  DCHECK(tooltip);

  // If a tooltip has been explicitly set, use it.
  if (!tooltip_text_.empty()) {
    tooltip->assign(tooltip_text_);
    return true;
  }

  // Show the full text if the text does not fit.
  if (ShouldShowDefaultTooltip()) {
    *tooltip = layout_text();
    return true;
  }

  return false;
}

void Label::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_STATIC_TEXT;
  state->AddStateFlag(ui::AX_STATE_READ_ONLY);
  state->name = layout_text();
}

void Label::PaintText(gfx::Canvas* canvas,
                      const base::string16& text,
                      const gfx::Rect& text_bounds,
                      int flags) {
  SkColor color = enabled() ? actual_enabled_color_ : actual_disabled_color_;
  if (elide_behavior_ == gfx::FADE_TAIL) {
    canvas->DrawFadedString(text, font_list_, color, text_bounds, flags);
  } else {
    canvas->DrawStringRectWithShadows(text, font_list_, color, text_bounds,
                                      line_height_, flags, shadows_);
  }

  if (HasFocus()) {
    gfx::Rect focus_bounds = text_bounds;
    focus_bounds.Inset(-kFocusBorderPadding, -kFocusBorderPadding);
    canvas->DrawFocusRect(focus_bounds);
  }
}

gfx::Size Label::GetTextSize() const {
  if (!text_size_valid_) {
    // For single-line strings, we supply the largest possible width, because
    // while adding NO_ELLIPSIS to the flags works on Windows for forcing
    // SizeStringInt() to calculate the desired width, it doesn't seem to work
    // on Linux.
    int w = is_multi_line_ ?
        GetAvailableRect().width() : std::numeric_limits<int>::max();
    int h = font_list_.GetHeight();
    // For single-line strings, ignore the available width and calculate how
    // wide the text wants to be.
    int flags = ComputeDrawStringFlags();
    if (!is_multi_line_)
      flags |= gfx::Canvas::NO_ELLIPSIS;
    gfx::Canvas::SizeStringInt(
        layout_text(), font_list_, &w, &h, line_height_, flags);
    text_size_.SetSize(w, h);
    const gfx::Insets shadow_margin = -gfx::ShadowValue::GetMargin(shadows_);
    text_size_.Enlarge(shadow_margin.width(), shadow_margin.height());
    text_size_valid_ = true;
  }

  return text_size_;
}

void Label::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  text_size_valid_ &= !is_multi_line_;
}

void Label::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  // We skip painting the focus border because it is being handled seperately by
  // some subclasses of Label. We do not want View's focus border painting to
  // interfere with that.
  OnPaintBorder(canvas);

  base::string16 paint_text;
  gfx::Rect text_bounds;
  int flags = 0;
  CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  PaintText(canvas, paint_text, text_bounds, flags);
}

void Label::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void Label::Init(const base::string16& text, const gfx::FontList& font_list) {
  font_list_ = font_list;
  enabled_color_set_ = disabled_color_set_ = background_color_set_ = false;
  subpixel_rendering_enabled_ = true;
  auto_color_readability_ = true;
  UpdateColorsFromTheme(ui::NativeTheme::instance());
  horizontal_alignment_ = gfx::ALIGN_CENTER;
  line_height_ = 0;
  is_multi_line_ = false;
  is_obscured_ = false;
  allow_character_break_ = false;
  elide_behavior_ = gfx::ELIDE_TAIL;
  collapse_when_hidden_ = false;
  directionality_mode_ = gfx::DIRECTIONALITY_FROM_UI;
  cached_heights_.resize(kCachedSizeLimit);
  ResetCachedSize();

  SetText(text);
}

void Label::RecalculateColors() {
  actual_enabled_color_ = auto_color_readability_ ?
      color_utils::GetReadableColor(requested_enabled_color_,
                                    background_color_) :
      requested_enabled_color_;
  actual_disabled_color_ = auto_color_readability_ ?
      color_utils::GetReadableColor(requested_disabled_color_,
                                    background_color_) :
      requested_disabled_color_;
}

gfx::Rect Label::GetTextBounds() const {
  gfx::Rect available(GetAvailableRect());
  gfx::Size text_size(GetTextSize());
  text_size.set_width(std::min(available.width(), text_size.width()));
  gfx::Point origin(GetInsets().left(), GetInsets().top());
  switch (GetHorizontalAlignment()) {
    case gfx::ALIGN_LEFT:
      break;
    case gfx::ALIGN_CENTER:
      // Put any extra margin pixel on the left to match the legacy behavior
      // from the use of GetTextExtentPoint32() on Windows.
      origin.Offset((available.width() + 1 - text_size.width()) / 2, 0);
      break;
    case gfx::ALIGN_RIGHT:
      origin.set_x(available.right() - text_size.width());
      break;
    default:
      NOTREACHED();
      break;
  }
  origin.Offset(0, std::max(0, (available.height() - text_size.height())) / 2);
  return gfx::Rect(origin, text_size);
}

int Label::ComputeDrawStringFlags() const {
  int flags = 0;

  // We can't use subpixel rendering if the background is non-opaque.
  if (SkColorGetA(background_color_) != 0xFF || !subpixel_rendering_enabled_)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;

  if (directionality_mode_ == gfx::DIRECTIONALITY_FORCE_LTR) {
    flags |= gfx::Canvas::FORCE_LTR_DIRECTIONALITY;
  } else if (directionality_mode_ == gfx::DIRECTIONALITY_FORCE_RTL) {
    flags |= gfx::Canvas::FORCE_RTL_DIRECTIONALITY;
  } else if (directionality_mode_ == gfx::DIRECTIONALITY_FROM_TEXT) {
    base::i18n::TextDirection direction =
        base::i18n::GetFirstStrongCharacterDirection(layout_text());
    if (direction == base::i18n::RIGHT_TO_LEFT)
      flags |= gfx::Canvas::FORCE_RTL_DIRECTIONALITY;
    else
      flags |= gfx::Canvas::FORCE_LTR_DIRECTIONALITY;
  }

  switch (GetHorizontalAlignment()) {
    case gfx::ALIGN_LEFT:
      flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
      break;
    case gfx::ALIGN_CENTER:
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
      break;
    case gfx::ALIGN_RIGHT:
      flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (!is_multi_line_)
    return flags;

  flags |= gfx::Canvas::MULTI_LINE;
#if !defined(OS_WIN)
    // Don't elide multiline labels on Linux.
    // Todo(davemoore): Do we depend on eliding multiline text?
    // Pango insists on limiting the number of lines to one if text is
    // elided. You can get around this if you can pass a maximum height
    // but we don't currently have that data when we call the pango code.
    flags |= gfx::Canvas::NO_ELLIPSIS;
#endif
  if (allow_character_break_)
    flags |= gfx::Canvas::CHARACTER_BREAK;

  return flags;
}

gfx::Rect Label::GetAvailableRect() const {
  gfx::Rect bounds(size());
  bounds.Inset(GetInsets());
  return bounds;
}

void Label::CalculateDrawStringParams(base::string16* paint_text,
                                      gfx::Rect* text_bounds,
                                      int* flags) const {
  DCHECK(paint_text && text_bounds && flags);

  const bool forbid_ellipsis = elide_behavior_ == gfx::TRUNCATE ||
                               elide_behavior_ == gfx::FADE_TAIL;
  if (is_multi_line_ || forbid_ellipsis) {
    *paint_text = layout_text();
  } else {
    *paint_text = gfx::ElideText(layout_text(), font_list_,
                                 GetAvailableRect().width(), elide_behavior_);
  }

  *text_bounds = GetTextBounds();
  *flags = ComputeDrawStringFlags();
  // TODO(msw): Elide multi-line text with ElideRectangleText instead.
  if (!is_multi_line_ || forbid_ellipsis)
    *flags |= gfx::Canvas::NO_ELLIPSIS;
}

void Label::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  if (!enabled_color_set_) {
    requested_enabled_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelEnabledColor);
  }
  if (!disabled_color_set_) {
    requested_disabled_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelDisabledColor);
  }
  if (!background_color_set_) {
    background_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelBackgroundColor);
  }
  RecalculateColors();
}

void Label::ResetCachedSize() {
  text_size_valid_ = false;
  cached_heights_cursor_ = 0;
  for (int i = 0; i < kCachedSizeLimit; ++i)
    cached_heights_[i] = gfx::Size();
}

bool Label::ShouldShowDefaultTooltip() const {
  return !is_multi_line_ && !is_obscured_ &&
         gfx::GetStringWidth(layout_text(), font_list_) >
             GetAvailableRect().width();
}

}  // namespace views
