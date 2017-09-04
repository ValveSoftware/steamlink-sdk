// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_colour_parser.h"

#include "base/logging.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

// The definitions below are copied from the current libwebm top-of-tree.
// Chromium's third_party/libwebm doesn't have these yet. We can probably just
// include libwebm header directly in the future.
// ---- Begin copy/paste from libwebm/webm_parser/include/webm/dom_types.h ----

/**
 A parsed \WebMID{MatrixCoefficients} element.

 Matroska/WebM adopted these values from Table 4 of ISO/IEC 23001-8:2013/DCOR1.
 See that document for further details.
 */
enum class MatrixCoefficients : std::uint64_t {
  /**
   The identity matrix.

   Typically used for GBR (often referred to as RGB); however, may also be used
   for YZX (often referred to as XYZ).
   */
  kRgb = 0,

  /**
   Rec. ITU-R BT.709-5.
   */
  kBt709 = 1,

  /**
   Image characteristics are unknown or are determined by the application.
   */
  kUnspecified = 2,

  /**
   United States Federal Communications Commission Title 47 Code of Federal
   Regulations (2003) 73.682 (a) (20).
   */
  kFcc = 4,

  /**
   Rec. ITU-R BT.470‑6 System B, G (historical).
   */
  kBt470Bg = 5,

  /**
   Society of Motion Picture and Television Engineers 170M (2004).
   */
  kSmpte170M = 6,

  /**
   Society of Motion Picture and Television Engineers 240M (1999).
   */
  kSmpte240M = 7,

  /**
   YCgCo.
   */
  kYCgCo = 8,

  /**
   Rec. ITU-R BT.2020 (non-constant luminance).
   */
  kBt2020NonconstantLuminance = 9,

  /**
   Rec. ITU-R BT.2020 (constant luminance).
   */
  kBt2020ConstantLuminance = 10,
};

/**
 A parsed \WebMID{Range} element.
 */
enum class Range : std::uint64_t {
  /**
   Unspecified.
   */
  kUnspecified = 0,

  /**
   Broadcast range.
   */
  kBroadcast = 1,

  /**
   Full range (no clipping).
   */
  kFull = 2,

  /**
   Defined by MatrixCoefficients/TransferCharacteristics.
   */
  kDerived = 3,
};

/**
 A parsed \WebMID{TransferCharacteristics} element.

 Matroska/WebM adopted these values from Table 3 of ISO/IEC 23001-8:2013/DCOR1.
 See that document for further details.
 */
enum class TransferCharacteristics : std::uint64_t {
  /**
   Rec. ITU-R BT.709-6.
   */
  kBt709 = 1,

  /**
   Image characteristics are unknown or are determined by the application.
   */
  kUnspecified = 2,

  /**
   Rec. ITU‑R BT.470‑6 System M (historical) with assumed display gamma 2.2.
   */
  kGamma22curve = 4,

  /**
   Rec. ITU‑R BT.470-6 System B, G (historical) with assumed display gamma 2.8.
   */
  kGamma28curve = 5,

  /**
   Society of Motion Picture and Television Engineers 170M (2004).
   */
  kSmpte170M = 6,

  /**
   Society of Motion Picture and Television Engineers 240M (1999).
   */
  kSmpte240M = 7,

  /**
   Linear transfer characteristics.
   */
  kLinear = 8,

  /**
   Logarithmic transfer characteristic (100:1 range).
   */
  kLog = 9,

  /**
   Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range).
   */
  kLogSqrt = 10,

  /**
   IEC 61966-2-4.
   */
  kIec6196624 = 11,

  /**
   Rec. ITU‑R BT.1361-0 extended colour gamut system (historical).
   */
  kBt1361ExtendedColourGamut = 12,

  /**
   IEC 61966-2-1 sRGB or sYCC.
   */
  kIec6196621 = 13,

  /**
   Rec. ITU-R BT.2020-2 (10-bit system).
   */
  k10BitBt2020 = 14,

  /**
   Rec. ITU-R BT.2020-2 (12-bit system).
   */
  k12BitBt2020 = 15,

  /**
   Society of Motion Picture and Television Engineers ST 2084.
   */
  kSmpteSt2084 = 16,

  /**
   Society of Motion Picture and Television Engineers ST 428-1.
   */
  kSmpteSt4281 = 17,

  /**
   Association of Radio Industries and Businesses (ARIB) STD-B67.
   */
  kAribStdB67Hlg = 18,
};

/**
 A parsed \WebMID{Primaries} element.

 Matroska/WebM adopted these values from Table 2 of ISO/IEC 23001-8:2013/DCOR1.
 See that document for further details.
 */
