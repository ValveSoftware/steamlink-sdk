// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/stream_parser_factory.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/formats/mpeg/adts_stream_parser.h"
#include "media/formats/mpeg/mp3_stream_parser.h"
#include "media/formats/webm/webm_stream_parser.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(USE_PROPRIETARY_CODECS)
#if defined(ENABLE_MPEG2TS_STREAM_PARSER)
#include "media/formats/mp2t/mp2t_stream_parser.h"
#endif
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#endif

namespace media {

typedef bool (*CodecIDValidatorFunction)(
    const std::string& codecs_id, const LogCB& log_cb);

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
    HISTOGRAM_MAX = HISTOGRAM_OPUS  // Must be equal to largest logged entry.
  };

  const char* pattern;
  Type type;
  CodecIDValidatorFunction validator;
  HistogramTag tag;
};

typedef StreamParser* (*ParserFactoryFunction)(
    const std::vector<std::string>& codecs,
    const LogCB& log_cb);

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

static StreamParser* BuildWebMParser(
    const std::vector<std::string>& codecs,
    const LogCB& log_cb) {
  return new WebMStreamParser();
}

#if defined(USE_PROPRIETARY_CODECS)
// AAC Object Type IDs that Chrome supports.
static const int kAACLCObjectType = 2;
static const int kAACSBRObjectType = 5;

static int GetMP4AudioObjectType(const std::string& codec_id,
                                 const LogCB& log_cb) {
  int audio_object_type;
  std::vector<std::string> tokens;
  if (Tokenize(codec_id, ".", &tokens) != 3 ||
      tokens[0] != "mp4a" || tokens[1] != "40" ||
      !base::HexStringToInt(tokens[2], &audio_object_type)) {
    MEDIA_LOG(log_cb) << "Malformed mimetype codec '" << codec_id << "'";
    return -1;
  }


  return audio_object_type;
}

bool ValidateMP4ACodecID(const std::string& codec_id, const LogCB& log_cb) {
  int audio_object_type = GetMP4AudioObjectType(codec_id, log_cb);
  if (audio_object_type == kAACLCObjectType ||
      audio_object_type == kAACSBRObjectType) {
    return true;
  }

  MEDIA_LOG(log_cb) << "Unsupported audio object type "
                    << "0x" << std::hex << audio_object_type
                    << " in codec '" << codec_id << "'";
  return false;
}

static const CodecInfo kH264AVC1CodecInfo = { "avc1.*", CodecInfo::VIDEO, NULL,
                                              CodecInfo::HISTOGRAM_H264 };
static const CodecInfo kH264AVC3CodecInfo = { "avc3.*", CodecInfo::VIDEO, NULL,
                                              CodecInfo::HISTOGRAM_H264 };
static const CodecInfo kMPEG4AACCodecInfo = { "mp4a.40.*", CodecInfo::AUDIO,
                                              &ValidateMP4ACodecID,
                                              CodecInfo::HISTOGRAM_MPEG4AAC };
static const CodecInfo kMPEG2AACLCCodecInfo = { "mp4a.67", CodecInfo::AUDIO,
                                                NULL,
                                                CodecInfo::HISTOGRAM_MPEG2AAC };

static const CodecInfo* kVideoMP4Codecs[] = {
  &kH264AVC1CodecInfo,
  &kH264AVC3CodecInfo,
  &kMPEG4AACCodecInfo,
  &kMPEG2AACLCCodecInfo,
  NULL
};

static const CodecInfo* kAudioMP4Codecs[] = {
  &kMPEG4AACCodecInfo,
  &kMPEG2AACLCCodecInfo,
  NULL
};

