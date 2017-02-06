// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_IMAGE_DECODER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_IMAGE_DECODER_H_

#include <stddef.h>

class SkBitmap;

namespace blimp {
namespace client {

// DecodeBlimpImage is an implementation of SkPicture::InstallPixelRefProc
// which is used by the client to decode WebP images that are part of an
// SkPicture.
bool DecodeBlimpImage(const void* input, size_t input_size, SkBitmap* bitmap);

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_IMAGE_DECODER_H_
