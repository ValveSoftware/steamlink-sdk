// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blob_channel/helium_blob_receiver_delegate.h"

#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/blob_channel.pb.h"
#include "net/base/net_errors.h"

namespace blimp {

HeliumBlobReceiverDelegate::HeliumBlobReceiverDelegate() {}

HeliumBlobReceiverDelegate::~HeliumBlobReceiverDelegate() {}

void HeliumBlobReceiverDelegate::SetReceiver(BlobChannelReceiver* receiver) {
  DCHECK(receiver);
  receiver_ = receiver;
}

void HeliumBlobReceiverDelegate::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  if (!message->has_blob_channel()) {
    DLOG(WARNING) << "BlobChannel message has no |blob_channel| submessage.";
    callback.Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  // Take a mutable pointer to the blob_channel message so that we can re-use
  // its allocated buffers.
  BlobChannelMessage* blob_msg = message->mutable_blob_channel();
  if (blob_msg->type_case() != BlobChannelMessage::TypeCase::kTransferBlob) {
    callback.Run(net::ERR_NOT_IMPLEMENTED);
    return;
  }

  if (blob_msg->transfer_blob().blob_id().empty()) {
    callback.Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  // Create a temporarily non-const BlobData so that we may efficiently reuse
  // the allocated payload string via string::swap().
  scoped_refptr<BlobData> blob_data(new BlobData);
  blob_data->data.swap(*blob_msg->mutable_transfer_blob()->mutable_payload());
  receiver_->OnBlobReceived(blob_msg->transfer_blob().blob_id(), blob_data);

  callback.Run(net::OK);
}

}  // namespace blimp
