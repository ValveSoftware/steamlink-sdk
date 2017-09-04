// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#define MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_

#include "ipc/ipc_message_macros.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/channel_layout.h"
#include "media/base/decode_status.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_keys.h"
#include "media/base/sample_format.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"

// Enum traits.

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioCodec, media::AudioCodec::kAudioCodecMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioLatency::LatencyType,
                          media::AudioLatency::LATENCY_COUNT)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioParameters::Format,
                          media::AudioParameters::AUDIO_FORMAT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::BufferingState,
                          media::BufferingState::BUFFERING_STATE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::CdmKeyInformation::KeyStatus,
                          media::CdmKeyInformation::KEY_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::ChannelLayout, media::CHANNEL_LAYOUT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::ColorSpace, media::COLOR_SPACE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::DecodeStatus,
                          media::DecodeStatus::DECODE_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::Decryptor::Status,
                          media::Decryptor::Status::kStatusMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::Decryptor::StreamType,
                          media::Decryptor::StreamType::kStreamTypeMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::DemuxerStream::Status,
                          media::DemuxerStream::kStatusMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::DemuxerStream::Type,
                          media::DemuxerStream::TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::EmeInitDataType, media::EmeInitDataType::MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::EncryptionScheme::CipherMode,
                          media::EncryptionScheme::CipherMode::CIPHER_MODE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::CdmPromise::Exception,
                          media::CdmPromise::EXCEPTION_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaKeys::MessageType,
                          media::MediaKeys::MESSAGE_TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaKeys::SessionType,
                          media::MediaKeys::SESSION_TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::SampleFormat, media::kSampleFormatMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoCodec, media::kVideoCodecMax)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelFormat, media::PIXEL_FORMAT_MAX)

// Struct traits.

IPC_STRUCT_TRAITS_BEGIN(media::CdmKeyInformation)
  IPC_STRUCT_TRAITS_MEMBER(key_id)
  IPC_STRUCT_TRAITS_MEMBER(status)
  IPC_STRUCT_TRAITS_MEMBER(system_code)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

#endif  // MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
