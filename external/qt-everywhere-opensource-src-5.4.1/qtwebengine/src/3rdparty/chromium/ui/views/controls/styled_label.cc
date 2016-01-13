// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <vector>

#include "base/strings/string_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {


// Helpers --------------------------------------------------------------------

namespace {

// Calculates the height of a line of text. Currently returns the height of
// a label.
int CalculateLineHeight(const gfx::FontList& font_list) {
  Label label;
  label.SetFontList(font_list);
  return label.GetPreferredSize().height();
}

scoped_ptr<Label> CreateLabelRange(
    const base::string16& text,
    const gfx::FontList& font_list,
    const StyledLabel::RangeStyleInfo& style_info,
    views::LinkListener* link_listener) {
  scoped_ptr<Label> result;

  if (style_info.is_link) {
    Link* link = new Link(text);
    link->set_listener(link_listener);
    link->SetUnderline((style_info.font_style & gfx::Font::UNDERLINE) != 0);
    result.reset(link);
  } else {
    result.reset(new Label(text));
  }

  result->SetEnabledColor(style_info.color);
  result->SetFontList(font_list);

  if (!style_info.tooltip.empty())
    result->SetTooltipText(style_info.tooltip);
  if (style_info.font_style != gfx::Font::NORMAL) {
    result->SetFontList(
        result->font_list().DeriveWithStyle(style_info.font_style));
  }

  return result.Pass();
}

}  // namespace


// StyledLabel::RangeStyleInfo ------------------------------------------------

StyledLabel::RangeStyleInfo::RangeStyleInfo()
    : font_style(gfx::Font::NORMAL),
      color(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelEnabledColor)),
      disable_line_wrapping(false),
      is_link(false) {}

StyledLabel::RangeStyleInfo::~RangeStyleInfo() {}

// static
StyledLabel::RangeStyleInfo StyledLabel::RangeStyleInfo::CreateForLink() {
  RangeStyleInfo result;
  result.disable_line_wrapping = true;
  result.is_link = true;
  result.color = Link::GetDefaultEnabledColor();
  return result;
}


// StyledLabel::StyleRange ----------------------------------------------------

bool StyledLabel::StyleRange::operator<(
    const StyledLabel::StyleRange& other) const {
  return range.start() < other.range.start();
}


// StyledLabel ----------------------------------------------------------------

StyledLabel::StyledLabel(const base::string16& text,
                         StyledLabelListener* listener)
    : listener_(listener),
      displayed_on_background_color_set_(false),
      auto_color_readability_enabled_(true) {
  base::TrimWhitespace(text, base::TRIM_TRAILING, &text_);
}

StyledLabel::~StyledLabel() {}

void StyledLabel::SetText(const base::string16& text) {
  text_ = text;
  style_ranges_.clear();
  RemoveAllChildViews(true);
  PreferredSizeChanged();
}

void StyledLabel::SetBaseFontList(const gfx::FontList& font_list) {
  font_list_ = font_list;
  PreferredSizeChanged();
}

void StyledLabel::AddStyleRange(const gfx::Range& range,
                                const RangeStyleInfo& style_info) {
  DCHECK(!range.is_reversed());
  DCHECK(!range.is_empty());
  DCHECK(gfx::Range(0, text_.size()).Contains(range));

  // Insert the new range in sorted order.
  StyleRanges new_range;
  new_range.push_front(StyleRange(range, style_info));
  style_ranges_.merge(new_range);

  PreferredSizeChanged();
}

void StyledLabel::SetDefaultStyle(const RangeStyleInfo& style_info) {
  default_style_info_ = style_info;
  PreferredSizeChanged();
}

void StyledLabel::SetDisplayedOnBackgroundColor(SkColor color) {
  displayed_on_background_color_ = color;
  displayed_on_background_color_set_ = true;
}

gfx::Insets StyledLabel::GetInsets() const {
  gfx::Insets insets = View::GetInsets();

  // We need a focus border iff we contain a link that will have a focus border.
  // That in turn will be true only if the link is non-empty.
  for (StyleRanges::const_iterator i(style_ranges_.begin());
        i != style_ranges_.end(); ++i) {
    if (i->style_info.is_link && !i->range.is_empty()) {
      const gfx::Insets focus_border_padding(
          Label::kFocusBorderPadding, Label::kFocusBorderPadding,
          Label::kFocusBorderPadding, Label::kFocusBorderPadding);
      insets += focus_border_padding;
      break;
    }
  }

  return insets;
}

int StyledLabel::GetHeightForWidth(int w) const {
  if (w != calculated_size_.width()) {
    // TODO(erg): Munge the const-ness of the style label. CalculateAndDoLayout
    // doesn't actually make any changes to member variables when |dry_run| is
    // set to true. In general, the mutating and non-mutating parts shouldn't
    // be in the same codepath.
    calculated_size_ =
        const_cast<StyledLabel*>(this)->CalculateAndDoLayout(w, true);
  }
  return calculated_size_.height();
}

