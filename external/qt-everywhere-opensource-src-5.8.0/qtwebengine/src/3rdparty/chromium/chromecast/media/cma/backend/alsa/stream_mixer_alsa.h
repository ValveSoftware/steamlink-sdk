// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_H_

#include <alsa/asoundlib.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "chromecast/media/cma/backend/alsa/audio_filter_interface.h"
#include "chromecast/media/cma/backend/alsa/media_pipeline_backend_alsa.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input.h"
#include "chromecast/public/cast_media_shlib.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {
class AlsaWrapper;
class StreamMixerAlsaInputImpl;

// Mixer implementation. The mixer has one or more input queues; these can be
// added/removed at any time. When an input source pushes frames to an input
// queue, the queue should call StreamMixerAlsa::WriteFrames(); this causes
// the mixer to attempt to mix and write out as many frames as possible. To do
// this, the mixer determines how many frames can be read from all inputs (ie,
// it gets the maximum number of frames that can be read from each input, and
// uses the minimum value). Assuming that all primary inputs have some data
// available, the calculated number of frames are pulled from each input (maybe
// resampled, if the input's incoming sample rate is not equal to the mixer's
// output sample rate) and written to the ALSA stack.
//
// The rendering delay is recalculated after every successful write to the ALSA
// stack. This delay is passed up to the input sources whenever some new data
// is sucessfully added to the input queue (which happens whenever the amount
// of data in the queue drops below the maximum limit, if data is pending). Note
// that the rendering delay is not accurate while the mixer is gathering frames
// to write, so the rendering delay and the queue size for each input must be
// updated atomically after each write is complete (ie, in AfterWriteFrames()).
class StreamMixerAlsa {
 public:
  // This mixer will pull data from InputQueues which are added to it, mix the
  // data from the streams, and write the mixed data as a single stream to ALSA.
  // These methods will be called on the mixer thread only.
  class InputQueue {
   public:
    using OnReadyToDeleteCb = base::Callback<void(InputQueue*)>;

    virtual ~InputQueue() {}

    // Returns the sample rate of this stream *before* data is resampled to
    // match the sample rate expected by the mixer. The returned value must be
    // positive.
    virtual int input_samples_per_second() const = 0;

    // This number will be used to scale the stream before it is mixed. The
    // result must be in the range (0.0, 1.0].
    virtual float volume_multiplier() const = 0;

    // Returns true if the stream is primary. Primary streams will be given
    // precedence for sample rates and will dictate when data is polled.
    virtual bool primary() const = 0;

    // Returns true if PrepareToDelete() has been called.
    virtual bool IsDeleting() const = 0;

    // Initializes the InputQueue after the mixer is set up. At this point the
    // input can correctly determine the mixer's output sample rate.
    virtual void Initialize(const MediaPipelineBackendAlsa::RenderingDelay&
                                mixer_rendering_delay) = 0;

    // Returns the maximum number of frames that can be read from this input
    // stream without filling with zeros. This should return 0 if the queue is
    // empty and EOS has not been queued.
    virtual int MaxReadSize() = 0;

    // Pulls data from the input stream. The input stream should populate |dest|
    // with |frames| frames of data to be mixed. The mixer expects data to be
    // at a sample rate of |output_samples_per_second()|, so each input stream
    // should resample as necessary before returning. |frames| is guaranteed to
    // be no larger than the value returned by the most recent call to
    // MaxReadSize(), and |dest->frames()| shall be >= |frames|.
    virtual void GetResampledData(::media::AudioBus* dest, int frames) = 0;

    // This is called for every InputQueue when the mixer writes data to ALSA
    // for any of its input streams.
    virtual void AfterWriteFrames(
        const MediaPipelineBackendAlsa::RenderingDelay&
            mixer_rendering_delay) = 0;

    // This will be called when a fatal error occurs in the mixer.
    virtual void SignalError(StreamMixerAlsaInput::MixerError error) = 0;

    // Notifies the input that it is being removed by the upper layers, and
    // should do whatever is necessary to become ready to delete from the mixer.
    // Once the input is ready to be removed, it should call the supplied
    // |delete_cb|; this should only happen once per input.
    virtual void PrepareToDelete(const OnReadyToDeleteCb& delete_cb) = 0;
  };

  enum State {
    kStateUninitialized,
    kStateNormalPlayback,
    kStateError,
  };

  static StreamMixerAlsa* Get();
  static void MakeSingleThreadedForTest();

