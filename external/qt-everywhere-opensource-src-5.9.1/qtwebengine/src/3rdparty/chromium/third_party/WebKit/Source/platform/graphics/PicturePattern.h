// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PicturePattern_h
#define PicturePattern_h

#include "platform/graphics/Pattern.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT PicturePattern final : public Pattern {
 public:
  static PassRefPtr<PicturePattern> create(sk_sp<SkPicture>, RepeatMode);

  ~PicturePattern() override;

 protected:
  sk_sp<SkShader> createShader(const SkMatrix&) override;

 private:
  PicturePattern(sk_sp<SkPicture>, RepeatMode);

  sk_sp<SkPicture> m_tilePicture;
};

}  // namespace blink

#endif
