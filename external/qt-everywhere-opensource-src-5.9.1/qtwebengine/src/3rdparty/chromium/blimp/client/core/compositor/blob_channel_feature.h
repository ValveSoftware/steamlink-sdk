// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLOB_CHANNEL_FEATURE_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLOB_CHANNEL_FEATURE_H_

#include <memory>

#include "base/macros.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/core/compositor/decoding_image_generator.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {

class BlobChannelReceiver;

namespace client {

// BlobChannelFeature processes incoming BlobChannel message from the engine and
// holds the cache of Blob payloads.
class BlobChannelFeature : public BlimpMessageProcessor {
 public:
  explicit BlobChannelFeature(
      BlobImageSerializationProcessor::ErrorDelegate* delegate);
  ~BlobChannelFeature() override;

 protected:
  // The receiver that does the actual blob processing, protected for test.
  std::unique_ptr<BlobChannelReceiver> blob_receiver_;

 private:
  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // Receives blob BlimpMessages and relays blob data to BlobChannelReceiver.
  // Owned by BlobChannelReceiver, therefore stored as a raw pointer here.
  BlimpMessageProcessor* incoming_msg_processor_ = nullptr;

  // Retrieves and decodes image data from |blob_receiver_|. Must outlive
  // |blob_receiver_|.
  BlobImageSerializationProcessor blob_image_processor_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLOB_CHANNEL_FEATURE_H_
