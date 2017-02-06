// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mime_util_internal.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "media/base/android/media_codec_util.h"
#endif

namespace media {
namespace internal {

struct CodecIDMappings {
  const char* const codec_id;
  MimeUtil::Codec codec;
};

// List of codec IDs that provide enough information to determine the
// codec and profile being requested.
//
// The "mp4a" strings come from RFC 6381.
static const CodecIDMappings kUnambiguousCodecStringMap[] = {
    {"1", MimeUtil::PCM},  // We only allow this for WAV so it isn't ambiguous.
    // avc1/avc3.XXXXXX may be unambiguous; handled by ParseAVCCodecId().
    // hev1/hvc1.XXXXXX may be unambiguous; handled by ParseHEVCCodecID().
    // vp9, vp9.0, vp09.xx.xx.xx.xx.xx.xx.xx may be unambiguous; handled by
    // ParseVp9CodecID().
    {"mp3", MimeUtil::MP3},
    // Following is the list of RFC 6381 compliant audio codec strings:
    //   mp4a.66     - MPEG-2 AAC MAIN
    //   mp4a.67     - MPEG-2 AAC LC
    //   mp4a.68     - MPEG-2 AAC SSR
    //   mp4a.69     - MPEG-2 extension to MPEG-1 (MP3)
    //   mp4a.6B     - MPEG-1 audio (MP3)
    //   mp4a.40.2   - MPEG-4 AAC LC
    //   mp4a.40.02  - MPEG-4 AAC LC (leading 0 in aud-oti for compatibility)
    //   mp4a.40.5   - MPEG-4 HE-AAC v1 (AAC LC + SBR)
    //   mp4a.40.05  - MPEG-4 HE-AAC v1 (AAC LC + SBR) (leading 0 in aud-oti for
    //                 compatibility)
    //   mp4a.40.29  - MPEG-4 HE-AAC v2 (AAC LC + SBR + PS)
    {"mp4a.66", MimeUtil::MPEG2_AAC},
    {"mp4a.67", MimeUtil::MPEG2_AAC},
    {"mp4a.68", MimeUtil::MPEG2_AAC},
    {"mp4a.69", MimeUtil::MP3},
    {"mp4a.6B", MimeUtil::MP3},
    {"mp4a.40.2", MimeUtil::MPEG4_AAC},
    {"mp4a.40.02", MimeUtil::MPEG4_AAC},
    {"mp4a.40.5", MimeUtil::MPEG4_AAC},
    {"mp4a.40.05", MimeUtil::MPEG4_AAC},
    {"mp4a.40.29", MimeUtil::MPEG4_AAC},
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    // TODO(servolk): Strictly speaking only mp4a.A5 and mp4a.A6 codec ids are
    // valid according to RFC 6381 section 3.3, 3.4. Lower-case oti (mp4a.a5 and
    // mp4a.a6) should be rejected. But we used to allow those in older versions
    // of Chromecast firmware and some apps (notably MPL) depend on those codec
    // types being supported, so they should be allowed for now
    // (crbug.com/564960).
    {"ac-3", MimeUtil::AC3},
    {"mp4a.a5", MimeUtil::AC3},
    {"mp4a.A5", MimeUtil::AC3},
    {"ec-3", MimeUtil::EAC3},
    {"mp4a.a6", MimeUtil::EAC3},
    {"mp4a.A6", MimeUtil::EAC3},
#endif
    {"vorbis", MimeUtil::VORBIS},
    {"opus", MimeUtil::OPUS},
    {"vp8", MimeUtil::VP8},
    {"vp8.0", MimeUtil::VP8},
    {"theora", MimeUtil::THEORA}};

// List of codec IDs that are ambiguous and don't provide
// enough information to determine the codec and profile.
// The codec in these entries indicate the codec and profile
// we assume the user is trying to indicate.
static const CodecIDMappings kAmbiguousCodecStringMap[] = {
    {"mp4a.40", MimeUtil::MPEG4_AAC},
    {"avc1", MimeUtil::H264},
    {"avc3", MimeUtil::H264},
    // avc1/avc3.XXXXXX may be ambiguous; handled by ParseAVCCodecId().
};

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
static const char kHexString[] = "0123456789ABCDEF";
static char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

static std::string TranslateLegacyAvc1CodecIds(const std::string& codec_id) {
  // Special handling for old, pre-RFC 6381 format avc1 strings, which are still
  // being used by some HLS apps to preserve backward compatibility with older
  // iOS devices. The old format was avc1.<profile>.<level>
  // Where <profile> is H.264 profile_idc encoded as a decimal number, i.e.
  // 66 is baseline profile (0x42)
  // 77 is main profile (0x4d)
  // 100 is high profile (0x64)
  // And <level> is H.264 level multiplied by 10, also encoded as decimal number
  // E.g. <level> 31 corresponds to H.264 level 3.1
  // See, for example, http://qtdevseed.apple.com/qadrift/testcases/tc-0133.php
  uint32_t level_start = 0;
  std::string result;
  if (base::StartsWith(codec_id, "avc1.66.", base::CompareCase::SENSITIVE)) {
    level_start = 8;
    result = "avc1.4200";
  } else if (base::StartsWith(codec_id, "avc1.77.",
                              base::CompareCase::SENSITIVE)) {
    level_start = 8;
    result = "avc1.4D00";
  } else if (base::StartsWith(codec_id, "avc1.100.",
                              base::CompareCase::SENSITIVE)) {
    level_start = 9;
    result = "avc1.6400";
  }

  uint32_t level = 0;
  if (level_start > 0 &&
      base::StringToUint(codec_id.substr(level_start), &level) && level < 256) {
    // This is a valid legacy avc1 codec id - return the codec id translated
    // into RFC 6381 format.
    result.push_back(IntToHex(level >> 4));
    result.push_back(IntToHex(level & 0xf));
    return result;
  }

  // This is not a valid legacy avc1 codec id - return the original codec id.
  return codec_id;
}
#endif

static bool IsValidH264Level(uint8_t level_idc) {
  // Valid levels taken from Table A-1 in ISO/IEC 14496-10.
  // Level_idc represents the standard level represented as decimal number
  // multiplied by ten, e.g. level_idc==32 corresponds to level==3.2
  return ((level_idc >= 10 && level_idc <= 13) ||
          (level_idc >= 20 && level_idc <= 22) ||
          (level_idc >= 30 && level_idc <= 32) ||
          (level_idc >= 40 && level_idc <= 42) ||
          (level_idc >= 50 && level_idc <= 51));
}

// Handle parsing of vp9 codec IDs.
static bool ParseVp9CodecID(const std::string& mime_type_lower_case,
                            const std::string& codec_id,
                            VideoCodecProfile* profile) {
  if (mime_type_lower_case == "video/webm") {
    if (codec_id == "vp9" || codec_id == "vp9.0") {
      // Profile is not included in the codec string. Assuming profile 0 to be
      // backward compatible.
      *profile = VP9PROFILE_PROFILE0;
      return true;
    }
    // TODO(kqyang): Should we support new codec string in WebM?
    return false;
  } else if (mime_type_lower_case == "audio/webm") {
    return false;
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVp9InMp4)) {
    return false;
  }

