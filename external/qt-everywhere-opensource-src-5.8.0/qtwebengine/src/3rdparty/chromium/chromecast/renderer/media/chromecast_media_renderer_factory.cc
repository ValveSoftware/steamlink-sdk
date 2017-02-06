// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/chromecast_media_renderer_factory.h"

#include <utility>

#include "chromecast/renderer/media/cma_renderer.h"
#include "chromecast/renderer/media/media_pipeline_proxy.h"
#include "content/public/renderer/render_thread.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace chromecast {
namespace media {

ChromecastMediaRendererFactory::ChromecastMediaRendererFactory(
    ::media::GpuVideoAcceleratorFactories* gpu_factories,
    int render_frame_id)
    : render_frame_id_(render_frame_id), gpu_factories_(gpu_factories) {}

ChromecastMediaRendererFactory::~ChromecastMediaRendererFactory() {
}

std::unique_ptr<::media::Renderer>
ChromecastMediaRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    ::media::AudioRendererSink* audio_renderer_sink,
    ::media::VideoRendererSink* video_renderer_sink,
    const ::media::RequestSurfaceCB& request_surface_cb) {
  // TODO(erickung): crbug.com/443956. Need to provide right LoadType.
  LoadType cma_load_type = kLoadTypeMediaSource;
  std::unique_ptr<MediaPipelineProxy> cma_media_pipeline(new MediaPipelineProxy(
      render_frame_id_, content::RenderThread::Get()->GetIOMessageLoopProxy(),
      cma_load_type));
  std::unique_ptr<CmaRenderer> cma_renderer(new CmaRenderer(
      std::move(cma_media_pipeline), video_renderer_sink, gpu_factories_));
  return std::move(cma_renderer);
}

}  // namespace media
}  // namespace chromecast
