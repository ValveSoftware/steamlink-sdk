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

namespace media {

class AudioHardwareConfig;
class AudioRendererSink;
class VideoRendererSink;

// Default MojoMediaClient for MojoMediaApplication.
class TestMojoMediaClient : public MojoMediaClient {
 public:
  TestMojoMediaClient();
  ~TestMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize() final;
  void WillQuit() final;
  std::unique_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log) final;
  AudioRendererSink* CreateAudioRendererSink() final;
  VideoRendererSink* CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      shell::mojom::InterfaceProvider* /* interface_provider */) final;

 private:
  ScopedAudioManagerPtr audio_manager_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  std::unique_ptr<VideoRendererSink> video_renderer_sink_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
