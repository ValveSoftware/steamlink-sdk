// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/property_type_converters.h"

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Maximum allowed height or width of a bitmap, in pixels. This limit prevents
// malformed bitmap headers from causing arbitrarily large memory allocations
// for pixel data.
const int kMaxBitmapSize = 4096;

}  // namespace

namespace mojo {

// static
std::vector<uint8_t> TypeConverter<std::vector<uint8_t>, gfx::Rect>::Convert(
    const gfx::Rect& input) {
  std::vector<uint8_t> vec(16);
  vec[0] = (input.x() >> 24) & 0xFF;
  vec[1] = (input.x() >> 16) & 0xFF;
  vec[2] = (input.x() >> 8) & 0xFF;
  vec[3] = input.x() & 0xFF;
  vec[4] = (input.y() >> 24) & 0xFF;
  vec[5] = (input.y() >> 16) & 0xFF;
  vec[6] = (input.y() >> 8) & 0xFF;
  vec[7] = input.y() & 0xFF;
  vec[8] = (input.width() >> 24) & 0xFF;
  vec[9] = (input.width() >> 16) & 0xFF;
  vec[10] = (input.width() >> 8) & 0xFF;
  vec[11] = input.width() & 0xFF;
  vec[12] = (input.height() >> 24) & 0xFF;
  vec[13] = (input.height() >> 16) & 0xFF;
  vec[14] = (input.height() >> 8) & 0xFF;
  vec[15] = input.height() & 0xFF;
  return vec;
}

// static
gfx::Rect TypeConverter<gfx::Rect, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  return gfx::Rect(
      input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3],
      input[4] << 24 | input[5] << 16 | input[6] << 8 | input[7],
      input[8] << 24 | input[9] << 16 | input[10] << 8 | input[11],
      input[12] << 24 | input[13] << 16 | input[14] << 8 | input[15]);
}

// static
std::vector<uint8_t> TypeConverter<std::vector<uint8_t>, gfx::Size>::Convert(
    const gfx::Size& input) {
  std::vector<uint8_t> vec(8);
  vec[0] = (input.width() >> 24) & 0xFF;
  vec[1] = (input.width() >> 16) & 0xFF;
  vec[2] = (input.width() >> 8) & 0xFF;
  vec[3] = input.width() & 0xFF;
  vec[4] = (input.height() >> 24) & 0xFF;
  vec[5] = (input.height() >> 16) & 0xFF;
  vec[6] = (input.height() >> 8) & 0xFF;
  vec[7] = input.height() & 0xFF;
  return vec;
}

// static
gfx::Size TypeConverter<gfx::Size, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  return gfx::Size(input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3],
                   input[4] << 24 | input[5] << 16 | input[6] << 8 | input[7]);
}

// static
std::vector<uint8_t> TypeConverter<std::vector<uint8_t>, int32_t>::Convert(
    const int32_t& input) {
  std::vector<uint8_t> vec(4);
  vec[0] = (input >> 24) & 0xFF;
  vec[1] = (input >> 16) & 0xFF;
  vec[2] = (input >> 8) & 0xFF;
  vec[3] = input & 0xFF;
  return vec;
}

// static
int32_t TypeConverter<int32_t, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  return input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3];
}

// static
std::vector<uint8_t>
TypeConverter<std::vector<uint8_t>, base::string16>::Convert(
    const base::string16& input) {
  return ConvertTo<std::vector<uint8_t>>(base::UTF16ToUTF8(input));
}

// static
base::string16 TypeConverter<base::string16, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  return base::UTF8ToUTF16(ConvertTo<std::string>(input));
}

// static
std::vector<uint8_t> TypeConverter<std::vector<uint8_t>, std::string>::Convert(
    const std::string& input) {
  return std::vector<uint8_t>(input.begin(), input.end());
}

// static
std::string TypeConverter<std::string, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  return std::string(input.begin(), input.end());
}

// static
std::vector<uint8_t> TypeConverter<std::vector<uint8_t>, SkBitmap>::Convert(
    const SkBitmap& input) {
  // Empty images are valid to serialize and are represented by an empty vector.
  if (input.isNull())
    return std::vector<uint8_t>();

  // Only RGBA 8888 bitmaps with premultiplied alpha are supported.
  if (input.colorType() != kBGRA_8888_SkColorType ||
      input.alphaType() != kPremul_SkAlphaType) {
    NOTREACHED();
    return std::vector<uint8_t>();
  }

  // Sanity check the bitmap size.
  int width = input.width();
  int height = input.height();
  if (width < 0 || width > kMaxBitmapSize || height < 0 ||
      height > kMaxBitmapSize) {
    NOTREACHED();
    return std::vector<uint8_t>();
  }

  // Serialize the bitmap. The size is restricted so only 2 bytes are required
  // per dimension.
  std::vector<uint8_t> vec(4 + input.getSize());
  vec[0] = (width >> 8) & 0xFF;
  vec[1] = width & 0xFF;
  vec[2] = (height >> 8) & 0xFF;
  vec[3] = height & 0xFF;
  if (!input.copyPixelsTo(&vec[4], input.getSize()))
    return std::vector<uint8_t>();
  return vec;
}

// static
SkBitmap TypeConverter<SkBitmap, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  // Empty images are represented by empty vectors.
  if (input.empty())
    return SkBitmap();

  // Read and sanity check size.
  int width = input[0] << 8 | input[1];
  int height = input[2] << 8 | input[3];
  if (width < 0 || width > kMaxBitmapSize || height < 0 ||
      height > kMaxBitmapSize) {
    NOTREACHED();
    return SkBitmap();
  }

  // Try to allocate a bitmap of the appropriate size.
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(SkImageInfo::Make(
          width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType))) {
    return SkBitmap();
  }

  // Ensure the vector contains the right amount of data.
  if (input.size() != bitmap.getSize() + 4) {
    NOTREACHED();
    return SkBitmap();
  }

  // Read the pixel data.
  SkAutoLockPixels lock(bitmap);
  memcpy(bitmap.getPixels(), &input[4], bitmap.getSize());
  return bitmap;
}

// static
std::vector<uint8_t> TypeConverter<std::vector<uint8_t>, bool>::Convert(
    bool input) {
  std::vector<uint8_t> vec(1);
  vec[0] = input ? 1 : 0;
  return vec;
}

// static
bool TypeConverter<bool, std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  // Empty vectors are interpreted as false.
  return !input.empty() && (input[0] == 1);
}

}  // namespace mojo
