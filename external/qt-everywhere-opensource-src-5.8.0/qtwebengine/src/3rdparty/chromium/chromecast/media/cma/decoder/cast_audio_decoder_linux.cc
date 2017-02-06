// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/decoder/cast_audio_decoder.h"

#include <stdint.h>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/cdm_context.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/sample_format.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/opus_audio_decoder.h"

namespace chromecast {
namespace media {

namespace {

const int kOpusSamplingRate = 48000;
const uint8_t kFakeOpusExtraData[19] = {
  'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',  // offset 0, OpusHead
  0,  // offset 8, version
  2,  // offset 9, channels
  0, 0,  // offset 10, skip
  static_cast<uint8_t>(kOpusSamplingRate & 0xFF),  // offset 12, LE
  static_cast<uint8_t>((kOpusSamplingRate >> 8) & 0xFF),
  static_cast<uint8_t>((kOpusSamplingRate >> 16) & 0xFF),
  static_cast<uint8_t>((kOpusSamplingRate >> 24) & 0xFF),
  0, 0,  // offset 16, gain
  0,  // offset 18, stereo mapping
};

const int kOutputChannelCount = 2;  // Always output stereo audio.
const int kMaxChannelInput = 2;

class CastAudioDecoderImpl : public CastAudioDecoder {
 public:
  CastAudioDecoderImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const InitializedCallback& initialized_callback,
      OutputFormat output_format)
      : task_runner_(task_runner),
        initialized_callback_(initialized_callback),
        output_format_(output_format),
        initialized_(false),
        decode_pending_(false),
        weak_factory_(this) {}

  ~CastAudioDecoderImpl() override {}

  void Initialize(const media::AudioConfig& config) {
    DCHECK(!initialized_);
    DCHECK_LE(config_.channel_number, kMaxChannelInput);
    config_ = config;

    // Media pipeline should already decrypt the stream with the selected key
    // system. If media pipeline fails to decrypt the stream for some reason,
    // the decoder will report kDecodeError when decoding.
    config_.encryption_scheme = Unencrypted();

    if (config_.channel_number == 1) {
      // If the input is mono, create a ChannelMixer to convert mono to stereo.
      // TODO(kmackay) Support other channel format conversions?
      mixer_.reset(new ::media::ChannelMixer(::media::CHANNEL_LAYOUT_MONO,
                                             ::media::CHANNEL_LAYOUT_STEREO));
    }
    base::WeakPtr<CastAudioDecoderImpl> self = weak_factory_.GetWeakPtr();
    if (config.codec == media::kCodecOpus) {
      // Insert fake extradata to make OpusAudioDecoder work with v2mirroring.
      if (config_.extra_data.empty() &&
          config_.samples_per_second == kOpusSamplingRate &&
          config_.channel_number == 2)
        config_.extra_data.assign(
            kFakeOpusExtraData,
            kFakeOpusExtraData + sizeof(kFakeOpusExtraData));
      decoder_.reset(new ::media::OpusAudioDecoder(task_runner_));
    } else {
      decoder_.reset(new ::media::FFmpegAudioDecoder(
          task_runner_, make_scoped_refptr(new ::media::MediaLog())));
    }
    decoder_->Initialize(
        media::DecoderConfigAdapter::ToMediaAudioDecoderConfig(config_),
        nullptr, base::Bind(&CastAudioDecoderImpl::OnInitialized, self),
        base::Bind(&CastAudioDecoderImpl::OnDecoderOutput, self));
    // Unfortunately there is no result from decoder_->Initialize() until later
    // (the pipeline status callback is posted to the task runner).
  }

  // CastAudioDecoder implementation:
  bool Decode(const scoped_refptr<media::DecoderBufferBase>& data,
              const DecodeCallback& decode_callback) override {
    DCHECK(!decode_callback.is_null());
    DCHECK(task_runner_->BelongsToCurrentThread());

    if (data->decrypt_context() != nullptr) {
      LOG(ERROR) << "Audio decoder doesn't support encrypted stream";
      decode_callback.Run(kDecodeError, data);
    } else if (!initialized_ || decode_pending_) {
      decode_queue_.push(std::make_pair(data, decode_callback));
    } else {
      DecodeNow(data, decode_callback);
    }
    return true;
  }

 private:
  typedef std::pair<scoped_refptr<media::DecoderBufferBase>, DecodeCallback>
      DecodeBufferCallbackPair;

  void DecodeNow(const scoped_refptr<media::DecoderBufferBase>& data,
                 const DecodeCallback& decode_callback) {
    if (data->end_of_stream()) {
      // Post the task to ensure that |decode_callback| is not called from
      // within a call to Decode().
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(decode_callback, kDecodeOk, data));
      return;
    }

    // FFmpegAudioDecoder requires a timestamp to be set.
    base::TimeDelta timestamp =
        base::TimeDelta::FromMicroseconds(data->timestamp());
    if (timestamp == ::media::kNoTimestamp())
      data->set_timestamp(base::TimeDelta());

    decode_pending_ = true;
    decoder_->Decode(
        data->ToMediaBuffer(),
        base::Bind(&CastAudioDecoderImpl::OnDecodeStatus,
                   weak_factory_.GetWeakPtr(), timestamp, decode_callback));
  }

  void OnInitialized(bool success) {
    DCHECK(!initialized_);
    LOG_IF(ERROR, !success) << "Failed to initialize FFmpegAudioDecoder";
    if (success)
      initialized_ = true;

    if (success && !decode_queue_.empty()) {
      const auto& d = decode_queue_.front();
      DecodeNow(d.first, d.second);
      decode_queue_.pop();
    }

    if (!initialized_callback_.is_null())
      initialized_callback_.Run(initialized_);
  }

