// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/blob_cache/mock_blob_cache.h"
#include "blimp/net/blob_channel/blob_channel_sender_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;

namespace blimp {
namespace {

const char kBlobId[] =
    "\x20\x1e\x33\xb2\x2a\xa4\xf5\x5a\x98\xfc\x6b\x5b\x14\xc6\xab\x2b"
    "\x99\xbc\xcc\x1b\x1c\xa0\xa1\x8a\xf0\x45\x4a\x04\x7f\x6b\x06\x72";
const char kBlobPayload[] = "blob-1-payload";
const char kBlobId2[] =
    "\x90\x11\x3f\xcf\x1f\xc0\xef\x0a\x72\x11\xe0\x8d\xe4\x74\xd6\xdf"
    "\x04\x04\x8d\x61\xf0\xa1\xdf\xc2\xc6\xd3\x86\x9f\xfd\x92\x97\xf1";

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

TEST_F(BlobChannelSenderTest, PutBlob) {
  EXPECT_CALL(*mock_cache_,
              Put(kBlobId, BlobDataEqual(CreatePayload(kBlobPayload))));
  EXPECT_CALL(*mock_cache_, Contains(kBlobId)).WillOnce(Return(false));
  blob_sender_->PutBlob(kBlobId, CreatePayload(kBlobPayload));
}

TEST_F(BlobChannelSenderTest, PutBlobDuplicate) {
  EXPECT_CALL(*mock_cache_, Contains(kBlobId)).WillOnce(Return(true));
  blob_sender_->PutBlob(kBlobId, CreatePayload(kBlobPayload));
}

TEST_F(BlobChannelSenderTest, Push) {
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

TEST_F(BlobChannelSenderTest, GetCachedBlobIds) {
  std::string blob_id1 = CalculateBlobId(kBlobId);
  std::string blob_id2 = CalculateBlobId(kBlobId2);
  BlobDataPtr blob_payload1 = CreatePayload(kBlobPayload);

  EXPECT_CALL(*mock_cache_, Contains(blob_id1)).WillOnce(Return(true));
  EXPECT_CALL(*mock_cache_, Get(blob_id1)).WillOnce(Return(blob_payload1));

  std::vector<BlobId> cache_state;
  cache_state.push_back(blob_id1);
  cache_state.push_back(blob_id2);

  EXPECT_CALL(*mock_cache_, GetCachedBlobIds()).WillOnce(Return(cache_state));
  EXPECT_CALL(*mock_delegate_,
              DeliverBlob(blob_id1, BlobDataEqual(blob_payload1)));

  // Mark one of the blobs as delivered.
  blob_sender_->DeliverBlob(blob_id1);

  std::vector<BlobChannelSender::CacheStateEntry> actual =
      blob_sender_->GetCachedBlobIds();
  EXPECT_EQ(2u, actual.size());
  EXPECT_EQ(blob_id1, actual[0].id);
  EXPECT_TRUE(actual[0].was_delivered);
  EXPECT_EQ(blob_id2, actual[1].id);
  EXPECT_FALSE(actual[1].was_delivered);
}

}  // namespace
}  // namespace blimp
