// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

template <typename T>
class sk_sp;
class SkColorSpace;

namespace gfx {

class ICCProfile;
class ColorSpaceToColorSpaceTransform;

// Used to represet a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class GFX_EXPORT ColorSpace {
 public:
  enum class PrimaryID : uint16_t {
    // The first 0-255 values should match the H264 specification (see Table E-3
    // Colour Primaries in https://www.itu.int/rec/T-REC-H.264/en).
    RESERVED0 = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    RESERVED = 3,
    BT470M = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    FILM = 8,
    BT2020 = 9,
    SMPTEST428_1 = 10,
    SMPTEST431_2 = 11,
    SMPTEST432_1 = 12,

    LAST_STANDARD_VALUE = SMPTEST432_1,

    // Chrome-specific values start at 1000.
    UNKNOWN = 1000,
    XYZ_D50,
    CUSTOM,
    LAST = CUSTOM
  };

  enum class TransferID : uint16_t {
    // The first 0-255 values should match the H264 specification (see Table E-4
    // Transfer Characteristics in https://www.itu.int/rec/T-REC-H.264/en).
    RESERVED0 = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    RESERVED = 3,
    GAMMA22 = 4,
    GAMMA28 = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    LINEAR = 8,
    LOG = 9,
    LOG_SQRT = 10,
    IEC61966_2_4 = 11,
    BT1361_ECG = 12,
    IEC61966_2_1 = 13,
    BT2020_10 = 14,
    BT2020_12 = 15,
    SMPTEST2084 = 16,
    SMPTEST428_1 = 17,
    ARIB_STD_B67 = 18,  // AKA hybrid-log gamma, HLG.

    LAST_STANDARD_VALUE = SMPTEST428_1,

    // Chrome-specific values start at 1000.
    UNKNOWN = 1000,
    GAMMA24,

    // This is an ad-hoc transfer function that decodes SMPTE 2084 content
    // into a 0-1 range more or less suitable for viewing on a non-hdr
    // display.
    SMPTEST2084_NON_HDR,

    // TODO(hubbe): Need to store an approximation of the gamma function(s).
    CUSTOM,
    LAST = CUSTOM,
  };

  enum class MatrixID : int16_t {
    // The first 0-255 values should match the H264 specification (see Table E-5
    // Matrix Coefficients in https://www.itu.int/rec/T-REC-H.264/en).
    RGB = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    RESERVED = 3,
    FCC = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    YCOCG = 8,
    BT2020_NCL = 9,
    BT2020_CL = 10,
    YDZDX = 11,

    LAST_STANDARD_VALUE = YDZDX,

    // Chrome-specific values start at 1000
    UNKNOWN = 1000,
    LAST = UNKNOWN,
  };

  // This corresponds to the WebM Range enum which is part of WebM color data
  // (see http://www.webmproject.org/docs/container/#Range).
  // H.264 only uses a bool, which corresponds to the LIMITED/FULL values.
  // Chrome-specific values start at 1000.
  enum class RangeID : int8_t {
    // Range is not explicitly specified / unknown.
    UNSPECIFIED = 0,

    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    LIMITED = 1,

    // Full RGB color range with RGB valees from 0 to 255.
    FULL = 2,

    // Range is defined by TransferID/MatrixID.
    DERIVED = 3,

    LAST = DERIVED
  };

  ColorSpace();
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID full_range);
  ColorSpace(int primaries, int transfer, int matrix, RangeID full_range);

  static PrimaryID PrimaryIDFromInt(int primary_id);
  static TransferID TransferIDFromInt(int transfer_id);
  static MatrixID MatrixIDFromInt(int matrix_id);

  static ColorSpace CreateSRGB();
  static ColorSpace CreateXYZD50();

  // TODO: Remove these, and replace with more generic constructors.
  static ColorSpace CreateJpeg();
  static ColorSpace CreateREC601();
  static ColorSpace CreateREC709();

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;

  // Note that this may return nullptr.
  sk_sp<SkColorSpace> ToSkColorSpace() const;
  static ColorSpace FromSkColorSpace(const sk_sp<SkColorSpace>& sk_color_space);

 private:
  PrimaryID primaries_ = PrimaryID::UNSPECIFIED;
  TransferID transfer_ = TransferID::UNSPECIFIED;
  MatrixID matrix_ = MatrixID::UNSPECIFIED;
  RangeID range_ = RangeID::LIMITED;

  // Only used if primaries_ == PrimaryID::CUSTOM
  float custom_primary_matrix_[12];

  // This is used to look up the ICCProfile from which this ColorSpace was
  // created, if possible.
  uint64_t icc_profile_id_ = 0;

  friend class ICCProfile;
  friend class ColorSpaceToColorSpaceTransform;
  friend struct IPC::ParamTraits<gfx::ColorSpace>;
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
