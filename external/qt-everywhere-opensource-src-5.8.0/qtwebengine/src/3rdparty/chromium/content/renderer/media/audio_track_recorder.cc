// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_track_recorder.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/opus/src/include/opus.h"

// Note that this code follows the Chrome media convention of defining a "frame"
// as "one multi-channel sample" as opposed to another common definition meaning
// "a chunk of samples". Here this second definition of "frame" is called a
// "buffer"; so what might be called "frame duration" is instead "buffer
// duration", and so on.

namespace content {

namespace {

enum : int {
  // Recommended value for opus_encode_float(), according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints, and is independent of the
  // duration of the encoded buffer.
  kOpusMaxDataBytes = 4000,

  // Opus preferred sampling rate for encoding. This is also the one WebM likes
  // to have: https://wiki.xiph.org/MatroskaOpus.
  kOpusPreferredSamplingRate = 48000,

  // For quality reasons we try to encode 60ms, the maximum Opus buffer.
  kOpusPreferredBufferDurationMs = 60,

  // Maximum amount of buffers that can be held in the AudioEncoders' AudioFifo.
  // Recording is not real time, hence a certain buffering is allowed.
  kMaxNumberOfFifoBuffers = 2,
};

// The amount of Frames in a 60 ms buffer @ 48000 samples/second.
const int kOpusPreferredFramesPerBuffer = kOpusPreferredSamplingRate *
                                          kOpusPreferredBufferDurationMs /
                                          base::Time::kMillisecondsPerSecond;

// Tries to encode |data_in|'s |num_samples| into |data_out|.
bool DoEncode(OpusEncoder* opus_encoder,
              float* data_in,
              int num_samples,
              std::string* data_out) {
  DCHECK_EQ(kOpusPreferredFramesPerBuffer, num_samples);

  data_out->resize(kOpusMaxDataBytes);
  const opus_int32 result = opus_encode_float(
      opus_encoder, data_in, num_samples,
      reinterpret_cast<uint8_t*>(string_as_array(data_out)), kOpusMaxDataBytes);

  if (result > 1) {
    // TODO(ajose): Investigate improving this. http://crbug.com/547918
    data_out->resize(result);
    return true;
  }
  // If |result| in {0,1}, do nothing; the documentation says that a return
  // value of zero or one means the packet does not need to be transmitted.
  // Otherwise, we have an error.
  DLOG_IF(ERROR, result < 0) << " encode failed: " << opus_strerror(result);
  return false;
}

}  // anonymous namespace

// Nested class encapsulating opus-related encoding details. It contains an
// AudioConverter to adapt incoming data to the format Opus likes to have.
// AudioEncoder is created and destroyed on ATR's main thread (usually the main
// render thread) but otherwise should operate entirely on |encoder_thread_|,
// which is owned by AudioTrackRecorder. Be sure to delete |encoder_thread_|
// before deleting the AudioEncoder using it.
class AudioTrackRecorder::AudioEncoder
    : public base::RefCountedThreadSafe<AudioEncoder>,
      public media::AudioConverter::InputCallback {
 public:
  AudioEncoder(const OnEncodedAudioCB& on_encoded_audio_cb,
               int32_t bits_per_second);

  void OnSetFormat(const media::AudioParameters& params);

  void EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                   const base::TimeTicks& capture_time);

  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class base::RefCountedThreadSafe<AudioEncoder>;
  ~AudioEncoder() override;

  bool is_initialized() const { return !!opus_encoder_; }

  // media::AudioConverted::InputCallback implementation.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override;

  void DestroyExistingOpusEncoder();

  const OnEncodedAudioCB on_encoded_audio_cb_;

  // Target bitrate for Opus. If 0, Opus provide automatic bitrate is used.
  const int32_t bits_per_second_;

  base::ThreadChecker encoder_thread_checker_;

  // Track Audio (ingress) and Opus encoder input parameters, respectively. They
  // only differ in their sample_rate() and frames_per_buffer(): output is
  // 48ksamples/s and 2880, respectively.
  media::AudioParameters input_params_;
  media::AudioParameters output_params_;

