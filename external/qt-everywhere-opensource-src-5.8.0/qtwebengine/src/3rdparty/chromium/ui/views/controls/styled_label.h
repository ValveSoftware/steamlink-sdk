// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_STYLED_LABEL_H_
#define UI_VIEWS_CONTROLS_STYLED_LABEL_H_

#include <list>
#include <map>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"

namespace views {

class Link;
class StyledLabelListener;

// A class which can apply mixed styles to a block of text. Currently, text is
// always multiline. Trailing whitespace in the styled label text is not
// supported and will be trimmed on StyledLabel construction. Leading
// whitespace is respected, provided not only whitespace fits in the first line.
// In this case, leading whitespace is ignored.
class VIEWS_EXPORT StyledLabel : public View, public LinkListener {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // Parameters that define label style for a styled label's text range.
  struct VIEWS_EXPORT RangeStyleInfo {
    RangeStyleInfo();
    ~RangeStyleInfo();

    // Creates a range style info with default values for link.
    static RangeStyleInfo CreateForLink();

    // The font style that will be applied to the range. Should be a bitmask of
    // values defined in gfx::Font::FontStyle (ITALIC, UNDERLINE).
    int font_style;

    // The font weight to be applied to the range. Default is Weight::NORMAL.
    gfx::Font::Weight weight;

    // The text color for the range. Default is SK_ColorTRANSPARENT, indicating
    // the theme's default color should be used.
    SkColor color;

    // Tooltip for the range.
    base::string16 tooltip;

    // If set, the whole range will be put on a single line.
    bool disable_line_wrapping;

    // If set, the range will be created as a link.
    bool is_link;
  };

  // Note that any trailing whitespace in |text| will be trimmed.
  StyledLabel(const base::string16& text, StyledLabelListener* listener);
  ~StyledLabel() override;

  // Sets the text to be displayed, and clears any previous styling.
  void SetText(const base::string16& text);

  // Sets the fonts used by all labels. Can be augemented by styling set by
  // AddStyleRange and SetDefaultStyle.
  void SetBaseFontList(const gfx::FontList& font_list);

  // Marks the given range within |text_| with style defined by |style_info|.
  // |range| must be contained in |text_|.
  void AddStyleRange(const gfx::Range& range, const RangeStyleInfo& style_info);

  // Sets the default style to use for any part of the text that isn't within
  // a range set by AddStyleRange.
  void SetDefaultStyle(const RangeStyleInfo& style_info);

  // Get or set the distance in pixels between baselines of multi-line text.
  // Default is 0, indicating the distance between lines should be the standard
  // one for the label's text, font list, and platform.
  void SetLineHeight(int height);

  // Sets the color of the background on which the label is drawn. This won't
  // be explicitly drawn, but the label will force the text color to be
  // readable over it.
  void SetDisplayedOnBackgroundColor(SkColor color);
  SkColor displayed_on_background_color() const {
    return displayed_on_background_color_;
  }

  void set_auto_color_readability_enabled(bool auto_color_readability) {
    auto_color_readability_enabled_ = auto_color_readability;
  }

  // Resizes the label so its width is set to the width of the longest line and
  // its height deduced accordingly.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |max_width| is the maximum width that will be used (longer lines will be
  // wrapped). If 0, no maximum width is enforced.
  void SizeToFit(int max_width);

  // View:
  const char* GetClassName() const override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  void PreferredSizeChanged() override;

  // LinkListener implementation:
  void LinkClicked(Link* source, int event_flags) override;

 private:
  struct StyleRange {
    StyleRange(const gfx::Range& range,
               const RangeStyleInfo& style_info)
        : range(range),
          style_info(style_info) {
    }
    ~StyleRange() {}

    bool operator<(const StyleRange& other) const;

    gfx::Range range;
    RangeStyleInfo style_info;
  };
  typedef std::list<StyleRange> StyleRanges;

  // Calculates how to layout child views, creates them and sets their size and
  // position. |width| is the horizontal space, in pixels, that the view has to
  // work with. If |dry_run| is true, the view hierarchy is not touched. Caches
  // the results in |calculated_size_|, |width_at_last_layout_|, and
  // |width_at_last_size_calculation_|. Returns the needed size.
  gfx::Size CalculateAndDoLayout(int width, bool dry_run);

  // The text to display.
  base::string16 text_;

  // Fonts used to display text. Can be augmented by RangeStyleInfo.
  gfx::FontList font_list_;

  // Line height.
  int specified_line_height_;

  // The default style to use for any part of the text that isn't within
  // a range in |style_ranges_|.
  RangeStyleInfo default_style_info_;

  // The listener that will be informed of link clicks.
  StyledLabelListener* listener_;

  // The ranges that should be linkified, sorted by start position.
  StyleRanges style_ranges_;

  // A mapping from a view to the range it corresponds to in |text_|. Only views
  // that correspond to ranges with is_link style set will be added to the map.
  std::map<View*, gfx::Range> link_targets_;

  // This variable saves the result of the last GetHeightForWidth call in order
  // to avoid repeated calculation.
  mutable gfx::Size calculated_size_;
  mutable int width_at_last_size_calculation_;
  int width_at_last_layout_;

  // Background color on which the label is drawn, for auto color readability.
  SkColor displayed_on_background_color_;
  bool displayed_on_background_color_set_;

  // Controls whether the text is automatically re-colored to be readable on the
  // background.
  bool auto_color_readability_enabled_;

  DISALLOW_COPY_AND_ASSIGN(StyledLabel);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_STYLED_LABEL_H_
