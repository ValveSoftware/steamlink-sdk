// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"

#include "base/macros.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/context_menu_params.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_handle_drawable_aura.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
namespace {

// Delay before showing the quick menu, in milliseconds.
const int kQuickMenuDelayInMs = 100;

gfx::Rect ConvertRectToScreen(aura::Window* window, const gfx::RectF& rect) {
  gfx::Point origin = gfx::ToRoundedPoint(rect.origin());
  gfx::Point bottom_right = gfx::ToRoundedPoint(rect.bottom_right());

  aura::Window* root_window = window->GetRootWindow();
  if (root_window) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client) {
      screen_position_client->ConvertPointToScreen(window, &origin);
      screen_position_client->ConvertPointToScreen(window, &bottom_right);
    }
  }
  return gfx::Rect(origin.x(), origin.y(), bottom_right.x() - origin.x(),
                   bottom_right.y() - origin.y());
}

}  // namespace

// A pre-target event handler for aura::Env which deactivates touch selection on
// mouse and keyboard events.
class TouchSelectionControllerClientAura::EnvPreTargetHandler
    : public ui::EventHandler {
 public:
  EnvPreTargetHandler(ui::TouchSelectionController* selection_controller,
                      aura::Window* window);
  ~EnvPreTargetHandler() override;

 private:
  // EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  ui::TouchSelectionController* selection_controller_;
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(EnvPreTargetHandler);
};

TouchSelectionControllerClientAura::EnvPreTargetHandler::EnvPreTargetHandler(
    ui::TouchSelectionController* selection_controller,
    aura::Window* window)
    : selection_controller_(selection_controller), window_(window) {
  aura::Env::GetInstance()->AddPreTargetHandler(this);
}

TouchSelectionControllerClientAura::EnvPreTargetHandler::
    ~EnvPreTargetHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

void TouchSelectionControllerClientAura::EnvPreTargetHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  DCHECK_NE(ui::TouchSelectionController::INACTIVE,
            selection_controller_->active_status());

  selection_controller_->HideAndDisallowShowingAutomatically();
}

void TouchSelectionControllerClientAura::EnvPreTargetHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  DCHECK_NE(ui::TouchSelectionController::INACTIVE,
            selection_controller_->active_status());

  // If mouse events are not enabled, this mouse event is synthesized from a
  // touch event in which case we don't want to deactivate touch selection.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (!cursor_client || cursor_client->IsMouseEventsEnabled())
    selection_controller_->HideAndDisallowShowingAutomatically();
}

void TouchSelectionControllerClientAura::EnvPreTargetHandler::OnScrollEvent(
    ui::ScrollEvent* event) {
  DCHECK_NE(ui::TouchSelectionController::INACTIVE,
            selection_controller_->active_status());

  selection_controller_->HideAndDisallowShowingAutomatically();
}

TouchSelectionControllerClientAura::TouchSelectionControllerClientAura(
    RenderWidgetHostViewAura* rwhva)
    : rwhva_(rwhva),
      quick_menu_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kQuickMenuDelayInMs),
          base::Bind(&TouchSelectionControllerClientAura::ShowQuickMenu,
                     base::Unretained(this)),
          false),
      quick_menu_requested_(false),
      touch_down_(false),
      scroll_in_progress_(false),
      handle_drag_in_progress_(false) {
  DCHECK(rwhva_);
}

TouchSelectionControllerClientAura::~TouchSelectionControllerClientAura() {
}

void TouchSelectionControllerClientAura::OnWindowMoved() {
  UpdateQuickMenu();
}

void TouchSelectionControllerClientAura::OnTouchDown() {
  touch_down_ = true;
  UpdateQuickMenu();
}

void TouchSelectionControllerClientAura::OnTouchUp() {
  touch_down_ = false;
  UpdateQuickMenu();
}

void TouchSelectionControllerClientAura::OnScrollStarted() {
  scroll_in_progress_ = true;
  rwhva_->selection_controller()->SetTemporarilyHidden(true);
  UpdateQuickMenu();
}

void TouchSelectionControllerClientAura::OnScrollCompleted() {
  scroll_in_progress_ = false;
  rwhva_->selection_controller()->SetTemporarilyHidden(false);
  UpdateQuickMenu();
}

bool TouchSelectionControllerClientAura::HandleContextMenu(
    const ContextMenuParams& params) {
  if (params.source_type == ui::MENU_SOURCE_TOUCH && params.is_editable &&
      params.selection_text.empty() && IsQuickMenuAvailable()) {
    quick_menu_requested_ = true;
    UpdateQuickMenu();
    return true;
  }
  rwhva_->selection_controller()->HideAndDisallowShowingAutomatically();
  return false;
}

bool TouchSelectionControllerClientAura::IsQuickMenuAvailable() const {
  return ui::TouchSelectionMenuRunner::GetInstance() &&
         ui::TouchSelectionMenuRunner::GetInstance()->IsMenuAvailable(this);
}

void TouchSelectionControllerClientAura::ShowQuickMenu() {
  if (!ui::TouchSelectionMenuRunner::GetInstance())
    return;

  gfx::RectF rect = rwhva_->selection_controller()->GetRectBetweenBounds();

  // Clip rect, which is in |rwhva_|'s window's coordinate space, to client
  // bounds.
  gfx::PointF origin = rect.origin();
  gfx::PointF bottom_right = rect.bottom_right();
  auto client_bounds = gfx::RectF(rwhva_->GetNativeView()->bounds());
  origin.SetToMax(client_bounds.origin());
  bottom_right.SetToMin(client_bounds.bottom_right());
  if (origin.x() > bottom_right.x() || origin.y() > bottom_right.y())
    return;

  gfx::Vector2dF diagonal = bottom_right - origin;
  gfx::SizeF size(diagonal.x(), diagonal.y());
  gfx::RectF anchor_rect(origin, size);

  // Calculate maximum handle image size;
  gfx::SizeF max_handle_size =
      rwhva_->selection_controller()->GetStartHandleRect().size();
  max_handle_size.SetToMax(
      rwhva_->selection_controller()->GetEndHandleRect().size());

  aura::Window* parent = rwhva_->GetNativeView();
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, ConvertRectToScreen(parent, anchor_rect),
      gfx::ToRoundedSize(max_handle_size), parent->GetToplevelWindow());
}

