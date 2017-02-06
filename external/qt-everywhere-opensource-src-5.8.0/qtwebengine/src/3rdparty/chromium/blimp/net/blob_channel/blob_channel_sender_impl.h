// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_SENDER_IMPL_H_
#define BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_SENDER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"

namespace blimp {

// Sends blobs to a remote receiver.
// The transport-specific details are provided by the caller using the Delegate
// subclass.
class BLIMP_NET_EXPORT BlobChannelSenderImpl : public BlobChannelSender {
 public:
  // Delegate interface for transport-specific implementations of blob transfer
  // commands.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Send blob |id| with payload |data| to the receiver.
    virtual void DeliverBlob(const BlobId& id, BlobDataPtr data) = 0;
  };

  BlobChannelSenderImpl(std::unique_ptr<BlobCache> cache,
                        std::unique_ptr<Delegate> delegate);
  ~BlobChannelSenderImpl() override;

  // BlobChannelSender implementation.
  void PutBlob(const BlobId& id, BlobDataPtr data) override;
  void DeliverBlob(const BlobId& id) override;

 private:
  std::unique_ptr<BlobCache> cache_;
  std::unique_ptr<Delegate> delegate_;

  // Used to track the item IDs in the receiver's cache. This may differ from
  // the set of IDs in |cache_|, for instance if an ID hasn't yet been
  // delivered, or has been evicted at the receiver.
  std::set<BlobId> receiver_cache_contents_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelSenderImpl);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_SENDER_IMPL_H_
