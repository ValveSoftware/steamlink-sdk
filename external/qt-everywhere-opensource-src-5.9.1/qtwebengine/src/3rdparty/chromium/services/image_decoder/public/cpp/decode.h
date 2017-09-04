// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_DECODER_PUBLIC_CPP_DECODE_H_
#define SERVICES_IMAGE_DECODER_PUBLIC_CPP_DECODE_H_

#include <stdint.h>

#include <vector>

#include "services/image_decoder/public/interfaces/image_decoder.mojom.h"

namespace service_manager {
class Connector;
}

namespace image_decoder {

const uint64_t kDefaultMaxSizeInBytes = 128 * 1024 * 1024;

// Helper function to decode an image via the image_decoder service. Upon
// completion, |callback| is invoked on the calling thread TaskRunner with an
// SkBitmap argument. The SkBitmap will be null on failure and non-null on
// success.
void Decode(service_manager::Connector* connector,
            const std::vector<uint8_t>& encoded_bytes,
            mojom::ImageCodec codec,
            bool shrink_to_fit,
            uint64_t max_size_in_bytes,
            const mojom::ImageDecoder::DecodeImageCallback& callback);

}  // namespace image_decoder

#endif  // SERVICES_IMAGE_DECODER_PUBLIC_CPP_DECODE_H_
