// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_SINK_CACHE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_SINK_CACHE_H_

#include <string>

#include "content/common/content_export.h"
#include "media/base/output_device_info.h"

namespace url {
class Origin;
}

namespace media {
class AudioRendererSink;
}

namespace content {

// Caches AudioRendererSink instances, provides them to the clients for usage,
// tracks their used/unused state, reuses them to obtain output device
// information, garbage-collects unused sinks.
// Thread safe.
class CONTENT_EXPORT AudioRendererSinkCache {
 public:
  virtual ~AudioRendererSinkCache() {}

  // Creates default cache, to be used by AudioRendererMixerManager.
  static std::unique_ptr<AudioRendererSinkCache> Create();

  // Returns output device information for a specified sink.
  virtual media::OutputDeviceInfo GetSinkInfo(
      int source_render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin) = 0;

  // Provides a sink for usage. The sink must be returned to the cache by
  // calling ReleaseSink(). The sink must be stopped by the user before
  // deletion, but after releasing it from the cache.
  virtual scoped_refptr<media::AudioRendererSink> GetSink(
      int source_render_frame_id,
      const std::string& device_id,
      const url::Origin& security_origin) = 0;

  // Notifies the cache that the sink is not in use any more. Must be
  // called by the client, so that the cache can garbage-collect the sink
  // reference.
  virtual void ReleaseSink(const media::AudioRendererSink* sink_ptr) = 0;

 protected:
  AudioRendererSinkCache() {}

  DISALLOW_COPY_AND_ASSIGN(AudioRendererSinkCache);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_SINK_CACHE_H_
