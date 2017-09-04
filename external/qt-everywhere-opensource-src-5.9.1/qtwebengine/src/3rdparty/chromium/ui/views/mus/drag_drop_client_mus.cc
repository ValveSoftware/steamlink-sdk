// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/drag_drop_client_mus.h"

#include <map>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/ui/public/cpp/window.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/window.h"

namespace views {
namespace {

DragDropClientMus* current_dragging_client = nullptr;

}  // namespace

DragDropClientMus::DragDropClientMus(ui::Window* ui_window)
    : ui_window_(ui_window) {}

DragDropClientMus::~DragDropClientMus() {}

int DragDropClientMus::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int drag_operations,
    ui::DragDropTypes::DragEventSource source) {
  std::map<std::string, std::vector<uint8_t>> drag_data =
      static_cast<const aura::OSExchangeDataProviderMus&>(data.provider())
          .GetData();

  // TODO(erg): Right now, I'm passing the cursor_location, but maybe I want to
  // pass OSExchangeData::GetDragImageOffset() instead?

  bool success = false;
  gfx::Point cursor_location = screen_location;
  uint32_t action_taken = ui::mojom::kDropEffectNone;
  current_dragging_client = this;
  ui_window_->PerformDragDrop(
      drag_data, drag_operations, cursor_location,
      *data.provider().GetDragImage().bitmap(),
      base::Bind(&DragDropClientMus::OnMoveLoopEnd, base::Unretained(this),
                 &success, &action_taken));

  base::MessageLoop* loop = base::MessageLoop::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;

  runloop_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  current_dragging_client = nullptr;

  return action_taken;
}

void DragDropClientMus::DragCancel() {
  ui_window_->CancelDragDrop();
}

bool DragDropClientMus::IsDragDropInProgress() {
  return !!current_dragging_client;
}

void DragDropClientMus::OnMoveLoopEnd(bool* out_success,
                                      uint32_t* out_action,
                                      bool in_success,
                                      uint32_t in_action) {
  *out_success = in_success;
  *out_action = in_action;
  runloop_quit_closure_.Run();
}

}  // namespace views
