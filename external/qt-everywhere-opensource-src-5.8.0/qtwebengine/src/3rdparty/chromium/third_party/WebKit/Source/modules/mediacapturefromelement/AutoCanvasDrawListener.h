// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoCanvasDrawListener_h
#define AutoCanvasDrawListener_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include <memory>

namespace blink {

class AutoCanvasDrawListener final : public GarbageCollectedFinalized<AutoCanvasDrawListener>, public CanvasDrawListener {
    USING_GARBAGE_COLLECTED_MIXIN(AutoCanvasDrawListener);
public:
    static AutoCanvasDrawListener* create(std::unique_ptr<WebCanvasCaptureHandler>);
    ~AutoCanvasDrawListener() {}

    DEFINE_INLINE_TRACE() {}
private:
    AutoCanvasDrawListener(std::unique_ptr<WebCanvasCaptureHandler>);
};

} // namespace blink

#endif
