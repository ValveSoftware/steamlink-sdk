// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_image/user_image.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace user_manager {

namespace {

// Default quality for encoding user images.
const int kDefaultEncodingQuality = 90;

}  // namespace

// static
std::unique_ptr<UserImage::Bytes> UserImage::Encode(const SkBitmap& bitmap) {
  TRACE_EVENT2("oobe", "UserImage::Encode",
               "width", bitmap.width(), "height", bitmap.height());
  SkAutoLockPixels lock_bitmap(bitmap);
  std::unique_ptr<Bytes> output(new Bytes);
  if (gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          bitmap.width(),
          bitmap.height(),
          bitmap.width() * bitmap.bytesPerPixel(),
          kDefaultEncodingQuality, output.get())) {
    return output;
  } else {
    return nullptr;
  }
}

// static
std::unique_ptr<UserImage> UserImage::CreateAndEncode(
    const gfx::ImageSkia& image) {
  if (image.isNull())
    return base::WrapUnique(new UserImage);

  std::unique_ptr<Bytes> image_bytes = Encode(*image.bitmap());
  if (image_bytes) {
    // TODO(crbug.com/593251): Remove the data copy via |image_bytes|.
    std::unique_ptr<UserImage> result(new UserImage(image, *image_bytes));
    result->MarkAsSafe();
    return result;
  }
  return base::WrapUnique(new UserImage(image));
}

UserImage::UserImage()
    : has_image_bytes_(false),
      is_safe_format_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image),
      has_image_bytes_(false),
      is_safe_format_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     const Bytes& image_bytes)
    : image_(image),
      has_image_bytes_(false),
      is_safe_format_(false) {
  has_image_bytes_ = true;
  image_bytes_ = image_bytes;
}

UserImage::~UserImage() {}

void UserImage::MarkAsSafe() {
  is_safe_format_ = true;
}

}  // namespace user_manager
