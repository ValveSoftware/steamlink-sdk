// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_TRAY_BUBBLE_VIEW_H_
#define UI_VIEWS_BUBBLE_TRAY_BUBBLE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/views_export.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
class Widget;
}

namespace views {

namespace internal {
class TrayBubbleBorder;
class TrayBubbleContentMask;
}

// Specialized bubble view for bubbles associated with a tray icon (e.g. the
// Ash status area). Mostly this handles custom anchor location and arrow and
// border rendering. This also has its own delegate for handling mouse events
// and other implementation specific details.
class VIEWS_EXPORT TrayBubbleView : public views::BubbleDialogDelegateView,
                                    public views::MouseWatcherListener {
 public:
  // AnchorType differentiates between bubbles that are anchored on a tray
  // element (ANCHOR_TYPE_TRAY) and display an arrow, or that are floating on
  // the screen away from the tray (ANCHOR_TYPE_BUBBLE).
  enum AnchorType {
    ANCHOR_TYPE_TRAY,
    ANCHOR_TYPE_BUBBLE,
  };

  // AnchorAlignment determines to which side of the anchor the bubble will
  // align itself.
  enum AnchorAlignment {
    ANCHOR_ALIGNMENT_BOTTOM,
    ANCHOR_ALIGNMENT_LEFT,
    ANCHOR_ALIGNMENT_RIGHT,
    ANCHOR_ALIGNMENT_TOP
  };

  class VIEWS_EXPORT Delegate {
   public:
    typedef TrayBubbleView::AnchorType AnchorType;
    typedef TrayBubbleView::AnchorAlignment AnchorAlignment;

    Delegate() {}
    virtual ~Delegate() {}

    // Called when the view is destroyed. Any pointers to the view should be
    // cleared when this gets called.
    virtual void BubbleViewDestroyed() = 0;

    // Called when the mouse enters/exits the view.
    // Note: This event will only be called if the mouse gets actively moved by
    // the user to enter the view.
    virtual void OnMouseEnteredView() = 0;
    virtual void OnMouseExitedView() = 0;

    // Called from GetAccessibleState(); should return the appropriate
    // accessible name for the bubble.
    virtual base::string16 GetAccessibleNameForBubble() = 0;

    // Passes responsibility for BubbleDialogDelegateView::GetAnchorRect to the
    // delegate.
    virtual gfx::Rect GetAnchorRect(
        views::Widget* anchor_widget,
        AnchorType anchor_type,
        AnchorAlignment anchor_alignment) const = 0;

    // Called before Widget::Init() on |bubble_widget|. Allows |params| to be
    // modified.
    virtual void OnBeforeBubbleWidgetInit(Widget* anchor_widget,
                                          Widget* bubble_widget,
                                          Widget::InitParams* params) const = 0;

    // Called when a bubble wants to hide/destroy itself (e.g. last visible
    // child view was closed).
    virtual void HideBubble(const TrayBubbleView* bubble_view) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  struct VIEWS_EXPORT InitParams {
    static const int kArrowDefaultOffset;

    InitParams(AnchorType anchor_type,
               AnchorAlignment anchor_alignment,
               int min_width,
               int max_width);
    InitParams(const InitParams& other);
    AnchorType anchor_type;
    AnchorAlignment anchor_alignment;
    int min_width;
    int max_width;
    int max_height;
    bool can_activate;
    bool close_on_deactivate;
    SkColor arrow_color;
    bool first_item_has_no_margin;
    views::BubbleBorder::Arrow arrow;
    int arrow_offset;
    views::BubbleBorder::ArrowPaintType arrow_paint_type;
    views::BubbleBorder::Shadow shadow;
    views::BubbleBorder::BubbleAlignment arrow_alignment;
  };

  // Constructs and returns a TrayBubbleView. |init_params| may be modified.
  static TrayBubbleView* Create(views::View* anchor,
                                Delegate* delegate,
                                InitParams* init_params);

  ~TrayBubbleView() override;

  // Sets up animations, and show the bubble. Must occur after CreateBubble()
  // is called.
  void InitializeAndShowBubble();

  // Called whenever the bubble size or location may have changed.
  void UpdateBubble();

  // Sets the maximum bubble height and resizes the bubble.
  void SetMaxHeight(int height);

  // Sets the bubble width.
  void SetWidth(int width);

  // Sets whether or not to paint the bubble border arrow.
  void SetArrowPaintType(views::BubbleBorder::ArrowPaintType arrow_paint_type);

  // Returns the border insets. Called by TrayEventFilter.
  gfx::Insets GetBorderInsets() const;

  // Called when the delegate is destroyed.
  void reset_delegate() { delegate_ = NULL; }

  Delegate* delegate() { return delegate_; }

  void set_gesture_dragging(bool dragging) { is_gesture_dragging_ = dragging; }
  bool is_gesture_dragging() const { return is_gesture_dragging_; }

  // Overridden from views::WidgetDelegate.
  bool CanActivate() const override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // Overridden from views::BubbleDialogDelegateView.
  gfx::Rect GetAnchorRect() const override;
  void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                Widget* bubble_widget) const override;

  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;
  gfx::Size GetMaximumSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // Overridden from MouseWatcherListener
  void MouseMovedOutOfHost() override;

 protected:
  TrayBubbleView(views::View* anchor,
                 Delegate* delegate,
                 const InitParams& init_params);

  // Overridden from views::BubbleDialogDelegateView.
  int GetDialogButtons() const override;
  void Init() override;

  // Overridden from views::View.
  void ChildPreferredSizeChanged(View* child) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  InitParams params_;
  Delegate* delegate_;
  int preferred_width_;
  // |bubble_border_| and |owned_bubble_border_| point to the same thing, but
  // the latter ensures we don't leak it before passing off ownership.
  internal::TrayBubbleBorder* bubble_border_;
  std::unique_ptr<views::BubbleBorder> owned_bubble_border_;
  std::unique_ptr<internal::TrayBubbleContentMask> bubble_content_mask_;
  bool is_gesture_dragging_;

  // True once the mouse cursor was actively moved by the user over the bubble.
  // Only then the OnMouseExitedView() event will get passed on to listeners.
  bool mouse_actively_entered_;

  // Used to find any mouse movements.
  std::unique_ptr<MouseWatcher> mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_TRAY_BUBBLE_VIEW_H_
