// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_MANAGER_H_
#define COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ServerWindow;
class ServerWindowSurface;
class ServerWindowSurfaceManagerTestApi;

// ServerWindowSurfaceManager tracks the surfaces associated with a
// ServerWindow.
class ServerWindowSurfaceManager {
 public:
  explicit ServerWindowSurfaceManager(ServerWindow* window);
  ~ServerWindowSurfaceManager();

  // Returns true if the surfaces from this manager should be drawn.
  bool ShouldDraw();

  // Creates a new surface of the specified type, replacing the existing one of
  // the specified type.
  void CreateSurface(mojom::SurfaceType surface_type,
                     mojo::InterfaceRequest<mojom::Surface> request,
                     mojom::SurfaceClientPtr client);

  ServerWindow* window() { return window_; }

  ServerWindowSurface* GetDefaultSurface() const;
  ServerWindowSurface* GetUnderlaySurface() const;
  ServerWindowSurface* GetSurfaceByType(mojom::SurfaceType type) const;
  bool HasSurfaceOfType(mojom::SurfaceType type) const;
  bool HasAnySurface() const;

  uint32_t id_namespace() const { return surface_id_allocator_.id_namespace(); }
  cc::SurfaceManager* GetSurfaceManager();

 private:
  friend class ServerWindowSurfaceManagerTestApi;
  friend class ServerWindowSurface;

  // Returns true if a surface of |type| has been set and its size is greater
  // than the size of the window.
  bool IsSurfaceReadyAndNonEmpty(mojom::SurfaceType type) const;

  cc::SurfaceId GenerateId();

  ServerWindow* window_;

  cc::SurfaceIdAllocator surface_id_allocator_;

  using TypeToSurfaceMap =
      std::map<mojom::SurfaceType, std::unique_ptr<ServerWindowSurface>>;

  TypeToSurfaceMap type_to_surface_map_;

  // While true the window is not drawn. This is initially true if the window
  // has the property |kWaitForUnderlay_Property|. This is set to false once
  // the underlay and default surface have been set *and* their size is at
  // least that of the window. Ideally we would wait for sizes to match, but
  // the underlay is not necessarily as big as the window.
  bool waiting_for_initial_frames_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowSurfaceManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_MANAGER_H_
