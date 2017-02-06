// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamOperations_h
#define ReadableStreamOperations_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include <v8.h>

namespace blink {

class UnderlyingSourceBase;
class ExceptionState;
class ScriptState;

// This class has various methods for ReadableStream[Reader] implemented with
// V8 Extras.
// All methods should be called in an appropriate V8 context. All ScriptValue
// arguments must not be empty.
class CORE_EXPORT ReadableStreamOperations {
    STATIC_ONLY(ReadableStreamOperations);
public:
    // createReadableStreamWithExternalController
    // If the caller supplies an invalid strategy (e.g. one that returns
    // negative sizes, or doesn't have appropriate properties), this will crash.
    static ScriptValue createReadableStream(ScriptState*, UnderlyingSourceBase*, ScriptValue strategy);

    // createBuiltInCountQueuingStrategy
    static ScriptValue createCountQueuingStrategy(ScriptState*, size_t highWaterMark);

    // AcquireReadableStreamDefaultReader
    // This function assumes |isReadableStream(stream)|.
    // Returns an empty value and throws an error via the ExceptionState when
    // errored.
    static ScriptValue getReader(ScriptState*, ScriptValue stream, ExceptionState&);

    // IsReadableStream
    static bool isReadableStream(ScriptState*, ScriptValue);

    // IsReadableStreamDisturbed
    // This function assumes |isReadableStream(stream)|.
    static bool isDisturbed(ScriptState*, ScriptValue stream);

    // IsReadableStreamLocked
    // This function assumes |isReadableStream(stream)|.
    static bool isLocked(ScriptState*, ScriptValue stream);

    // IsReadableStreamReadable
    // This function assumes |isReadableStream(stream)|.
    static bool isReadable(ScriptState*, ScriptValue stream);

    // IsReadableStreamClosed
    // This function assumes |isReadableStream(stream)|.
    static bool isClosed(ScriptState*, ScriptValue stream);

    // IsReadableStreamErrored
    // This function assumes |isReadableStream(stream)|.
    static bool isErrored(ScriptState*, ScriptValue stream);

    // IsReadableStreamDefaultReader
    static bool isReadableStreamDefaultReader(ScriptState*, ScriptValue);

    // ReadableStreamDefaultReaderRead
    // This function assumes |isReadableStreamDefaultReader(reader)|.
    static ScriptPromise defaultReaderRead(ScriptState*, ScriptValue reader);

    // ReadableStreamTee
    // This function assumes |isReadableStream(stream)| and |!isLocked(stream)|
    static void tee(ScriptState*, ScriptValue stream, ScriptValue* newStream1, ScriptValue* newStream2);
};

} // namespace blink

#endif // ReadableStreamOperations_h
