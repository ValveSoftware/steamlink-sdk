// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_MOJO_BLOB_CHANNEL_SERVICE_H_
#define BLIMP_ENGINE_MOJO_BLOB_CHANNEL_SERVICE_H_

#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blimp {

class BlobChannelSender;

namespace engine {

// Service for processing BlobChannel requests from the renderer.
// Runs on the browser process.
class BlobChannelService : public mojom::BlobChannel {
 public:
  ~BlobChannelService() override;

  // Factory method called by Mojo.
  static void Create(BlobChannelSender* blob_channel_sender,
                     mojo::InterfaceRequest<mojom::BlobChannel> request);

 private:
  // Creates a BlobChannel bound to the connection specified by |request|.
  // |blob_channel_sender| must outlive the Mojo connection.
  BlobChannelService(BlobChannelSender* blob_channel_sender,
                     mojom::BlobChannelRequest request);

  // BlobChannel implementation.
  void PutBlob(const mojo::String& id, const mojo::String& data) override;
  void DeliverBlob(const mojo::String& id) override;

  // Binds |this| and its object lifetime to a Mojo connection.
  mojo::StrongBinding<mojom::BlobChannel> binding_;

  // Sender object which will receive the blobs passed over the Mojo service.
  BlobChannelSender* blob_channel_sender_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelService);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_MOJO_BLOB_CHANNEL_SERVICE_H_
