// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_PROCESSOR_H_
#define UI_EVENTS_EVENT_PROCESSOR_H_

#include "ui/events/event_dispatcher.h"
#include "ui/events/event_source.h"

namespace ui {

// EventProcessor receives an event from an EventSource and dispatches it to a
// tree of EventTargets.
class EVENTS_EXPORT EventProcessor : public EventDispatcherDelegate {
 public:
  virtual ~EventProcessor() {}

  // Returns the root of the tree this event processor owns.
  virtual EventTarget* GetRootTarget() = 0;

  // Dispatches an event received from the EventSource to the tree of
  // EventTargets (whose root is returned by GetRootTarget()).  The co-ordinate
  // space of the source must be the same as the root target, except that the
  // target may have a high-dpi scale applied.
  virtual EventDispatchDetails OnEventFromSource(Event* event)
      WARN_UNUSED_RESULT;

 protected:
  // Prepares the event so that it can be dispatched. This is invoked before
  // an EventTargeter is used to find the target of the event. So this can be
  // used to update the event so that the targeter can operate correctly (e.g.
  // it can be used to updated the location of the event when disptaching from
  // an EventSource in high-DPI).
  virtual void PrepareEventForDispatch(Event* event);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_PROCESSOR_H_
