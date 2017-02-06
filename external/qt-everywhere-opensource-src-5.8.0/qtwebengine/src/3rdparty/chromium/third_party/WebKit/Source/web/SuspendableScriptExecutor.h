// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuspendableScriptExecutor_h
#define SuspendableScriptExecutor_h

#include "core/frame/SuspendableTimer.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class ScriptSourceCode;
class WebScriptExecutionCallback;

class SuspendableScriptExecutor final : public GarbageCollectedFinalized<SuspendableScriptExecutor>, public SuspendableTimer {
    USING_GARBAGE_COLLECTED_MIXIN(SuspendableScriptExecutor);
public:
    static void createAndRun(LocalFrame*, int worldID, const HeapVector<ScriptSourceCode>& sources, int extensionGroup, bool userGesture, WebScriptExecutionCallback*);
    ~SuspendableScriptExecutor() override;

    void contextDestroyed() override;

    DECLARE_VIRTUAL_TRACE();

private:
    SuspendableScriptExecutor(LocalFrame*, int worldID, const HeapVector<ScriptSourceCode>& sources, int extensionGroup, bool userGesture, WebScriptExecutionCallback*);

    void fired() override;

    void run();
    void executeAndDestroySelf();
    void dispose();

    Member<LocalFrame> m_frame;
    HeapVector<ScriptSourceCode> m_sources;
    WebScriptExecutionCallback* m_callback;

    SelfKeepAlive<SuspendableScriptExecutor> m_keepAlive;

    int m_worldID;
    int m_extensionGroup;

    bool m_userGesture;
};

} // namespace blink

#endif // SuspendableScriptExecutor_h
