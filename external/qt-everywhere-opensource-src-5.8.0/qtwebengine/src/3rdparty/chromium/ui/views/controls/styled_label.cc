// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <limits>
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

std::unique_ptr<Label> CreateLabelRange(
    const base::string16& text,
    const gfx::FontList& font_list,
    const StyledLabel::RangeStyleInfo& style_info,
    views::LinkListener* link_listener) {
  std::unique_ptr<Label> result;

  if (style_info.is_link) {
    Link* link = new Link(text);
    link->set_listener(link_listener);
    link->SetUnderline((style_info.font_style & gfx::Font::UNDERLINE) != 0);
    result.reset(link);
  } else {
    result.reset(new Label(text));
  }

  if (style_info.color != SK_ColorTRANSPARENT)
    result->SetEnabledColor(style_info.color);
  result->SetFontList(font_list);

  if (!style_info.tooltip.empty())
    result->SetTooltipText(style_info.tooltip);
  if (style_info.font_style != gfx::Font::NORMAL ||
      style_info.weight != gfx::Font::Weight::NORMAL) {
    result->SetFontList(result->font_list().Derive(0, style_info.font_style,
                                                   style_info.weight));
  }

  return result;
}

}  // namespace


// StyledLabel::RangeStyleInfo ------------------------------------------------

StyledLabel::RangeStyleInfo::RangeStyleInfo()
    : font_style(gfx::Font::NORMAL),
      weight(gfx::Font::Weight::NORMAL),
      color(SK_ColorTRANSPARENT),
      disable_line_wrapping(false),
      is_link(false) {}

StyledLabel::RangeStyleInfo::~RangeStyleInfo() {}

// static
StyledLabel::RangeStyleInfo StyledLabel::RangeStyleInfo::CreateForLink() {
  RangeStyleInfo result;
  result.disable_line_wrapping = true;
  result.is_link = true;
  return result;
}


// StyledLabel::StyleRange ----------------------------------------------------

bool StyledLabel::StyleRange::operator<(
    const StyledLabel::StyleRange& other) const {
  return range.start() < other.range.start();
}


// StyledLabel ----------------------------------------------------------------

// static
const char StyledLabel::kViewClassName[] = "StyledLabel";

StyledLabel::StyledLabel(const base::string16& text,
                         StyledLabelListener* listener)
    : font_list_(Label().font_list()),
      specified_line_height_(0),
      listener_(listener),
      width_at_last_size_calculation_(0),
      width_at_last_layout_(0),
      displayed_on_background_color_(SkColorSetRGB(0xFF, 0xFF, 0xFF)),
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

void StyledLabel::SetLineHeight(int line_height) {
  specified_line_height_ = line_height;
  PreferredSizeChanged();
}

void StyledLabel::SetDisplayedOnBackgroundColor(SkColor color) {
  if (displayed_on_background_color_ == color &&
      displayed_on_background_color_set_)
    return;

  displayed_on_background_color_ = color;
  displayed_on_background_color_set_ = true;

  for (int i = 0, count = child_count(); i < count; ++i) {
    DCHECK((child_at(i)->GetClassName() == Label::kViewClassName) ||
           (child_at(i)->GetClassName() == Link::kViewClassName));
    static_cast<Label*>(child_at(i))->SetBackgroundColor(color);
  }
}

void StyledLabel::SizeToFit(int max_width) {
  if (max_width == 0)
    max_width = std::numeric_limits<int>::max();

  SetSize(CalculateAndDoLayout(max_width, true));
}

const char* StyledLabel::GetClassName() const {
  return kViewClassName;
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

gfx::Size StyledLabel::GetPreferredSize() const {
  return calculated_size_;
}

int StyledLabel::GetHeightForWidth(int w) const {
  // TODO(erg): Munge the const-ness of the style label. CalculateAndDoLayout
  // doesn't actually make any changes to member variables when |dry_run| is
  // set to true. In general, the mutating and non-mutating parts shouldn't
  // be in the same codepath.
  return const_cast<StyledLabel*>(this)->CalculateAndDoLayout(w, true).height();
}

void StyledLabel::Layout() {
  CalculateAndDoLayout(GetLocalBounds().width(), false);
}

void StyledLabel::PreferredSizeChanged() {
  calculated_size_ = gfx::Size();
  width_at_last_size_calculation_ = 0;
  width_at_last_layout_ = 0;
  View::PreferredSizeChanged();
}

void StyledLabel::LinkClicked(Link* source, int event_flags) {
  if (listener_)
    listener_->StyledLabelLinkClicked(this, link_targets_[source], event_flags);
}

gfx::Size StyledLabel::CalculateAndDoLayout(int width, bool dry_run) {
  if (width == width_at_last_size_calculation_ &&
      (dry_run || width == width_at_last_layout_))
    return calculated_size_;

  width_at_last_size_calculation_ = width;
  if (!dry_run)
    width_at_last_layout_ = width;

  width -= GetInsets().width();

  if (!dry_run) {
    RemoveAllChildViews(true);
    link_targets_.clear();
  }

  if (width <= 0 || text_.empty())
    return gfx::Size();

  const int line_height = specified_line_height_ > 0 ? specified_line_height_
      : CalculateLineHeight(font_list_);
  // The index of the line we're on.
  int line = 0;
  // The x position (in pixels) of the line we're on, relative to content
  // bounds.
  int x = 0;
  int total_height = 0;
  // The width that was actually used. Guaranteed to be no larger than |width|.
  int used_width = 0;

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
      text_font_list =
          text_font_list.Derive(0, current_range->style_info.font_style,
                                current_range->style_info.weight);
    }
    gfx::ElideRectangleText(remaining_string,
                            text_font_list,
                            chunk_bounds.width(),
                            chunk_bounds.height(),
                            gfx::WRAP_LONG_WORDS,
                            &substrings);

    if (substrings.empty() || substrings[0].empty()) {
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

    base::string16 chunk = substrings[0];

    std::unique_ptr<Label> label;
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

      if (chunk.size() > range.end() - position)
        chunk = chunk.substr(0, range.end() - position);

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
    label->SetBoundsRect(gfx::Rect(
        gfx::Point(
            GetInsets().left() + x - focus_border_insets.left(),
            GetInsets().top() + line * line_height - focus_border_insets.top()),
        view_size));
    x += view_size.width() - focus_border_insets.width();
    used_width = std::max(used_width, x);
    total_height = std::max(total_height, label->bounds().bottom());
    if (!dry_run)
      AddChildView(label.release());

    // If |gfx::ElideRectangleText| returned more than one substring, that
    // means the whole text did not fit into remaining line width, with text
    // after |susbtring[0]| spilling into next line. If whole |substring[0]|
    // was added to the current line (this may not be the case if part of the
    // substring has different style), proceed to the next line.
    if (substrings.size() > 1 && chunk.size() == substrings[0].size()) {
      x = 0;
      ++line;
    }

    remaining_string = remaining_string.substr(chunk.size());
  }

  DCHECK_LE(used_width, width);
  calculated_size_ = gfx::Size(used_width + GetInsets().width(), total_height);
  return calculated_size_;
}

}  // namespace views
