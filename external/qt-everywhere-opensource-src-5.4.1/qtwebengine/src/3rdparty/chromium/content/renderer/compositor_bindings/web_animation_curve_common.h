// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_CURVE_COMMON_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_CURVE_COMMON_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebAnimationCurve.h"

namespace cc {
class TimingFunction;
}

namespace content {
scoped_ptr<cc::TimingFunction> CreateTimingFunction(
    blink::WebAnimationCurve::TimingFunctionType);
}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_CURVE_COMMON_H_

