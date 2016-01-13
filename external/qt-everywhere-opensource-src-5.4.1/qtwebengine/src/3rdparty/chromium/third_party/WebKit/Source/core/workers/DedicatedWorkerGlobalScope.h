/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DedicatedWorkerGlobalScope_h
#define DedicatedWorkerGlobalScope_h

#include "core/dom/MessagePort.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class DedicatedWorkerThread;
class WorkerThreadStartupData;

class DedicatedWorkerGlobalScope FINAL : public WorkerGlobalScope {
public:
    typedef WorkerGlobalScope Base;
    static PassRefPtrWillBeRawPtr<DedicatedWorkerGlobalScope> create(DedicatedWorkerThread*, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>, double timeOrigin);
    virtual ~DedicatedWorkerGlobalScope();

    virtual bool isDedicatedWorkerGlobalScope() const OVERRIDE { return true; }
    virtual void countFeature(UseCounter::Feature) const OVERRIDE;
    virtual void countDeprecation(UseCounter::Feature) const OVERRIDE;

    // Overridden to allow us to check our pending activity after executing imported script.
    virtual void importScripts(const Vector<String>& urls, ExceptionState&) OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;

    void postMessage(PassRefPtr<SerializedScriptValue>, const MessagePortArray*, ExceptionState&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

    DedicatedWorkerThread* thread() const;

    virtual void trace(Visitor*) OVERRIDE;

private:
    DedicatedWorkerGlobalScope(const KURL&, const String& userAgent, DedicatedWorkerThread*, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerClients>);
};

} // namespace WebCore

#endif // DedicatedWorkerGlobalScope_h
