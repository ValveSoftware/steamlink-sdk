// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decode_status.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/public/cpp/system/buffer.h"

namespace mojo {

#define ASSERT_ENUM_EQ(media_enum, media_prefix, mojo_prefix, value)        \
  static_assert(media::media_prefix##value ==                               \
                    static_cast<media::media_enum>(                         \
                        media::mojom::media_enum::mojo_prefix##value),      \
                "Mismatched enum: " #media_prefix #value " != " #media_enum \
                "::" #mojo_prefix #value)

#define ASSERT_ENUM_EQ_RAW(media_enum, media_enum_value, mojo_enum_value)      \
  static_assert(media::media_enum_value == static_cast<media::media_enum>(     \
                                               media::mojom::mojo_enum_value), \
                "Mismatched enum: " #media_enum_value " != " #mojo_enum_value)

#define ASSERT_ENUM_CLASS_EQ(media_enum, value)                            \
  static_assert(                                                           \
      media::media_enum::value ==                                          \
          static_cast<media::media_enum>(media::mojom::media_enum::value), \
      "Mismatched enum: " #media_enum #value)

// BufferingState.
ASSERT_ENUM_EQ(BufferingState, BUFFERING_, , HAVE_NOTHING);
ASSERT_ENUM_EQ(BufferingState, BUFFERING_, , HAVE_ENOUGH);

// DecodeStatus.
ASSERT_ENUM_CLASS_EQ(DecodeStatus, OK);
ASSERT_ENUM_CLASS_EQ(DecodeStatus, ABORTED);
ASSERT_ENUM_CLASS_EQ(DecodeStatus, DECODE_ERROR);

// AudioCodec.
ASSERT_ENUM_EQ_RAW(AudioCodec, kUnknownAudioCodec, AudioCodec::UNKNOWN);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , AAC);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , MP3);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , PCM);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , Vorbis);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , FLAC);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , AMR_NB);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , PCM_MULAW);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , GSM_MS);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , PCM_S16BE);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , PCM_S24BE);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , Opus);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , EAC3);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , PCM_ALAW);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , ALAC);
ASSERT_ENUM_EQ(AudioCodec, kCodec, , AC3);
ASSERT_ENUM_EQ_RAW(AudioCodec, kAudioCodecMax, AudioCodec::MAX);

// ChannelLayout.
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _NONE);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _UNSUPPORTED);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _MONO);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _STEREO);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _2_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _SURROUND);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _4_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _2_2);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _QUAD);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _5_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _5_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _5_0_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _5_1_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _7_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _7_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _7_1_WIDE);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _STEREO_DOWNMIX);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _2POINT1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _3_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _4_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _6_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _6_0_FRONT);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _HEXAGONAL);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _6_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _6_1_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _6_1_FRONT);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _7_0_FRONT);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _7_1_WIDE_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _OCTAGONAL);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _DISCRETE);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _STEREO_AND_KEYBOARD_MIC);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _4_1_QUAD_SIDE);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, k, _MAX);

// SampleFormat.
ASSERT_ENUM_EQ_RAW(SampleFormat, kUnknownSampleFormat, SampleFormat::UNKNOWN);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , U8);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , S16);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , S32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , F32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , PlanarS16);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , PlanarF32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, , Max);

// DemuxerStream Type.  Note: Mojo DemuxerStream's don't have the TEXT type.
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::UNKNOWN,
                   DemuxerStream::Type::UNKNOWN);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::AUDIO,
                   DemuxerStream::Type::AUDIO);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::VIDEO,
                   DemuxerStream::Type::VIDEO);
static_assert(
    media::DemuxerStream::NUM_TYPES ==
        static_cast<media::DemuxerStream::Type>(
            static_cast<int>(media::mojom::DemuxerStream::Type::LAST_TYPE) + 2),
    "Mismatched enum: media::DemuxerStream::NUM_TYPES != "
    "media::mojom::DemuxerStream::Type::LAST_TYPE + 2");

// DemuxerStream Status.
ASSERT_ENUM_EQ_RAW(DemuxerStream::Status,
                   DemuxerStream::kOk,
                   DemuxerStream::Status::OK);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Status,
                   DemuxerStream::kAborted,
                   DemuxerStream::Status::ABORTED);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Status,
                   DemuxerStream::kConfigChanged,
                   DemuxerStream::Status::CONFIG_CHANGED);

