// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_H_

#include "base/gtest_prod_util.h"
#include "ui/views/animation/scroll_animator.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/repeat_controller.h"

namespace views {

class BaseScrollBarThumb;
class MenuRunner;

///////////////////////////////////////////////////////////////////////////////
//
// BaseScrollBar
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BaseScrollBar : public ScrollBar,
                                   public ScrollDelegate,
                                   public ContextMenuController,
                                   public MenuDelegate {
 public:
  BaseScrollBar(bool horizontal, BaseScrollBarThumb* thumb);
  virtual ~BaseScrollBar();

  // Get the bounds of the "track" area that the thumb is free to slide within.
  virtual gfx::Rect GetTrackBounds() const = 0;

  // An enumeration of different amounts of incremental scroll, representing
  // events sent from different parts of the UI/keyboard.
  enum ScrollAmount {
    SCROLL_NONE = 0,
    SCROLL_START,
    SCROLL_END,
    SCROLL_PREV_LINE,
    SCROLL_NEXT_LINE,
    SCROLL_PREV_PAGE,
    SCROLL_NEXT_PAGE,
  };

  // Scroll the contents by the specified type (see ScrollAmount above).
  void ScrollByAmount(ScrollAmount amount);

  // Scroll the contents to the appropriate position given the supplied
  // position of the thumb (thumb track coordinates). If |scroll_to_middle| is
  // true, then the conversion assumes |thumb_position| is in the middle of the
  // thumb rather than the top.
  void ScrollToThumbPosition(int thumb_position, bool scroll_to_middle);

  // Scroll the contents by the specified offset (contents coordinates).
  bool ScrollByContentsOffset(int contents_offset);

  // Called when the thumb state has been changed from |old_state| to
  // |new_state|.
  void OnThumbStateChanged(CustomButton::ButtonState old_state,
                           CustomButton::ButtonState new_state);

  // View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE = 0;
  virtual void Layout() OVERRIDE = 0;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // ScrollBar overrides:
  virtual void Update(int viewport_size,
                      int content_size,
                      int contents_scroll_offset) OVERRIDE;
  virtual int GetLayoutSize() const OVERRIDE = 0;
  virtual int GetPosition() const OVERRIDE;

  // ScrollDelegate overrides:
  virtual bool OnScroll(float dx, float dy) OVERRIDE;

  // ContextMenuController overrides:
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // Menu::Delegate overrides:
  virtual base::string16 GetLabel(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

 protected:
  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE = 0;

  BaseScrollBarThumb* GetThumb() const;
  CustomButton::ButtonState GetThumbTrackState() const;

  // Wrapper functions that calls the controller. We need this since native
  // scrollbars wrap around a different scrollbar. When calling the controller
  // we need to pass in the appropriate scrollbar. For normal scrollbars it's
  // the |this| scrollbar, for native scrollbars it's the native scrollbar used
  // to create this.
  virtual void ScrollToPosition(int position);
  virtual int GetScrollIncrement(bool is_page, bool is_positive);

 private:
  FRIEND_TEST_ALL_PREFIXES(NativeScrollBarTest, ScrollBarFitsToBottom);
  int GetThumbSizeForTest();

  // Changes to 'pushed' state and starts a timer to scroll repeatedly.
  void ProcessPressEvent(const ui::LocatedEvent& event);

  // Sets state to |state| and stops the repeater.
  void SetState(CustomButton::ButtonState state);

  // Called when the mouse is pressed down in the track area.
  void TrackClicked();

  // Responsible for scrolling the contents and also updating the UI to the
  // current value of the Scroll Offset.
  void ScrollContentsToOffset();

  // Returns the size (width or height) of the track area of the ScrollBar.
  int GetTrackSize() const;

  // Calculate the position of the thumb within the track based on the
  // specified scroll offset of the contents.
  int CalculateThumbPosition(int contents_scroll_offset) const;

  // Calculates the current value of the contents offset (contents coordinates)
  // based on the current thumb position (thumb track coordinates). See
  // |ScrollToThumbPosition| for an explanation of |scroll_to_middle|.
  int CalculateContentsOffset(int thumb_position,
                              bool scroll_to_middle) const;

  // Called when the state of the thumb track changes (e.g. by the user
  // pressing the mouse button down in it).
  void SetThumbTrackState(CustomButton::ButtonState state);

  BaseScrollBarThumb* thumb_;

  // The size of the scrolled contents, in pixels.
  int contents_size_;

  // The current amount the contents is offset by in the viewport.
  int contents_scroll_offset_;

  // The current size of the view port, in pixels.
  int viewport_size_;

  // The state of the scrollbar track. Typically, the track will highlight when
  // the user presses the mouse on them (during page scrolling).
  CustomButton::ButtonState thumb_track_state_;

  // The last amount of incremental scroll that this scrollbar performed. This
  // is accessed by the callbacks for the auto-repeat up/down buttons to know
  // what direction to repeatedly scroll in.
  ScrollAmount last_scroll_amount_;

  // An instance of a RepeatController which scrolls the scrollbar continuously
  // as the user presses the mouse button down on the up/down buttons or the
  // track.
  RepeatController repeater_;

  // The position of the mouse within the scroll bar when the context menu
  // was invoked.
  int context_menu_mouse_position_;

  scoped_ptr<MenuRunner> menu_runner_;
  scoped_ptr<ScrollAnimator> scroll_animator_;

  // Difference between current position and cumulative deltas obtained from
  // scroll update events.
  // TODO(tdresser): This should be removed when raw pixel scrolling for views
  // is enabled. See crbug.com/329354.
  gfx::Vector2dF roundoff_error_;

  DISALLOW_COPY_AND_ASSIGN(BaseScrollBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_H_
