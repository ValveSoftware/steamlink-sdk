// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller_impl.h"

#include "base/time/time.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/masked_window_targeter.h"
#include "ui/wm/core/window_animations.h"

namespace {

// Constants defining the visual attributes of selection handles
const int kSelectionHandleLineWidth = 1;
const SkColor kSelectionHandleLineColor =
    SkColorSetRGB(0x42, 0x81, 0xf4);

// When a handle is dragged, the drag position reported to the client view is
// offset vertically to represent the cursor position. This constant specifies
// the offset in  pixels above the "O" (see pic below). This is required because
// say if this is zero, that means the drag position we report is the point
// right above the "O" or the bottom most point of the cursor "|". In that case,
// a vertical movement of even one pixel will make the handle jump to the line
// below it. So when the user just starts dragging, the handle will jump to the
// next line if the user makes any vertical movement. It is correct but
// looks/feels weird. So we have this non-zero offset to prevent this jumping.
//
// Editing handle widget showing the difference between the position of the
// ET_GESTURE_SCROLL_UPDATE event and the drag position reported to the client:
//                                  _____
//                                 |  |<-|---- Drag position reported to client
//                              _  |  O  |
//          Vertical Padding __|   |   <-|---- ET_GESTURE_SCROLL_UPDATE position
//                             |_  |_____|<--- Editing handle widget
//
//                                 | |
//                                  T
//                          Horizontal Padding
//
const int kSelectionHandleVerticalDragOffset = 5;

// Padding around the selection handle defining the area that will be included
// in the touch target to make dragging the handle easier (see pic above).
const int kSelectionHandleHorizPadding = 10;
const int kSelectionHandleVertPadding = 20;

const int kContextMenuTimoutMs = 200;

const int kSelectionHandleQuickFadeDurationMs = 50;

// Minimum height for selection handle bar. If the bar height is going to be
// less than this value, handle will not be shown.
const int kSelectionHandleBarMinHeight = 5;
// Maximum amount that selection handle bar can stick out of client view's
// boundaries.
const int kSelectionHandleBarBottomAllowance = 3;

// Creates a widget to host SelectionHandleView.
views::Widget* CreateTouchSelectionPopupWidget(
    gfx::NativeView context,
    views::WidgetDelegate* widget_delegate) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = context;
  params.delegate = widget_delegate;
  widget->Init(params);
  return widget;
}

gfx::Image* GetHandleImage() {
  static gfx::Image* handle_image = NULL;
  if (!handle_image) {
    handle_image = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TEXT_SELECTION_HANDLE);
  }
  return handle_image;
}

gfx::Size GetHandleImageSize() {
  return GetHandleImage()->Size();
}

// Cannot use gfx::UnionRect since it does not work for empty rects.
gfx::Rect Union(const gfx::Rect& r1, const gfx::Rect& r2) {
  int rx = std::min(r1.x(), r2.x());
  int ry = std::min(r1.y(), r2.y());
  int rr = std::max(r1.right(), r2.right());
  int rb = std::max(r1.bottom(), r2.bottom());

  return gfx::Rect(rx, ry, rr - rx, rb - ry);
}

// Convenience methods to convert a |rect| from screen to the |client|'s
// coordinate system and vice versa.
// Note that this is not quite correct because it does not take into account
// transforms such as rotation and scaling. This should be in TouchEditable.
// TODO(varunjain): Fix this.
gfx::Rect ConvertFromScreen(ui::TouchEditable* client, const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  client->ConvertPointFromScreen(&origin);
  return gfx::Rect(origin, rect.size());
}
gfx::Rect ConvertToScreen(ui::TouchEditable* client, const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  client->ConvertPointToScreen(&origin);
  return gfx::Rect(origin, rect.size());
}

}  // namespace

namespace views {

typedef TouchSelectionControllerImpl::EditingHandleView EditingHandleView;

class TouchHandleWindowTargeter : public wm::MaskedWindowTargeter {
 public:
  TouchHandleWindowTargeter(aura::Window* window,
                            EditingHandleView* handle_view);

  virtual ~TouchHandleWindowTargeter() {}

 private:
  // wm::MaskedWindowTargeter:
  virtual bool GetHitTestMask(aura::Window* window,
                              gfx::Path* mask) const OVERRIDE;

  EditingHandleView* handle_view_;

