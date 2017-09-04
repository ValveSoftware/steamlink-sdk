// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_ENGINE_TRANSPORT_H_
#define BLIMP_NET_BLIMP_ENGINE_TRANSPORT_H_

#include <memory>
#include <string>

#include "blimp/net/blimp_transport.h"
#include "net/base/ip_endpoint.h"

namespace blimp {

// Interface specific to engine transport on top of generic |BlimpTransport|.
class BlimpEngineTransport : public BlimpTransport {
 public:
  ~BlimpEngineTransport() override {}

  // Obtains the local listening address.
  virtual void GetLocalAddress(net::IPEndPoint* ip_address) const = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_ENGINE_TRANSPORT_H_
