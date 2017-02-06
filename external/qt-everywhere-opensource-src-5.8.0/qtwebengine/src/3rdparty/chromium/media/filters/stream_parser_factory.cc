// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/stream_parser_factory.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/formats/mpeg/adts_stream_parser.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "media/media_features.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

#if defined(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#include "media/formats/mp2t/mp2t_stream_parser.h"
#endif
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#endif

namespace media {

typedef bool (*CodecIDValidatorFunction)(
    const std::string& codecs_id,
    const scoped_refptr<MediaLog>& media_log);

struct CodecInfo {
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO
  };

  // Update tools/metrics/histograms/histograms.xml if new values are added.
  enum HistogramTag {
    HISTOGRAM_UNKNOWN,
    HISTOGRAM_VP8,
    HISTOGRAM_VP9,
    HISTOGRAM_VORBIS,
    HISTOGRAM_H264,
    HISTOGRAM_MPEG2AAC,
    HISTOGRAM_MPEG4AAC,
    HISTOGRAM_EAC3,
    HISTOGRAM_MP3,
    HISTOGRAM_OPUS,
    HISTOGRAM_HEVC,
    HISTOGRAM_AC3,
    HISTOGRAM_MAX = HISTOGRAM_AC3  // Must be equal to largest logged entry.
  };

  const char* pattern;
  Type type;
  CodecIDValidatorFunction validator;
  HistogramTag tag;
};

typedef StreamParser* (*ParserFactoryFunction)(
    const std::vector<std::string>& codecs,
    const scoped_refptr<MediaLog>& media_log);

struct SupportedTypeInfo {
  const char* type;
  const ParserFactoryFunction factory_function;
  const CodecInfo** codecs;
};

static const CodecInfo kVP8CodecInfo = { "vp8", CodecInfo::VIDEO, NULL,
                                         CodecInfo::HISTOGRAM_VP8 };
static const CodecInfo kVP9CodecInfo = { "vp9", CodecInfo::VIDEO, NULL,
                                         CodecInfo::HISTOGRAM_VP9 };
static const CodecInfo kVorbisCodecInfo = { "vorbis", CodecInfo::AUDIO, NULL,
                                            CodecInfo::HISTOGRAM_VORBIS };
static const CodecInfo kOpusCodecInfo = { "opus", CodecInfo::AUDIO, NULL,
                                          CodecInfo::HISTOGRAM_OPUS };

static const CodecInfo* kVideoWebMCodecs[] = {
  &kVP8CodecInfo,
  &kVP9CodecInfo,
  &kVorbisCodecInfo,
  &kOpusCodecInfo,
  NULL
};

static const CodecInfo* kAudioWebMCodecs[] = {
  &kVorbisCodecInfo,
  &kOpusCodecInfo,
  NULL
};

static StreamParser* BuildWebMParser(const std::vector<std::string>& codecs,
                                     const scoped_refptr<MediaLog>& media_log) {
  return new WebMStreamParser();
}

#if defined(USE_PROPRIETARY_CODECS)
bool CheckIfMp4Vp9DemuxingEnabled(const std::string& codec_id,
                                  const scoped_refptr<MediaLog>& media_log) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVp9InMp4);
}

// AAC Object Type IDs that Chrome supports.
static const int kAACLCObjectType = 2;
static const int kAACSBRObjectType = 5;
static const int kAACPSObjectType = 29;

