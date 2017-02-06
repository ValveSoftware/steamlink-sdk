/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptRunner_h
#define ScriptRunner_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CancellableTaskFactory;
class Document;
class ScriptLoader;
class WebTaskRunner;

class CORE_EXPORT ScriptRunner final : public GarbageCollected<ScriptRunner> {
    WTF_MAKE_NONCOPYABLE(ScriptRunner);
public:
    static ScriptRunner* create(Document* document)
    {
        return new ScriptRunner(document);
    }

    enum ExecutionType { ASYNC_EXECUTION, IN_ORDER_EXECUTION };
    void queueScriptForExecution(ScriptLoader*, ExecutionType);
    bool hasPendingScripts() const { return !m_pendingInOrderScripts.isEmpty() || !m_pendingAsyncScripts.isEmpty(); }
    void suspend();
    void resume();
    void notifyScriptReady(ScriptLoader*, ExecutionType);
    void notifyScriptLoadError(ScriptLoader*, ExecutionType);

    static void movePendingScript(Document&, Document&, ScriptLoader*);

    DECLARE_TRACE();

private:
    class Task;

    explicit ScriptRunner(Document*);

    void movePendingScript(ScriptRunner*, ScriptLoader*);
    bool removePendingInOrderScript(ScriptLoader*);
    void scheduleReadyInOrderScripts();

    void postTask(const WebTraceLocation&);

    bool executeTaskFromQueue(HeapDeque<Member<ScriptLoader>>*);

    void executeTask();

    Member<Document> m_document;

    HeapDeque<Member<ScriptLoader>> m_pendingInOrderScripts;
    HeapHashSet<Member<ScriptLoader>> m_pendingAsyncScripts;

    // http://www.whatwg.org/specs/web-apps/current-work/#set-of-scripts-that-will-execute-as-soon-as-possible
    HeapDeque<Member<ScriptLoader>> m_asyncScriptsToExecuteSoon;
    HeapDeque<Member<ScriptLoader>> m_inOrderScriptsToExecuteSoon;

    WebTaskRunner* m_taskRunner;

    int m_numberOfInOrderScriptsWithPendingNotification;

    bool m_isSuspended;
#ifndef NDEBUG
    bool m_hasEverBeenSuspended;
#endif
};

} // namespace blink

#endif // ScriptRunner_h
