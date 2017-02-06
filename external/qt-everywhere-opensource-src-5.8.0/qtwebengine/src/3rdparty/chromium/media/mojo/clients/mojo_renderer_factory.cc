// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_factory.h"

#include "base/single_thread_task_runner.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/renderers/video_overlay_factory.h"
#include "services/shell/public/cpp/connect.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    const GetGpuFactoriesCB& get_gpu_factories_cb,
    shell::mojom::InterfaceProvider* interface_provider)
    : get_gpu_factories_cb_(get_gpu_factories_cb),
      interface_provider_(interface_provider) {
  DCHECK(!get_gpu_factories_cb_.is_null());
  DCHECK(interface_provider_);
}

MojoRendererFactory::~MojoRendererFactory() {}

std::unique_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* video_renderer_sink,
    const RequestSurfaceCB& /* request_surface_cb */) {
  std::unique_ptr<VideoOverlayFactory> overlay_factory(
      new VideoOverlayFactory(get_gpu_factories_cb_.Run()));

  mojom::RendererPtr renderer_ptr;
  shell::GetInterface<mojom::Renderer>(interface_provider_, &renderer_ptr);

  return std::unique_ptr<Renderer>(
      new MojoRenderer(media_task_runner, std::move(overlay_factory),
                       video_renderer_sink, std::move(renderer_ptr)));
}

}  // namespace media
