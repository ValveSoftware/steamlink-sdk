// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SESSION_NETWORK_EVENT_OBSERVER_H_
#define BLIMP_CLIENT_CORE_SESSION_NETWORK_EVENT_OBSERVER_H_

namespace blimp {
namespace client {

// A NetworkEventObserver is informed whenever changes happen to the connection
// to the Blimp engine.
class NetworkEventObserver {
 public:
  NetworkEventObserver() {}
  virtual ~NetworkEventObserver() {}

  virtual void OnConnected() = 0;
  virtual void OnDisconnected(int result) = 0;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SESSION_NETWORK_EVENT_OBSERVER_H_