// VideoFormat.
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_UNKNOWN,
                   VideoFormat::UNKNOWN);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_I420, VideoFormat::I420);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV12, VideoFormat::YV12);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV16, VideoFormat::YV16);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV12A, VideoFormat::YV12A);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV24, VideoFormat::YV24);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_NV12, VideoFormat::NV12);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_NV21, VideoFormat::NV21);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_UYVY, VideoFormat::UYVY);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YUY2, VideoFormat::YUY2);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_ARGB, VideoFormat::ARGB);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_XRGB, VideoFormat::XRGB);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_RGB24, VideoFormat::RGB24);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_RGB32, VideoFormat::RGB32);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_MJPEG, VideoFormat::MJPEG);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_MT21, VideoFormat::MT21);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_YUV420P9,
                   VideoFormat::YUV420P9);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_YUV422P9,
                   VideoFormat::YUV422P9);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_YUV444P9,
                   VideoFormat::YUV444P9);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_YUV420P10,
                   VideoFormat::YUV420P10);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_YUV422P10,
                   VideoFormat::YUV422P10);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_YUV444P10,
                   VideoFormat::YUV444P10);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_MAX, VideoFormat::FORMAT_MAX);

// ColorSpace.
ASSERT_ENUM_EQ_RAW(ColorSpace,
                   COLOR_SPACE_UNSPECIFIED,
                   ColorSpace::UNSPECIFIED);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_JPEG, ColorSpace::JPEG);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_HD_REC709, ColorSpace::HD_REC709);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_SD_REC601, ColorSpace::SD_REC601);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_MAX, ColorSpace::MAX);

// VideoCodec
ASSERT_ENUM_EQ_RAW(VideoCodec, kUnknownVideoCodec, VideoCodec::UNKNOWN);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , H264);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , HEVC);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , VC1);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , MPEG2);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , MPEG4);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , Theora);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , VP8);
ASSERT_ENUM_EQ(VideoCodec, kCodec, , VP9);
ASSERT_ENUM_EQ_RAW(VideoCodec, kVideoCodecMax, VideoCodec::Max);

// VideoCodecProfile
ASSERT_ENUM_EQ(VideoCodecProfile, , , VIDEO_CODEC_PROFILE_UNKNOWN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VIDEO_CODEC_PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_BASELINE);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_MAIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_EXTENDED);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_HIGH);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_HIGH10PROFILE);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_HIGH422PROFILE);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_HIGH444PREDICTIVEPROFILE);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_SCALABLEBASELINE);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_SCALABLEHIGH);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_STEREOHIGH);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_MULTIVIEWHIGH);
ASSERT_ENUM_EQ(VideoCodecProfile, , , H264PROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP8PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP8PROFILE_ANY);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP8PROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP9PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP9PROFILE_PROFILE0);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP9PROFILE_PROFILE1);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP9PROFILE_PROFILE2);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP9PROFILE_PROFILE3);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VP9PROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile, , , HEVCPROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , HEVCPROFILE_MAIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , , HEVCPROFILE_MAIN10);
ASSERT_ENUM_EQ(VideoCodecProfile, , , HEVCPROFILE_MAIN_STILL_PICTURE);
ASSERT_ENUM_EQ(VideoCodecProfile, , , HEVCPROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile, , , VIDEO_CODEC_PROFILE_MAX);

// CipherMode
ASSERT_ENUM_EQ_RAW(EncryptionScheme::CipherMode,
                   EncryptionScheme::CipherMode::CIPHER_MODE_UNENCRYPTED,
                   CipherMode::UNENCRYPTED);
ASSERT_ENUM_EQ_RAW(EncryptionScheme::CipherMode,
                   EncryptionScheme::CipherMode::CIPHER_MODE_AES_CTR,
                   CipherMode::AES_CTR);
ASSERT_ENUM_EQ_RAW(EncryptionScheme::CipherMode,
                   EncryptionScheme::CipherMode::CIPHER_MODE_AES_CBC,
                   CipherMode::AES_CBC);
ASSERT_ENUM_EQ_RAW(EncryptionScheme::CipherMode,
                   EncryptionScheme::CipherMode::CIPHER_MODE_MAX,
                   CipherMode::MAX);

