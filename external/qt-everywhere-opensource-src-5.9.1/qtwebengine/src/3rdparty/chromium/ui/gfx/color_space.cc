// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/icc_profile.h"

namespace gfx {

ColorSpace::PrimaryID ColorSpace::PrimaryIDFromInt(int primary_id) {
  if (primary_id < 0 || primary_id > static_cast<int>(PrimaryID::LAST))
    return PrimaryID::UNKNOWN;
  if (primary_id > static_cast<int>(PrimaryID::LAST_STANDARD_VALUE) &&
      primary_id < 1000)
    return PrimaryID::UNKNOWN;
  return static_cast<PrimaryID>(primary_id);
}

ColorSpace::TransferID ColorSpace::TransferIDFromInt(int transfer_id) {
  if (transfer_id < 0 || transfer_id > static_cast<int>(TransferID::LAST))
    return TransferID::UNKNOWN;
  if (transfer_id > static_cast<int>(TransferID::LAST_STANDARD_VALUE) &&
      transfer_id < 1000)
    return TransferID::UNKNOWN;
  return static_cast<TransferID>(transfer_id);
}

ColorSpace::MatrixID ColorSpace::MatrixIDFromInt(int matrix_id) {
  if (matrix_id < 0 || matrix_id > static_cast<int>(MatrixID::LAST))
    return MatrixID::UNKNOWN;
  if (matrix_id > static_cast<int>(MatrixID::LAST_STANDARD_VALUE) &&
      matrix_id < 1000)
    return MatrixID::UNKNOWN;
  return static_cast<MatrixID>(matrix_id);
}

ColorSpace::ColorSpace() {
  memset(custom_primary_matrix_, 0, sizeof(custom_primary_matrix_));
}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range) {
  memset(custom_primary_matrix_, 0, sizeof(custom_primary_matrix_));
  // TODO: Set profile_id_
}

ColorSpace::ColorSpace(int primaries, int transfer, int matrix, RangeID range)
    : primaries_(PrimaryIDFromInt(primaries)),
      transfer_(TransferIDFromInt(transfer)),
      matrix_(MatrixIDFromInt(matrix)),
      range_(range) {
  memset(custom_primary_matrix_, 0, sizeof(custom_primary_matrix_));
  // TODO: Set profile_id_
}

// static
ColorSpace ColorSpace::CreateSRGB() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::RGB,
                    RangeID::FULL);
}

// Static
ColorSpace ColorSpace::CreateXYZD50() {
  return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateJpeg() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::BT709,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateREC601() {
  return ColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                    MatrixID::SMPTE170M, RangeID::LIMITED);
}

// static
ColorSpace ColorSpace::CreateREC709() {
  return ColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                    RangeID::LIMITED);
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  if (primaries_ != other.primaries_ || transfer_ != other.transfer_ ||
      matrix_ != other.matrix_ || range_ != other.range_)
    return false;
  if (primaries_ == PrimaryID::CUSTOM &&
      memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
             sizeof(custom_primary_matrix_)))
    return false;
  return true;
}

bool ColorSpace::operator!=(const ColorSpace& other) const {
  return !(*this == other);
}

bool ColorSpace::operator<(const ColorSpace& other) const {
  if (primaries_ < other.primaries_)
    return true;
  if (primaries_ > other.primaries_)
    return false;
  if (transfer_ < other.transfer_)
    return true;
  if (transfer_ > other.transfer_)
    return false;
  if (matrix_ < other.matrix_)
    return true;
  if (matrix_ > other.matrix_)
    return false;
  if (range_ < other.range_)
    return true;
  if (range_ > other.range_)
    return false;
  if (primaries_ == PrimaryID::CUSTOM) {
    int primary_result =
        memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
               sizeof(custom_primary_matrix_));
    if (primary_result < 0)
      return true;
    if (primary_result > 0)
      return false;
  }
  return false;
}

sk_sp<SkColorSpace> ColorSpace::ToSkColorSpace() const {
  // Unspecified color spaces are represented as nullptr SkColorSpaces.
  if (*this == gfx::ColorSpace())
    return nullptr;
  if (*this == gfx::ColorSpace::CreateSRGB())
    return SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);

  // TODO(crbug.com/634102): Support more than just ICC profile based color
  // spaces. The DCHECK here is to ensure that callers that expect a valid
  // result are notified of this incomplete functionality.
  std::vector<char> icc_data = gfx::ICCProfile::FromColorSpace(*this).GetData();
  sk_sp<SkColorSpace> result =
      SkColorSpace::MakeICC(icc_data.data(), icc_data.size());
  DCHECK(result);
  return result;
}

ColorSpace ColorSpace::FromSkColorSpace(
    const sk_sp<SkColorSpace>& sk_color_space) {
  if (!sk_color_space)
    return gfx::ColorSpace();
  if (SkColorSpace::Equals(
          sk_color_space.get(),
          SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named).get()))
    return gfx::ColorSpace::CreateSRGB();

  // TODO(crbug.com/634102): Add conversion to gfx::ColorSpace.
  return gfx::ColorSpace();
}

}  // namespace gfx
