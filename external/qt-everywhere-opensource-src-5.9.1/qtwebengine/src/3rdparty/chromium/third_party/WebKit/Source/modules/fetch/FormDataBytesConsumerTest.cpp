// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FormDataBytesConsumer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/Document.h"
#include "core/html/FormData.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace {

using Result = BytesConsumer::Result;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using MockBytesConsumer = BytesConsumerTestUtil::MockBytesConsumer;

String toString(const Vector<char>& v) {
  return String(v.data(), v.size());
}

PassRefPtr<EncodedFormData> complexFormData() {
  RefPtr<EncodedFormData> data = EncodedFormData::create();

  data->appendData("foo", 3);
  data->appendFileRange("/foo/bar/baz", 3, 4, 5);
  data->appendFileSystemURLRange(KURL(KURL(), "file:///foo/bar/baz"), 6, 7, 8);
  std::unique_ptr<BlobData> blobData = BlobData::create();
  blobData->appendText("hello", false);
  auto size = blobData->length();
  RefPtr<BlobDataHandle> blobDataHandle =
      BlobDataHandle::create(std::move(blobData), size);
  data->appendBlob(blobDataHandle->uuid(), blobDataHandle);
  Vector<char> boundary;
  boundary.append("\0", 1);
  data->setBoundary(boundary);
  return data.release();
}

class NoopClient final : public GarbageCollectedFinalized<NoopClient>,
                         public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(NoopClient);

 public:
  void onStateChange() override {}
};

class FormDataBytesConsumerTest : public ::testing::Test {
 public:
  FormDataBytesConsumerTest() : m_page(DummyPageHolder::create()) {}

 protected:
  Document* getDocument() { return &m_page->document(); }

  std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromString) {
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer("hello, world")))
                    ->run();
  EXPECT_EQ(Result::Done, result.first);
  EXPECT_EQ("hello, world", toString(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromStringNonLatin) {
  constexpr UChar cs[] = {0x3042, 0};
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer(String(cs))))
                    ->run();
  EXPECT_EQ(Result::Done, result.first);
  EXPECT_EQ("\xe3\x81\x82", toString(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromArrayBuffer) {
  constexpr unsigned char data[] = {0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3,
                                    0x42, 0x30, 0x42, 0x99, 0x88};
  DOMArrayBuffer* buffer = DOMArrayBuffer::create(data, WTF_ARRAY_LENGTH(data));
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer(buffer)))
                    ->run();
  Vector<char> expected;
  expected.append(data, WTF_ARRAY_LENGTH(data));

  EXPECT_EQ(Result::Done, result.first);
  EXPECT_EQ(expected, result.second);
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromArrayBufferView) {
  constexpr unsigned char data[] = {0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3,
                                    0x42, 0x30, 0x42, 0x99, 0x88};
  constexpr size_t offset = 1, size = 4;
  DOMArrayBuffer* buffer = DOMArrayBuffer::create(data, WTF_ARRAY_LENGTH(data));
  auto result =
      (new BytesConsumerTestUtil::TwoPhaseReader(new FormDataBytesConsumer(
           DOMUint8Array::create(buffer, offset, size))))
          ->run();
  Vector<char> expected;
  expected.append(data + offset, size);

  EXPECT_EQ(Result::Done, result.first);
  EXPECT_EQ(expected, result.second);
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromSimpleFormData) {
  RefPtr<EncodedFormData> data = EncodedFormData::create();
  data->appendData("foo", 3);
  data->appendData("hoge", 4);

  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer(getDocument(), data)))
                    ->run();
  EXPECT_EQ(Result::Done, result.first);
  EXPECT_EQ("foohoge", toString(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromComplexFormData) {
  RefPtr<EncodedFormData> data = complexFormData();
  MockBytesConsumer* underlying = MockBytesConsumer::create();
  BytesConsumer* consumer =
      FormDataBytesConsumer::createForTesting(getDocument(), data, underlying);
  Checkpoint checkpoint;

  const char* buffer = nullptr;
  size_t available = 0;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, beginRead(&buffer, &available))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*underlying, endRead(0)).WillOnce(Return(Result::Ok));
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  checkpoint.Call(2);
  EXPECT_EQ(Result::Ok, consumer->endRead(0));
  checkpoint.Call(3);
}

