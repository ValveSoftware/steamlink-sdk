// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/ReadableStreamBytesConsumer.h"

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "core/streams/ReadableStreamOperations.h"
#include "wtf/Assertions.h"
#include "wtf/text/WTFString.h"
#include <algorithm>
#include <string.h>
#include <v8.h>

namespace blink {

class ReadableStreamBytesConsumer::OnFulfilled final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> createFunction(
      ScriptState* scriptState,
      ReadableStreamBytesConsumer* consumer) {
    return (new OnFulfilled(scriptState, consumer))->bindToV8Function();
  }

  ScriptValue call(ScriptValue v) override {
    bool done;
    v8::Local<v8::Value> item = v.v8Value();
    DCHECK(item->IsObject());
    v8::Local<v8::Value> value =
        v8UnpackIteratorResult(v.getScriptState(), item.As<v8::Object>(), &done)
            .ToLocalChecked();
    if (done) {
      m_consumer->onReadDone();
      return v;
    }
    if (!value->IsUint8Array()) {
      m_consumer->onRejected();
      return ScriptValue();
    }
    m_consumer->onRead(V8Uint8Array::toImpl(value.As<v8::Object>()));
    return v;
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    ScriptFunction::trace(visitor);
  }

 private:
  OnFulfilled(ScriptState* scriptState, ReadableStreamBytesConsumer* consumer)
      : ScriptFunction(scriptState), m_consumer(consumer) {}

  Member<ReadableStreamBytesConsumer> m_consumer;
};

class ReadableStreamBytesConsumer::OnRejected final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> createFunction(
      ScriptState* scriptState,
      ReadableStreamBytesConsumer* consumer) {
    return (new OnRejected(scriptState, consumer))->bindToV8Function();
  }

  ScriptValue call(ScriptValue v) override {
    m_consumer->onRejected();
    return v;
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    ScriptFunction::trace(visitor);
  }

 private:
  OnRejected(ScriptState* scriptState, ReadableStreamBytesConsumer* consumer)
      : ScriptFunction(scriptState), m_consumer(consumer) {}

  Member<ReadableStreamBytesConsumer> m_consumer;
};

ReadableStreamBytesConsumer::ReadableStreamBytesConsumer(
    ScriptState* scriptState,
    ScriptValue streamReader)
    : m_reader(scriptState->isolate(), streamReader.v8Value()),
      m_scriptState(scriptState) {
  m_reader.setPhantom();
}

ReadableStreamBytesConsumer::~ReadableStreamBytesConsumer() {}

BytesConsumer::Result ReadableStreamBytesConsumer::beginRead(
    const char** buffer,
    size_t* available) {
  *buffer = nullptr;
  *available = 0;
  if (m_state == PublicState::Errored)
    return Result::Error;
  if (m_state == PublicState::Closed)
    return Result::Done;

  if (m_pendingBuffer) {
    DCHECK_LE(m_pendingOffset, m_pendingBuffer->length());
    *buffer = reinterpret_cast<const char*>(m_pendingBuffer->data()) +
              m_pendingOffset;
    *available = m_pendingBuffer->length() - m_pendingOffset;
    return Result::Ok;
  }
  if (!m_isReading) {
    m_isReading = true;
    ScriptState::Scope scope(m_scriptState.get());
    ScriptValue reader(m_scriptState.get(),
                       m_reader.newLocal(m_scriptState->isolate()));
    // The owner must retain the reader.
    DCHECK(!reader.isEmpty());
    ReadableStreamOperations::defaultReaderRead(m_scriptState.get(), reader)
        .then(OnFulfilled::createFunction(m_scriptState.get(), this),
              OnRejected::createFunction(m_scriptState.get(), this));
  }
  return Result::ShouldWait;
}

BytesConsumer::Result ReadableStreamBytesConsumer::endRead(size_t readSize) {
  DCHECK(m_pendingBuffer);
  DCHECK_LE(m_pendingOffset + readSize, m_pendingBuffer->length());
  m_pendingOffset += readSize;
  if (m_pendingOffset >= m_pendingBuffer->length()) {
    m_pendingBuffer = nullptr;
    m_pendingOffset = 0;
  }
  return Result::Ok;
}

void ReadableStreamBytesConsumer::setClient(Client* client) {
  DCHECK(!m_client);
  DCHECK(client);
  m_client = client;
}

void ReadableStreamBytesConsumer::clearClient() {
  m_client = nullptr;
}

void ReadableStreamBytesConsumer::cancel() {
  if (m_state == PublicState::Closed || m_state == PublicState::Errored)
    return;
  m_state = PublicState::Closed;
  clearClient();
  m_reader.clear();
}

BytesConsumer::PublicState ReadableStreamBytesConsumer::getPublicState() const {
  return m_state;
}

BytesConsumer::Error ReadableStreamBytesConsumer::getError() const {
  return Error("Failed to read from a ReadableStream.");
}

DEFINE_TRACE(ReadableStreamBytesConsumer) {
  visitor->trace(m_client);
  visitor->trace(m_pendingBuffer);
  BytesConsumer::trace(visitor);
}

void ReadableStreamBytesConsumer::dispose() {
  m_reader.clear();
}

void ReadableStreamBytesConsumer::onRead(DOMUint8Array* buffer) {
  DCHECK(m_isReading);
  DCHECK(buffer);
  DCHECK(!m_pendingBuffer);
  DCHECK(!m_pendingOffset);
  m_isReading = false;
  if (m_state == PublicState::Closed)
    return;
  DCHECK_EQ(m_state, PublicState::ReadableOrWaiting);
  m_pendingBuffer = buffer;
  if (m_client)
    m_client->onStateChange();
}

void ReadableStreamBytesConsumer::onReadDone() {
  DCHECK(m_isReading);
  DCHECK(!m_pendingBuffer);
  m_isReading = false;
  if (m_state == PublicState::Closed)
    return;
  DCHECK_EQ(m_state, PublicState::ReadableOrWaiting);
  m_state = PublicState::Closed;
  m_reader.clear();
  Client* client = m_client;
  clearClient();
  if (client)
    client->onStateChange();
}

void ReadableStreamBytesConsumer::onRejected() {
  DCHECK(m_isReading);
  DCHECK(!m_pendingBuffer);
  m_isReading = false;
  if (m_state == PublicState::Closed)
    return;
  DCHECK_EQ(m_state, PublicState::ReadableOrWaiting);
  m_state = PublicState::Errored;
  m_reader.clear();
  Client* client = m_client;
  clearClient();
  if (client)
    client->onStateChange();
}

}  // namespace blink
