// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OnRequestCanvasDrawListener_h
#define OnRequestCanvasDrawListener_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include <memory>

namespace blink {

class OnRequestCanvasDrawListener final
    : public GarbageCollectedFinalized<OnRequestCanvasDrawListener>,
      public CanvasDrawListener {
  USING_GARBAGE_COLLECTED_MIXIN(OnRequestCanvasDrawListener);

 public:
  ~OnRequestCanvasDrawListener();
  static OnRequestCanvasDrawListener* create(
      std::unique_ptr<WebCanvasCaptureHandler>);
  void sendNewFrame(sk_sp<SkImage>) override;

  DEFINE_INLINE_TRACE() {}

 private:
  OnRequestCanvasDrawListener(std::unique_ptr<WebCanvasCaptureHandler>);
};

}  // namespace blink

#endif
