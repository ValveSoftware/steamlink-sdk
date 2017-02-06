// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerGlobalScope_h
#define CompositorWorkerGlobalScope_h

#include "core/dom/CompositorProxyClient.h"
#include "core/dom/FrameRequestCallbackCollection.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/ModulesExport.h"
#include <memory>

namespace blink {

class CompositorWorkerThread;
class WorkerThreadStartupData;

class MODULES_EXPORT CompositorWorkerGlobalScope final : public WorkerGlobalScope {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CompositorWorkerGlobalScope* create(CompositorWorkerThread*, std::unique_ptr<WorkerThreadStartupData>, double timeOrigin);
    ~CompositorWorkerGlobalScope() override;

    // EventTarget
    const AtomicString& interfaceName() const override;

    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue>, const MessagePortArray&, ExceptionState&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

    int requestAnimationFrame(FrameRequestCallback*);
    void cancelAnimationFrame(int id);
    bool executeAnimationFrameCallbacks(double highResTimeMs);

    // ExecutionContext:
    bool isCompositorWorkerGlobalScope() const override { return true; }

    DECLARE_VIRTUAL_TRACE();

private:
    CompositorWorkerGlobalScope(const KURL&, const String& userAgent, CompositorWorkerThread*, double timeOrigin, std::unique_ptr<SecurityOrigin::PrivilegeData>, WorkerClients*);
    CompositorWorkerThread* thread() const;

    bool m_executingAnimationFrameCallbacks;
    FrameRequestCallbackCollection m_callbackCollection;
};

} // namespace blink

#endif // CompositorWorkerGlobalScope_h