enum class Primaries : std::uint64_t {
  /**
   Rec. ITU‑R BT.709-6.
   */
  kBt709 = 1,

  /**
   Image characteristics are unknown or are determined by the application.
   */
  kUnspecified = 2,

  /**
   Rec. ITU‑R BT.470‑6 System M (historical).
   */
  kBt470M = 4,

  /**
   Rec. ITU‑R BT.470‑6 System B, G (historical).
   */
  kBt470Bg = 5,

  /**
   Society of Motion Picture and Television Engineers 170M (2004).
   */
  kSmpte170M = 6,

  /**
   Society of Motion Picture and Television Engineers 240M (1999).
   */
  kSmpte240M = 7,

  /**
   Generic film.
   */
  kFilm = 8,

  /**
   Rec. ITU-R BT.2020-2.
   */
  kBt2020 = 9,

  /**
   Society of Motion Picture and Television Engineers ST 428-1.
   */
  kSmpteSt4281 = 10,

  /**
   JEDEC P22 phosphors/EBU Tech. 3213-E (1975).
   */
  kJedecP22Phosphors = 22,
};

// ---- End copy/paste from libwebm/webm_parser/include/webm/dom_types.h ----

// Ensure that libwebm enum values match enums in gfx::ColorSpace.
#define STATIC_ASSERT_ENUM(a, b)                                             \
  static_assert(static_cast<int>(a) == static_cast<int>(gfx::ColorSpace::b), \
                "mismatching enums: " #a " and " #b)

STATIC_ASSERT_ENUM(MatrixCoefficients::kRgb, MatrixID::RGB);
STATIC_ASSERT_ENUM(MatrixCoefficients::kBt709, MatrixID::BT709);
STATIC_ASSERT_ENUM(MatrixCoefficients::kUnspecified, MatrixID::UNSPECIFIED);
STATIC_ASSERT_ENUM(MatrixCoefficients::kFcc, MatrixID::FCC);
STATIC_ASSERT_ENUM(MatrixCoefficients::kBt470Bg, MatrixID::BT470BG);
STATIC_ASSERT_ENUM(MatrixCoefficients::kSmpte170M, MatrixID::SMPTE170M);
STATIC_ASSERT_ENUM(MatrixCoefficients::kSmpte240M, MatrixID::SMPTE240M);
STATIC_ASSERT_ENUM(MatrixCoefficients::kYCgCo, MatrixID::YCOCG);
STATIC_ASSERT_ENUM(MatrixCoefficients::kBt2020NonconstantLuminance,
                   MatrixID::BT2020_NCL);
STATIC_ASSERT_ENUM(MatrixCoefficients::kBt2020ConstantLuminance,
                   MatrixID::BT2020_CL);

gfx::ColorSpace::MatrixID FromWebMMatrixCoefficients(MatrixCoefficients c) {
  return static_cast<gfx::ColorSpace::MatrixID>(c);
}

STATIC_ASSERT_ENUM(Range::kUnspecified, RangeID::UNSPECIFIED);
STATIC_ASSERT_ENUM(Range::kBroadcast, RangeID::LIMITED);
STATIC_ASSERT_ENUM(Range::kFull, RangeID::FULL);
STATIC_ASSERT_ENUM(Range::kDerived, RangeID::DERIVED);

gfx::ColorSpace::RangeID FromWebMRange(Range range) {
  return static_cast<gfx::ColorSpace::RangeID>(range);
}

STATIC_ASSERT_ENUM(TransferCharacteristics::kBt709, TransferID::BT709);
STATIC_ASSERT_ENUM(TransferCharacteristics::kUnspecified,
                   TransferID::UNSPECIFIED);
STATIC_ASSERT_ENUM(TransferCharacteristics::kGamma22curve, TransferID::GAMMA22);
STATIC_ASSERT_ENUM(TransferCharacteristics::kGamma28curve, TransferID::GAMMA28);
STATIC_ASSERT_ENUM(TransferCharacteristics::kSmpte170M, TransferID::SMPTE170M);
STATIC_ASSERT_ENUM(TransferCharacteristics::kSmpte240M, TransferID::SMPTE240M);
STATIC_ASSERT_ENUM(TransferCharacteristics::kLinear, TransferID::LINEAR);
STATIC_ASSERT_ENUM(TransferCharacteristics::kLog, TransferID::LOG);
STATIC_ASSERT_ENUM(TransferCharacteristics::kLogSqrt, TransferID::LOG_SQRT);
STATIC_ASSERT_ENUM(TransferCharacteristics::kIec6196624,
                   TransferID::IEC61966_2_4);
STATIC_ASSERT_ENUM(TransferCharacteristics::kBt1361ExtendedColourGamut,
                   TransferID::BT1361_ECG);
STATIC_ASSERT_ENUM(TransferCharacteristics::kIec6196621,
                   TransferID::IEC61966_2_1);
STATIC_ASSERT_ENUM(TransferCharacteristics::k10BitBt2020,
                   TransferID::BT2020_10);
STATIC_ASSERT_ENUM(TransferCharacteristics::k12BitBt2020,
                   TransferID::BT2020_12);
STATIC_ASSERT_ENUM(TransferCharacteristics::kSmpteSt2084,
                   TransferID::SMPTEST2084);
STATIC_ASSERT_ENUM(TransferCharacteristics::kSmpteSt4281,
                   TransferID::SMPTEST428_1);
STATIC_ASSERT_ENUM(TransferCharacteristics::kAribStdB67Hlg,
                   TransferID::ARIB_STD_B67);

gfx::ColorSpace::TransferID FromWebMTransferCharacteristics(
    TransferCharacteristics tc) {
  return static_cast<gfx::ColorSpace::TransferID>(tc);
}

STATIC_ASSERT_ENUM(Primaries::kBt709, PrimaryID::BT709);
STATIC_ASSERT_ENUM(Primaries::kUnspecified, PrimaryID::UNSPECIFIED);
STATIC_ASSERT_ENUM(Primaries::kBt470M, PrimaryID::BT470M);
STATIC_ASSERT_ENUM(Primaries::kBt470Bg, PrimaryID::BT470BG);
STATIC_ASSERT_ENUM(Primaries::kSmpte170M, PrimaryID::SMPTE170M);
STATIC_ASSERT_ENUM(Primaries::kSmpte240M, PrimaryID::SMPTE240M);
STATIC_ASSERT_ENUM(Primaries::kFilm, PrimaryID::FILM);
STATIC_ASSERT_ENUM(Primaries::kBt2020, PrimaryID::BT2020);
STATIC_ASSERT_ENUM(Primaries::kSmpteSt4281, PrimaryID::SMPTEST428_1);

gfx::ColorSpace::PrimaryID FromWebMPrimaries(Primaries primaries) {
  return static_cast<gfx::ColorSpace::PrimaryID>(primaries);
}

WebMColorMetadata::WebMColorMetadata() {}
WebMColorMetadata::WebMColorMetadata(const WebMColorMetadata& rhs) = default;

WebMMasteringMetadataParser::WebMMasteringMetadataParser() {}
WebMMasteringMetadataParser::~WebMMasteringMetadataParser() {}

bool WebMMasteringMetadataParser::OnFloat(int id, double val) {
  switch (id) {
    case kWebMIdPrimaryRChromaticityX:
      mastering_metadata_.primary_r_chromaticity_x = val;
      break;
    case kWebMIdPrimaryRChromaticityY:
      mastering_metadata_.primary_r_chromaticity_y = val;
      break;
    case kWebMIdPrimaryGChromaticityX:
      mastering_metadata_.primary_g_chromaticity_x = val;
      break;
    case kWebMIdPrimaryGChromaticityY:
      mastering_metadata_.primary_g_chromaticity_y = val;
      break;
    case kWebMIdPrimaryBChromaticityX:
      mastering_metadata_.primary_b_chromaticity_x = val;
      break;
    case kWebMIdPrimaryBChromaticityY:
      mastering_metadata_.primary_b_chromaticity_y = val;
      break;
    case kWebMIdWhitePointChromaticityX:
      mastering_metadata_.white_point_chromaticity_x = val;
      break;
    case kWebMIdWhitePointChromaticityY:
      mastering_metadata_.white_point_chromaticity_y = val;
      break;
    case kWebMIdLuminanceMax:
      mastering_metadata_.luminance_max = val;
      break;
    case kWebMIdLuminanceMin:
      mastering_metadata_.luminance_min = val;
      break;
    default:
      DVLOG(1) << "Unexpected id in MasteringMetadata: 0x" << std::hex << id;
      return false;
  }
  return true;
}

WebMColourParser::WebMColourParser() {
  Reset();
}

WebMColourParser::~WebMColourParser() {}

void WebMColourParser::Reset() {
  matrix_coefficients_ = -1;
  bits_per_channel_ = -1;
  chroma_subsampling_horz_ = -1;
  chroma_subsampling_vert_ = -1;
  cb_subsampling_horz_ = -1;
  cb_subsampling_vert_ = -1;
  chroma_siting_horz_ = -1;
  chroma_siting_vert_ = -1;
  range_ = -1;
  transfer_characteristics_ = -1;
  primaries_ = -1;
  max_cll_ = -1;
  max_fall_ = -1;
}

WebMParserClient* WebMColourParser::OnListStart(int id) {
  if (id == kWebMIdMasteringMetadata) {
    mastering_metadata_parsed_ = false;
    return &mastering_metadata_parser_;
  }

  return this;
}

bool WebMColourParser::OnListEnd(int id) {
  if (id == kWebMIdMasteringMetadata)
    mastering_metadata_parsed_ = true;
  return true;
}

bool WebMColourParser::OnUInt(int id, int64_t val) {
  int64_t* dst = nullptr;

  switch (id) {
    case kWebMIdMatrixCoefficients:
      dst = &matrix_coefficients_;
      break;
    case kWebMIdBitsPerChannel:
      dst = &bits_per_channel_;
      break;
    case kWebMIdChromaSubsamplingHorz:
      dst = &chroma_subsampling_horz_;
      break;
    case kWebMIdChromaSubsamplingVert:
      dst = &chroma_subsampling_vert_;
      break;
    case kWebMIdCbSubsamplingHorz:
      dst = &cb_subsampling_horz_;
      break;
    case kWebMIdCbSubsamplingVert:
      dst = &cb_subsampling_vert_;
      break;
    case kWebMIdChromaSitingHorz:
      dst = &chroma_siting_horz_;
      break;
    case kWebMIdChromaSitingVert:
      dst = &chroma_siting_vert_;
      break;
    case kWebMIdRange:
      dst = &range_;
      break;
    case kWebMIdTransferCharacteristics:
      dst = &transfer_characteristics_;
      break;
    case kWebMIdPrimaries:
      dst = &primaries_;
      break;
    case kWebMIdMaxCLL:
      dst = &max_cll_;
      break;
    case kWebMIdMaxFALL:
      dst = &max_fall_;
      break;
    default:
      return true;
  }

  DCHECK(dst);
  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified ("
               << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

WebMColorMetadata WebMColourParser::GetWebMColorMetadata() const {
  WebMColorMetadata color_metadata;

  if (bits_per_channel_ != -1)
    color_metadata.BitsPerChannel = bits_per_channel_;

  if (chroma_subsampling_horz_ != -1)
    color_metadata.ChromaSubsamplingHorz = chroma_subsampling_horz_;
  if (chroma_subsampling_vert_ != -1)
    color_metadata.ChromaSubsamplingVert = chroma_subsampling_vert_;
  if (cb_subsampling_horz_ != -1)
    color_metadata.CbSubsamplingHorz = cb_subsampling_horz_;
  if (cb_subsampling_vert_ != -1)
    color_metadata.CbSubsamplingVert = cb_subsampling_vert_;
  if (chroma_siting_horz_ != -1)
    color_metadata.ChromaSitingHorz = chroma_siting_horz_;
  if (chroma_siting_vert_ != -1)
    color_metadata.ChromaSitingVert = chroma_siting_vert_;

  gfx::ColorSpace::MatrixID matrix_id = gfx::ColorSpace::MatrixID::UNSPECIFIED;
  if (matrix_coefficients_ != -1)
    matrix_id = FromWebMMatrixCoefficients(
        static_cast<MatrixCoefficients>(matrix_coefficients_));

  gfx::ColorSpace::RangeID range_id = gfx::ColorSpace::RangeID::UNSPECIFIED;
  if (range_ != -1)
    range_id = FromWebMRange(static_cast<Range>(range_));

  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::UNSPECIFIED;
  if (transfer_characteristics_ != -1)
    transfer_id = FromWebMTransferCharacteristics(
        static_cast<TransferCharacteristics>(transfer_characteristics_));

  gfx::ColorSpace::PrimaryID primary_id =
      gfx::ColorSpace::PrimaryID::UNSPECIFIED;
  if (primaries_ != -1)
    primary_id = FromWebMPrimaries(static_cast<Primaries>(primaries_));

  color_metadata.color_space =
      gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);

  if (max_cll_ != -1)
    color_metadata.hdr_metadata.max_cll = max_cll_;

  if (max_fall_ != -1)
    color_metadata.hdr_metadata.max_fall = max_fall_;

  if (mastering_metadata_parsed_)
    color_metadata.hdr_metadata.mastering_metadata =
        mastering_metadata_parser_.GetMasteringMetadata();

  return color_metadata;
}

}  // namespace media
