// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "blimp/common/blob_cache/mock_blob_cache.h"
#include "blimp/net/blob_channel/blob_channel_sender_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

using testing::_;
using testing::Pointee;
using testing::Return;

const char kBlobId[] = "blob-1";
const char kBlobPayload[] = "blob-1-payload";

// Helper function for creating a cache payload vector from a string.
BlobDataPtr CreatePayload(const std::string& input) {
  BlobDataPtr output(new BlobData(input));
  return output;
}

MATCHER_P(BlobDataEqual, expected, "") {
  return expected->data == arg->data;
}

class MockSenderDelegate : public BlobChannelSenderImpl::Delegate {
 public:
  MockSenderDelegate() {}
  ~MockSenderDelegate() override {}

  MOCK_METHOD2(DeliverBlob, void(const BlobId&, BlobDataPtr));
};

class BlobChannelSenderTest : public testing::Test {
 public:
  BlobChannelSenderTest()
      : mock_delegate_(new testing::StrictMock<MockSenderDelegate>),
        mock_cache_(new testing::StrictMock<MockBlobCache>),
        blob_sender_(
            new BlobChannelSenderImpl(base::WrapUnique(mock_cache_),
                                      base::WrapUnique(mock_delegate_))) {}
  ~BlobChannelSenderTest() override {}

  testing::StrictMock<MockSenderDelegate>* mock_delegate_;
  testing::StrictMock<MockBlobCache>* mock_cache_;
  std::unique_ptr<BlobChannelSender> blob_sender_;
};

TEST_F(BlobChannelSenderTest, TestPutBlob) {
  EXPECT_CALL(*mock_cache_,
              Put(kBlobId, BlobDataEqual(CreatePayload(kBlobPayload))));
  EXPECT_CALL(*mock_cache_, Contains(kBlobId)).WillOnce(Return(false));
  blob_sender_->PutBlob(kBlobId, CreatePayload(kBlobPayload));
}

TEST_F(BlobChannelSenderTest, TestPutBlobDuplicate) {
  EXPECT_CALL(*mock_cache_, Contains(kBlobId)).WillOnce(Return(true));
  blob_sender_->PutBlob(kBlobId, CreatePayload(kBlobPayload));
}

TEST_F(BlobChannelSenderTest, TestPush) {
  auto payload = CreatePayload(kBlobPayload);
  EXPECT_CALL(*mock_delegate_, DeliverBlob(kBlobId, BlobDataEqual(payload)));
  EXPECT_CALL(*mock_cache_, Contains(kBlobId))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_cache_, Put(kBlobId, BlobDataEqual(payload)));
  EXPECT_CALL(*mock_cache_, Get(kBlobId)).WillOnce(Return(payload));
  blob_sender_->PutBlob(kBlobId, payload);
  blob_sender_->DeliverBlob(kBlobId);
}

}  // namespace
}  // namespace blimp
