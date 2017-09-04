// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_MOCK_BLOB_CHANNEL_SENDER_H_
#define BLIMP_NET_BLOB_CHANNEL_MOCK_BLOB_CHANNEL_SENDER_H_

#include <string>
#include <vector>

#include "blimp/net/blob_channel/blob_channel_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {

class BLIMP_NET_EXPORT MockBlobChannelSender : public BlobChannelSender {
 public:
  MockBlobChannelSender();
  ~MockBlobChannelSender() override;

  MOCK_METHOD2(PutBlob, void(const BlobId& id, BlobDataPtr data));
  MOCK_METHOD1(DeliverBlob, void(const BlobId& id));
  MOCK_CONST_METHOD0(GetCachedBlobIds, std::vector<CacheStateEntry>());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlobChannelSender);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_MOCK_BLOB_CHANNEL_SENDER_H_
