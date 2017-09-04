// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MIME_UTIL_INTERNAL_H_
#define MEDIA_BASE_MIME_UTIL_INTERNAL_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"

namespace media {
namespace internal {

// Internal utility class for handling mime types.  Should only be invoked by
// tests and the functions within mime_util.cc -- NOT for direct use by others.
class MEDIA_EXPORT MimeUtil {
 public:
  MimeUtil();
  ~MimeUtil();

  enum Codec {
    INVALID_CODEC,
    PCM,
    MP3,
    AC3,
    EAC3,
    MPEG2_AAC,
    MPEG4_AAC,
    VORBIS,
    OPUS,
    FLAC,
    H264,
    HEVC,
    VP8,
    VP9,
    THEORA,
    LAST_CODEC = THEORA
  };

  // Platform configuration structure.  Controls which codecs are supported at
  // runtime.  Also used by tests to simulate platform differences.
  struct PlatformInfo {
    bool has_platform_decoders = false;

    bool has_platform_vp8_decoder = false;
    bool has_platform_vp9_decoder = false;
    bool supports_opus = false;

    bool is_unified_media_pipeline_enabled = false;
  };

  // See mime_util.h for more information on these methods.
  bool IsSupportedMediaMimeType(const std::string& mime_type) const;
  void ParseCodecString(const std::string& codecs,
                        std::vector<std::string>* codecs_out,
                        bool strip);
  SupportsType IsSupportedMediaFormat(const std::string& mime_type,
                                      const std::vector<std::string>& codecs,
                                      bool is_encrypted) const;

  void RemoveProprietaryMediaTypesAndCodecs();

  // Checks special platform specific codec restrictions. Returns true if
  // |codec| is supported when contained in |mime_type_lower_case|.
  // |is_encrypted| means the codec will be used with encrypted blocks.
  // |platform_info| describes the availability of various platform features;
  // see PlatformInfo for more details.
  static bool IsCodecSupportedOnPlatform(
      Codec codec,
      const std::string& mime_type_lower_case,
      bool is_encrypted,
      const PlatformInfo& platform_info);

 private:
  typedef base::hash_set<int> CodecSet;
  typedef std::map<std::string, CodecSet> MediaFormatMappings;
  struct CodecEntry {
    CodecEntry() : codec(INVALID_CODEC), is_ambiguous(true) {}
    CodecEntry(Codec c, bool ambiguous) : codec(c), is_ambiguous(ambiguous) {}
    Codec codec;
    bool is_ambiguous;
  };
  typedef std::map<std::string, CodecEntry> StringToCodecMappings;

  // Initializes the supported media types into hash sets for faster lookup.
  void InitializeMimeTypeMaps();

  // Initializes the supported media formats (|media_format_map_|).
  void AddSupportedMediaFormats();

  // Adds |mime_type| with the specified codecs to |media_format_map_|.
  void AddContainerWithCodecs(const std::string& mime_type,
                              const CodecSet& codecs_list,
                              bool is_proprietary_mime_type);

  // Returns IsSupported if all codec IDs in |codecs| are unambiguous and are
  // supported in |mime_type_lower_case|. MayBeSupported is returned if at least
  // one codec ID in |codecs| is ambiguous but all the codecs are supported.
  // IsNotSupported is returned if |mime_type_lower_case| is not supported or at
  // least one is not supported in |mime_type_lower_case|. |is_encrypted| means
  // the codec will be used with encrypted blocks.
  SupportsType AreSupportedCodecs(const CodecSet& supported_codecs,
                                  const std::vector<std::string>& codecs,
                                  const std::string& mime_type_lower_case,
                                  bool is_encrypted) const;

  // Converts a codec ID into an Codec enum value and indicates
  // whether the conversion was ambiguous.
  // Returns true if this method was able to map |codec_id| with
  // |mime_type_lower_case| to a specific Codec enum value. |codec| and
  // |is_ambiguous| are only valid if true is returned. Otherwise their value is
  // undefined after the call.
  // |is_ambiguous| is true if |codec_id| did not have enough information to
  // unambiguously determine the proper Codec enum value. If |is_ambiguous|
  // is true |codec| contains the best guess for the intended Codec enum value.
  // |profile| and |level| indicate video codec profile and level (unused for
  // audio codecs).
  // |is_encrypted| means the codec will be used with encrypted blocks.
  bool StringToCodec(const std::string& mime_type_lower_case,
                     const std::string& codec_id,
                     Codec* codec,
                     bool* is_ambiguous,
                     VideoCodecProfile* out_profile,
                     uint8_t* out_level,
                     bool is_encrypted) const;

  // Returns true if |codec| is supported when contained in
  // |mime_type_lower_case|. Note: This method will always return false for
  // proprietary codecs if |allow_proprietary_codecs_| is set to false.
  // |is_encrypted| means the codec will be used with encrypted blocks.
  bool IsCodecSupported(Codec codec,
                        const std::string& mime_type_lower_case,
                        bool is_encrypted) const;

  // Returns true if |codec| refers to a proprietary codec.
  bool IsCodecProprietary(Codec codec) const;

  // Returns true and sets |*default_codec| if |mime_type| has a  default codec
  // associated with it. Returns false otherwise and the value of
  // |*default_codec| is undefined.
  bool GetDefaultCodecLowerCase(const std::string& mime_type_lower_case,
                                Codec* default_codec) const;

  // Returns true if |mime_type_lower_case| has a default codec associated with
  // it and IsCodecSupported() returns true for that particular codec.
  // |is_encrypted| means the codec will be used with encrypted blocks.
  bool IsDefaultCodecSupportedLowerCase(const std::string& mime_type_lower_case,
                                        bool is_encrypted) const;

#if defined(OS_ANDROID)
  // Indicates the support of various codecs within the platform.
  PlatformInfo platform_info_;
#endif

  // A map of mime_types and hash map of the supported codecs for the mime_type.
  MediaFormatMappings media_format_map_;

  // List of proprietary containers in |media_format_map_|.
  std::vector<std::string> proprietary_media_containers_;
  // Whether proprietary codec support should be advertised to callers.
  bool allow_proprietary_codecs_;

  // Lookup table for string compare based string -> Codec mappings.
  StringToCodecMappings string_to_codec_map_;

  DISALLOW_COPY_AND_ASSIGN(MimeUtil);
};

}  // namespace internal
}  // namespace media

#endif  // MEDIA_BASE_MIME_UTIL_INTERNAL_H_