  std::vector<std::string> fields = base::SplitString(
      codec_id, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (fields.size() < 1)
    return false;

  if (fields[0] != "vp09")
    return false;

  if (fields.size() > 8)
    return false;

  std::vector<int> values;
  for (size_t i = 1; i < fields.size(); ++i) {
    // Missing value is not allowed.
    if (fields[i] == "")
      return false;
    int value;
    if (!base::StringToInt(fields[i], &value))
      return false;
    if (value < 0)
      return false;
    values.push_back(value);
  }

  // The spec specifies 8 fields (7 values excluding the first codec field).
  // We do not allow missing fields.
  if (values.size() < 7)
    return false;

  const int profile_idc = values[0];
  switch (profile_idc) {
    case 0:
      *profile = VP9PROFILE_PROFILE0;
      break;
    case 1:
      *profile = VP9PROFILE_PROFILE1;
      break;
    case 2:
      *profile = VP9PROFILE_PROFILE2;
      break;
    case 3:
      *profile = VP9PROFILE_PROFILE3;
      break;
    default:
      return false;
  }

  const int bit_depth = values[2];
  if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12)
    return false;

  const int color_space = values[3];
  if (color_space > 7)
    return false;

  const int chroma_subsampling = values[4];
  if (chroma_subsampling > 3)
    return false;

