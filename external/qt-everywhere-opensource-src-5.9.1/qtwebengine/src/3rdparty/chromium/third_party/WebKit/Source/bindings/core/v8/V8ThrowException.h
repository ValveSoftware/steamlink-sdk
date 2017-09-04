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

#ifndef V8ThrowException_h
#define V8ThrowException_h

#include "core/CoreExport.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

// Provides utility functions to create and/or throw a V8 exception.
// Mostly a set of wrapper functions for v8::Exception class.
class CORE_EXPORT V8ThrowException {
  STATIC_ONLY(V8ThrowException);

 public:
  // Creates and returns an exception object, or returns an empty handle if
  // failed.  |unsanitizedMessage| should not be specified unless it's
  // SecurityError.
  static v8::Local<v8::Value> createDOMException(
      v8::Isolate*,
      ExceptionCode,
      const String& sanitizedMessage,
      const String& unsanitizedMessage = String());

  static void throwException(v8::Isolate* isolate,
                             v8::Local<v8::Value> exception) {
    if (!isolate->IsExecutionTerminating())
      isolate->ThrowException(exception);
  }

  static v8::Local<v8::Value> createError(v8::Isolate*, const String& message);
  static v8::Local<v8::Value> createRangeError(v8::Isolate*,
                                               const String& message);
  static v8::Local<v8::Value> createReferenceError(v8::Isolate*,
                                                   const String& message);
  static v8::Local<v8::Value> createSyntaxError(v8::Isolate*,
                                                const String& message);
  static v8::Local<v8::Value> createTypeError(v8::Isolate*,
                                              const String& message);

  static void throwError(v8::Isolate*, const String& message);
  static void throwRangeError(v8::Isolate*, const String& message);
  static void throwReferenceError(v8::Isolate*, const String& message);
  static void throwSyntaxError(v8::Isolate*, const String& message);
  static void throwTypeError(v8::Isolate*, const String& message);
};

}  // namespace blink

#endif  // V8ThrowException_h