static StreamParser* BuildMP4Parser(
    const std::vector<std::string>& codecs, const LogCB& log_cb) {
  std::set<int> audio_object_types;

  bool has_sbr = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    std::string codec_id = codecs[i];
    if (MatchPattern(codec_id, kMPEG2AACLCCodecInfo.pattern)) {
      audio_object_types.insert(mp4::kISO_13818_7_AAC_LC);
    } else if (MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern)) {
      int audio_object_type = GetMP4AudioObjectType(codec_id, log_cb);
      DCHECK_GT(audio_object_type, 0);

      audio_object_types.insert(mp4::kISO_14496_3);

      if (audio_object_type == kAACSBRObjectType) {
        has_sbr = true;
        break;
      }
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

static StreamParser* BuildMP3Parser(
    const std::vector<std::string>& codecs, const LogCB& log_cb) {
  return new MP3StreamParser();
}

static const CodecInfo kADTSCodecInfo = { NULL, CodecInfo::AUDIO, NULL,
                                          CodecInfo::HISTOGRAM_MPEG4AAC };
static const CodecInfo* kAudioADTSCodecs[] = {
  &kADTSCodecInfo,
  NULL
};

static StreamParser* BuildADTSParser(
    const std::vector<std::string>& codecs, const LogCB& log_cb) {
  return new ADTSStreamParser();
}

#if defined(ENABLE_MPEG2TS_STREAM_PARSER)
static const CodecInfo* kVideoMP2TCodecs[] = {
  &kH264AVC1CodecInfo,
  &kH264AVC3CodecInfo,
  &kMPEG4AACCodecInfo,
  &kMPEG2AACLCCodecInfo,
  NULL
};

static StreamParser* BuildMP2TParser(
    const std::vector<std::string>& codecs, const media::LogCB& log_cb) {
  bool has_sbr = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    std::string codec_id = codecs[i];
    if (MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern) &&
        GetMP4AudioObjectType(codec_id, log_cb) == kAACSBRObjectType) {
      has_sbr = true;
    }
  }

  return new media::mp2t::Mp2tStreamParser(has_sbr);
}
#endif
#endif


static const SupportedTypeInfo kSupportedTypeInfo[] = {
  { "video/webm", &BuildWebMParser, kVideoWebMCodecs },
  { "audio/webm", &BuildWebMParser, kAudioWebMCodecs },
#if defined(USE_PROPRIETARY_CODECS)
  { "audio/aac", &BuildADTSParser, kAudioADTSCodecs },
  { "audio/mpeg", &BuildMP3Parser, kAudioMP3Codecs },
  { "video/mp4", &BuildMP4Parser, kVideoMP4Codecs },
  { "audio/mp4", &BuildMP4Parser, kAudioMP4Codecs },
#if defined(ENABLE_MPEG2TS_STREAM_PARSER)
  { "video/mp2t", &BuildMP2TParser, kVideoMP2TCodecs },
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
#if defined(ENABLE_EAC3_PLAYBACK)
      if (codec_info->tag == CodecInfo::HISTOGRAM_EAC3) {
        const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
        if (!cmd_line->HasSwitch(switches::kEnableEac3Playback))
          return false;
      }
#endif
      if (audio_codecs)
        audio_codecs->push_back(codec_info->tag);
      return true;
    case CodecInfo::VIDEO:
#if defined(OS_ANDROID)
      // VP9 is only supported on KitKat+ (API Level 19).
      if (codec_info->tag == CodecInfo::HISTOGRAM_VP9 &&
          base::android::BuildInfo::GetInstance()->sdk_int() < 19) {
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
    const LogCB& log_cb,
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

        MEDIA_LOG(log_cb) << "A codecs parameter must be provided for '"
                          << type << "'";
        return false;
      }

      // Make sure all the codecs specified in |codecs| are
      // in the supported type info.
      for (size_t j = 0; j < codecs.size(); ++j) {
        // Search the type info for a match.
        bool found_codec = false;
        std::string codec_id = codecs[j];
        for (int k = 0; type_info.codecs[k]; ++k) {
          if (MatchPattern(codec_id, type_info.codecs[k]->pattern) &&
              (!type_info.codecs[k]->validator ||
               type_info.codecs[k]->validator(codec_id, log_cb))) {
            found_codec =
                VerifyCodec(type_info.codecs[k], audio_codecs, video_codecs);
            break;  // Since only 1 pattern will match, no need to check others.
          }
        }

        if (!found_codec) {
          MEDIA_LOG(log_cb) << "Codec '" << codec_id
                            << "' is not supported for '" << type << "'";
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
  return CheckTypeAndCodecs(type, codecs, LogCB(), NULL, NULL, NULL);
}

scoped_ptr<StreamParser> StreamParserFactory::Create(
    const std::string& type,
    const std::vector<std::string>& codecs,
    const LogCB& log_cb,
    bool* has_audio,
    bool* has_video) {
  scoped_ptr<StreamParser> stream_parser;
  ParserFactoryFunction factory_function;
  std::vector<CodecInfo::HistogramTag> audio_codecs;
  std::vector<CodecInfo::HistogramTag> video_codecs;
  *has_audio = false;
  *has_video = false;

  if (CheckTypeAndCodecs(type,
                         codecs,
                         log_cb,
                         &factory_function,
                         &audio_codecs,
                         &video_codecs)) {
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

    stream_parser.reset(factory_function(codecs, log_cb));
  }

  return stream_parser.Pass();
}

}  // namespace media