void TouchSelectionControllerClientAura::UpdateQuickMenu() {
  bool menu_is_showing =
      ui::TouchSelectionMenuRunner::GetInstance() &&
      ui::TouchSelectionMenuRunner::GetInstance()->IsRunning();

  // Hide the quick menu if there is any. This should happen even if the menu
  // should be shown again, in order to update its location or content.
  if (menu_is_showing)
    ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  else
    quick_menu_timer_.Stop();

  bool should_show_menu = quick_menu_requested_ && !touch_down_ &&
                          !scroll_in_progress_ && !handle_drag_in_progress_ &&
                          IsQuickMenuAvailable();

  // Start timer to show quick menu if necessary.
  if (should_show_menu) {
    if (show_quick_menu_immediately_for_test_)
      ShowQuickMenu();
    else
      quick_menu_timer_.Reset();
  }
}

bool TouchSelectionControllerClientAura::SupportsAnimation() const {
  return false;
}

void TouchSelectionControllerClientAura::SetNeedsAnimate() {
  NOTREACHED();
}

void TouchSelectionControllerClientAura::MoveCaret(
    const gfx::PointF& position) {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost());
  host->MoveCaret(gfx::ToRoundedPoint(position));
}

void TouchSelectionControllerClientAura::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost())->delegate();
  if (host_delegate)
    host_delegate->MoveRangeSelectionExtent(gfx::ToRoundedPoint(extent));
}

void TouchSelectionControllerClientAura::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost())->delegate();
  if (host_delegate) {
    host_delegate->SelectRange(gfx::ToRoundedPoint(base),
                               gfx::ToRoundedPoint(extent));
  }
}

void TouchSelectionControllerClientAura::OnSelectionEvent(
    ui::SelectionEventType event) {
  switch (event) {
    case ui::SELECTION_HANDLES_SHOWN:
      quick_menu_requested_ = true;
      // Fall through.
    case ui::INSERTION_HANDLE_SHOWN:
      UpdateQuickMenu();
      env_pre_target_handler_.reset(new EnvPreTargetHandler(
          rwhva_->selection_controller(), rwhva_->GetNativeView()));
      break;
    case ui::SELECTION_HANDLES_CLEARED:
    case ui::INSERTION_HANDLE_CLEARED:
      env_pre_target_handler_.reset();
      quick_menu_requested_ = false;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLE_DRAG_STARTED:
    case ui::INSERTION_HANDLE_DRAG_STARTED:
      handle_drag_in_progress_ = true;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLE_DRAG_STOPPED:
    case ui::INSERTION_HANDLE_DRAG_STOPPED:
      handle_drag_in_progress_ = false;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLES_MOVED:
    case ui::INSERTION_HANDLE_MOVED:
      UpdateQuickMenu();
      break;
    case ui::INSERTION_HANDLE_TAPPED:
      quick_menu_requested_ = !quick_menu_requested_;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_ESTABLISHED:
    case ui::SELECTION_DISSOLVED:
      break;
  };
}

std::unique_ptr<ui::TouchHandleDrawable>
TouchSelectionControllerClientAura::CreateDrawable() {
  return std::unique_ptr<ui::TouchHandleDrawable>(
      new ui::TouchHandleDrawableAura(rwhva_->GetNativeView()));
}

bool TouchSelectionControllerClientAura::IsCommandIdEnabled(
    int command_id) const {
  bool editable = rwhva_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  bool readable = rwhva_->GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD;
  gfx::Range selection_range;
  rwhva_->GetSelectionRange(&selection_range);
  bool has_selection = !selection_range.is_empty();
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && readable && has_selection;
    case IDS_APP_COPY:
      return readable && has_selection;
    case IDS_APP_PASTE: {
      base::string16 result;
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
      return editable && !result.empty();
    }
    default:
      return false;
  }
}

void TouchSelectionControllerClientAura::ExecuteCommand(int command_id,
                                                        int event_flags) {
  rwhva_->selection_controller()->HideAndDisallowShowingAutomatically();
  RenderWidgetHostDelegate* host_delegate =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost())->delegate();
  if (!host_delegate)
    return;

  switch (command_id) {
    case IDS_APP_CUT:
      host_delegate->Cut();
      break;
    case IDS_APP_COPY:
      host_delegate->Copy();
      break;
    case IDS_APP_PASTE:
      host_delegate->Paste();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void TouchSelectionControllerClientAura::RunContextMenu() {
  gfx::RectF anchor_rect =
      rwhva_->selection_controller()->GetRectBetweenBounds();
  gfx::PointF anchor_point =
      gfx::PointF(anchor_rect.CenterPoint().x(), anchor_rect.y());
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost());
  host->Send(new ViewMsg_ShowContextMenu(host->GetRoutingID(),
                                         ui::MENU_SOURCE_TOUCH_EDIT_MENU,
                                         gfx::ToRoundedPoint(anchor_point)));

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  rwhva_->selection_controller()->HideAndDisallowShowingAutomatically();
}

}  // namespace content
