// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/lib/service_connector.h"

namespace mojo {
namespace internal {

ServiceConnectorBase::ServiceConnectorBase(const std::string& name)
    : name_(name),
      registry_(NULL) {
}

ServiceConnectorBase::~ServiceConnectorBase() {}

}  // namespace internal
}  // namespace mojo