static int GetMP4AudioObjectType(const std::string& codec_id,
                                 const scoped_refptr<MediaLog>& media_log) {
  // From RFC 6381 section 3.3 (ISO Base Media File Format Name Space):
  // When the first element of a ['codecs' parameter value] is 'mp4a' ...,
  // the second element is a hexadecimal representation of the MP4 Registration
  // Authority ObjectTypeIndication (OTI). Note that MP4RA uses a leading "0x"
  // with these values, which is omitted here and hence implied.
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      codec_id, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() == 3 && tokens[0] == "mp4a" && tokens[1] == "40") {
    // From RFC 6381 section 3.3:
    // One of the OTI values for 'mp4a' is 40 (identifying MPEG-4 audio). For
    // this value, the third element identifies the audio ObjectTypeIndication
    // (OTI) ... expressed as a decimal number.
    int audio_object_type;
    if (base::StringToInt(tokens[2], &audio_object_type))
      return audio_object_type;
  }

  MEDIA_LOG(DEBUG, media_log) << "Malformed mimetype codec '" << codec_id
                              << "'";
  return -1;
}

bool ValidateMP4ACodecID(const std::string& codec_id,
                         const scoped_refptr<MediaLog>& media_log) {
  int audio_object_type = GetMP4AudioObjectType(codec_id, media_log);
  if (audio_object_type == kAACLCObjectType ||
      audio_object_type == kAACSBRObjectType ||
      audio_object_type == kAACPSObjectType) {
    return true;
  }

  MEDIA_LOG(DEBUG, media_log) << "Unsupported audio object type "
                              << audio_object_type << " in codec '" << codec_id
                              << "'";
  return false;
}

static const CodecInfo kH264AVC1CodecInfo = { "avc1.*", CodecInfo::VIDEO, NULL,
                                              CodecInfo::HISTOGRAM_H264 };
static const CodecInfo kH264AVC3CodecInfo = { "avc3.*", CodecInfo::VIDEO, NULL,
                                              CodecInfo::HISTOGRAM_H264 };
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
static const CodecInfo kHEVCHEV1CodecInfo = { "hev1.*", CodecInfo::VIDEO, NULL,
                                              CodecInfo::HISTOGRAM_HEVC };
static const CodecInfo kHEVCHVC1CodecInfo = { "hvc1.*", CodecInfo::VIDEO, NULL,
                                              CodecInfo::HISTOGRAM_HEVC };
#endif
static const CodecInfo kMPEG4VP09CodecInfo = {"vp09.*", CodecInfo::VIDEO,
                                              &CheckIfMp4Vp9DemuxingEnabled,
                                              CodecInfo::HISTOGRAM_VP9};
static const CodecInfo kMPEG4AACCodecInfo = { "mp4a.40.*", CodecInfo::AUDIO,
                                              &ValidateMP4ACodecID,
                                              CodecInfo::HISTOGRAM_MPEG4AAC };
static const CodecInfo kMPEG2AACLCCodecInfo = { "mp4a.67", CodecInfo::AUDIO,
                                                NULL,
                                                CodecInfo::HISTOGRAM_MPEG2AAC };
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
// The 'ac-3' and 'ec-3' are mime codec ids for AC3 and EAC3 according to
// http://www.mp4ra.org/codecs.html
// The object types for AC3 and EAC3 in MP4 container are 0xa5 and 0xa6, so
// according to RFC 6381 this corresponds to codec ids 'mp4a.A5' and 'mp4a.A6'.
// Codec ids with lower case oti (mp4a.a5 and mp4a.a6) are supported for
// backward compatibility.
static const CodecInfo kAC3CodecInfo1 = {"ac-3", CodecInfo::AUDIO, NULL,
                                         CodecInfo::HISTOGRAM_AC3};
static const CodecInfo kAC3CodecInfo2 = {"mp4a.a5", CodecInfo::AUDIO, NULL,
                                         CodecInfo::HISTOGRAM_AC3};
static const CodecInfo kAC3CodecInfo3 = {"mp4a.A5", CodecInfo::AUDIO, NULL,
                                         CodecInfo::HISTOGRAM_AC3};
static const CodecInfo kEAC3CodecInfo1 = {"ec-3", CodecInfo::AUDIO, NULL,
                                          CodecInfo::HISTOGRAM_EAC3};
