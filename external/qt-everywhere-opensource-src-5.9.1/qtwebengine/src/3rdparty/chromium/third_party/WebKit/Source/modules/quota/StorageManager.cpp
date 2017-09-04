// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/quota/StorageManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/V8StorageEstimate.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/permissions/PermissionUtils.h"
#include "modules/quota/StorageEstimate.h"
#include "platform/StorageQuotaCallbacks.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/Platform.h"
#include "wtf/Functional.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

class EstimateCallbacks final : public StorageQuotaCallbacks {
  WTF_MAKE_NONCOPYABLE(EstimateCallbacks);

 public:
  explicit EstimateCallbacks(ScriptPromiseResolver* resolver)
      : m_resolver(resolver) {}

  ~EstimateCallbacks() override {}

  void didQueryStorageUsageAndQuota(unsigned long long usageInBytes,
                                    unsigned long long quotaInBytes) override {
    StorageEstimate estimate;
    estimate.setUsage(usageInBytes);
    estimate.setQuota(quotaInBytes);
    m_resolver->resolve(estimate);
  }

  void didFail(WebStorageQuotaError error) override {
    m_resolver->reject(DOMException::create(static_cast<ExceptionCode>(error)));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_resolver);
    StorageQuotaCallbacks::trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver> m_resolver;
};

}  // namespace

ScriptPromise StorageManager::persist(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  SecurityOrigin* securityOrigin = executionContext->getSecurityOrigin();
  // TODO(dgrogan): Is the isUnique() check covered by the later
  // isSecureContext() check? If so, maybe remove it. Write a test if it
  // stays.
  if (securityOrigin->isUnique()) {
    resolver->reject(DOMException::create(NotSupportedError));
    return promise;
  }
  String errorMessage;
  if (!executionContext->isSecureContext(errorMessage)) {
    resolver->reject(DOMException::create(SecurityError, errorMessage));
    return promise;
  }
  ASSERT(executionContext->isDocument());
  PermissionService* permissionService =
      getPermissionService(scriptState->getExecutionContext());
  if (!permissionService) {
    resolver->reject(DOMException::create(
        InvalidStateError,
        "In its current state, the global scope can't request permissions."));
    return promise;
  }
  permissionService->RequestPermission(
      createPermissionDescriptor(PermissionName::DURABLE_STORAGE),
      scriptState->getExecutionContext()->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(
          WTF::bind(&StorageManager::permissionRequestComplete,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

ScriptPromise StorageManager::persisted(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  PermissionService* permissionService =
      getPermissionService(scriptState->getExecutionContext());
  if (!permissionService) {
    resolver->reject(DOMException::create(
        InvalidStateError,
        "In its current state, the global scope can't query permissions."));
    return promise;
  }
  permissionService->HasPermission(
      createPermissionDescriptor(PermissionName::DURABLE_STORAGE),
      scriptState->getExecutionContext()->getSecurityOrigin(),
      convertToBaseCallback(
          WTF::bind(&StorageManager::permissionRequestComplete,
                    wrapPersistent(this), wrapPersistent(resolver))));
  return promise;
}

ScriptPromise StorageManager::estimate(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  SecurityOrigin* securityOrigin = executionContext->getSecurityOrigin();
  if (securityOrigin->isUnique()) {
    resolver->reject(DOMException::create(NotSupportedError));
    return promise;
  }
  // IDL has: [SecureContext]
  String errorMessage;
  if (!executionContext->isSecureContext(errorMessage)) {
    resolver->reject(DOMException::create(SecurityError, errorMessage));
    return promise;
  }

  KURL storagePartition = KURL(KURL(), securityOrigin->toString());
  Platform::current()->queryStorageUsageAndQuota(
      storagePartition, WebStorageQuotaTypeTemporary,
      new EstimateCallbacks(resolver));
  return promise;
}

DEFINE_TRACE(StorageManager) {}

PermissionService* StorageManager::getPermissionService(
    ExecutionContext* executionContext) {
  if (!m_permissionService &&
      connectToPermissionService(executionContext,
                                 mojo::GetProxy(&m_permissionService)))
    m_permissionService.set_connection_error_handler(convertToBaseCallback(
        WTF::bind(&StorageManager::permissionServiceConnectionError,
                  wrapWeakPersistent(this))));
  return m_permissionService.get();
}

void StorageManager::permissionServiceConnectionError() {
  if (!Platform::current()) {
    // TODO(rockot): Clean this up once renderer shutdown sequence is fixed.
    return;
  }

  m_permissionService.reset();
}

void StorageManager::permissionRequestComplete(ScriptPromiseResolver* resolver,
                                               PermissionStatus status) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  resolver->resolve(status == PermissionStatus::GRANTED);
}

}  // namespace blink
