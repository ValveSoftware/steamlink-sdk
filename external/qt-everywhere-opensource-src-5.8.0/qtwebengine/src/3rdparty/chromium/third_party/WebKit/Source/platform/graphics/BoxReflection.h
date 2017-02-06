// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxReflection_h
#define BoxReflection_h

#include "platform/PlatformExport.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

class SkImageFilter;
class SkMatrix;
class SkPicture;

namespace blink {

class FloatRect;

// A reflection, as created by -webkit-box-reflect. Consists of:
// * a direction (either vertical or horizontal)
// * an offset to be applied to the reflection after flipping about the
//   x- or y-axis, according to the direction
// * a mask image, which will be applied to the reflection before the
//   reflection matrix is applied
class PLATFORM_EXPORT BoxReflection {
public:
    enum ReflectionDirection {
        // Vertically flipped (to appear above or below).
        VerticalReflection,
        // Horizontally flipped (to appear to the left or right).
        HorizontalReflection,
    };

    BoxReflection(ReflectionDirection direction, float offset, PassRefPtr<SkPicture> mask = nullptr)
        : m_direction(direction), m_offset(offset), m_mask(mask) {}

    ReflectionDirection direction() const { return m_direction; }
    float offset() const { return m_offset; }
    SkPicture* mask() const { return m_mask.get(); }

    // Returns a matrix which maps points between the original content and its
    // reflection. Reflections are self-inverse, so this matrix can be used to
    // map in either direction.
    SkMatrix reflectionMatrix() const;

    // Maps a source rectangle to the destination rectangle it can affect,
    // including this reflection. Due to the symmetry of reflections, this can
    // also be used to map from a destination rectangle to the source rectangle
    // which contributes to it.
    FloatRect mapRect(const FloatRect&) const;

private:
    ReflectionDirection m_direction;
    float m_offset;
    RefPtr<SkPicture> m_mask;
};

inline bool operator==(const BoxReflection& a, const BoxReflection& b)
{
    return a.direction() == b.direction()
        && a.offset() == b.offset()
        && a.mask() == b.mask();
}

inline bool operator!=(const BoxReflection& a, const BoxReflection& b)
{
    return !(a == b);
}

} // namespace blink

#endif // BoxReflection_h
