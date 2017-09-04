// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TimedCanvasDrawListener_h
#define TimedCanvasDrawListener_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include <memory>

namespace blink {

class TimedCanvasDrawListener final
    : public GarbageCollectedFinalized<TimedCanvasDrawListener>,
      public CanvasDrawListener {
  USING_GARBAGE_COLLECTED_MIXIN(TimedCanvasDrawListener);

 public:
  ~TimedCanvasDrawListener();
  static TimedCanvasDrawListener* create(
      std::unique_ptr<WebCanvasCaptureHandler>,
      double frameRate);
  void sendNewFrame(sk_sp<SkImage>) override;

  DEFINE_INLINE_TRACE() {}

 private:
  TimedCanvasDrawListener(std::unique_ptr<WebCanvasCaptureHandler>,
                          double frameRate);
  // Implementation of TimerFiredFunction.
  void requestFrameTimerFired(TimerBase*);

  double m_frameInterval;
  UnthrottledThreadTimer<TimedCanvasDrawListener> m_requestFrameTimer;
};

}  // namespace blink

#endif