  DISALLOW_COPY_AND_ASSIGN(TouchHandleWindowTargeter);
};

// A View that displays the text selection handle.
class TouchSelectionControllerImpl::EditingHandleView
    : public views::WidgetDelegateView {
 public:
  EditingHandleView(TouchSelectionControllerImpl* controller,
                    gfx::NativeView context)
      : controller_(controller),
        drag_offset_(0),
        draw_invisible_(false) {
    widget_.reset(CreateTouchSelectionPopupWidget(context, this));
    widget_->SetContentsView(this);

    aura::Window* window = widget_->GetNativeWindow();
    window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
        new TouchHandleWindowTargeter(window, this)));

    // We are owned by the TouchSelectionController.
    set_owned_by_client();
  }

  virtual ~EditingHandleView() {
    SetWidgetVisible(false, false);
  }

  // Overridden from views::WidgetDelegateView:
  virtual bool WidgetHasHitTestMask() const OVERRIDE {
    return true;
  }

  virtual void GetWidgetHitTestMask(gfx::Path* mask) const OVERRIDE {
    gfx::Size image_size = GetHandleImageSize();
    mask->addRect(SkIntToScalar(0), SkIntToScalar(selection_rect_.height()),
        SkIntToScalar(image_size.width()) + 2 * kSelectionHandleHorizPadding,
        SkIntToScalar(selection_rect_.height() + image_size.height() +
            kSelectionHandleVertPadding));
  }

  virtual void DeleteDelegate() OVERRIDE {
    // We are owned and deleted by TouchSelectionController.
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if (draw_invisible_)
      return;
    gfx::Size image_size = GetHandleImageSize();
    int cursor_pos_x = image_size.width() / 2 - kSelectionHandleLineWidth +
        kSelectionHandleHorizPadding;

    // Draw the cursor line.
    canvas->FillRect(
        gfx::Rect(cursor_pos_x, 0,
                  2 * kSelectionHandleLineWidth + 1, selection_rect_.height()),
        kSelectionHandleLineColor);

    // Draw the handle image.
    canvas->DrawImageInt(*GetHandleImage()->ToImageSkia(),
        kSelectionHandleHorizPadding, selection_rect_.height());
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    event->SetHandled();
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        widget_->SetCapture(this);
        controller_->SetDraggingHandle(this);
        drag_offset_ = event->y() - selection_rect_.height() +
            kSelectionHandleVerticalDragOffset;
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE: {
        gfx::Point drag_pos(event->location().x(),
            event->location().y() - drag_offset_);
        controller_->SelectionHandleDragged(drag_pos);
        break;
      }
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_SCROLL_FLING_START:
        widget_->ReleaseCapture();
        controller_->SetDraggingHandle(NULL);
        break;
      default:
        break;
    }
  }

  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    gfx::Size image_size = GetHandleImageSize();
    return gfx::Size(image_size.width() + 2 * kSelectionHandleHorizPadding,
                     image_size.height() + selection_rect_.height() +
                         kSelectionHandleVertPadding);
  }

  bool IsWidgetVisible() const {
    return widget_->IsVisible();
  }

  void SetWidgetVisible(bool visible, bool quick) {
    if (widget_->IsVisible() == visible)
      return;
    wm::SetWindowVisibilityAnimationDuration(
        widget_->GetNativeView(),
        base::TimeDelta::FromMilliseconds(
            quick ? kSelectionHandleQuickFadeDurationMs : 0));
    if (visible)
      widget_->Show();
    else
      widget_->Hide();
  }

  void SetSelectionRectInScreen(const gfx::Rect& rect) {
    gfx::Size image_size = GetHandleImageSize();
    selection_rect_ = rect;
    gfx::Rect widget_bounds(
        rect.x() - image_size.width() / 2 - kSelectionHandleHorizPadding,
        rect.y(),
        image_size.width() + 2 * kSelectionHandleHorizPadding,
        rect.height() + image_size.height() + kSelectionHandleVertPadding);
    widget_->SetBounds(widget_bounds);
  }

  gfx::Point GetScreenPosition() {
    return widget_->GetClientAreaBoundsInScreen().origin();
  }

  void SetDrawInvisible(bool draw_invisible) {
    if (draw_invisible_ == draw_invisible)
      return;
    draw_invisible_ = draw_invisible;
    SchedulePaint();
  }

  const gfx::Rect& selection_rect() const { return selection_rect_; }

 private:
  scoped_ptr<Widget> widget_;
  TouchSelectionControllerImpl* controller_;
  gfx::Rect selection_rect_;

  // Vertical offset between the scroll event position and the drag position
  // reported to the client view (see the ASCII figure at the top of the file
  // and its description for more details).
  int drag_offset_;

  // If set to true, the handle will not draw anything, hence providing an empty
  // widget. We need this because we may want to stop showing the handle while
  // it is being dragged. Since it is being dragged, we cannot destroy the
  // handle.
  bool draw_invisible_;

  DISALLOW_COPY_AND_ASSIGN(EditingHandleView);
};

