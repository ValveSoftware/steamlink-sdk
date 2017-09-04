// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_SENDER_H_
#define BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_SENDER_H_

#include <vector>

#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

// Blob size upper limit, for abuse prevention.
const size_t kMaxBlobSizeBytes = 10 * 1024 * 1024;

class BLIMP_NET_EXPORT BlobChannelSender {
 public:
  struct CacheStateEntry {
    BlobId id;
    bool was_delivered;
  };

  virtual ~BlobChannelSender() {}

  // Gets the list of cache keys and their replication status in the BlobCache.
  virtual std::vector<CacheStateEntry> GetCachedBlobIds() const = 0;

  // Puts a blob in the local BlobChannel. The blob can then be pushed to the
  // remote receiver via "DeliverBlob()".
  // Does nothing if there is already a blob |id| in the channel.
  virtual void PutBlob(const BlobId& id, BlobDataPtr data) = 0;

  // Sends the blob |id| to the remote side, if the remote side doesn't already
  // have the blob.
  virtual void DeliverBlob(const BlobId& id) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_SENDER_H_
