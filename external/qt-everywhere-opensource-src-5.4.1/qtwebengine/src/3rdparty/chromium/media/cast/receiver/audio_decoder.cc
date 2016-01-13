// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/audio_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sys_byteorder.h"
#include "media/cast/cast_defines.h"
#include "third_party/opus/src/include/opus.h"

namespace media {
namespace cast {

// Base class that handles the common problem of detecting dropped frames, and
// then invoking the Decode() method implemented by the subclasses to convert
// the encoded payload data into usable audio data.
class AudioDecoder::ImplBase
    : public base::RefCountedThreadSafe<AudioDecoder::ImplBase> {
 public:
  ImplBase(const scoped_refptr<CastEnvironment>& cast_environment,
           transport::AudioCodec codec,
           int num_channels,
           int sampling_rate)
      : cast_environment_(cast_environment),
        codec_(codec),
        num_channels_(num_channels),
        cast_initialization_status_(STATUS_AUDIO_UNINITIALIZED),
        seen_first_frame_(false) {
    if (num_channels_ <= 0 || sampling_rate <= 0 || sampling_rate % 100 != 0)
      cast_initialization_status_ = STATUS_INVALID_AUDIO_CONFIGURATION;
  }

  CastInitializationStatus InitializationResult() const {
    return cast_initialization_status_;
  }

  void DecodeFrame(scoped_ptr<transport::EncodedFrame> encoded_frame,
                   const DecodeFrameCallback& callback) {
    DCHECK_EQ(cast_initialization_status_, STATUS_AUDIO_INITIALIZED);

    COMPILE_ASSERT(sizeof(encoded_frame->frame_id) == sizeof(last_frame_id_),
                   size_of_frame_id_types_do_not_match);
    bool is_continuous = true;
    if (seen_first_frame_) {
      const uint32 frames_ahead = encoded_frame->frame_id - last_frame_id_;
      if (frames_ahead > 1) {
        RecoverBecauseFramesWereDropped();
        is_continuous = false;
      }
    } else {
      seen_first_frame_ = true;
    }
    last_frame_id_ = encoded_frame->frame_id;

    scoped_ptr<AudioBus> decoded_audio = Decode(
        encoded_frame->mutable_bytes(),
        static_cast<int>(encoded_frame->data.size()));
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(callback,
                                           base::Passed(&decoded_audio),
                                           is_continuous));
  }

 protected:
  friend class base::RefCountedThreadSafe<ImplBase>;
  virtual ~ImplBase() {}

  virtual void RecoverBecauseFramesWereDropped() {}

  // Note: Implementation of Decode() is allowed to mutate |data|.
  virtual scoped_ptr<AudioBus> Decode(uint8* data, int len) = 0;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const transport::AudioCodec codec_;
  const int num_channels_;

  // Subclass' ctor is expected to set this to STATUS_AUDIO_INITIALIZED.
  CastInitializationStatus cast_initialization_status_;

 private:
  bool seen_first_frame_;
  uint32 last_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(ImplBase);
};

class AudioDecoder::OpusImpl : public AudioDecoder::ImplBase {
 public:
  OpusImpl(const scoped_refptr<CastEnvironment>& cast_environment,
           int num_channels,
           int sampling_rate)
      : ImplBase(cast_environment,
                 transport::kOpus,
                 num_channels,
                 sampling_rate),
        decoder_memory_(new uint8[opus_decoder_get_size(num_channels)]),
        opus_decoder_(reinterpret_cast<OpusDecoder*>(decoder_memory_.get())),
        max_samples_per_frame_(
            kOpusMaxFrameDurationMillis * sampling_rate / 1000),
        buffer_(new float[max_samples_per_frame_ * num_channels]) {
    if (ImplBase::cast_initialization_status_ != STATUS_AUDIO_UNINITIALIZED)
      return;
    if (opus_decoder_init(opus_decoder_, sampling_rate, num_channels) !=
            OPUS_OK) {
      ImplBase::cast_initialization_status_ =
          STATUS_INVALID_AUDIO_CONFIGURATION;
      return;
    }
    ImplBase::cast_initialization_status_ = STATUS_AUDIO_INITIALIZED;
  }

 private:
  virtual ~OpusImpl() {}

  virtual void RecoverBecauseFramesWereDropped() OVERRIDE {
    // Passing NULL for the input data notifies the decoder of frame loss.
    const opus_int32 result =
        opus_decode_float(
            opus_decoder_, NULL, 0, buffer_.get(), max_samples_per_frame_, 0);
    DCHECK_GE(result, 0);
  }