static const CodecInfo kEAC3CodecInfo2 = {"mp4a.a6", CodecInfo::AUDIO, NULL,
                                          CodecInfo::HISTOGRAM_EAC3};
static const CodecInfo kEAC3CodecInfo3 = {"mp4a.A6", CodecInfo::AUDIO, NULL,
                                          CodecInfo::HISTOGRAM_EAC3};
#endif

static const CodecInfo* kVideoMP4Codecs[] = {
    &kH264AVC1CodecInfo,  &kH264AVC3CodecInfo,
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    &kHEVCHEV1CodecInfo,  &kHEVCHVC1CodecInfo,
#endif
    &kMPEG4VP09CodecInfo,
    &kMPEG4AACCodecInfo,  &kMPEG2AACLCCodecInfo, NULL};

static const CodecInfo* kAudioMP4Codecs[] = {&kMPEG4AACCodecInfo,
                                             &kMPEG2AACLCCodecInfo,
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
                                             &kAC3CodecInfo1,
                                             &kAC3CodecInfo2,
                                             &kAC3CodecInfo3,
                                             &kEAC3CodecInfo1,
                                             &kEAC3CodecInfo2,
                                             &kEAC3CodecInfo3,
#endif
                                             NULL};

static StreamParser* BuildMP4Parser(const std::vector<std::string>& codecs,
                                    const scoped_refptr<MediaLog>& media_log) {
  std::set<int> audio_object_types;

  bool has_sbr = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    std::string codec_id = codecs[i];
    if (base::MatchPattern(codec_id, kMPEG2AACLCCodecInfo.pattern)) {
      audio_object_types.insert(mp4::kISO_13818_7_AAC_LC);
    } else if (base::MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern)) {
      int audio_object_type = GetMP4AudioObjectType(codec_id, media_log);
      DCHECK_GT(audio_object_type, 0);

      audio_object_types.insert(mp4::kISO_14496_3);

      if (audio_object_type == kAACSBRObjectType ||
          audio_object_type == kAACPSObjectType) {
        has_sbr = true;
        break;
      }
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    } else if (base::MatchPattern(codec_id, kAC3CodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kAC3CodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kAC3CodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kAC3);
    } else if (base::MatchPattern(codec_id, kEAC3CodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kEAC3CodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kEAC3CodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kEAC3);
#endif
    }
  }

  return new mp4::MP4StreamParser(audio_object_types, has_sbr);
}

static const CodecInfo kMP3CodecInfo = { NULL, CodecInfo::AUDIO, NULL,
                                         CodecInfo::HISTOGRAM_MP3 };

static const CodecInfo* kAudioMP3Codecs[] = {
  &kMP3CodecInfo,
  NULL
};

static StreamParser* BuildMP3Parser(const std::vector<std::string>& codecs,
                                    const scoped_refptr<MediaLog>& media_log) {
  return new MPEG1AudioStreamParser();
}

static const CodecInfo kADTSCodecInfo = { NULL, CodecInfo::AUDIO, NULL,
                                          CodecInfo::HISTOGRAM_MPEG4AAC };
static const CodecInfo* kAudioADTSCodecs[] = {
  &kADTSCodecInfo,
  NULL
};

static StreamParser* BuildADTSParser(const std::vector<std::string>& codecs,
                                     const scoped_refptr<MediaLog>& media_log) {
  return new ADTSStreamParser();
}

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
static const CodecInfo* kVideoMP2TCodecs[] = {
  &kH264AVC1CodecInfo,
  &kH264AVC3CodecInfo,
  &kMPEG4AACCodecInfo,
  &kMPEG2AACLCCodecInfo,
  NULL
};

static StreamParser* BuildMP2TParser(const std::vector<std::string>& codecs,
                                     const scoped_refptr<MediaLog>& media_log) {
  bool has_sbr = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    std::string codec_id = codecs[i];
    if (base::MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern)) {
      int audio_object_type = GetMP4AudioObjectType(codec_id, media_log);
      if (audio_object_type == kAACSBRObjectType ||
          audio_object_type == kAACPSObjectType) {
        has_sbr = true;
      }
    }
  }

  return new media::mp2t::Mp2tStreamParser(has_sbr);
}
#endif
#endif

