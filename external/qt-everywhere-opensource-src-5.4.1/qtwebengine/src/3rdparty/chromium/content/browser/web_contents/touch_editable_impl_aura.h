// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_TOUCH_EDITABLE_IMPL_AURA_H_
#define CONTENT_BROWSER_WEB_CONTENTS_TOUCH_EDITABLE_IMPL_AURA_H_

#include <deque>
#include <map>
#include <queue>

#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/window_observer.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace ui {
class Accelerator;
}

namespace content {
class TouchEditableImplAuraTest;

// Aura specific implementation of ui::TouchEditable for a RenderWidgetHostView.
class CONTENT_EXPORT TouchEditableImplAura
    : public ui::TouchEditable,
      public NON_EXPORTED_BASE(RenderWidgetHostViewAura::TouchEditingClient) {
 public:
  virtual ~TouchEditableImplAura();

  static TouchEditableImplAura* Create();

  void AttachToView(RenderWidgetHostViewAura* view);

  // Updates the |touch_selection_controller_| or ends touch editing session
  // depending on the current selection and cursor state.
  void UpdateEditingController();

  void OverscrollStarted();
  void OverscrollCompleted();

  // Overridden from RenderWidgetHostViewAura::TouchEditingClient.
  virtual void StartTouchEditing() OVERRIDE;
  virtual void EndTouchEditing(bool quick) OVERRIDE;
  virtual void OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                          const gfx::Rect& focus) OVERRIDE;
  virtual void OnTextInputTypeChanged(ui::TextInputType type) OVERRIDE;
  virtual bool HandleInputEvent(const ui::Event* event) OVERRIDE;
  virtual void GestureEventAck(int gesture_event_type) OVERRIDE;
  virtual void OnViewDestroyed() OVERRIDE;

  // Overridden from ui::TouchEditable:
  virtual void SelectRect(const gfx::Point& start,
                          const gfx::Point& end) OVERRIDE;
  virtual void MoveCaretTo(const gfx::Point& point) OVERRIDE;
  virtual void GetSelectionEndPoints(gfx::Rect* p1, gfx::Rect* p2) OVERRIDE;
  virtual gfx::Rect GetBounds() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual void ConvertPointToScreen(gfx::Point* point) OVERRIDE;
  virtual void ConvertPointFromScreen(gfx::Point* point) OVERRIDE;
  virtual bool DrawsHandles() OVERRIDE;
  virtual void OpenContextMenu(const gfx::Point& anchor) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual void DestroyTouchSelection() OVERRIDE;

 protected:
  TouchEditableImplAura();

 private:
  friend class TouchEditableImplAuraTest;

  void Cleanup();

  // Rectangles for the selection anchor and focus.
  gfx::Rect selection_anchor_rect_;
  gfx::Rect selection_focus_rect_;

  // The current text input type.
  ui::TextInputType text_input_type_;

  RenderWidgetHostViewAura* rwhva_;
  scoped_ptr<ui::TouchSelectionController> touch_selection_controller_;

  // True if |rwhva_| is currently handling a gesture that could result in a
  // change in selection (long press, double tap or triple tap).
  bool selection_gesture_in_process_;

  // Set to true if handles are hidden when user is scrolling. Used to determine
  // whether to re-show handles after a scrolling session.
  bool handles_hidden_due_to_scroll_;

  // Keeps track of when the user is scrolling.
  bool scroll_in_progress_;

  // Set to true when the page starts an overscroll.
  bool overscroll_in_progress_;

  // Used to track if a textfield was focused when the current tap gesture
  // happened.
  bool textfield_was_focused_on_tap_;

  DISALLOW_COPY_AND_ASSIGN(TouchEditableImplAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_TOUCH_EDITABLE_IMPL_AURA_H_
