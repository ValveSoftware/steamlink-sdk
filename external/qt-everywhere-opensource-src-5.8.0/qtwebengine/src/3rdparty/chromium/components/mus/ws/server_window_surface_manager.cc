// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window_surface_manager.h"

#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_surface.h"

namespace mus {
namespace ws {

ServerWindowSurfaceManager::ServerWindowSurfaceManager(ServerWindow* window)
    : window_(window),
      surface_id_allocator_(
          window->delegate()->GetSurfacesState()->next_id_namespace()),
      waiting_for_initial_frames_(
          window_->properties().count(mus::mojom::kWaitForUnderlay_Property) >
          0) {
  surface_id_allocator_.RegisterSurfaceIdNamespace(GetSurfaceManager());
}

ServerWindowSurfaceManager::~ServerWindowSurfaceManager() {
  // Explicitly clear the type to surface manager so that this manager
  // is still valid prior during ~ServerWindowSurface.
  type_to_surface_map_.clear();
}

bool ServerWindowSurfaceManager::ShouldDraw() {
  if (!waiting_for_initial_frames_)
    return true;

  waiting_for_initial_frames_ =
      !IsSurfaceReadyAndNonEmpty(mojom::SurfaceType::DEFAULT) ||
      !IsSurfaceReadyAndNonEmpty(mojom::SurfaceType::UNDERLAY);
  return !waiting_for_initial_frames_;
}

void ServerWindowSurfaceManager::CreateSurface(
    mojom::SurfaceType surface_type,
    mojo::InterfaceRequest<mojom::Surface> request,
    mojom::SurfaceClientPtr client) {
  std::unique_ptr<ServerWindowSurface> surface(
      new ServerWindowSurface(this, std::move(request), std::move(client)));
  if (!HasAnySurface()) {
    // Only one SurfaceFactoryClient can be registered per surface id namespace,
    // so register the first one.  Since all surfaces created by this manager
    // represent the same window, the begin frame source can be shared by
    // all surfaces created here.
    surface->RegisterForBeginFrames();
  }
  type_to_surface_map_[surface_type] = std::move(surface);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetDefaultSurface() const {
  return GetSurfaceByType(mojom::SurfaceType::DEFAULT);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetUnderlaySurface() const {
  return GetSurfaceByType(mojom::SurfaceType::UNDERLAY);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetSurfaceByType(
    mojom::SurfaceType type) const {
  auto iter = type_to_surface_map_.find(type);
  return iter == type_to_surface_map_.end() ? nullptr : iter->second.get();
}

bool ServerWindowSurfaceManager::HasSurfaceOfType(
    mojom::SurfaceType type) const {
  return type_to_surface_map_.count(type) > 0;
}

bool ServerWindowSurfaceManager::HasAnySurface() const {
  return GetDefaultSurface() || GetUnderlaySurface();
}

cc::SurfaceManager* ServerWindowSurfaceManager::GetSurfaceManager() {
  return window()->delegate()->GetSurfacesState()->manager();
}

bool ServerWindowSurfaceManager::IsSurfaceReadyAndNonEmpty(
    mojom::SurfaceType type) const {
  auto iter = type_to_surface_map_.find(type);
  if (iter == type_to_surface_map_.end())
    return false;
  if (iter->second->last_submitted_frame_size().IsEmpty())
    return false;
  const gfx::Size& last_submitted_frame_size =
      iter->second->last_submitted_frame_size();
  return last_submitted_frame_size.width() >= window_->bounds().width() &&
         last_submitted_frame_size.height() >= window_->bounds().height();
}

cc::SurfaceId ServerWindowSurfaceManager::GenerateId() {
  return surface_id_allocator_.GenerateId();
}

}  // namespace ws
}  // namespace mus
