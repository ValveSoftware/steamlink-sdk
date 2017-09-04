// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Functional.h"

#include <algorithm>
#include <string.h>

namespace blink {

BytesConsumerForDataConsumerHandle::BytesConsumerForDataConsumerHandle(
    ExecutionContext* executionContext,
    std::unique_ptr<WebDataConsumerHandle> handle)
    : m_executionContext(executionContext),
      m_reader(handle->obtainReader(this)) {}

BytesConsumerForDataConsumerHandle::~BytesConsumerForDataConsumerHandle() {}

BytesConsumer::Result BytesConsumerForDataConsumerHandle::beginRead(
    const char** buffer,
    size_t* available) {
  DCHECK(!m_isInTwoPhaseRead);
  *buffer = nullptr;
  *available = 0;
  if (m_state == InternalState::Closed)
    return Result::Done;
  if (m_state == InternalState::Errored)
    return Result::Error;

  WebDataConsumerHandle::Result r =
      m_reader->beginRead(reinterpret_cast<const void**>(buffer),
                          WebDataConsumerHandle::FlagNone, available);
  switch (r) {
    case WebDataConsumerHandle::Ok:
      m_isInTwoPhaseRead = true;
      return Result::Ok;
    case WebDataConsumerHandle::ShouldWait:
      return Result::ShouldWait;
    case WebDataConsumerHandle::Done:
      close();
      return Result::Done;
    case WebDataConsumerHandle::Busy:
    case WebDataConsumerHandle::ResourceExhausted:
    case WebDataConsumerHandle::UnexpectedError:
      error();
      return Result::Error;
  }
  NOTREACHED();
  return Result::Error;
}

BytesConsumer::Result BytesConsumerForDataConsumerHandle::endRead(size_t read) {
  DCHECK(m_isInTwoPhaseRead);
  m_isInTwoPhaseRead = false;
  DCHECK(m_state == InternalState::Readable ||
         m_state == InternalState::Waiting);
  WebDataConsumerHandle::Result r = m_reader->endRead(read);
  if (r != WebDataConsumerHandle::Ok) {
    m_hasPendingNotification = false;
    error();
    return Result::Error;
  }
  if (m_hasPendingNotification) {
    m_hasPendingNotification = false;
    TaskRunnerHelper::get(TaskType::Networking, m_executionContext)
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&BytesConsumerForDataConsumerHandle::notify,
                             wrapPersistent(this)));
  }
  return Result::Ok;
}

void BytesConsumerForDataConsumerHandle::setClient(
    BytesConsumer::Client* client) {
  DCHECK(!m_client);
  DCHECK(client);
  if (m_state == InternalState::Readable || m_state == InternalState::Waiting)
    m_client = client;
}

void BytesConsumerForDataConsumerHandle::clearClient() {
  m_client = nullptr;
}

void BytesConsumerForDataConsumerHandle::cancel() {
  DCHECK(!m_isInTwoPhaseRead);
  if (m_state == InternalState::Readable || m_state == InternalState::Waiting) {
    // We don't want the client to be notified in this case.
    BytesConsumer::Client* client = m_client;
    m_client = nullptr;
    close();
    m_client = client;
  }
}

BytesConsumer::PublicState BytesConsumerForDataConsumerHandle::getPublicState()
    const {
  return getPublicStateFromInternalState(m_state);
}

void BytesConsumerForDataConsumerHandle::didGetReadable() {
  DCHECK(m_state == InternalState::Readable ||
         m_state == InternalState::Waiting);
  if (m_isInTwoPhaseRead) {
    m_hasPendingNotification = true;
    return;
  }
  // Perform zero-length read to call check handle's status.
  size_t readSize;
  WebDataConsumerHandle::Result result =
      m_reader->read(nullptr, 0, WebDataConsumerHandle::FlagNone, &readSize);
  BytesConsumer::Client* client = m_client;
  switch (result) {
    case WebDataConsumerHandle::Ok:
    case WebDataConsumerHandle::ShouldWait:
      if (client)
        client->onStateChange();
      return;
    case WebDataConsumerHandle::Done:
      close();
      if (client)
        client->onStateChange();
      return;
    case WebDataConsumerHandle::Busy:
    case WebDataConsumerHandle::ResourceExhausted:
    case WebDataConsumerHandle::UnexpectedError:
      error();
      if (client)
        client->onStateChange();
      return;
  }
  return;
}

DEFINE_TRACE(BytesConsumerForDataConsumerHandle) {
  visitor->trace(m_executionContext);
  visitor->trace(m_client);
  BytesConsumer::trace(visitor);
}

void BytesConsumerForDataConsumerHandle::close() {
  DCHECK(!m_isInTwoPhaseRead);
  if (m_state == InternalState::Closed)
    return;
  DCHECK(m_state == InternalState::Readable ||
         m_state == InternalState::Waiting);
  m_state = InternalState::Closed;
  m_reader = nullptr;
  clearClient();
}

void BytesConsumerForDataConsumerHandle::error() {
  DCHECK(!m_isInTwoPhaseRead);
  if (m_state == InternalState::Errored)
    return;
  DCHECK(m_state == InternalState::Readable ||
         m_state == InternalState::Waiting);
  m_state = InternalState::Errored;
  m_reader = nullptr;
  m_error = Error("error");
  clearClient();
}

void BytesConsumerForDataConsumerHandle::notify() {
  if (m_state == InternalState::Closed || m_state == InternalState::Errored)
    return;
  didGetReadable();
}

}  // namespace blink
