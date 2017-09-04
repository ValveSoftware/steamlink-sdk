// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VisualViewportResizeEvent_h
#define VisualViewportResizeEvent_h

#include "core/events/Event.h"

namespace blink {

class VisualViewportResizeEvent final : public Event {
 public:
  ~VisualViewportResizeEvent() override;

  static VisualViewportResizeEvent* create() {
    return new VisualViewportResizeEvent();
  }

  void doneDispatchingEventAtCurrentTarget() override;

  DEFINE_INLINE_VIRTUAL_TRACE() { Event::trace(visitor); }

 private:
  VisualViewportResizeEvent();
};

}  // namespace blink

#endif  // VisualViewportResizeEvent_h
