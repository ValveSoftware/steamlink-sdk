// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_DECODER_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_DECODER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {

using ImageDecodedCallback = base::Callback<void(const gfx::Image&)>;

// ImageDecoder defines the common interface for decoding images.
class ImageDecoder {
 public:
  ImageDecoder() {}
  virtual ~ImageDecoder() {}

  // Decodes the passed |image_data| and runs the given callback. The callback
  // is run even if decoding the image fails. In case an error occured during
  // decoding the image an empty image is passed to the callback.
  virtual void DecodeImage(const std::string& image_data,
                           const ImageDecodedCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_DECODER_H_
