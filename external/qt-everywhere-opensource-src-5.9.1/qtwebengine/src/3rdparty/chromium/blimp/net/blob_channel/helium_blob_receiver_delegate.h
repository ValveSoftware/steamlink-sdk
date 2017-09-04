// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_HELIUM_BLOB_RECEIVER_DELEGATE_H_
#define BLIMP_NET_BLOB_CHANNEL_HELIUM_BLOB_RECEIVER_DELEGATE_H_

#include <memory>

#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "net/base/completion_callback.h"

namespace blimp {

// Receives and processes incoming blob messages in BlimpMessage format and
// passes them to an attached BlobChannelReceiver.
// The caller must provide the receiver object via SetReceiver() before any
// messages can be processed.
class BLIMP_NET_EXPORT HeliumBlobReceiverDelegate
    : public BlobChannelReceiver::Delegate,
      public BlimpMessageProcessor {
 public:
  HeliumBlobReceiverDelegate();
  ~HeliumBlobReceiverDelegate() override;

  // Sets the Receiver object which will take incoming blobs.
  void SetReceiver(BlobChannelReceiver* receiver) override;

 private:
  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  BlobChannelReceiver* receiver_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HeliumBlobReceiverDelegate);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_HELIUM_BLOB_RECEIVER_DELEGATE_H_
