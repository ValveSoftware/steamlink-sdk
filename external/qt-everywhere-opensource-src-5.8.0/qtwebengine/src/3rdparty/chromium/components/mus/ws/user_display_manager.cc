// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/user_display_manager.h"

#include "components/mus/ws/display.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/display_manager_delegate.h"

namespace mus {
namespace ws {

UserDisplayManager::UserDisplayManager(ws::DisplayManager* display_manager,
                                       DisplayManagerDelegate* delegate,
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

  mojo::Array<mojom::DisplayPtr> displays = GetAllDisplays();
  display_manager_observers_.ForAllPtrs(
      [this, &displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(displays.Clone());
      });
  if (test_observer_)
    test_observer_->OnDisplaysChanged(displays.Clone());
}

void UserDisplayManager::AddDisplayManagerBinding(
    mojo::InterfaceRequest<mojom::DisplayManager> request) {
  display_manager_bindings_.AddBinding(this, std::move(request));
}

void UserDisplayManager::OnWillDestroyDisplay(Display* display) {
  if (!got_valid_frame_decorations_)
    return;

  display_manager_observers_.ForAllPtrs(
      [this, &display](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplayRemoved(display->id());
      });
  if (test_observer_)
    test_observer_->OnDisplayRemoved(display->id());
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

void UserDisplayManager::OnDisplayUpdate(Display* display) {
  if (!got_valid_frame_decorations_)
    return;

  mojo::Array<mojom::DisplayPtr> displays(1);
  displays[0] = display->ToMojomDisplay();
  delegate_->GetFrameDecorationsForUser(
      user_id_, &(displays[0]->frame_decoration_values));
  display_manager_observers_.ForAllPtrs(
      [this, &displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(displays.Clone());
      });
  if (test_observer_)
    test_observer_->OnDisplaysChanged(displays.Clone());
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

mojo::Array<mojom::DisplayPtr> UserDisplayManager::GetAllDisplays() {
  const std::set<Display*>& displays = display_manager_->displays();
  mojo::Array<mojom::DisplayPtr> display_ptrs(displays.size());
  {
    size_t i = 0;
    // TODO(sky): need ordering!
    for (Display* display : displays) {
      display_ptrs[i] = display->ToMojomDisplay();
      delegate_->GetFrameDecorationsForUser(
          user_id_, &(display_ptrs[i]->frame_decoration_values));
      ++i;
    }
  }
  return display_ptrs;
}

void UserDisplayManager::CallOnDisplays(
    mojom::DisplayManagerObserver* observer) {
  observer->OnDisplays(GetAllDisplays());
}

void UserDisplayManager::AddObserver(
    mojom::DisplayManagerObserverPtr observer) {
  mojom::DisplayManagerObserver* observer_impl = observer.get();
  display_manager_observers_.AddPtr(std::move(observer));
  OnObserverAdded(observer_impl);
}

}  // namespace ws
}  // namespace mus
