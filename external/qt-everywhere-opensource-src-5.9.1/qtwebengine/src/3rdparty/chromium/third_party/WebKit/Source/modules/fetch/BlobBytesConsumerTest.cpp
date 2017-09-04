// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BlobBytesConsumer.h"

#include "core/dom/Document.h"
#include "core/loader/ThreadableLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceResponse.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

using Command = DataConsumerHandleTestUtil::Command;
using PublicState = BytesConsumer::PublicState;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using Result = BytesConsumer::Result;

String toString(const Vector<char>& v) {
  return String(v.data(), v.size());
}

class TestThreadableLoader : public ThreadableLoader {
 public:
  ~TestThreadableLoader() override {
    EXPECT_FALSE(m_shouldBeCancelled && !m_isCancelled)
        << "The loader should be cancelled but is not cancelled.";
  }

  void start(const ResourceRequest& request) override { m_isStarted = true; }

  void overrideTimeout(unsigned long timeoutMilliseconds) override {
    ADD_FAILURE() << "overrideTimeout should not be called.";
  }

  void cancel() override { m_isCancelled = true; }

  bool isStarted() const { return m_isStarted; }
  bool isCancelled() const { return m_isCancelled; }
  void setShouldBeCancelled() { m_shouldBeCancelled = true; }

 private:
  bool m_isStarted = false;
  bool m_isCancelled = false;
  bool m_shouldBeCancelled = false;
};

class SyncLoadingTestThreadableLoader : public ThreadableLoader {
 public:
  ~SyncLoadingTestThreadableLoader() override { DCHECK(!m_handle); }

  void start(const ResourceRequest& request) override {
    m_isStarted = true;
    m_client->didReceiveResponse(0, ResourceResponse(), std::move(m_handle));
    m_client->didFinishLoading(0, 0);
  }

  void overrideTimeout(unsigned long timeoutMilliseconds) override {
    ADD_FAILURE() << "overrideTimeout should not be called.";
  }

  void cancel() override { m_isCancelled = true; }

  bool isStarted() const { return m_isStarted; }
  bool isCancelled() const { return m_isCancelled; }

  void setClient(ThreadableLoaderClient* client) { m_client = client; }

  void setHandle(std::unique_ptr<WebDataConsumerHandle> handle) {
    m_handle = std::move(handle);
  }

 private:
  bool m_isStarted = false;
  bool m_isCancelled = false;
  ThreadableLoaderClient* m_client = nullptr;
  std::unique_ptr<WebDataConsumerHandle> m_handle;
};

class SyncErrorTestThreadableLoader : public ThreadableLoader {
 public:
  ~SyncErrorTestThreadableLoader() override {}

  void start(const ResourceRequest& request) override {
    m_isStarted = true;
    m_client->didFail(ResourceError());
  }

  void overrideTimeout(unsigned long timeoutMilliseconds) override {
    ADD_FAILURE() << "overrideTimeout should not be called.";
  }

  void cancel() override { m_isCancelled = true; }

  bool isStarted() const { return m_isStarted; }
  bool isCancelled() const { return m_isCancelled; }

  void setClient(ThreadableLoaderClient* client) { m_client = client; }

 private:
  bool m_isStarted = false;
  bool m_isCancelled = false;
  ThreadableLoaderClient* m_client = nullptr;
};

class TestClient final : public GarbageCollectedFinalized<TestClient>,
                         public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(TestClient);

 public:
  void onStateChange() override { ++m_numOnStateChangeCalled; }
  int numOnStateChangeCalled() const { return m_numOnStateChangeCalled; }

 private:
  int m_numOnStateChangeCalled = 0;
};

class BlobBytesConsumerTest : public ::testing::Test {
 public:
  BlobBytesConsumerTest()
      : m_dummyPageHolder(DummyPageHolder::create(IntSize(1, 1))) {}