static const SupportedTypeInfo kSupportedTypeInfo[] = {
    {"video/webm", &BuildWebMParser, kVideoWebMCodecs},
    {"audio/webm", &BuildWebMParser, kAudioWebMCodecs},
#if defined(USE_PROPRIETARY_CODECS)
    {"audio/aac", &BuildADTSParser, kAudioADTSCodecs},
    {"audio/mpeg", &BuildMP3Parser, kAudioMP3Codecs},
    {"video/mp4", &BuildMP4Parser, kVideoMP4Codecs},
    {"audio/mp4", &BuildMP4Parser, kAudioMP4Codecs},
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
    {"video/mp2t", &BuildMP2TParser, kVideoMP2TCodecs},
#endif
#endif
};

// Verify that |codec_info| is supported on this platform.
//
// Returns true if |codec_info| is a valid audio/video codec and is allowed.
// |audio_codecs| has |codec_info|.tag added to its list if |codec_info| is an
// audio codec. |audio_codecs| may be NULL, in which case it is not updated.
// |video_codecs| has |codec_info|.tag added to its list if |codec_info| is a
// video codec. |video_codecs| may be NULL, in which case it is not updated.
//
// Returns false otherwise, and |audio_codecs| and |video_codecs| not touched.
static bool VerifyCodec(
    const CodecInfo* codec_info,
    std::vector<CodecInfo::HistogramTag>* audio_codecs,
    std::vector<CodecInfo::HistogramTag>* video_codecs) {
  switch (codec_info->type) {
    case CodecInfo::AUDIO:
      if (audio_codecs)
        audio_codecs->push_back(codec_info->tag);
      return true;
    case CodecInfo::VIDEO:
#if defined(OS_ANDROID)
      // TODO(wolenetz, dalecurtis): This should instead use MimeUtil() to avoid
      // duplication of subtle Android behavior.  http://crbug.com/587303.
      if (codec_info->tag == CodecInfo::HISTOGRAM_H264) {
        if (media::IsUnifiedMediaPipelineEnabled() &&
            !media::HasPlatformDecoderSupport()) {
          return false;
        }

        if (!MediaCodecUtil::IsMediaCodecAvailable())
          return false;
      }
      if (codec_info->tag == CodecInfo::HISTOGRAM_VP8 &&
          !media::MediaCodecUtil::IsVp8DecoderAvailable() &&
          !media::IsUnifiedMediaPipelineEnabled()) {
        return false;
      }
      if (codec_info->tag == CodecInfo::HISTOGRAM_VP9 &&
          !media::MediaCodecUtil::IsVp9DecoderAvailable() &&
          !media::IsUnifiedMediaPipelineEnabled()) {
        return false;
      }
      if (codec_info->tag == CodecInfo::HISTOGRAM_OPUS &&
          !media::PlatformHasOpusSupport() &&
          !media::IsUnifiedMediaPipelineEnabled()) {
        return false;
      }
#endif
      if (video_codecs)
        video_codecs->push_back(codec_info->tag);
      return true;
    default:
      // Not audio or video, so skip it.
      DVLOG(1) << "CodecInfo type of " << codec_info->type
               << " should not be specified in a SupportedTypes list";
      return false;
  }
}

