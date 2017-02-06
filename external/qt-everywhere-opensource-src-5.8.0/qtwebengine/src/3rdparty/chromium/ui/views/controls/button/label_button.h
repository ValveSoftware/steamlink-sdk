// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

class InkDropRipple;
class InkDropHighlight;
class LabelButtonBorder;
class Painter;

// LabelButton is a button with text and an icon, it's not focusable by default.
class VIEWS_EXPORT LabelButton : public CustomButton,
                                 public NativeThemeDelegate {
 public:
  // The length of the hover fade animation.
  static const int kHoverAnimationDurationMs;

  // Amount to inset each edge of the button when drawing the focus rectangle.
  static const int kFocusRectInset;

  static const char kViewClassName[];

  LabelButton(ButtonListener* listener, const base::string16& text);
  ~LabelButton() override;

  // Gets or sets the image shown for the specified button state.
  // GetImage returns the image for STATE_NORMAL if the state's image is empty.
  virtual gfx::ImageSkia GetImage(ButtonState for_state) const;
  void SetImage(ButtonState for_state, const gfx::ImageSkia& image);

  // Gets or sets the text shown on the button.
  const base::string16& GetText() const;
  void SetText(const base::string16& text);

  // Sets the text color shown for the specified button |for_state| to |color|.
  void SetTextColor(ButtonState for_state, SkColor color);

  // Sets the text colors shown for the non-disabled states to |color|.
  virtual void SetEnabledTextColors(SkColor color);

  // Sets drop shadows underneath the text.
  void SetTextShadows(const gfx::ShadowValues& shadows);

  // Sets whether subpixel rendering is used on the label.
  void SetTextSubpixelRenderingEnabled(bool enabled);

  // Gets or sets the font list used by this button.
  const gfx::FontList& GetFontList() const;
  void SetFontList(const gfx::FontList& font_list);

  // Sets the elide behavior of this button.
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // Sets the horizontal alignment used for the button; reversed in RTL. The
  // optional image will lead the text, unless the button is right-aligned.
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Call SetMinSize(gfx::Size()) to clear the monotonically increasing size.
  void SetMinSize(const gfx::Size& min_size);
  void SetMaxSize(const gfx::Size& max_size);

  // Gets or sets the option to handle the return key; false by default.
  bool is_default() const { return is_default_; }
  void SetIsDefault(bool is_default);

  // Gets or sets the button's overall style; the default is |STYLE_TEXTBUTTON|.
  ButtonStyle style() const { return style_; }
  void SetStyle(ButtonStyle style);

  // Sets the spacing between the image and the text. Shrinking the spacing
  // will not shrink the overall button size, as it is monotonically increasing.
  // Call SetMinSize(gfx::Size()) to clear the size if needed.
  void SetImageLabelSpacing(int spacing);

  void SetFocusPainter(std::unique_ptr<Painter> focus_painter);
  Painter* focus_painter() { return focus_painter_.get(); }

  // Creates the default border for this button. This can be overridden by
  // subclasses.
  virtual std::unique_ptr<LabelButtonBorder> CreateDefaultBorder() const;

  // View:
  void SetBorder(std::unique_ptr<Border> border) override;
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  const char* GetClassName() const override;
  void EnableCanvasFlippingForRTLUI(bool flip) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const override;

 protected:
  ImageView* image() const { return image_; }
  Label* label() const { return label_; }

  bool explicitly_set_normal_color() const {
    return explicitly_set_colors_[STATE_NORMAL];
  }

  // Returns the available area for the label and image. Subclasses can change
  // these bounds if they need room to do manual painting.
  virtual gfx::Rect GetChildAreaBounds();

  // View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // CustomButton:
  void StateChanged() override;

  // Fills |params| with information about the button.
  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  // Resets colors from the NativeTheme, explicitly set colors are unchanged.
  virtual void ResetColorsFromNativeTheme();

  // Changes the visual styling of this button to reflect the state of
  // |is_default()|.
  virtual void UpdateStyleToIndicateDefaultStatus();

  // Updates the image view to contain the appropriate button state image.
  void UpdateImage();

  // Updates the border as per the NativeTheme, unless a different border was
  // set with SetBorder.
  void UpdateThemedBorder();

  // NativeThemeDelegate:
  gfx::Rect GetThemePaintRect() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Init);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Label);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Image);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, LabelAndImage);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, FontList);

  void SetTextInternal(const base::string16& text);

  // View:
  void ChildPreferredSizeChanged(View* child) override;

  // NativeThemeDelegate:
  ui::NativeTheme::Part GetThemePart() const override;
  ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  const gfx::Animation* GetThemeAnimation() const override;
  ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;

  // Resets |cached_preferred_size_| and marks |cached_preferred_size_valid_|
  // as false.
  void ResetCachedPreferredSize();

  // Updates additional state related to focus or default status, rather than
  // merely the CustomButton::state(). E.g. ensures the label text color is
  // correct for the current background.
  void ResetLabelEnabledColor();

  // The image and label shown in the button.
  ImageView* image_;
  Label* label_;

  // A separate view is necessary to hold the ink drop layer so that it can
  // be stacked below |image_| and on top of |label_|, without resorting to
  // drawing |label_| on a layer (which can mess with subpixel anti-aliasing).
  View* ink_drop_container_;

  // The cached font lists in the normal and bold style.
  gfx::FontList cached_normal_font_list_;
  gfx::FontList cached_bold_font_list_;

  // The images and colors for each button state.
  gfx::ImageSkia button_state_images_[STATE_COUNT];
  SkColor button_state_colors_[STATE_COUNT];

  // Used to track whether SetTextColor() has been invoked.
  bool explicitly_set_colors_[STATE_COUNT];

  // |min_size_| increases monotonically with the preferred size.
  mutable gfx::Size min_size_;
  // |max_size_| may be set to clamp the preferred size.
  gfx::Size max_size_;

  // Cache the last computed preferred size.
  mutable gfx::Size cached_preferred_size_;
  mutable bool cached_preferred_size_valid_;

  // Flag indicating default handling of the return key via an accelerator.
  // Whether or not the button appears or behaves as the default button in its
  // current context;
  bool is_default_;

  // The button's overall style.
  ButtonStyle style_;

  // True if current border was set by UpdateThemedBorder. Defaults to true.
  bool border_is_themed_border_;

  // Spacing between the image and the text.
  int image_label_spacing_;

  // Alignment of the button. This can be different from the alignment of the
  // text; for example, the label may be set to ALIGN_TO_HEAD (alignment matches
  // text direction) while |this| is laid out as ALIGN_LEFT (alignment matches
  // UI direction).
  gfx::HorizontalAlignment horizontal_alignment_;

  std::unique_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(LabelButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
