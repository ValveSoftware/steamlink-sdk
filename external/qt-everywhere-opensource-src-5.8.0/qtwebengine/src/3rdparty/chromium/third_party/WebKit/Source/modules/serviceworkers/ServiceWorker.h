/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ServiceWorker_h
#define ServiceWorker_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/workers/AbstractWorker.h"
#include "modules/ModulesExport.h"
#include "public/platform/modules/serviceworker/WebServiceWorker.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProxy.h"
#include "wtf/PassRefPtr.h"
#include <memory>

namespace blink {

class ScriptPromiseResolver;

class MODULES_EXPORT ServiceWorker final : public AbstractWorker, public ActiveScriptWrappable, public WebServiceWorkerProxy {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(ServiceWorker);
public:
    static ServiceWorker* from(ExecutionContext*, std::unique_ptr<WebServiceWorker::Handle>);

    ~ServiceWorker() override;
    DECLARE_VIRTUAL_TRACE();

    // Eager finalization needed to promptly release owned WebServiceWorker.
    EAGERLY_FINALIZE();

    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray&, ExceptionState&);

    String scriptURL() const;
    String state() const;
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    // WebServiceWorkerProxy overrides.
    void dispatchStateChangeEvent() override;

    // AbstractWorker overrides.
    const AtomicString& interfaceName() const override;

    void internalsTerminate();
private:
    static ServiceWorker* getOrCreate(ExecutionContext*, std::unique_ptr<WebServiceWorker::Handle>);
    ServiceWorker(ExecutionContext*, std::unique_ptr<WebServiceWorker::Handle>);

    // ActiveScriptWrappable overrides.
    bool hasPendingActivity() const final;

    // ActiveDOMObject overrides.
    void stop() override;

    // A handle to the service worker representation in the embedder.
    std::unique_ptr<WebServiceWorker::Handle> m_handle;
    bool m_wasStopped;
};

} // namespace blink

#endif // ServiceWorker_h
