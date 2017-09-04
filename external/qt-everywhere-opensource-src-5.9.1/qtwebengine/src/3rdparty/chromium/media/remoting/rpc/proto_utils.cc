// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/rpc/proto_utils.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/base/encryption_scheme.h"
#include "media/remoting/rpc/proto_enum_utils.h"

namespace media {
namespace remoting {

namespace {

constexpr size_t kPayloadVersionFieldSize = sizeof(uint8_t);
constexpr size_t kProtoBufferHeaderSize = sizeof(uint16_t);
constexpr size_t kDataBufferHeaderSize = sizeof(uint32_t);

std::unique_ptr<::media::DecryptConfig> ConvertProtoToDecryptConfig(
    const pb::DecryptConfig& config_message) {
  if (!config_message.has_key_id())
    return nullptr;
  if (!config_message.has_iv())
    return nullptr;

  std::vector<::media::SubsampleEntry> entries(
      config_message.sub_samples_size());
  for (int i = 0; i < config_message.sub_samples_size(); ++i) {
    entries.push_back(
        ::media::SubsampleEntry(config_message.sub_samples(i).clear_bytes(),
                                config_message.sub_samples(i).cypher_bytes()));
  }

  std::unique_ptr<::media::DecryptConfig> decrypt_config(
      new ::media::DecryptConfig(config_message.key_id(), config_message.iv(),
                                 entries));
  return decrypt_config;
}

scoped_refptr<::media::DecoderBuffer> ConvertProtoToDecoderBuffer(
    const pb::DecoderBuffer& buffer_message,
    scoped_refptr<::media::DecoderBuffer> buffer) {
  if (buffer_message.is_eos()) {
    VLOG(1) << "EOS data";
    return ::media::DecoderBuffer::CreateEOSBuffer();
  }

  if (buffer_message.has_timestamp_usec()) {
    buffer->set_timestamp(
        base::TimeDelta::FromMicroseconds(buffer_message.timestamp_usec()));
  }

  if (buffer_message.has_duration_usec()) {
    buffer->set_duration(
        base::TimeDelta::FromMicroseconds(buffer_message.duration_usec()));
  }
  VLOG(3) << "timestamp:" << buffer_message.timestamp_usec()
          << " duration:" << buffer_message.duration_usec();

  if (buffer_message.has_is_key_frame())
    buffer->set_is_key_frame(buffer_message.is_key_frame());

  if (buffer_message.has_decrypt_config()) {
    buffer->set_decrypt_config(
        ConvertProtoToDecryptConfig(buffer_message.decrypt_config()));
  }

  bool has_discard = false;
  base::TimeDelta front_discard;
  if (buffer_message.has_front_discard_usec()) {
    has_discard = true;
    front_discard =
        base::TimeDelta::FromMicroseconds(buffer_message.front_discard_usec());
  }
  base::TimeDelta back_discard;
  if (buffer_message.has_back_discard_usec()) {
    has_discard = true;
    back_discard =
        base::TimeDelta::FromMicroseconds(buffer_message.back_discard_usec());
  }

  if (has_discard) {
    buffer->set_discard_padding(
        ::media::DecoderBuffer::DiscardPadding(front_discard, back_discard));
  }

  if (buffer_message.has_splice_timestamp_usec()) {
    buffer->set_splice_timestamp(base::TimeDelta::FromMicroseconds(
        buffer_message.splice_timestamp_usec()));
  }

  if (buffer_message.has_side_data()) {
    buffer->CopySideDataFrom(
        reinterpret_cast<const uint8_t*>(buffer_message.side_data().data()),
        buffer_message.side_data().size());
  }

  return buffer;
}

void ConvertDecryptConfigToProto(const ::media::DecryptConfig& decrypt_config,
                                 pb::DecryptConfig* config_message) {
  DCHECK(config_message);

  config_message->set_key_id(decrypt_config.key_id());
  config_message->set_iv(decrypt_config.iv());

  for (const auto& entry : decrypt_config.subsamples()) {
    pb::DecryptConfig::SubSample* sub_sample =
        config_message->add_sub_samples();
    sub_sample->set_clear_bytes(entry.clear_bytes);
    sub_sample->set_cypher_bytes(entry.cypher_bytes);
  }
}

void ConvertDecoderBufferToProto(
    const scoped_refptr<::media::DecoderBuffer>& decoder_buffer,
    pb::DecoderBuffer* buffer_message) {
  if (decoder_buffer->end_of_stream()) {
    buffer_message->set_is_eos(true);
    return;
  }

  VLOG(3) << "timestamp:" << decoder_buffer->timestamp().InMicroseconds()
          << " duration:" << decoder_buffer->duration().InMicroseconds();
  buffer_message->set_timestamp_usec(
      decoder_buffer->timestamp().InMicroseconds());
  buffer_message->set_duration_usec(
      decoder_buffer->duration().InMicroseconds());
  buffer_message->set_is_key_frame(decoder_buffer->is_key_frame());

  if (decoder_buffer->decrypt_config()) {
    ConvertDecryptConfigToProto(*decoder_buffer->decrypt_config(),
                                buffer_message->mutable_decrypt_config());
  }

  buffer_message->set_front_discard_usec(
      decoder_buffer->discard_padding().first.InMicroseconds());
  buffer_message->set_back_discard_usec(
      decoder_buffer->discard_padding().second.InMicroseconds());
  buffer_message->set_splice_timestamp_usec(
      decoder_buffer->splice_timestamp().InMicroseconds());

  if (decoder_buffer->side_data_size()) {
    buffer_message->set_side_data(decoder_buffer->side_data(),
                                  decoder_buffer->side_data_size());
  }
}

}  // namespace

scoped_refptr<::media::DecoderBuffer> ByteArrayToDecoderBuffer(
    const uint8_t* data,
    uint32_t size) {
  base::BigEndianReader reader(reinterpret_cast<const char*>(data), size);
  uint8_t payload_version = 0;
  uint16_t proto_size = 0;
  pb::DecoderBuffer segment;
  uint32_t buffer_size = 0;
  if (reader.ReadU8(&payload_version) && payload_version == 0 &&
      reader.ReadU16(&proto_size) &&
      static_cast<int>(proto_size) < reader.remaining() &&
      segment.ParseFromArray(reader.ptr(), proto_size) &&
      reader.Skip(proto_size) && reader.ReadU32(&buffer_size) &&
      static_cast<int64_t>(buffer_size) <= reader.remaining()) {
    // Deserialize proto buffer. It passes the pre allocated DecoderBuffer into
    // the function because the proto buffer may overwrite DecoderBuffer since
    // it may be EOS buffer.
    scoped_refptr<media::DecoderBuffer> decoder_buffer =
        ConvertProtoToDecoderBuffer(
            segment,
            DecoderBuffer::CopyFrom(
                reinterpret_cast<const uint8_t*>(reader.ptr()), buffer_size));
    return decoder_buffer;
  }

  LOG(ERROR) << "Not able to convert byte array to ::media::DecoderBuffer";
  return nullptr;
}

std::vector<uint8_t> DecoderBufferToByteArray(
    const scoped_refptr<::media::DecoderBuffer>& decoder_buffer) {
  pb::DecoderBuffer decoder_buffer_message;
  ConvertDecoderBufferToProto(decoder_buffer, &decoder_buffer_message);

  size_t decoder_buffer_size =
      decoder_buffer->end_of_stream() ? 0 : decoder_buffer->data_size();
  size_t size = kPayloadVersionFieldSize + kProtoBufferHeaderSize +
                decoder_buffer_message.ByteSize() + kDataBufferHeaderSize +
                decoder_buffer_size;
  std::vector<uint8_t> buffer(size);
  base::BigEndianWriter writer(reinterpret_cast<char*>(buffer.data()),
                               buffer.size());
  if (writer.WriteU8(0) &&
      writer.WriteU16(
          static_cast<uint16_t>(decoder_buffer_message.GetCachedSize())) &&
      decoder_buffer_message.SerializeToArray(
          writer.ptr(), decoder_buffer_message.GetCachedSize()) &&
      writer.Skip(decoder_buffer_message.GetCachedSize()) &&
      writer.WriteU32(decoder_buffer_size)) {
    if (decoder_buffer_size) {
      // DecoderBuffer frame data.
      writer.WriteBytes(reinterpret_cast<const void*>(decoder_buffer->data()),
                        decoder_buffer->data_size());
    }
    return buffer;
  }

  // Reset buffer since serialization of the data failed.
  LOG(ERROR) << "Not able to convert ::media::DecoderBuffer to byte array";
  buffer.clear();
  return buffer;
}

void ConvertEncryptionSchemeToProto(
    const ::media::EncryptionScheme& encryption_scheme,
    pb::EncryptionScheme* message) {
  DCHECK(message);
  message->set_mode(
      ToProtoEncryptionSchemeCipherMode(encryption_scheme.mode()).value());
  message->set_encrypt_blocks(encryption_scheme.pattern().encrypt_blocks());
  message->set_skip_blocks(encryption_scheme.pattern().skip_blocks());
}

::media::EncryptionScheme ConvertProtoToEncryptionScheme(
    const pb::EncryptionScheme& message) {
  return ::media::EncryptionScheme(
      ToMediaEncryptionSchemeCipherMode(message.mode()).value(),
      ::media::EncryptionScheme::Pattern(message.encrypt_blocks(),
                                         message.skip_blocks()));
}

void ConvertAudioDecoderConfigToProto(
    const ::media::AudioDecoderConfig& audio_config,
    pb::AudioDecoderConfig* audio_message) {
  DCHECK(audio_config.IsValidConfig());
  DCHECK(audio_message);

  audio_message->set_codec(
      ToProtoAudioDecoderConfigCodec(audio_config.codec()).value());
  audio_message->set_sample_format(
      ToProtoAudioDecoderConfigSampleFormat(audio_config.sample_format())
          .value());
  audio_message->set_channel_layout(
      ToProtoAudioDecoderConfigChannelLayout(audio_config.channel_layout())
          .value());
  audio_message->set_samples_per_second(audio_config.samples_per_second());
  audio_message->set_seek_preroll_usec(
      audio_config.seek_preroll().InMicroseconds());
  audio_message->set_codec_delay(audio_config.codec_delay());

  if (!audio_config.extra_data().empty()) {
    audio_message->set_extra_data(audio_config.extra_data().data(),
                                  audio_config.extra_data().size());
  }

  if (audio_config.is_encrypted()) {
    pb::EncryptionScheme* encryption_scheme_message =
        audio_message->mutable_encryption_scheme();
    ConvertEncryptionSchemeToProto(audio_config.encryption_scheme(),
                                   encryption_scheme_message);
  }
}

bool ConvertProtoToAudioDecoderConfig(
    const pb::AudioDecoderConfig& audio_message,
    ::media::AudioDecoderConfig* audio_config) {
  DCHECK(audio_config);
  audio_config->Initialize(
      ToMediaAudioCodec(audio_message.codec()).value(),
      ToMediaSampleFormat(audio_message.sample_format()).value(),
      ToMediaChannelLayout(audio_message.channel_layout()).value(),
      audio_message.samples_per_second(),
      std::vector<uint8_t>(audio_message.extra_data().begin(),
                           audio_message.extra_data().end()),
      ConvertProtoToEncryptionScheme(audio_message.encryption_scheme()),
      base::TimeDelta::FromMicroseconds(audio_message.seek_preroll_usec()),
      audio_message.codec_delay());
  return audio_config->IsValidConfig();
}

void ConvertVideoDecoderConfigToProto(
    const ::media::VideoDecoderConfig& video_config,
    pb::VideoDecoderConfig* video_message) {
  DCHECK(video_config.IsValidConfig());
  DCHECK(video_message);

  video_message->set_codec(
      ToProtoVideoDecoderConfigCodec(video_config.codec()).value());
  video_message->set_profile(
      ToProtoVideoDecoderConfigProfile(video_config.profile()).value());
  video_message->set_format(
      ToProtoVideoDecoderConfigFormat(video_config.format()).value());
  video_message->set_color_space(
      ToProtoVideoDecoderConfigColorSpace(video_config.color_space()).value());

  pb::Size* coded_size_message = video_message->mutable_coded_size();
  coded_size_message->set_width(video_config.coded_size().width());
  coded_size_message->set_height(video_config.coded_size().height());

  pb::Rect* visible_rect_message = video_message->mutable_visible_rect();
  visible_rect_message->set_x(video_config.visible_rect().x());
  visible_rect_message->set_y(video_config.visible_rect().y());
  visible_rect_message->set_width(video_config.visible_rect().width());
  visible_rect_message->set_height(video_config.visible_rect().height());

  pb::Size* natural_size_message = video_message->mutable_natural_size();
  natural_size_message->set_width(video_config.natural_size().width());
  natural_size_message->set_height(video_config.natural_size().height());

  if (!video_config.extra_data().empty()) {
    video_message->set_extra_data(video_config.extra_data().data(),
                                  video_config.extra_data().size());
  }

  if (video_config.is_encrypted()) {
    pb::EncryptionScheme* encryption_scheme_message =
        video_message->mutable_encryption_scheme();
    ConvertEncryptionSchemeToProto(video_config.encryption_scheme(),
                                   encryption_scheme_message);
  }
}

bool ConvertProtoToVideoDecoderConfig(
    const pb::VideoDecoderConfig& video_message,
    ::media::VideoDecoderConfig* video_config) {
  DCHECK(video_config);
  ::media::EncryptionScheme encryption_scheme;
  video_config->Initialize(
      ToMediaVideoCodec(video_message.codec()).value(),
      ToMediaVideoCodecProfile(video_message.profile()).value(),
      ToMediaVideoPixelFormat(video_message.format()).value(),
      ToMediaColorSpace(video_message.color_space()).value(),
      gfx::Size(video_message.coded_size().width(),
                video_message.coded_size().height()),
      gfx::Rect(video_message.visible_rect().x(),
                video_message.visible_rect().y(),
                video_message.visible_rect().width(),
                video_message.visible_rect().height()),
      gfx::Size(video_message.natural_size().width(),
                video_message.natural_size().height()),
      std::vector<uint8_t>(video_message.extra_data().begin(),
                           video_message.extra_data().end()),
      ConvertProtoToEncryptionScheme(video_message.encryption_scheme()));
  return video_config->IsValidConfig();
}

void ConvertCdmKeyInfoToProto(
    const ::media::CdmKeysInfo& keys_information,
    pb::CdmClientOnSessionKeysChange* key_change_message) {
  for (const auto& info : keys_information) {
    pb::CdmKeyInformation* key = key_change_message->add_key_information();
    key->set_key_id(info->key_id.data(), info->key_id.size());
    key->set_status(ToProtoCdmKeyInformation(info->status).value());
    key->set_system_code(info->system_code);
  }
}

void ConvertProtoToCdmKeyInfo(
    const pb::CdmClientOnSessionKeysChange keychange_message,
    CdmKeysInfo* key_information) {
  DCHECK(key_information);
  key_information->reserve(keychange_message.key_information_size());
  for (int i = 0; i < keychange_message.key_information_size(); ++i) {
    const pb::CdmKeyInformation key_info_msg =
        keychange_message.key_information(i);

    std::unique_ptr<::media::CdmKeyInformation> key(
        new ::media::CdmKeyInformation(
            key_info_msg.key_id(),
            ToMediaCdmKeyInformationKeyStatus(key_info_msg.status()).value(),
            key_info_msg.system_code()));
    key_information->push_back(std::move(key));
  }
}

void ConvertCdmPromiseToProto(const CdmPromiseResult& result,
                              pb::CdmPromise* promise_message) {
  promise_message->set_success(result.success());
  if (!result.success()) {
    promise_message->set_exception(
        ToProtoMediaKeysException(result.exception()).value());
    promise_message->set_system_code(result.system_code());
    promise_message->set_error_message(result.error_message());
  }
}

void ConvertCdmPromiseWithSessionIdToProto(const CdmPromiseResult& result,
                                           const std::string& session_id,
                                           pb::CdmPromise* promise_message) {
  ConvertCdmPromiseToProto(result, promise_message);
  promise_message->set_session_id(session_id);
}

void ConvertCdmPromiseWithCdmIdToProto(const CdmPromiseResult& result,
                                       int cdm_id,
                                       pb::CdmPromise* promise_message) {
  ConvertCdmPromiseToProto(result, promise_message);
  promise_message->set_cdm_id(cdm_id);
}

bool ConvertProtoToCdmPromise(const pb::CdmPromise& promise_message,
                              CdmPromiseResult* result) {
  if (!promise_message.has_success())
    return false;

  bool success = promise_message.success();
  if (success) {
    *result = CdmPromiseResult::SuccessResult();
    return true;
  }

  ::media::CdmPromise::Exception exception = ::media::CdmPromise::UNKNOWN_ERROR;
  uint32_t system_code = 0;
  std::string error_message;

  exception = ToCdmPromiseException(promise_message.exception()).value();
  system_code = promise_message.system_code();
  error_message = promise_message.error_message();
  *result = CdmPromiseResult(exception, system_code, error_message);
  return true;
}

bool ConvertProtoToCdmPromiseWithCdmIdSessionId(const pb::RpcMessage& message,
                                                CdmPromiseResult* result,
                                                int* cdm_id,
                                                std::string* session_id) {
  if (!message.has_cdm_promise_rpc())
    return false;

  const auto& promise_message = message.cdm_promise_rpc();
  if (!ConvertProtoToCdmPromise(promise_message, result))
    return false;

  if (cdm_id)
    *cdm_id = promise_message.cdm_id();
  if (session_id)
    *session_id = promise_message.session_id();

  return true;
}

//==============================================================================
CdmPromiseResult::CdmPromiseResult()
    : CdmPromiseResult(::media::CdmPromise::UNKNOWN_ERROR, 0, "") {}

CdmPromiseResult::CdmPromiseResult(::media::CdmPromise::Exception exception,
                                   uint32_t system_code,
                                   std::string error_message)
    : success_(false),
      exception_(exception),
      system_code_(system_code),
      error_message_(error_message) {}

CdmPromiseResult::CdmPromiseResult(const CdmPromiseResult& other) = default;

CdmPromiseResult::~CdmPromiseResult() = default;

CdmPromiseResult CdmPromiseResult::SuccessResult() {
  CdmPromiseResult result(static_cast<::media::CdmPromise::Exception>(0), 0,
                          "");
  result.success_ = true;
  return result;
}

}  // namespace remoting
}  // namespace media
