// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ScriptValueDeserializer_h
#define V8ScriptValueDeserializer_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializationTag.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {

class File;

// Deserializes V8 values serialized using V8ScriptValueSerializer (or its
// predecessor, ScriptValueSerializer).
//
// Supports only basic JavaScript objects and core DOM types. Support for
// modules types is implemented in a subclass.
//
// A deserializer cannot be used multiple times; it is expected that its
// deserialize method will be invoked exactly once.
class GC_PLUGIN_IGNORE("https://crbug.com/644725") CORE_EXPORT
    V8ScriptValueDeserializer : public v8::ValueDeserializer::Delegate {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(V8ScriptValueDeserializer);

 public:
  V8ScriptValueDeserializer(RefPtr<ScriptState>, RefPtr<SerializedScriptValue>);

  void setTransferredMessagePorts(const MessagePortArray* ports) {
    m_transferredMessagePorts = ports;
  }
  void setBlobInfoArray(const WebBlobInfoArray* blobInfoArray) {
    m_blobInfoArray = blobInfoArray;
  }

  v8::Local<v8::Value> deserialize();

 protected:
  virtual ScriptWrappable* readDOMObject(SerializationTag);

  ScriptState* getScriptState() const { return m_scriptState.get(); }

  uint32_t version() const { return m_version; }
  bool readTag(SerializationTag* tag) {
    const void* tagBytes = nullptr;
    if (!m_deserializer.ReadRawBytes(1, &tagBytes))
      return false;
    *tag = static_cast<SerializationTag>(
        *reinterpret_cast<const uint8_t*>(tagBytes));
    return true;
  }
  bool readUint32(uint32_t* value) { return m_deserializer.ReadUint32(value); }
  bool readUint64(uint64_t* value) { return m_deserializer.ReadUint64(value); }
  bool readDouble(double* value) { return m_deserializer.ReadDouble(value); }
  bool readRawBytes(size_t size, const void** data) {
    return m_deserializer.ReadRawBytes(size, data);
  }
  bool readUTF8String(String* stringOut);

 private:
  void transfer();

  File* readFile();
  File* readFileIndex();

  RefPtr<BlobDataHandle> getOrCreateBlobDataHandle(const String& uuid,
                                                   const String& type,
                                                   uint64_t size);

  // v8::ValueDeserializer::Delegate
  v8::MaybeLocal<v8::Object> ReadHostObject(v8::Isolate*) override;

  RefPtr<ScriptState> m_scriptState;
  RefPtr<SerializedScriptValue> m_serializedScriptValue;
  v8::ValueDeserializer m_deserializer;

  // Message ports which were transferred in.
  const MessagePortArray* m_transferredMessagePorts = nullptr;

  // ImageBitmaps which were transferred in.
  HeapVector<Member<ImageBitmap>> m_transferredImageBitmaps;

  // Blob info for blobs stored by index.
  const WebBlobInfoArray* m_blobInfoArray = nullptr;

  // Set during deserialize after the header is read.
  uint32_t m_version = 0;
#if DCHECK_IS_ON()
  bool m_deserializeInvoked = false;
#endif
};

}  // namespace blink

#endif  // V8ScriptValueDeserializer_h
