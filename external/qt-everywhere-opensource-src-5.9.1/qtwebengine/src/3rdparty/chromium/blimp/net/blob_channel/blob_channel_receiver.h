// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_RECEIVER_H_
#define BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_RECEIVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlobCache;

class BLIMP_NET_EXPORT BlobChannelReceiver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Sets the Receiver object which will take incoming blobs.
    virtual void SetReceiver(BlobChannelReceiver* receiver) = 0;
  };

  virtual ~BlobChannelReceiver() {}

  // Constructs a BlobChannelReceiverImpl object for use.
  static std::unique_ptr<BlobChannelReceiver> Create(
      std::unique_ptr<BlobCache> cache,
      std::unique_ptr<Delegate> delegate);

  // Gets a blob from the BlobChannel.
  // Returns nullptr if the blob is not available in the channel.
  // Can be accessed concurrently from any thread. Calling code must ensure that
  // the object instance outlives all calls to Get().
  virtual BlobDataPtr Get(const BlobId& id) = 0;

  // Called by Delegate::OnBlobReceived() when a blob arrives over the channel.
  virtual void OnBlobReceived(const BlobId& id, BlobDataPtr data) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_RECEIVER_H_
