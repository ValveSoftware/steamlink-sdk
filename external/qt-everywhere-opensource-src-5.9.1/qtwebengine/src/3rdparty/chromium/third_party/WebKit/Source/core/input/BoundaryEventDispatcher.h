// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoundaryEventDispatcher_h
#define BoundaryEventDispatcher_h

#include "core/events/EventTarget.h"

namespace blink {

// This class contains the common logic of finding related node in a boundary
// crossing action and sending boundary events. The subclasses of this class
// must define what events should be sent in every case.
class BoundaryEventDispatcher {
  STACK_ALLOCATED()
 public:
  BoundaryEventDispatcher() {}
  virtual ~BoundaryEventDispatcher() {}

  void sendBoundaryEvents(EventTarget* exitedTarget,
                          EventTarget* enteredTarget);

 protected:
  virtual void dispatchOut(EventTarget*, EventTarget* relatedTarget) = 0;
  virtual void dispatchOver(EventTarget*, EventTarget* relatedTarget) = 0;
  virtual void dispatchLeave(EventTarget*,
                             EventTarget* relatedTarget,
                             bool checkForListener) = 0;
  virtual void dispatchEnter(EventTarget*,
                             EventTarget* relatedTarget,
                             bool checkForListener) = 0;
  virtual AtomicString getLeaveEvent() = 0;
  virtual AtomicString getEnterEvent() = 0;
};

}  // namespace blink

#endif  // BoundaryEventDispatcher_h
