// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace media {
class AudioOutputDevice;
}

namespace content {

class RendererWebAudioDeviceImpl
    : public blink::WebAudioDevice,
      public media::AudioRendererSink::RenderCallback {
 public:
  RendererWebAudioDeviceImpl(const media::AudioParameters& params,
                             blink::WebAudioDevice::RenderCallback* callback,
                             int session_id);
  virtual ~RendererWebAudioDeviceImpl();

  // blink::WebAudioDevice implementation.
  virtual void start();
  virtual void stop();
  virtual double sampleRate();

  // AudioRendererSink::RenderCallback implementation.
  virtual int Render(media::AudioBus* dest,
                     int audio_delay_milliseconds) OVERRIDE;

  virtual void OnRenderError() OVERRIDE;

 private:
  const media::AudioParameters params_;

  // Weak reference to the callback into WebKit code.
  blink::WebAudioDevice::RenderCallback* const client_callback_;

  // To avoid the need for locking, ensure the control methods of the
  // blink::WebAudioDevice implementation are called on the same thread.
  base::ThreadChecker thread_checker_;

  // When non-NULL, we are started.  When NULL, we are stopped.
  scoped_refptr<media::AudioOutputDevice> output_device_;

  // ID to allow browser to select the correct input device for unified IO.
  int session_id_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