  // Sampling rate adapter between an OpusEncoder supported and the provided.
  std::unique_ptr<media::AudioConverter> converter_;
  std::unique_ptr<media::AudioFifo> fifo_;

  // Buffer for passing AudioBus data to OpusEncoder.
  std::unique_ptr<float[]> buffer_;

  // While |paused_|, AudioBuses are not encoded.
  bool paused_;

  OpusEncoder* opus_encoder_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

AudioTrackRecorder::AudioEncoder::AudioEncoder(
    const OnEncodedAudioCB& on_encoded_audio_cb,
    int32_t bits_per_second)
    : on_encoded_audio_cb_(on_encoded_audio_cb),
      bits_per_second_(bits_per_second),
      paused_(false),
      opus_encoder_(nullptr) {
  // AudioEncoder is constructed on the thread that ATR lives on, but should
  // operate only on the encoder thread after that. Reset
  // |encoder_thread_checker_| here, as the next call to CalledOnValidThread()
  // will be from the encoder thread.
  encoder_thread_checker_.DetachFromThread();
}

AudioTrackRecorder::AudioEncoder::~AudioEncoder() {
  // We don't DCHECK that we're on the encoder thread here, as it should have
  // already been deleted at this point.
  DestroyExistingOpusEncoder();
}

void AudioTrackRecorder::AudioEncoder::OnSetFormat(
    const media::AudioParameters& input_params) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(encoder_thread_checker_.CalledOnValidThread());
  if (input_params_.Equals(input_params))
    return;

  DestroyExistingOpusEncoder();

  if (!input_params.IsValid()) {
    DLOG(ERROR) << "Invalid params: " << input_params.AsHumanReadableString();
    return;
  }
  input_params_ = input_params;
  input_params_.set_frames_per_buffer(input_params_.sample_rate() *
                                      kOpusPreferredBufferDurationMs /
                                      base::Time::kMillisecondsPerSecond);

  // third_party/libopus supports up to 2 channels (see implementation of
  // opus_encoder_create()): force |output_params_| to at most those.
  output_params_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(std::min(input_params_.channels(), 2)),
      kOpusPreferredSamplingRate,
      input_params_.bits_per_sample(),
      kOpusPreferredFramesPerBuffer);
  DVLOG(1) << "|input_params_|:" << input_params_.AsHumanReadableString()
           << " -->|output_params_|:" << output_params_.AsHumanReadableString();

  converter_.reset(new media::AudioConverter(input_params_, output_params_,
                                             false /* disable_fifo */));
  converter_->AddInput(this);
  converter_->PrimeWithSilence();

  fifo_.reset(new media::AudioFifo(
      input_params_.channels(),
      kMaxNumberOfFifoBuffers * input_params_.frames_per_buffer()));

  buffer_.reset(new float[output_params_.channels() *
                          output_params_.frames_per_buffer()]);

  // Initialize OpusEncoder.
  int opus_result;
  opus_encoder_ = opus_encoder_create(output_params_.sample_rate(),
                                      output_params_.channels(),
                                      OPUS_APPLICATION_AUDIO,
                                      &opus_result);
  if (opus_result < 0) {
    DLOG(ERROR) << "Couldn't init opus encoder: " << opus_strerror(opus_result)
                << ", sample rate: " << output_params_.sample_rate()
                << ", channels: " << output_params_.channels();
    return;
  }

  // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
  // variable bitrate up to 102kbps for 2-channel, 48 kHz audio and a 10 ms
  // buffer duration. The opus library authors may, of course, adjust this in
  // later versions.
  const opus_int32 bitrate =
      (bits_per_second_ > 0) ? bits_per_second_ : OPUS_AUTO;
  if (opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
    DLOG(ERROR) << "Failed to set opus bitrate: " << bitrate;
    return;
  }
}

