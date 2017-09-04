// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VisualViewportScrollEvent_h
#define VisualViewportScrollEvent_h

#include "core/events/Event.h"

namespace blink {

class VisualViewportScrollEvent final : public Event {
 public:
  ~VisualViewportScrollEvent() override;

  static VisualViewportScrollEvent* create() {
    return new VisualViewportScrollEvent();
  }

  void doneDispatchingEventAtCurrentTarget() override;

  DEFINE_INLINE_VIRTUAL_TRACE() { Event::trace(visitor); }

 private:
  VisualViewportScrollEvent();
};

}  // namespace blink

#endif  // VisualViewportScrollEvent_h
