// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_REMOTE_PROTO_CHANNEL_H_
#define CC_TREES_REMOTE_PROTO_CHANNEL_H_

#include <memory>

#include "cc/base/cc_export.h"

namespace cc {

namespace proto {
class CompositorMessage;
}

// Provides a bridge for getting compositor protobuf messages to/from the
// outside world.
class CC_EXPORT RemoteProtoChannel {
 public:
  // Meant to be implemented by a RemoteChannel that needs to receive and parse
  // incoming protobufs.
  class CC_EXPORT ProtoReceiver {
   public:
    // TODO(khushalsagar): This should probably include a closure that returns
    // the status of processing this proto. See crbug/576974
    virtual void OnProtoReceived(
        std::unique_ptr<proto::CompositorMessage> proto) = 0;

   protected:
    virtual ~ProtoReceiver() {}
  };

  // Called by the ProtoReceiver. The RemoteProtoChannel must outlive its
  // receiver.
  virtual void SetProtoReceiver(ProtoReceiver* receiver) = 0;

  virtual void SendCompositorProto(const proto::CompositorMessage& proto) = 0;

 protected:
  virtual ~RemoteProtoChannel() {}
};

}  // namespace cc

#endif  // CC_TREES_REMOTE_PROTO_CHANNEL_H_