TouchHandleWindowTargeter::TouchHandleWindowTargeter(
    aura::Window* window,
    EditingHandleView* handle_view)
    : wm::MaskedWindowTargeter(window),
      handle_view_(handle_view) {
}

bool TouchHandleWindowTargeter::GetHitTestMask(aura::Window* window,
                                               gfx::Path* mask) const {
  const gfx::Rect& selection_rect = handle_view_->selection_rect();
  gfx::Size image_size = GetHandleImageSize();
  mask->addRect(SkIntToScalar(0), SkIntToScalar(selection_rect.height()),
      SkIntToScalar(image_size.width()) + 2 * kSelectionHandleHorizPadding,
      SkIntToScalar(selection_rect.height() + image_size.height() +
                    kSelectionHandleVertPadding));
  return true;
}

TouchSelectionControllerImpl::TouchSelectionControllerImpl(
    ui::TouchEditable* client_view)
    : client_view_(client_view),
      client_widget_(NULL),
      selection_handle_1_(new EditingHandleView(this,
                          client_view->GetNativeView())),
      selection_handle_2_(new EditingHandleView(this,
                          client_view->GetNativeView())),
      cursor_handle_(new EditingHandleView(this,
                     client_view->GetNativeView())),
      context_menu_(NULL),
      dragging_handle_(NULL) {
  client_widget_ = Widget::GetTopLevelWidgetForNativeView(
      client_view_->GetNativeView());
  if (client_widget_)
    client_widget_->AddObserver(this);
  aura::Env::GetInstance()->AddPreTargetHandler(this);
}

TouchSelectionControllerImpl::~TouchSelectionControllerImpl() {
  HideContextMenu();
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
  if (client_widget_)
    client_widget_->RemoveObserver(this);
}

void TouchSelectionControllerImpl::SelectionChanged() {
  gfx::Rect r1, r2;
  client_view_->GetSelectionEndPoints(&r1, &r2);
  gfx::Rect screen_rect_1 = ConvertToScreen(client_view_, r1);
  gfx::Rect screen_rect_2 = ConvertToScreen(client_view_, r2);
  gfx::Rect client_bounds = client_view_->GetBounds();
  if (r1.y() < client_bounds.y())
    r1.Inset(0, client_bounds.y() - r1.y(), 0, 0);
  if (r2.y() < client_bounds.y())
    r2.Inset(0, client_bounds.y() - r2.y(), 0, 0);
  gfx::Rect screen_rect_1_clipped = ConvertToScreen(client_view_, r1);
  gfx::Rect screen_rect_2_clipped = ConvertToScreen(client_view_, r2);
  if (screen_rect_1_clipped == selection_end_point_1_clipped_ &&
      screen_rect_2_clipped == selection_end_point_2_clipped_)
    return;

  selection_end_point_1_ = screen_rect_1;
  selection_end_point_2_ = screen_rect_2;
  selection_end_point_1_clipped_ = screen_rect_1_clipped;
  selection_end_point_2_clipped_ = screen_rect_2_clipped;

  if (client_view_->DrawsHandles()) {
    UpdateContextMenu();
    return;
  }
  if (dragging_handle_) {
    // We need to reposition only the selection handle that is being dragged.
    // The other handle stays the same. Also, the selection handle being dragged
    // will always be at the end of selection, while the other handle will be at
    // the start.
    // If the new location of this handle is out of client view, its widget
    // should not get hidden, since it should still receive touch events.
    // Hence, we are not using |SetHandleSelectionRect()| method here.
    dragging_handle_->SetSelectionRectInScreen(screen_rect_2_clipped);

    // Temporary fix for selection handle going outside a window. On a webpage,
    // the page should scroll if the selection handle is dragged outside the
    // window. That does not happen currently. So we just hide the handle for
    // now.
    // TODO(varunjain): Fix this: crbug.com/269003
    dragging_handle_->SetDrawInvisible(!ShouldShowHandleFor(r2));

    if (dragging_handle_ != cursor_handle_.get()) {
      // The non-dragging-handle might have recently become visible.
      EditingHandleView* non_dragging_handle = selection_handle_1_.get();
      if (dragging_handle_ == selection_handle_1_) {
        non_dragging_handle = selection_handle_2_.get();
        // if handle 1 is being dragged, it is corresponding to the end of
        // selection and the other handle to the start of selection.
        selection_end_point_1_ = screen_rect_2;
        selection_end_point_2_ = screen_rect_1;
        selection_end_point_1_clipped_ = screen_rect_2_clipped;
        selection_end_point_2_clipped_ = screen_rect_1_clipped;
      }
      SetHandleSelectionRect(non_dragging_handle, r1, screen_rect_1_clipped);
    }
  } else {
    UpdateContextMenu();

    // Check if there is any selection at all.
    if (screen_rect_1.origin() == screen_rect_2.origin()) {
      selection_handle_1_->SetWidgetVisible(false, false);
      selection_handle_2_->SetWidgetVisible(false, false);
      SetHandleSelectionRect(cursor_handle_.get(), r1, screen_rect_1_clipped);
      return;
    }

    cursor_handle_->SetWidgetVisible(false, false);
    SetHandleSelectionRect(selection_handle_1_.get(), r1,
                           screen_rect_1_clipped);
    SetHandleSelectionRect(selection_handle_2_.get(), r2,
                           screen_rect_2_clipped);
  }
}

