// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImagePattern_h
#define ImagePattern_h

#include "platform/graphics/Pattern.h"

class SkImage;

namespace blink {

class Image;

class PLATFORM_EXPORT ImagePattern final : public Pattern {
public:
    static PassRefPtr<ImagePattern> create(PassRefPtr<Image>, RepeatMode);

    bool isTextureBacked() const override;

protected:
    sk_sp<SkShader> createShader(const SkMatrix&) const override;

private:
    ImagePattern(PassRefPtr<Image>, RepeatMode);

    sk_sp<SkImage> m_tileImage;
};

} // namespace blink

#endif  /* ImagePattern_h */