// Decryptor Status
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kSuccess,
                   Decryptor::Status::SUCCESS);
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kNoKey,
                   Decryptor::Status::NO_KEY);
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kNeedMoreData,
                   Decryptor::Status::NEED_MORE_DATA);
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kError,
                   Decryptor::Status::DECRYPTION_ERROR);

// CdmException
#define ASSERT_CDM_EXCEPTION(value)                                        \
  static_assert(                                                           \
      media::MediaKeys::value == static_cast<media::MediaKeys::Exception>( \
                                     media::mojom::CdmException::value),   \
      "Mismatched CDM Exception")
ASSERT_CDM_EXCEPTION(NOT_SUPPORTED_ERROR);
ASSERT_CDM_EXCEPTION(INVALID_STATE_ERROR);
ASSERT_CDM_EXCEPTION(INVALID_ACCESS_ERROR);
ASSERT_CDM_EXCEPTION(QUOTA_EXCEEDED_ERROR);
ASSERT_CDM_EXCEPTION(UNKNOWN_ERROR);
ASSERT_CDM_EXCEPTION(CLIENT_ERROR);
ASSERT_CDM_EXCEPTION(OUTPUT_ERROR);

// CDM Session Type
#define ASSERT_CDM_SESSION_TYPE(value)                                    \
  static_assert(                                                          \
      media::MediaKeys::value ==                                          \
          static_cast<media::MediaKeys::SessionType>(                     \
              media::mojom::ContentDecryptionModule::SessionType::value), \
      "Mismatched CDM Session Type")
ASSERT_CDM_SESSION_TYPE(TEMPORARY_SESSION);
ASSERT_CDM_SESSION_TYPE(PERSISTENT_LICENSE_SESSION);
ASSERT_CDM_SESSION_TYPE(PERSISTENT_RELEASE_MESSAGE_SESSION);

// CDM InitDataType
#define ASSERT_CDM_INIT_DATA_TYPE(value)                                   \
  static_assert(                                                           \
      media::EmeInitDataType::value ==                                     \
          static_cast<media::EmeInitDataType>(                             \
              media::mojom::ContentDecryptionModule::InitDataType::value), \
      "Mismatched CDM Init Data Type")
ASSERT_CDM_INIT_DATA_TYPE(UNKNOWN);
ASSERT_CDM_INIT_DATA_TYPE(WEBM);
ASSERT_CDM_INIT_DATA_TYPE(CENC);
ASSERT_CDM_INIT_DATA_TYPE(KEYIDS);

// CDM Key Status
#define ASSERT_CDM_KEY_STATUS(value)                                  \
  static_assert(media::CdmKeyInformation::value ==                    \
                    static_cast<media::CdmKeyInformation::KeyStatus>( \
                        media::mojom::CdmKeyStatus::value),           \
                "Mismatched CDM Key Status")
ASSERT_CDM_KEY_STATUS(USABLE);
ASSERT_CDM_KEY_STATUS(INTERNAL_ERROR);
ASSERT_CDM_KEY_STATUS(EXPIRED);
ASSERT_CDM_KEY_STATUS(OUTPUT_RESTRICTED);
ASSERT_CDM_KEY_STATUS(OUTPUT_DOWNSCALED);
ASSERT_CDM_KEY_STATUS(KEY_STATUS_PENDING);

// CDM Message Type
#define ASSERT_CDM_MESSAGE_TYPE(value)                                       \
  static_assert(                                                             \
      media::MediaKeys::value == static_cast<media::MediaKeys::MessageType>( \
                                     media::mojom::CdmMessageType::value),   \
      "Mismatched CDM Message Type")
ASSERT_CDM_MESSAGE_TYPE(LICENSE_REQUEST);
ASSERT_CDM_MESSAGE_TYPE(LICENSE_RENEWAL);
ASSERT_CDM_MESSAGE_TYPE(LICENSE_RELEASE);

template <>
struct TypeConverter<media::mojom::PatternPtr,
                     media::EncryptionScheme::Pattern> {
  static media::mojom::PatternPtr Convert(
      const media::EncryptionScheme::Pattern& input);
};
template <>
struct TypeConverter<media::EncryptionScheme::Pattern,
                     media::mojom::PatternPtr> {
  static media::EncryptionScheme::Pattern Convert(
      const media::mojom::PatternPtr& input);
};

