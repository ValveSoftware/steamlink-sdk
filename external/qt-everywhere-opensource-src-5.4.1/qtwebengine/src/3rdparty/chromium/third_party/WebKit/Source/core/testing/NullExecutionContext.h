// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NullExecutionContext_h
#define NullExecutionContext_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/events/EventQueue.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class NullExecutionContext FINAL : public RefCountedWillBeGarbageCollectedFinalized<NullExecutionContext>, public SecurityContext, public ExecutionContext {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NullExecutionContext);
public:
    NullExecutionContext();

    virtual EventQueue* eventQueue() const OVERRIDE { return m_queue.get(); }

    virtual bool tasksNeedSuspension() OVERRIDE { return m_tasksNeedSuspension; }
    void setTasksNeedSuspension(bool flag) { m_tasksNeedSuspension = flag; }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_queue);
        ExecutionContext::trace(visitor);
    }

    virtual void reportBlockedScriptExecutionToInspector(const String& directiveText) OVERRIDE { }
    virtual SecurityContext& securityContext() OVERRIDE { return *this; }

#if !ENABLE(OILPAN)
    using RefCounted<NullExecutionContext>::ref;
    using RefCounted<NullExecutionContext>::deref;

    virtual void refExecutionContext() OVERRIDE { ref(); }
    virtual void derefExecutionContext() OVERRIDE { deref(); }
#endif

protected:
    virtual const KURL& virtualURL() const OVERRIDE { return m_dummyURL; }
    virtual KURL virtualCompleteURL(const String&) const OVERRIDE { return m_dummyURL; }

private:
    bool m_tasksNeedSuspension;
    OwnPtrWillBeMember<EventQueue> m_queue;

    KURL m_dummyURL;
};

} // namespace WebCore

#endif // NullExecutionContext_h
