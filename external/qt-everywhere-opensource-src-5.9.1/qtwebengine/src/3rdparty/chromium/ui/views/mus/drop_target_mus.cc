// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/drop_target_mus.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"

namespace views {

static_assert(ui::DragDropTypes::DRAG_NONE == ui::mojom::kDropEffectNone,
              "Drag constants must be the same");
static_assert(ui::DragDropTypes::DRAG_MOVE == ui::mojom::kDropEffectMove,
              "Drag constants must be the same");
static_assert(ui::DragDropTypes::DRAG_COPY == ui::mojom::kDropEffectCopy,
              "Drag constants must be the same");
static_assert(ui::DragDropTypes::DRAG_LINK == ui::mojom::kDropEffectLink,
              "Drag constants must be the same");

DropTargetMus::DropTargetMus(aura::Window* root_window)
    : root_window_(root_window), target_window_(nullptr) {}

DropTargetMus::~DropTargetMus() {}

void DropTargetMus::Translate(uint32_t key_state,
                              const gfx::Point& screen_location,
                              uint32_t effect,
                              std::unique_ptr<ui::DropTargetEvent>* event,
                              aura::client::DragDropDelegate** delegate) {
  gfx::Point location = screen_location;
  gfx::Point root_location = location;
  root_window_->GetHost()->ConvertPointFromNativeScreen(&root_location);
  aura::Window* target_window =
      root_window_->GetEventHandlerForPoint(root_location);
  bool target_window_changed = false;
  if (target_window != target_window_) {
    if (target_window_)
      NotifyDragExited();
    target_window_ = target_window;
    if (target_window_)
      target_window_->AddObserver(this);
    target_window_changed = true;
  }
  *delegate = nullptr;
  if (!target_window_)
    return;
  *delegate = aura::client::GetDragDropDelegate(target_window_);
  if (!*delegate)
    return;

  location = root_location;
  aura::Window::ConvertPointToTarget(root_window_, target_window_, &location);
  *event = base::MakeUnique<ui::DropTargetEvent>(
      *(os_exchange_data_.get()), location, root_location, effect);
  (*event)->set_flags(key_state);
  if (target_window_changed)
    (*delegate)->OnDragEntered(*event->get());
}

void DropTargetMus::NotifyDragExited() {
  if (!target_window_)
    return;

  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target_window_);
  if (delegate)
    delegate->OnDragExited();

  target_window_->RemoveObserver(this);
  target_window_ = nullptr;
}

void DropTargetMus::OnDragDropStart(
    std::map<std::string, std::vector<uint8_t>> mime_data) {
  // We store the mime data here because we need to access it during each phase
  // of the drag, but we also don't move the data cross-process multiple times.
  os_exchange_data_ = base::MakeUnique<ui::OSExchangeData>(
      base::MakeUnique<aura::OSExchangeDataProviderMus>(std::move(mime_data)));
}

uint32_t DropTargetMus::OnDragEnter(uint32_t key_state,
                                    const gfx::Point& position,
                                    uint32_t effect_bitmask) {
  std::unique_ptr<ui::DropTargetEvent> event;
  aura::client::DragDropDelegate* delegate = nullptr;
  // Translate will call OnDragEntered.
  Translate(key_state, position, effect_bitmask, &event, &delegate);
  return ui::mojom::kDropEffectNone;
}

uint32_t DropTargetMus::OnDragOver(uint32_t key_state,
                                   const gfx::Point& position,
                                   uint32_t effect) {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  std::unique_ptr<ui::DropTargetEvent> event;
  aura::client::DragDropDelegate* delegate = nullptr;

  Translate(key_state, position, effect, &event, &delegate);
  if (delegate)
    drag_operation = delegate->OnDragUpdated(*event);
  return drag_operation;
}

void DropTargetMus::OnDragLeave() {
  NotifyDragExited();
}

uint32_t DropTargetMus::OnCompleteDrop(uint32_t key_state,
                                       const gfx::Point& position,
                                       uint32_t effect) {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  std::unique_ptr<ui::DropTargetEvent> event;
  aura::client::DragDropDelegate* delegate = nullptr;
  Translate(key_state, position, effect, &event, &delegate);
  if (delegate)
    drag_operation = delegate->OnPerformDrop(*event);
  if (target_window_) {
    target_window_->RemoveObserver(this);
    target_window_ = nullptr;
  }

  return drag_operation;
}

void DropTargetMus::OnDragDropDone() {
  os_exchange_data_.reset();
}

void DropTargetMus::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, target_window_);
  target_window_ = nullptr;
}

}  // namespace views