// static
media::mojom::PatternPtr
TypeConverter<media::mojom::PatternPtr, media::EncryptionScheme::Pattern>::
    Convert(const media::EncryptionScheme::Pattern& input) {
  media::mojom::PatternPtr mojo_pattern(media::mojom::Pattern::New());
  mojo_pattern->encrypt_blocks = input.encrypt_blocks();
  mojo_pattern->skip_blocks = input.skip_blocks();
  return mojo_pattern;
}

// static
media::EncryptionScheme::Pattern TypeConverter<
    media::EncryptionScheme::Pattern,
    media::mojom::PatternPtr>::Convert(const media::mojom::PatternPtr& input) {
  return media::EncryptionScheme::Pattern(input->encrypt_blocks,
                                          input->skip_blocks);
}

// static
media::mojom::EncryptionSchemePtr TypeConverter<
    media::mojom::EncryptionSchemePtr,
    media::EncryptionScheme>::Convert(const media::EncryptionScheme& input) {
  media::mojom::EncryptionSchemePtr mojo_encryption_scheme(
      media::mojom::EncryptionScheme::New());
  mojo_encryption_scheme->mode =
      static_cast<media::mojom::CipherMode>(input.mode());
  mojo_encryption_scheme->pattern =
      media::mojom::Pattern::From(input.pattern());
  return mojo_encryption_scheme;
}

// static
media::EncryptionScheme
TypeConverter<media::EncryptionScheme, media::mojom::EncryptionSchemePtr>::
    Convert(const media::mojom::EncryptionSchemePtr& input) {
  return media::EncryptionScheme(
      static_cast<media::EncryptionScheme::CipherMode>(input->mode),
      input->pattern.To<media::EncryptionScheme::Pattern>());
}

// static
media::mojom::SubsampleEntryPtr
TypeConverter<media::mojom::SubsampleEntryPtr, media::SubsampleEntry>::Convert(
    const media::SubsampleEntry& input) {
  media::mojom::SubsampleEntryPtr mojo_subsample_entry(
      media::mojom::SubsampleEntry::New());
  mojo_subsample_entry->clear_bytes = input.clear_bytes;
  mojo_subsample_entry->cypher_bytes = input.cypher_bytes;
  return mojo_subsample_entry;
}

// static
media::SubsampleEntry
TypeConverter<media::SubsampleEntry, media::mojom::SubsampleEntryPtr>::Convert(
    const media::mojom::SubsampleEntryPtr& input) {
  return media::SubsampleEntry(input->clear_bytes, input->cypher_bytes);
}

// static
media::mojom::DecryptConfigPtr
TypeConverter<media::mojom::DecryptConfigPtr, media::DecryptConfig>::Convert(
    const media::DecryptConfig& input) {
  media::mojom::DecryptConfigPtr mojo_decrypt_config(
      media::mojom::DecryptConfig::New());
  mojo_decrypt_config->key_id = input.key_id();
  mojo_decrypt_config->iv = input.iv();
  mojo_decrypt_config->subsamples =
      Array<media::mojom::SubsampleEntryPtr>::From(input.subsamples());
  return mojo_decrypt_config;
}

// static
std::unique_ptr<media::DecryptConfig>
TypeConverter<std::unique_ptr<media::DecryptConfig>,
              media::mojom::DecryptConfigPtr>::
    Convert(const media::mojom::DecryptConfigPtr& input) {
  return base::WrapUnique(new media::DecryptConfig(
      input->key_id, input->iv,
      input->subsamples.To<std::vector<media::SubsampleEntry>>()));
}

