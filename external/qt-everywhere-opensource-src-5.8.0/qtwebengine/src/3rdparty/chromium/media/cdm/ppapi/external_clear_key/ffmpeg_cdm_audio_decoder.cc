// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/external_clear_key/ffmpeg_cdm_audio_decoder.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"
#include "media/ffmpeg/ffmpeg_common.h"

// Include FFmpeg header files.
extern "C" {
// Temporarily disable possible loss of data warning.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavcodec/avcodec.h>
MSVC_POP_WARNING();
}  // extern "C"

namespace media {

// Maximum number of channels with defined layout in src/media.
static const int kMaxChannels = 8;

static AVCodecID CdmAudioCodecToCodecID(
    cdm::AudioDecoderConfig::AudioCodec audio_codec) {
  switch (audio_codec) {
    case cdm::AudioDecoderConfig::kCodecVorbis:
      return AV_CODEC_ID_VORBIS;
    case cdm::AudioDecoderConfig::kCodecAac:
      return AV_CODEC_ID_AAC;
    case cdm::AudioDecoderConfig::kUnknownAudioCodec:
    default:
      NOTREACHED() << "Unsupported cdm::AudioCodec: " << audio_codec;
      return AV_CODEC_ID_NONE;
  }
}

static void CdmAudioDecoderConfigToAVCodecContext(
    const cdm::AudioDecoderConfig& config,
    AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = CdmAudioCodecToCodecID(config.codec);

  switch (config.bits_per_channel) {
    case 8:
      codec_context->sample_fmt = AV_SAMPLE_FMT_U8;
      break;
    case 16:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
      break;
    case 32:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S32;
      break;
    default:
      DVLOG(1) << "CdmAudioDecoderConfigToAVCodecContext() Unsupported bits "
                  "per channel: " << config.bits_per_channel;
      codec_context->sample_fmt = AV_SAMPLE_FMT_NONE;
  }

  codec_context->channels = config.channel_count;
  codec_context->sample_rate = config.samples_per_second;

  if (config.extra_data) {
    codec_context->extradata_size = config.extra_data_size;
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, config.extra_data,
           config.extra_data_size);
    memset(codec_context->extradata + config.extra_data_size, '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    codec_context->extradata = NULL;
    codec_context->extradata_size = 0;
  }
}

static cdm::AudioFormat AVSampleFormatToCdmAudioFormat(
    AVSampleFormat sample_format) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
      return cdm::kAudioFormatU8;
    case AV_SAMPLE_FMT_S16:
      return cdm::kAudioFormatS16;
    case AV_SAMPLE_FMT_S32:
      return cdm::kAudioFormatS32;
    case AV_SAMPLE_FMT_FLT:
      return cdm::kAudioFormatF32;
    case AV_SAMPLE_FMT_S16P:
      return cdm::kAudioFormatPlanarS16;
    case AV_SAMPLE_FMT_FLTP:
      return cdm::kAudioFormatPlanarF32;
    default:
      DVLOG(1) << "Unknown AVSampleFormat: " << sample_format;
  }
  return cdm::kUnknownAudioFormat;
}

static void CopySamples(cdm::AudioFormat cdm_format,
                        int decoded_audio_size,
                        const AVFrame& av_frame,
                        uint8_t* output_buffer) {
  switch (cdm_format) {
    case cdm::kAudioFormatU8:
    case cdm::kAudioFormatS16:
    case cdm::kAudioFormatS32:
    case cdm::kAudioFormatF32:
      memcpy(output_buffer, av_frame.data[0], decoded_audio_size);
      break;
    case cdm::kAudioFormatPlanarS16:
    case cdm::kAudioFormatPlanarF32: {
      const int decoded_size_per_channel =
          decoded_audio_size / av_frame.channels;
      for (int i = 0; i < av_frame.channels; ++i) {
        memcpy(output_buffer,
               av_frame.extended_data[i],
               decoded_size_per_channel);
        output_buffer += decoded_size_per_channel;
      }
      break;
    }
    default:
      NOTREACHED() << "Unsupported CDM Audio Format!";
      memset(output_buffer, 0, decoded_audio_size);
  }
}

