// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_BLOB_CHANNEL_SENDER_PROXY_H_
#define BLIMP_ENGINE_RENDERER_BLOB_CHANNEL_SENDER_PROXY_H_

#include <memory>
#include <string>

#include "base/containers/hash_tables.h"
#include "blimp/common/blimp_common_export.h"
#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"

namespace blimp {
namespace engine {

// Class for sending blobs (e.g. images) from the renderer to the browser
// process.
class BLIMP_COMMON_EXPORT BlobChannelSenderProxy : public BlobChannelSender {
 public:
  BlobChannelSenderProxy();
  ~BlobChannelSenderProxy() override;

  // Returns true if the blob is known to exist within the Engine cache.
  bool IsInEngineCache(const std::string& id) const;

  // Returns true if the blob has been delivered to the Client cache.
  bool IsInClientCache(const std::string& id) const;

  // BlobChannelSender implementation.
  void PutBlob(const BlobId& id, BlobDataPtr data) override;
  void DeliverBlob(const BlobId& id) override;

 private:
  // BlobChannel Mojo IPC stub, used for delivering blobs to the browser
  // process.
  mojom::BlobChannelPtr blob_channel_;

  // Local copy of the cache state for the local and remote BlobChannel.
  // Knowledge of the cache state enables callers to avoid unnecessary
  // image encoding and transferral if the content is already in the system.
  base::hash_map<std::string, bool> replication_state_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelSenderProxy);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_BLOB_CHANNEL_SENDER_PROXY_H_
