// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_IMPL_H_
#define COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_IMPL_H_

#include "base/memory/ref_counted.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/public/interfaces/gpu/display_compositor.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mus {
namespace gpu {

class CompositorFrameSinkDelegate;

// A client presents visuals to screen by submitting CompositorFrames to a
// CompositorFrameSink.
class CompositorFrameSinkImpl : public cc::SurfaceFactoryClient,
                                public mojom::CompositorFrameSink,
                                public cc::BeginFrameObserver {
 public:
  CompositorFrameSinkImpl(
      CompositorFrameSinkDelegate* delegate,
      int sink_id,
      const scoped_refptr<SurfacesState>& surfaces_state,
      mojo::InterfaceRequest<mojom::CompositorFrameSink> request,
      mojom::CompositorFrameSinkClientPtr client);
  ~CompositorFrameSinkImpl() override;

  // mojom::CompositorFrameSink implementation.
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(
      cc::CompositorFrame compositor_frame,
      const SubmitCompositorFrameCallback& callback) override;

 private:
  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::SurfaceId& surface_id,
                       const gfx::Rect& damage_rect) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  // BeginFrameObserver implementation.
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  const cc::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  void OnConnectionLost();

  CompositorFrameSinkDelegate* const delegate_;
  scoped_refptr<SurfacesState> surfaces_state_;
  const int sink_id_;
  cc::BeginFrameSource* begin_frame_source_;
  bool needs_begin_frame_;
  cc::BeginFrameArgs last_used_begin_frame_args_;
  cc::SurfaceFactory factory_;
  cc::SurfaceId surface_id_;
  gfx::Size last_submitted_frame_size_;
  mojom::CompositorFrameSinkClientPtr client_;
  mojo::Binding<mojom::CompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkImpl);
};

}  // namespace gpu
}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_IMPL_H_
