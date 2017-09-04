// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_MEDIA_TYPE_CONVERTERS_H_
#define MEDIA_MOJO_COMMON_MEDIA_TYPE_CONVERTERS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace media {
class AudioBuffer;
class AudioDecoderConfig;
class DecoderBuffer;
class DecryptConfig;
class EncryptionScheme;
class VideoDecoderConfig;
class VideoFrame;
struct CdmConfig;
struct CdmKeyInformation;
}

// These are specializations of mojo::TypeConverter and have to be in the mojo
// namespace.
namespace mojo {

template <>
struct TypeConverter<media::mojom::EncryptionSchemePtr,
                     media::EncryptionScheme> {
  static media::mojom::EncryptionSchemePtr Convert(
      const media::EncryptionScheme& input);
};
template <>
struct TypeConverter<media::EncryptionScheme,
                     media::mojom::EncryptionSchemePtr> {
  static media::EncryptionScheme Convert(
      const media::mojom::EncryptionSchemePtr& input);
};

template <>
struct TypeConverter<media::mojom::DecryptConfigPtr, media::DecryptConfig> {
  static media::mojom::DecryptConfigPtr Convert(
      const media::DecryptConfig& input);
};
template <>
struct TypeConverter<std::unique_ptr<media::DecryptConfig>,
                     media::mojom::DecryptConfigPtr> {
  static std::unique_ptr<media::DecryptConfig> Convert(
      const media::mojom::DecryptConfigPtr& input);
};

template <>
struct TypeConverter<media::mojom::DecoderBufferPtr,
                     scoped_refptr<media::DecoderBuffer>> {
  static media::mojom::DecoderBufferPtr Convert(
      const scoped_refptr<media::DecoderBuffer>& input);
};
template <>
struct TypeConverter<scoped_refptr<media::DecoderBuffer>,
                     media::mojom::DecoderBufferPtr> {
  static scoped_refptr<media::DecoderBuffer> Convert(
      const media::mojom::DecoderBufferPtr& input);
};

template <>
struct TypeConverter<media::mojom::AudioDecoderConfigPtr,
                     media::AudioDecoderConfig> {
  static media::mojom::AudioDecoderConfigPtr Convert(
      const media::AudioDecoderConfig& input);
};
template <>
struct TypeConverter<media::AudioDecoderConfig,
                     media::mojom::AudioDecoderConfigPtr> {
  static media::AudioDecoderConfig Convert(
      const media::mojom::AudioDecoderConfigPtr& input);
};

template <>
struct TypeConverter<media::mojom::VideoDecoderConfigPtr,
                     media::VideoDecoderConfig> {
  static media::mojom::VideoDecoderConfigPtr Convert(
      const media::VideoDecoderConfig& input);
};
template <>
struct TypeConverter<media::VideoDecoderConfig,
                     media::mojom::VideoDecoderConfigPtr> {
  static media::VideoDecoderConfig Convert(
      const media::mojom::VideoDecoderConfigPtr& input);
};

template <>
struct TypeConverter<media::mojom::CdmKeyInformationPtr,
                     media::CdmKeyInformation> {
  static media::mojom::CdmKeyInformationPtr Convert(
      const media::CdmKeyInformation& input);
};
template <>
struct TypeConverter<std::unique_ptr<media::CdmKeyInformation>,
                     media::mojom::CdmKeyInformationPtr> {
  static std::unique_ptr<media::CdmKeyInformation> Convert(
      const media::mojom::CdmKeyInformationPtr& input);
};

template <>
struct TypeConverter<media::mojom::CdmConfigPtr, media::CdmConfig> {
  static media::mojom::CdmConfigPtr Convert(const media::CdmConfig& input);
};
template <>
struct TypeConverter<media::CdmConfig, media::mojom::CdmConfigPtr> {
  static media::CdmConfig Convert(const media::mojom::CdmConfigPtr& input);
};

template <>
struct TypeConverter<media::mojom::AudioBufferPtr,
                     scoped_refptr<media::AudioBuffer>> {
  static media::mojom::AudioBufferPtr Convert(
      const scoped_refptr<media::AudioBuffer>& input);
};
template <>
struct TypeConverter<scoped_refptr<media::AudioBuffer>,
                     media::mojom::AudioBufferPtr> {
  static scoped_refptr<media::AudioBuffer> Convert(
      const media::mojom::AudioBufferPtr& input);
};

template <>
struct TypeConverter<media::mojom::VideoFramePtr,
                     scoped_refptr<media::VideoFrame>> {
  static media::mojom::VideoFramePtr Convert(
      const scoped_refptr<media::VideoFrame>& input);
};
template <>
struct TypeConverter<scoped_refptr<media::VideoFrame>,
                     media::mojom::VideoFramePtr> {
  static scoped_refptr<media::VideoFrame> Convert(
      const media::mojom::VideoFramePtr& input);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_COMMON_MEDIA_TYPE_CONVERTERS_H_
