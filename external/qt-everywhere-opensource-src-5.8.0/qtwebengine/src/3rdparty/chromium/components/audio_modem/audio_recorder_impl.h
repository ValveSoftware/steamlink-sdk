// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_AUDIO_RECORDER_IMPL_H_
#define COMPONENTS_AUDIO_MODEM_AUDIO_RECORDER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/audio_modem/audio_recorder.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace base {
class MessageLoop;
}

namespace media {
class AudioBus;
}

namespace audio_modem {

// The AudioRecorder class will record audio until told to stop.
class AudioRecorderImpl final
    : public AudioRecorder,
      public media::AudioInputStream::AudioInputCallback {
 public:
  using RecordedSamplesCallback = base::Callback<void(const std::string&)>;

  AudioRecorderImpl();

  // AudioRecorder overrides:
  void Initialize(const RecordedSamplesCallback& decode_callback) override;
  void Record() override;
  void Stop() override;
  void Finalize() override;

  // Takes ownership of the stream.
  void set_input_stream_for_testing(
      media::AudioInputStream* input_stream_for_testing) {
    input_stream_for_testing_.reset(input_stream_for_testing);
  }

  // Takes ownership of the params.
  void set_params_for_testing(media::AudioParameters* params_for_testing) {
    params_for_testing_.reset(params_for_testing);
  }

 protected:
  ~AudioRecorderImpl() override;
  void set_is_recording(bool is_recording) { is_recording_ = is_recording; }

 private:
  friend class AudioRecorderTest;
  FRIEND_TEST_ALL_PREFIXES(AudioRecorderTest, BasicRecordAndStop);
  FRIEND_TEST_ALL_PREFIXES(AudioRecorderTest, OutOfOrderRecordAndStopMultiple);

  // Methods to do our various operations; all of these need to be run on the
  // audio thread.
  void InitializeOnAudioThread();
  void RecordOnAudioThread();
  void StopOnAudioThread();
  void StopAndCloseOnAudioThread();
  void FinalizeOnAudioThread();

  // AudioInputStream::AudioInputCallback overrides:
  // Called by the audio recorder when a full packet of audio data is
  // available. This is called from a special audio thread and the
  // implementation should return as soon as possible.
  void OnData(media::AudioInputStream* stream,
              const media::AudioBus* source,
              uint32_t hardware_delay_bytes,
              double volume) override;
  void OnError(media::AudioInputStream* stream) override;

  bool is_recording_;

  media::AudioInputStream* stream_;

  RecordedSamplesCallback decode_callback_;

  // Outside of the ctor/Initialize method, only access the next variables on
  // the recording thread.
  std::unique_ptr<media::AudioBus> buffer_;
  int total_buffer_frames_;
  int buffer_frame_index_;

  std::unique_ptr<media::AudioInputStream> input_stream_for_testing_;
  std::unique_ptr<media::AudioParameters> params_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(AudioRecorderImpl);
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_AUDIO_RECORDER_IMPL_H_
