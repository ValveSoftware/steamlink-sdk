// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppManager.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/payments/PaymentAppManifest.h"
#include "modules/payments/PaymentAppOption.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace mojo {

using payments::mojom::blink::PaymentAppManifest;
using payments::mojom::blink::PaymentAppManifestPtr;
using payments::mojom::blink::PaymentAppOption;
using payments::mojom::blink::PaymentAppOptionPtr;

template <>
struct TypeConverter<PaymentAppOptionPtr, blink::PaymentAppOption> {
  static PaymentAppOptionPtr Convert(const blink::PaymentAppOption& input) {
    PaymentAppOptionPtr output = PaymentAppOption::New();
    output->label = input.hasLabel() ? input.label() : WTF::emptyString();
    output->icon = input.hasIcon() ? input.icon() : WTF::String();
    output->id = input.hasId() ? input.id() : WTF::emptyString();
    output->enabled_methods = WTF::Vector<WTF::String>(input.enabledMethods());
    return output;
  }
};

template <>
struct TypeConverter<PaymentAppManifestPtr, blink::PaymentAppManifest> {
  static PaymentAppManifestPtr Convert(const blink::PaymentAppManifest& input) {
    PaymentAppManifestPtr output = PaymentAppManifest::New();
    output->label = input.hasLabel() ? input.label() : WTF::emptyString();
    output->icon = input.hasIcon() ? input.icon() : WTF::String();
    if (input.hasOptions()) {
      for (size_t i = 0; i < input.options().size(); ++i) {
        output->options.append(PaymentAppOption::From(input.options()[i]));
      }
    }
    return output;
  }
};

}  // namespace mojo

namespace blink {

PaymentAppManager* PaymentAppManager::create(
    ScriptState* scriptState,
    ServiceWorkerRegistration* registration) {
  return new PaymentAppManager(scriptState, registration);
}

ScriptPromise PaymentAppManager::getManifest(ScriptState* scriptState) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise PaymentAppManager::setManifest(
    ScriptState* scriptState,
    const PaymentAppManifest& manifest) {
  if (!m_manager) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "Payment app manager unavailable."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_manager->SetManifest(
      m_registration->scope(),
      payments::mojom::blink::PaymentAppManifest::From(manifest),
      convertToBaseCallback(WTF::bind(&PaymentAppManager::onSetManifest,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));

  return promise;
}

void PaymentAppManager::onSetManifest(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentAppManifestError error) {
  DCHECK(resolver);
  switch (error) {
    case payments::mojom::blink::PaymentAppManifestError::NOT_IMPLEMENTED:
      resolver->reject(
          DOMException::create(NotSupportedError, "Not implemented yet."));
      break;
    default:
      NOTREACHED();
  }
}

DEFINE_TRACE(PaymentAppManager) {
  visitor->trace(m_registration);
  ContextLifecycleObserver::trace(visitor);
}

PaymentAppManager::PaymentAppManager(ScriptState* scriptState,
                                     ServiceWorkerRegistration* registration)
    : ContextLifecycleObserver(scriptState->getExecutionContext()),
      m_registration(registration) {
  DCHECK(registration);
  Platform::current()->interfaceProvider()->getInterface(
      mojo::GetProxy(&m_manager));

  m_manager.set_connection_error_handler(convertToBaseCallback(WTF::bind(
      &PaymentAppManager::onServiceConnectionError, wrapWeakPersistent(this))));
}

void PaymentAppManager::onServiceConnectionError() {
  if (!Platform::current()) {
    return;
  }

  m_manager.reset();
}

void PaymentAppManager::contextDestroyed() {
  m_manager.reset();
}

}  // namespace blink
