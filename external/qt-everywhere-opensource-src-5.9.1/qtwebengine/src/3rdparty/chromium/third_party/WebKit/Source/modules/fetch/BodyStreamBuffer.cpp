// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BodyStreamBuffer.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStreamController.h"
#include "core/streams/ReadableStreamOperations.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/ReadableStreamBytesConsumer.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include <memory>

namespace blink {

class BodyStreamBuffer::LoaderClient final
    : public GarbageCollectedFinalized<LoaderClient>,
      public ActiveDOMObject,
      public FetchDataLoader::Client {
  WTF_MAKE_NONCOPYABLE(LoaderClient);
  USING_GARBAGE_COLLECTED_MIXIN(LoaderClient);

 public:
  LoaderClient(ExecutionContext* executionContext,
               BodyStreamBuffer* buffer,
               FetchDataLoader::Client* client)
      : ActiveDOMObject(executionContext), m_buffer(buffer), m_client(client) {
    suspendIfNeeded();
  }

  void didFetchDataLoadedBlobHandle(
      PassRefPtr<BlobDataHandle> blobDataHandle) override {
    m_buffer->endLoading();
    m_client->didFetchDataLoadedBlobHandle(std::move(blobDataHandle));
  }

  void didFetchDataLoadedArrayBuffer(DOMArrayBuffer* arrayBuffer) override {
    m_buffer->endLoading();
    m_client->didFetchDataLoadedArrayBuffer(arrayBuffer);
  }

  void didFetchDataLoadedString(const String& string) override {
    m_buffer->endLoading();
    m_client->didFetchDataLoadedString(string);
  }

  void didFetchDataLoadedStream() override {
    m_buffer->endLoading();
    m_client->didFetchDataLoadedStream();
  }

  void didFetchDataLoadFailed() override {
    m_buffer->endLoading();
    m_client->didFetchDataLoadFailed();
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_buffer);
    visitor->trace(m_client);
    ActiveDOMObject::trace(visitor);
    FetchDataLoader::Client::trace(visitor);
  }

 private:
  void contextDestroyed() override { m_buffer->stopLoading(); }

  Member<BodyStreamBuffer> m_buffer;
  Member<FetchDataLoader::Client> m_client;
};

BodyStreamBuffer::BodyStreamBuffer(ScriptState* scriptState,
                                   BytesConsumer* consumer)
    : UnderlyingSourceBase(scriptState),
      m_scriptState(scriptState),
      m_consumer(consumer),
      m_madeFromReadableStream(false) {
  v8::Local<v8::Value> bodyValue = toV8(this, scriptState);
  DCHECK(!bodyValue.IsEmpty());
  DCHECK(bodyValue->IsObject());
  v8::Local<v8::Object> body = bodyValue.As<v8::Object>();

  ScriptValue readableStream = ReadableStreamOperations::createReadableStream(
      scriptState, this,
      ReadableStreamOperations::createCountQueuingStrategy(scriptState, 0));
  DCHECK(!readableStream.isEmpty());
  V8HiddenValue::setHiddenValue(
      scriptState, body,
      V8HiddenValue::internalBodyStream(scriptState->isolate()),
      readableStream.v8Value());
  m_consumer->setClient(this);
  onStateChange();
}

BodyStreamBuffer::BodyStreamBuffer(ScriptState* scriptState, ScriptValue stream)
    : UnderlyingSourceBase(scriptState),
      m_scriptState(scriptState),
      m_madeFromReadableStream(true) {
  DCHECK(ReadableStreamOperations::isReadableStream(scriptState, stream));
  v8::Local<v8::Value> bodyValue = toV8(this, scriptState);
  DCHECK(!bodyValue.IsEmpty());
  DCHECK(bodyValue->IsObject());
  v8::Local<v8::Object> body = bodyValue.As<v8::Object>();

  V8HiddenValue::setHiddenValue(
      scriptState, body,
      V8HiddenValue::internalBodyStream(scriptState->isolate()),
      stream.v8Value());
}

ScriptValue BodyStreamBuffer::stream() {
  ScriptState::Scope scope(m_scriptState.get());
  v8::Local<v8::Value> bodyValue = toV8(this, m_scriptState.get());
  DCHECK(!bodyValue.IsEmpty());
  DCHECK(bodyValue->IsObject());
  v8::Local<v8::Object> body = bodyValue.As<v8::Object>();
  return ScriptValue(
      m_scriptState.get(),
      V8HiddenValue::getHiddenValue(
          m_scriptState.get(), body,
          V8HiddenValue::internalBodyStream(m_scriptState->isolate())));
}