  Document& document() { return m_dummyPageHolder->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(BlobBytesConsumerTest, TwoPhaseRead) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Data, "world"));
  src->add(Command(Command::Done));

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_TRUE(loader->isStarted());
  EXPECT_FALSE(consumer->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize));
  EXPECT_FALSE(consumer->drainAsFormData());
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());

  consumer->didReceiveResponse(0, ResourceResponse(), std::move(src));
  consumer->didFinishLoading(0, 0);

  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(consumer))->run();
  EXPECT_EQ(Result::Done, result.first);
  EXPECT_EQ("hello, world", toString(result.second));
}

TEST_F(BlobBytesConsumerTest, FailLoading) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_TRUE(loader->isStarted());
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());

  int numOnStateChangeCalled = client->numOnStateChangeCalled();
  consumer->didFail(ResourceError());

  EXPECT_EQ(numOnStateChangeCalled + 1, client->numOnStateChangeCalled());
  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

TEST_F(BlobBytesConsumerTest, FailLoadingAfterResponseReceived) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_TRUE(loader->isStarted());
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());

  int numOnStateChangeCalled = client->numOnStateChangeCalled();
  consumer->didReceiveResponse(
      0, ResourceResponse(),
      DataConsumerHandleTestUtil::createWaitingDataConsumerHandle());
  EXPECT_EQ(numOnStateChangeCalled + 1, client->numOnStateChangeCalled());
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());

  consumer->didFail(ResourceError());
  EXPECT_EQ(numOnStateChangeCalled + 2, client->numOnStateChangeCalled());
  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

TEST_F(BlobBytesConsumerTest, FailAccessControlCheck) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_TRUE(loader->isStarted());
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());

  int numOnStateChangeCalled = client->numOnStateChangeCalled();
  consumer->didFailAccessControlCheck(ResourceError());
  EXPECT_EQ(numOnStateChangeCalled + 1, client->numOnStateChangeCalled());

  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

TEST_F(BlobBytesConsumerTest, CancelBeforeStarting) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);

  consumer->cancel();
  // This should be FALSE in production, but TRUE here because we set the
  // loader before starting loading in tests.
  EXPECT_TRUE(loader->isCancelled());

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());
  EXPECT_EQ(0, client->numOnStateChangeCalled());
}

TEST_F(BlobBytesConsumerTest, CancelAfterStarting) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_TRUE(loader->isStarted());
  EXPECT_EQ(0, client->numOnStateChangeCalled());

  consumer->cancel();
  EXPECT_TRUE(loader->isCancelled());
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(0, client->numOnStateChangeCalled());
}

TEST_F(BlobBytesConsumerTest, ReadLastChunkBeforeDidFinishLoadingArrives) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);
  std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
  src->add(Command(Command::Data, "hello"));
  src->add(Command(Command::Done));

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_TRUE(loader->isStarted());
  EXPECT_EQ(0, client->numOnStateChangeCalled());

  consumer->didReceiveResponse(0, ResourceResponse(), std::move(src));
  EXPECT_EQ(1, client->numOnStateChangeCalled());
  testing::runPendingTasks();
  EXPECT_EQ(2, client->numOnStateChangeCalled());

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(5u, available);
  EXPECT_EQ("hello", String(buffer, available));
  ASSERT_EQ(Result::Ok, consumer->endRead(available));

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  ASSERT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());

  consumer->didFinishLoading(0, 0);
  EXPECT_EQ(3, client->numOnStateChangeCalled());
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  ASSERT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
}

TEST_F(BlobBytesConsumerTest, ReadLastChunkAfterDidFinishLoadingArrives) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  TestClient* client = new TestClient();
  consumer->setClient(client);
  std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
  src->add(Command(Command::Data, "hello"));
  src->add(Command(Command::Done));

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_TRUE(loader->isStarted());
  EXPECT_EQ(0, client->numOnStateChangeCalled());

  consumer->didReceiveResponse(0, ResourceResponse(), std::move(src));
  EXPECT_EQ(1, client->numOnStateChangeCalled());
  testing::runPendingTasks();
  EXPECT_EQ(2, client->numOnStateChangeCalled());

  consumer->didFinishLoading(0, 0);
  testing::runPendingTasks();
  EXPECT_EQ(2, client->numOnStateChangeCalled());

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(5u, available);
  EXPECT_EQ("hello", String(buffer, available));
  ASSERT_EQ(Result::Ok, consumer->endRead(available));

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(2, client->numOnStateChangeCalled());
}

