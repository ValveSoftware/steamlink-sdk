// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_processor.h"

#include "ui/events/event_target.h"
#include "ui/events/event_targeter.h"

namespace ui {

EventDispatchDetails EventProcessor::OnEventFromSource(Event* event) {
  EventTarget* root = GetRootTarget();
  CHECK(root);
  EventTargeter* targeter = root->GetEventTargeter();
  CHECK(targeter);

  PrepareEventForDispatch(event);
  EventTarget* target = targeter->FindTargetForEvent(root, event);

  while (target) {
    EventDispatchDetails details = DispatchEvent(target, event);
    if (details.dispatcher_destroyed ||
        details.target_destroyed ||
        event->handled()) {
      return details;
    }

    target = targeter->FindNextBestTarget(target, event);
  }

  return EventDispatchDetails();
}

void EventProcessor::PrepareEventForDispatch(Event* event) {
}

}  // namespace ui
