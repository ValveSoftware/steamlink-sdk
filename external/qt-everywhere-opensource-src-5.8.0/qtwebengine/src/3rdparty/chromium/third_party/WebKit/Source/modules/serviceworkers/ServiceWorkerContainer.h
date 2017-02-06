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

#ifndef ServiceWorkerContainer_h
#define ServiceWorkerContainer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/RegistrationOptions.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class ExecutionContext;
class WebServiceWorker;
class WebServiceWorkerProvider;
class WebServiceWorkerRegistration;

class MODULES_EXPORT ServiceWorkerContainer final
    : public EventTargetWithInlineData
    , public ContextLifecycleObserver
    , public WebServiceWorkerProviderClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerContainer);
public:
    using RegistrationCallbacks = WebServiceWorkerProvider::WebServiceWorkerRegistrationCallbacks;

    static ServiceWorkerContainer* create(ExecutionContext*);
    ~ServiceWorkerContainer();

    void willBeDetachedFromFrame();

    DECLARE_VIRTUAL_TRACE();

    ServiceWorker* controller() { return m_controller; }
    ScriptPromise ready(ScriptState*);
    WebServiceWorkerProvider* provider() { return m_provider; }

    void registerServiceWorkerImpl(ExecutionContext*, const KURL& scriptURL, const KURL& scope, std::unique_ptr<RegistrationCallbacks>);

    ScriptPromise registerServiceWorker(ScriptState*, const String& pattern, const RegistrationOptions&);
    ScriptPromise getRegistration(ScriptState*, const String& documentURL);
    ScriptPromise getRegistrations(ScriptState*);

    // WebServiceWorkerProviderClient overrides.
    void setController(std::unique_ptr<WebServiceWorker::Handle>, bool shouldNotifyControllerChange) override;
    void dispatchMessageEvent(std::unique_ptr<WebServiceWorker::Handle>, const WebString& message, const WebMessagePortChannelArray&) override;

    // EventTarget overrides.
    ExecutionContext* getExecutionContext() const override { return ContextLifecycleObserver::getExecutionContext(); }
    const AtomicString& interfaceName() const override;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(controllerchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

private:
    explicit ServiceWorkerContainer(ExecutionContext*);

    class GetRegistrationForReadyCallback;
    typedef ScriptPromiseProperty<Member<ServiceWorkerContainer>, Member<ServiceWorkerRegistration>, Member<ServiceWorkerRegistration>> ReadyProperty;
    ReadyProperty* createReadyProperty();

    WebServiceWorkerProvider* m_provider;
    Member<ServiceWorker> m_controller;
    Member<ReadyProperty> m_ready;
};

} // namespace blink

#endif // ServiceWorkerContainer_h
