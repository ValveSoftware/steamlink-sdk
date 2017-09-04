// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SerializedScriptValueFactory.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValueSerializer.h"
#include "bindings/core/v8/Transferables.h"
#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/MessagePort.h"
#include "core/frame/ImageBitmap.h"
#include "platform/tracing/TraceEvent.h"
#include "wtf/ByteOrder.h"
#include "wtf/text/StringBuffer.h"

namespace blink {

SerializedScriptValueFactory* SerializedScriptValueFactory::m_instance = 0;

PassRefPtr<SerializedScriptValue> SerializedScriptValueFactory::create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    Transferables* transferables,
    WebBlobInfoArray* blobInfo,
    ExceptionState& exceptionState) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  if (RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled()) {
    V8ScriptValueSerializer serializer(ScriptState::current(isolate));
    serializer.setBlobInfoArray(blobInfo);
    return serializer.serialize(value, transferables, exceptionState);
  }
  SerializedScriptValueWriter writer;
  ScriptValueSerializer serializer(writer, blobInfo,
                                   ScriptState::current(isolate));
  return serializer.serialize(value, transferables, exceptionState);
}

v8::Local<v8::Value> SerializedScriptValueFactory::deserialize(
    SerializedScriptValue* value,
    v8::Isolate* isolate,
    MessagePortArray* messagePorts,
    const WebBlobInfoArray* blobInfo) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  if (RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled()) {
    V8ScriptValueDeserializer deserializer(ScriptState::current(isolate),
                                           value);
    deserializer.setTransferredMessagePorts(messagePorts);
    deserializer.setBlobInfoArray(blobInfo);
    return deserializer.deserialize();
  }
  // deserialize() can run arbitrary script (e.g., setters), which could result
  // in |this| being destroyed.  Holding a RefPtr ensures we are alive (along
  // with our internal data) throughout the operation.
  RefPtr<SerializedScriptValue> protect(value);
  if (!value->dataLengthInBytes())
    return v8::Null(isolate);
  static_assert(sizeof(SerializedScriptValueWriter::BufferValueType) == 2,
                "BufferValueType should be 2 bytes");
  // FIXME: SerializedScriptValue shouldn't use String for its underlying
  // storage. Instead, it should use SharedBuffer or Vector<uint8_t>. The
  // information stored in m_data isn't even encoded in UTF-16. Instead,
  // unicode characters are encoded as UTF-8 with two code units per UChar.
  SerializedScriptValueReader reader(value->data(), value->dataLengthInBytes(),
                                     blobInfo, value->blobDataHandles(),
                                     ScriptState::current(isolate));
  ScriptValueDeserializer deserializer(reader, messagePorts,
                                       value->getArrayBufferContentsArray(),
                                       value->getImageBitmapContentsArray());

  // deserialize() can run arbitrary script (e.g., setters), which could result
  // in |this| being destroyed.  Holding a RefPtr ensures we are alive (along
  // with our internal data) throughout the operation.
  return deserializer.deserialize();
}

}  // namespace blink
