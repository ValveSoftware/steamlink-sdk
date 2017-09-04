// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/BudgetService.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/budget/BudgetState.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/budget_service/budget_service.mojom-blink.h"

namespace blink {
namespace {

mojom::blink::BudgetOperationType stringToOperationType(
    const AtomicString& operation) {
  if (operation == "silent-push")
    return mojom::blink::BudgetOperationType::SILENT_PUSH;

  return mojom::blink::BudgetOperationType::INVALID_OPERATION;
}

DOMException* errorTypeToException(mojom::blink::BudgetServiceErrorType error) {
  switch (error) {
    case mojom::blink::BudgetServiceErrorType::NONE:
      return nullptr;
    case mojom::blink::BudgetServiceErrorType::DATABASE_ERROR:
      return DOMException::create(DataError,
                                  "Error reading the budget database.");
    case mojom::blink::BudgetServiceErrorType::NOT_SUPPORTED:
      return DOMException::create(NotSupportedError,
                                  "Requested opration was not supported");
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

BudgetService::BudgetService() {
  Platform::current()->interfaceProvider()->getInterface(
      mojo::GetProxy(&m_service));

  // Set a connection error handler, so that if an embedder doesn't
  // implement a BudgetSerice mojo service, the developer will get a
  // actionable information.
  m_service.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&BudgetService::onConnectionError, wrapWeakPersistent(this))));
}

BudgetService::~BudgetService() {}

ScriptPromise BudgetService::getCost(ScriptState* scriptState,
                                     const AtomicString& operation) {
  DCHECK(m_service);

  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(errorMessage))
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SecurityError, errorMessage));

  mojom::blink::BudgetOperationType type = stringToOperationType(operation);
  if (type == mojom::blink::BudgetOperationType::INVALID_OPERATION)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotSupportedError,
                                          "Invalid operation type specified"));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // Get the cost for the action from the browser BudgetService.
  m_service->GetCost(type, convertToBaseCallback(WTF::bind(
                               &BudgetService::gotCost, wrapPersistent(this),
                               wrapPersistent(resolver))));
  return promise;
}

void BudgetService::gotCost(ScriptPromiseResolver* resolver,
                            double cost) const {
  resolver->resolve(cost);
}

ScriptPromise BudgetService::getBudget(ScriptState* scriptState) {
  DCHECK(m_service);

  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(errorMessage))
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SecurityError, errorMessage));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // Get the budget from the browser BudgetService.
  RefPtr<SecurityOrigin> origin(
      scriptState->getExecutionContext()->getSecurityOrigin());
  m_service->GetBudget(
      origin, convertToBaseCallback(WTF::bind(&BudgetService::gotBudget,
                                              wrapPersistent(this),
                                              wrapPersistent(resolver))));
  return promise;
}

void BudgetService::gotBudget(
    ScriptPromiseResolver* resolver,
    mojom::blink::BudgetServiceErrorType error,
    const mojo::WTFArray<mojom::blink::BudgetStatePtr> expectations) const {
  if (error != mojom::blink::BudgetServiceErrorType::NONE) {
    resolver->reject(errorTypeToException(error));
    return;
  }

  // Copy the chunks into the budget array.
  HeapVector<Member<BudgetState>> budget(expectations.size());
  for (size_t i = 0; i < expectations.size(); i++)
    budget[i] =
        new BudgetState(expectations[i]->budget_at, expectations[i]->time);

  resolver->resolve(budget);
}

ScriptPromise BudgetService::reserve(ScriptState* scriptState,
                                     const AtomicString& operation) {
  DCHECK(m_service);

  mojom::blink::BudgetOperationType type = stringToOperationType(operation);
  if (type == mojom::blink::BudgetOperationType::INVALID_OPERATION)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotSupportedError,
                                          "Invalid operation type specified"));

  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(errorMessage))
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SecurityError, errorMessage));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // Call to the BudgetService to place the reservation.
  RefPtr<SecurityOrigin> origin(
      scriptState->getExecutionContext()->getSecurityOrigin());
  m_service->Reserve(origin, type,
                     convertToBaseCallback(WTF::bind(
                         &BudgetService::gotReservation, wrapPersistent(this),
                         wrapPersistent(resolver))));
  return promise;
}

void BudgetService::gotReservation(ScriptPromiseResolver* resolver,
                                   mojom::blink::BudgetServiceErrorType error,
                                   bool success) const {
  if (error != mojom::blink::BudgetServiceErrorType::NONE) {
    resolver->reject(errorTypeToException(error));
    return;
  }

  resolver->resolve(success);
}

void BudgetService::onConnectionError() {
  LOG(ERROR) << "Unable to connect to the Mojo BudgetService.";
  // TODO(harkness): Reject in flight promises.
}

}  // namespace blink
