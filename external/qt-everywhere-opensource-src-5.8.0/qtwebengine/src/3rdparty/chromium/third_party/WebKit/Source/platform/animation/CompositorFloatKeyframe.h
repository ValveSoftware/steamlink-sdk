// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatKeyframe_h
#define CompositorFloatKeyframe_h

namespace blink {

struct CompositorFloatKeyframe {
    CompositorFloatKeyframe(double time, float value)
        : time(time)
        , value(value)
    {
    }

    double time;
    float value;
};

} // namespace blink

#endif // CompositorFloatKeyframe_h