  void OnDecodeStatus(base::TimeDelta buffer_timestamp,
                      const DecodeCallback& decode_callback,
                      ::media::DecodeStatus status) {
    Status result_status = kDecodeOk;
    scoped_refptr<media::DecoderBufferBase> decoded;
    if (status == ::media::DecodeStatus::OK && !decoded_chunks_.empty()) {
      decoded = ConvertDecoded();
    } else {
      if (status != ::media::DecodeStatus::OK)
        result_status = kDecodeError;
      decoded = new media::DecoderBufferAdapter(config_.id,
                                                new ::media::DecoderBuffer(0));
    }
    decoded_chunks_.clear();
    decoded->set_timestamp(buffer_timestamp);
    base::WeakPtr<CastAudioDecoderImpl> self = weak_factory_.GetWeakPtr();
    decode_callback.Run(result_status, decoded);
    if (!self.get())
      return;  // Return immediately if the decode callback deleted this.

    // Do not reset decode_pending_ to false until after the callback has
    // finished running because the callback may call Decode().
    decode_pending_ = false;

    if (decode_queue_.empty())
      return;

    const auto& d = decode_queue_.front();
    // Calling DecodeNow() here does not result in a loop, because
    // OnDecodeStatus() is always called asynchronously (guaranteed by the
    // AudioDecoder interface).
    DecodeNow(d.first, d.second);
    decode_queue_.pop();
  }

  void OnDecoderOutput(const scoped_refptr<::media::AudioBuffer>& decoded) {
    decoded_chunks_.push_back(decoded);
  }

  scoped_refptr<media::DecoderBufferBase> ConvertDecoded() {
    DCHECK(!decoded_chunks_.empty());
    int num_frames = 0;
    for (auto& chunk : decoded_chunks_)
      num_frames += chunk->frame_count();

    // Copy decoded data into an AudioBus for conversion.
    std::unique_ptr<::media::AudioBus> decoded =
        ::media::AudioBus::Create(config_.channel_number, num_frames);
    int bus_frame_offset = 0;
    for (auto& chunk : decoded_chunks_) {
      chunk->ReadFrames(chunk->frame_count(), 0, bus_frame_offset,
                        decoded.get());
      bus_frame_offset += chunk->frame_count();
    }

    if (mixer_) {
      // Convert to stereo if necessary.
      std::unique_ptr<::media::AudioBus> converted_to_stereo =
          ::media::AudioBus::Create(kOutputChannelCount, num_frames);
      mixer_->Transform(decoded.get(), converted_to_stereo.get());
      decoded.swap(converted_to_stereo);
    }

    // Convert to the desired output format.
    return FinishConversion(decoded.get());
  }

  scoped_refptr<media::DecoderBufferBase> FinishConversion(
      ::media::AudioBus* bus) {
    DCHECK_EQ(kOutputChannelCount, bus->channels());
    int size = bus->frames() * kOutputChannelCount *
               OutputFormatSizeInBytes(output_format_);
    scoped_refptr<::media::DecoderBuffer> result(
        new ::media::DecoderBuffer(size));

    if (output_format_ == kOutputSigned16) {
      bus->ToInterleaved(bus->frames(), OutputFormatSizeInBytes(output_format_),
                         result->writable_data());
    } else if (output_format_ == kOutputPlanarFloat) {
      // Data in an AudioBus is already in planar float format; just copy each
      // channel into the result buffer in order.
      float* ptr = reinterpret_cast<float*>(result->writable_data());
      for (int c = 0; c < bus->channels(); ++c) {
        memcpy(ptr, bus->channel(c), bus->frames() * sizeof(float));
        ptr += bus->frames();
      }
    } else {
      NOTREACHED();
    }

    result->set_duration(base::TimeDelta::FromMicroseconds(
        bus->frames() * base::Time::kMicrosecondsPerSecond /
        config_.samples_per_second));
    return make_scoped_refptr(
        new media::DecoderBufferAdapter(config_.id, result));
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InitializedCallback initialized_callback_;
  OutputFormat output_format_;
  media::AudioConfig config_;
  std::unique_ptr<::media::AudioDecoder> decoder_;
  std::queue<DecodeBufferCallbackPair> decode_queue_;
  bool initialized_;
  std::unique_ptr<::media::ChannelMixer> mixer_;
  bool decode_pending_;
  std::vector<scoped_refptr<::media::AudioBuffer>> decoded_chunks_;
  base::WeakPtrFactory<CastAudioDecoderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioDecoderImpl);
};

}  // namespace

// static
std::unique_ptr<CastAudioDecoder> CastAudioDecoder::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const media::AudioConfig& config,
    OutputFormat output_format,
    const InitializedCallback& initialized_callback) {
  std::unique_ptr<CastAudioDecoderImpl> decoder(new CastAudioDecoderImpl(
      task_runner, initialized_callback, output_format));
  decoder->Initialize(config);
  return std::move(decoder);
}

// static
int CastAudioDecoder::OutputFormatSizeInBytes(
    CastAudioDecoder::OutputFormat format) {
  switch (format) {
    case CastAudioDecoder::OutputFormat::kOutputSigned16:
      return 2;
    case CastAudioDecoder::OutputFormat::kOutputPlanarFloat:
      return 4;
  }
  NOTREACHED();
  return 1;
}

}  // namespace media
}  // namespace chromecast
