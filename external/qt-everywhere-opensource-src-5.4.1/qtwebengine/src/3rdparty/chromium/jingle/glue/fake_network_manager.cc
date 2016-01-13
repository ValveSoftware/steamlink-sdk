// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/fake_network_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "jingle/glue/utils.h"
#include "net/base/ip_endpoint.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace jingle_glue {

FakeNetworkManager::FakeNetworkManager(const net::IPAddressNumber& address)
    : started_(false),
      weak_factory_(this) {
  net::IPEndPoint endpoint(address, 0);
  talk_base::SocketAddress socket_address;
  CHECK(IPEndPointToSocketAddress(endpoint, &socket_address));
  network_.reset(new talk_base::Network("fake", "Fake Network",
                                        socket_address.ipaddr(), 32));
  network_->AddIP(socket_address.ipaddr());
}

FakeNetworkManager::~FakeNetworkManager() {
}

void FakeNetworkManager::StartUpdating() {
  started_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeNetworkManager::SendNetworksChangedSignal,
                 weak_factory_.GetWeakPtr()));
}

void FakeNetworkManager::StopUpdating() {
  started_ = false;
}

void FakeNetworkManager::GetNetworks(NetworkList* networks) const {
  networks->clear();
  networks->push_back(network_.get());
}

void FakeNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace jingle_glue
