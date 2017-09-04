// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_CONNECTION_ERROR_OBSERVER_H_
#define BLIMP_NET_CONNECTION_ERROR_OBSERVER_H_

namespace blimp {

class ConnectionErrorObserver {
 public:
  virtual ~ConnectionErrorObserver() {}

  // Called when a blimp connection encounters an error.
  // Negative |error| values correspond directly to net/ error codes.
  // Positive values indicate the EndConnection.reason code specified
  // by the peer during a "clean" disconnection.
  virtual void OnConnectionError(int error) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_CONNECTION_ERROR_OBSERVER_H_
