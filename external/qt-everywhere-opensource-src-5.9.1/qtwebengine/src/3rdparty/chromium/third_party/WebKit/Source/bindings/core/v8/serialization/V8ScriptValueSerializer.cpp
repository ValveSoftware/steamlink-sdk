// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8CompositorProxy.h"
#include "bindings/core/v8/V8File.h"
#include "bindings/core/v8/V8FileList.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8OffscreenCanvas.h"
#include "core/dom/DOMArrayBufferBase.h"
#include "core/html/ImageData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebBlobInfo.h"
#include "wtf/AutoReset.h"
#include "wtf/DateMath.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/StringUTF8Adaptor.h"

namespace blink {

V8ScriptValueSerializer::V8ScriptValueSerializer(
    RefPtr<ScriptState> scriptState)
    : m_scriptState(std::move(scriptState)),
      m_serializedScriptValue(SerializedScriptValue::create()),
      m_serializer(m_scriptState->isolate(), this) {
  DCHECK(RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled());
}

RefPtr<SerializedScriptValue> V8ScriptValueSerializer::serialize(
    v8::Local<v8::Value> value,
    Transferables* transferables,
    ExceptionState& exceptionState) {
#if DCHECK_IS_ON()
  DCHECK(!m_serializeInvoked);
  m_serializeInvoked = true;
#endif
  DCHECK(m_serializedScriptValue);
  AutoReset<const ExceptionState*> reset(&m_exceptionState, &exceptionState);

  // Prepare to transfer the provided transferables.
  prepareTransfer(transferables);

  // Serialize the value and handle errors.
  v8::TryCatch tryCatch(m_scriptState->isolate());
  m_serializer.WriteHeader();
  bool wroteValue;
  if (!m_serializer.WriteValue(m_scriptState->context(), value)
           .To(&wroteValue)) {
    DCHECK(tryCatch.HasCaught());
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    return nullptr;
  }
  DCHECK(wroteValue);

  // Finalize the transfer (e.g. neutering array buffers).
  finalizeTransfer(exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  // Finalize the results.
  std::pair<uint8_t*, size_t> buffer = m_serializer.Release();
  m_serializedScriptValue->setData(
      SerializedScriptValue::DataBufferPtr(buffer.first), buffer.second);
  return std::move(m_serializedScriptValue);
}

void V8ScriptValueSerializer::prepareTransfer(Transferables* transferables) {
  if (!transferables)
    return;
  m_transferables = transferables;

  // Transfer array buffers.
  for (uint32_t i = 0; i < transferables->arrayBuffers.size(); i++) {
    DOMArrayBufferBase* arrayBuffer = transferables->arrayBuffers[i].get();
    v8::Local<v8::Value> wrapper = toV8(arrayBuffer, m_scriptState.get());
    if (wrapper->IsArrayBuffer()) {
      m_serializer.TransferArrayBuffer(
          i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    } else if (wrapper->IsSharedArrayBuffer()) {
      m_serializer.TransferSharedArrayBuffer(
          i, v8::Local<v8::SharedArrayBuffer>::Cast(wrapper));
    } else {
      NOTREACHED() << "Unknown type of array buffer in transfer list.";
    }
  }
}

void V8ScriptValueSerializer::finalizeTransfer(ExceptionState& exceptionState) {
  if (!m_transferables)
    return;

  // TODO(jbroman): Strictly speaking, this is not correct; transfer should
  // occur in the order of the transfer list.
  // https://html.spec.whatwg.org/multipage/infrastructure.html#structuredclonewithtransfer

  v8::Isolate* isolate = m_scriptState->isolate();
  m_serializedScriptValue->transferArrayBuffers(
      isolate, m_transferables->arrayBuffers, exceptionState);
  if (exceptionState.hadException())
    return;

  m_serializedScriptValue->transferImageBitmaps(
      isolate, m_transferables->imageBitmaps, exceptionState);
  if (exceptionState.hadException())
    return;

  m_serializedScriptValue->transferOffscreenCanvas(
      isolate, m_transferables->offscreenCanvases, exceptionState);
  if (exceptionState.hadException())
    return;
}

void V8ScriptValueSerializer::writeUTF8String(const String& string) {
  // TODO(jbroman): Ideally this method would take a WTF::StringView, but the
  // StringUTF8Adaptor trick doesn't yet work with StringView.
  StringUTF8Adaptor utf8(string);
  DCHECK_LT(utf8.length(), std::numeric_limits<uint32_t>::max());
  writeUint32(utf8.length());
  writeRawBytes(utf8.data(), utf8.length());
}

bool V8ScriptValueSerializer::writeDOMObject(ScriptWrappable* wrappable,
                                             ExceptionState& exceptionState) {
  const WrapperTypeInfo* wrapperTypeInfo = wrappable->wrapperTypeInfo();
  if (wrapperTypeInfo == &V8Blob::wrapperTypeInfo) {
    Blob* blob = wrappable->toImpl<Blob>();
    if (blob->isClosed()) {
      exceptionState.throwDOMException(
          DataCloneError,
          "A Blob object has been closed, and could therefore not be cloned.");
      return false;
    }
    m_serializedScriptValue->blobDataHandles().set(blob->uuid(),
                                                   blob->blobDataHandle());
    if (m_blobInfoArray) {
      size_t index = m_blobInfoArray->size();
      DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
      m_blobInfoArray->emplace_back(blob->uuid(), blob->type(), blob->size());
      writeTag(BlobIndexTag);
      writeUint32(static_cast<uint32_t>(index));
    } else {
      writeTag(BlobTag);
      writeUTF8String(blob->uuid());
      writeUTF8String(blob->type());
      writeUint64(blob->size());
    }
    return true;
  }
  if (wrapperTypeInfo == &V8CompositorProxy::wrapperTypeInfo) {
    DCHECK(RuntimeEnabledFeatures::compositorWorkerEnabled());
    CompositorProxy* proxy = wrappable->toImpl<CompositorProxy>();
    if (!proxy->connected()) {
      exceptionState.throwDOMException(DataCloneError,
                                       "A CompositorProxy object has been "
                                       "disconnected, and could therefore not "
                                       "be cloned.");
      return false;
    }
    // TODO(jbroman): This seems likely to break in cross-process or
    // persistent scenarios. Keeping as-is for now because the successor
    // does not use this approach (and this feature is unshipped).
    writeTag(CompositorProxyTag);
    writeUint64(proxy->elementId());
    writeUint32(proxy->compositorMutableProperties());
    return true;
  }
  if (wrapperTypeInfo == &V8File::wrapperTypeInfo) {
    writeTag(m_blobInfoArray ? FileIndexTag : FileTag);
    return writeFile(wrappable->toImpl<File>(), exceptionState);
  }
  if (wrapperTypeInfo == &V8FileList::wrapperTypeInfo) {
    // This does not presently deduplicate a File object and its entry in a
    // FileList, which is non-standard behavior.
    FileList* fileList = wrappable->toImpl<FileList>();
    unsigned length = fileList->length();
    writeTag(m_blobInfoArray ? FileListIndexTag : FileListTag);
    writeUint32(length);
    for (unsigned i = 0; i < length; i++) {
      if (!writeFile(fileList->item(i), exceptionState))
        return false;
    }
    return true;
  }
  if (wrapperTypeInfo == &V8ImageBitmap::wrapperTypeInfo) {
    ImageBitmap* imageBitmap = wrappable->toImpl<ImageBitmap>();
    if (imageBitmap->isNeutered()) {
      exceptionState.throwDOMException(
          DataCloneError,
          "An ImageBitmap is detached and could not be cloned.");
      return false;
    }

    // If this ImageBitmap was transferred, it can be serialized by index.
    size_t index = kNotFound;
    if (m_transferables)
      index = m_transferables->imageBitmaps.find(imageBitmap);
    if (index != kNotFound) {
      DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
      writeTag(ImageBitmapTransferTag);
      writeUint32(static_cast<uint32_t>(index));
      return true;
    }

    // Otherwise, it must be fully serialized.
    // Warning: using N32ColorType here is not portable (across CPU
    // architectures, across platforms, etc.).
    RefPtr<Uint8Array> pixels = imageBitmap->copyBitmapData(
        imageBitmap->isPremultiplied() ? PremultiplyAlpha
                                       : DontPremultiplyAlpha,
        N32ColorType);
    writeTag(ImageBitmapTag);
    writeUint32(imageBitmap->originClean());
    writeUint32(imageBitmap->isPremultiplied());
    writeUint32(imageBitmap->width());
    writeUint32(imageBitmap->height());
    writeUint32(pixels->length());
    writeRawBytes(pixels->data(), pixels->length());
    return true;
  }
  if (wrapperTypeInfo == &V8ImageData::wrapperTypeInfo) {
    ImageData* imageData = wrappable->toImpl<ImageData>();
    DOMUint8ClampedArray* pixels = imageData->data();
    writeTag(ImageDataTag);
    writeUint32(imageData->width());
    writeUint32(imageData->height());
    writeUint32(pixels->length());
    writeRawBytes(pixels->data(), pixels->length());
    return true;
  }
  if (wrapperTypeInfo == &V8MessagePort::wrapperTypeInfo) {
    MessagePort* messagePort = wrappable->toImpl<MessagePort>();
    size_t index = kNotFound;
    if (m_transferables)
      index = m_transferables->messagePorts.find(messagePort);
    if (index == kNotFound) {
      exceptionState.throwDOMException(
          DataCloneError,
          "A MessagePort could not be cloned because it was not transferred.");
      return false;
    }
    DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
    writeTag(MessagePortTag);
    writeUint32(static_cast<uint32_t>(index));
    return true;
  }
  if (wrapperTypeInfo == &V8OffscreenCanvas::wrapperTypeInfo) {
    OffscreenCanvas* canvas = wrappable->toImpl<OffscreenCanvas>();
    size_t index = kNotFound;
    if (m_transferables)
      index = m_transferables->offscreenCanvases.find(canvas);
    if (index == kNotFound) {
      exceptionState.throwDOMException(DataCloneError,
                                       "An OffscreenCanvas could not be cloned "
                                       "because it was not transferred.");
      return false;
    }
    if (canvas->isNeutered()) {
      exceptionState.throwDOMException(
          DataCloneError,
          "An OffscreenCanvas could not be cloned because it was detached.");
      return false;
    }
    if (canvas->renderingContext()) {
      exceptionState.throwDOMException(DataCloneError,
                                       "An OffscreenCanvas could not be cloned "
                                       "because it had a rendering context.");
      return false;
    }
    writeTag(OffscreenCanvasTransferTag);
    writeUint32(canvas->width());
    writeUint32(canvas->height());
    writeUint32(canvas->placeholderCanvasId());
    writeUint32(canvas->clientId());
    writeUint32(canvas->sinkId());
    writeUint32(canvas->localId());
    writeUint64(canvas->nonceHigh());
    writeUint64(canvas->nonceLow());
    return true;
  }
  return false;
}

bool V8ScriptValueSerializer::writeFile(File* file,
                                        ExceptionState& exceptionState) {
  if (file->isClosed()) {
    exceptionState.throwDOMException(
        DataCloneError,
        "A File object has been closed, and could therefore not be cloned.");
    return false;
  }
  m_serializedScriptValue->blobDataHandles().set(file->uuid(),
                                                 file->blobDataHandle());
  if (m_blobInfoArray) {
    size_t index = m_blobInfoArray->size();
    DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
    long long size = -1;
    double lastModifiedMs = invalidFileTime();
    file->captureSnapshot(size, lastModifiedMs);
    // FIXME: transition WebBlobInfo.lastModified to be milliseconds-based also.
    double lastModified = lastModifiedMs / msPerSecond;
    m_blobInfoArray->emplace_back(file->uuid(), file->path(), file->name(),
                                  file->type(), lastModified, size);
    writeUint32(static_cast<uint32_t>(index));
  } else {
    writeUTF8String(file->hasBackingFile() ? file->path() : emptyString());
    writeUTF8String(file->name());
    writeUTF8String(file->webkitRelativePath());
    writeUTF8String(file->uuid());
    writeUTF8String(file->type());
    // TODO(jsbell): metadata is unconditionally captured in the index case.
    // Why this inconsistency?
    if (file->hasValidSnapshotMetadata()) {
      writeUint32(1);
      long long size;
      double lastModifiedMs;
      file->captureSnapshot(size, lastModifiedMs);
      DCHECK_GE(size, 0);
      writeUint64(static_cast<uint64_t>(size));
      writeDouble(lastModifiedMs);
    } else {
      writeUint32(0);
    }
    writeUint32(file->getUserVisibility() == File::IsUserVisible ? 1 : 0);
  }
  return true;
}

void V8ScriptValueSerializer::ThrowDataCloneError(
    v8::Local<v8::String> v8Message) {
  DCHECK(m_exceptionState);
  String message = m_exceptionState->addExceptionContext(
      v8StringToWebCoreString<String>(v8Message, DoNotExternalize));
  v8::Local<v8::Value> exception = V8ThrowException::createDOMException(
      m_scriptState->isolate(), DataCloneError, message);
  V8ThrowException::throwException(m_scriptState->isolate(), exception);
}

v8::Maybe<bool> V8ScriptValueSerializer::WriteHostObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> object) {
  DCHECK(m_exceptionState);
  DCHECK_EQ(isolate, m_scriptState->isolate());
  ExceptionState exceptionState(isolate, m_exceptionState->context(),
                                m_exceptionState->interfaceName(),
                                m_exceptionState->propertyName());

  if (!V8DOMWrapper::isWrapper(isolate, object)) {
    exceptionState.throwDOMException(DataCloneError,
                                     "An object could not be cloned.");
    return v8::Nothing<bool>();
  }
  ScriptWrappable* wrappable = toScriptWrappable(object);
  bool wroteDOMObject = writeDOMObject(wrappable, exceptionState);
  if (wroteDOMObject) {
    DCHECK(!exceptionState.hadException());
    return v8::Just(true);
  }
  if (!exceptionState.hadException()) {
    StringView interface = wrappable->wrapperTypeInfo()->interfaceName;
    exceptionState.throwDOMException(
        DataCloneError, interface + " object could not be cloned.");
  }
  return v8::Nothing<bool>();
}

void* V8ScriptValueSerializer::ReallocateBufferMemory(void* oldBuffer,
                                                      size_t size,
                                                      size_t* actualSize) {
  *actualSize = WTF::Partitions::bufferActualSize(size);
  return WTF::Partitions::bufferRealloc(oldBuffer, size,
                                        "SerializedScriptValue buffer");
}

void V8ScriptValueSerializer::FreeBufferMemory(void* buffer) {
  return WTF::Partitions::bufferFree(buffer);
}

}  // namespace blink
