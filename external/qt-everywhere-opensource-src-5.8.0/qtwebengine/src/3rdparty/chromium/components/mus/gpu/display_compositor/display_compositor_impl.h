// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_IMPL_H_
#define COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_IMPL_H_

#include "components/mus/public/interfaces/gpu/display_compositor.mojom.h"
#include "components/mus/public/interfaces/gpu/display_compositor_host.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mus {
namespace gpu {

class DisplayCompositorImpl : public mojom::DisplayCompositor {
 public:
  explicit DisplayCompositorImpl(
      mojo::InterfaceRequest<mojom::DisplayCompositor> request);
  ~DisplayCompositorImpl() override;

  // mojom::DisplayCompositor implementation.
  void CreateDisplay(int accelerated_widget,
                     mojo::InterfaceRequest<mojom::Display> display,
                     mojom::DisplayHostPtr host,
                     mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
                     mojom::CompositorFrameSinkClientPtr client) override;

 private:
  mojo::StrongBinding<mojom::DisplayCompositor> binding_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCompositorImpl);
};

}  // namespace gpu
}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_IMPL_H_