void StyledLabel::Layout() {
  calculated_size_ = CalculateAndDoLayout(GetLocalBounds().width(), false);
}

void StyledLabel::PreferredSizeChanged() {
  calculated_size_ = gfx::Size();
  View::PreferredSizeChanged();
}

void StyledLabel::LinkClicked(Link* source, int event_flags) {
  if (listener_)
    listener_->StyledLabelLinkClicked(link_targets_[source], event_flags);
}

gfx::Size StyledLabel::CalculateAndDoLayout(int width, bool dry_run) {
  if (!dry_run) {
    RemoveAllChildViews(true);
    link_targets_.clear();
  }

  width -= GetInsets().width();
  if (width <= 0 || text_.empty())
    return gfx::Size();

  const int line_height = CalculateLineHeight(font_list_);
  // The index of the line we're on.
  int line = 0;
  // The x position (in pixels) of the line we're on, relative to content
  // bounds.
  int x = 0;

  base::string16 remaining_string = text_;
  StyleRanges::const_iterator current_range = style_ranges_.begin();

  // Iterate over the text, creating a bunch of labels and links and laying them
  // out in the appropriate positions.
  while (!remaining_string.empty()) {
    // Don't put whitespace at beginning of a line with an exception for the
    // first line (so the text's leading whitespace is respected).
    if (x == 0 && line > 0) {
      base::TrimWhitespace(remaining_string, base::TRIM_LEADING,
                           &remaining_string);
    }

    gfx::Range range(gfx::Range::InvalidRange());
    if (current_range != style_ranges_.end())
      range = current_range->range;

    const size_t position = text_.size() - remaining_string.size();

    const gfx::Rect chunk_bounds(x, 0, width - x, 2 * line_height);
    std::vector<base::string16> substrings;
    gfx::FontList text_font_list = font_list_;
    // If the start of the remaining text is inside a styled range, the font
    // style may differ from the base font. The font specified by the range
    // should be used when eliding text.
    if (position >= range.start()) {
      text_font_list = text_font_list.DeriveWithStyle(
          current_range->style_info.font_style);
    }
    gfx::ElideRectangleText(remaining_string,
                            text_font_list,
                            chunk_bounds.width(),
                            chunk_bounds.height(),
                            gfx::IGNORE_LONG_WORDS,
                            &substrings);

    DCHECK(!substrings.empty());
    base::string16 chunk = substrings[0];
    if (chunk.empty()) {
      // Nothing fits on this line. Start a new line.
      // If x is 0, first line may have leading whitespace that doesn't fit in a
      // single line, so try trimming those. Otherwise there is no room for
      // anything; abort.
      if (x == 0) {
        if (line == 0) {
          base::TrimWhitespace(remaining_string, base::TRIM_LEADING,
                               &remaining_string);
          continue;
        }
        break;
      }

      x = 0;
      line++;
      continue;
    }

    scoped_ptr<Label> label;
    if (position >= range.start()) {
      const RangeStyleInfo& style_info = current_range->style_info;

      if (style_info.disable_line_wrapping && chunk.size() < range.length() &&
          position == range.start() && x != 0) {
        // If the chunk should not be wrapped, try to fit it entirely on the
        // next line.
        x = 0;
        line++;
        continue;
      }

      chunk = chunk.substr(0, std::min(chunk.size(), range.end() - position));

      label = CreateLabelRange(chunk, font_list_, style_info, this);

      if (style_info.is_link && !dry_run)
        link_targets_[label.get()] = range;

      if (position + chunk.size() >= range.end())
        ++current_range;
    } else {
      // This chunk is normal text.
      if (position + chunk.size() > range.start())
        chunk = chunk.substr(0, range.start() - position);
      label = CreateLabelRange(chunk, font_list_, default_style_info_, this);
    }

    if (displayed_on_background_color_set_)
      label->SetBackgroundColor(displayed_on_background_color_);
    label->SetAutoColorReadabilityEnabled(auto_color_readability_enabled_);

    // Calculate the size of the optional focus border, and overlap by that
    // amount. Otherwise, "<a>link</a>," will render as "link ,".
    gfx::Insets focus_border_insets(label->GetInsets());
    focus_border_insets += -label->View::GetInsets();
    const gfx::Size view_size = label->GetPreferredSize();
    DCHECK_EQ(line_height, view_size.height() - focus_border_insets.height());
    if (!dry_run) {
      label->SetBoundsRect(gfx::Rect(
          gfx::Point(GetInsets().left() + x - focus_border_insets.left(),
                     GetInsets().top() + line * line_height -
                         focus_border_insets.top()),
          view_size));
      AddChildView(label.release());
    }
    x += view_size.width() - focus_border_insets.width();

    remaining_string = remaining_string.substr(chunk.size());
  }

  return gfx::Size(width, (line + 1) * line_height + GetInsets().height());
}

}  // namespace views
