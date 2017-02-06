// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/BoxReflection.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/effects/SkXfermodeImageFilter.h"

#include <utility>

namespace blink {

SkMatrix BoxReflection::reflectionMatrix() const
{
    SkMatrix flipMatrix;
    switch (m_direction) {
    case VerticalReflection:
        flipMatrix.setScale(1, -1);
        flipMatrix.postTranslate(0, m_offset);
        break;
    case HorizontalReflection:
        flipMatrix.setScale(-1, 1);
        flipMatrix.postTranslate(m_offset, 0);
        break;
    default:
        // MSVC requires that SkMatrix be initialized in this unreachable case.
        NOTREACHED();
        flipMatrix.reset();
        break;
    }
    return flipMatrix;
}

FloatRect BoxReflection::mapRect(const FloatRect& rect) const
{
    SkRect reflection(rect);
    reflectionMatrix().mapRect(&reflection);
    FloatRect result = rect;
    result.unite(reflection);
    return result;
}

} // namespace blink