TEST_F(BlobBytesConsumerTest, DrainAsBlobDataHandle) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  RefPtr<BlobDataHandle> result = consumer->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize);
  ASSERT_TRUE(result);
  EXPECT_FALSE(consumer->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize));
  EXPECT_EQ(12345u, result->size());
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());
}

TEST_F(BlobBytesConsumerTest, DrainAsBlobDataHandle_2) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), -1);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  RefPtr<BlobDataHandle> result = consumer->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize);
  ASSERT_TRUE(result);
  EXPECT_FALSE(consumer->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize));
  EXPECT_EQ(UINT64_MAX, result->size());
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());
}

TEST_F(BlobBytesConsumerTest, DrainAsBlobDataHandle_3) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), -1);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  EXPECT_FALSE(consumer->drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize));
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());
}

TEST_F(BlobBytesConsumerTest, DrainAsFormData) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  TestThreadableLoader* loader = new TestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());

  RefPtr<EncodedFormData> result = consumer->drainAsFormData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->elements().size());
  ASSERT_EQ(FormDataElement::encodedBlob, result->elements()[0].m_type);
  ASSERT_TRUE(result->elements()[0].m_optionalBlobDataHandle);
  EXPECT_EQ(12345u, result->elements()[0].m_optionalBlobDataHandle->size());
  EXPECT_EQ(blobDataHandle->uuid(), result->elements()[0].m_blobUUID);
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_FALSE(loader->isStarted());
}

TEST_F(BlobBytesConsumerTest, LoaderShouldBeCancelled) {
  {
    RefPtr<BlobDataHandle> blobDataHandle =
        BlobDataHandle::create(BlobData::create(), 12345);
    TestThreadableLoader* loader = new TestThreadableLoader();
    BlobBytesConsumer* consumer = BlobBytesConsumer::createForTesting(
        &document(), blobDataHandle, loader);

    const char* buffer = nullptr;
    size_t available;
    EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
    EXPECT_TRUE(loader->isStarted());
    loader->setShouldBeCancelled();
  }
  ThreadState::current()->collectAllGarbage();
}

TEST_F(BlobBytesConsumerTest, SyncErrorDispatch) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  SyncErrorTestThreadableLoader* loader = new SyncErrorTestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  loader->setClient(consumer);
  TestClient* client = new TestClient();
  consumer->setClient(client);

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
  EXPECT_TRUE(loader->isStarted());

  EXPECT_EQ(0, client->numOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::Errored, consumer->getPublicState());
}

TEST_F(BlobBytesConsumerTest, SyncLoading) {
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(BlobData::create(), 12345);
  SyncLoadingTestThreadableLoader* loader =
      new SyncLoadingTestThreadableLoader();
  BlobBytesConsumer* consumer =
      BlobBytesConsumer::createForTesting(&document(), blobDataHandle, loader);
  std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
  src->add(Command(Command::Data, "hello, "));
  src->add(Command(Command::Wait));
  src->add(Command(Command::Data, "world"));
  src->add(Command(Command::Done));
  loader->setClient(consumer);
  loader->setHandle(std::move(src));
  TestClient* client = new TestClient();
  consumer->setClient(client);

  const char* buffer = nullptr;
  size_t available;
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  EXPECT_TRUE(loader->isStarted());
  ASSERT_EQ(7u, available);
  EXPECT_EQ("hello, ", String(buffer, available));

  EXPECT_EQ(0, client->numOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            consumer->getPublicState());
}

TEST_F(BlobBytesConsumerTest, ConstructedFromNullHandle) {
  BlobBytesConsumer* consumer = new BlobBytesConsumer(&document(), nullptr);
  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
}

}  // namespace

}  // namespace blink
