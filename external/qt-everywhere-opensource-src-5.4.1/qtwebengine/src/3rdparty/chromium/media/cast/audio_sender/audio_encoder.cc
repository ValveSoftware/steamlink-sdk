// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_encoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "third_party/opus/src/include/opus.h"

namespace media {
namespace cast {

namespace {

// The fixed number of audio frames per second and, inversely, the duration of
// one frame's worth of samples.
const int kFramesPerSecond = 100;
const int kFrameDurationMillis = 1000 / kFramesPerSecond;  // No remainder!

// Threshold used to decide whether audio being delivered to the encoder is
// coming in too slow with respect to the capture timestamps.
const int kUnderrunThresholdMillis = 3 * kFrameDurationMillis;

}  // namespace


// Base class that handles the common problem of feeding one or more AudioBus'
// data into a buffer and then, once the buffer is full, encoding the signal and
// emitting an EncodedFrame via the FrameEncodedCallback.
//
// Subclasses complete the implementation by handling the actual encoding
// details.
class AudioEncoder::ImplBase
    : public base::RefCountedThreadSafe<AudioEncoder::ImplBase> {
 public:
  ImplBase(const scoped_refptr<CastEnvironment>& cast_environment,
           transport::AudioCodec codec,
           int num_channels,
           int sampling_rate,
           const FrameEncodedCallback& callback)
      : cast_environment_(cast_environment),
        codec_(codec),
        num_channels_(num_channels),
        samples_per_frame_(sampling_rate / kFramesPerSecond),
        callback_(callback),
        cast_initialization_status_(STATUS_AUDIO_UNINITIALIZED),
        buffer_fill_end_(0),
        frame_id_(0),
        frame_rtp_timestamp_(0) {
    // Support for max sampling rate of 48KHz, 2 channels, 100 ms duration.
    const int kMaxSamplesTimesChannelsPerFrame = 48 * 2 * 100;
    if (num_channels_ <= 0 || samples_per_frame_ <= 0 ||
        sampling_rate % kFramesPerSecond != 0 ||
        samples_per_frame_ * num_channels_ > kMaxSamplesTimesChannelsPerFrame) {
      cast_initialization_status_ = STATUS_INVALID_AUDIO_CONFIGURATION;
    }
  }

  CastInitializationStatus InitializationResult() const {
    return cast_initialization_status_;
  }

  void EncodeAudio(scoped_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time) {
    DCHECK_EQ(cast_initialization_status_, STATUS_AUDIO_INITIALIZED);
    DCHECK(!recorded_time.is_null());

    // Determine whether |recorded_time| is consistent with the amount of audio
    // data having been processed in the past.  Resolve the underrun problem by
    // dropping data from the internal buffer and skipping ahead the next
    // frame's RTP timestamp by the estimated number of frames missed.  On the
    // other hand, don't attempt to resolve overruns: A receiver should
    // gracefully deal with an excess of audio data.
    const base::TimeDelta frame_duration =
        base::TimeDelta::FromMilliseconds(kFrameDurationMillis);
    base::TimeDelta buffer_fill_duration =
        buffer_fill_end_ * frame_duration / samples_per_frame_;
    if (!frame_capture_time_.is_null()) {
      const base::TimeDelta amount_ahead_by =
          recorded_time - (frame_capture_time_ + buffer_fill_duration);
      if (amount_ahead_by >
              base::TimeDelta::FromMilliseconds(kUnderrunThresholdMillis)) {
        buffer_fill_end_ = 0;
        buffer_fill_duration = base::TimeDelta();
        const int64 num_frames_missed = amount_ahead_by /
            base::TimeDelta::FromMilliseconds(kFrameDurationMillis);
        frame_rtp_timestamp_ +=
            static_cast<uint32>(num_frames_missed * samples_per_frame_);
        DVLOG(1) << "Skipping RTP timestamp ahead to account for "
                 << num_frames_missed * samples_per_frame_
                 << " samples' worth of underrun.";
      }
    }
    frame_capture_time_ = recorded_time - buffer_fill_duration;

    // Encode all audio in |audio_bus| into zero or more frames.
    int src_pos = 0;
    while (src_pos < audio_bus->frames()) {
      const int num_samples_to_xfer = std::min(
          samples_per_frame_ - buffer_fill_end_, audio_bus->frames() - src_pos);
      DCHECK_EQ(audio_bus->channels(), num_channels_);
      TransferSamplesIntoBuffer(
          audio_bus.get(), src_pos, buffer_fill_end_, num_samples_to_xfer);
      src_pos += num_samples_to_xfer;
      buffer_fill_end_ += num_samples_to_xfer;

      if (buffer_fill_end_ < samples_per_frame_)
        break;

      scoped_ptr<transport::EncodedFrame> audio_frame(
          new transport::EncodedFrame());
      audio_frame->dependency = transport::EncodedFrame::KEY;
      audio_frame->frame_id = frame_id_;
      audio_frame->referenced_frame_id = frame_id_;
      audio_frame->rtp_timestamp = frame_rtp_timestamp_;
      audio_frame->reference_time = frame_capture_time_;

      if (EncodeFromFilledBuffer(&audio_frame->data)) {
        cast_environment_->PostTask(
            CastEnvironment::MAIN,
            FROM_HERE,
            base::Bind(callback_, base::Passed(&audio_frame)));
      }

      // Reset the internal buffer, frame ID, and timestamps for the next frame.
      buffer_fill_end_ = 0;
      ++frame_id_;
      frame_rtp_timestamp_ += samples_per_frame_;
      frame_capture_time_ += frame_duration;
    }
  }

 protected:
  friend class base::RefCountedThreadSafe<ImplBase>;
  virtual ~ImplBase() {}

  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) = 0;
  virtual bool EncodeFromFilledBuffer(std::string* out) = 0;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const transport::AudioCodec codec_;
  const int num_channels_;
  const int samples_per_frame_;
  const FrameEncodedCallback callback_;

