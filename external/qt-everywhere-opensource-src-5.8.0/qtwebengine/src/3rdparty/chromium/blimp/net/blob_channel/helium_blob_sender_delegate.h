// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_HELIUM_BLOB_SENDER_DELEGATE_H_
#define BLIMP_NET_BLOB_CHANNEL_HELIUM_BLOB_SENDER_DELEGATE_H_

#include <string>
#include <vector>

#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"
#include "blimp/net/blob_channel/blob_channel_sender_impl.h"

namespace blimp {

// Sends blob messages as Helium messages to a BlimpMessageProcessor.
class BLIMP_NET_EXPORT HeliumBlobSenderDelegate
    : public BlobChannelSenderImpl::Delegate,
      public BlimpMessageProcessor {
 public:
  HeliumBlobSenderDelegate();
  ~HeliumBlobSenderDelegate() override;

  // Sets the message processor to which blob messages will be sent.
  void set_outgoing_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor) {
    outgoing_processor_ = std::move(processor);
  }

  // BlobChannelSenderImpl::Delegate implementation.
  void DeliverBlob(const BlobId& id, BlobDataPtr data) override;

 private:
  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  std::unique_ptr<BlimpMessageProcessor> outgoing_processor_;

  DISALLOW_COPY_AND_ASSIGN(HeliumBlobSenderDelegate);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_HELIUM_BLOB_SENDER_DELEGATE_H_