FFmpegCdmAudioDecoder::FFmpegCdmAudioDecoder(ClearKeyCdmHost* host)
    : is_initialized_(false),
      host_(host),
      samples_per_second_(0),
      channels_(0),
      av_sample_format_(0),
      bytes_per_frame_(0),
      last_input_timestamp_(kNoTimestamp()),
      output_bytes_to_drop_(0) {
}

FFmpegCdmAudioDecoder::~FFmpegCdmAudioDecoder() {
  ReleaseFFmpegResources();
}

bool FFmpegCdmAudioDecoder::Initialize(const cdm::AudioDecoderConfig& config) {
  DVLOG(1) << "Initialize()";
  if (!IsValidConfig(config)) {
    LOG(ERROR) << "Initialize(): invalid audio decoder configuration.";
    return false;
  }

  if (is_initialized_) {
    LOG(ERROR) << "Initialize(): Already initialized.";
    return false;
  }

  // Initialize AVCodecContext structure.
  codec_context_.reset(avcodec_alloc_context3(NULL));
  CdmAudioDecoderConfigToAVCodecContext(config, codec_context_.get());

  // MP3 decodes to S16P which we don't support, tell it to use S16 instead.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P)
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    return false;
  }

  // Ensure avcodec_open2() respected our format request.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
    DLOG(ERROR) << "Unable to configure a supported sample format: "
                << codec_context_->sample_fmt;
    return false;
  }

  // Success!
  av_frame_.reset(av_frame_alloc());
  samples_per_second_ = config.samples_per_second;
  bytes_per_frame_ = codec_context_->channels * config.bits_per_channel / 8;
  output_timestamp_helper_.reset(
      new AudioTimestampHelper(config.samples_per_second));
  is_initialized_ = true;

  // Store initial values to guard against midstream configuration changes.
  channels_ = codec_context_->channels;
  av_sample_format_ = codec_context_->sample_fmt;

  return true;
}

void FFmpegCdmAudioDecoder::Deinitialize() {
  DVLOG(1) << "Deinitialize()";
  ReleaseFFmpegResources();
  is_initialized_ = false;
  ResetTimestampState();
}

void FFmpegCdmAudioDecoder::Reset() {
  DVLOG(1) << "Reset()";
  avcodec_flush_buffers(codec_context_.get());
  ResetTimestampState();
}

// static
bool FFmpegCdmAudioDecoder::IsValidConfig(
    const cdm::AudioDecoderConfig& config) {
  return config.codec != cdm::AudioDecoderConfig::kUnknownAudioCodec &&
         config.channel_count > 0 &&
         config.channel_count <= kMaxChannels &&
         config.bits_per_channel > 0 &&
         config.bits_per_channel <= limits::kMaxBitsPerSample &&
         config.samples_per_second > 0 &&
         config.samples_per_second <= limits::kMaxSampleRate;
}

