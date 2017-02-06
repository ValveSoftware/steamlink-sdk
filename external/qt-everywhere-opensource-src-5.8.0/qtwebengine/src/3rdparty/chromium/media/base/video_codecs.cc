// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_codecs.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace media {

// The names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(VideoCodec codec) {
  switch (codec) {
    case kUnknownVideoCodec:
      return "unknown";
    case kCodecH264:
      return "h264";
    case kCodecHEVC:
      return "hevc";
    case kCodecVC1:
      return "vc1";
    case kCodecMPEG2:
      return "mpeg2video";
    case kCodecMPEG4:
      return "mpeg4";
    case kCodecTheora:
      return "theora";
    case kCodecVP8:
      return "vp8";
    case kCodecVP9:
      return "vp9";
  }
  NOTREACHED();
  return "";
}

std::string GetProfileName(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return "unknown";
    case H264PROFILE_BASELINE:
      return "h264 baseline";
    case H264PROFILE_MAIN:
      return "h264 main";
    case H264PROFILE_EXTENDED:
      return "h264 extended";
    case H264PROFILE_HIGH:
      return "h264 high";
    case H264PROFILE_HIGH10PROFILE:
      return "h264 high 10";
    case H264PROFILE_HIGH422PROFILE:
      return "h264 high 4:2:2";
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return "h264 high 4:4:4 predictive";
    case H264PROFILE_SCALABLEBASELINE:
      return "h264 scalable baseline";
    case H264PROFILE_SCALABLEHIGH:
      return "h264 scalable high";
    case H264PROFILE_STEREOHIGH:
      return "h264 stereo high";
    case H264PROFILE_MULTIVIEWHIGH:
      return "h264 multiview high";
    case HEVCPROFILE_MAIN:
      return "hevc main";
    case HEVCPROFILE_MAIN10:
      return "hevc main 10";
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return "hevc main still-picture";
    case VP8PROFILE_ANY:
      return "vp8";
    case VP9PROFILE_PROFILE0:
      return "vp9 profile0";
    case VP9PROFILE_PROFILE1:
      return "vp9 profile1";
    case VP9PROFILE_PROFILE2:
      return "vp9 profile2";
    case VP9PROFILE_PROFILE3:
      return "vp9 profile3";
  }
  NOTREACHED();
  return "";
}

