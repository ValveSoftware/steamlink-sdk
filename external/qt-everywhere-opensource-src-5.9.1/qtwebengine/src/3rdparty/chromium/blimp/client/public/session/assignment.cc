// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/session/assignment.h"

#include "net/cert/x509_certificate.h"

namespace blimp {
namespace client {

Assignment::Assignment() : transport_protocol(TransportProtocol::UNKNOWN) {}

Assignment::Assignment(const Assignment& other) = default;

Assignment::~Assignment() {}

bool Assignment::IsValid() const {
  if (engine_endpoint.address().empty() || engine_endpoint.port() == 0 ||
      transport_protocol == TransportProtocol::UNKNOWN) {
    return false;
  }
  if (transport_protocol == TransportProtocol::SSL && !cert) {
    return false;
  }
  return true;
}

}  // namespace client
}  // namespace blimp
