// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_BLOB_CHANNEL_SENDER_PROXY_H_
#define BLIMP_ENGINE_RENDERER_BLOB_CHANNEL_SENDER_PROXY_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "blimp/common/blimp_common_export.h"
#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"

namespace blimp {
namespace engine {

// Class for sending blobs (e.g. images) from the renderer to the browser
// process.
class BLIMP_COMMON_EXPORT BlobChannelSenderProxy : public BlobChannelSender {
 public:
  // Asynchronously request the set of cache keys from the browser process.
  // Knowledge of the browser's cache allows the renderer to avoid re-encoding
  // and re-transferring image assets that are already tracked by the
  // browser-side cache.
  //
  // If |this| is used in the brief time before the response arrives,
  // the object will continue to work, but false negatives will be returned
  // for IsInEngineCache() and IsInClientCache().
  BlobChannelSenderProxy();

  ~BlobChannelSenderProxy() override;

  static std::unique_ptr<BlobChannelSenderProxy> CreateForTest(
      mojom::BlobChannelPtr blob_channel);

  // Returns true if the blob is known to exist within the Engine cache.
  bool IsInEngineCache(const std::string& id) const;

  // Returns true if the blob has been delivered to the Client cache.
  bool IsInClientCache(const std::string& id) const;

  // BlobChannelSender implementation.
  std::vector<CacheStateEntry> GetCachedBlobIds() const override;
  void PutBlob(const BlobId& id, BlobDataPtr data) override;
  void DeliverBlob(const BlobId& id) override;

 private:
  void OnGetCacheStateComplete(
      const std::unordered_map<std::string, bool>& cache_state);

  // Testing constructor, used to supply a BlobChannel Mojo proxy directly.
  explicit BlobChannelSenderProxy(mojom::BlobChannelPtr blob_channel);

  // BlobChannel Mojo IPC stub, used for delivering blobs to the browser
  // process.
  mojom::BlobChannelPtr blob_channel_;

  // Local copy of the cache state for the local and remote BlobChannel.
  // Knowledge of the cache state enables callers to avoid unnecessary
  // image encoding and transferral if the content is already in the system.
  base::hash_map<std::string, bool> replication_state_;

  base::WeakPtrFactory<BlobChannelSenderProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelSenderProxy);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_BLOB_CHANNEL_SENDER_PROXY_H_
