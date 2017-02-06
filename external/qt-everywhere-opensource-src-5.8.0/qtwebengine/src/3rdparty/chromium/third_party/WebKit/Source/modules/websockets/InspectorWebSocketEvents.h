// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorWebSocketEvents_h
#define InspectorWebSocketEvents_h

#include "core/inspector/InspectorTraceEvents.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include <memory>

namespace blink {

class Document;
class KURL;

class InspectorWebSocketCreateEvent {
    STATIC_ONLY(InspectorWebSocketCreateEvent);
public:
    static std::unique_ptr<TracedValue> data(Document*, unsigned long identifier, const KURL&, const String& protocol);
};

class InspectorWebSocketEvent {
    STATIC_ONLY(InspectorWebSocketEvent);
public:
    static std::unique_ptr<TracedValue> data(Document*, unsigned long identifier);
};

} // namespace blink


#endif // !defined(InspectorWebSocketEvents_h)
