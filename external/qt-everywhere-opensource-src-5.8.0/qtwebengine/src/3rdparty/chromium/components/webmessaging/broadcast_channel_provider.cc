// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webmessaging/broadcast_channel_provider.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace webmessaging {

// There is a one-to-one mapping of BroadcastChannel instances in the renderer
// and Connection instances in the browser. The Connection is owned by a
// BroadcastChannelProvider.
class BroadcastChannelProvider::Connection
    : public mojom::BroadcastChannelClient {
 public:
  Connection(const url::Origin& origin,
             const mojo::String& name,
             mojom::BroadcastChannelClientAssociatedPtrInfo client,
             mojom::BroadcastChannelClientAssociatedRequest connection,
             webmessaging::BroadcastChannelProvider* service);

  void OnMessage(const mojo::String& message) override;
  void MessageToClient(const mojo::String& message) const {
    client_->OnMessage(message);
  }
  const url::Origin& origin() const { return origin_; }
  const std::string& name() const { return name_; }

  void set_connection_error_handler(const base::Closure& error_handler) {
    binding_.set_connection_error_handler(error_handler);
    client_.set_connection_error_handler(error_handler);
  }

 private:
  mojo::AssociatedBinding<mojom::BroadcastChannelClient> binding_;
  mojom::BroadcastChannelClientAssociatedPtr client_;

  webmessaging::BroadcastChannelProvider* service_;
  url::Origin origin_;
  std::string name_;
};

BroadcastChannelProvider::Connection::Connection(
    const url::Origin& origin,
    const mojo::String& name,
    mojom::BroadcastChannelClientAssociatedPtrInfo client,
    mojom::BroadcastChannelClientAssociatedRequest connection,
    webmessaging::BroadcastChannelProvider* service)
    : binding_(this, std::move(connection)),
      service_(service),
      origin_(origin),
      name_(name) {
  client_.Bind(std::move(client));
}

void BroadcastChannelProvider::Connection::OnMessage(
    const mojo::String& message) {
  service_->ReceivedMessageOnConnection(this, message);
}

BroadcastChannelProvider::BroadcastChannelProvider() {}

void BroadcastChannelProvider::Connect(
    mojo::InterfaceRequest<mojom::BroadcastChannelProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

void BroadcastChannelProvider::ConnectToChannel(
    const url::Origin& origin,
    const mojo::String& name,
    mojom::BroadcastChannelClientAssociatedPtrInfo client,
    mojom::BroadcastChannelClientAssociatedRequest connection) {
  std::unique_ptr<Connection> c(new Connection(origin, name, std::move(client),
                                               std::move(connection), this));
  c->set_connection_error_handler(
      base::Bind(&BroadcastChannelProvider::UnregisterConnection,
                 base::Unretained(this), c.get()));
  connections_[origin].insert(std::make_pair(name, std::move(c)));
}

BroadcastChannelProvider::~BroadcastChannelProvider() {}

void BroadcastChannelProvider::UnregisterConnection(Connection* c) {
  url::Origin origin = c->origin();
  auto& connections = connections_[origin];
  for (auto it = connections.lower_bound(c->name()),
            end = connections.upper_bound(c->name());
       it != end; ++it) {
    if (it->second.get() == c) {
      connections.erase(it);
      break;
    }
  }
  if (connections.empty())
    connections_.erase(origin);
}

void BroadcastChannelProvider::ReceivedMessageOnConnection(
    Connection* c,
    const mojo::String& message) {
  auto& connections = connections_[c->origin()];
  for (auto it = connections.lower_bound(c->name()),
            end = connections.upper_bound(c->name());
       it != end; ++it) {
    if (it->second.get() != c)
      it->second->MessageToClient(message);
  }
}

}  // namespace webmessaging
