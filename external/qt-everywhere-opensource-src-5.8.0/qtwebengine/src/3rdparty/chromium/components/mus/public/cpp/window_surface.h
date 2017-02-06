// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/mus/public/interfaces/surface.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"

namespace mus {

class WindowSurfaceBinding;
class WindowSurfaceClient;
class Window;

// A WindowSurface is wrapper to simplify submitting CompositorFrames to
// Windows, and receiving ReturnedResources.
class WindowSurface : public mojom::SurfaceClient {
 public:
  // static
  static std::unique_ptr<WindowSurface> Create(
      std::unique_ptr<WindowSurfaceBinding>* surface_binding);

  ~WindowSurface() override;

  // Called to indicate that the current thread has assumed control of this
  // object.
  void BindToThread();

  void SubmitCompositorFrame(cc::CompositorFrame frame,
                             const base::Closure& callback);

  void set_client(WindowSurfaceClient* client) { client_ = client; }

 private:
  friend class Window;

  WindowSurface(mojo::InterfacePtrInfo<mojom::Surface> surface_info,
                mojo::InterfaceRequest<mojom::SurfaceClient> client_request);

  // SurfaceClient implementation:
  void ReturnResources(mojo::Array<cc::ReturnedResource> resources) override;

  WindowSurfaceClient* client_;
  mojo::InterfacePtrInfo<mojom::Surface> surface_info_;
  mojo::InterfaceRequest<mojom::SurfaceClient> client_request_;
  mojom::SurfacePtr surface_;
  std::unique_ptr<mojo::Binding<mojom::SurfaceClient>> client_binding_;
  std::unique_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(WindowSurface);
};

// A WindowSurfaceBinding is a bundle of mojo interfaces that are to be used by
// or implemented by the Mus window server when passed into
// Window::AttachSurface. WindowSurfaceBinding has no standalone functionality.
class WindowSurfaceBinding {
 public:
  ~WindowSurfaceBinding();

 private:
  friend class WindowSurface;
  friend class Window;

  WindowSurfaceBinding(
      mojo::InterfaceRequest<mojom::Surface> surface_request,
      mojo::InterfacePtrInfo<mojom::SurfaceClient> surface_client);

  mojo::InterfaceRequest<mojom::Surface> surface_request_;
  mojo::InterfacePtrInfo<mojom::SurfaceClient> surface_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowSurfaceBinding);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_H_
