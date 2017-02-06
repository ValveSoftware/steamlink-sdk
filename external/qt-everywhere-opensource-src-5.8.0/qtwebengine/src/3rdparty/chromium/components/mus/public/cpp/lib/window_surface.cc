// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_surface.h"

#include "base/memory/ptr_util.h"
#include "components/mus/public/cpp/window_surface_client.h"

namespace mus {

// static
std::unique_ptr<WindowSurface> WindowSurface::Create(
    std::unique_ptr<WindowSurfaceBinding>* surface_binding) {
  mojom::SurfacePtr surface;
  mojom::SurfaceClientPtr surface_client;
  mojo::InterfaceRequest<mojom::SurfaceClient> surface_client_request =
      GetProxy(&surface_client);

  surface_binding->reset(new WindowSurfaceBinding(
      GetProxy(&surface), surface_client.PassInterface()));
  return base::WrapUnique(new WindowSurface(surface.PassInterface(),
                                            std::move(surface_client_request)));
}

WindowSurface::~WindowSurface() {}

void WindowSurface::BindToThread() {
  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
  surface_.Bind(std::move(surface_info_));
  client_binding_.reset(new mojo::Binding<mojom::SurfaceClient>(
      this, std::move(client_request_)));
}

void WindowSurface::SubmitCompositorFrame(cc::CompositorFrame frame,
                                          const base::Closure& callback) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!surface_)
    return;
  surface_->SubmitCompositorFrame(std::move(frame), callback);
}

WindowSurface::WindowSurface(
    mojo::InterfacePtrInfo<mojom::Surface> surface_info,
    mojo::InterfaceRequest<mojom::SurfaceClient> client_request)
    : client_(nullptr),
      surface_info_(std::move(surface_info)),
      client_request_(std::move(client_request)) {}

void WindowSurface::ReturnResources(
    mojo::Array<cc::ReturnedResource> resources) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->OnResourcesReturned(this, std::move(resources));
}

WindowSurfaceBinding::~WindowSurfaceBinding() {}

WindowSurfaceBinding::WindowSurfaceBinding(
    mojo::InterfaceRequest<mojom::Surface> surface_request,
    mojo::InterfacePtrInfo<mojom::SurfaceClient> surface_client)
    : surface_request_(std::move(surface_request)),
      surface_client_(std::move(surface_client)) {}

}  // namespace mus
