// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_decoder/image_decoder_impl.h"

#include <string.h>

#include <utility>

#include "base/logging.h"
#include "content/public/child/image_decoder_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_CHROMEOS)
#include "ui/gfx/chromeos/codec/jpeg_codec_robust_slow.h"
#include "ui/gfx/codec/png_codec.h"
#endif

namespace image_decoder {

namespace {

int64_t kPadding = 64;

}  // namespace

ImageDecoderImpl::ImageDecoderImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

ImageDecoderImpl::~ImageDecoderImpl() = default;

void ImageDecoderImpl::DecodeImage(const std::vector<uint8_t>& encoded_data,
                                   mojom::ImageCodec codec,
                                   bool shrink_to_fit,
                                   int64_t max_size_in_bytes,
                                   const DecodeImageCallback& callback) {
  if (encoded_data.size() == 0) {
    callback.Run(SkBitmap());
    return;
  }

  SkBitmap decoded_image;
#if defined(OS_CHROMEOS)
  if (codec == mojom::ImageCodec::ROBUST_JPEG) {
    // Our robust jpeg decoding is using IJG libjpeg.
    if (encoded_data.size()) {
      std::unique_ptr<SkBitmap> decoded_jpeg(gfx::JPEGCodecRobustSlow::Decode(
          encoded_data.data(), encoded_data.size()));
      if (decoded_jpeg.get() && !decoded_jpeg->empty())
        decoded_image = *decoded_jpeg;
    }
  } else if (codec == mojom::ImageCodec::ROBUST_PNG) {
    // Our robust PNG decoding is using libpng.
    if (encoded_data.size()) {
      SkBitmap decoded_png;
      if (gfx::PNGCodec::Decode(
            encoded_data.data(), encoded_data.size(), &decoded_png)) {
        decoded_image = decoded_png;
      }
    }
  }
#endif  // defined(OS_CHROMEOS)
  if (codec == mojom::ImageCodec::DEFAULT) {
    decoded_image = content::DecodeImage(
        encoded_data.data(), gfx::Size(), encoded_data.size());
  }

  if (!decoded_image.isNull()) {
    // When serialized, the space taken up by a skia::mojom::Bitmap excluding
    // the pixel data payload should be:
    //   sizeof(skia::mojom::Bitmap::Data_) + pixel data array header (8 bytes)
    // Use a bigger number in case we need padding at the end.
    int64_t struct_size = sizeof(skia::mojom::Bitmap::Data_) + kPadding;
    int64_t image_size = decoded_image.computeSize64();
    int halves = 0;
    while (struct_size + (image_size >> 2 * halves) > max_size_in_bytes)
      halves++;
    if (halves) {
      // If the decoded image is too large, either discard it or shrink it.
      //
      // TODO(rockot): Also support exposing the bytes via shared memory for
      // larger images. https://crbug.com/416916.
      if (shrink_to_fit) {
        // Shrinking by halves prevents quality loss and should never overshrink
        // on displays smaller than 3600x2400.
        decoded_image = skia::ImageOperations::Resize(
            decoded_image, skia::ImageOperations::RESIZE_LANCZOS3,
            decoded_image.width() >> halves, decoded_image.height() >> halves);
      } else {
        decoded_image.reset();
      }
    }
  }

  callback.Run(decoded_image);
}

}  // namespace image_decoder
