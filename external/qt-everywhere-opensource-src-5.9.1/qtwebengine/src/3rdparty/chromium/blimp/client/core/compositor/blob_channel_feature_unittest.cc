// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "blimp/client/core/compositor/blob_channel_feature.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {

class MockImageDecodeErrorDelegate
    : public BlobImageSerializationProcessor::ErrorDelegate {
  MOCK_METHOD0(OnImageDecodeError, void());
};

class MockBlobChannelFeature : public BlobChannelFeature {
 public:
  MockBlobChannelFeature() : BlobChannelFeature(&mock_delegate) {}
  ~MockBlobChannelFeature() override = default;

  BlobChannelReceiver* receiver() { return blob_receiver_.get(); }

 private:
  MockImageDecodeErrorDelegate mock_delegate;
};

class BlobManagerTest : public testing::Test {
 public:
  BlobManagerTest() = default;
  ~BlobManagerTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobManagerTest);
};

TEST_F(BlobManagerTest, CheckReceiver) {
  MockBlobChannelFeature blob_channel_feature;

  // BlobManager wrapper must have the receiver.
  DCHECK(blob_channel_feature.receiver());
}

}  // namespace client
}  // namespace blimp
