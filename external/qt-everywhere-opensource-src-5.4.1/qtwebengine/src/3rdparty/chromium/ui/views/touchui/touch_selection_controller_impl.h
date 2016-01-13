// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#define UI_UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_

#include "base/timer/timer.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/gfx/point.h"
#include "ui/views/touchui/touch_editing_menu.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

namespace test {
class WidgetTestInteractive;
}

// Touch specific implementation of TouchSelectionController. Responsible for
// displaying selection handles and menu elements relevant in a touch interface.
class VIEWS_EXPORT TouchSelectionControllerImpl
    : public ui::TouchSelectionController,
      public TouchEditingMenuController,
      public WidgetObserver,
      public ui::EventHandler {
 public:
  class EditingHandleView;

  // Use TextSelectionController::create().
  explicit TouchSelectionControllerImpl(
      ui::TouchEditable* client_view);

  virtual ~TouchSelectionControllerImpl();

  // TextSelectionController.
  virtual void SelectionChanged() OVERRIDE;
  virtual bool IsHandleDragInProgress() OVERRIDE;
  virtual void HideHandles(bool quick) OVERRIDE;

 private:
  friend class TouchSelectionControllerImplTest;
  friend class test::WidgetTestInteractive;

  void SetDraggingHandle(EditingHandleView* handle);

  // Callback to inform the client view that the selection handle has been
  // dragged, hence selection may need to be updated.
  void SelectionHandleDragged(const gfx::Point& drag_pos);

  // Convenience method to convert a point from a selection handle's coordinate
  // system to that of the client view.
  void ConvertPointToClientView(EditingHandleView* source, gfx::Point* point);

  // Convenience method to set a handle's selection rect and hide it if it is
  // located out of client view.
  void SetHandleSelectionRect(EditingHandleView* handle, const gfx::Rect& rect,
                              const gfx::Rect& rect_in_screen);

  // Checks if handle should be shown for a selection end-point at |rect|.
  // |rect| should be the clipped version of the selection end-point.
  bool ShouldShowHandleFor(const gfx::Rect& rect) const;

  // Overridden from TouchEditingMenuController.
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual void OpenContextMenu() OVERRIDE;
  virtual void OnMenuClosed(TouchEditingMenuView* menu) OVERRIDE;

  // Overridden from WidgetObserver. We will observe the widget backing the
  // |client_view_| so that when its moved/resized, we can update the selection
  // handles appropriately.
  virtual void OnWidgetClosing(Widget* widget) OVERRIDE;
  virtual void OnWidgetBoundsChanged(Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // Overriden from ui::EventHandler.
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // Time to show context menu.
  void ContextMenuTimerFired();

  void StartContextMenuTimer();

  // Convenience method to update the position/visibility of the context menu.
  void UpdateContextMenu();

  // Convenience method for hiding context menu.
  void HideContextMenu();

  // Convenience methods for testing.
  gfx::NativeView GetCursorHandleNativeView();
  gfx::Point GetSelectionHandle1Position();
  gfx::Point GetSelectionHandle2Position();
  gfx::Point GetCursorHandlePosition();
  bool IsSelectionHandle1Visible();
  bool IsSelectionHandle2Visible();
  bool IsCursorHandleVisible();

  ui::TouchEditable* client_view_;
  Widget* client_widget_;
  scoped_ptr<EditingHandleView> selection_handle_1_;
  scoped_ptr<EditingHandleView> selection_handle_2_;
  scoped_ptr<EditingHandleView> cursor_handle_;
  TouchEditingMenuView* context_menu_;

  // Timer to trigger |context_menu| (|context_menu| is not shown if the
  // selection handles are being updated. It appears only when the handles are
  // stationary for a certain amount of time).
  base::OneShotTimer<TouchSelectionControllerImpl> context_menu_timer_;

  // Pointer to the SelectionHandleView being dragged during a drag session.
  EditingHandleView* dragging_handle_;

  // Selection end points. In cursor mode, the two end points are the same and
  // correspond to |cursor_handle_|; otherwise, they correspond to
  // |selection_handle_1_| and |selection_handle_2_|, respectively. These
  // values should be used when selection end points are needed rather than
  // position of handles which might be invalid when handles are hidden.
  gfx::Rect selection_end_point_1_;
  gfx::Rect selection_end_point_2_;
  // Selection end points, clipped to client view's boundaries.
  gfx::Rect selection_end_point_1_clipped_;
  gfx::Rect selection_end_point_2_clipped_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerImpl);
};

}  // namespace views

#endif  // UI_UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
