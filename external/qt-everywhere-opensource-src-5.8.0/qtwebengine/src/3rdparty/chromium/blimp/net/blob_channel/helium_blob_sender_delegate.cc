// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blob_channel/helium_blob_sender_delegate.h"

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/blob_channel.pb.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace {

void DoNothingCompletionCallback(int) {}

}  // namespace

HeliumBlobSenderDelegate::HeliumBlobSenderDelegate() {}

HeliumBlobSenderDelegate::~HeliumBlobSenderDelegate() {}

void HeliumBlobSenderDelegate::DeliverBlob(const BlobId& id, BlobDataPtr data) {
  BlobChannelMessage* blob_message;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&blob_message);
  blob_message->mutable_transfer_blob()->set_payload(&data->data[0],
                                                     data->data.size());
  blob_message->mutable_transfer_blob()->set_blob_id(id);
  outgoing_processor_->ProcessMessage(std::move(message),
                                      base::Bind(&DoNothingCompletionCallback));
}

void HeliumBlobSenderDelegate::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(net::ERR_NOT_IMPLEMENTED);
}

}  // namespace blimp
