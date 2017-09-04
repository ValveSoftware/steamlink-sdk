// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_DRAG_DROP_CLIENT_MUS_H_
#define UI_VIEWS_MUS_DRAG_DROP_CLIENT_MUS_H_

#include "base/callback.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"

namespace aura {
class Window;
}

namespace ui {
class OSExchangeData;
class Window;
}

namespace views {

// An aura client object that translates aura dragging methods to their
// remote ui::Window equivalents.
class DragDropClientMus : public aura::client::DragDropClient {
 public:
  DragDropClientMus(ui::Window* ui_window);
  ~DragDropClientMus() override;

  // Overridden from aura::client::DragDropClient:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int drag_operations,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;

 private:
  // Callback for StartDragAndDrop().
  void OnMoveLoopEnd(bool* out_success,
                     uint32_t* out_action,
                     bool in_success,
                     uint32_t in_action);

  ui::Window* ui_window_;

  base::Closure runloop_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(DragDropClientMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_DRAG_DROP_CLIENT_MUS_H_
