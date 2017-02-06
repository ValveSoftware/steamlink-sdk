// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialsContainer.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/frame/UseCounter.h"
#include "core/page/FrameTree.h"
#include "modules/credentialmanager/Credential.h"
#include "modules/credentialmanager/CredentialManagerClient.h"
#include "modules/credentialmanager/CredentialRequestOptions.h"
#include "modules/credentialmanager/FederatedCredential.h"
#include "modules/credentialmanager/FederatedCredentialRequestOptions.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebCredentialManagerClient.h"
#include "public/platform/WebCredentialManagerError.h"
#include "public/platform/WebFederatedCredential.h"
#include "public/platform/WebPasswordCredential.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static void rejectDueToCredentialManagerError(ScriptPromiseResolver* resolver, WebCredentialManagerError reason)
{
    switch (reason) {
    case WebCredentialManagerDisabledError:
        resolver->reject(DOMException::create(InvalidStateError, "The credential manager is disabled."));
        break;
    case WebCredentialManagerPendingRequestError:
        resolver->reject(DOMException::create(InvalidStateError, "A 'get()' request is pending."));
        break;
    case WebCredentialManagerUnknownError:
    default:
        resolver->reject(DOMException::create(NotReadableError, "An unknown error occurred while talking to the credential manager."));
        break;
    }
}

class NotificationCallbacks : public WebCredentialManagerClient::NotificationCallbacks {
    WTF_MAKE_NONCOPYABLE(NotificationCallbacks);
public:
    explicit NotificationCallbacks(ScriptPromiseResolver* resolver) : m_resolver(resolver) { }
    ~NotificationCallbacks() override { }

    void onSuccess() override
    {
        Frame* frame = toDocument(m_resolver->getScriptState()->getExecutionContext())->frame();
        SECURITY_CHECK(!frame || frame == frame->tree().top());

        m_resolver->resolve();
    }

    void onError(WebCredentialManagerError reason) override
    {
        rejectDueToCredentialManagerError(m_resolver, reason);
    }

private:
    const Persistent<ScriptPromiseResolver> m_resolver;
};

class RequestCallbacks : public WebCredentialManagerClient::RequestCallbacks {
    WTF_MAKE_NONCOPYABLE(RequestCallbacks);
public:
    explicit RequestCallbacks(ScriptPromiseResolver* resolver) : m_resolver(resolver) { }
    ~RequestCallbacks() override { }

    void onSuccess(std::unique_ptr<WebCredential> webCredential) override
    {
        Frame* frame = toDocument(m_resolver->getScriptState()->getExecutionContext())->frame();
        SECURITY_CHECK(!frame || frame == frame->tree().top());

        std::unique_ptr<WebCredential> credential = wrapUnique(webCredential.release());
        if (!credential || !frame) {
            m_resolver->resolve();
            return;
        }

        ASSERT(credential->isPasswordCredential() || credential->isFederatedCredential());
        UseCounter::count(m_resolver->getScriptState()->getExecutionContext(), UseCounter::CredentialManagerGetReturnedCredential);
        if (credential->isPasswordCredential())
            m_resolver->resolve(PasswordCredential::create(static_cast<WebPasswordCredential*>(credential.get())));
        else
            m_resolver->resolve(FederatedCredential::create(static_cast<WebFederatedCredential*>(credential.get())));
    }

    void onError(WebCredentialManagerError reason) override
    {
        rejectDueToCredentialManagerError(m_resolver, reason);
    }

private:
    const Persistent<ScriptPromiseResolver> m_resolver;
};


CredentialsContainer* CredentialsContainer::create()
{
    return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer()
{
}

static bool checkBoilerplate(ScriptPromiseResolver* resolver)
{
    Frame* frame = toDocument(resolver->getScriptState()->getExecutionContext())->frame();
    if (!frame || frame != frame->tree().top()) {
        resolver->reject(DOMException::create(SecurityError, "CredentialContainer methods may only be executed in a top-level document."));
        return false;
    }

    String errorMessage;
    if (!resolver->getScriptState()->getExecutionContext()->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(SecurityError, errorMessage));
        return false;
    }

    CredentialManagerClient* client = CredentialManagerClient::from(resolver->getScriptState()->getExecutionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "Could not establish connection to the credential manager."));
        return false;
    }

    return true;
}

ScriptPromise CredentialsContainer::get(ScriptState* scriptState, const CredentialRequestOptions& options)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    Vector<KURL> providers;
    if (options.hasFederated() && options.federated().hasProviders()) {
        // TODO(mkwst): CredentialRequestOptions::federated() needs to return a reference, not a value.
        // Because it returns a temporary value now, a for loop that directly references the value
        // generates code that holds a reference to a value that no longer exists by the time the loop
        // starts looping. In order to avoid this crazyness for the moment, we're making a copy of the
        // vector. https://crbug.com/587088
        const Vector<String> providerStrings = options.federated().providers();
        for (const auto& string : providerStrings) {
            KURL url = KURL(KURL(), string);
            if (url.isValid())
                providers.append(url);
        }
    }

    UseCounter::count(scriptState->getExecutionContext(), options.unmediated() ? UseCounter::CredentialManagerGetWithoutUI : UseCounter::CredentialManagerGetWithUI);

    CredentialManagerClient::from(scriptState->getExecutionContext())->dispatchGet(options.unmediated(), options.password(), providers, new RequestCallbacks(resolver));
    return promise;
}

ScriptPromise CredentialsContainer::store(ScriptState* scriptState, Credential* credential)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    auto webCredential = WebCredential::create(credential->getPlatformCredential());
    CredentialManagerClient::from(scriptState->getExecutionContext())->dispatchStore(*webCredential, new NotificationCallbacks(resolver));
    return promise;
}

ScriptPromise CredentialsContainer::requireUserMediation(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    CredentialManagerClient::from(scriptState->getExecutionContext())->dispatchRequireUserMediation(new NotificationCallbacks(resolver));
    return promise;
}

} // namespace blink
