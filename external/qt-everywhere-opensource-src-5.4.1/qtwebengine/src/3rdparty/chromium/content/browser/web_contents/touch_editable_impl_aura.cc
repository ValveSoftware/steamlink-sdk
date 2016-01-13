// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/touch_editable_impl_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "grit/ui_strings.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/range/range.h"
#include "ui/wm/public/activation_client.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, public:

TouchEditableImplAura::~TouchEditableImplAura() {
  Cleanup();
}

// static
TouchEditableImplAura* TouchEditableImplAura::Create() {
  if (switches::IsTouchEditingEnabled())
    return new TouchEditableImplAura();
  return NULL;
}

void TouchEditableImplAura::AttachToView(RenderWidgetHostViewAura* view) {
  if (rwhva_ == view)
    return;

  Cleanup();
  if (!view)
    return;

  rwhva_ = view;
  rwhva_->set_touch_editing_client(this);
}

void TouchEditableImplAura::UpdateEditingController() {
  if (!rwhva_ || !rwhva_->HasFocus())
    return;

  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE ||
      selection_anchor_rect_ != selection_focus_rect_) {
    if (touch_selection_controller_)
      touch_selection_controller_->SelectionChanged();
  } else {
    EndTouchEditing(false);
  }
}

void TouchEditableImplAura::OverscrollStarted() {
  overscroll_in_progress_ = true;
}

void TouchEditableImplAura::OverscrollCompleted() {
  // We might receive multiple OverscrollStarted() and OverscrollCompleted()
  // during the same scroll session (for example, when the scroll direction
  // changes). We want to show the handles only when:
  // 1. Overscroll has completed
  // 2. Scrolling session is over, i.e. we have received ET_GESTURE_SCROLL_END.
  // 3. If we had hidden the handles when scrolling started
  // 4. If there is still a need to show handles (there is a non-empty selection
  // or non-NONE |text_input_type_|)
  if (overscroll_in_progress_ && !scroll_in_progress_ &&
      handles_hidden_due_to_scroll_ &&
      (selection_anchor_rect_ != selection_focus_rect_ ||
          text_input_type_ != ui::TEXT_INPUT_TYPE_NONE)) {
    StartTouchEditing();
    UpdateEditingController();
  }
  overscroll_in_progress_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, RenderWidgetHostViewAura::TouchEditingClient
// implementation:

void TouchEditableImplAura::StartTouchEditing() {
  if (!rwhva_ || !rwhva_->HasFocus())
    return;

  if (!touch_selection_controller_) {
    touch_selection_controller_.reset(
        ui::TouchSelectionController::create(this));
  }
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void TouchEditableImplAura::EndTouchEditing(bool quick) {
  if (touch_selection_controller_) {
    if (touch_selection_controller_->IsHandleDragInProgress()) {
      touch_selection_controller_->SelectionChanged();
    } else {
      selection_gesture_in_process_ = false;
      touch_selection_controller_->HideHandles(quick);
      touch_selection_controller_.reset();
    }
  }
}

void TouchEditableImplAura::OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                                       const gfx::Rect& focus) {
  selection_anchor_rect_ = anchor;
  selection_focus_rect_ = focus;

  // If touch editing handles were not visible, we bring them up only if
  // there is non-zero selection on the page. And the current event is a
  // gesture event (we dont want to show handles if the user is selecting
  // using mouse or keyboard).
  if (selection_gesture_in_process_ && !scroll_in_progress_ &&
      !overscroll_in_progress_ &&
      selection_anchor_rect_ != selection_focus_rect_) {
    StartTouchEditing();
    selection_gesture_in_process_ = false;
  }

  UpdateEditingController();
}

void TouchEditableImplAura::OnTextInputTypeChanged(ui::TextInputType type) {
  text_input_type_ = type;
}

bool TouchEditableImplAura::HandleInputEvent(const ui::Event* event) {
  DCHECK(rwhva_);
  if (!event->IsGestureEvent()) {
    // Ignore all non-gesture events. Non-gesture events that can deactivate
    // touch editing are handled in TouchSelectionControllerImpl.
    return false;
  }

  const ui::GestureEvent* gesture_event =
      static_cast<const ui::GestureEvent*>(event);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      // When the user taps, we want to show touch editing handles if user
      // tapped on selected text.
      if (gesture_event->details().tap_count() == 1 &&
          selection_anchor_rect_ != selection_focus_rect_) {
        // UnionRects only works for rects with non-zero width.
        gfx::Rect anchor(selection_anchor_rect_.origin(),
                         gfx::Size(1, selection_anchor_rect_.height()));
        gfx::Rect focus(selection_focus_rect_.origin(),
                        gfx::Size(1, selection_focus_rect_.height()));
        gfx::Rect selection_rect = gfx::UnionRects(anchor, focus);
        if (selection_rect.Contains(gesture_event->location())) {
          StartTouchEditing();
          return true;
        }
      }
      // For single taps, not inside selected region, we want to show handles
      // only when the tap is on an already focused textfield.
      textfield_was_focused_on_tap_ = false;
      if (gesture_event->details().tap_count() == 1 &&
          text_input_type_ != ui::TEXT_INPUT_TYPE_NONE)
        textfield_was_focused_on_tap_ = true;
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      selection_gesture_in_process_ = true;
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      // If selection handles are currently visible, we want to get them back up
      // when scrolling ends. So we set |handles_hidden_due_to_scroll_| so that
      // we can re-start touch editing on scroll end gesture.
      scroll_in_progress_ = true;
      handles_hidden_due_to_scroll_ = false;
      if (touch_selection_controller_)
        handles_hidden_due_to_scroll_ = true;
      EndTouchEditing(true);
      break;
    case ui::ET_GESTURE_SCROLL_END:
      // Scroll has ended, but we might still be in overscroll animation.
      if (handles_hidden_due_to_scroll_ && !overscroll_in_progress_ &&
          (selection_anchor_rect_ != selection_focus_rect_ ||
              text_input_type_ != ui::TEXT_INPUT_TYPE_NONE)) {
        StartTouchEditing();
        UpdateEditingController();
      }
      // fall through to reset |scroll_in_progress_|.
    case ui::ET_SCROLL_FLING_START:
      selection_gesture_in_process_ = false;
      scroll_in_progress_ = false;
      break;
    default:
      break;
  }
  return false;
}

