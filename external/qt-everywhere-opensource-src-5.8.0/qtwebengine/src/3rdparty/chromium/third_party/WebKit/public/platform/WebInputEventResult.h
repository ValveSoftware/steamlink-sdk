// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputEventResult_h
#define WebInputEventResult_h

namespace blink {

enum class WebInputEventResult {
    // Event was not consumed by application or system.
    NotHandled,
    // Event was consumed but suppressed before dispatched to application.
    HandledSuppressed,
    // Event was consumed by application itself; ie. a script handler calling preventDefault.
    HandledApplication,
    // Event was consumed by the system; ie. executing the default action.
    HandledSystem,
};

} // namespace blink

#endif // WebInputEventResult_h
