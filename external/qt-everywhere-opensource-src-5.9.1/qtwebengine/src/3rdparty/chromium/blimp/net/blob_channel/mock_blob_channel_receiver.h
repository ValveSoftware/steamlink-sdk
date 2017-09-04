// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_MOCK_BLOB_CHANNEL_RECEIVER_H_
#define BLIMP_NET_BLOB_CHANNEL_MOCK_BLOB_CHANNEL_RECEIVER_H_

#include <string>
#include <vector>

#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {

class MockBlobChannelReceiver : public BlobChannelReceiver {
 public:
  MockBlobChannelReceiver();
  ~MockBlobChannelReceiver();

  MOCK_METHOD1(Get, BlobDataPtr(const BlobId& id));
  MOCK_METHOD2(OnBlobReceived, void(const BlobId& id, BlobDataPtr data));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlobChannelReceiver);
};

class MockBlobChannelReceiverDelegate : public BlobChannelReceiver::Delegate {
 public:
  MockBlobChannelReceiverDelegate();
  ~MockBlobChannelReceiverDelegate() override;

  MOCK_METHOD1(SetReceiver, void(BlobChannelReceiver* receiver));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlobChannelReceiverDelegate);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_MOCK_BLOB_CHANNEL_RECEIVER_H_
