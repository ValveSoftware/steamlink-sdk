// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumer.h"

#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {

namespace {

using Command = BytesConsumerTestUtil::Command;
using Result = BytesConsumer::Result;
using ReplayingBytesConsumer = BytesConsumerTestUtil::ReplayingBytesConsumer;

String toString(const Vector<char>& v) {
  return String(v.data(), v.size());
}

class TestClient final : public GarbageCollectedFinalized<TestClient>,
                         public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(TestClient);

 public:
  void onStateChange() override { ++m_numOnStateChangeCalled; }
  int numOnStateChangeCalled() const { return m_numOnStateChangeCalled; }

 private:
  int m_numOnStateChangeCalled = 0;
};

class BytesConsumerTeeTest : public ::testing::Test {
 public:
  BytesConsumerTeeTest() : m_page(DummyPageHolder::create()) {}

  Document* document() { return &m_page->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_page;
};

class FakeBlobBytesConsumer : public BytesConsumer {
 public:
  explicit FakeBlobBytesConsumer(PassRefPtr<BlobDataHandle> handle)
      : m_blobHandle(handle) {}
  ~FakeBlobBytesConsumer() override {}

  Result beginRead(const char** buffer, size_t* available) override {
    if (m_state == PublicState::Closed)
      return Result::Done;
    m_blobHandle = nullptr;
    m_state = PublicState::Errored;
    return Result::Error;
  }
  Result endRead(size_t readSize) override {
    if (m_state == PublicState::Closed)
      return Result::Error;
    m_blobHandle = nullptr;
    m_state = PublicState::Errored;
    return Result::Error;
  }
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy policy) {
    if (m_state != PublicState::ReadableOrWaiting)
      return nullptr;
    DCHECK(m_blobHandle);
    if (policy == BlobSizePolicy::DisallowBlobWithInvalidSize &&
        m_blobHandle->size() == UINT64_MAX)
      return nullptr;
    m_state = PublicState::Closed;
    return m_blobHandle.release();
  }

  void setClient(Client*) override {}
  void clearClient() override {}
  void cancel() override {}
  PublicState getPublicState() const override { return m_state; }
  Error getError() const override { return Error(); }
  String debugName() const override { return "FakeBlobBytesConsumer"; }

 private:
  PublicState m_state = PublicState::ReadableOrWaiting;
  RefPtr<BlobDataHandle> m_blobHandle;
};

TEST_F(BytesConsumerTeeTest, CreateDone) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());
  src->add(Command(Command::Done));
  EXPECT_FALSE(src->isCancelled());

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  auto result1 = (new BytesConsumerTestUtil::TwoPhaseReader(dest1))->run();
  auto result2 = (new BytesConsumerTestUtil::TwoPhaseReader(dest2))->run();

  EXPECT_EQ(Result::Done, result1.first);
  EXPECT_TRUE(result1.second.isEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(Result::Done, result2.first);
  EXPECT_TRUE(result2.second.isEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());

  // Cancelling does nothing when closed.
  dest1->cancel();
  dest2->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());
}

TEST_F(BytesConsumerTeeTest, TwoPhaseRead) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());

  src->add(Command(Command::Wait));
  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Data, "world"));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Done));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());

  auto result1 = (new BytesConsumerTestUtil::TwoPhaseReader(dest1))->run();
  auto result2 = (new BytesConsumerTestUtil::TwoPhaseReader(dest2))->run();

  EXPECT_EQ(Result::Done, result1.first);
  EXPECT_EQ("hello, world", toString(result1.second));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(Result::Done, result2.first);
  EXPECT_EQ("hello, world", toString(result2.second));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());
}

TEST_F(BytesConsumerTeeTest, Error) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());

  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Data, "world"));
  src->add(Command(Command::Error));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest2->getPublicState());

  auto result1 = (new BytesConsumerTestUtil::TwoPhaseReader(dest1))->run();
  auto result2 = (new BytesConsumerTestUtil::TwoPhaseReader(dest2))->run();

  EXPECT_EQ(Result::Error, result1.first);
  EXPECT_TRUE(result1.second.isEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest1->getPublicState());
  EXPECT_EQ(Result::Error, result2.first);
  EXPECT_TRUE(result2.second.isEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());

  // Cancelling does nothing when errored.
  dest1->cancel();
  dest2->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());
}

TEST_F(BytesConsumerTeeTest, Cancel) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());

  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Wait));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());

  EXPECT_FALSE(src->isCancelled());
  dest1->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());
  dest2->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest2->getPublicState());
  EXPECT_TRUE(src->isCancelled());
}

TEST_F(BytesConsumerTeeTest, CancelShouldNotAffectTheOtherDestination) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());

  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Data, "world"));
  src->add(Command(Command::Done));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());

  EXPECT_FALSE(src->isCancelled());
  dest1->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());

  auto result2 = (new BytesConsumerTestUtil::TwoPhaseReader(dest2))->run();

  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest2->getPublicState());
  EXPECT_EQ(Result::Done, result2.first);
  EXPECT_EQ("hello, world", toString(result2.second));
  EXPECT_FALSE(src->isCancelled());
}

