// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blob_channel_sender_proxy.h"

#include "content/public/renderer/render_thread.h"
#include "services/shell/public/cpp/interface_provider.h"

namespace blimp {
namespace engine {
namespace {

mojom::BlobChannelPtr GetConnectedBlobChannel() {
  mojom::BlobChannelPtr blob_channel_ptr;
  content::RenderThread::Get()->GetRemoteInterfaces()->GetInterface(
      &blob_channel_ptr);
  CHECK(blob_channel_ptr) << "Could not connect to BlobChannel Mojo interface.";
  return blob_channel_ptr;
}

}  // namespace

BlobChannelSenderProxy::BlobChannelSenderProxy()
    : blob_channel_(GetConnectedBlobChannel()) {}

BlobChannelSenderProxy::~BlobChannelSenderProxy() {}

bool BlobChannelSenderProxy::IsInEngineCache(const std::string& id) const {
  return replication_state_.find(id) != replication_state_.end();
}

bool BlobChannelSenderProxy::IsInClientCache(const std::string& id) const {
  return replication_state_.find(id)->second;
}

void BlobChannelSenderProxy::PutBlob(const BlobId& id, BlobDataPtr data) {
  DCHECK(!IsInEngineCache(id));

  replication_state_[id] = false;
  blob_channel_->PutBlob(id, data->data);
}

void BlobChannelSenderProxy::DeliverBlob(const std::string& id) {
  DCHECK(!IsInClientCache(id));

  // We assume that the client will have the blob if we push it.
  // TODO(kmarshall): Revisit this assumption when asynchronous blob transport
  // is supported.
  replication_state_[id] = true;

  blob_channel_->DeliverBlob(id);
}

}  // namespace engine
}  // namespace blimp
