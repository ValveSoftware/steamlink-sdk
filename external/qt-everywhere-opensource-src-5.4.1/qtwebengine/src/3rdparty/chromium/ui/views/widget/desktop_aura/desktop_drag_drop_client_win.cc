// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source_win.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/views/widget/desktop_aura/desktop_drop_target_win.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {

DesktopDragDropClientWin::DesktopDragDropClientWin(
    aura::Window* root_window,
    HWND window)
    : drag_drop_in_progress_(false),
      drag_operation_(0) {
  drop_target_ = new DesktopDropTargetWin(root_window, window);
}

DesktopDragDropClientWin::~DesktopDragDropClientWin() {
}

int DesktopDragDropClientWin::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  drag_drop_in_progress_ = true;
  drag_operation_ = operation;

  drag_source_ = new ui::DragSourceWin;
  DWORD effect;
  HRESULT result = DoDragDrop(
      ui::OSExchangeDataProviderWin::GetIDataObject(data),
      drag_source_,
      ui::DragDropTypes::DragOperationToDropEffect(operation),
      &effect);

  drag_drop_in_progress_ = false;

  if (result != DRAGDROP_S_DROP)
    effect = DROPEFFECT_NONE;

  return ui::DragDropTypes::DropEffectToDragOperation(effect);
}

void DesktopDragDropClientWin::DragUpdate(aura::Window* target,
                                          const ui::LocatedEvent& event) {
}

void DesktopDragDropClientWin::Drop(aura::Window* target,
                                    const ui::LocatedEvent& event) {
}

void DesktopDragDropClientWin::DragCancel() {
  drag_source_->CancelDrag();
  drag_operation_ = 0;
}

bool DesktopDragDropClientWin::IsDragDropInProgress() {
  return drag_drop_in_progress_;
}

void DesktopDragDropClientWin::OnNativeWidgetDestroying(HWND window) {
  if (drop_target_.get()) {
    RevokeDragDrop(window);
    drop_target_ = NULL;
  }
}

}  // namespace views
