// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_INPUT_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_INPUT_IMPL_H_

#include <deque>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chromecast/media/cma/backend/alsa/media_pipeline_backend_alsa.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class AudioBus;
class MultiChannelResampler;
}  // namespace media

namespace chromecast {
namespace media {

// Input queue implementation for StreamMixerAlsa. Each input source pushes
// frames to an instance of StreamMixerAlsaInputImpl; this then signals the
// mixer to pull as much data as possible from all input queues, and mix it and
// write it out to the ALSA stack. The delegate's OnWritePcmCompletion() method
// is called (on the caller thread) whenever data has been successfully added to
// the queue (this may not happen immediately if the queue's maximum size limit
// has been exceeded).
//
// If an input is being resampled, it is conservative about how much data it can
// provide to be written. This is because the resampler buffers some data
// internally, and consumes data from the queue in large chunks. A resampled
// input ignores the data buffered inside the resampler, as well as any data
// that does not fit within a multiple of the resampler chunk size, for the
// purpose of calculating how much data it can provide. Note that this only
// holds true if the input is not in end-of-stream mode.
//
// When an input is paused, it rapidly fades out (to avoid any pops or clicks),
// and then begins feeding silence to the mixer. The "paused" state is
// transparent to the mixer (when paused, this class provides silence when the
// mixer asks for data). On unpause, the sound is rapidly faded in and then
// resumes normal playback. Similarly, the sound is rapidly faded in at the
// start of playback, and faded out when the input is being removed. This
// prevents sound distortions on track skip or seeking.
//
// The rendering delay is recalculated after every successful write to the ALSA
// stack. This delay is passed up to the input sources whenever some new data
// is sucessfully added to the input queue (which happens whenever the amount
// of data in the queue drops below the maximum limit, if data is pending). Note
// that the rendering delay is not accurate while the mixer is gathering frames
// to write, so pending data is not added to an input queue until after a write
// has completed (in AfterWriteFrames()).
//
// This class is constructed on the caller thread, and the StreamMixerAlsaInput
// methods (WritePcm(), SetPaused(), SetVolumeMultiplier(), and
// PreventDelegateCalls()) must be called on the caller thread. These methods
// must not be called after the input is being removed (ie, after
// mixer->RemoveInput() has been called for this input impl). All other methods
// (including the destructor) must be called on the mixer thread.
//
// When an input is removed, the mixer tells the impl that it is about to be
// removed (it is not deleted yet) by calling PrepareToRemove(). The impl then
// fades out any remaining audio data. Once that is done (or if it is not
// possible/necessary) then the impl calls the |delete_cb| to tell the mixer to
// actually delete it.
class StreamMixerAlsaInputImpl : public StreamMixerAlsa::InputQueue {
 public:
  enum State {
    kStateUninitialized,   // No data has been queued yet.
    kStateNormalPlayback,  // Normal playback.
    kStateFadingOut,       // Fading out to a paused state.
    kStatePaused,          // Currently paused.
    kStateGotEos,          // Got the end-of-stream buffer (normal playback).
    kStateFinalFade,       // Fading out to a deleted state.
    kStateDeleted,         // Told the mixer to delete this.
    kStateError,           // A mixer error occurred, this is unusable now.
  };

  StreamMixerAlsaInputImpl(StreamMixerAlsaInput::Delegate* delegate,
                           int input_samples_per_second,
                           bool primary,
                           StreamMixerAlsa* mixer);

  ~StreamMixerAlsaInputImpl() override;

  // Queues some PCM data to be mixed. |data| must be in planar float format.
  void WritePcm(const scoped_refptr<DecoderBufferBase>& data);

  // Sets the pause state of this stream.
  void SetPaused(bool paused);

  // Sets the volume multiplier for this stream. If |multiplier| < 0, sets the
  // volume multiplier to 0. If |multiplier| > 1, sets the volume multiplier
  // to 1.
  void SetVolumeMultiplier(float multiplier);

  // Prevents any further calls to the delegate (ie, called when the delegate
  // is being destroyed).
  void PreventDelegateCalls();

  State state() const { return state_; }

 private:
  // StreamMixerAlsa::InputQueue implementation:
  int input_samples_per_second() const override;
  float volume_multiplier() const override;
  bool primary() const override;
  bool IsDeleting() const override;
  void Initialize(const MediaPipelineBackendAlsa::RenderingDelay&
                      mixer_rendering_delay) override;
  int MaxReadSize() override;
  void GetResampledData(::media::AudioBus* dest, int frames) override;
  void AfterWriteFrames(const MediaPipelineBackendAlsa::RenderingDelay&
                            mixer_rendering_delay) override;
  void SignalError(StreamMixerAlsaInput::MixerError error) override;
  void PrepareToDelete(const OnReadyToDeleteCb& delete_cb) override;

  // Tells the mixer to delete |this|. Makes sure not to call |delete_cb_| more
  // than once for |this|.
  void DeleteThis();
  MediaPipelineBackendAlsa::RenderingDelay QueueData(
      const scoped_refptr<DecoderBufferBase>& data);
  void PostPcmCallback(const MediaPipelineBackendAlsa::RenderingDelay& delay);
  void DidQueueData(bool end_of_stream);
  void ReadCB(int frame_delay, ::media::AudioBus* output);
  void FillFrames(int frame_delay, ::media::AudioBus* output, int frames);
  int NormalFadeFrames();
  void FadeIn(::media::AudioBus* dest, int frames);
  void FadeOut(::media::AudioBus* dest, int frames);
  void PostError(StreamMixerAlsaInput::MixerError error);

  StreamMixerAlsaInput::Delegate* const delegate_;
  const int input_samples_per_second_;
  const bool primary_;
  StreamMixerAlsa* const mixer_;
  const scoped_refptr<base::SingleThreadTaskRunner> mixer_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  State state_;
  float volume_multiplier_;

  base::Lock queue_lock_;  // Lock for the following queue-related members.
  scoped_refptr<DecoderBufferBase> pending_data_;
  std::deque<scoped_refptr<DecoderBufferBase>> queue_;
  int queued_frames_;
  double queued_frames_including_resampler_;
  MediaPipelineBackendAlsa::RenderingDelay mixer_rendering_delay_;
  // End of members that queue_lock_ controls access for.

  int current_buffer_offset_;
  int max_queued_frames_;
  int fade_frames_remaining_;
  int fade_out_frames_total_;
  int zeroed_frames_;  // current count of consecutive 0-filled frames

  OnReadyToDeleteCb delete_cb_;

  std::unique_ptr<::media::MultiChannelResampler> resampler_;

  base::WeakPtr<StreamMixerAlsaInputImpl> weak_this_;
  base::WeakPtrFactory<StreamMixerAlsaInputImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsaInputImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_INPUT_IMPL_H_
