// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_CONNECTION_NOTIFIER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_CONNECTION_NOTIFIER_H_

#include "base/callback.h"
#include "net/base/network_change_notifier.h"

namespace offline_pages {

class ConnectionNotifier
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Callback to call when we become connected.
  typedef base::Callback<void()> ConnectedCallback;

  ConnectionNotifier(const ConnectionNotifier::ConnectedCallback& callback);
  ~ConnectionNotifier() override;

  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  base::Callback<void()> callback_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionNotifier);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_CONNECTION_NOTIFIER_H_
