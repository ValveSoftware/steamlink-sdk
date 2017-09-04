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

#include "bindings/core/v8/ExceptionState.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

void ExceptionState::throwDOMException(const ExceptionCode& ec,
                                       const String& message) {
  // SecurityError is thrown via ::throwSecurityError, and _careful_
  // consideration must be given to the data exposed to JavaScript via the
  // 'sanitizedMessage'.
  DCHECK(ec != SecurityError);

  const String& processedMessage = addExceptionContext(message);
  setException(ec, processedMessage, V8ThrowException::createDOMException(
                                         m_isolate, ec, processedMessage));
}

void ExceptionState::throwRangeError(const String& message) {
  setException(V8RangeError, message,
               V8ThrowException::createRangeError(
                   m_isolate, addExceptionContext(message)));
}

void ExceptionState::throwSecurityError(const String& sanitizedMessage,
                                        const String& unsanitizedMessage) {
  const String& finalSanitized = addExceptionContext(sanitizedMessage);
  const String& finalUnsanitized = addExceptionContext(unsanitizedMessage);
  setException(SecurityError, finalSanitized,
               V8ThrowException::createDOMException(
                   m_isolate, SecurityError, finalSanitized, finalUnsanitized));
}

void ExceptionState::throwTypeError(const String& message) {
  setException(V8TypeError, message,
               V8ThrowException::createTypeError(m_isolate,
                                                 addExceptionContext(message)));
}

void ExceptionState::rethrowV8Exception(v8::Local<v8::Value> value) {
  setException(kRethrownException, String(), value);
}

void ExceptionState::clearException() {
  m_code = 0;
  m_message = String();
  m_exception.clear();
}

ScriptPromise ExceptionState::reject(ScriptState* scriptState) {
  ScriptPromise promise = ScriptPromise::reject(scriptState, getException());
  clearException();
  return promise;
}

void ExceptionState::reject(ScriptPromiseResolver* resolver) {
  resolver->reject(getException());
  clearException();
}

void ExceptionState::setException(ExceptionCode ec,
                                  const String& message,
                                  v8::Local<v8::Value> exception) {
  CHECK(ec);

  m_code = ec;
  m_message = message;
  if (exception.IsEmpty()) {
    m_exception.clear();
  } else {
    DCHECK(m_isolate);
    m_exception.set(m_isolate, exception);
  }
}

String ExceptionState::addExceptionContext(const String& message) const {
  if (message.isEmpty())
    return message;

  String processedMessage = message;
  if (propertyName() && interfaceName() && m_context != UnknownContext) {
    if (m_context == DeletionContext)
      processedMessage = ExceptionMessages::failedToDelete(
          propertyName(), interfaceName(), message);
    else if (m_context == ExecutionContext)
      processedMessage = ExceptionMessages::failedToExecute(
          propertyName(), interfaceName(), message);
    else if (m_context == GetterContext)
      processedMessage = ExceptionMessages::failedToGet(
          propertyName(), interfaceName(), message);
    else if (m_context == SetterContext)
      processedMessage = ExceptionMessages::failedToSet(
          propertyName(), interfaceName(), message);
  } else if (!propertyName() && interfaceName()) {
    if (m_context == ConstructionContext)
      processedMessage =
          ExceptionMessages::failedToConstruct(interfaceName(), message);
    else if (m_context == EnumerationContext)
      processedMessage =
          ExceptionMessages::failedToEnumerate(interfaceName(), message);
    else if (m_context == IndexedDeletionContext)
      processedMessage =
          ExceptionMessages::failedToDeleteIndexed(interfaceName(), message);
    else if (m_context == IndexedGetterContext)
      processedMessage =
          ExceptionMessages::failedToGetIndexed(interfaceName(), message);
    else if (m_context == IndexedSetterContext)
      processedMessage =
          ExceptionMessages::failedToSetIndexed(interfaceName(), message);
  }
  return processedMessage;
}

void NonThrowableExceptionState::throwDOMException(const ExceptionCode& ec,
                                                   const String& message) {
  NOTREACHED();
}

void NonThrowableExceptionState::throwRangeError(const String& message) {
  NOTREACHED();
}

void NonThrowableExceptionState::throwSecurityError(
    const String& sanitizedMessage,
    const String&) {
  NOTREACHED();
}

void NonThrowableExceptionState::throwTypeError(const String& message) {
  NOTREACHED();
}

void NonThrowableExceptionState::rethrowV8Exception(v8::Local<v8::Value>) {
  NOTREACHED();
}

void TrackExceptionState::throwDOMException(const ExceptionCode& ec,
                                            const String& message) {
  setException(ec, message, v8::Local<v8::Value>());
}

void TrackExceptionState::throwRangeError(const String& message) {
  setException(V8RangeError, message, v8::Local<v8::Value>());
}

void TrackExceptionState::throwSecurityError(const String& sanitizedMessage,
                                             const String&) {
  setException(SecurityError, sanitizedMessage, v8::Local<v8::Value>());
}

void TrackExceptionState::throwTypeError(const String& message) {
  setException(V8TypeError, message, v8::Local<v8::Value>());
}

void TrackExceptionState::rethrowV8Exception(v8::Local<v8::Value>) {
  setException(kRethrownException, String(), v8::Local<v8::Value>());
}

}  // namespace blink
