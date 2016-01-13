// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_

#include "ui/gfx/size.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {

class BaseScrollBar;

///////////////////////////////////////////////////////////////////////////////
//
// BaseScrollBarThumb
//
//  A view that acts as the thumb in the scroll bar track that the user can
//  drag to scroll the associated contents view within the viewport.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BaseScrollBarThumb : public View {
 public:
  explicit BaseScrollBarThumb(BaseScrollBar* scroll_bar);
  virtual ~BaseScrollBarThumb();

  // Sets the size (width or height) of the thumb to the specified value.
  void SetSize(int size);

  // Retrieves the size (width or height) of the thumb.
  int GetSize() const;

  // Sets the position of the thumb on the x or y axis.
  void SetPosition(int position);

  // Gets the position of the thumb on the x or y axis.
  int GetPosition() const;

  // View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE = 0;

 protected:
  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE = 0;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

  CustomButton::ButtonState GetState() const;
  // Update our state and schedule a repaint when the mouse moves over us.
  void SetState(CustomButton::ButtonState state);

  BaseScrollBar* scroll_bar() { return scroll_bar_; }

 private:
  // The BaseScrollBar that owns us.
  BaseScrollBar* scroll_bar_;

  int drag_start_position_;

  // The position of the mouse on the scroll axis relative to the top of this
  // View when the drag started.
  int mouse_offset_;

  // The current state of the thumb button.
  CustomButton::ButtonState state_;

  DISALLOW_COPY_AND_ASSIGN(BaseScrollBarThumb);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_
