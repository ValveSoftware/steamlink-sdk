// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/ports/port_ref.h"

#include "mojo/edk/system/ports/port.h"

namespace mojo {
namespace edk {
namespace ports {

PortRef::~PortRef() {
}

PortRef::PortRef() {
}

PortRef::PortRef(const PortName& name, const scoped_refptr<Port>& port)
    : name_(name), port_(port) {
}

PortRef::PortRef(const PortRef& other)
    : name_(other.name_), port_(other.port_) {
}

PortRef& PortRef::operator=(const PortRef& other) {
  if (&other != this) {
    name_ = other.name_;
    port_ = other.port_;
  }
  return *this;
}

}  // namespace ports
}  // namespace edk
}  // namespace mojo