  // Subclass' ctor is expected to set this to STATUS_AUDIO_INITIALIZED.
  CastInitializationStatus cast_initialization_status_;

 private:
  // In the case where a call to EncodeAudio() cannot completely fill the
  // buffer, this points to the position at which to populate data in a later
  // call.
  int buffer_fill_end_;

  // A counter used to label EncodedFrames.
  uint32 frame_id_;

  // The RTP timestamp for the next frame of encoded audio.  This is defined as
  // the number of audio samples encoded so far, plus the estimated number of
  // samples that were missed due to data underruns.  A receiver uses this value
  // to detect gaps in the audio signal data being provided.  Per the spec, RTP
  // timestamp values are allowed to overflow and roll around past zero.
  uint32 frame_rtp_timestamp_;

  // The local system time associated with the start of the next frame of
  // encoded audio.  This value is passed on to a receiver as a reference clock
  // timestamp for the purposes of synchronizing audio and video.  Its
  // progression is expected to drift relative to the elapsed time implied by
  // the RTP timestamps.
  base::TimeTicks frame_capture_time_;

  DISALLOW_COPY_AND_ASSIGN(ImplBase);
};

class AudioEncoder::OpusImpl : public AudioEncoder::ImplBase {
 public:
  OpusImpl(const scoped_refptr<CastEnvironment>& cast_environment,
           int num_channels,
           int sampling_rate,
           int bitrate,
           const FrameEncodedCallback& callback)
      : ImplBase(cast_environment,
                 transport::kOpus,
                 num_channels,
                 sampling_rate,
                 callback),
        encoder_memory_(new uint8[opus_encoder_get_size(num_channels)]),
        opus_encoder_(reinterpret_cast<OpusEncoder*>(encoder_memory_.get())),
        buffer_(new float[num_channels * samples_per_frame_]) {
    if (ImplBase::cast_initialization_status_ != STATUS_AUDIO_UNINITIALIZED)
      return;
    if (opus_encoder_init(opus_encoder_,
                          sampling_rate,
                          num_channels,
                          OPUS_APPLICATION_AUDIO) != OPUS_OK) {
      ImplBase::cast_initialization_status_ =
          STATUS_INVALID_AUDIO_CONFIGURATION;
      return;
    }
    ImplBase::cast_initialization_status_ = STATUS_AUDIO_INITIALIZED;

    if (bitrate <= 0) {
      // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
      // variable bitrate up to 102kbps for 2-channel, 48 kHz audio and a 10 ms
      // frame size.  The opus library authors may, of course, adjust this in
      // later versions.
      bitrate = OPUS_AUTO;
    }
    CHECK_EQ(opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(bitrate)),
             OPUS_OK);
  }

