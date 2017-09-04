// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/user_display_manager.h"

#include <utility>

#include "services/ui/display/platform_screen.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/user_display_manager_delegate.h"

namespace ui {
namespace ws {

UserDisplayManager::UserDisplayManager(ws::DisplayManager* display_manager,
                                       UserDisplayManagerDelegate* delegate,
                                       const UserId& user_id)
    : display_manager_(display_manager),
      delegate_(delegate),
      user_id_(user_id),
      got_valid_frame_decorations_(
          delegate->GetFrameDecorationsForUser(user_id, nullptr)),
      current_cursor_location_(0) {}

UserDisplayManager::~UserDisplayManager() {}

void UserDisplayManager::OnFrameDecorationValuesChanged() {
  if (!got_valid_frame_decorations_) {
    got_valid_frame_decorations_ = true;
    display_manager_observers_.ForAllPtrs([this](
        mojom::DisplayManagerObserver* observer) { CallOnDisplays(observer); });
    if (test_observer_)
      CallOnDisplays(test_observer_);
    return;
  }

  mojo::Array<mojom::WsDisplayPtr> displays = GetAllDisplays();
  display_manager_observers_.ForAllPtrs(
      [this, &displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(displays.Clone().PassStorage());
      });
  if (test_observer_)
    test_observer_->OnDisplaysChanged(displays.Clone().PassStorage());
}

void UserDisplayManager::AddDisplayManagerBinding(
    mojo::InterfaceRequest<mojom::DisplayManager> request) {
  display_manager_bindings_.AddBinding(this, std::move(request));
}

void UserDisplayManager::OnDisplayUpdate(Display* display) {
  if (!got_valid_frame_decorations_)
    return;

  mojo::Array<mojom::WsDisplayPtr> displays(1);
  displays[0] = GetWsDisplayPtr(*display);

  display_manager_observers_.ForAllPtrs(
      [&displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(displays.Clone().PassStorage());
      });
  if (test_observer_)
    test_observer_->OnDisplaysChanged(displays.Clone().PassStorage());
}

void UserDisplayManager::OnWillDestroyDisplay(Display* display) {
  if (!got_valid_frame_decorations_)
    return;

  display_manager_observers_.ForAllPtrs(
      [&display](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplayRemoved(display->GetId());
      });
  if (test_observer_)
    test_observer_->OnDisplayRemoved(display->GetId());
}

void UserDisplayManager::OnPrimaryDisplayChanged(int64_t primary_display_id) {
  if (!got_valid_frame_decorations_)
    return;

  display_manager_observers_.ForAllPtrs(
      [primary_display_id](mojom::DisplayManagerObserver* observer) {
        observer->OnPrimaryDisplayChanged(primary_display_id);
      });
  if (test_observer_)
    test_observer_->OnPrimaryDisplayChanged(primary_display_id);
}

void UserDisplayManager::OnMouseCursorLocationChanged(const gfx::Point& point) {
  current_cursor_location_ =
      static_cast<base::subtle::Atomic32>(
          (point.x() & 0xFFFF) << 16 | (point.y() & 0xFFFF));
  if (cursor_location_memory()) {
    base::subtle::NoBarrier_Store(cursor_location_memory(),
                                  current_cursor_location_);
  }
}

mojo::ScopedSharedBufferHandle UserDisplayManager::GetCursorLocationMemory() {
  if (!cursor_location_handle_.is_valid()) {
    // Create our shared memory segment to share the cursor state with our
    // window clients.
    cursor_location_handle_ =
        mojo::SharedBufferHandle::Create(sizeof(base::subtle::Atomic32));

    if (!cursor_location_handle_.is_valid())
      return mojo::ScopedSharedBufferHandle();

    cursor_location_mapping_ =
        cursor_location_handle_->Map(sizeof(base::subtle::Atomic32));
    if (!cursor_location_mapping_)
      return mojo::ScopedSharedBufferHandle();
    base::subtle::NoBarrier_Store(cursor_location_memory(),
                                  current_cursor_location_);
  }

  return cursor_location_handle_->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_ONLY);
}

void UserDisplayManager::OnObserverAdded(
    mojom::DisplayManagerObserver* observer) {
  // Many clients key off the frame decorations to size widgets. Wait for frame
  // decorations before notifying so that we don't have to worry about clients
  // resizing appropriately.
  if (!got_valid_frame_decorations_)
    return;

  CallOnDisplays(observer);
}

mojom::WsDisplayPtr UserDisplayManager::GetWsDisplayPtr(
    const Display& display) {
  mojom::WsDisplayPtr ws_display = mojom::WsDisplay::New();
  ws_display->display = display.ToDisplay();
  delegate_->GetFrameDecorationsForUser(user_id_,
                                        &ws_display->frame_decoration_values);
  return ws_display;
}

mojo::Array<mojom::WsDisplayPtr> UserDisplayManager::GetAllDisplays() {
  const auto& displays = display_manager_->displays();
  mojo::Array<mojom::WsDisplayPtr> display_ptrs(displays.size());

  size_t i = 0;
  // TODO(sky): need ordering!
  for (Display* display : displays) {
    display_ptrs[i] = GetWsDisplayPtr(*display);
    ++i;
  }

  return display_ptrs;
}

void UserDisplayManager::CallOnDisplays(
    mojom::DisplayManagerObserver* observer) {
  // TODO(kylechar): Pass internal display id to clients here.
  observer->OnDisplays(
      GetAllDisplays().PassStorage(),
      display::PlatformScreen::GetInstance()->GetPrimaryDisplayId(),
      display::Display::kInvalidDisplayID);
}

void UserDisplayManager::AddObserver(
    mojom::DisplayManagerObserverPtr observer) {
  mojom::DisplayManagerObserver* observer_impl = observer.get();
  display_manager_observers_.AddPtr(std::move(observer));
  OnObserverAdded(observer_impl);
}

}  // namespace ws
}  // namespace ui