TEST_F(BytesConsumerTeeTest, CancelShouldNotAffectTheOtherDestination2) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());

  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Data, "world"));
  src->add(Command(Command::Error));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());

  EXPECT_FALSE(src->isCancelled());
  dest1->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest2->getPublicState());
  EXPECT_FALSE(src->isCancelled());

  auto result2 = (new BytesConsumerTestUtil::TwoPhaseReader(dest2))->run();

  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest2->getPublicState());
  EXPECT_EQ(Result::Error, result2.first);
  EXPECT_FALSE(src->isCancelled());
}

TEST_F(BytesConsumerTeeTest, BlobHandle) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  BytesConsumer* src = new FakeBlobBytesConsumer(blobDataHandle);

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  RefPtr<BlobDataHandle> destBlobDataHandle1 = dest1->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize);
  RefPtr<BlobDataHandle> destBlobDataHandle2 = dest2->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize);
  ASSERT_TRUE(destBlobDataHandle1);
  ASSERT_TRUE(destBlobDataHandle2);
  EXPECT_EQ(12345u, destBlobDataHandle1->size());
  EXPECT_EQ(12345u, destBlobDataHandle2->size());
}

TEST_F(BytesConsumerTeeTest, BlobHandleWithInvalidSize) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), -1);
  BytesConsumer* src = new FakeBlobBytesConsumer(blobDataHandle);

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  RefPtr<BlobDataHandle> destBlobDataHandle1 = dest1->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize);
  RefPtr<BlobDataHandle> destBlobDataHandle2 = dest2->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize);
  ASSERT_TRUE(destBlobDataHandle1);
  ASSERT_FALSE(destBlobDataHandle2);
  EXPECT_EQ(UINT64_MAX, destBlobDataHandle1->size());
}

TEST_F(BytesConsumerTeeTest, ConsumerCanBeErroredInTwoPhaseRead) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());
  src->add(Command(Command::Data, "a"));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Error));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);
  TestClient* client = new TestClient();
  dest1->setClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::Ok, dest1->beginRead(&buffer, &available));
  ASSERT_EQ(1u, available);

  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());
  int numOnStateChangeCalled = client->numOnStateChangeCalled();
  EXPECT_EQ(Result::Error,
            (new BytesConsumerTestUtil::TwoPhaseReader(dest2))->run().first);
  EXPECT_EQ(BytesConsumer::PublicState::Errored, dest1->getPublicState());
  EXPECT_EQ(numOnStateChangeCalled + 1, client->numOnStateChangeCalled());
  EXPECT_EQ('a', buffer[0]);
  EXPECT_EQ(Result::Ok, dest1->endRead(available));
}

TEST_F(BytesConsumerTeeTest,
       AsyncNotificationShouldBeDispatchedWhenAllDataIsConsumed) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());
  src->add(Command(Command::Data, "a"));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Done));
  TestClient* client = new TestClient();

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  dest1->setClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::Ok, dest1->beginRead(&buffer, &available));
  ASSERT_EQ(1u, available);
  EXPECT_EQ('a', buffer[0]);

  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            src->getPublicState());
  testing::runPendingTasks();
  EXPECT_EQ(BytesConsumer::PublicState::Closed, src->getPublicState());
  // Just for checking UAF.
  EXPECT_EQ('a', buffer[0]);
  ASSERT_EQ(Result::Ok, dest1->endRead(1));

  EXPECT_EQ(0, client->numOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());
  testing::runPendingTasks();
  EXPECT_EQ(1, client->numOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
}

TEST_F(BytesConsumerTeeTest,
       AsyncCloseNotificationShouldBeCancelledBySubsequentReadCall) {
  ReplayingBytesConsumer* src = new ReplayingBytesConsumer(document());
  src->add(Command(Command::Data, "a"));
  src->add(Command(Command::Done));
  TestClient* client = new TestClient();

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::tee(document(), src, &dest1, &dest2);

  dest1->setClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::Ok, dest1->beginRead(&buffer, &available));
  ASSERT_EQ(1u, available);
  EXPECT_EQ('a', buffer[0]);

  testing::runPendingTasks();
  // Just for checking UAF.
  EXPECT_EQ('a', buffer[0]);
  ASSERT_EQ(Result::Ok, dest1->endRead(1));
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            dest1->getPublicState());

  EXPECT_EQ(Result::Done, dest1->beginRead(&buffer, &available));
  EXPECT_EQ(0, client->numOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
  testing::runPendingTasks();
  EXPECT_EQ(0, client->numOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::Closed, dest1->getPublicState());
}

TEST(BytesConusmerTest, ClosedBytesConsumer) {
  BytesConsumer* consumer = BytesConsumer::createClosed();

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST(BytesConusmerTest, ErroredBytesConsumer) {
  BytesConsumer::Error error("hello");
  BytesConsumer* consumer = BytesConsumer::createErrored(error);

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(error.message(), consumer->getError().message());

  consumer->cancel();
  EXPECT_EQ(BytesConsumer::PublicState::Errored, consumer->getPublicState());
}

}  // namespace

}  // namespace blink
