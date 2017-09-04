// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMAGE_DECODER_IMPL_H_
#define CHROME_UTILITY_IMAGE_DECODER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "services/image_decoder/public/interfaces/image_decoder.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace image_decoder {

class ImageDecoderImpl : public mojom::ImageDecoder {
 public:
  explicit ImageDecoderImpl(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~ImageDecoderImpl() override;

  // Overridden from mojom::ImageDecoder:
  void DecodeImage(
      const std::vector<uint8_t>& encoded_data,
      mojom::ImageCodec codec,
      bool shrink_to_fit,
      int64_t max_size_in_bytes,
      const DecodeImageCallback& callback) override;

 private:
  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoderImpl);
};

}  // namespace image_decoder

#endif  // CHROME_UTILITY_IMAGE_DECODER_IMPL_H_
