// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/silent_sink_suspender.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebAudioDevice;
using blink::WebLocalFrame;
using blink::WebVector;
using blink::WebView;

namespace content {

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const media::AudioParameters& params,
    WebAudioDevice::RenderCallback* callback,
    int session_id,
    const url::Origin& security_origin)
    : params_(params),
      client_callback_(callback),
      session_id_(session_id),
      security_origin_(security_origin) {
  DCHECK(client_callback_);
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!sink_);
}

void RendererWebAudioDeviceImpl::start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sink_)
    return;  // Already started.

  // Assumption: This method is being invoked within a V8 call stack.  CHECKs
  // will fail in the call to frameForCurrentContext() otherwise.
  //
  // Therefore, we can perform look-ups to determine which RenderView is
  // starting the audio device.  The reason for all this is because the creator
  // of the WebAudio objects might not be the actual source of the audio (e.g.,
  // an extension creates a object that is passed and used within a page).
  WebLocalFrame* const web_frame = WebLocalFrame::frameForCurrentContext();
  RenderFrame* const render_frame =
      web_frame ? RenderFrame::FromWebFrame(web_frame) : NULL;
  sink_ = AudioDeviceFactory::NewAudioRendererSink(
      AudioDeviceFactory::kSourceWebAudioInteractive,
      render_frame ? render_frame->GetRoutingID() : MSG_ROUTING_NONE,
      session_id_, std::string(), security_origin_);

  // Specify the latency info to be passed to the browser side.
  media::AudioParameters sink_params(params_);
  sink_params.set_latency_tag(AudioDeviceFactory::GetSourceLatencyType(
      AudioDeviceFactory::kSourceWebAudioInteractive));

#if defined(OS_ANDROID)
  // Use the media thread instead of the render thread for fake Render() calls
  // since it has special connotations for Blink and garbage collection. Timeout
  // value chosen to be highly unlikely in the normal case.
  webaudio_suspender_.reset(new media::SilentSinkSuspender(
      this, base::TimeDelta::FromSeconds(30), sink_params, sink_,
      RenderThreadImpl::current()->GetMediaThreadTaskRunner()));
  sink_->Initialize(sink_params, webaudio_suspender_.get());
#else
  sink_->Initialize(sink_params, this);
#endif

  sink_->Start();
  sink_->Play();
}

void RendererWebAudioDeviceImpl::stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sink_) {
    sink_->Stop();
    sink_ = nullptr;
  }

#if defined(OS_ANDROID)
  webaudio_suspender_.reset();
#endif
}

double RendererWebAudioDeviceImpl::sampleRate() {
  return params_.sample_rate();
}

int RendererWebAudioDeviceImpl::Render(media::AudioBus* dest,
                                       uint32_t frames_delayed,
                                       uint32_t frames_skipped) {
  // Wrap the output pointers using WebVector.
  WebVector<float*> web_audio_dest_data(static_cast<size_t>(dest->channels()));
  for (int i = 0; i < dest->channels(); ++i)
    web_audio_dest_data[i] = dest->channel(i);

  // TODO(xians): Remove the following |web_audio_source_data| after
  // changing the blink interface.
  WebVector<float*> web_audio_source_data(static_cast<size_t>(0));
  client_callback_->render(web_audio_source_data, web_audio_dest_data,
                           dest->frames());
  return dest->frames();
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}

}  // namespace content