  const int transfer_function = values[5];
  if (transfer_function > 1)
    return false;

  const int video_full_range_flag = values[6];
  if (video_full_range_flag > 1)
    return false;

  return true;
}

MimeUtil::MimeUtil() : allow_proprietary_codecs_(false) {
#if defined(OS_ANDROID)
  platform_info_.is_unified_media_pipeline_enabled =
      IsUnifiedMediaPipelineEnabled();
  // When the unified media pipeline is enabled, we need support for both GPU
  // video decoders and MediaCodec; indicated by HasPlatformDecoderSupport().
  // When the Android pipeline is used, we only need access to MediaCodec.
  platform_info_.has_platform_decoders = ArePlatformDecodersAvailable();
  platform_info_.has_platform_vp8_decoder =
      MediaCodecUtil::IsVp8DecoderAvailable();
  platform_info_.has_platform_vp9_decoder =
      MediaCodecUtil::IsVp9DecoderAvailable();
  platform_info_.supports_opus = PlatformHasOpusSupport();
#endif

  InitializeMimeTypeMaps();
}

MimeUtil::~MimeUtil() {}

SupportsType MimeUtil::AreSupportedCodecs(
    const CodecSet& supported_codecs,
    const std::vector<std::string>& codecs,
    const std::string& mime_type_lower_case,
    bool is_encrypted) const {
  DCHECK(!supported_codecs.empty());
  DCHECK(!codecs.empty());

  SupportsType result = IsSupported;
  for (size_t i = 0; i < codecs.size(); ++i) {
    bool is_ambiguous = true;
    Codec codec = INVALID_CODEC;
    if (!StringToCodec(mime_type_lower_case, codecs[i], &codec, &is_ambiguous,
                       is_encrypted)) {
      return IsNotSupported;
    }

    if (!IsCodecSupported(codec, mime_type_lower_case, is_encrypted) ||
        supported_codecs.find(codec) == supported_codecs.end()) {
      return IsNotSupported;
    }

    if (is_ambiguous)
      result = MayBeSupported;
  }

  return result;
}

void MimeUtil::InitializeMimeTypeMaps() {
#if defined(USE_PROPRIETARY_CODECS)
  allow_proprietary_codecs_ = true;
#endif

  for (size_t i = 0; i < arraysize(kUnambiguousCodecStringMap); ++i) {
    string_to_codec_map_[kUnambiguousCodecStringMap[i].codec_id] =
        CodecEntry(kUnambiguousCodecStringMap[i].codec, false);
  }

  for (size_t i = 0; i < arraysize(kAmbiguousCodecStringMap); ++i) {
    string_to_codec_map_[kAmbiguousCodecStringMap[i].codec_id] =
        CodecEntry(kAmbiguousCodecStringMap[i].codec, true);
  }

  AddSupportedMediaFormats();
}

// Each call to AddContainerWithCodecs() contains a media type
// (https://en.wikipedia.org/wiki/Media_type) and corresponding media codec(s)
// supported by these types/containers.
// TODO(ddorwin): Replace insert() calls with initializer_list when allowed.
void MimeUtil::AddSupportedMediaFormats() {
  CodecSet implicit_codec;
  CodecSet wav_codecs;
  wav_codecs.insert(PCM);

  CodecSet ogg_audio_codecs;
  ogg_audio_codecs.insert(OPUS);
  ogg_audio_codecs.insert(VORBIS);
  CodecSet ogg_video_codecs;
#if !defined(OS_ANDROID)
  ogg_video_codecs.insert(THEORA);
#endif  // !defined(OS_ANDROID)
  CodecSet ogg_codecs(ogg_audio_codecs);
  ogg_codecs.insert(ogg_video_codecs.begin(), ogg_video_codecs.end());

  CodecSet webm_audio_codecs;
  webm_audio_codecs.insert(OPUS);
  webm_audio_codecs.insert(VORBIS);
  CodecSet webm_video_codecs;
  webm_video_codecs.insert(VP8);
  webm_video_codecs.insert(VP9);
  CodecSet webm_codecs(webm_audio_codecs);
  webm_codecs.insert(webm_video_codecs.begin(), webm_video_codecs.end());

#if defined(USE_PROPRIETARY_CODECS)
  CodecSet mp3_codecs;
  mp3_codecs.insert(MP3);

  CodecSet aac;
  aac.insert(MPEG2_AAC);
  aac.insert(MPEG4_AAC);

  CodecSet avc_and_aac(aac);
  avc_and_aac.insert(H264);

  CodecSet mp4_audio_codecs(aac);
  mp4_audio_codecs.insert(MP3);
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
  mp4_audio_codecs.insert(AC3);
  mp4_audio_codecs.insert(EAC3);
#endif  // BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)

  CodecSet mp4_video_codecs;
  mp4_video_codecs.insert(H264);
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  mp4_video_codecs.insert(HEVC);
#endif  // BUILDFLAG(ENABLE_HEVC_DEMUXING)
  // Only VP9 with valid codec string vp09.xx.xx.xx.xx.xx.xx.xx is supported.
  // See ParseVp9CodecID for details.
  mp4_video_codecs.insert(VP9);
  CodecSet mp4_codecs(mp4_audio_codecs);
  mp4_codecs.insert(mp4_video_codecs.begin(), mp4_video_codecs.end());
#endif  // defined(USE_PROPRIETARY_CODECS)

  AddContainerWithCodecs("audio/wav", wav_codecs, false);
  AddContainerWithCodecs("audio/x-wav", wav_codecs, false);
  AddContainerWithCodecs("audio/webm", webm_audio_codecs, false);
  DCHECK(!webm_video_codecs.empty());
  AddContainerWithCodecs("video/webm", webm_codecs, false);
  AddContainerWithCodecs("audio/ogg", ogg_audio_codecs, false);
  // video/ogg is only supported if an appropriate video codec is supported.
  // Note: This assumes such codecs cannot be later excluded.
  if (!ogg_video_codecs.empty())
    AddContainerWithCodecs("video/ogg", ogg_codecs, false);
  // TODO(ddorwin): Should the application type support Opus?
  AddContainerWithCodecs("application/ogg", ogg_codecs, false);

#if defined(USE_PROPRIETARY_CODECS)
  AddContainerWithCodecs("audio/mpeg", mp3_codecs, true);  // Allow "mp3".
  AddContainerWithCodecs("audio/mp3", implicit_codec, true);
  AddContainerWithCodecs("audio/x-mp3", implicit_codec, true);
  AddContainerWithCodecs("audio/aac", implicit_codec, true);  // AAC / ADTS.
  AddContainerWithCodecs("audio/mp4", mp4_audio_codecs, true);
  DCHECK(!mp4_video_codecs.empty());
  AddContainerWithCodecs("video/mp4", mp4_codecs, true);
  // These strings are supported for backwards compatibility only and thus only
  // support the codecs needed for compatibility.
  AddContainerWithCodecs("audio/x-m4a", aac, true);
  AddContainerWithCodecs("video/x-m4v", avc_and_aac, true);

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  // TODO(ddorwin): Exactly which codecs should be supported?
  DCHECK(!mp4_video_codecs.empty());
  AddContainerWithCodecs("video/mp2t", mp4_codecs, true);
#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#if defined(OS_ANDROID)
  // HTTP Live Streaming (HLS).
  // TODO(ddorwin): Is any MP3 codec string variant included in real queries?
  CodecSet hls_codecs(avc_and_aac);
  hls_codecs.insert(MP3);
  AddContainerWithCodecs("application/x-mpegurl", hls_codecs, true);
  AddContainerWithCodecs("application/vnd.apple.mpegurl", hls_codecs, true);
#endif  // defined(OS_ANDROID)
#endif  // defined(USE_PROPRIETARY_CODECS)
}

void MimeUtil::AddContainerWithCodecs(const std::string& mime_type,
                                      const CodecSet& codecs,
                                      bool is_proprietary_mime_type) {
#if !defined(USE_PROPRIETARY_CODECS)
  DCHECK(!is_proprietary_mime_type);
#endif

  media_format_map_[mime_type] = codecs;

  if (is_proprietary_mime_type)
    proprietary_media_containers_.push_back(mime_type);
}

bool MimeUtil::IsSupportedMediaMimeType(const std::string& mime_type) const {
  return media_format_map_.find(base::ToLowerASCII(mime_type)) !=
         media_format_map_.end();
}

void MimeUtil::ParseCodecString(const std::string& codecs,
                                std::vector<std::string>* codecs_out,
                                bool strip) {
  *codecs_out =
      base::SplitString(base::TrimString(codecs, "\"", base::TRIM_ALL), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Convert empty or all-whitespace input to 0 results.
  if (codecs_out->size() == 1 && (*codecs_out)[0].empty())
    codecs_out->clear();

  if (!strip)
    return;

  // Strip everything past the first '.'
  for (std::vector<std::string>::iterator it = codecs_out->begin();
       it != codecs_out->end(); ++it) {
    size_t found = it->find_first_of('.');
    if (found != std::string::npos)
      it->resize(found);
  }
}

SupportsType MimeUtil::IsSupportedMediaFormat(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    bool is_encrypted) const {
  const std::string mime_type_lower_case = base::ToLowerASCII(mime_type);
  MediaFormatMappings::const_iterator it_media_format_map =
      media_format_map_.find(mime_type_lower_case);
  if (it_media_format_map == media_format_map_.end())
    return IsNotSupported;

  if (it_media_format_map->second.empty()) {
    // We get here if the mimetype does not expect a codecs parameter.
    return (codecs.empty() && IsDefaultCodecSupportedLowerCase(
                                  mime_type_lower_case, is_encrypted))
               ? IsSupported
               : IsNotSupported;
  }

  if (codecs.empty()) {
    // We get here if the mimetype expects to get a codecs parameter,
    // but didn't get one. If |mime_type_lower_case| does not have a default
    // codec the best we can do is say "maybe" because we don't have enough
    // information.
    Codec default_codec = INVALID_CODEC;
    if (!GetDefaultCodecLowerCase(mime_type_lower_case, &default_codec))
      return MayBeSupported;

    return IsCodecSupported(default_codec, mime_type_lower_case, is_encrypted)
               ? IsSupported
               : IsNotSupported;
  }

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  if (mime_type_lower_case == "video/mp2t") {
    std::vector<std::string> codecs_to_check;
    for (const auto& codec_id : codecs) {
      codecs_to_check.push_back(TranslateLegacyAvc1CodecIds(codec_id));
    }
    return AreSupportedCodecs(it_media_format_map->second, codecs_to_check,
                              mime_type_lower_case, is_encrypted);
  }
#endif

  return AreSupportedCodecs(it_media_format_map->second, codecs,
                            mime_type_lower_case, is_encrypted);
}

void MimeUtil::RemoveProprietaryMediaTypesAndCodecs() {
  for (const auto& container : proprietary_media_containers_)
    media_format_map_.erase(container);
  allow_proprietary_codecs_ = false;
}

// static
bool MimeUtil::IsCodecSupportedOnPlatform(
    Codec codec,
    const std::string& mime_type_lower_case,
    bool is_encrypted,
    const PlatformInfo& platform_info) {
  DCHECK_NE(mime_type_lower_case, "");

  // Encrypted block support is never available without platform decoders.
  if (is_encrypted && !platform_info.has_platform_decoders)
    return false;

  // NOTE: We do not account for Media Source Extensions (MSE) within these
  // checks since it has its own isTypeSupported() which will handle platform
  // specific codec rejections.  See http://crbug.com/587303.

  switch (codec) {
    // ----------------------------------------------------------------------
    // The following codecs are never supported.
    // ----------------------------------------------------------------------
    case INVALID_CODEC:
    case AC3:
    case EAC3:
    case THEORA:
      return false;

    // ----------------------------------------------------------------------
    // The remaining codecs may be supported depending on platform abilities.
    // ----------------------------------------------------------------------

    case PCM:
    case MP3:
    case MPEG4_AAC:
    case VORBIS:
      // These codecs are always supported; via a platform decoder (when used
      // with MSE/EME), a software decoder (the unified pipeline), or with
      // MediaPlayer.
      DCHECK(!is_encrypted || platform_info.has_platform_decoders);
      return true;

    case MPEG2_AAC:
      // MPEG-2 variants of AAC are not supported on Android unless the unified
      // media pipeline can be used and the container is not HLS. These codecs
      // will be decoded in software. See https:crbug.com/544268 for details.
      if (mime_type_lower_case == "application/x-mpegurl" ||
          mime_type_lower_case == "application/vnd.apple.mpegurl") {
        return false;
      }
      return !is_encrypted && platform_info.is_unified_media_pipeline_enabled;

    case OPUS:
      // If clear, the unified pipeline can always decode Opus in software.
      if (!is_encrypted && platform_info.is_unified_media_pipeline_enabled)
        return true;

      // Otherwise, platform support is required.
      if (!platform_info.supports_opus)
        return false;

      // MediaPlayer does not support Opus in ogg containers.
      if (base::EndsWith(mime_type_lower_case, "ogg",
                         base::CompareCase::SENSITIVE)) {
        return false;
      }

      DCHECK(!is_encrypted || platform_info.has_platform_decoders);
      return true;

    case H264:
      // The unified pipeline requires platform support for h264.
      if (platform_info.is_unified_media_pipeline_enabled)
        return platform_info.has_platform_decoders;

      // When MediaPlayer or MediaCodec is used, h264 is always supported.
      DCHECK(!is_encrypted || platform_info.has_platform_decoders);
      return true;

    case HEVC:
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
      if (platform_info.is_unified_media_pipeline_enabled &&
          !platform_info.has_platform_decoders) {
        return false;
      }

#if defined(OS_ANDROID)
      // HEVC/H.265 is supported in Lollipop+ (API Level 21), according to
      // http://developer.android.com/reference/android/media/MediaFormat.html
      return base::android::BuildInfo::GetInstance()->sdk_int() >= 21;
#else
      return true;
#endif  // defined(OS_ANDROID)
#else
      return false;
#endif  // BUILDFLAG(ENABLE_HEVC_DEMUXING)

    case VP8:
      // If clear, the unified pipeline can always decode VP8 in software.
      if (!is_encrypted && platform_info.is_unified_media_pipeline_enabled)
        return true;

      if (is_encrypted)
        return platform_info.has_platform_vp8_decoder;

      // MediaPlayer can always play VP8. Note: This is incorrect for MSE, but
      // MSE does not use this code. http://crbug.com/587303.
      return true;

    case VP9: {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kReportVp9AsAnUnsupportedMimeType)) {
        return false;
      }

      // If clear, the unified pipeline can always decode VP9 in software.
      if (!is_encrypted && platform_info.is_unified_media_pipeline_enabled)
        return true;

      if (!platform_info.has_platform_vp9_decoder)
        return false;

      // Encrypted content is demuxed so the container is irrelevant.
      if (is_encrypted)
        return true;

      // MediaPlayer only supports VP9 in WebM.
      return mime_type_lower_case == "video/webm";
    }
  }

