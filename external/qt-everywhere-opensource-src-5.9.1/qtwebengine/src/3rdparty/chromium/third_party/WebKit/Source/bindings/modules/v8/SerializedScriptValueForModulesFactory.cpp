// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/SerializedScriptValueForModulesFactory.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/modules/v8/ScriptValueSerializerForModules.h"
#include "bindings/modules/v8/serialization/V8ScriptValueDeserializerForModules.h"
#include "bindings/modules/v8/serialization/V8ScriptValueSerializerForModules.h"
#include "core/dom/ExceptionCode.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

PassRefPtr<SerializedScriptValue>
SerializedScriptValueForModulesFactory::create(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               Transferables* transferables,
                                               WebBlobInfoArray* blobInfo,
                                               ExceptionState& exceptionState) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  if (RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled()) {
    V8ScriptValueSerializerForModules serializer(ScriptState::current(isolate));
    serializer.setBlobInfoArray(blobInfo);
    return serializer.serialize(value, transferables, exceptionState);
  }
  SerializedScriptValueWriterForModules writer;
  ScriptValueSerializerForModules serializer(writer, blobInfo,
                                             ScriptState::current(isolate));
  return serializer.serialize(value, transferables, exceptionState);
}

v8::Local<v8::Value> SerializedScriptValueForModulesFactory::deserialize(
    SerializedScriptValue* value,
    v8::Isolate* isolate,
    MessagePortArray* messagePorts,
    const WebBlobInfoArray* blobInfo) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  if (RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled()) {
    V8ScriptValueDeserializerForModules deserializer(
        ScriptState::current(isolate), value);
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
  SerializedScriptValueReaderForModules reader(
      value->data(), value->dataLengthInBytes(), blobInfo,
      value->blobDataHandles(), ScriptState::current(isolate));
  ScriptValueDeserializerForModules deserializer(
      reader, messagePorts, value->getArrayBufferContentsArray(),
      value->getImageBitmapContentsArray());
  return deserializer.deserialize();
}

}  // namespace blink