void TouchEditableImplAura::GestureEventAck(int gesture_event_type) {
  DCHECK(rwhva_);
  if (gesture_event_type == blink::WebInputEvent::GestureTap &&
      text_input_type_ != ui::TEXT_INPUT_TYPE_NONE &&
      textfield_was_focused_on_tap_) {
    StartTouchEditing();
    UpdateEditingController();
  }
}

void TouchEditableImplAura::OnViewDestroyed() {
  Cleanup();
}

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, ui::TouchEditable implementation:

void TouchEditableImplAura::SelectRect(const gfx::Point& start,
                                       const gfx::Point& end) {
  RenderWidgetHost* host = rwhva_->GetRenderWidgetHost();
  RenderViewHost* rvh = RenderViewHost::From(host);
  WebContentsImpl* wc =
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(rvh));
  wc->SelectRange(start, end);
}

void TouchEditableImplAura::MoveCaretTo(const gfx::Point& point) {
  if (!rwhva_)
    return;

  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  host->MoveCaret(point);
}

void TouchEditableImplAura::GetSelectionEndPoints(gfx::Rect* p1,
                                                  gfx::Rect* p2) {
  *p1 = selection_anchor_rect_;
  *p2 = selection_focus_rect_;
}

gfx::Rect TouchEditableImplAura::GetBounds() {
  return rwhva_ ? gfx::Rect(rwhva_->GetNativeView()->bounds().size()) :
      gfx::Rect();
}

gfx::NativeView TouchEditableImplAura::GetNativeView() const {
  return rwhva_ ? rwhva_->GetNativeView()->GetToplevelWindow() : NULL;
}

void TouchEditableImplAura::ConvertPointToScreen(gfx::Point* point) {
  if (!rwhva_)
    return;
  aura::Window* window = rwhva_->GetNativeView();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(window, point);
}

void TouchEditableImplAura::ConvertPointFromScreen(gfx::Point* point) {
  if (!rwhva_)
    return;
  aura::Window* window = rwhva_->GetNativeView();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  if (screen_position_client)
    screen_position_client->ConvertPointFromScreen(window, point);
}

bool TouchEditableImplAura::DrawsHandles() {
  return false;
}

void TouchEditableImplAura::OpenContextMenu(const gfx::Point& anchor) {
  if (!rwhva_)
    return;
  gfx::Point point = anchor;
  ConvertPointFromScreen(&point);
  RenderWidgetHost* host = rwhva_->GetRenderWidgetHost();
  host->Send(new ViewMsg_ShowContextMenu(
      host->GetRoutingID(), ui::MENU_SOURCE_TOUCH_EDIT_MENU, point));
  EndTouchEditing(false);
}

bool TouchEditableImplAura::IsCommandIdChecked(int command_id) const {
  NOTREACHED();
  return false;
}

bool TouchEditableImplAura::IsCommandIdEnabled(int command_id) const {
  if (!rwhva_)
    return false;
  bool editable = rwhva_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  gfx::Range selection_range;
  rwhva_->GetSelectionRange(&selection_range);
  bool has_selection = !selection_range.is_empty();
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && has_selection;
    case IDS_APP_COPY:
      return has_selection;
    case IDS_APP_PASTE: {
      base::string16 result;
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
      return editable && !result.empty();
    }
    case IDS_APP_DELETE:
      return editable && has_selection;
    case IDS_APP_SELECT_ALL:
      return true;
    default:
      return false;
  }
}

bool TouchEditableImplAura::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void TouchEditableImplAura::ExecuteCommand(int command_id, int event_flags) {
  RenderWidgetHost* host = rwhva_->GetRenderWidgetHost();
  RenderViewHost* rvh = RenderViewHost::From(host);
  WebContents* wc = WebContents::FromRenderViewHost(rvh);

  switch (command_id) {
    case IDS_APP_CUT:
      wc->Cut();
      break;
    case IDS_APP_COPY:
      wc->Copy();
      break;
    case IDS_APP_PASTE:
      wc->Paste();
      break;
    case IDS_APP_DELETE:
      wc->Delete();
      break;
    case IDS_APP_SELECT_ALL:
      wc->SelectAll();
      break;
    default:
      NOTREACHED();
      break;
  }
  EndTouchEditing(false);
}

void TouchEditableImplAura::DestroyTouchSelection() {
  EndTouchEditing(false);
}

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, private:

TouchEditableImplAura::TouchEditableImplAura()
    : text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      rwhva_(NULL),
      selection_gesture_in_process_(false),
      handles_hidden_due_to_scroll_(false),
      scroll_in_progress_(false),
      overscroll_in_progress_(false),
      textfield_was_focused_on_tap_(false) {
}

void TouchEditableImplAura::Cleanup() {
  if (rwhva_) {
    rwhva_->set_touch_editing_client(NULL);
    rwhva_ = NULL;
  }
  text_input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  EndTouchEditing(true);
  selection_gesture_in_process_ = false;
  handles_hidden_due_to_scroll_ = false;
  scroll_in_progress_ = false;
  overscroll_in_progress_ = false;
}

}  // namespace content