  return false;
}

bool MimeUtil::StringToCodec(const std::string& mime_type_lower_case,
                             const std::string& codec_id,
                             Codec* codec,
                             bool* is_ambiguous,
                             bool is_encrypted) const {
  StringToCodecMappings::const_iterator itr =
      string_to_codec_map_.find(codec_id);
  if (itr != string_to_codec_map_.end()) {
    *codec = itr->second.codec;
    *is_ambiguous = itr->second.is_ambiguous;
    return true;
  }

// If |codec_id| is not in |string_to_codec_map_|, then we assume that it is
// either H.264 or HEVC/H.265 codec ID because currently those are the only
// ones that are not added to the |string_to_codec_map_| and require parsing.
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level_idc = 0;

  if (ParseAVCCodecId(codec_id, &profile, &level_idc)) {
    *codec = MimeUtil::H264;
    switch (profile) {
// HIGH10PROFILE is supported through fallback to the ffmpeg decoder
// which is not available on Android, or if FFMPEG is not used.
#if !defined(MEDIA_DISABLE_FFMPEG) && !defined(OS_ANDROID)
      case H264PROFILE_HIGH10PROFILE:
        if (is_encrypted) {
          // FFmpeg is not generally used for encrypted videos, so we do not
          // know whether 10-bit is supported.
          *is_ambiguous = true;
          break;
        }
// Fall through.
#endif

      case H264PROFILE_BASELINE:
      case H264PROFILE_MAIN:
      case H264PROFILE_HIGH:
        *is_ambiguous = !IsValidH264Level(level_idc);
        break;
      default:
        *is_ambiguous = true;
    }
    return true;
  }

  if (ParseVp9CodecID(mime_type_lower_case, codec_id, &profile)) {
    *codec = MimeUtil::VP9;
    switch (profile) {
      case VP9PROFILE_PROFILE0:
        // Profile 0 should always be supported if VP9 is supported.
        *is_ambiguous = false;
        break;
      default:
        // We don't know if the underlying platform supports these profiles.
        // Need to add platform level querying to get supported profiles
        // (crbug/604566).
        *is_ambiguous = true;
    }
    return true;
  }

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  if (ParseHEVCCodecId(codec_id, &profile, &level_idc)) {
    // TODO(servolk): Set |is_ambiguous| to true for now to make CanPlayType
    // return 'maybe' for HEVC codec ids, instead of probably. This needs to be
    // changed to false after adding platform-level HEVC profile and level
    // checks, see crbug.com/601949.
    *is_ambiguous = true;
    *codec = MimeUtil::HEVC;
    return true;
  }
#endif

  DVLOG(4) << __FUNCTION__ << ": Unrecognized codec id " << codec_id;
  return false;
}

