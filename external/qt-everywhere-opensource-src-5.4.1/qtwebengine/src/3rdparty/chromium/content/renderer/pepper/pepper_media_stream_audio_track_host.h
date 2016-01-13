// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_

#include <deque>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"
#include "media/audio/audio_parameters.h"
#include "ppapi/shared_impl/media_stream_audio_track_shared.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace content {

class PepperMediaStreamAudioTrackHost : public PepperMediaStreamTrackHostBase {
 public:
  PepperMediaStreamAudioTrackHost(RendererPpapiHost* host,
                                  PP_Instance instance,
                                  PP_Resource resource,
                                  const blink::WebMediaStreamTrack& track);

 private:
  // A helper class for receiving audio samples in the audio thread.
  // This class is created and destroyed on the renderer main thread.
  class AudioSink : public MediaStreamAudioSink {
   public:
    explicit AudioSink(PepperMediaStreamAudioTrackHost* host);
    virtual ~AudioSink();

    // Enqueues a free buffer index into |buffers_| which will be used for
    // sending audio samples to plugin.
    // This function is called on the main thread.
    void EnqueueBuffer(int32_t index);

    // This function is called on the main thread.
    void Configure(int32_t number_of_buffers);

   private:
    // Initializes buffers on the main thread.
    void SetFormatOnMainThread(int bytes_per_second);

    void InitBuffers();

    // Send enqueue buffer message on the main thread.
    void SendEnqueueBufferMessageOnMainThread(int32_t index);

    // MediaStreamAudioSink overrides:
    // These two functions should be called on the audio thread.
    virtual void OnData(const int16* audio_data,
                        int sample_rate,
                        int number_of_channels,
                        int number_of_frames) OVERRIDE;
    virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE;

    // Unowned host which is available during the AudioSink's lifespan.
    // It is mainly used in the main thread. But the audio thread will use
    // host_->buffer_manager() to read some buffer properties. It is safe
    // because the buffer_manager()'s properties will not be changed after
    // initialization.
    PepperMediaStreamAudioTrackHost* host_;

    // Timestamp of the next received audio buffer.
    // Access only on the audio thread.
    base::TimeDelta timestamp_;

    // Duration of one audio buffer.
    // Access only on the audio thread.
    base::TimeDelta buffer_duration_;

    // The current audio parameters.
    // Access only on the audio thread.
    media::AudioParameters audio_params_;

    // The original audio parameters which is set in the first time of
    // OnSetFormat being called.
    // Access only on the audio thread.
    media::AudioParameters original_audio_params_;

    // The audio data size of one audio buffer in bytes.
    // Access only on the audio thread.
    uint32_t buffer_data_size_;

    // A lock to protect the index queue |buffers_|.
    base::Lock lock_;

    // A queue for free buffer indices.
    std::deque<int32_t> buffers_;

    scoped_refptr<base::MessageLoopProxy> main_message_loop_proxy_;

    base::ThreadChecker audio_thread_checker_;

    base::WeakPtrFactory<AudioSink> weak_factory_;

    // Number of buffers.
    int32_t number_of_buffers_;

    // Number of bytes per second.
    int bytes_per_second_;

    DISALLOW_COPY_AND_ASSIGN(AudioSink);
  };

  virtual ~PepperMediaStreamAudioTrackHost();

  // ResourceMessageHandler overrides:
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // Message handlers:
  int32_t OnHostMsgConfigure(
      ppapi::host::HostMessageContext* context,
      const ppapi::MediaStreamAudioTrackShared::Attributes& attributes);

  // PepperMediaStreamTrackHostBase overrides:
  virtual void OnClose() OVERRIDE;

  // MediaStreamBufferManager::Delegate overrides:
  virtual void OnNewBufferEnqueued() OVERRIDE;

  // ResourceHost overrides:
  virtual void DidConnectPendingHostToResource() OVERRIDE;

  blink::WebMediaStreamTrack track_;

  // True if |audio_sink_| has been added to |blink::WebMediaStreamTrack|
  // as a sink.
  bool connected_;

  AudioSink audio_sink_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaStreamAudioTrackHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_
