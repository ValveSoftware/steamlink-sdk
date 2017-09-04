// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumerTestUtil.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/WebTaskRunner.h"
#include "platform/testing/UnitTestHelpers.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"

namespace blink {

using Result = BytesConsumer::Result;
using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

BytesConsumerTestUtil::MockBytesConsumer::MockBytesConsumer() {
  ON_CALL(*this, beginRead(_, _))
      .WillByDefault(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                           Return(Result::Error)));
  ON_CALL(*this, endRead(_)).WillByDefault(Return(Result::Error));
  ON_CALL(*this, getPublicState()).WillByDefault(Return(PublicState::Errored));
  ON_CALL(*this, drainAsBlobDataHandle(_))
      .WillByDefault(Return(ByMove(nullptr)));
  ON_CALL(*this, drainAsFormData()).WillByDefault(Return(ByMove(nullptr)));
}

BytesConsumerTestUtil::ReplayingBytesConsumer::ReplayingBytesConsumer(
    ExecutionContext* executionContext)
    : m_executionContext(executionContext) {}

BytesConsumerTestUtil::ReplayingBytesConsumer::~ReplayingBytesConsumer() {}

Result BytesConsumerTestUtil::ReplayingBytesConsumer::beginRead(
    const char** buffer,
    size_t* available) {
  ++m_notificationToken;
  if (m_commands.isEmpty()) {
    switch (m_state) {
      case BytesConsumer::InternalState::Readable:
      case BytesConsumer::InternalState::Waiting:
        return Result::ShouldWait;
      case BytesConsumer::InternalState::Closed:
        return Result::Done;
      case BytesConsumer::InternalState::Errored:
        return Result::Error;
    }
  }
  const Command& command = m_commands[0];
  switch (command.getName()) {
    case Command::Data:
      DCHECK_LE(m_offset, command.body().size());
      *buffer = command.body().data() + m_offset;
      *available = command.body().size() - m_offset;
      return Result::Ok;
    case Command::Done:
      m_commands.removeFirst();
      close();
      return Result::Done;
    case Command::Error: {
      Error e(String::fromUTF8(command.body().data(), command.body().size()));
      m_commands.removeFirst();
      error(std::move(e));
      return Result::Error;
    }
    case Command::Wait:
      m_commands.removeFirst();
      m_state = InternalState::Waiting;
      TaskRunnerHelper::get(TaskType::Networking, m_executionContext)
          ->postTask(BLINK_FROM_HERE,
                     WTF::bind(&ReplayingBytesConsumer::notifyAsReadable,
                               wrapPersistent(this), m_notificationToken));
      return Result::ShouldWait;
  }
  NOTREACHED();
  return Result::Error;
}

Result BytesConsumerTestUtil::ReplayingBytesConsumer::endRead(size_t read) {
  DCHECK(!m_commands.isEmpty());
  const Command& command = m_commands[0];
  DCHECK_EQ(Command::Data, command.getName());
  m_offset += read;
  DCHECK_LE(m_offset, command.body().size());
  if (m_offset < command.body().size())
    return Result::Ok;

  m_offset = 0;
  m_commands.removeFirst();
  return Result::Ok;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::setClient(Client* client) {
  DCHECK(!m_client);
  DCHECK(client);
  m_client = client;
  ++m_notificationToken;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::clearClient() {
  DCHECK(m_client);
  m_client = nullptr;
  ++m_notificationToken;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::cancel() {
  close();
  m_isCancelled = true;
}

BytesConsumer::PublicState
BytesConsumerTestUtil::ReplayingBytesConsumer::getPublicState() const {
  return getPublicStateFromInternalState(m_state);
}

BytesConsumer::Error BytesConsumerTestUtil::ReplayingBytesConsumer::getError()
    const {
  return m_error;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::notifyAsReadable(
    int notificationToken) {
  if (m_notificationToken != notificationToken) {
    // The notification is cancelled.
    return;
  }
  DCHECK(m_client);
  DCHECK_NE(InternalState::Closed, m_state);
  DCHECK_NE(InternalState::Errored, m_state);
  m_client->onStateChange();
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::close() {
  m_commands.clear();
  m_offset = 0;
  m_state = InternalState::Closed;
  ++m_notificationToken;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::error(const Error& e) {
  m_commands.clear();
  m_offset = 0;
  m_error = e;
  m_state = InternalState::Errored;
  ++m_notificationToken;
}

DEFINE_TRACE(BytesConsumerTestUtil::ReplayingBytesConsumer) {
  visitor->trace(m_executionContext);
  visitor->trace(m_client);
  BytesConsumer::trace(visitor);
}

BytesConsumerTestUtil::TwoPhaseReader::TwoPhaseReader(BytesConsumer* consumer)
    : m_consumer(consumer) {
  m_consumer->setClient(this);
}

void BytesConsumerTestUtil::TwoPhaseReader::onStateChange() {
  while (true) {
    const char* buffer = nullptr;
    size_t available = 0;
    auto result = m_consumer->beginRead(&buffer, &available);
    if (result == BytesConsumer::Result::ShouldWait)
      return;
    if (result == BytesConsumer::Result::Ok) {
      // We don't use |available| as-is to test cases where endRead
      // is called with a number smaller than |available|. We choose 3
      // because of the same reasons as Reader::onStateChange.
      size_t read = std::min(static_cast<size_t>(3), available);
      m_data.append(buffer, read);
      result = m_consumer->endRead(read);
    }
    DCHECK_NE(result, BytesConsumer::Result::ShouldWait);
    if (result != BytesConsumer::Result::Ok) {
      m_result = result;
      return;
    }
  }
}

std::pair<BytesConsumer::Result, Vector<char>>
BytesConsumerTestUtil::TwoPhaseReader::run() {
  onStateChange();
  while (m_result != BytesConsumer::Result::Done &&
         m_result != BytesConsumer::Result::Error)
    testing::runPendingTasks();
  testing::runPendingTasks();
  return std::make_pair(m_result, std::move(m_data));
}

}  // namespace blink
