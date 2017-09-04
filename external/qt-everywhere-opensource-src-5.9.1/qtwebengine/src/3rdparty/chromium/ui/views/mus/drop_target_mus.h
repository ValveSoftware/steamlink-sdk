// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_DROP_TARGET_MUS_H_
#define UI_VIEWS_MUS_DROP_TARGET_MUS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "services/ui/public/cpp/window_drop_target.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;

namespace client {
class DragDropDelegate;
}
}

namespace ui {
class DropTargetEvent;
class OSExchangeData;
}

namespace views {

// An adapter class which takes signals from mus' WindowDropTarget, performs
// targeting on the underlying aura::Window tree, and dispatches them to the
// aura DragDropDelegate of the targeted aura::Window.
class DropTargetMus : public ui::WindowDropTarget, public aura::WindowObserver {
 public:
  explicit DropTargetMus(aura::Window* root_window);
  ~DropTargetMus() override;

 private:
  // Common functionality for the WindowDropTarget methods to translate from
  // mus data types to Aura ones. This method takes in mus messages and
  // performs the common tasks of keeping track of which aura window (along
  // with enter/leave messages at that layer), doing coordinate translation,
  // and creation of ui layer event objects that get dispatched to aura/views.
  void Translate(uint32_t key_state,
                 const gfx::Point& screen_location,
                 uint32_t effect_bitmask,
                 std::unique_ptr<ui::DropTargetEvent>* event,
                 aura::client::DragDropDelegate** delegate);

  void NotifyDragExited();

  // Overridden from ui::WindowDropTarget:
  void OnDragDropStart(
      std::map<std::string, std::vector<uint8_t>> mime_data) override;
  uint32_t OnDragEnter(uint32_t key_state,
                       const gfx::Point& position,
                       uint32_t effect_bitmask) override;
  uint32_t OnDragOver(uint32_t key_state,
                      const gfx::Point& position,
                      uint32_t effect_bitmask) override;
  void OnDragLeave() override;
  uint32_t OnCompleteDrop(uint32_t key_state,
                          const gfx::Point& position,
                          uint32_t effect_bitmask) override;
  void OnDragDropDone() override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // The root window associated with this drop target.
  aura::Window* root_window_;

  // The Aura window that is currently under the cursor. We need to manually
  // keep track of this because mus will only call our drag enter method once
  // when the user enters the associated mus::Window. But inside mus there
  // could be multiple aura windows, so we need to generate drag enter events
  // for them.
  aura::Window* target_window_;

  // The entire drag data payload. We receive this during the drag enter event
  // and cache it so we don't send this multiple times. We reset this value on
  // leave or drop.
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;

  DISALLOW_COPY_AND_ASSIGN(DropTargetMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_DROP_TARGET_MUS_H_
