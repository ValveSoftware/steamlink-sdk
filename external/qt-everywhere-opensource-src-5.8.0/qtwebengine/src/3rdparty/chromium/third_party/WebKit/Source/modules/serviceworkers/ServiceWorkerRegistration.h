// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistration_h
#define ServiceWorkerRegistration_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class ScriptPromise;
class ScriptState;
class WebServiceWorkerProvider;

// The implementation of a service worker registration object in Blink. Actual
// registration representation is in the embedder and this class accesses it
// via WebServiceWorkerRegistration::Handle object.
class ServiceWorkerRegistration final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public WebServiceWorkerRegistrationProxy
    , public Supplementable<ServiceWorkerRegistration> {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
    USING_PRE_FINALIZER(ServiceWorkerRegistration, dispose);
public:
    // EventTarget overrides.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override { return ActiveDOMObject::getExecutionContext(); }

    // WebServiceWorkerRegistrationProxy overrides.
    void dispatchUpdateFoundEvent() override;
    void setInstalling(std::unique_ptr<WebServiceWorker::Handle>) override;
    void setWaiting(std::unique_ptr<WebServiceWorker::Handle>) override;
    void setActive(std::unique_ptr<WebServiceWorker::Handle>) override;

    // Returns an existing registration object for the handle if it exists.
    // Otherwise, returns a new registration object.
    static ServiceWorkerRegistration* getOrCreate(ExecutionContext*, std::unique_ptr<WebServiceWorkerRegistration::Handle>);

    ServiceWorker* installing() { return m_installing; }
    ServiceWorker* waiting() { return m_waiting; }
    ServiceWorker* active() { return m_active; }

    String scope() const;

    WebServiceWorkerRegistration* webRegistration() { return m_handle->registration(); }

    ScriptPromise update(ScriptState*);
    ScriptPromise unregister(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound);

    ~ServiceWorkerRegistration() override;

    DECLARE_VIRTUAL_TRACE();

private:
    ServiceWorkerRegistration(ExecutionContext*, std::unique_ptr<WebServiceWorkerRegistration::Handle>);
    void dispose();

    // ActiveScriptWrappable overrides.
    bool hasPendingActivity() const final;

    // ActiveDOMObject overrides.
    void stop() override;

    // A handle to the registration representation in the embedder.
    std::unique_ptr<WebServiceWorkerRegistration::Handle> m_handle;

    Member<ServiceWorker> m_installing;
    Member<ServiceWorker> m_waiting;
    Member<ServiceWorker> m_active;

    bool m_stopped;
};

class ServiceWorkerRegistrationArray {
    STATIC_ONLY(ServiceWorkerRegistrationArray);
public:
    static HeapVector<Member<ServiceWorkerRegistration>> take(ScriptPromiseResolver* resolver, Vector<std::unique_ptr<WebServiceWorkerRegistration::Handle>>* webServiceWorkerRegistrations)
    {
        HeapVector<Member<ServiceWorkerRegistration>> registrations;
        for (auto& registration : *webServiceWorkerRegistrations)
            registrations.append(ServiceWorkerRegistration::getOrCreate(resolver->getExecutionContext(), std::move(registration)));
        return registrations;
    }
};

} // namespace blink

#endif // ServiceWorkerRegistration_h