  int output_samples_per_second() const { return output_samples_per_second_; }
  bool empty() const { return inputs_.empty(); }
  State state() const { return state_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return mixer_task_runner_;
  }

  // Adds an input to the mixer. The input will live at least until
  // RemoveInput(input) is called. Can be called on any thread.
  void AddInput(std::unique_ptr<InputQueue> input);
  // Instructs the mixer to remove an input. The input should not be referenced
  // after this is called. Can be called on any thread.
  void RemoveInput(InputQueue* input);

  // Attempts to write some frames of audio to ALSA. Must only be called on the
  // mixer thread.
  void OnFramesQueued();

  void SetAlsaWrapperForTest(std::unique_ptr<AlsaWrapper> alsa_wrapper);
  void WriteFramesForTest();  // Can be called on any thread.
  void ClearInputsForTest();  // Removes all inputs.

  void AddLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

  void RemoveLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

 protected:
  StreamMixerAlsa();
  virtual ~StreamMixerAlsa();

 private:
  void ResetTaskRunnerForTest();
  void FinalizeOnMixerThread();
  void FinishFinalize();

  // Reads the buffer size, period size, start threshold, and avail min value
  // from the provided command line flags or uses default values if no flags are
  // provided.
  void DefineAlsaParameters();

  // Takes the provided ALSA config and sets all ALSA output hardware/software
  // playback parameters.  It will try to select sane fallback parameters based
  // on what the output hardware supports and will log warnings if it does so.
  // If any ALSA function returns an unexpected error code, the error code will
  // be returned by this function. Otherwise, it will return 0.
  int SetAlsaPlaybackParams();
  void Start();
  void Stop();
  void Close();
  void SignalError();
  void CheckChangeOutputRate(int input_samples_per_second);
  unsigned int DetermineOutputRate(unsigned int requested_rate);

  // Deletes an input queue that has finished preparing to delete itself.
  // May be called on any thread.
  void DeleteInputQueue(InputQueue* input);
  // Runs on mixer thread to complete input queue deletion.
  void DeleteInputQueueInternal(InputQueue* input);
  // Called after a timeout period to close the PCM handle if no inputs are
  // present.
  void CheckClose();

  void WriteFrames();
  bool TryWriteFrames();
  void WriteMixedPcm(const ::media::AudioBus& mixed, int frames);
  void UpdateRenderingDelay(int newly_pushed_frames);
  ssize_t BytesPerOutputFormatSample();

  static bool single_threaded_for_test_;

  std::unique_ptr<AlsaWrapper> alsa_;
  std::unique_ptr<base::Thread> mixer_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> mixer_task_runner_;

  unsigned int fixed_output_samples_per_second_;
  unsigned int low_sample_rate_cutoff_;
  int requested_output_samples_per_second_;
  int output_samples_per_second_;
  snd_pcm_t* pcm_;
  snd_pcm_hw_params_t* pcm_hw_params_;
  snd_pcm_status_t* pcm_status_;
  snd_pcm_format_t pcm_format_;

  // User-configurable ALSA parameters. This caches the results, so the code
  // only has to interact with the command line parameters once.
  std::string alsa_device_name_;
  snd_pcm_uframes_t alsa_buffer_size_;
  bool alsa_period_explicitly_set;
  snd_pcm_uframes_t alsa_period_size_;
  snd_pcm_uframes_t alsa_start_threshold_;
  snd_pcm_uframes_t alsa_avail_min_;

  State state_;

  std::vector<std::unique_ptr<InputQueue>> inputs_;
  std::vector<std::unique_ptr<InputQueue>> ignored_inputs_;
  MediaPipelineBackendAlsa::RenderingDelay rendering_delay_;
  // Buffer to write final interleaved data before sending to snd_pcm_writei().
  std::vector<uint8_t> interleaved_;

  // Buffers that hold audio data while it is mixed, before it is passed to the
  // ALSA layer. These are kept as members of this class to minimize copies and
  // allocations.
  std::unique_ptr<::media::AudioBus> temp_;
  std::unique_ptr<::media::AudioBus> mixed_;

  std::unique_ptr<base::Timer> retry_write_frames_timer_;

  int check_close_timeout_;
  std::unique_ptr<base::Timer> check_close_timer_;

  std::vector<CastMediaShlib::LoopbackAudioObserver*> loopback_observers_;

  std::unique_ptr<AudioFilterInterface> pre_loopback_filter_;
  std::unique_ptr<AudioFilterInterface> post_loopback_filter_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsa);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_STREAM_MIXER_ALSA_H_
