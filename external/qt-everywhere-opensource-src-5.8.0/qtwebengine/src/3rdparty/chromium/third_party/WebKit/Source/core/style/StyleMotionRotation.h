// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleMotionRotation_h
#define StyleMotionRotation_h

#include "core/style/ComputedStyleConstants.h"

namespace blink {

struct StyleMotionRotation {
    StyleMotionRotation(float angle, MotionRotationType type)
        : angle(angle)
        , type(type)
    { }

    bool operator==(const StyleMotionRotation& other) const
    {
        return angle == other.angle
            && type == other.type;
    }
    bool operator!=(const StyleMotionRotation& other) const { return !(*this == other); }

    float angle;
    MotionRotationType type;
};

} // namespace blink

#endif // StyleMotionRotation_h
