// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/mojo/blob_channel_service.h"

#include "blimp/net/blob_channel/blob_channel_sender.h"

namespace blimp {
namespace engine {

BlobChannelService::BlobChannelService(BlobChannelSender* blob_channel_sender,
                                       mojom::BlobChannelRequest request)
    : binding_(this, std::move(request)),
      blob_channel_sender_(blob_channel_sender) {
  DCHECK(blob_channel_sender_);
}

BlobChannelService::~BlobChannelService() {}

void BlobChannelService::PutBlob(const mojo::String& id,
                                 const mojo::String& data) {
  blob_channel_sender_->PutBlob(id, new BlobData(data));
}

void BlobChannelService::DeliverBlob(const mojo::String& id) {
  blob_channel_sender_->DeliverBlob(id);
}

// static
void BlobChannelService::Create(
    BlobChannelSender* blob_channel_sender,
    mojo::InterfaceRequest<mojom::BlobChannel> request) {
  // Object lifetime is managed by BlobChannelService's StrongBinding
  // |binding_|.
  new BlobChannelService(blob_channel_sender, std::move(request));
}

}  // namespace engine
}  // namespace blimp
