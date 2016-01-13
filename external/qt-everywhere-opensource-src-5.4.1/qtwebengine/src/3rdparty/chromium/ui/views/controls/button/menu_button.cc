// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::TimeTicks;
using base::TimeDelta;

namespace views {

// Default menu offset.
static const int kDefaultMenuOffsetX = -2;
static const int kDefaultMenuOffsetY = -4;

// static
const char MenuButton::kViewClassName[] = "MenuButton";
const int MenuButton::kMenuMarkerPaddingLeft = 3;
const int MenuButton::kMenuMarkerPaddingRight = -1;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::MenuButton(ButtonListener* listener,
                       const base::string16& text,
                       MenuButtonListener* menu_button_listener,
                       bool show_menu_marker)
    : LabelButton(listener, text),
      menu_visible_(false),
      menu_offset_(kDefaultMenuOffsetX, kDefaultMenuOffsetY),
      listener_(menu_button_listener),
      show_menu_marker_(show_menu_marker),
      menu_marker_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_MENU_DROPARROW).ToImageSkia()),
      destroyed_flag_(NULL) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

MenuButton::~MenuButton() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Public APIs
//
////////////////////////////////////////////////////////////////////////////////

bool MenuButton::Activate() {
  SetState(STATE_PRESSED);
  if (listener_) {
    gfx::Rect lb = GetLocalBounds();

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    gfx::Point menu_position(lb.right(), lb.bottom());
    if (base::i18n::IsRTL())
      menu_position.set_x(lb.x());

    View::ConvertPointToScreen(this, &menu_position);
    if (base::i18n::IsRTL())
      menu_position.Offset(-menu_offset_.x(), menu_offset_.y());
    else
      menu_position.Offset(menu_offset_.x(), menu_offset_.y());

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x())
      menu_position.set_x(max_x_coordinate - 1);

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    static_cast<internal::RootView*>(GetWidget()->GetRootView())->
        SetMouseHandler(NULL);

    menu_visible_ = true;

    bool destroyed = false;
    destroyed_flag_ = &destroyed;

    listener_->OnMenuButtonClicked(this, menu_position);

    if (destroyed) {
      // The menu was deleted while showing. Don't attempt any processing.
      return false;
    }

    destroyed_flag_ = NULL;

    menu_visible_ = false;
    menu_closed_time_ = TimeTicks::Now();

    // Now that the menu has closed, we need to manually reset state to
    // "normal" since the menu modal loop will have prevented normal
    // mouse move messages from getting to this View. We set "normal"
    // and not "hot" because the likelihood is that the mouse is now
    // somewhere else (user clicked elsewhere on screen to close the menu
    // or selected an item) and we will inevitably refresh the hot state
    // in the event the mouse _is_ over the view.
    SetState(STATE_NORMAL);

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }
  return true;
}

