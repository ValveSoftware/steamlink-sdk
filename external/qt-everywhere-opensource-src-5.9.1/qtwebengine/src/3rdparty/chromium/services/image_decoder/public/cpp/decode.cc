// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_decoder/public/cpp/decode.h"

#include "services/image_decoder/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace image_decoder {

namespace {

// Helper callback which owns an ImageDecoderPtr until invoked. This keeps the
// ImageDecoder pipe open just long enough to dispatch a reply, at which point
// the reply is forwarded to the wrapped |callback|.
void OnDecodeImage(mojom::ImageDecoderPtr decoder,
                   const mojom::ImageDecoder::DecodeImageCallback& callback,
                   const SkBitmap& bitmap) {
  callback.Run(bitmap);
}

// Called in the case of a connection error on an ImageDecoder proxy.
void OnConnectionError(
    const mojom::ImageDecoder::DecodeImageCallback& callback) {
  SkBitmap null_bitmap;
  callback.Run(null_bitmap);
}

}  // namespace

void Decode(service_manager::Connector* connector,
            const std::vector<uint8_t>& encoded_bytes,
            mojom::ImageCodec codec,
            bool shrink_to_fit,
            uint64_t max_size_in_bytes,
            const mojom::ImageDecoder::DecodeImageCallback& callback) {
  mojom::ImageDecoderPtr decoder;
  connector->ConnectToInterface(mojom::kServiceName, &decoder);
  decoder.set_connection_error_handler(
      base::Bind(&OnConnectionError, callback));
  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeImage(
      encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      base::Bind(&OnDecodeImage, base::Passed(&decoder), callback));
}

}  // namespace image_decoder
