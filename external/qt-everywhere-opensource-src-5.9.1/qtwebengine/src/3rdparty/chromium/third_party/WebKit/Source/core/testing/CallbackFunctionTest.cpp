// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/CallbackFunctionTest.h"

#include "bindings/core/v8/TestCallback.h"
#include "bindings/core/v8/TestInterfaceCallback.h"
#include "bindings/core/v8/TestReceiverObjectCallback.h"
#include "bindings/core/v8/TestSequenceCallback.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/html/HTMLDivElement.h"

namespace blink {

DEFINE_TRACE(CallbackFunctionTest) {}

String CallbackFunctionTest::testCallback(TestCallback* callback,
                                          const String& message1,
                                          const String& message2,
                                          ExceptionState& exceptionState) {
  ScriptWrappable* scriptWrappable;
  String returnValue;

  if (callback->call(scriptWrappable = nullptr, message1, message2,
                     returnValue)) {
    return String("SUCCESS: ") + returnValue;
  }
  return String("Error!");
}

void CallbackFunctionTest::testInterfaceCallback(
    TestInterfaceCallback* callback,
    HTMLDivElement* divElement,
    ExceptionState& exceptionState) {
  ScriptWrappable* scriptWrappable;

  callback->call(scriptWrappable = nullptr, divElement);
  return;
}

void CallbackFunctionTest::testReceiverObjectCallback(
    TestReceiverObjectCallback* callback,
    ExceptionState& exceptionState) {
  callback->call(this);
  return;
}

Vector<String> CallbackFunctionTest::testSequenceCallback(
    TestSequenceCallback* callback,
    const Vector<int>& numbers,
    ExceptionState& exceptionState) {
  Vector<String> returnValue;
  if (callback->call(nullptr, numbers, returnValue)) {
    return returnValue;
  }
  return Vector<String>();
}

}  // namespace blink