cdm::Status FFmpegCdmAudioDecoder::DecodeBuffer(
    const uint8_t* compressed_buffer,
    int32_t compressed_buffer_size,
    int64_t input_timestamp,
    cdm::AudioFrames* decoded_frames) {
  DVLOG(1) << "DecodeBuffer()";
  const bool is_end_of_stream = !compressed_buffer;
  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(input_timestamp);

  bool is_vorbis = codec_context_->codec_id == AV_CODEC_ID_VORBIS;
  if (!is_end_of_stream) {
    if (last_input_timestamp_ == kNoTimestamp()) {
      if (is_vorbis && timestamp < base::TimeDelta()) {
        // Dropping frames for negative timestamps as outlined in section A.2
        // in the Vorbis spec. http://xiph.org/vorbis/doc/Vorbis_I_spec.html
        int frames_to_drop = floor(
            0.5 + -timestamp.InSecondsF() * samples_per_second_);
        output_bytes_to_drop_ = bytes_per_frame_ * frames_to_drop;
      } else {
        last_input_timestamp_ = timestamp;
      }
    } else if (timestamp != kNoTimestamp()) {
      if (timestamp < last_input_timestamp_) {
        base::TimeDelta diff = timestamp - last_input_timestamp_;
        DVLOG(1) << "Input timestamps are not monotonically increasing! "
                 << " ts " << timestamp.InMicroseconds() << " us"
                 << " diff " << diff.InMicroseconds() << " us";
        return cdm::kDecodeError;
      }

      last_input_timestamp_ = timestamp;
    }
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(compressed_buffer);
  packet.size = compressed_buffer_size;

  // Tell the CDM what AudioFormat we're using.
  const cdm::AudioFormat cdm_format = AVSampleFormatToCdmAudioFormat(
      static_cast<AVSampleFormat>(av_sample_format_));
  DCHECK_NE(cdm_format, cdm::kUnknownAudioFormat);
  decoded_frames->SetFormat(cdm_format);

  // Each audio packet may contain several frames, so we must call the decoder
  // until we've exhausted the packet.  Regardless of the packet size we always
  // want to hand it to the decoder at least once, otherwise we would end up
  // skipping end of stream packets since they have a size of zero.
  do {
    // Reset frame to default values.
    av_frame_unref(av_frame_.get());

    int frame_decoded = 0;
    int result = avcodec_decode_audio4(
        codec_context_.get(), av_frame_.get(), &frame_decoded, &packet);

    if (result < 0) {
      DCHECK(!is_end_of_stream)
          << "End of stream buffer produced an error! "
          << "This is quite possibly a bug in the audio decoder not handling "
          << "end of stream AVPackets correctly.";

      DLOG(ERROR)
          << "Error decoding an audio frame with timestamp: "
          << timestamp.InMicroseconds() << " us, duration: "
          << timestamp.InMicroseconds() << " us, packet size: "
          << compressed_buffer_size << " bytes";

      return cdm::kDecodeError;
    }

    // Update packet size and data pointer in case we need to call the decoder
    // with the remaining bytes from this packet.
    packet.size -= result;
    packet.data += result;

    if (output_timestamp_helper_->base_timestamp() == kNoTimestamp() &&
        !is_end_of_stream) {
      DCHECK(timestamp != kNoTimestamp());
      if (output_bytes_to_drop_ > 0) {
        // Currently Vorbis is the only codec that causes us to drop samples.
        // If we have to drop samples it always means the timeline starts at 0.
        DCHECK_EQ(codec_context_->codec_id, AV_CODEC_ID_VORBIS);
        output_timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
      } else {
        output_timestamp_helper_->SetBaseTimestamp(timestamp);
      }
    }

    int decoded_audio_size = 0;
    if (frame_decoded) {
      if (av_frame_->sample_rate != samples_per_second_ ||
          av_frame_->channels != channels_ ||
          av_frame_->format != av_sample_format_) {
        DLOG(ERROR) << "Unsupported midstream configuration change!"
                    << " Sample Rate: " << av_frame_->sample_rate << " vs "
                    << samples_per_second_
                    << ", Channels: " << av_frame_->channels << " vs "
                    << channels_
                    << ", Sample Format: " << av_frame_->format << " vs "
                    << av_sample_format_;
        return cdm::kDecodeError;
      }

      decoded_audio_size = av_samples_get_buffer_size(
          NULL, codec_context_->channels, av_frame_->nb_samples,
          codec_context_->sample_fmt, 1);
    }

    if (decoded_audio_size > 0 && output_bytes_to_drop_ > 0) {
      DCHECK_EQ(decoded_audio_size % bytes_per_frame_, 0)
          << "Decoder didn't output full frames";

      int dropped_size = std::min(decoded_audio_size, output_bytes_to_drop_);
      decoded_audio_size -= dropped_size;
      output_bytes_to_drop_ -= dropped_size;
    }

    if (decoded_audio_size > 0) {
      DCHECK_EQ(decoded_audio_size % bytes_per_frame_, 0)
          << "Decoder didn't output full frames";

      base::TimeDelta output_timestamp =
          output_timestamp_helper_->GetTimestamp();
      output_timestamp_helper_->AddFrames(decoded_audio_size /
                                          bytes_per_frame_);

      // If we've exhausted the packet in the first decode we can write directly
      // into the frame buffer instead of a multistep serialization approach.
      if (serialized_audio_frames_.empty() && !packet.size) {
        const uint32_t buffer_size = decoded_audio_size + sizeof(int64_t) * 2;
        decoded_frames->SetFrameBuffer(host_->Allocate(buffer_size));
        if (!decoded_frames->FrameBuffer()) {
          LOG(ERROR) << "DecodeBuffer() ClearKeyCdmHost::Allocate failed.";
          return cdm::kDecodeError;
        }
        decoded_frames->FrameBuffer()->SetSize(buffer_size);
        uint8_t* output_buffer = decoded_frames->FrameBuffer()->Data();

        const int64_t timestamp = output_timestamp.InMicroseconds();
        memcpy(output_buffer, &timestamp, sizeof(timestamp));
        output_buffer += sizeof(timestamp);

        const int64_t output_size = decoded_audio_size;
        memcpy(output_buffer, &output_size, sizeof(output_size));
        output_buffer += sizeof(output_size);

        // Copy the samples and return success.
        CopySamples(
            cdm_format, decoded_audio_size, *av_frame_, output_buffer);
        return cdm::kSuccess;
      }

      // There are still more frames to decode, so we need to serialize them in
      // a secondary buffer since we don't know their sizes ahead of time (which
      // is required to allocate the FrameBuffer object).
      SerializeInt64(output_timestamp.InMicroseconds());
      SerializeInt64(decoded_audio_size);

      const size_t previous_size = serialized_audio_frames_.size();
      serialized_audio_frames_.resize(previous_size + decoded_audio_size);
      uint8_t* output_buffer = &serialized_audio_frames_[0] + previous_size;
      CopySamples(
          cdm_format, decoded_audio_size, *av_frame_, output_buffer);
    }
  } while (packet.size > 0);

  if (!serialized_audio_frames_.empty()) {
    decoded_frames->SetFrameBuffer(
        host_->Allocate(serialized_audio_frames_.size()));
    if (!decoded_frames->FrameBuffer()) {
      LOG(ERROR) << "DecodeBuffer() ClearKeyCdmHost::Allocate failed.";
      return cdm::kDecodeError;
    }
    memcpy(decoded_frames->FrameBuffer()->Data(),
           &serialized_audio_frames_[0],
           serialized_audio_frames_.size());
    decoded_frames->FrameBuffer()->SetSize(serialized_audio_frames_.size());
    serialized_audio_frames_.clear();

    return cdm::kSuccess;
  }

  return cdm::kNeedMoreData;
}

void FFmpegCdmAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  last_input_timestamp_ = kNoTimestamp();
  output_bytes_to_drop_ = 0;
}

void FFmpegCdmAudioDecoder::ReleaseFFmpegResources() {
  DVLOG(1) << "ReleaseFFmpegResources()";

  codec_context_.reset();
  av_frame_.reset();
}

void FFmpegCdmAudioDecoder::SerializeInt64(int64_t value) {
  const size_t previous_size = serialized_audio_frames_.size();
  serialized_audio_frames_.resize(previous_size + sizeof(value));
  memcpy(&serialized_audio_frames_[0] + previous_size, &value, sizeof(value));
}

}  // namespace media