// Checks to see if the specified |type| and |codecs| list are supported.
//
// Returns true if |type| and all codecs listed in |codecs| are supported.
// |factory_function| contains a function that can build a StreamParser for this
// type. Value may be NULL, in which case it is not touched.
// |audio_codecs| is updated with the appropriate HistogramTags for matching
// audio codecs specified in |codecs|. Value may be NULL, in which case it is
// not touched.
// |video_codecs| is updated with the appropriate HistogramTags for matching
// video codecs specified in |codecs|. Value may be NULL, in which case it is
// not touched.
//
// Returns false otherwise. The values of |factory_function|, |audio_codecs|,
// and |video_codecs| are undefined.
static bool CheckTypeAndCodecs(
    const std::string& type,
    const std::vector<std::string>& codecs,
    const scoped_refptr<MediaLog>& media_log,
    ParserFactoryFunction* factory_function,
    std::vector<CodecInfo::HistogramTag>* audio_codecs,
    std::vector<CodecInfo::HistogramTag>* video_codecs) {
  // Search for the SupportedTypeInfo for |type|.
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (type == type_info.type) {
      if (codecs.empty()) {
        const CodecInfo* codec_info = type_info.codecs[0];
        if (codec_info && !codec_info->pattern &&
            VerifyCodec(codec_info, audio_codecs, video_codecs)) {

          if (factory_function)
            *factory_function = type_info.factory_function;
          return true;
        }

        MEDIA_LOG(DEBUG, media_log)
            << "A codecs parameter must be provided for '" << type << "'";
        return false;
      }

      // Make sure all the codecs specified in |codecs| are
      // in the supported type info.
      for (size_t j = 0; j < codecs.size(); ++j) {
        // Search the type info for a match.
        bool found_codec = false;
        std::string codec_id = codecs[j];
        for (int k = 0; type_info.codecs[k]; ++k) {
          if (base::MatchPattern(codec_id, type_info.codecs[k]->pattern) &&
              (!type_info.codecs[k]->validator ||
               type_info.codecs[k]->validator(codec_id, media_log))) {
            found_codec =
                VerifyCodec(type_info.codecs[k], audio_codecs, video_codecs);
            break;  // Since only 1 pattern will match, no need to check others.
          }
        }

        if (!found_codec) {
          MEDIA_LOG(DEBUG, media_log) << "Codec '" << codec_id
                                      << "' is not supported for '" << type
                                      << "'";
          return false;
        }
      }

      if (factory_function)
        *factory_function = type_info.factory_function;

      // All codecs were supported by this |type|.
      return true;
    }
  }

  // |type| didn't match any of the supported types.
  return false;
}

bool StreamParserFactory::IsTypeSupported(
    const std::string& type, const std::vector<std::string>& codecs) {
  return CheckTypeAndCodecs(type, codecs, new MediaLog(), NULL, NULL, NULL);
}

std::unique_ptr<StreamParser> StreamParserFactory::Create(
    const std::string& type,
    const std::vector<std::string>& codecs,
    const scoped_refptr<MediaLog>& media_log,
    bool* has_audio,
    bool* has_video) {
  std::unique_ptr<StreamParser> stream_parser;
  ParserFactoryFunction factory_function;
  std::vector<CodecInfo::HistogramTag> audio_codecs;
  std::vector<CodecInfo::HistogramTag> video_codecs;
  *has_audio = false;
  *has_video = false;

  if (CheckTypeAndCodecs(type, codecs, media_log, &factory_function,
                         &audio_codecs, &video_codecs)) {
    *has_audio = !audio_codecs.empty();
    *has_video = !video_codecs.empty();

    // Log the number of codecs specified, as well as the details on each one.
    UMA_HISTOGRAM_COUNTS_100("Media.MSE.NumberOfTracks", codecs.size());
    for (size_t i = 0; i < audio_codecs.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("Media.MSE.AudioCodec",
                                audio_codecs[i],
                                CodecInfo::HISTOGRAM_MAX + 1);
    }
    for (size_t i = 0; i < video_codecs.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("Media.MSE.VideoCodec",
                                video_codecs[i],
                                CodecInfo::HISTOGRAM_MAX + 1);
    }

    stream_parser.reset(factory_function(codecs, media_log));
  }

  return stream_parser;
}

}  // namespace media
