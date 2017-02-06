// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8EventListenerInfo_h
#define V8EventListenerInfo_h

#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

#include <v8.h>

namespace blink {

class V8EventListenerInfo {
public:
    V8EventListenerInfo(AtomicString eventType, bool useCapture, bool passive, v8::Local<v8::Object> handler, v8::MaybeLocal<v8::Function> removeFunction)
        : eventType(eventType)
        , useCapture(useCapture)
        , passive(passive)
        , handler(handler)
        , removeFunction(removeFunction)
    {
    }

    AtomicString eventType;
    bool useCapture;
    bool passive;
    v8::Local<v8::Object> handler;
    v8::MaybeLocal<v8::Function> removeFunction;
};

using V8EventListenerInfoList = Vector<V8EventListenerInfo>;

} // namespace blink

#endif // V8EventListenerInfo_h