bool TouchSelectionControllerImpl::IsHandleDragInProgress() {
  return !!dragging_handle_;
}

void TouchSelectionControllerImpl::HideHandles(bool quick) {
  selection_handle_1_->SetWidgetVisible(false, quick);
  selection_handle_2_->SetWidgetVisible(false, quick);
  cursor_handle_->SetWidgetVisible(false, quick);
}

void TouchSelectionControllerImpl::SetDraggingHandle(
    EditingHandleView* handle) {
  dragging_handle_ = handle;
  if (dragging_handle_)
    HideContextMenu();
  else
    StartContextMenuTimer();
}

void TouchSelectionControllerImpl::SelectionHandleDragged(
    const gfx::Point& drag_pos) {
  // We do not want to show the context menu while dragging.
  HideContextMenu();

  DCHECK(dragging_handle_);
  gfx::Point drag_pos_in_client = drag_pos;
  ConvertPointToClientView(dragging_handle_, &drag_pos_in_client);

  if (dragging_handle_ == cursor_handle_.get()) {
    client_view_->MoveCaretTo(drag_pos_in_client);
    return;
  }

  // Find the stationary selection handle.
  gfx::Rect fixed_handle_rect = selection_end_point_1_;
  if (selection_handle_1_ == dragging_handle_)
    fixed_handle_rect = selection_end_point_2_;

  // Find selection end points in client_view's coordinate system.
  gfx::Point p2 = fixed_handle_rect.origin();
  p2.Offset(0, fixed_handle_rect.height() / 2);
  client_view_->ConvertPointFromScreen(&p2);

  // Instruct client_view to select the region between p1 and p2. The position
  // of |fixed_handle| is the start and that of |dragging_handle| is the end
  // of selection.
  client_view_->SelectRect(p2, drag_pos_in_client);
}

void TouchSelectionControllerImpl::ConvertPointToClientView(
    EditingHandleView* source, gfx::Point* point) {
  View::ConvertPointToScreen(source, point);
  client_view_->ConvertPointFromScreen(point);
}

void TouchSelectionControllerImpl::SetHandleSelectionRect(
    EditingHandleView* handle,
    const gfx::Rect& rect,
    const gfx::Rect& rect_in_screen) {
  handle->SetWidgetVisible(ShouldShowHandleFor(rect), false);
  if (handle->IsWidgetVisible())
    handle->SetSelectionRectInScreen(rect_in_screen);
}

bool TouchSelectionControllerImpl::ShouldShowHandleFor(
    const gfx::Rect& rect) const {
  if (rect.height() < kSelectionHandleBarMinHeight)
    return false;
  gfx::Rect bounds = client_view_->GetBounds();
  bounds.Inset(0, 0, 0, -kSelectionHandleBarBottomAllowance);
  return bounds.Contains(rect);
}

bool TouchSelectionControllerImpl::IsCommandIdEnabled(int command_id) const {
  return client_view_->IsCommandIdEnabled(command_id);
}

void TouchSelectionControllerImpl::ExecuteCommand(int command_id,
                                                  int event_flags) {
  HideContextMenu();
  client_view_->ExecuteCommand(command_id, event_flags);
}

void TouchSelectionControllerImpl::OpenContextMenu() {
  // Context menu should appear centered on top of the selected region.
  const gfx::Rect rect = context_menu_->GetAnchorRect();
  const gfx::Point anchor(rect.CenterPoint().x(), rect.y());
  HideContextMenu();
  client_view_->OpenContextMenu(anchor);
}