bool ParseAVCCodecId(const std::string& codec_id,
                     VideoCodecProfile* profile,
                     uint8_t* level_idc) {
  // Make sure we have avc1.xxxxxx or avc3.xxxxxx , where xxxxxx are hex digits
  if (!base::StartsWith(codec_id, "avc1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "avc3.", base::CompareCase::SENSITIVE)) {
    return false;
  }
  uint32_t elem = 0;
  if (codec_id.size() != 11 ||
      !base::HexStringToUInt(base::StringPiece(codec_id).substr(5), &elem)) {
    DVLOG(4) << __FUNCTION__ << ": invalid avc codec id (" << codec_id << ")";
    return false;
  }

  uint8_t level_byte = elem & 0xFF;
  uint8_t constraints_byte = (elem >> 8) & 0xFF;
  uint8_t profile_idc = (elem >> 16) & 0xFF;

  // Check that the lower two bits of |constraints_byte| are zero (those are
  // reserved and must be zero according to ISO IEC 14496-10).
  if (constraints_byte & 3) {
    DVLOG(4) << __FUNCTION__ << ": non-zero reserved bits in codec id "
             << codec_id;
    return false;
  }

  VideoCodecProfile out_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  // profile_idc values for each profile are taken from ISO IEC 14496-10 and
  // https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC#Profiles
  switch (profile_idc) {
    case 66:
      out_profile = H264PROFILE_BASELINE;
      break;
    case 77:
      out_profile = H264PROFILE_MAIN;
      break;
    case 83:
      out_profile = H264PROFILE_SCALABLEBASELINE;
      break;
    case 86:
      out_profile = H264PROFILE_SCALABLEHIGH;
      break;
    case 88:
      out_profile = H264PROFILE_EXTENDED;
      break;
    case 100:
      out_profile = H264PROFILE_HIGH;
      break;
    case 110:
      out_profile = H264PROFILE_HIGH10PROFILE;
      break;
    case 118:
      out_profile = H264PROFILE_MULTIVIEWHIGH;
      break;
    case 122:
      out_profile = H264PROFILE_HIGH422PROFILE;
      break;
    case 128:
      out_profile = H264PROFILE_STEREOHIGH;
      break;
    case 244:
      out_profile = H264PROFILE_HIGH444PREDICTIVEPROFILE;
      break;
    default:
      DVLOG(1) << "Warning: unrecognized AVC/H.264 profile " << profile_idc;
      return false;
  }

  // TODO(servolk): Take into account also constraint set flags 3 through 5.
  uint8_t constraint_set0_flag = (constraints_byte >> 7) & 1;
  uint8_t constraint_set1_flag = (constraints_byte >> 6) & 1;
  uint8_t constraint_set2_flag = (constraints_byte >> 5) & 1;
  if (constraint_set2_flag && out_profile > H264PROFILE_EXTENDED) {
    out_profile = H264PROFILE_EXTENDED;
  }
  if (constraint_set1_flag && out_profile > H264PROFILE_MAIN) {
    out_profile = H264PROFILE_MAIN;
  }
  if (constraint_set0_flag && out_profile > H264PROFILE_BASELINE) {
    out_profile = H264PROFILE_BASELINE;
  }

  if (level_idc)
    *level_idc = level_byte;

  if (profile)
    *profile = out_profile;

  return true;
}

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
// The specification for HEVC codec id strings can be found in ISO IEC 14496-15
// dated 2012 or newer in the Annex E.3
bool ParseHEVCCodecId(const std::string& codec_id,
                      VideoCodecProfile* profile,
                      uint8_t* level_idc) {
  if (!base::StartsWith(codec_id, "hev1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "hvc1.", base::CompareCase::SENSITIVE)) {
    return false;
  }

  // HEVC codec id consists of:
  const int kMaxHevcCodecIdLength =
      5 +  // 'hev1.' or 'hvc1.' prefix (5 chars)
      4 +  // profile, e.g. '.A12' (max 4 chars)
      9 +  // profile_compatibility, dot + 32-bit hex number (max 9 chars)
      5 +  // tier and level, e.g. '.H120' (max 5 chars)
      18;  // up to 6 constraint bytes, bytes are dot-separated and hex-encoded.

  if (codec_id.size() > kMaxHevcCodecIdLength) {
    DVLOG(4) << __FUNCTION__ << ": Codec id is too long (" << codec_id << ")";
    return false;
  }

  std::vector<std::string> elem = base::SplitString(
      codec_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(elem[0] == "hev1" || elem[0] == "hvc1");

  if (elem.size() < 4) {
    DVLOG(4) << __FUNCTION__ << ": invalid HEVC codec id " << codec_id;
    return false;
  }

  uint8_t general_profile_space = 0;
  if (elem[1].size() > 0 &&
      (elem[1][0] == 'A' || elem[1][0] == 'B' || elem[1][0] == 'C')) {
    general_profile_space = 1 + (elem[1][0] - 'A');
    elem[1].erase(0, 1);
  }
  DCHECK(general_profile_space >= 0 && general_profile_space <= 3);

  unsigned general_profile_idc = 0;
  if (!base::StringToUint(elem[1], &general_profile_idc) ||
      general_profile_idc > 0x1f) {
    DVLOG(4) << __FUNCTION__ << ": invalid general_profile_idc=" << elem[1];
    return false;
  }

  uint32_t general_profile_compatibility_flags = 0;
  if (!base::HexStringToUInt(elem[2], &general_profile_compatibility_flags)) {
    DVLOG(4) << __FUNCTION__
             << ": invalid general_profile_compatibility_flags=" << elem[2];
    return false;
  }

  if (profile) {
    // TODO(servolk): Handle format range extension profiles as explained in
    // HEVC standard (ISO/IEC ISO/IEC 23008-2) section A.3.5
    if (general_profile_idc == 3 || (general_profile_compatibility_flags & 4)) {
      *profile = HEVCPROFILE_MAIN_STILL_PICTURE;
    }
    if (general_profile_idc == 2 || (general_profile_compatibility_flags & 2)) {
      *profile = HEVCPROFILE_MAIN10;
    }
    if (general_profile_idc == 1 || (general_profile_compatibility_flags & 1)) {
      *profile = HEVCPROFILE_MAIN;
    }
  }

  uint8_t general_tier_flag;
  if (elem[3].size() > 0 && (elem[3][0] == 'L' || elem[3][0] == 'H')) {
    general_tier_flag = (elem[3][0] == 'L') ? 0 : 1;
    elem[3].erase(0, 1);
  } else {
    DVLOG(4) << __FUNCTION__ << ": invalid general_tier_flag=" << elem[3];
    return false;
  }
  DCHECK(general_tier_flag == 0 || general_tier_flag == 1);

  unsigned general_level_idc = 0;
  if (!base::StringToUint(elem[3], &general_level_idc) ||
      general_level_idc > 0xff) {
    DVLOG(4) << __FUNCTION__ << ": invalid general_level_idc=" << elem[3];
    return false;
  }

  if (level_idc)
    *level_idc = static_cast<uint8_t>(general_level_idc);

  uint8_t constraint_flags[6];
  memset(constraint_flags, 0, sizeof(constraint_flags));

  if (elem.size() > 10) {
    DVLOG(4) << __FUNCTION__ << ": unexpected number of trailing bytes in HEVC "
             << "codec id " << codec_id;
    return false;
  }
  for (size_t i = 4; i < elem.size(); ++i) {
    unsigned constr_byte = 0;
    if (!base::HexStringToUInt(elem[i], &constr_byte) || constr_byte > 0xFF) {
      DVLOG(4) << __FUNCTION__ << ": invalid constraint byte=" << elem[i];
      return false;
    }
    constraint_flags[i] = constr_byte;
  }

  return true;
}
#endif

}  // namespace media
