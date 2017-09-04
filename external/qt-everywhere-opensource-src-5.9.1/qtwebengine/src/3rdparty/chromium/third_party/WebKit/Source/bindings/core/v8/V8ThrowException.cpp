/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8ThrowException.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

namespace {

void domExceptionStackGetter(v8::Local<v8::Name> name,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value;
  if (info.Data()
          .As<v8::Object>()
          ->Get(isolate->GetCurrentContext(), v8AtomicString(isolate, "stack"))
          .ToLocal(&value))
    v8SetReturnValue(info, value);
}

void domExceptionStackSetter(v8::Local<v8::Name> name,
                             v8::Local<v8::Value> value,
                             const v8::PropertyCallbackInfo<void>& info) {
  v8::Maybe<bool> unused = info.Data().As<v8::Object>()->Set(
      info.GetIsolate()->GetCurrentContext(),
      v8AtomicString(info.GetIsolate(), "stack"), value);
  ALLOW_UNUSED_LOCAL(unused);
}

}  // namespace

v8::Local<v8::Value> V8ThrowException::createDOMException(
    v8::Isolate* isolate,
    ExceptionCode exceptionCode,
    const String& sanitizedMessage,
    const String& unsanitizedMessage) {
  DCHECK_GT(exceptionCode, 0);
  DCHECK(exceptionCode == SecurityError || unsanitizedMessage.isNull());

  if (isolate->IsExecutionTerminating())
    return v8::Local<v8::Value>();

  switch (exceptionCode) {
    case V8Error:
      return createError(isolate, sanitizedMessage);
    case V8TypeError:
      return createTypeError(isolate, sanitizedMessage);
    case V8RangeError:
      return createRangeError(isolate, sanitizedMessage);
    case V8SyntaxError:
      return createSyntaxError(isolate, sanitizedMessage);
    case V8ReferenceError:
      return createReferenceError(isolate, sanitizedMessage);
  }

  DOMException* domException =
      DOMException::create(exceptionCode, sanitizedMessage, unsanitizedMessage);
  v8::Local<v8::Object> exceptionObj =
      toV8(domException, isolate->GetCurrentContext()->Global(), isolate)
          .As<v8::Object>();
  // Attach an Error object to the DOMException. This is then lazily used to
  // get the stack value.
  v8::Local<v8::Value> error =
      v8::Exception::Error(v8String(isolate, domException->message()));
  exceptionObj
      ->SetAccessor(isolate->GetCurrentContext(),
                    v8AtomicString(isolate, "stack"), domExceptionStackGetter,
                    domExceptionStackSetter, error)
      .ToChecked();

  auto privateError = V8PrivateProperty::getDOMExceptionError(isolate);
  privateError.set(isolate->GetCurrentContext(), exceptionObj, error);

  return exceptionObj;
}

#define DEFINE_CREATE_AND_THROW_ERROR_FUNC(blinkErrorType, v8ErrorType,  \
                                           defaultMessage)               \
  v8::Local<v8::Value> V8ThrowException::create##blinkErrorType(         \
      v8::Isolate* isolate, const String& message) {                     \
    return v8::Exception::v8ErrorType(                                   \
        v8String(isolate, message.isNull() ? defaultMessage : message)); \
  }                                                                      \
                                                                         \
  void V8ThrowException::throw##blinkErrorType(v8::Isolate * isolate,    \
                                               const String& message) {  \
    throwException(isolate, create##blinkErrorType(isolate, message));   \
  }

DEFINE_CREATE_AND_THROW_ERROR_FUNC(Error, Error, "Error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(RangeError, RangeError, "Range error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(ReferenceError,
                                   ReferenceError,
                                   "Reference error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(SyntaxError, SyntaxError, "Syntax error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(TypeError, TypeError, "Type error")

#undef DEFINE_CREATE_AND_THROW_ERROR_FUNC

}  // namespace blink
