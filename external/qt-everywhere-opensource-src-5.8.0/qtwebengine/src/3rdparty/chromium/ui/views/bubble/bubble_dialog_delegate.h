// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace gfx {
class FontList;
class Rect;
}

namespace views {

class BubbleFrameView;

// BubbleDialogDelegateView is a special DialogDelegateView for bubbles.
class VIEWS_EXPORT BubbleDialogDelegateView : public DialogDelegateView,
                                              public WidgetObserver {
 public:
  // Internal class name.
  static const char kViewClassName[];

  enum class CloseReason {
    DEACTIVATION,
    CLOSE_BUTTON,
    UNKNOWN,
  };

  ~BubbleDialogDelegateView() override;

  // Create and initialize the bubble Widget(s) with proper bounds.
  static Widget* CreateBubble(BubbleDialogDelegateView* bubble_delegate);

  // WidgetDelegateView overrides:
  BubbleDialogDelegateView* AsBubbleDialogDelegate() override;
  bool ShouldShowCloseButton() const override;
  ClientView* CreateClientView(Widget* widget) override;
  NonClientFrameView* CreateNonClientFrameView(Widget* widget) override;
  const char* GetClassName() const override;

  // WidgetObserver overrides:
  void OnWidgetDestroying(Widget* widget) override;
  void OnWidgetVisibilityChanging(Widget* widget, bool visible) override;
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(Widget* widget, bool active) override;
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override;

  bool close_on_deactivate() const { return close_on_deactivate_; }
  void set_close_on_deactivate(bool close) { close_on_deactivate_ = close; }

  View* GetAnchorView() const;
  Widget* anchor_widget() const { return anchor_widget_; }

  // The anchor rect is used in the absence of an assigned anchor view.
  const gfx::Rect& anchor_rect() const { return anchor_rect_; }

  BubbleBorder::Arrow arrow() const { return arrow_; }
  void set_arrow(BubbleBorder::Arrow arrow) { arrow_ = arrow; }

  void set_mirror_arrow_in_rtl(bool mirror) { mirror_arrow_in_rtl_ = mirror; }

  BubbleBorder::Shadow shadow() const { return shadow_; }
  void set_shadow(BubbleBorder::Shadow shadow) { shadow_ = shadow; }

  SkColor color() const { return color_; }
  void set_color(SkColor color) {
    color_ = color;
    color_explicitly_set_ = true;
  }

  const gfx::Insets& margins() const { return margins_; }
  void set_margins(const gfx::Insets& margins) { margins_ = margins; }

  const gfx::Insets& anchor_view_insets() const { return anchor_view_insets_; }
  void set_anchor_view_insets(const gfx::Insets& i) { anchor_view_insets_ = i; }

  gfx::NativeView parent_window() const { return parent_window_; }
  void set_parent_window(gfx::NativeView window) { parent_window_ = window; }

  bool accept_events() const { return accept_events_; }
  void set_accept_events(bool accept_events) { accept_events_ = accept_events; }

  bool border_accepts_events() const { return border_accepts_events_; }
  void set_border_accepts_events(bool event) { border_accepts_events_ = event; }

  bool adjust_if_offscreen() const { return adjust_if_offscreen_; }
  void set_adjust_if_offscreen(bool adjust) { adjust_if_offscreen_ = adjust; }

  // Get the arrow's anchor rect in screen space.
  virtual gfx::Rect GetAnchorRect() const;

  // Allows delegates to provide custom parameters before widget initialization.
  // For example, mus needs to set a custom mus::Window* parent.
  virtual void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                        Widget* widget) const;

  // Sets |margins_| to a default picked for smaller bubbles.
  void UseCompactMargins();

  // Sets the bubble alignment relative to the anchor. This may only be called
  // after calling CreateBubble.
  void SetAlignment(BubbleBorder::BubbleAlignment alignment);

  // Sets the bubble arrow paint type.
  void SetArrowPaintType(BubbleBorder::ArrowPaintType paint_type);

  // Call this method when the anchor bounds have changed to reposition the
  // bubble. The bubble is automatically repositioned when the anchor view
  // bounds change as a result of the widget's bounds changing.
  void OnAnchorBoundsChanged();

 protected:
  BubbleDialogDelegateView();
  BubbleDialogDelegateView(View* anchor_view, BubbleBorder::Arrow arrow);

  // Get bubble bounds from the anchor rect and client view's preferred size.
  virtual gfx::Rect GetBubbleBounds();

  // Return a FontList to use for the title of the bubble.
  // (The default is MediumFont).
  virtual const gfx::FontList& GetTitleFontList() const;

  // View overrides:
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Perform view initialization on the contents for bubble sizing.
  virtual void Init();

  // Sets the anchor view or rect and repositions the bubble. Note that if a
  // valid view gets passed, the anchor rect will get ignored. If the view gets
  // deleted, but no new view gets set, the last known anchor postion will get
  // returned.
  void SetAnchorView(View* anchor_view);
  void SetAnchorRect(const gfx::Rect& rect);

  // Resize and potentially move the bubble to fit the content's preferred size.
  void SizeToContents();

  BubbleFrameView* GetBubbleFrameView() const;

 private:
  friend class BubbleBorderDelegate;
  friend class BubbleWindowTargeter;

  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CreateDelegate);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, NonClientHitTest);

  // Update the bubble color from |theme|, unless it was explicitly set.
  void UpdateColorsFromTheme(const ui::NativeTheme* theme);

  // Handles widget visibility changes.
  void HandleVisibilityChanged(Widget* widget, bool visible);

  // A flag controlling bubble closure on deactivation.
  bool close_on_deactivate_;

  // The view and widget to which this bubble is anchored. Since an anchor view
  // can be deleted without notice, we store it in the ViewStorage and retrieve
  // it from there. It will make sure that the view is still valid.
  const int anchor_view_storage_id_;
  Widget* anchor_widget_;

  // The anchor rect used in the absence of an anchor view.
  mutable gfx::Rect anchor_rect_;

  // The arrow's location on the bubble.
  BubbleBorder::Arrow arrow_;

  // Automatically mirror the arrow in RTL layout.
  bool mirror_arrow_in_rtl_;

  // Bubble border shadow to use.
  BubbleBorder::Shadow shadow_;

  // The background color of the bubble; and flag for when it's explicitly set.
  SkColor color_;
  bool color_explicitly_set_;

  // The margins between the content and the inside of the border.
  gfx::Insets margins_;

  // Insets applied to the |anchor_view_| bounds.
  gfx::Insets anchor_view_insets_;

  // Specifies whether the bubble (or its border) handles mouse events, etc.
  bool accept_events_;
  bool border_accepts_events_;

  // If true (defaults to true), the arrow may be mirrored and moved to fit the
  // bubble on screen better. It would be a no-op if the bubble has no arrow.
  bool adjust_if_offscreen_;

  // Parent native window of the bubble.
  gfx::NativeView parent_window_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDialogDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DELEGATE2_H_
