// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include <stdint.h>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "url/origin.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class NullAudioSink;
}

namespace content {

class RendererWebAudioDeviceImpl
    : public blink::WebAudioDevice,
      public media::AudioRendererSink::RenderCallback {
 public:
  RendererWebAudioDeviceImpl(const media::AudioParameters& params,
                             blink::WebAudioDevice::RenderCallback* callback,
                             int session_id,
                             const url::Origin& security_origin);
  ~RendererWebAudioDeviceImpl() override;

  // blink::WebAudioDevice implementation.
  void start() override;
  void stop() override;
  double sampleRate() override;

  // AudioRendererSink::RenderCallback implementation.
  int Render(media::AudioBus* dest,
             uint32_t frames_delayed,
             uint32_t frames_skipped) override;

  void OnRenderError() override;

 private:
  const media::AudioParameters params_;

  // Weak reference to the callback into WebKit code.
  blink::WebAudioDevice::RenderCallback* const client_callback_;

  // To avoid the need for locking, ensure the control methods of the
  // blink::WebAudioDevice implementation are called on the same thread.
  base::ThreadChecker thread_checker_;

  // When non-NULL, we are started.  When NULL, we are stopped.
  scoped_refptr<media::AudioRendererSink> sink_;

  // ID to allow browser to select the correct input device for unified IO.
  int session_id_;

  // Timeticks when the silence starts.
  base::TimeTicks first_silence_time_ ;

  // TaskRunner to post callbacks to the render thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // A fake audio sink object that consumes data when long period of silence
  // audio is detected. This object lives on the render thread.
  scoped_refptr<media::NullAudioSink> null_audio_sink_;

  // Whether audio output is directed to |null_audio_sink_|.
  bool is_using_null_audio_sink_;

  // First audio buffer after silence finishes. We store this buffer so that
  // it can be sent to the |output_device_| later after switching from
  // |null_audio_sink_|.
  std::unique_ptr<media::AudioBus> first_buffer_after_silence_;

  bool is_first_buffer_after_silence_;

  // A cancelable task that is posted to start the |null_audio_sink_| after a
  // period of silence. We do this on android to save battery consumption.
  base::CancelableClosure start_null_audio_sink_callback_;

  // Security origin, used to check permissions for |output_device_|.
  url::Origin security_origin_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