TEST_F(FormDataBytesConsumerTest, EndReadCanReturnDone) {
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(12u, available);
  EXPECT_EQ("hello, world", String(buffer, available));
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            consumer->getPublicState());
  EXPECT_EQ(Result::Done, consumer->endRead(available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromString) {
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  RefPtr<BlobDataHandle> blobDataHandle = consumer->drainAsBlobDataHandle();
  ASSERT_TRUE(blobDataHandle);

  EXPECT_EQ(String(), blobDataHandle->type());
  EXPECT_EQ(12u, blobDataHandle->size());
  EXPECT_FALSE(consumer->drainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromArrayBuffer) {
  BytesConsumer* consumer =
      new FormDataBytesConsumer(DOMArrayBuffer::create("foo", 3));
  RefPtr<BlobDataHandle> blobDataHandle = consumer->drainAsBlobDataHandle();
  ASSERT_TRUE(blobDataHandle);

  EXPECT_EQ(String(), blobDataHandle->type());
  EXPECT_EQ(3u, blobDataHandle->size());
  EXPECT_FALSE(consumer->drainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromSimpleFormData) {
  FormData* data = FormData::create(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  RefPtr<EncodedFormData> inputFormData = data->encodeMultiPartFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(getDocument(), inputFormData);
  RefPtr<BlobDataHandle> blobDataHandle = consumer->drainAsBlobDataHandle();
  ASSERT_TRUE(blobDataHandle);

  EXPECT_EQ(String(), blobDataHandle->type());
  EXPECT_EQ(inputFormData->flattenToString().utf8().length(),
            blobDataHandle->size());
  EXPECT_FALSE(consumer->drainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromComplexFormData) {
  RefPtr<EncodedFormData> inputFormData = complexFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(getDocument(), inputFormData);
  RefPtr<BlobDataHandle> blobDataHandle = consumer->drainAsBlobDataHandle();
  ASSERT_TRUE(blobDataHandle);

  EXPECT_FALSE(consumer->drainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromString) {
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  RefPtr<EncodedFormData> formData = consumer->drainAsFormData();
  ASSERT_TRUE(formData);
  EXPECT_EQ("hello, world", formData->flattenToString());

  EXPECT_FALSE(consumer->drainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromArrayBuffer) {
  BytesConsumer* consumer =
      new FormDataBytesConsumer(DOMArrayBuffer::create("foo", 3));
  RefPtr<EncodedFormData> formData = consumer->drainAsFormData();
  ASSERT_TRUE(formData);
  EXPECT_TRUE(formData->isSafeToSendToAnotherThread());
  EXPECT_EQ("foo", formData->flattenToString());

  EXPECT_FALSE(consumer->drainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromSimpleFormData) {
  FormData* data = FormData::create(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  RefPtr<EncodedFormData> inputFormData = data->encodeMultiPartFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(getDocument(), inputFormData);
  EXPECT_EQ(inputFormData, consumer->drainAsFormData());
  EXPECT_FALSE(consumer->drainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromComplexFormData) {
  RefPtr<EncodedFormData> inputFormData = complexFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(getDocument(), inputFormData);
  EXPECT_EQ(inputFormData, consumer->drainAsFormData());
  EXPECT_FALSE(consumer->drainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, BeginReadAffectsDraining) {
  const char* buffer = nullptr;
  size_t available = 0;
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  EXPECT_EQ("hello, world", String(buffer, available));

  ASSERT_EQ(Result::Ok, consumer->endRead(0));
  EXPECT_FALSE(consumer->drainAsFormData());
  EXPECT_FALSE(consumer->drainAsBlobDataHandle());
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            consumer->getPublicState());
}

TEST_F(FormDataBytesConsumerTest, BeginReadAffectsDrainingWithComplexFormData) {
  MockBytesConsumer* underlying = MockBytesConsumer::create();
  BytesConsumer* consumer = FormDataBytesConsumer::createForTesting(
      getDocument(), complexFormData(), underlying);

  const char* buffer = nullptr;
  size_t available = 0;
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, beginRead(&buffer, &available))
      .WillOnce(Return(Result::Ok));
  EXPECT_CALL(*underlying, endRead(0)).WillOnce(Return(Result::Ok));
  EXPECT_CALL(checkpoint, Call(2));
  // drainAsFormData should not be called here.
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*underlying, drainAsBlobDataHandle(_));
  EXPECT_CALL(checkpoint, Call(4));
  // |consumer| delegates the getPublicState call to |underlying|.
  EXPECT_CALL(*underlying, getPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::ReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(5));

  checkpoint.Call(1);
  ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(Result::Ok, consumer->endRead(0));
  checkpoint.Call(2);
  EXPECT_FALSE(consumer->drainAsFormData());
  checkpoint.Call(3);
  EXPECT_FALSE(consumer->drainAsBlobDataHandle());
  checkpoint.Call(4);
  EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting,
            consumer->getPublicState());
  checkpoint.Call(5);
}

TEST_F(FormDataBytesConsumerTest, SetClientWithComplexFormData) {
  RefPtr<EncodedFormData> inputFormData = complexFormData();

  MockBytesConsumer* underlying = MockBytesConsumer::create();
  BytesConsumer* consumer = FormDataBytesConsumer::createForTesting(
      getDocument(), inputFormData, underlying);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, setClient(_));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*underlying, clearClient());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  consumer->setClient(new NoopClient());
  checkpoint.Call(2);
  consumer->clearClient();
  checkpoint.Call(3);
}

TEST_F(FormDataBytesConsumerTest, CancelWithComplexFormData) {
  RefPtr<EncodedFormData> inputFormData = complexFormData();

  MockBytesConsumer* underlying = MockBytesConsumer::create();
  BytesConsumer* consumer = FormDataBytesConsumer::createForTesting(
      getDocument(), inputFormData, underlying);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, cancel());
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(1);
  consumer->cancel();
  checkpoint.Call(2);
}

}  // namespace
}  // namespace blink
