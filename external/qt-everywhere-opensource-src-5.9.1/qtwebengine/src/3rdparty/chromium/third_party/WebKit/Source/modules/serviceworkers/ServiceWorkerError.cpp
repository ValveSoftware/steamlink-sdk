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

#include "modules/serviceworkers/ServiceWorkerError.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ToV8.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

using blink::WebServiceWorkerError;

namespace blink {

namespace {

struct ExceptionParams {
  ExceptionParams(ExceptionCode code,
                  const String& defaultMessage = String(),
                  const String& message = String())
      : code(code), message(message.isEmpty() ? defaultMessage : message) {}

  ExceptionCode code;
  String message;
};

ExceptionParams getExceptionParams(const WebServiceWorkerError& webError) {
  switch (webError.errorType) {
    case WebServiceWorkerError::ErrorTypeAbort:
      return ExceptionParams(AbortError,
                             "The Service Worker operation was aborted.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeActivate:
      // Not currently returned as a promise rejection.
      // TODO: Introduce new ActivateError type to ExceptionCodes?
      return ExceptionParams(AbortError,
                             "The Service Worker activation failed.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeDisabled:
      return ExceptionParams(NotSupportedError,
                             "Service Worker support is disabled.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeInstall:
      // TODO: Introduce new InstallError type to ExceptionCodes?
      return ExceptionParams(AbortError,
                             "The Service Worker installation failed.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeScriptEvaluateFailed:
      return ExceptionParams(AbortError,
                             "The Service Worker script failed to evaluate.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeNavigation:
      // ErrorTypeNavigation should have bailed out before calling this.
      ASSERT_NOT_REACHED();
      return ExceptionParams(UnknownError);
    case WebServiceWorkerError::ErrorTypeNetwork:
      return ExceptionParams(NetworkError,
                             "The Service Worker failed by network.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeNotFound:
      return ExceptionParams(
          NotFoundError, "The specified Service Worker resource was not found.",
          webError.message);
    case WebServiceWorkerError::ErrorTypeSecurity:
      return ExceptionParams(
          SecurityError,
          "The Service Worker security policy prevented an action.",
          webError.message);
    case WebServiceWorkerError::ErrorTypeState:
      return ExceptionParams(InvalidStateError,
                             "The Service Worker state was not valid.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeTimeout:
      return ExceptionParams(AbortError,
                             "The Service Worker operation timed out.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeUnknown:
      return ExceptionParams(UnknownError,
                             "An unknown error occurred within Service Worker.",
                             webError.message);
    case WebServiceWorkerError::ErrorTypeType:
      // ErrorTypeType should have been handled before reaching this point.
      ASSERT_NOT_REACHED();
      return ExceptionParams(UnknownError);
  }
  ASSERT_NOT_REACHED();
  return ExceptionParams(UnknownError);
}

}  // namespace

// static
DOMException* ServiceWorkerError::take(ScriptPromiseResolver*,
                                       const WebServiceWorkerError& webError) {
  ExceptionParams params = getExceptionParams(webError);
  ASSERT(params.code != UnknownError);
  return DOMException::create(params.code, params.message);
}

// static
v8::Local<v8::Value> ServiceWorkerErrorForUpdate::take(
    ScriptPromiseResolver* resolver,
    const WebServiceWorkerError& webError) {
  ScriptState* scriptState = resolver->getScriptState();
  switch (webError.errorType) {
    case WebServiceWorkerError::ErrorTypeNetwork:
    case WebServiceWorkerError::ErrorTypeNotFound:
    case WebServiceWorkerError::ErrorTypeScriptEvaluateFailed:
      // According to the spec, these errors during update should result in
      // a TypeError.
      return V8ThrowException::createTypeError(
          scriptState->isolate(), getExceptionParams(webError).message);
    default:
      return toV8(ServiceWorkerError::take(resolver, webError),
                  scriptState->context()->Global(), scriptState->isolate());
  }
}

}  // namespace blink
