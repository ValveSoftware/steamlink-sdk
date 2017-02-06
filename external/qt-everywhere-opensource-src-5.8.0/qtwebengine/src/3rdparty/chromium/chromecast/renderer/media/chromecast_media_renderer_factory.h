// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_CHROMECAST_MEDIA_RENDERER_FACTORY_H_
#define CHROMECAST_RENDERER_MEDIA_CHROMECAST_MEDIA_RENDERER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/renderer_factory.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace chromecast {
namespace media {

class ChromecastMediaRendererFactory : public ::media::RendererFactory {
 public:
  ChromecastMediaRendererFactory(
      ::media::GpuVideoAcceleratorFactories* gpu_factories,
      int render_frame_id);
  ~ChromecastMediaRendererFactory() final;

  // ::media::RendererFactory implementation.
  std::unique_ptr<::media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      ::media::AudioRendererSink* audio_renderer_sink,
      ::media::VideoRendererSink* video_renderer_sink,
      const ::media::RequestSurfaceCB& request_surface_cb) final;

 private:
  const int render_frame_id_;
  ::media::GpuVideoAcceleratorFactories* const gpu_factories_;

  DISALLOW_COPY_AND_ASSIGN(ChromecastMediaRendererFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_CHROMECAST_MEDIA_RENDERER_FACTORY_H_
