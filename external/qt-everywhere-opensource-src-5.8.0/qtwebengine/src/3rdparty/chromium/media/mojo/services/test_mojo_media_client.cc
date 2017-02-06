// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/test_mojo_media_client.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/cdm_factory.h"
#include "media/base/media.h"
#include "media/base/null_video_sink.h"
#include "media/base/renderer_factory.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace media {

TestMojoMediaClient::TestMojoMediaClient() {}

TestMojoMediaClient::~TestMojoMediaClient() {}

void TestMojoMediaClient::Initialize() {
  InitializeMediaLibrary();
  // TODO(dalecurtis): We should find a single owner per process for the audio
  // manager or make it a lazy instance.  It's not safe to call Get()/Create()
  // across multiple threads...
  AudioManager* audio_manager = AudioManager::Get();
  if (!audio_manager) {
    audio_manager_ = media::AudioManager::CreateForTesting(
        base::ThreadTaskRunnerHandle::Get());
    audio_manager = audio_manager_.get();
    // Flush the message loop to ensure that the audio manager is initialized.
    base::RunLoop().RunUntilIdle();
  }

}

void TestMojoMediaClient::WillQuit() {
  DVLOG(1) << __FUNCTION__;
  // AudioManager destructor requires MessageLoop.
  // Destroy it before the message loop goes away.
  audio_manager_.reset();
  // Flush the message loop to ensure that the audio manager is destroyed.
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<RendererFactory> TestMojoMediaClient::CreateRendererFactory(
    const scoped_refptr<MediaLog>& media_log) {
  DVLOG(1) << __FUNCTION__;
  return base::WrapUnique(new DefaultRendererFactory(
      media_log, nullptr, DefaultRendererFactory::GetGpuFactoriesCB()));
}

AudioRendererSink* TestMojoMediaClient::CreateAudioRendererSink() {
  if (!audio_renderer_sink_)
    audio_renderer_sink_ = new AudioOutputStreamSink();

  return audio_renderer_sink_.get();
}

VideoRendererSink* TestMojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  if (!video_renderer_sink_) {
    video_renderer_sink_ = base::WrapUnique(
        new NullVideoSink(false, base::TimeDelta::FromSecondsD(1.0 / 60),
                          NullVideoSink::NewFrameCB(), task_runner));
  }

  return video_renderer_sink_.get();
}

std::unique_ptr<CdmFactory> TestMojoMediaClient::CreateCdmFactory(
    shell::mojom::InterfaceProvider* /* interface_provider */) {
  DVLOG(1) << __FUNCTION__;
  return base::WrapUnique(new DefaultCdmFactory());
}

}  // namespace media