// static
media::mojom::DecoderBufferPtr
TypeConverter<media::mojom::DecoderBufferPtr,
              scoped_refptr<media::DecoderBuffer>>::
    Convert(const scoped_refptr<media::DecoderBuffer>& input) {
  DCHECK(input);

  media::mojom::DecoderBufferPtr mojo_buffer(
      media::mojom::DecoderBuffer::New());
  if (input->end_of_stream())
    return mojo_buffer;

  mojo_buffer->timestamp_usec = input->timestamp().InMicroseconds();
  mojo_buffer->duration_usec = input->duration().InMicroseconds();
  mojo_buffer->is_key_frame = input->is_key_frame();
  mojo_buffer->data_size = base::checked_cast<uint32_t>(input->data_size());
  mojo_buffer->front_discard_usec =
      input->discard_padding().first.InMicroseconds();
  mojo_buffer->back_discard_usec =
      input->discard_padding().second.InMicroseconds();
  mojo_buffer->splice_timestamp_usec =
      input->splice_timestamp().InMicroseconds();

  // Note: The side data is always small, so this copy is okay.
  std::vector<uint8_t> side_data(input->side_data(),
                                 input->side_data() + input->side_data_size());
  mojo_buffer->side_data.Swap(&side_data);

  if (input->decrypt_config()) {
    mojo_buffer->decrypt_config =
        media::mojom::DecryptConfig::From(*input->decrypt_config());
  }

  // TODO(dalecurtis): We intentionally do not serialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return mojo_buffer;
}

// static
scoped_refptr<media::DecoderBuffer>
TypeConverter<scoped_refptr<media::DecoderBuffer>,
              media::mojom::DecoderBufferPtr>::
    Convert(const media::mojom::DecoderBufferPtr& input) {
  if (!input->data_size)
    return media::DecoderBuffer::CreateEOSBuffer();

  scoped_refptr<media::DecoderBuffer> buffer(
      new media::DecoderBuffer(input->data_size));

  if (!input->side_data.empty()) {
    buffer->CopySideDataFrom(&input->side_data.front(),
                             input->side_data.size());
  }

  buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
  buffer->set_duration(base::TimeDelta::FromMicroseconds(input->duration_usec));

  if (input->is_key_frame)
    buffer->set_is_key_frame(true);

  if (input->decrypt_config) {
    buffer->set_decrypt_config(
        input->decrypt_config.To<std::unique_ptr<media::DecryptConfig>>());
  }

  media::DecoderBuffer::DiscardPadding discard_padding(
      base::TimeDelta::FromMicroseconds(input->front_discard_usec),
      base::TimeDelta::FromMicroseconds(input->back_discard_usec));
  buffer->set_discard_padding(discard_padding);
  buffer->set_splice_timestamp(
      base::TimeDelta::FromMicroseconds(input->splice_timestamp_usec));

  // TODO(dalecurtis): We intentionally do not deserialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return buffer;
}

// static
media::mojom::AudioDecoderConfigPtr
TypeConverter<media::mojom::AudioDecoderConfigPtr, media::AudioDecoderConfig>::
    Convert(const media::AudioDecoderConfig& input) {
  media::mojom::AudioDecoderConfigPtr config(
      media::mojom::AudioDecoderConfig::New());
  config->codec = static_cast<media::mojom::AudioCodec>(input.codec());
  config->sample_format =
      static_cast<media::mojom::SampleFormat>(input.sample_format());
  config->channel_layout =
      static_cast<media::mojom::ChannelLayout>(input.channel_layout());
  config->samples_per_second = input.samples_per_second();
  if (!input.extra_data().empty()) {
    config->extra_data = mojo::Array<uint8_t>::From(input.extra_data());
  }
  config->seek_preroll_usec = input.seek_preroll().InMicroseconds();
  config->codec_delay = input.codec_delay();
  config->encryption_scheme =
      media::mojom::EncryptionScheme::From(input.encryption_scheme());
  return config;
}

// static
media::AudioDecoderConfig
TypeConverter<media::AudioDecoderConfig, media::mojom::AudioDecoderConfigPtr>::
    Convert(const media::mojom::AudioDecoderConfigPtr& input) {
  media::AudioDecoderConfig config;
  config.Initialize(static_cast<media::AudioCodec>(input->codec),
                    static_cast<media::SampleFormat>(input->sample_format),
                    static_cast<media::ChannelLayout>(input->channel_layout),
                    input->samples_per_second, input->extra_data.storage(),
                    input->encryption_scheme.To<media::EncryptionScheme>(),
                    base::TimeDelta::FromMicroseconds(input->seek_preroll_usec),
                    input->codec_delay);
  return config;
}

