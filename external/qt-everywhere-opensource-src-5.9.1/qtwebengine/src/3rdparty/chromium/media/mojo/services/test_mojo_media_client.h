// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager.h"
#include "media/mojo/services/mojo_media_client.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioRendererSink;
class MediaLog;
class RendererFactory;
class VideoRendererSink;

// Default MojoMediaClient for MediaService.
class TestMojoMediaClient : public MojoMediaClient {
 public:
  TestMojoMediaClient();
  ~TestMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize() final;
  scoped_refptr<AudioRendererSink> CreateAudioRendererSink(
      const std::string& audio_device_id) final;
  std::unique_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) final;
  std::unique_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* /* interface_provider */)
      final;

 private:
  ScopedAudioManagerPtr audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
