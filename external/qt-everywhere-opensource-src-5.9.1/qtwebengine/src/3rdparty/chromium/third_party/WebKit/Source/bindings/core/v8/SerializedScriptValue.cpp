/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "bindings/core/v8/SerializedScriptValue.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValueSerializer.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/Transferables.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8OffscreenCanvas.h"
#include "bindings/core/v8/V8SharedArrayBuffer.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/frame/ImageBitmap.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/ByteOrder.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringHash.h"
#include <memory>

namespace blink {

PassRefPtr<SerializedScriptValue> SerializedScriptValue::serialize(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    Transferables* transferables,
    WebBlobInfoArray* blobInfo,
    ExceptionState& exception) {
  return SerializedScriptValueFactory::instance().create(
      isolate, value, transferables, blobInfo, exception);
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::serialize(
    const String& str) {
  return create(ScriptValueSerializer::serializeWTFString(str));
}

PassRefPtr<SerializedScriptValue>
SerializedScriptValue::serializeAndSwallowExceptions(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  TrackExceptionState exceptionState;
  RefPtr<SerializedScriptValue> serialized =
      serialize(isolate, value, nullptr, nullptr, exceptionState);
  if (exceptionState.hadException())
    return nullValue();
  return serialized.release();
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create() {
  return adoptRef(new SerializedScriptValue);
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(
    const String& data) {
  return adoptRef(new SerializedScriptValue(data));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(
    const char* data,
    size_t length) {
  if (!data)
    return create();

  // Decode wire data from big endian to host byte order.
  DCHECK(!(length % sizeof(UChar)));
  size_t stringLength = length / sizeof(UChar);
  StringBuffer<UChar> buffer(stringLength);
  const UChar* src = reinterpret_cast<const UChar*>(data);
  UChar* dst = buffer.characters();
  for (size_t i = 0; i < stringLength; i++)
    dst[i] = ntohs(src[i]);

  return adoptRef(new SerializedScriptValue(String::adopt(buffer)));
}

SerializedScriptValue::SerializedScriptValue()
    : m_externallyAllocatedMemory(0) {}

SerializedScriptValue::SerializedScriptValue(const String& wireData)
    : m_dataString(wireData.isolatedCopy()), m_externallyAllocatedMemory(0) {}

SerializedScriptValue::~SerializedScriptValue() {
  // If the allocated memory was not registered before, then this class is
  // likely used in a context other than Worker's onmessage environment and the
  // presence of current v8 context is not guaranteed. Avoid calling v8 then.
  if (m_externallyAllocatedMemory) {
    ASSERT(v8::Isolate::GetCurrent());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -m_externallyAllocatedMemory);
  }
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::nullValue() {
  return create(ScriptValueSerializer::serializeNullValue());
}

String SerializedScriptValue::toWireString() const {
  if (!m_dataString.isNull())
    return m_dataString;

  // Add the padding '\0', but don't put it in |m_dataBuffer|.
  // This requires direct use of uninitialized strings, though.
  UChar* destination;
  size_t stringSizeBytes = (m_dataBufferSize + 1) & ~1;
  String wireString =
      String::createUninitialized(stringSizeBytes / 2, destination);
  memcpy(destination, m_dataBuffer.get(), m_dataBufferSize);
  if (stringSizeBytes > m_dataBufferSize)
    reinterpret_cast<char*>(destination)[stringSizeBytes - 1] = '\0';
  return wireString;
}

// Convert serialized string to big endian wire data.
void SerializedScriptValue::toWireBytes(Vector<char>& result) const {
  DCHECK(result.isEmpty());

  if (m_dataString.isNull()) {
    size_t wireSizeBytes = (m_dataBufferSize + 1) & ~1;
    result.resize(wireSizeBytes);

    const UChar* src = reinterpret_cast<UChar*>(m_dataBuffer.get());
    UChar* dst = reinterpret_cast<UChar*>(result.data());
    for (size_t i = 0; i < m_dataBufferSize / 2; i++)
      dst[i] = htons(src[i]);

    // This is equivalent to swapping the byte order of the two bytes (x, 0),
    // depending on endianness.
    if (m_dataBufferSize % 2)
      dst[wireSizeBytes / 2 - 1] = m_dataBuffer[m_dataBufferSize - 1] << 8;

    return;
  }

  size_t length = m_dataString.length();
  result.resize(length * sizeof(UChar));
  UChar* dst = reinterpret_cast<UChar*>(result.data());

  if (m_dataString.is8Bit()) {
    const LChar* src = m_dataString.characters8();
    for (size_t i = 0; i < length; i++)
      dst[i] = htons(static_cast<UChar>(src[i]));
  } else {
    const UChar* src = m_dataString.characters16();
    for (size_t i = 0; i < length; i++)
      dst[i] = htons(src[i]);
  }
}

static void accumulateArrayBuffersForAllWorlds(
    v8::Isolate* isolate,
    DOMArrayBuffer* object,
    Vector<v8::Local<v8::ArrayBuffer>, 4>& buffers) {
  if (isMainThread()) {
    Vector<RefPtr<DOMWrapperWorld>> worlds;
    DOMWrapperWorld::allWorldsInMainThread(worlds);
    for (size_t i = 0; i < worlds.size(); i++) {
      v8::Local<v8::Object> wrapper =
          worlds[i]->domDataStore().get(object, isolate);
      if (!wrapper.IsEmpty())
        buffers.append(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    }
  } else {
    v8::Local<v8::Object> wrapper =
        DOMWrapperWorld::current(isolate).domDataStore().get(object, isolate);
    if (!wrapper.IsEmpty())
      buffers.append(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
  }
}

void SerializedScriptValue::transferImageBitmaps(
    v8::Isolate* isolate,
    const ImageBitmapArray& imageBitmaps,
    ExceptionState& exceptionState) {
  if (!imageBitmaps.size())
    return;

  for (size_t i = 0; i < imageBitmaps.size(); ++i) {
    if (imageBitmaps[i]->isNeutered()) {
      exceptionState.throwDOMException(
          DataCloneError, "ImageBitmap at index " + String::number(i) +
                              " is already detached.");
      return;
    }
  }

  std::unique_ptr<ImageBitmapContentsArray> contents =
      wrapUnique(new ImageBitmapContentsArray);
  HeapHashSet<Member<ImageBitmap>> visited;
  for (size_t i = 0; i < imageBitmaps.size(); ++i) {
    if (visited.contains(imageBitmaps[i]))
      continue;
    visited.add(imageBitmaps[i]);
    contents->append(imageBitmaps[i]->transfer());
  }
  m_imageBitmapContentsArray = std::move(contents);
}

void SerializedScriptValue::transferOffscreenCanvas(
    v8::Isolate* isolate,
    const OffscreenCanvasArray& offscreenCanvases,
    ExceptionState& exceptionState) {
  if (!offscreenCanvases.size())
    return;

  HeapHashSet<Member<OffscreenCanvas>> visited;
  for (size_t i = 0; i < offscreenCanvases.size(); i++) {
    if (visited.contains(offscreenCanvases[i].get()))
      continue;
    if (offscreenCanvases[i]->isNeutered()) {
      exceptionState.throwDOMException(
          DataCloneError, "OffscreenCanvas at index " + String::number(i) +
                              " is already detached.");
      return;
    }
    if (offscreenCanvases[i]->renderingContext()) {
      exceptionState.throwDOMException(
          DataCloneError, "OffscreenCanvas at index " + String::number(i) +
                              " has an associated context.");
      return;
    }
    visited.add(offscreenCanvases[i].get());
    offscreenCanvases[i].get()->setNeutered();
  }
}

void SerializedScriptValue::transferArrayBuffers(
    v8::Isolate* isolate,
    const ArrayBufferArray& arrayBuffers,
    ExceptionState& exceptionState) {
  m_arrayBufferContentsArray =
      transferArrayBufferContents(isolate, arrayBuffers, exceptionState);
}

v8::Local<v8::Value> SerializedScriptValue::deserialize(
    MessagePortArray* messagePorts) {
  return deserialize(v8::Isolate::GetCurrent(), messagePorts, 0);
}

v8::Local<v8::Value> SerializedScriptValue::deserialize(
    v8::Isolate* isolate,
    MessagePortArray* messagePorts,
    const WebBlobInfoArray* blobInfo) {
  return SerializedScriptValueFactory::instance().deserialize(
      this, isolate, messagePorts, blobInfo);
}

bool SerializedScriptValue::extractTransferables(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    int argumentIndex,
    Transferables& transferables,
    ExceptionState& exceptionState) {
  if (value.IsEmpty() || value->IsUndefined())
    return true;

  uint32_t length = 0;
  if (value->IsArray()) {
    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
    length = array->Length();
  } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
    if (!exceptionState.hadException())
      exceptionState.throwTypeError(
          ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex + 1));
    return false;
  }

  v8::Local<v8::Object> transferableArray = v8::Local<v8::Object>::Cast(value);

  // Validate the passed array of transferables.
  for (unsigned i = 0; i < length; ++i) {
    v8::Local<v8::Value> transferableObject;
    if (!transferableArray->Get(isolate->GetCurrentContext(), i)
             .ToLocal(&transferableObject))
      return false;
    // Validation of non-null objects, per HTML5 spec 10.3.3.
    if (isUndefinedOrNull(transferableObject)) {
      exceptionState.throwTypeError(
          "Value at index " + String::number(i) + " is an untransferable " +
          (transferableObject->IsUndefined() ? "'undefined'" : "'null'") +
          " value.");
      return false;
    }
    // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
    if (V8MessagePort::hasInstance(transferableObject, isolate)) {
      MessagePort* port = V8MessagePort::toImpl(
          v8::Local<v8::Object>::Cast(transferableObject));
      // Check for duplicate MessagePorts.
      if (transferables.messagePorts.contains(port)) {
        exceptionState.throwDOMException(
            DataCloneError, "Message port at index " + String::number(i) +
                                " is a duplicate of an earlier port.");
        return false;
      }
      transferables.messagePorts.append(port);
    } else if (transferableObject->IsArrayBuffer()) {
      DOMArrayBuffer* arrayBuffer = V8ArrayBuffer::toImpl(
          v8::Local<v8::Object>::Cast(transferableObject));
      if (transferables.arrayBuffers.contains(arrayBuffer)) {
        exceptionState.throwDOMException(
            DataCloneError, "ArrayBuffer at index " + String::number(i) +
                                " is a duplicate of an earlier ArrayBuffer.");
        return false;
      }
      transferables.arrayBuffers.append(arrayBuffer);
    } else if (transferableObject->IsSharedArrayBuffer()) {
      DOMSharedArrayBuffer* sharedArrayBuffer = V8SharedArrayBuffer::toImpl(
          v8::Local<v8::Object>::Cast(transferableObject));
      if (transferables.arrayBuffers.contains(sharedArrayBuffer)) {
        exceptionState.throwDOMException(
            DataCloneError,
            "SharedArrayBuffer at index " + String::number(i) +
                " is a duplicate of an earlier SharedArrayBuffer.");
        return false;
      }
      transferables.arrayBuffers.append(sharedArrayBuffer);
    } else if (V8ImageBitmap::hasInstance(transferableObject, isolate)) {
      ImageBitmap* imageBitmap = V8ImageBitmap::toImpl(
          v8::Local<v8::Object>::Cast(transferableObject));
      if (transferables.imageBitmaps.contains(imageBitmap)) {
        exceptionState.throwDOMException(
            DataCloneError, "ImageBitmap at index " + String::number(i) +
                                " is a duplicate of an earlier ImageBitmap.");
        return false;
      }
      transferables.imageBitmaps.append(imageBitmap);
    } else if (V8OffscreenCanvas::hasInstance(transferableObject, isolate)) {
      OffscreenCanvas* offscreenCanvas = V8OffscreenCanvas::toImpl(
          v8::Local<v8::Object>::Cast(transferableObject));
      if (transferables.offscreenCanvases.contains(offscreenCanvas)) {
        exceptionState.throwDOMException(
            DataCloneError,
            "OffscreenCanvas at index " + String::number(i) +
                " is a duplicate of an earlier OffscreenCanvas.");
        return false;
      }
      transferables.offscreenCanvases.append(offscreenCanvas);
    } else {
      exceptionState.throwTypeError("Value at index " + String::number(i) +
                                    " does not have a transferable type.");
      return false;
    }
  }
  return true;
}

std::unique_ptr<ArrayBufferContentsArray>
SerializedScriptValue::transferArrayBufferContents(
    v8::Isolate* isolate,
    const ArrayBufferArray& arrayBuffers,
    ExceptionState& exceptionState) {
  if (!arrayBuffers.size())
    return nullptr;

  for (auto it = arrayBuffers.begin(); it != arrayBuffers.end(); ++it) {
    DOMArrayBufferBase* arrayBuffer = *it;
    if (arrayBuffer->isNeutered()) {
      size_t index = std::distance(arrayBuffers.begin(), it);
      exceptionState.throwDOMException(
          DataCloneError, "ArrayBuffer at index " + String::number(index) +
                              " is already neutered.");
      return nullptr;
    }
  }

  std::unique_ptr<ArrayBufferContentsArray> contents =
      wrapUnique(new ArrayBufferContentsArray(arrayBuffers.size()));

  HeapHashSet<Member<DOMArrayBufferBase>> visited;
  for (auto it = arrayBuffers.begin(); it != arrayBuffers.end(); ++it) {
    DOMArrayBufferBase* arrayBuffer = *it;
    if (visited.contains(arrayBuffer))
      continue;
    visited.add(arrayBuffer);

    size_t index = std::distance(arrayBuffers.begin(), it);
    if (arrayBuffer->isShared()) {
      if (!arrayBuffer->shareContentsWith(contents->at(index))) {
        exceptionState.throwDOMException(DataCloneError,
                                         "SharedArrayBuffer at index " +
                                             String::number(index) +
                                             " could not be transferred.");
        return nullptr;
      }
    } else {
      Vector<v8::Local<v8::ArrayBuffer>, 4> bufferHandles;
      v8::HandleScope handleScope(isolate);
      accumulateArrayBuffersForAllWorlds(
          isolate, static_cast<DOMArrayBuffer*>(it->get()), bufferHandles);
      bool isNeuterable = true;
      for (const auto& bufferHandle : bufferHandles)
        isNeuterable &= bufferHandle->IsNeuterable();

      DOMArrayBufferBase* toTransfer = arrayBuffer;
      if (!isNeuterable) {
        toTransfer = DOMArrayBuffer::create(
            arrayBuffer->buffer()->data(), arrayBuffer->buffer()->byteLength());
      }
      if (!toTransfer->transfer(contents->at(index))) {
        exceptionState.throwDOMException(
            DataCloneError, "ArrayBuffer at index " + String::number(index) +
                                " could not be transferred.");
        return nullptr;
      }

      if (isNeuterable) {
        for (const auto& bufferHandle : bufferHandles)
          bufferHandle->Neuter();
      }
    }
  }
  return contents;
}

void SerializedScriptValue::registerMemoryAllocatedWithCurrentScriptContext() {
  if (m_externallyAllocatedMemory)
    return;

  m_externallyAllocatedMemory = static_cast<intptr_t>(dataLengthInBytes());
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      m_externallyAllocatedMemory);
}

}  // namespace blink