// static
media::mojom::VideoDecoderConfigPtr
TypeConverter<media::mojom::VideoDecoderConfigPtr, media::VideoDecoderConfig>::
    Convert(const media::VideoDecoderConfig& input) {
  media::mojom::VideoDecoderConfigPtr config(
      media::mojom::VideoDecoderConfig::New());
  config->codec = static_cast<media::mojom::VideoCodec>(input.codec());
  config->profile =
      static_cast<media::mojom::VideoCodecProfile>(input.profile());
  config->format = static_cast<media::mojom::VideoFormat>(input.format());
  config->color_space =
      static_cast<media::mojom::ColorSpace>(input.color_space());
  config->coded_size = input.coded_size();
  config->visible_rect = input.visible_rect();
  config->natural_size = input.natural_size();
  if (!input.extra_data().empty()) {
    config->extra_data = mojo::Array<uint8_t>::From(input.extra_data());
  }
  config->encryption_scheme =
      media::mojom::EncryptionScheme::From(input.encryption_scheme());
  return config;
}

// static
media::VideoDecoderConfig
TypeConverter<media::VideoDecoderConfig, media::mojom::VideoDecoderConfigPtr>::
    Convert(const media::mojom::VideoDecoderConfigPtr& input) {
  media::VideoDecoderConfig config;
  config.Initialize(static_cast<media::VideoCodec>(input->codec),
                    static_cast<media::VideoCodecProfile>(input->profile),
                    static_cast<media::VideoPixelFormat>(input->format),
                    static_cast<media::ColorSpace>(input->color_space),
                    input->coded_size, input->visible_rect, input->natural_size,
                    input->extra_data.storage(),
                    input->encryption_scheme.To<media::EncryptionScheme>());
  return config;
}

// static
media::mojom::CdmKeyInformationPtr TypeConverter<
    media::mojom::CdmKeyInformationPtr,
    media::CdmKeyInformation>::Convert(const media::CdmKeyInformation& input) {
  media::mojom::CdmKeyInformationPtr info(
      media::mojom::CdmKeyInformation::New());
  std::vector<uint8_t> key_id_copy(input.key_id);
  info->key_id.Swap(&key_id_copy);
  info->status = static_cast<media::mojom::CdmKeyStatus>(input.status);
  info->system_code = input.system_code;
  return info;
}

// static
std::unique_ptr<media::CdmKeyInformation>
TypeConverter<std::unique_ptr<media::CdmKeyInformation>,
              media::mojom::CdmKeyInformationPtr>::
    Convert(const media::mojom::CdmKeyInformationPtr& input) {
  return base::WrapUnique(new media::CdmKeyInformation(
      input->key_id.storage(),
      static_cast<media::CdmKeyInformation::KeyStatus>(input->status),
      input->system_code));
}

// static
media::mojom::CdmConfigPtr
TypeConverter<media::mojom::CdmConfigPtr, media::CdmConfig>::Convert(
    const media::CdmConfig& input) {
  media::mojom::CdmConfigPtr config(media::mojom::CdmConfig::New());
  config->allow_distinctive_identifier = input.allow_distinctive_identifier;
  config->allow_persistent_state = input.allow_persistent_state;
  config->use_hw_secure_codecs = input.use_hw_secure_codecs;
  return config;
}

// static
media::CdmConfig
TypeConverter<media::CdmConfig, media::mojom::CdmConfigPtr>::Convert(
    const media::mojom::CdmConfigPtr& input) {
  media::CdmConfig config;
  config.allow_distinctive_identifier = input->allow_distinctive_identifier;
  config.allow_persistent_state = input->allow_persistent_state;
  config.use_hw_secure_codecs = input->use_hw_secure_codecs;
  return config;
}

// static
media::mojom::AudioBufferPtr
TypeConverter<media::mojom::AudioBufferPtr, scoped_refptr<media::AudioBuffer>>::
    Convert(const scoped_refptr<media::AudioBuffer>& input) {
  media::mojom::AudioBufferPtr buffer(media::mojom::AudioBuffer::New());
  buffer->sample_format =
      static_cast<media::mojom::SampleFormat>(input->sample_format_);
  buffer->channel_layout =
      static_cast<media::mojom::ChannelLayout>(input->channel_layout());
  buffer->channel_count = input->channel_count();
  buffer->sample_rate = input->sample_rate();
  buffer->frame_count = input->frame_count();
  buffer->end_of_stream = input->end_of_stream();
  buffer->timestamp_usec = input->timestamp().InMicroseconds();

  if (!input->end_of_stream()) {
    std::vector<uint8_t> input_data(input->data_.get(),
                                    input->data_.get() + input->data_size_);
    buffer->data.Swap(&input_data);
  }

  return buffer;
}

