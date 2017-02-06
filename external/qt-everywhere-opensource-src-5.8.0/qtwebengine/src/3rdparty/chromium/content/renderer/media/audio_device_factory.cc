// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_device_factory.h"

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/content_constants_internal.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/media_switches.h"
#include "url/origin.h"

namespace content {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = NULL;

namespace {
#if defined(OS_WIN)
// Due to driver deadlock issues on Windows (http://crbug/422522) there is a
// chance device authorization response is never received from the browser side.
// In this case we will time out, to avoid renderer hang forever waiting for
// device authorization (http://crbug/615589). This will result in "no audio".
const int64_t kMaxAuthorizationTimeoutMs = 1000;
#else
const int64_t kMaxAuthorizationTimeoutMs = 0;  // No timeout.
#endif  // defined(OS_WIN)

media::AudioLatency::LatencyType GetSourceLatencyType(
    AudioDeviceFactory::SourceType source) {
  switch (source) {
    case AudioDeviceFactory::kSourceWebAudioInteractive:
      return media::AudioLatency::LATENCY_INTERACTIVE;
    case AudioDeviceFactory::kSourceNone:
    case AudioDeviceFactory::kSourceWebRtc:
    case AudioDeviceFactory::kSourceNonRtcAudioTrack:
    case AudioDeviceFactory::kSourceWebAudioBalanced:
      return media::AudioLatency::LATENCY_RTC;
    case AudioDeviceFactory::kSourceMediaElement:
    case AudioDeviceFactory::kSourceWebAudioPlayback:
      return media::AudioLatency::LATENCY_PLAYBACK;
    case AudioDeviceFactory::kSourceWebAudioExact:
      return media::AudioLatency::LATENCY_EXACT_MS;
  }
  NOTREACHED();
  return media::AudioLatency::LATENCY_INTERACTIVE;
}

scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  AudioMessageFilter* const filter = AudioMessageFilter::Get();
  scoped_refptr<media::AudioOutputDevice> device(new media::AudioOutputDevice(
      filter->CreateAudioOutputIPC(render_frame_id), filter->io_task_runner(),
      session_id, device_id, security_origin,
      // Set authorization request timeout at 80% of renderer hung timeout, but
      // no more than kMaxAuthorizationTimeout.
      base::TimeDelta::FromMilliseconds(std::min(kHungRendererDelayMs * 8 / 10,
                                                 kMaxAuthorizationTimeoutMs))));
  device->RequestDeviceAuthorization();
  return device;
}

// This is where we decide which audio will go to mixers and which one to
// AudioOutpuDevice directly.
bool IsMixable(AudioDeviceFactory::SourceType source_type) {
  if (source_type == AudioDeviceFactory::kSourceMediaElement)
    return true;  // Must ALWAYS go through mixer.

  // Mix everything if experiment is enabled; otherwise mix nothing else.
  return base::FeatureList::IsEnabled(media::kNewAudioRenderingMixingStrategy);
}

scoped_refptr<media::SwitchableAudioRendererSink> NewMixableSink(
    AudioDeviceFactory::SourceType source_type,
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  return scoped_refptr<media::AudioRendererMixerInput>(
      render_thread->GetAudioRendererMixerManager()->CreateInput(
          render_frame_id, session_id, device_id, security_origin,
          GetSourceLatencyType(source_type)));
}

}  // namespace

scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererMixerSink(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  return NewFinalAudioRendererSink(render_frame_id, session_id, device_id,
                                   security_origin);
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererSink(SourceType source_type,
                                         int render_frame_id,
                                         int session_id,
                                         const std::string& device_id,
                                         const url::Origin& security_origin) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> device =
        factory_->CreateAudioRendererSink(source_type, render_frame_id,
                                          session_id, device_id,
                                          security_origin);
    if (device)
      return device;
  }

  if (IsMixable(source_type))
    return NewMixableSink(source_type, render_frame_id, session_id, device_id,
                          security_origin);

  return NewFinalAudioRendererSink(render_frame_id, session_id, device_id,
                                   security_origin);
}

// static
scoped_refptr<media::SwitchableAudioRendererSink>
AudioDeviceFactory::NewSwitchableAudioRendererSink(
    SourceType source_type,
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    scoped_refptr<media::SwitchableAudioRendererSink> sink =
        factory_->CreateSwitchableAudioRendererSink(source_type,
                                                    render_frame_id, session_id,
                                                    device_id, security_origin);
    if (sink)
      return sink;
  }

  if (IsMixable(source_type))
    return NewMixableSink(source_type, render_frame_id, session_id, device_id,
                          security_origin);

  // AudioOutputDevice is not RestartableAudioRendererSink, so we can't return
  // anything for those who wants to create an unmixable sink.
  NOTIMPLEMENTED();
  return nullptr;
}

// static
scoped_refptr<media::AudioCapturerSource>
AudioDeviceFactory::NewAudioCapturerSource(int render_frame_id) {
  if (factory_) {
    scoped_refptr<media::AudioCapturerSource> source =
        factory_->CreateAudioCapturerSource(render_frame_id);
    if (source)
      return source;
  }

  AudioInputMessageFilter* const filter = AudioInputMessageFilter::Get();
  return new media::AudioInputDevice(
      filter->CreateAudioInputIPC(render_frame_id), filter->io_task_runner());
}

// static
media::OutputDeviceInfo AudioDeviceFactory::GetOutputDeviceInfo(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread) << "RenderThreadImpl is not instantiated, or "
                        << "GetOutputDeviceInfo() is called on a wrong thread ";
  return render_thread->GetAudioRendererMixerManager()->GetOutputDeviceInfo(
      render_frame_id, session_id, device_id, security_origin);
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = NULL;
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewFinalAudioRendererSink(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> sink =
        factory_->CreateFinalAudioRendererSink(render_frame_id, session_id,
                                               device_id, security_origin);
    if (sink)
      return sink;
  }

  return NewOutputDevice(render_frame_id, session_id, device_id,
                         security_origin);
}

}  // namespace content
