// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_REMOTE_PROTO_CHANNEL_H_
#define CONTENT_PUBLIC_RENDERER_REMOTE_PROTO_CHANNEL_H_

#include <memory>

#include "content/common/content_export.h"

namespace cc {
namespace proto {
class CompositorMessage;
}  // namespace proto

}  // namespace cc

namespace content {

// Provides a bridge for getting compositor protobuf messages to/from the
// renderer and the browser.
class CONTENT_EXPORT RemoteProtoChannel {
 public:
  // Meant to be implemented by a RemoteChannel that needs to receive and parse
  // incoming protobufs.
  class CONTENT_EXPORT ProtoReceiver {
   public:
    virtual void OnProtoReceived(
        std::unique_ptr<cc::proto::CompositorMessage> proto) = 0;

   protected:
    virtual ~ProtoReceiver() {}
  };

  // Called by the ProtoReceiver. The RemoteProtoChannel must outlive its
  // receiver.
  virtual void SetProtoReceiver(ProtoReceiver* receiver) = 0;

  virtual void SendCompositorProto(
      const cc::proto::CompositorMessage& proto) = 0;

 protected:
  virtual ~RemoteProtoChannel() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_REMOTE_PROTO_CHANNEL_H_