void MenuButton::OnPaint(gfx::Canvas* canvas) {
  LabelButton::OnPaint(canvas);

  if (show_menu_marker_)
    PaintMenuMarker(canvas);
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Events
//
////////////////////////////////////////////////////////////////////////////////

gfx::Size MenuButton::GetPreferredSize() const {
  gfx::Size prefsize = LabelButton::GetPreferredSize();
  if (show_menu_marker_) {
    prefsize.Enlarge(menu_marker_->width() + kMenuMarkerPaddingLeft +
                         kMenuMarkerPaddingRight,
                     0);
  }
  return prefsize;
}

const char* MenuButton::GetClassName() const {
  return kViewClassName;
}

bool MenuButton::OnMousePressed(const ui::MouseEvent& event) {
  RequestFocus();
  if (state() != STATE_DISABLED) {
    // If we're draggable (GetDragOperations returns a non-zero value), then
    // don't pop on press, instead wait for release.
    if (event.IsOnlyLeftMouseButton() &&
        HitTestPoint(event.location()) &&
        GetDragOperations(event.location()) == ui::DragDropTypes::DRAG_NONE) {
      TimeDelta delta = TimeTicks::Now() - menu_closed_time_;
      if (delta.InMilliseconds() > kMinimumMsBetweenButtonClicks)
        return Activate();
    }
  }
  return true;
}

void MenuButton::OnMouseReleased(const ui::MouseEvent& event) {
  // Explicitly test for left mouse button to show the menu. If we tested for
  // !IsTriggerableEvent it could lead to a situation where we end up showing
  // the menu and context menu (this would happen if the right button is not
  // triggerable and there's a context menu).
  if (GetDragOperations(event.location()) != ui::DragDropTypes::DRAG_NONE &&
      state() != STATE_DISABLED && !InDrag() && event.IsOnlyLeftMouseButton() &&
      HitTestPoint(event.location())) {
    Activate();
  } else {
    LabelButton::OnMouseReleased(event);
  }
}

// The reason we override View::OnMouseExited is because we get this event when
// we display the menu. If we don't override this method then
// BaseButton::OnMouseExited will get the event and will set the button's state
// to STATE_NORMAL instead of keeping the state BM_PUSHED. This, in turn, will
// cause the button to appear depressed while the menu is displayed.
void MenuButton::OnMouseExited(const ui::MouseEvent& event) {
  if ((state_ != STATE_DISABLED) && (!menu_visible_) && (!InDrag())) {
    SetState(STATE_NORMAL);
  }
}

void MenuButton::OnGestureEvent(ui::GestureEvent* event) {
  if (state() != STATE_DISABLED && event->type() == ui::ET_GESTURE_TAP &&
      !Activate()) {
    // When |Activate()| returns |false|, it means that a menu is shown and
    // has handled the gesture event. So, there is no need to further process
    // the gesture event here.
    return;
  }
  LabelButton::OnGestureEvent(event);
}

bool MenuButton::OnKeyPressed(const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_SPACE:
      // Alt-space on windows should show the window menu.
      if (event.IsAltDown())
        break;
    case ui::VKEY_RETURN:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      // WARNING: we may have been deleted by the time Activate returns.
      Activate();
      // This is to prevent the keyboard event from being dispatched twice.  If
      // the keyboard event is not handled, we pass it to the default handler
      // which dispatches the event back to us causing the menu to get displayed
      // again. Return true to prevent this.
      return true;
    }
    default:
      break;
  }
  return false;
}

bool MenuButton::OnKeyReleased(const ui::KeyEvent& event) {
  // Override CustomButton's implementation, which presses the button when
  // you press space and clicks it when you release space.  For a MenuButton
  // we always activate the menu on key press.
  return false;
}

void MenuButton::GetAccessibleState(ui::AXViewState* state) {
  CustomButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_POP_UP_BUTTON;
  state->default_action = l10n_util::GetStringUTF16(IDS_APP_ACCACTION_PRESS);
  state->AddStateFlag(ui::AX_STATE_HASPOPUP);
}

void MenuButton::PaintMenuMarker(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  // We can not use the views' mirroring infrastructure for mirroring a
  // MenuButton control (see TextButton::OnPaint() for a detailed explanation
  // regarding why we can not flip the canvas). Therefore, we need to
  // manually mirror the position of the down arrow.
  gfx::Rect arrow_bounds(width() - insets.right() -
                         menu_marker_->width() - kMenuMarkerPaddingRight,
                         height() / 2 - menu_marker_->height() / 2,
                         menu_marker_->width(),
                         menu_marker_->height());
  arrow_bounds.set_x(GetMirroredXForRect(arrow_bounds));
  canvas->DrawImageInt(*menu_marker_, arrow_bounds.x(), arrow_bounds.y());
}

gfx::Rect MenuButton::GetChildAreaBounds() {
  gfx::Size s = size();

  if (show_menu_marker_) {
    s.set_width(s.width() - menu_marker_->width() - kMenuMarkerPaddingLeft -
                kMenuMarkerPaddingRight);
  }

  return gfx::Rect(s);
}

int MenuButton::GetMaximumScreenXCoordinate() {
  if (!GetWidget()) {
    NOTREACHED();
    return 0;
  }

  gfx::Rect monitor_bounds = GetWidget()->GetWorkAreaBoundsInScreen();
  return monitor_bounds.right() - 1;
}

}  // namespace views
