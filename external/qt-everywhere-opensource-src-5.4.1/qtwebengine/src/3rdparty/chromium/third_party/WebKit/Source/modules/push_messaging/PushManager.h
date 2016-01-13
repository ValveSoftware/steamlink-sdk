// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushManager_h
#define PushManager_h

#include "bindings/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExecutionContext;
class ScriptPromise;
class ScriptState;

class PushManager FINAL : public GarbageCollectedFinalized<PushManager>, public ScriptWrappable {
public:
    static PushManager* create()
    {
        return new PushManager();
    }
    virtual ~PushManager();

    ScriptPromise registerPushMessaging(ScriptState*, const String& senderId);

    void trace(Visitor*) { }

private:
    PushManager();
};

} // namespace WebCore

#endif // PushManager_h