void TouchSelectionControllerImpl::OnMenuClosed(TouchEditingMenuView* menu) {
  if (menu == context_menu_)
    context_menu_ = NULL;
}

void TouchSelectionControllerImpl::OnWidgetClosing(Widget* widget) {
  DCHECK_EQ(client_widget_, widget);
  client_widget_ = NULL;
}

void TouchSelectionControllerImpl::OnWidgetBoundsChanged(
    Widget* widget,
    const gfx::Rect& new_bounds) {
  DCHECK_EQ(client_widget_, widget);
  HideContextMenu();
  SelectionChanged();
}

void TouchSelectionControllerImpl::OnKeyEvent(ui::KeyEvent* event) {
  client_view_->DestroyTouchSelection();
}

void TouchSelectionControllerImpl::OnMouseEvent(ui::MouseEvent* event) {
  aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
      client_view_->GetNativeView()->GetRootWindow());
  if (!cursor_client || cursor_client->IsMouseEventsEnabled())
    client_view_->DestroyTouchSelection();
}

void TouchSelectionControllerImpl::OnScrollEvent(ui::ScrollEvent* event) {
  client_view_->DestroyTouchSelection();
}

void TouchSelectionControllerImpl::ContextMenuTimerFired() {
  // Get selection end points in client_view's space.
  gfx::Rect end_rect_1_in_screen;
  gfx::Rect end_rect_2_in_screen;
  if (cursor_handle_->IsWidgetVisible()) {
    end_rect_1_in_screen = selection_end_point_1_clipped_;
    end_rect_2_in_screen = end_rect_1_in_screen;
  } else {
    end_rect_1_in_screen = selection_end_point_1_clipped_;
    end_rect_2_in_screen = selection_end_point_2_clipped_;
  }

  // Convert from screen to client.
  gfx::Rect end_rect_1(ConvertFromScreen(client_view_, end_rect_1_in_screen));
  gfx::Rect end_rect_2(ConvertFromScreen(client_view_, end_rect_2_in_screen));

  // if selection is completely inside the view, we display the context menu
  // in the middle of the end points on the top. Else, we show it above the
  // visible handle. If no handle is visible, we do not show the menu.
  gfx::Rect menu_anchor;
  if (ShouldShowHandleFor(end_rect_1) &&
      ShouldShowHandleFor(end_rect_2))
    menu_anchor = Union(end_rect_1_in_screen,end_rect_2_in_screen);
  else if (ShouldShowHandleFor(end_rect_1))
    menu_anchor = end_rect_1_in_screen;
  else if (ShouldShowHandleFor(end_rect_2))
    menu_anchor = end_rect_2_in_screen;
  else
    return;

  DCHECK(!context_menu_);
  context_menu_ = TouchEditingMenuView::Create(this, menu_anchor,
                                               GetHandleImageSize(),
                                               client_view_->GetNativeView());
}

void TouchSelectionControllerImpl::StartContextMenuTimer() {
  if (context_menu_timer_.IsRunning())
    return;
  context_menu_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kContextMenuTimoutMs),
      this,
      &TouchSelectionControllerImpl::ContextMenuTimerFired);
}

void TouchSelectionControllerImpl::UpdateContextMenu() {
  // Hide context menu to be shown when the timer fires.
  HideContextMenu();
  StartContextMenuTimer();
}

void TouchSelectionControllerImpl::HideContextMenu() {
  if (context_menu_)
    context_menu_->Close();
  context_menu_ = NULL;
  context_menu_timer_.Stop();
}

gfx::NativeView TouchSelectionControllerImpl::GetCursorHandleNativeView() {
  return cursor_handle_->GetWidget()->GetNativeView();
}

gfx::Point TouchSelectionControllerImpl::GetSelectionHandle1Position() {
  return selection_handle_1_->GetScreenPosition();
}

gfx::Point TouchSelectionControllerImpl::GetSelectionHandle2Position() {
  return selection_handle_2_->GetScreenPosition();
}

gfx::Point TouchSelectionControllerImpl::GetCursorHandlePosition() {
  return cursor_handle_->GetScreenPosition();
}

bool TouchSelectionControllerImpl::IsSelectionHandle1Visible() {
  return selection_handle_1_->IsWidgetVisible();
}

bool TouchSelectionControllerImpl::IsSelectionHandle2Visible() {
  return selection_handle_2_->IsWidgetVisible();
}

bool TouchSelectionControllerImpl::IsCursorHandleVisible() {
  return cursor_handle_->IsWidgetVisible();
}

}  // namespace views