bool MimeUtil::IsCodecSupported(Codec codec,
                                const std::string& mime_type_lower_case,
                                bool is_encrypted) const {
  DCHECK_NE(codec, INVALID_CODEC);

#if defined(OS_ANDROID)
  if (!IsCodecSupportedOnPlatform(codec, mime_type_lower_case, is_encrypted,
                                  platform_info_)) {
    return false;
  }
#endif

  return allow_proprietary_codecs_ || !IsCodecProprietary(codec);
}

bool MimeUtil::IsCodecProprietary(Codec codec) const {
  switch (codec) {
    case INVALID_CODEC:
    case AC3:
    case EAC3:
    case MP3:
    case MPEG2_AAC:
    case MPEG4_AAC:
    case H264:
    case HEVC:
      return true;

    case PCM:
    case VORBIS:
    case OPUS:
    case VP8:
    case VP9:
    case THEORA:
      return false;
  }

  return true;
}

bool MimeUtil::GetDefaultCodecLowerCase(const std::string& mime_type_lower_case,
                                        Codec* default_codec) const {
  if (mime_type_lower_case == "audio/mpeg" ||
      mime_type_lower_case == "audio/mp3" ||
      mime_type_lower_case == "audio/x-mp3") {
    *default_codec = MimeUtil::MP3;
    return true;
  }

  if (mime_type_lower_case == "audio/aac") {
    *default_codec = MimeUtil::MPEG4_AAC;
    return true;
  }

  return false;
}

bool MimeUtil::IsDefaultCodecSupportedLowerCase(
    const std::string& mime_type_lower_case,
    bool is_encrypted) const {
  Codec default_codec = Codec::INVALID_CODEC;
  if (!GetDefaultCodecLowerCase(mime_type_lower_case, &default_codec))
    return false;
  return IsCodecSupported(default_codec, mime_type_lower_case, is_encrypted);
}

}  // namespace internal
}  // namespace media
