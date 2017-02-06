// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_binding.h"

#include <stdint.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "components/mus/public/cpp/context_provider.h"
#include "components/mus/public/cpp/output_surface.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/views/mus/window_tree_host_mus.h"

namespace views {

// PerClientState --------------------------------------------------------------

// State needed per WindowTreeClient. Provides the real implementation of
// CreateOutputSurface. SurfaceBinding obtains a pointer to the
// PerClientState appropriate for the WindowTreeClient. PerClientState is
// stored in a thread local map. When no more refereces to a PerClientState
// remain the PerClientState is deleted and the underlying map cleaned up.
class SurfaceBinding::PerClientState : public base::RefCounted<PerClientState> {
 public:
  static PerClientState* Get(shell::Connector* connector,
                             mus::WindowTreeClient* client);

  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      mus::Window* window,
      mus::mojom::SurfaceType type);

 private:
  typedef std::map<mus::WindowTreeClient*, PerClientState*> ClientToStateMap;

  friend class base::RefCounted<PerClientState>;

  PerClientState(shell::Connector* connector,
                 mus::WindowTreeClient* client);
  ~PerClientState();

  static base::LazyInstance<
      base::ThreadLocalPointer<ClientToStateMap>>::Leaky window_states;

  shell::Connector* connector_;
  mus::WindowTreeClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PerClientState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerClientState::ClientToStateMap>>::Leaky
    SurfaceBinding::PerClientState::window_states;

// static
SurfaceBinding::PerClientState* SurfaceBinding::PerClientState::Get(
    shell::Connector* connector,
    mus::WindowTreeClient* client) {
  // |connector| can be null in some unit-tests.
  if (!connector)
    return nullptr;
  ClientToStateMap* window_map = window_states.Pointer()->Get();
  if (!window_map) {
    window_map = new ClientToStateMap;
    window_states.Pointer()->Set(window_map);
  }
  if (!(*window_map)[client])
    (*window_map)[client] = new PerClientState(connector, client);
  return (*window_map)[client];
}

std::unique_ptr<cc::OutputSurface>
SurfaceBinding::PerClientState::CreateOutputSurface(
    mus::Window* window,
    mus::mojom::SurfaceType surface_type) {
  scoped_refptr<cc::ContextProvider> context_provider(
      new mus::ContextProvider(connector_));
  return base::WrapUnique(new mus::OutputSurface(
      context_provider, window->RequestSurface(surface_type)));
}

SurfaceBinding::PerClientState::PerClientState(
    shell::Connector* connector,
    mus::WindowTreeClient* client)
    : connector_(connector), client_(client) {}

SurfaceBinding::PerClientState::~PerClientState() {
  ClientToStateMap* window_map = window_states.Pointer()->Get();
  DCHECK(window_map);
  DCHECK_EQ(this, (*window_map)[client_]);
  window_map->erase(client_);
  if (window_map->empty()) {
    delete window_map;
    window_states.Pointer()->Set(nullptr);
  }
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(shell::Connector* connector,
                               mus::Window* window,
                               mus::mojom::SurfaceType surface_type)
    : window_(window),
      surface_type_(surface_type),
      state_(PerClientState::Get(connector, window->window_tree())) {}

SurfaceBinding::~SurfaceBinding() {}

std::unique_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_ ? state_->CreateOutputSurface(window_, surface_type_) : nullptr;
}

}  // namespace views
