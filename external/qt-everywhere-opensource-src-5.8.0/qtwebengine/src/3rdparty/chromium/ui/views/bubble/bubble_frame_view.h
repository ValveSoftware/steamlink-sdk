// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class FontList;
}

namespace views {

class Label;
class LabelButton;
class BubbleBorder;
class ImageView;

// The non-client frame view of bubble-styled widgets.
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView,
                                     public ButtonListener {
 public:
  // Internal class name.
  static const char kViewClassName[];

  BubbleFrameView(const gfx::Insets& title_margins,
                  const gfx::Insets& content_margins);
  ~BubbleFrameView() override;

  // Creates a close button used in the corner of the dialog.
  static LabelButton* CreateCloseButton(ButtonListener* listener);

  // NonClientFrameView overrides:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  bool GetClientMask(const gfx::Size& size, gfx::Path* path) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // Set the FontList to be used for the title of the bubble.
  // Caller must arrange to update the layout to have the call take effect.
  void SetTitleFontList(const gfx::FontList& font_list);

  // View overrides:
  const char* GetClassName() const override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetPreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const ui::PaintContext& context) override;
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Overridden from ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Use bubble_border() and SetBubbleBorder(), not border() and SetBorder().
  BubbleBorder* bubble_border() const { return bubble_border_; }
  void SetBubbleBorder(std::unique_ptr<BubbleBorder> border);

  gfx::Insets content_margins() const { return content_margins_; }

  void SetFootnoteView(View* view);

  // Given the size of the contents and the rect to point at, returns the bounds
  // of the bubble window. The bubble's arrow location may change if the bubble
  // does not fit on the monitor and |adjust_if_offscreen| is true.
  gfx::Rect GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                   gfx::Size client_size,
                                   bool adjust_if_offscreen);

  bool close_button_clicked() const { return close_button_clicked_; }

  LabelButton* GetCloseButtonForTest() { return close_; }

 protected:
  // Returns the available screen bounds if the frame were to show in |rect|.
  virtual gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const;

  bool IsCloseButtonVisible() const;
  gfx::Rect GetCloseButtonMirroredBounds() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, GetBoundsForClientView);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CloseReasons);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateTest, CloseMethods);

  // Mirrors the bubble's arrow location on the |vertical| or horizontal axis,
  // if the generated window bounds don't fit in the monitor bounds.
  void MirrorArrowIfOffScreen(bool vertical,
                              const gfx::Rect& anchor_rect,
                              const gfx::Size& client_size);

  // Adjust the bubble's arrow offsets if the generated window bounds don't fit
  // in the monitor bounds.
  void OffsetArrowIfOffScreen(const gfx::Rect& anchor_rect,
                              const gfx::Size& client_size);

  // Calculates the size needed to accommodate the given client area.
  gfx::Size GetSizeForClientSize(const gfx::Size& client_size) const;

  // The bubble border.
  BubbleBorder* bubble_border_;

  // Margins around the title label.
  gfx::Insets title_margins_;

  // Margins between the content and the inside of the border, in pixels.
  gfx::Insets content_margins_;

  // The optional title icon, title, and (x) close button.
  views::ImageView* title_icon_;
  Label* title_;
  LabelButton* close_;

  // A view to contain the footnote view, if it exists.
  View* footnote_container_;

  // Whether the close button was clicked.
  bool close_button_clicked_;

  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
