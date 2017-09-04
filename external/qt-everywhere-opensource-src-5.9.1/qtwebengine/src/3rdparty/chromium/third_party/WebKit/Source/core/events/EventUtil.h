// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventUtil_h
#define EventUtil_h

#include "core/EventTypeNames.h"

namespace blink {

namespace EventUtil {

bool isPointerEventType(const AtomicString& eventType);

}  // namespace eventUtil

}  // namespace blink

#endif  // EventUtil_h
