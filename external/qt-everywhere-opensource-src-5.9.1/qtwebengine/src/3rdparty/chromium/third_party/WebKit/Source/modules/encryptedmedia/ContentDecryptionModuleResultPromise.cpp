// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/ContentDecryptionModuleResultPromise.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "public/platform/WebString.h"
#include "wtf/Assertions.h"

namespace blink {

ExceptionCode WebCdmExceptionToExceptionCode(
    WebContentDecryptionModuleException cdmException) {
  switch (cdmException) {
    case WebContentDecryptionModuleExceptionTypeError:
      return V8TypeError;
    case WebContentDecryptionModuleExceptionNotSupportedError:
      return NotSupportedError;
    case WebContentDecryptionModuleExceptionInvalidStateError:
      return InvalidStateError;
    case WebContentDecryptionModuleExceptionQuotaExceededError:
      return QuotaExceededError;
    case WebContentDecryptionModuleExceptionUnknownError:
      return UnknownError;
  }

  NOTREACHED();
  return UnknownError;
}

ContentDecryptionModuleResultPromise::ContentDecryptionModuleResultPromise(
    ScriptState* scriptState)
    : m_resolver(ScriptPromiseResolver::create(scriptState)) {}

ContentDecryptionModuleResultPromise::~ContentDecryptionModuleResultPromise() {}

void ContentDecryptionModuleResultPromise::complete() {
  NOTREACHED();
  if (!isValidToFulfillPromise())
    return;
  reject(InvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::completeWithContentDecryptionModule(
    WebContentDecryptionModule* cdm) {
  NOTREACHED();
  if (!isValidToFulfillPromise())
    return;
  reject(InvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::completeWithSession(
    WebContentDecryptionModuleResult::SessionStatus status) {
  NOTREACHED();
  if (!isValidToFulfillPromise())
    return;
  reject(InvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::completeWithError(
    WebContentDecryptionModuleException exceptionCode,
    unsigned long systemCode,
    const WebString& errorMessage) {
  if (!isValidToFulfillPromise())
    return;

  // Non-zero |systemCode| is appended to the |errorMessage|. If the
  // |errorMessage| is empty, we'll report "Rejected with system code
  // (systemCode)".
  StringBuilder result;
  result.append(errorMessage);
  if (systemCode != 0) {
    if (result.isEmpty())
      result.append("Rejected with system code");
    result.append(" (");
    result.appendNumber(systemCode);
    result.append(')');
  }
  reject(WebCdmExceptionToExceptionCode(exceptionCode), result.toString());
}

ScriptPromise ContentDecryptionModuleResultPromise::promise() {
  return m_resolver->promise();
}

void ContentDecryptionModuleResultPromise::reject(ExceptionCode code,
                                                  const String& errorMessage) {
  DCHECK(isValidToFulfillPromise());

  ScriptState::Scope scope(m_resolver->getScriptState());
  v8::Isolate* isolate = m_resolver->getScriptState()->isolate();
  m_resolver->reject(
      V8ThrowException::createDOMException(isolate, code, errorMessage));
  m_resolver.clear();
}

ExecutionContext* ContentDecryptionModuleResultPromise::getExecutionContext()
    const {
  return m_resolver->getExecutionContext();
}

bool ContentDecryptionModuleResultPromise::isValidToFulfillPromise() {
  // getExecutionContext() is no longer valid once the context is destroyed.
  // activeDOMObjectsAreStopped() is called to see if the context is in the
  // process of being destroyed. If it is, there is no need to fulfill this
  // promise which is about to go away anyway.
  return getExecutionContext() &&
         !getExecutionContext()->activeDOMObjectsAreStopped();
}

DEFINE_TRACE(ContentDecryptionModuleResultPromise) {
  visitor->trace(m_resolver);
  ContentDecryptionModuleResult::trace(visitor);
}

}  // namespace blink
