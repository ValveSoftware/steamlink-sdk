// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/EventUtil.h"

#include "core/EventTypeNames.h"

namespace blink {

namespace EventUtil {

bool isPointerEventType(const AtomicString& eventType)
{
return eventType == EventTypeNames::gotpointercapture
    || eventType == EventTypeNames::lostpointercapture
    || eventType == EventTypeNames::pointercancel
    || eventType == EventTypeNames::pointerdown
    || eventType == EventTypeNames::pointerenter
    || eventType == EventTypeNames::pointerleave
    || eventType == EventTypeNames::pointermove
    || eventType == EventTypeNames::pointerout
    || eventType == EventTypeNames::pointerover
    || eventType == EventTypeNames::pointerup;
}

} // namespace eventUtil

} // namespace blink


