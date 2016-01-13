// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

#include "base/gtest_prod_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class FontList;
class Rect;
}

namespace views {

class BubbleFrameView;

// BubbleDelegateView creates frame and client views for bubble Widgets.
// BubbleDelegateView itself is the client's contents view.
class VIEWS_EXPORT BubbleDelegateView : public WidgetDelegateView,
                                        public WidgetObserver {
 public:
  BubbleDelegateView();
  BubbleDelegateView(View* anchor_view, BubbleBorder::Arrow arrow);
  virtual ~BubbleDelegateView();

  // Create and initialize the bubble Widget(s) with proper bounds.
  static Widget* CreateBubble(BubbleDelegateView* bubble_delegate);

  // WidgetDelegateView overrides:
  virtual BubbleDelegateView* AsBubbleDelegate() OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual bool ShouldShowCloseButton() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView(Widget* widget) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // WidgetObserver overrides:
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanging(Widget* widget, bool visible)
      OVERRIDE;
  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible)
      OVERRIDE;
  virtual void OnWidgetActivationChanged(Widget* widget, bool active) OVERRIDE;
  virtual void OnWidgetBoundsChanged(Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  bool close_on_esc() const { return close_on_esc_; }
  void set_close_on_esc(bool close_on_esc) { close_on_esc_ = close_on_esc; }

  bool close_on_deactivate() const { return close_on_deactivate_; }
  void set_close_on_deactivate(bool close) { close_on_deactivate_ = close; }

  View* GetAnchorView() const;
  Widget* anchor_widget() const { return anchor_widget_; }

  // The anchor rect is used in the absence of an assigned anchor view.
  const gfx::Rect& anchor_rect() const { return anchor_rect_; }

  BubbleBorder::Arrow arrow() const { return arrow_; }
  void set_arrow(BubbleBorder::Arrow arrow) { arrow_ = arrow; }

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

  bool use_focusless() const { return use_focusless_; }
  void set_use_focusless(bool focusless) { use_focusless_ = focusless; }

  bool accept_events() const { return accept_events_; }
  void set_accept_events(bool accept_events) { accept_events_ = accept_events; }

  bool border_accepts_events() const { return border_accepts_events_; }
  void set_border_accepts_events(bool event) { border_accepts_events_ = event; }

  bool adjust_if_offscreen() const { return adjust_if_offscreen_; }
  void set_adjust_if_offscreen(bool adjust) { adjust_if_offscreen_ = adjust; }

  // Get the arrow's anchor rect in screen space.
  virtual gfx::Rect GetAnchorRect() const;

  // Allows delegates to provide custom parameters before widget initialization.
  virtual void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                        Widget* widget) const;

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
  // Get bubble bounds from the anchor rect and client view's preferred size.
  virtual gfx::Rect GetBubbleBounds();

  // Return a FontList to use for the title of the bubble.
  // (The default is MediumFont).
  virtual const gfx::FontList& GetTitleFontList() const;

  // View overrides:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

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

  // Flags controlling bubble closure on the escape key and deactivation.
  bool close_on_esc_;
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

  // Bubble border shadow to use.
  BubbleBorder::Shadow shadow_;

  // The background color of the bubble; and flag for when it's explicitly set.
  SkColor color_;
  bool color_explicitly_set_;

  // The margins between the content and the inside of the border.
  gfx::Insets margins_;

  // Insets applied to the |anchor_view_| bounds.
  gfx::Insets anchor_view_insets_;

  // If true, the bubble does not take focus on display; default is false.
  bool use_focusless_;

  // Specifies whether the bubble (or its border) handles mouse events, etc.
  bool accept_events_;
  bool border_accepts_events_;

  // If true (defaults to true), the arrow may be mirrored and moved to fit the
  // bubble on screen better. It would be a no-op if the bubble has no arrow.
  bool adjust_if_offscreen_;

  // Parent native window of the bubble.
  gfx::NativeView parent_window_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
