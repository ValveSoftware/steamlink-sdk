// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LABEL_H_
#define UI_VIEWS_CONTROLS_LABEL_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/render_text.h"
#include "ui/views/view.h"

namespace views {

// A view subclass that can display a string.
class VIEWS_EXPORT Label : public View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // The padding for the focus border when rendering focused text.
  static const int kFocusBorderPadding;

  Label();
  explicit Label(const base::string16& text);
  Label(const base::string16& text, const gfx::FontList& font_list);
  ~Label() override;

  // Gets or sets the fonts used by this label.
  const gfx::FontList& font_list() const { return render_text_->font_list(); }

  virtual void SetFontList(const gfx::FontList& font_list);

  // Get or set the label text.
  const base::string16& text() const { return render_text_->text(); }
  virtual void SetText(const base::string16& text);

  // Enables or disables auto-color-readability (enabled by default).  If this
  // is enabled, then calls to set any foreground or background color will
  // trigger an automatic mapper that uses color_utils::GetReadableColor() to
  // ensure that the foreground colors are readable over the background color.
  void SetAutoColorReadabilityEnabled(bool enabled);

  // Sets the color.  This will automatically force the color to be readable
  // over the current background color, if auto color readability is enabled.
  virtual void SetEnabledColor(SkColor color);
  void SetDisabledColor(SkColor color);

  SkColor enabled_color() const { return actual_enabled_color_; }

  // Sets the background color.  This won't be explicitly drawn, but the label
  // will force the text color to be readable over it.
  void SetBackgroundColor(SkColor color);
  SkColor background_color() const { return background_color_; }

  // Set drop shadows underneath the text.
  void SetShadows(const gfx::ShadowValues& shadows);
  const gfx::ShadowValues& shadows() const { return render_text_->shadows(); }

  // Sets whether subpixel rendering is used; the default is true, but this
  // feature also requires an opaque background color.
  // TODO(mukai): rename this as SetSubpixelRenderingSuppressed() to keep the
  // consistency with RenderText field name.
  void SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled);

  // Sets the horizontal alignment; the argument value is mirrored in RTL UI.
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);
  gfx::HorizontalAlignment horizontal_alignment() const {
    return render_text_->horizontal_alignment();
  }

  // Get or set the distance in pixels between baselines of multi-line text.
  // Default is 0, indicating the distance between lines should be the standard
  // one for the label's text, font list, and platform.
  int line_height() const { return render_text_->min_line_height(); }
  void SetLineHeight(int height);

  // Get or set if the label text can wrap on multiple lines; default is false.
  bool multi_line() const { return multi_line_; }
  void SetMultiLine(bool multi_line);

  // Get or set if the label text should be obscured before rendering (e.g.
  // should "Password!" display as "*********"); default is false.
  bool obscured() const { return render_text_->obscured(); }
  void SetObscured(bool obscured);

  // Sets whether multi-line text can wrap mid-word; the default is false.
  // TODO(mukai): allow specifying WordWrapBehavior.
  void SetAllowCharacterBreak(bool allow_character_break);

  // Sets the eliding or fading behavior, applied as necessary. The default is
  // to elide at the end. Eliding is not well-supported for multi-line labels.
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);
  gfx::ElideBehavior elide_behavior() const { return elide_behavior_; }

  // Sets the tooltip text.  Default behavior for a label (single-line) is to
  // show the full text if it is wider than its bounds.  Calling this overrides
  // the default behavior and lets you set a custom tooltip.  To revert to
  // default behavior, call this with an empty string.
  void SetTooltipText(const base::string16& tooltip_text);

  // Get or set whether this label can act as a tooltip handler; the default is
  // true.  Set to false whenever an ancestor view should handle tooltips
  // instead.
  bool handles_tooltips() const { return handles_tooltips_; }
  void SetHandlesTooltips(bool enabled);

  // Resizes the label so its width is set to the fixed width and its height
  // deduced accordingly. Even if all widths of the lines are shorter than
  // |fixed_width|, the given value is applied to the element's width.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |fixed_width| is the fixed width that will be used (longer lines will be
  // wrapped).  If 0, no fixed width is enforced.
  void SizeToFit(int fixed_width);

  // Like SizeToFit, but uses a smaller width if possible.
  void SetMaximumWidth(int max_width);

  // Sets whether the preferred size is empty when the label is not visible.
  void set_collapse_when_hidden(bool value) { collapse_when_hidden_ = value; }

  // Get the text as displayed to the user, respecting the obscured flag.
  base::string16 GetDisplayTextForTesting();

  // View:
  gfx::Insets GetInsets() const override;
  int GetBaseline() const override;
  gfx::Size GetPreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  const char* GetClassName() const override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool CanProcessEventsWithinSubtree() const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  void OnEnabledChanged() override;

 protected:
  // Create a single RenderText instance to actually be painted.
  virtual std::unique_ptr<gfx::RenderText> CreateRenderText(
      const base::string16& text,
      gfx::HorizontalAlignment alignment,
      gfx::DirectionalityMode directionality,
      gfx::ElideBehavior elide_behavior);

  void PaintText(gfx::Canvas* canvas);

  SkColor disabled_color() const { return actual_disabled_color_; }

  // View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelTest, ResetRenderTextData);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, MultilineSupportedRenderText);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, TextChangeWithoutLayout);
  FRIEND_TEST_ALL_PREFIXES(LabelFocusTest, FocusBounds);
  FRIEND_TEST_ALL_PREFIXES(LabelFocusTest, EmptyLabel);

  void Init(const base::string16& text, const gfx::FontList& font_list);

  void ResetLayout();

  // Set up |lines_| to actually be painted.
  void MaybeBuildRenderTextLines();

  gfx::Rect GetFocusBounds();

  // Get the text broken into lines as needed to fit the given |width|.
  std::vector<base::string16> GetLinesForWidth(int width) const;

  // Get the text size for the current layout.
  gfx::Size GetTextSize() const;

  // Updates |actual_{enabled,disabled}_color_| from requested colors.
  void RecalculateColors();

  // Applies |actual_{enabled,disabled}_color_| to |lines_|.
  void ApplyTextColors();

  // Updates any colors that have not been explicitly set from the theme.
  void UpdateColorsFromTheme(const ui::NativeTheme* theme);

  bool ShouldShowDefaultTooltip() const;

  // An un-elided and single-line RenderText object used for preferred sizing.
  std::unique_ptr<gfx::RenderText> render_text_;

  // The RenderText instances used to display elided and multi-line text.
  std::vector<std::unique_ptr<gfx::RenderText>> lines_;

  SkColor requested_enabled_color_;
  SkColor actual_enabled_color_;
  SkColor requested_disabled_color_;
  SkColor actual_disabled_color_;
  SkColor background_color_;

  // Set to true once the corresponding setter is invoked.
  bool enabled_color_set_;
  bool disabled_color_set_;
  bool background_color_set_;

  gfx::ElideBehavior elide_behavior_;

  bool subpixel_rendering_enabled_;
  bool auto_color_readability_;
  // TODO(mukai): remove |multi_line_| when all RenderText can render multiline.
  bool multi_line_;
  base::string16 tooltip_text_;
  bool handles_tooltips_;
  // Whether to collapse the label when it's not visible.
  bool collapse_when_hidden_;
  int fixed_width_;
  int max_width_;

  // TODO(ckocagil): Remove is_first_paint_text_ before crbug.com/441028 is
  // closed.
  bool is_first_paint_text_;

  DISALLOW_COPY_AND_ASSIGN(Label);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LABEL_H_
