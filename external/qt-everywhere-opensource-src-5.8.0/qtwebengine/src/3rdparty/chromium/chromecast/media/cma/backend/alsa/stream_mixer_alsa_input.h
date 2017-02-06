// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_INPUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_INPUT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/backend/alsa/media_pipeline_backend_alsa.h"

namespace chromecast {
namespace media {

class DecoderBufferBase;
class StreamMixerAlsaInputImpl;

// Input handle to the mixer. All methods (including constructor and destructor)
// must be called on the same thread.
class StreamMixerAlsaInput {
 public:
  enum class MixerError {
    // This input is being ignored due to a sample rate changed.
    kInputIgnored,
    // An internal mixer error occurred. The input is no longer usable.
    kInternalError,
  };

  class Delegate {
   public:
    using MixerError = StreamMixerAlsaInput::MixerError;

    // Called when the last data passed to WritePcm() has been successfully
    // added to the queue.
    virtual void OnWritePcmCompletion(
        MediaPipelineBackendAlsa::BufferStatus status,
        const MediaPipelineBackendAlsa::RenderingDelay& delay) = 0;

    // Called when a mixer error occurs. No further data should be written.
    virtual void OnMixerError(MixerError error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Adds a new input to the mixer, creating the mixer if it does not already
  // exist.
  StreamMixerAlsaInput(Delegate* delegate,
                       int samples_per_second,
                       bool primary);
  // Removes this input from the mixer, destroying the mixer if there are no
  // remaining inputs.
  ~StreamMixerAlsaInput();

  // Writes some PCM data to be mixed. |data| must be in planar float format.
  // Once the data has been written, the delegate's OnWritePcmCompletion()
  // method will be called on the same thread that the StreamMixerAlsaInput was
  // created on. Note that no further calls to WritePcm() should be made until
  // OnWritePcmCompletion() has been called.
  void WritePcm(const scoped_refptr<DecoderBufferBase>& data);

  // Pauses/unpauses this input. If the mixer has other unpaused inputs, it
  // will continue to play sound while this input is paused.
  void SetPaused(bool paused);

  // Sets the volume multiplier for this input. If |multiplier| is outside the
  // range [0.0, 1.0], it is clamped to that range.
  void SetVolumeMultiplier(float multiplier);

 private:
  StreamMixerAlsaInputImpl* impl_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsaInput);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_INPUT_H_