void AudioTrackRecorder::AudioEncoder::EncodeAudio(
    std::unique_ptr<media::AudioBus> input_bus,
    const base::TimeTicks& capture_time) {
  DVLOG(3) << __FUNCTION__ << ", #frames " << input_bus->frames();
  DCHECK(encoder_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(input_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());
  DCHECK(converter_);

  if (!is_initialized() || paused_)
    return;
  // TODO(mcasas): Consider using a std::deque<std::unique_ptr<AudioBus>>
  // instead of
  // an AudioFifo, to avoid copying data needlessly since we know the sizes of
  // both input and output and they are multiples.
  fifo_->Push(input_bus.get());

  // Wait to have enough |input_bus|s to guarantee a satisfactory conversion.
  while (fifo_->frames() >= input_params_.frames_per_buffer()) {
    std::unique_ptr<media::AudioBus> audio_bus = media::AudioBus::Create(
        output_params_.channels(), kOpusPreferredFramesPerBuffer);
    converter_->Convert(audio_bus.get());
    audio_bus->ToInterleaved<media::Float32SampleTypeTraits>(
        audio_bus->frames(), buffer_.get());

    std::unique_ptr<std::string> encoded_data(new std::string());
    if (DoEncode(opus_encoder_, buffer_.get(), kOpusPreferredFramesPerBuffer,
                 encoded_data.get())) {
      const base::TimeTicks capture_time_of_first_sample =
          capture_time -
          base::TimeDelta::FromMicroseconds(fifo_->frames() *
                                            base::Time::kMicrosecondsPerSecond /
                                            input_params_.sample_rate());
      on_encoded_audio_cb_.Run(output_params_, std::move(encoded_data),
                               capture_time_of_first_sample);
    }
  }
}

double AudioTrackRecorder::AudioEncoder::ProvideInput(
    media::AudioBus* audio_bus,
    uint32_t frames_delayed) {
  fifo_->Consume(audio_bus, 0, audio_bus->frames());
  return 1.0;  // Return volume greater than zero to indicate we have more data.
}

void AudioTrackRecorder::AudioEncoder::DestroyExistingOpusEncoder() {
  // We don't DCHECK that we're on the encoder thread here, as this could be
  // called from the dtor (main thread) or from OnSetFormat() (encoder thread).
  if (opus_encoder_) {
    opus_encoder_destroy(opus_encoder_);
    opus_encoder_ = nullptr;
  }
}

AudioTrackRecorder::AudioTrackRecorder(
    const blink::WebMediaStreamTrack& track,
    const OnEncodedAudioCB& on_encoded_audio_cb,
    int32_t bits_per_second)
    : track_(track),
      encoder_(new AudioEncoder(media::BindToCurrentLoop(on_encoded_audio_cb),
                                bits_per_second)),
      encoder_thread_("AudioEncoderThread") {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!track_.isNull());
  DCHECK(MediaStreamAudioTrack::From(track_));

  // Start the |encoder_thread_|. From this point on, |encoder_| should work
  // only on |encoder_thread_|, as enforced by DCHECKs.
  DCHECK(!encoder_thread_.IsRunning());
  encoder_thread_.Start();

  // Connect the source provider to the track as a sink.
  MediaStreamAudioSink::AddToAudioTrack(this, track_);
}

AudioTrackRecorder::~AudioTrackRecorder() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  MediaStreamAudioSink::RemoveFromAudioTrack(this, track_);
}

void AudioTrackRecorder::OnSetFormat(const media::AudioParameters& params) {
  DCHECK(encoder_thread_.IsRunning());
  // If the source is restarted, might have changed to another capture thread.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&AudioEncoder::OnSetFormat, encoder_, params));
}

void AudioTrackRecorder::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks capture_time) {
  DCHECK(encoder_thread_.IsRunning());
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(!capture_time.is_null());

  std::unique_ptr<media::AudioBus> audio_data =
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
  audio_bus.CopyTo(audio_data.get());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&AudioEncoder::EncodeAudio, encoder_,
                            base::Passed(&audio_data), capture_time));
}

void AudioTrackRecorder::Pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(encoder_);
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&AudioEncoder::set_paused, encoder_, true));
}

void AudioTrackRecorder::Resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(encoder_);
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&AudioEncoder::set_paused, encoder_, false));
}

}  // namespace content