PassRefPtr<BlobDataHandle> BodyStreamBuffer::drainAsBlobDataHandle(
    BytesConsumer::BlobSizePolicy policy) {
  ASSERT(!isStreamLocked());
  ASSERT(!isStreamDisturbed());
  if (isStreamClosed() || isStreamErrored())
    return nullptr;

  if (m_madeFromReadableStream)
    return nullptr;

  RefPtr<BlobDataHandle> blobDataHandle =
      m_consumer->drainAsBlobDataHandle(policy);
  if (blobDataHandle) {
    closeAndLockAndDisturb();
    return blobDataHandle.release();
  }
  return nullptr;
}

PassRefPtr<EncodedFormData> BodyStreamBuffer::drainAsFormData() {
  ASSERT(!isStreamLocked());
  ASSERT(!isStreamDisturbed());
  if (isStreamClosed() || isStreamErrored())
    return nullptr;

  if (m_madeFromReadableStream)
    return nullptr;

  RefPtr<EncodedFormData> formData = m_consumer->drainAsFormData();
  if (formData) {
    closeAndLockAndDisturb();
    return formData.release();
  }
  return nullptr;
}

void BodyStreamBuffer::startLoading(FetchDataLoader* loader,
                                    FetchDataLoader::Client* client) {
  ASSERT(!m_loader);
  ASSERT(m_scriptState->contextIsValid());
  m_loader = loader;
  loader->start(
      releaseHandle(),
      new LoaderClient(m_scriptState->getExecutionContext(), this, client));
}

void BodyStreamBuffer::tee(BodyStreamBuffer** branch1,
                           BodyStreamBuffer** branch2) {
  DCHECK(!isStreamLocked());
  DCHECK(!isStreamDisturbed());
  *branch1 = nullptr;
  *branch2 = nullptr;

  if (m_madeFromReadableStream) {
    ScriptValue stream1, stream2;
    ReadableStreamOperations::tee(m_scriptState.get(), stream(), &stream1,
                                  &stream2);
    *branch1 = new BodyStreamBuffer(m_scriptState.get(), stream1);
    *branch2 = new BodyStreamBuffer(m_scriptState.get(), stream2);
    return;
  }
  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(m_scriptState->getExecutionContext(), releaseHandle(),
                     &dest1, &dest2);
  *branch1 = new BodyStreamBuffer(m_scriptState.get(), dest1);
  *branch2 = new BodyStreamBuffer(m_scriptState.get(), dest2);
}

ScriptPromise BodyStreamBuffer::pull(ScriptState* scriptState) {
  ASSERT(scriptState == m_scriptState.get());
  if (m_streamNeedsMore)
    return ScriptPromise::castUndefined(scriptState);
  m_streamNeedsMore = true;
  processData();
  return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise BodyStreamBuffer::cancel(ScriptState* scriptState,
                                       ScriptValue reason) {
  ASSERT(scriptState == m_scriptState.get());
  close();
  return ScriptPromise::castUndefined(scriptState);
}

void BodyStreamBuffer::onStateChange() {
  if (!m_consumer || !getExecutionContext() ||
      getExecutionContext()->activeDOMObjectsAreStopped())
    return;

  switch (m_consumer->getPublicState()) {
    case BytesConsumer::PublicState::ReadableOrWaiting:
      break;
    case BytesConsumer::PublicState::Closed:
      close();
      return;
    case BytesConsumer::PublicState::Errored:
      error();
      return;
  }
  processData();
}

bool BodyStreamBuffer::hasPendingActivity() const {
  if (m_loader)
    return true;
  return UnderlyingSourceBase::hasPendingActivity();
}

void BodyStreamBuffer::contextDestroyed() {
  cancelConsumer();
  UnderlyingSourceBase::contextDestroyed();
}

bool BodyStreamBuffer::isStreamReadable() {
  ScriptState::Scope scope(m_scriptState.get());
  return ReadableStreamOperations::isReadable(m_scriptState.get(), stream());
}

bool BodyStreamBuffer::isStreamClosed() {
  ScriptState::Scope scope(m_scriptState.get());
  return ReadableStreamOperations::isClosed(m_scriptState.get(), stream());
}

bool BodyStreamBuffer::isStreamErrored() {
  ScriptState::Scope scope(m_scriptState.get());
  return ReadableStreamOperations::isErrored(m_scriptState.get(), stream());
}

bool BodyStreamBuffer::isStreamLocked() {
  ScriptState::Scope scope(m_scriptState.get());
  return ReadableStreamOperations::isLocked(m_scriptState.get(), stream());
}

bool BodyStreamBuffer::isStreamDisturbed() {
  ScriptState::Scope scope(m_scriptState.get());
  return ReadableStreamOperations::isDisturbed(m_scriptState.get(), stream());
}

void BodyStreamBuffer::closeAndLockAndDisturb() {
  if (isStreamReadable()) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    close();
  }

  ScriptState::Scope scope(m_scriptState.get());
  NonThrowableExceptionState exceptionState;
  ScriptValue reader = ReadableStreamOperations::getReader(
      m_scriptState.get(), stream(), exceptionState);
  ReadableStreamOperations::defaultReaderRead(m_scriptState.get(), reader);
}

