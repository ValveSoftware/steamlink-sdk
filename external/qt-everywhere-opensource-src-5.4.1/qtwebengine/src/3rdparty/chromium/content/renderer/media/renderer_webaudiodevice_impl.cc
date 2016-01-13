// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_switches.h"
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
    int session_id)
    : params_(params),
      client_callback_(callback),
      session_id_(session_id) {
  DCHECK(client_callback_);
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!output_device_.get());
}

void RendererWebAudioDeviceImpl::start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (output_device_.get())
    return;  // Already started.

  // Assumption: This method is being invoked within a V8 call stack.  CHECKs
  // will fail in the call to frameForCurrentContext() otherwise.
  //
  // Therefore, we can perform look-ups to determine which RenderView is
  // starting the audio device.  The reason for all this is because the creator
  // of the WebAudio objects might not be the actual source of the audio (e.g.,
  // an extension creates a object that is passed and used within a page).
  WebLocalFrame* const web_frame = WebLocalFrame::frameForCurrentContext();
  WebView* const web_view = web_frame ? web_frame->view() : NULL;
  RenderFrame* const render_frame =
      web_frame ? RenderFrame::FromWebFrame(web_frame) : NULL;
  RenderViewImpl* const render_view =
      web_view ? RenderViewImpl::FromWebView(web_view) : NULL;
  output_device_ = AudioDeviceFactory::NewOutputDevice(
      render_view ? render_view->routing_id() : MSG_ROUTING_NONE,
      render_frame ? render_frame->GetRoutingID(): MSG_ROUTING_NONE);
  output_device_->InitializeWithSessionId(params_, this, session_id_);
  output_device_->Start();
  // Note: Default behavior is to auto-play on start.
}

void RendererWebAudioDeviceImpl::stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (output_device_.get()) {
    output_device_->Stop();
    output_device_ = NULL;
  }
}

double RendererWebAudioDeviceImpl::sampleRate() {
  return params_.sample_rate();
}

int RendererWebAudioDeviceImpl::Render(media::AudioBus* dest,
                                       int audio_delay_milliseconds) {
  if (client_callback_) {
    // Wrap the output pointers using WebVector.
    WebVector<float*> web_audio_dest_data(
        static_cast<size_t>(dest->channels()));
    for (int i = 0; i < dest->channels(); ++i)
      web_audio_dest_data[i] = dest->channel(i);

    // TODO(xians): Remove the following |web_audio_source_data| after
    // changing the blink interface.
    WebVector<float*> web_audio_source_data(static_cast<size_t>(0));
    client_callback_->render(web_audio_source_data,
                             web_audio_dest_data,
                             dest->frames());
  }

  return dest->frames();
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}

}  // namespace content
