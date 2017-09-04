// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RPC_PROTO_ENUM_UTILS_H_
#define MEDIA_REMOTING_RPC_PROTO_ENUM_UTILS_H_

#include "base/optional.h"
#include "media/base/audio_codecs.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_keys.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/remoting/remoting_rpc_message.pb.h"

namespace media {
namespace remoting {

// The following functions map between the enum values in media/base modules and
// the equivalents in the media/remoting protobuf classes. The purpose of these
// converters is to decouple the media/base modules from the media/remoting
// modules while maintaining compile-time checks to ensure that there are always
// valid, backwards-compatible mappings between the two.
//
// Each returns a base::Optional value. If it is not set, that indicates the
// conversion failed.

base::Optional<::media::EncryptionScheme::CipherMode>
ToMediaEncryptionSchemeCipherMode(pb::EncryptionScheme::CipherMode value);
base::Optional<pb::EncryptionScheme::CipherMode>
ToProtoEncryptionSchemeCipherMode(::media::EncryptionScheme::CipherMode value);

base::Optional<::media::AudioCodec> ToMediaAudioCodec(
    pb::AudioDecoderConfig::Codec value);
base::Optional<pb::AudioDecoderConfig::Codec> ToProtoAudioDecoderConfigCodec(
    ::media::AudioCodec value);

base::Optional<::media::SampleFormat> ToMediaSampleFormat(
    pb::AudioDecoderConfig::SampleFormat value);
base::Optional<pb::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(::media::SampleFormat value);

base::Optional<::media::ChannelLayout> ToMediaChannelLayout(
    pb::AudioDecoderConfig::ChannelLayout value);
base::Optional<pb::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(::media::ChannelLayout value);

base::Optional<::media::VideoCodec> ToMediaVideoCodec(
    pb::VideoDecoderConfig::Codec value);
base::Optional<pb::VideoDecoderConfig::Codec> ToProtoVideoDecoderConfigCodec(
    ::media::VideoCodec value);

base::Optional<::media::VideoCodecProfile> ToMediaVideoCodecProfile(
    pb::VideoDecoderConfig::Profile value);
base::Optional<pb::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(::media::VideoCodecProfile value);

base::Optional<::media::VideoPixelFormat> ToMediaVideoPixelFormat(
    pb::VideoDecoderConfig::Format value);
base::Optional<pb::VideoDecoderConfig::Format> ToProtoVideoDecoderConfigFormat(
    ::media::VideoPixelFormat value);

base::Optional<::media::ColorSpace> ToMediaColorSpace(
    pb::VideoDecoderConfig::ColorSpace value);
base::Optional<pb::VideoDecoderConfig::ColorSpace>
ToProtoVideoDecoderConfigColorSpace(::media::ColorSpace value);

base::Optional<::media::BufferingState> ToMediaBufferingState(
    pb::RendererClientOnBufferingStateChange::State value);
base::Optional<pb::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(::media::BufferingState value);

base::Optional<::media::CdmKeyInformation::KeyStatus>
ToMediaCdmKeyInformationKeyStatus(pb::CdmKeyInformation::KeyStatus value);
base::Optional<pb::CdmKeyInformation::KeyStatus> ToProtoCdmKeyInformation(
    ::media::CdmKeyInformation::KeyStatus value);

base::Optional<::media::CdmPromise::Exception> ToCdmPromiseException(
    pb::MediaKeysException value);
base::Optional<pb::MediaKeysException> ToProtoMediaKeysException(
    ::media::CdmPromise::Exception value);

base::Optional<::media::MediaKeys::MessageType> ToMediaMediaKeysMessageType(
    pb::MediaKeysMessageType value);
base::Optional<pb::MediaKeysMessageType> ToProtoMediaKeysMessageType(
    ::media::MediaKeys::MessageType value);

base::Optional<::media::MediaKeys::SessionType> ToMediaKeysSessionType(
    pb::MediaKeysSessionType value);
base::Optional<pb::MediaKeysSessionType> ToProtoMediaKeysSessionType(
    ::media::MediaKeys::SessionType value);

base::Optional<::media::EmeInitDataType> ToMediaEmeInitDataType(
    pb::CdmCreateSessionAndGenerateRequest::EmeInitDataType value);
base::Optional<pb::CdmCreateSessionAndGenerateRequest::EmeInitDataType>
ToProtoMediaEmeInitDataType(::media::EmeInitDataType value);

base::Optional<::media::DemuxerStream::Status> ToDemuxerStreamStatus(
    pb::DemuxerStreamReadUntilCallback::Status value);
base::Optional<pb::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(::media::DemuxerStream::Status value);

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RPC_PROTO_ENUM_UTILS_H_
