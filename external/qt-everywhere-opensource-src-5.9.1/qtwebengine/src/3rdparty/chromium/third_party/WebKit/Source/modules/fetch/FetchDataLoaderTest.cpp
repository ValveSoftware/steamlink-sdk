// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataLoader.h"

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::StrictMock;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using MockFetchDataLoaderClient =
    BytesConsumerTestUtil::MockFetchDataLoaderClient;
using MockBytesConsumer = BytesConsumerTestUtil::MockBytesConsumer;
using Result = BytesConsumer::Result;

constexpr char kQuickBrownFox[] = "Quick brown fox";
constexpr size_t kQuickBrownFoxLength = 15;
constexpr size_t kQuickBrownFoxLengthWithTerminatingNull = 16;

TEST(FetchDataLoaderTest, LoadAsBlob) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();
  RefPtr<BlobDataHandle> blobDataHandle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              drainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Done));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blobDataHandle));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);

  ASSERT_TRUE(blobDataHandle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blobDataHandle->size());
  EXPECT_EQ(String("text/test"), blobDataHandle->type());
}

TEST(FetchDataLoaderTest, LoadAsBlobFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              drainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Error));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsBlobCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              drainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  fetchDataLoader->cancel();
  checkpoint.Call(3);
}

TEST(FetchDataLoaderTest,
     LoadAsBlobViaDrainAsBlobDataHandleWithSameContentType) {
  std::unique_ptr<BlobData> blobData = BlobData::create();
  blobData->appendBytes(kQuickBrownFox,
                        kQuickBrownFoxLengthWithTerminatingNull);
  blobData->setContentType("text/test");
  RefPtr<BlobDataHandle> inputBlobDataHandle = BlobDataHandle::create(
      std::move(blobData), kQuickBrownFoxLengthWithTerminatingNull);

  Checkpoint checkpoint;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();
  RefPtr<BlobDataHandle> blobDataHandle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              drainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(inputBlobDataHandle)));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blobDataHandle));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  fetchDataLoader->cancel();
  checkpoint.Call(3);

  ASSERT_TRUE(blobDataHandle);
  EXPECT_EQ(inputBlobDataHandle, blobDataHandle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blobDataHandle->size());
  EXPECT_EQ(String("text/test"), blobDataHandle->type());
}

TEST(FetchDataLoaderTest,
     LoadAsBlobViaDrainAsBlobDataHandleWithDifferentContentType) {
  std::unique_ptr<BlobData> blobData = BlobData::create();
  blobData->appendBytes(kQuickBrownFox,
                        kQuickBrownFoxLengthWithTerminatingNull);
  blobData->setContentType("text/different");
  RefPtr<BlobDataHandle> inputBlobDataHandle = BlobDataHandle::create(
      std::move(blobData), kQuickBrownFoxLengthWithTerminatingNull);

  Checkpoint checkpoint;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();
  RefPtr<BlobDataHandle> blobDataHandle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              drainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(inputBlobDataHandle)));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blobDataHandle));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  fetchDataLoader->cancel();
  checkpoint.Call(3);

  ASSERT_TRUE(blobDataHandle);
  EXPECT_NE(inputBlobDataHandle, blobDataHandle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blobDataHandle->size());
  EXPECT_EQ(String("text/test"), blobDataHandle->type());
}

TEST(FetchDataLoaderTest, LoadAsArrayBuffer) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsArrayBuffer();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();
  DOMArrayBuffer* arrayBuffer = nullptr;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Done));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedArrayBufferMock(_))
      .WillOnce(SaveArg<0>(&arrayBuffer));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);

  ASSERT_TRUE(arrayBuffer);
  ASSERT_EQ(kQuickBrownFoxLengthWithTerminatingNull, arrayBuffer->byteLength());
  EXPECT_STREQ(kQuickBrownFox, static_cast<const char*>(arrayBuffer->data()));
}

TEST(FetchDataLoaderTest, LoadAsArrayBufferFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsArrayBuffer();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Error));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsArrayBufferCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader =
      FetchDataLoader::createLoaderAsArrayBuffer();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  fetchDataLoader->cancel();
  checkpoint.Call(3);
}

TEST(FetchDataLoaderTest, LoadAsString) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLength),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(kQuickBrownFoxLength))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Done));
  EXPECT_CALL(*fetchDataLoaderClient,
              didFetchDataLoadedString(String(kQuickBrownFox)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringWithNullBytes) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  constexpr char kPattern[] = "Quick\0brown\0fox";
  constexpr size_t kLength = sizeof(kPattern);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kPattern), SetArgPointee<1>(kLength),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(16)).WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Done));
  EXPECT_CALL(*fetchDataLoaderClient,
              didFetchDataLoadedString(String(kPattern, kLength)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringError) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLength),
                      Return(Result::Ok)));
  EXPECT_CALL(*consumer, endRead(kQuickBrownFoxLength))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*consumer, beginRead(_, _)).WillOnce(Return(Result::Error));
  EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->onStateChange();
  checkpoint.Call(3);
  fetchDataLoader->cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::create();

  FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
  MockFetchDataLoaderClient* fetchDataLoaderClient =
      MockFetchDataLoaderClient::create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, setClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, beginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::ShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetchDataLoader->start(consumer, fetchDataLoaderClient);
  checkpoint.Call(2);
  fetchDataLoader->cancel();
  checkpoint.Call(3);
}

}  // namespace

}  // namespace blink
