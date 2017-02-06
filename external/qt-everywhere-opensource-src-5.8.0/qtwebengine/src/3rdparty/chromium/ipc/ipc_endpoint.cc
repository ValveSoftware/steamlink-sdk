// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_endpoint.h"

namespace IPC {

Endpoint::Endpoint() : attachment_broker_endpoint_(false) {}

void Endpoint::SetAttachmentBrokerEndpoint(bool is_endpoint) {
  if (is_endpoint != attachment_broker_endpoint_) {
    attachment_broker_endpoint_ = is_endpoint;
    OnSetAttachmentBrokerEndpoint();
  }
}

}  // namespace IPC
