// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/ports/event.h"

#include <string.h>

namespace mojo {
namespace edk {
namespace ports {

namespace {

const size_t kPortsMessageAlignment = 8;

static_assert(sizeof(PortDescriptor) % kPortsMessageAlignment == 0,
              "Invalid PortDescriptor size.");

static_assert(sizeof(EventHeader) % kPortsMessageAlignment == 0,
              "Invalid EventHeader size.");

static_assert(sizeof(UserEventData) % kPortsMessageAlignment == 0,
              "Invalid UserEventData size.");

static_assert(sizeof(ObserveProxyEventData) % kPortsMessageAlignment == 0,
              "Invalid ObserveProxyEventData size.");

static_assert(sizeof(ObserveProxyAckEventData) % kPortsMessageAlignment == 0,
              "Invalid ObserveProxyAckEventData size.");

static_assert(sizeof(ObserveClosureEventData) % kPortsMessageAlignment == 0,
              "Invalid ObserveClosureEventData size.");

static_assert(sizeof(MergePortEventData) % kPortsMessageAlignment == 0,
              "Invalid MergePortEventData size.");

}  // namespace

PortDescriptor::PortDescriptor() {
  memset(padding, 0, sizeof(padding));
}

}  // namespace ports
}  // namespace edk
}  // namespace mojo
