// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blob_channel_feature.h"

#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "blimp/net/blob_channel/helium_blob_receiver_delegate.h"

namespace blimp {
namespace client {

BlobChannelFeature::BlobChannelFeature(
    BlobImageSerializationProcessor::ErrorDelegate* delegate) {
  std::unique_ptr<HeliumBlobReceiverDelegate> blob_delegate =
      base::MakeUnique<HeliumBlobReceiverDelegate>();

  // Set incoming blob message processor.
  incoming_msg_processor_ = blob_delegate.get();

  // Set BlobChannelReceiver that does the actual blob processing.
  blob_receiver_ = BlobChannelReceiver::Create(
      base::MakeUnique<InMemoryBlobCache>(), std::move(blob_delegate));
  blob_image_processor_.set_blob_receiver(blob_receiver_.get());

  // Set image decode error delegate.
  DCHECK(delegate);
  blob_image_processor_.set_error_delegate(delegate);
}

BlobChannelFeature::~BlobChannelFeature() = default;

void BlobChannelFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(incoming_msg_processor_);

  // Forward the message to |incoming_msg_processor_|.
  incoming_msg_processor_->ProcessMessage(std::move(message), callback);
}

}  // namespace client
}  // namespace blimp