  virtual scoped_ptr<AudioBus> Decode(uint8* data, int len) OVERRIDE {
    scoped_ptr<AudioBus> audio_bus;
    const opus_int32 num_samples_decoded = opus_decode_float(
        opus_decoder_, data, len, buffer_.get(), max_samples_per_frame_, 0);
    if (num_samples_decoded <= 0)
      return audio_bus.Pass();  // Decode error.

    // Copy interleaved samples from |buffer_| into a new AudioBus (where
    // samples are stored in planar format, for each channel).
    audio_bus = AudioBus::Create(num_channels_, num_samples_decoded).Pass();
    // TODO(miu): This should be moved into AudioBus::FromInterleaved().
    for (int ch = 0; ch < num_channels_; ++ch) {
      const float* src = buffer_.get() + ch;
      const float* const src_end = src + num_samples_decoded * num_channels_;
      float* dest = audio_bus->channel(ch);
      for (; src < src_end; src += num_channels_, ++dest)
        *dest = *src;
    }
    return audio_bus.Pass();
  }

  const scoped_ptr<uint8[]> decoder_memory_;
  OpusDecoder* const opus_decoder_;
  const int max_samples_per_frame_;
  const scoped_ptr<float[]> buffer_;

  // According to documentation in third_party/opus/src/include/opus.h, we must
  // provide enough space in |buffer_| to contain 120ms of samples.  At 48 kHz,
  // then, that means 5760 samples times the number of channels.
  static const int kOpusMaxFrameDurationMillis = 120;

  DISALLOW_COPY_AND_ASSIGN(OpusImpl);
};

class AudioDecoder::Pcm16Impl : public AudioDecoder::ImplBase {
 public:
  Pcm16Impl(const scoped_refptr<CastEnvironment>& cast_environment,
            int num_channels,
            int sampling_rate)
      : ImplBase(cast_environment,
                 transport::kPcm16,
                 num_channels,
                 sampling_rate) {
    if (ImplBase::cast_initialization_status_ != STATUS_AUDIO_UNINITIALIZED)
      return;
    ImplBase::cast_initialization_status_ = STATUS_AUDIO_INITIALIZED;
  }

 private:
  virtual ~Pcm16Impl() {}

  virtual scoped_ptr<AudioBus> Decode(uint8* data, int len) OVERRIDE {
    scoped_ptr<AudioBus> audio_bus;
    const int num_samples = len / sizeof(int16) / num_channels_;
    if (num_samples <= 0)
      return audio_bus.Pass();

    int16* const pcm_data = reinterpret_cast<int16*>(data);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    // Convert endianness.
    const int num_elements = num_samples * num_channels_;
    for (int i = 0; i < num_elements; ++i)
      pcm_data[i] = static_cast<int16>(base::NetToHost16(pcm_data[i]));
#endif
    audio_bus = AudioBus::Create(num_channels_, num_samples).Pass();
    audio_bus->FromInterleaved(pcm_data, num_samples, sizeof(int16));
    return audio_bus.Pass();
  }

  DISALLOW_COPY_AND_ASSIGN(Pcm16Impl);
};

AudioDecoder::AudioDecoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    int channels,
    int sampling_rate,
    transport::AudioCodec codec)
    : cast_environment_(cast_environment) {
  switch (codec) {
    case transport::kOpus:
      impl_ = new OpusImpl(cast_environment, channels, sampling_rate);
      break;
    case transport::kPcm16:
      impl_ = new Pcm16Impl(cast_environment, channels, sampling_rate);
      break;
    default:
      NOTREACHED() << "Unknown or unspecified codec.";
      break;
  }
}

AudioDecoder::~AudioDecoder() {}

CastInitializationStatus AudioDecoder::InitializationResult() const {
  if (impl_)
    return impl_->InitializationResult();
  return STATUS_UNSUPPORTED_AUDIO_CODEC;
}

void AudioDecoder::DecodeFrame(
    scoped_ptr<transport::EncodedFrame> encoded_frame,
    const DecodeFrameCallback& callback) {
  DCHECK(encoded_frame.get());
  DCHECK(!callback.is_null());
  if (!impl_ || impl_->InitializationResult() != STATUS_AUDIO_INITIALIZED) {
    callback.Run(make_scoped_ptr<AudioBus>(NULL), false);
    return;
  }
  cast_environment_->PostTask(CastEnvironment::AUDIO,
                              FROM_HERE,
                              base::Bind(&AudioDecoder::ImplBase::DecodeFrame,
                                         impl_,
                                         base::Passed(&encoded_frame),
                                         callback));
}

}  // namespace cast
}  // namespace media
