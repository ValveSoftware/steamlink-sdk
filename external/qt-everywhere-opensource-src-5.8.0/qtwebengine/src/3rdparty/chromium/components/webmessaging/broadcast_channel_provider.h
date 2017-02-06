// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBMESSAGING_BROADCAST_CHANNEL_PROVIDER_H_
#define COMPONENTS_WEBMESSAGING_BROADCAST_CHANNEL_PROVIDER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "components/webmessaging/public/interfaces/broadcast_channel.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "url/origin.h"

namespace webmessaging {

class BroadcastChannelProvider
    : public base::RefCountedThreadSafe<BroadcastChannelProvider>,
      public mojom::BroadcastChannelProvider {
 public:
  BroadcastChannelProvider();
  void Connect(mojo::InterfaceRequest<mojom::BroadcastChannelProvider> request);

  void ConnectToChannel(
      const url::Origin& origin,
      const mojo::String& name,
      mojom::BroadcastChannelClientAssociatedPtrInfo client,
      mojom::BroadcastChannelClientAssociatedRequest connection) override;

 private:
  friend class base::RefCountedThreadSafe<BroadcastChannelProvider>;
  class Connection;

  ~BroadcastChannelProvider() override;

  void UnregisterConnection(Connection*);
  void ReceivedMessageOnConnection(Connection*, const mojo::String& message);

  mojo::BindingSet<mojom::BroadcastChannelProvider> bindings_;
  std::map<url::Origin, std::multimap<std::string, std::unique_ptr<Connection>>>
      connections_;
};

}  // namespace webmessaging

#endif  // COMPONENTS_WEBMESSAGING_BROADCAST_CHANNEL_PROVIDER_H_
