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

#ifndef ExceptionState_h
#define ExceptionState_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

typedef int ExceptionCode;
class ScriptPromiseResolver;
class ScriptState;

// ExceptionState is a scope-like class and provides a way to throw an exception
// with an option to cancel it.  An exception message may be auto-generated.
// You can convert an exception to a reject promise.
class CORE_EXPORT ExceptionState {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ExceptionState);

 public:
  enum ContextType {
    ConstructionContext,
    ExecutionContext,
    DeletionContext,
    GetterContext,
    SetterContext,
    EnumerationContext,
    QueryContext,
    IndexedGetterContext,
    IndexedSetterContext,
    IndexedDeletionContext,
    UnknownContext,  // FIXME: Remove this once we've flipped over to the new
                     // API.
  };

  ExceptionState(v8::Isolate* isolate,
                 ContextType contextType,
                 const char* interfaceName,
                 const char* propertyName)
      : m_code(0),
        m_context(contextType),
        m_propertyName(propertyName),
        m_interfaceName(interfaceName),
        m_isolate(isolate) {}

  ExceptionState(v8::Isolate* isolate,
                 ContextType contextType,
                 const char* interfaceName)
      : ExceptionState(isolate, contextType, interfaceName, nullptr) {
#if ENABLE(ASSERT)
    switch (m_context) {
      case ConstructionContext:
      case EnumerationContext:
      case IndexedGetterContext:
      case IndexedSetterContext:
      case IndexedDeletionContext:
        break;
      default:
        NOTREACHED();
    }
#endif  // ENABLE(ASSERT)
  }

  ExceptionState(ContextType context,
                 const char* propertyName,
                 const char* interfaceName,
                 const v8::Local<v8::Object>& creationContext,
                 v8::Isolate* isolate)  // DEPRECATED
      : ExceptionState(isolate, context, interfaceName, propertyName) {}

  ExceptionState(ContextType context,
                 const char* interfaceName,
                 const v8::Local<v8::Object>& creationContext,
                 v8::Isolate* isolate)  // DEPRECATED
      : ExceptionState(isolate, context, interfaceName) {}

  ~ExceptionState() {
    if (!m_exception.isEmpty()) {
      V8ThrowException::throwException(m_isolate,
                                       m_exception.newLocal(m_isolate));
    }
  }

  virtual void throwDOMException(const ExceptionCode&, const String& message);
  virtual void throwRangeError(const String& message);
  virtual void throwSecurityError(const String& sanitizedMessage,
                                  const String& unsanitizedMessage = String());
  virtual void throwTypeError(const String& message);
  virtual void rethrowV8Exception(v8::Local<v8::Value>);

  bool hadException() const { return m_code; }
  void clearException();

  ExceptionCode code() const { return m_code; }
  const String& message() const { return m_message; }
  v8::Local<v8::Value> getException() {
    DCHECK(!m_exception.isEmpty());
    return m_exception.newLocal(m_isolate);
  }

  // This method clears out the exception which |this| has.
  ScriptPromise reject(ScriptState*);

  // This method clears out the exception which |this| has.
  void reject(ScriptPromiseResolver*);

  ContextType context() const { return m_context; }
  const char* propertyName() const { return m_propertyName; }
  const char* interfaceName() const { return m_interfaceName; }

  String addExceptionContext(const String&) const;

 protected:
  // An ExceptionCode for the case that an exception is rethrown.  In that
  // case, we cannot determine an exception code.
  static const int kRethrownException = UnknownError;

  void setException(ExceptionCode, const String&, v8::Local<v8::Value>);

 private:
  ExceptionCode m_code;
  ContextType m_context;
  String m_message;
  const char* m_propertyName;
  const char* m_interfaceName;
  // The exception is empty when it was thrown through TrackExceptionState.
  ScopedPersistent<v8::Value> m_exception;
  v8::Isolate* m_isolate;
};

// NonThrowableExceptionState never allow call sites to throw an exception.
// Should be used if an exception must not be thrown.
class CORE_EXPORT NonThrowableExceptionState final : public ExceptionState {
 public:
  NonThrowableExceptionState()
      : ExceptionState(nullptr,
                       ExceptionState::UnknownContext,
                       nullptr,
                       nullptr) {}

  void throwDOMException(const ExceptionCode&, const String& message) override;
  void throwTypeError(const String& message) override;
  void throwSecurityError(const String& sanitizedMessage,
                          const String& unsanitizedMessage) override;
  void throwRangeError(const String& message) override;
  void rethrowV8Exception(v8::Local<v8::Value>) override;
};

// TrackExceptionState never actually throws an exception, but just records
// whether a call site tried to throw an exception or not.  Should be used
// if any exceptions must be ignored.
class CORE_EXPORT TrackExceptionState final : public ExceptionState {
 public:
  TrackExceptionState()
      : ExceptionState(nullptr,
                       ExceptionState::UnknownContext,
                       nullptr,
                       nullptr) {}
  ~TrackExceptionState() {
    // Prevent the base class throw an exception.
    if (hadException()) {
      clearException();
    }
  }

  void throwDOMException(const ExceptionCode&, const String& message) override;
  void throwTypeError(const String& message) override;
  void throwSecurityError(const String& sanitizedMessage,
                          const String& unsanitizedMessage) override;
  void throwRangeError(const String& message) override;
  void rethrowV8Exception(v8::Local<v8::Value>) override;
};

}  // namespace blink

#endif  // ExceptionState_h