// static
scoped_refptr<media::AudioBuffer>
TypeConverter<scoped_refptr<media::AudioBuffer>, media::mojom::AudioBufferPtr>::
    Convert(const media::mojom::AudioBufferPtr& input) {
  if (input->end_of_stream)
    return media::AudioBuffer::CreateEOSBuffer();

  // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
  // one in the case of interleaved data.
  std::vector<const uint8_t*> channel_ptrs(input->channel_count, nullptr);
  std::vector<uint8_t> storage = input->data.storage();
  const size_t size_per_channel = storage.size() / input->channel_count;
  DCHECK_EQ(0u, storage.size() % input->channel_count);
  for (int i = 0; i < input->channel_count; ++i)
    channel_ptrs[i] = storage.data() + i * size_per_channel;

  return media::AudioBuffer::CopyFrom(
      static_cast<media::SampleFormat>(input->sample_format),
      static_cast<media::ChannelLayout>(input->channel_layout),
      input->channel_count, input->sample_rate, input->frame_count,
      &channel_ptrs[0],
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
}

// static
media::mojom::VideoFramePtr
TypeConverter<media::mojom::VideoFramePtr, scoped_refptr<media::VideoFrame>>::
    Convert(const scoped_refptr<media::VideoFrame>& input) {
  media::mojom::VideoFramePtr frame(media::mojom::VideoFrame::New());
  frame->end_of_stream =
      input->metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM);
  if (frame->end_of_stream)
    return frame;

  // Handle non EOS frame. It must be a MojoSharedBufferVideoFrame.
  // TODO(jrummell): Support other types of VideoFrame.
  CHECK_EQ(media::VideoFrame::STORAGE_MOJO_SHARED_BUFFER,
           input->storage_type());
  media::MojoSharedBufferVideoFrame* input_frame =
      static_cast<media::MojoSharedBufferVideoFrame*>(input.get());
  mojo::ScopedSharedBufferHandle duplicated_handle =
      input_frame->Handle().Clone();
  CHECK(duplicated_handle.is_valid());

  frame->format = static_cast<media::mojom::VideoFormat>(input->format());
  frame->coded_size = input->coded_size();
  frame->visible_rect = input->visible_rect();
  frame->natural_size = input->natural_size();
  frame->timestamp_usec = input->timestamp().InMicroseconds();
  frame->frame_data = std::move(duplicated_handle);
  frame->frame_data_size = input_frame->MappedSize();
  frame->y_stride = input_frame->stride(media::VideoFrame::kYPlane);
  frame->u_stride = input_frame->stride(media::VideoFrame::kUPlane);
  frame->v_stride = input_frame->stride(media::VideoFrame::kVPlane);
  frame->y_offset = input_frame->PlaneOffset(media::VideoFrame::kYPlane);
  frame->u_offset = input_frame->PlaneOffset(media::VideoFrame::kUPlane);
  frame->v_offset = input_frame->PlaneOffset(media::VideoFrame::kVPlane);
  return frame;
}

// static
scoped_refptr<media::VideoFrame>
TypeConverter<scoped_refptr<media::VideoFrame>, media::mojom::VideoFramePtr>::
    Convert(const media::mojom::VideoFramePtr& input) {
  if (input->end_of_stream)
    return media::VideoFrame::CreateEOSFrame();

  return media::MojoSharedBufferVideoFrame::Create(
      static_cast<media::VideoPixelFormat>(input->format), input->coded_size,
      input->visible_rect, input->natural_size, std::move(input->frame_data),
      base::saturated_cast<size_t>(input->frame_data_size),
      base::saturated_cast<size_t>(input->y_offset),
      base::saturated_cast<size_t>(input->u_offset),
      base::saturated_cast<size_t>(input->v_offset), input->y_stride,
      input->u_stride, input->v_stride,
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
}

}  // namespace mojo
