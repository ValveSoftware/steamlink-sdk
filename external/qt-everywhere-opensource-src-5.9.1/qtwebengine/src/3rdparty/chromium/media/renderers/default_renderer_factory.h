// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_DEFAULT_RENDERER_FACTORY_H_
#define MEDIA_RENDERERS_DEFAULT_RENDERER_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "media/base/media_export.h"
#include "media/base/renderer_factory.h"

namespace media {

class AudioDecoder;
class AudioRendererSink;
class DecoderFactory;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoDecoder;
class VideoRendererSink;

// The default factory class for creating RendererImpl.
class MEDIA_EXPORT DefaultRendererFactory : public RendererFactory {
 public:
  using GetGpuFactoriesCB = base::Callback<GpuVideoAcceleratorFactories*()>;

  DefaultRendererFactory(const scoped_refptr<MediaLog>& media_log,
                         DecoderFactory* decoder_factory,
                         const GetGpuFactoriesCB& get_gpu_factories_cb);
  ~DefaultRendererFactory() final;

  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      const RequestSurfaceCB& request_surface_cb) final;

 private:
  ScopedVector<AudioDecoder> CreateAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);
  ScopedVector<VideoDecoder> CreateVideoDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const RequestSurfaceCB& request_surface_cb,
      GpuVideoAcceleratorFactories* gpu_factories);

  scoped_refptr<MediaLog> media_log_;

  // Factory to create extra audio and video decoders.
  // Could be nullptr if not extra decoders are available.
  DecoderFactory* decoder_factory_;

  // Creates factories for supporting video accelerators. May be null.
  GetGpuFactoriesCB get_gpu_factories_cb_;

  DISALLOW_COPY_AND_ASSIGN(DefaultRendererFactory);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_DEFAULT_RENDERER_FACTORY_H_
