// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "blimp/common/blob_cache/mock_blob_cache.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "blimp/net/blob_channel/mock_blob_channel_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

using testing::_;
using testing::Return;
using testing::SaveArg;

const char kBlobId[] = "blob-1";
const char kBlobPayload[] = "blob-1-payload";

// Helper function for creating a cache payload vector from a string.
BlobDataPtr CreatePayload(const std::string& input) {
  return BlobDataPtr(new BlobData(input));
}

MATCHER_P(BlobDataEqual, expected, "") {
  return expected->data == arg->data;
}

class BlobChannelReceiverTest : public testing::Test {
 public:
  BlobChannelReceiverTest() : cache_(new testing::StrictMock<MockBlobCache>) {
    BlobChannelReceiver* stored_receiver;
    std::unique_ptr<MockBlobChannelReceiverDelegate> receiver_delegate(
        new MockBlobChannelReceiverDelegate);
    receiver_delegate_ = receiver_delegate.get();

    EXPECT_CALL(*receiver_delegate, SetReceiver(_))
        .WillOnce(SaveArg<0>(&stored_receiver));

    blob_receiver_ = BlobChannelReceiver::Create(base::WrapUnique(cache_),
                                                 std::move(receiver_delegate));
  }

  ~BlobChannelReceiverTest() override {}

  testing::StrictMock<MockBlobCache>* cache_;
  std::unique_ptr<BlobChannelReceiver> blob_receiver_;
  MockBlobChannelReceiverDelegate* receiver_delegate_;
};

TEST_F(BlobChannelReceiverTest, GetKnownBlob) {
  auto payload = CreatePayload(kBlobPayload);
  EXPECT_CALL(*cache_, Get(kBlobId)).WillOnce(Return(payload));

  auto result = blob_receiver_->Get(kBlobId);

  ASSERT_NE(nullptr, result.get());
  EXPECT_EQ(result->data, payload->data);
}

TEST_F(BlobChannelReceiverTest, GetFromDelegateReceiveMethod) {
  auto payload = CreatePayload(kBlobPayload);

  EXPECT_CALL(*cache_, Put(kBlobId, BlobDataEqual(payload)));
  EXPECT_CALL(*cache_, Get(kBlobId)).WillOnce(Return(payload));

  blob_receiver_->OnBlobReceived(kBlobId, payload);

  auto result = blob_receiver_->Get(kBlobId);

  ASSERT_NE(nullptr, result.get());
  EXPECT_EQ(payload->data, result->data);
}

TEST_F(BlobChannelReceiverTest, GetUnknownBlob) {
  EXPECT_CALL(*cache_, Get(kBlobId)).WillOnce(Return(nullptr));
  auto result = blob_receiver_->Get(kBlobId);

  ASSERT_EQ(nullptr, result.get());
}

}  // namespace
}  // namespace blimp