void BodyStreamBuffer::close() {
  controller()->close();
  cancelConsumer();
}

void BodyStreamBuffer::error() {
  {
    ScriptState::Scope scope(m_scriptState.get());
    controller()->error(V8ThrowException::createTypeError(
        m_scriptState->isolate(), "network error"));
  }
  cancelConsumer();
}

void BodyStreamBuffer::cancelConsumer() {
  if (m_consumer) {
    m_consumer->cancel();
    m_consumer = nullptr;
  }
}

void BodyStreamBuffer::processData() {
  DCHECK(m_consumer);
  while (m_streamNeedsMore) {
    const char* buffer = nullptr;
    size_t available = 0;
    auto result = m_consumer->beginRead(&buffer, &available);
    if (result == BytesConsumer::Result::ShouldWait)
      return;
    DOMUint8Array* array = nullptr;
    if (result == BytesConsumer::Result::Ok) {
      array = DOMUint8Array::create(
          reinterpret_cast<const unsigned char*>(buffer), available);
      result = m_consumer->endRead(available);
    }
    switch (result) {
      case BytesConsumer::Result::Ok: {
        DCHECK(array);
        // Clear m_streamNeedsMore in order to detect a pull call.
        m_streamNeedsMore = false;
        controller()->enqueue(array);
        // If m_streamNeedsMore is true, it means that pull is called and
        // the stream needs more data even if the desired size is not
        // positive.
        if (!m_streamNeedsMore)
          m_streamNeedsMore = controller()->desiredSize() > 0;
        break;
      }
      case BytesConsumer::Result::ShouldWait:
        NOTREACHED();
        return;
      case BytesConsumer::Result::Done:
        close();
        return;
      case BytesConsumer::Result::Error:
        error();
        return;
    }
  }
}

void BodyStreamBuffer::endLoading() {
  ASSERT(m_loader);
  m_loader = nullptr;
}

void BodyStreamBuffer::stopLoading() {
  if (!m_loader)
    return;
  m_loader->cancel();
  m_loader = nullptr;
}

BytesConsumer* BodyStreamBuffer::releaseHandle() {
  DCHECK(!isStreamLocked());
  DCHECK(!isStreamDisturbed());

  if (m_madeFromReadableStream) {
    ScriptState::Scope scope(m_scriptState.get());
    // We need to have |reader| alive by some means (as written in
    // ReadableStreamDataConsumerHandle). Based on the following facts
    //  - This function is used only from tee and startLoading.
    //  - This branch cannot be taken when called from tee.
    //  - startLoading makes hasPendingActivity return true while loading.
    // , we don't need to keep the reader explicitly.
    NonThrowableExceptionState exceptionState;
    ScriptValue reader = ReadableStreamOperations::getReader(
        m_scriptState.get(), stream(), exceptionState);
    return new ReadableStreamBytesConsumer(m_scriptState.get(), reader);
  }
  // We need to call these before calling closeAndLockAndDisturb.
  const bool isClosed = isStreamClosed();
  const bool isErrored = isStreamErrored();
  BytesConsumer* consumer = m_consumer.release();

  closeAndLockAndDisturb();

  if (isClosed) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    return BytesConsumer::createClosed();
  }
  if (isErrored)
    return BytesConsumer::createErrored(BytesConsumer::Error("error"));

  DCHECK(consumer);
  consumer->clearClient();
  return consumer;
}

}  // namespace blink
