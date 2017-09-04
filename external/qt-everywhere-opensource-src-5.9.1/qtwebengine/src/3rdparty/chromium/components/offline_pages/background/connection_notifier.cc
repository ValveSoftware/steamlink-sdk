// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/connection_notifier.h"

namespace offline_pages {

ConnectionNotifier::ConnectionNotifier(
    const ConnectionNotifier::ConnectedCallback& callback)
    : callback_(callback) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ConnectionNotifier::~ConnectionNotifier() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ConnectionNotifier::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE)
    callback_.Run();
}

}  // namespace offline_pages