 private:
  virtual ~OpusImpl() {}

  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) OVERRIDE {
    // Opus requires channel-interleaved samples in a single array.
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      const float* src = audio_bus->channel(ch) + source_offset;
      const float* const src_end = src + num_samples;
      float* dest = buffer_.get() + buffer_fill_offset * num_channels_ + ch;
      for (; src < src_end; ++src, dest += num_channels_)
        *dest = *src;
    }
  }

  virtual bool EncodeFromFilledBuffer(std::string* out) OVERRIDE {
    out->resize(kOpusMaxPayloadSize);
    const opus_int32 result =
        opus_encode_float(opus_encoder_,
                          buffer_.get(),
                          samples_per_frame_,
                          reinterpret_cast<uint8*>(string_as_array(out)),
                          kOpusMaxPayloadSize);
    if (result > 1) {
      out->resize(result);
      return true;
    } else if (result < 0) {
      LOG(ERROR) << "Error code from opus_encode_float(): " << result;
      return false;
    } else {
      // Do nothing: The documentation says that a return value of zero or
      // one byte means the packet does not need to be transmitted.
      return false;
    }
  }

  const scoped_ptr<uint8[]> encoder_memory_;
  OpusEncoder* const opus_encoder_;
  const scoped_ptr<float[]> buffer_;

  // This is the recommended value, according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints.
  //
  // Note: Whereas other RTP implementations do not, the cast library is
  // perfectly capable of transporting larger than MTU-sized audio frames.
  static const int kOpusMaxPayloadSize = 4000;

  DISALLOW_COPY_AND_ASSIGN(OpusImpl);
};

class AudioEncoder::Pcm16Impl : public AudioEncoder::ImplBase {
 public:
  Pcm16Impl(const scoped_refptr<CastEnvironment>& cast_environment,
            int num_channels,
            int sampling_rate,
            const FrameEncodedCallback& callback)
      : ImplBase(cast_environment,
                 transport::kPcm16,
                 num_channels,
                 sampling_rate,
                 callback),
        buffer_(new int16[num_channels * samples_per_frame_]) {
    if (ImplBase::cast_initialization_status_ != STATUS_AUDIO_UNINITIALIZED)
      return;
    cast_initialization_status_ = STATUS_AUDIO_INITIALIZED;
  }

 private:
  virtual ~Pcm16Impl() {}

  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) OVERRIDE {
    audio_bus->ToInterleavedPartial(
        source_offset,
        num_samples,
        sizeof(int16),
        buffer_.get() + buffer_fill_offset * num_channels_);
  }

  virtual bool EncodeFromFilledBuffer(std::string* out) OVERRIDE {
    // Output 16-bit PCM integers in big-endian byte order.
    out->resize(num_channels_ * samples_per_frame_ * sizeof(int16));
    const int16* src = buffer_.get();
    const int16* const src_end = src + num_channels_ * samples_per_frame_;
    uint16* dest = reinterpret_cast<uint16*>(&out->at(0));
    for (; src < src_end; ++src, ++dest)
      *dest = base::HostToNet16(*src);
    return true;
  }

 private:
  const scoped_ptr<int16[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Pcm16Impl);
};

AudioEncoder::AudioEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const AudioSenderConfig& audio_config,
    const FrameEncodedCallback& frame_encoded_callback)
    : cast_environment_(cast_environment) {
  // Note: It doesn't matter which thread constructs AudioEncoder, just so long
  // as all calls to InsertAudio() are by the same thread.
  insert_thread_checker_.DetachFromThread();
  switch (audio_config.codec) {
    case transport::kOpus:
      impl_ = new OpusImpl(cast_environment,
                           audio_config.channels,
                           audio_config.frequency,
                           audio_config.bitrate,
                           frame_encoded_callback);
      break;
    case transport::kPcm16:
      impl_ = new Pcm16Impl(cast_environment,
                            audio_config.channels,
                            audio_config.frequency,
                            frame_encoded_callback);
      break;
    default:
      NOTREACHED() << "Unsupported or unspecified codec for audio encoder";
      break;
  }
}

AudioEncoder::~AudioEncoder() {}

CastInitializationStatus AudioEncoder::InitializationResult() const {
  DCHECK(insert_thread_checker_.CalledOnValidThread());
  if (impl_) {
    return impl_->InitializationResult();
  }
  return STATUS_UNSUPPORTED_AUDIO_CODEC;
}

void AudioEncoder::InsertAudio(scoped_ptr<AudioBus> audio_bus,
                               const base::TimeTicks& recorded_time) {
  DCHECK(insert_thread_checker_.CalledOnValidThread());
  DCHECK(audio_bus.get());
  if (!impl_) {
    NOTREACHED();
    return;
  }
  cast_environment_->PostTask(CastEnvironment::AUDIO,
                              FROM_HERE,
                              base::Bind(&AudioEncoder::ImplBase::EncodeAudio,
                                         impl_,
                                         base::Passed(&audio_bus),
                                         recorded_time));
}

}  // namespace cast
}  // namespace media
