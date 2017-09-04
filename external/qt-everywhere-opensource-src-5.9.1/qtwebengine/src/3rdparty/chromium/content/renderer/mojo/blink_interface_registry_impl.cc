// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_interface_registry_impl.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace content {

BlinkInterfaceRegistryImpl::BlinkInterfaceRegistryImpl(
    base::WeakPtr<service_manager::InterfaceRegistry> interface_registry)
    : interface_registry_(interface_registry) {}

BlinkInterfaceRegistryImpl::~BlinkInterfaceRegistryImpl() = default;

void BlinkInterfaceRegistryImpl::addInterface(
    const char* name,
    const blink::InterfaceFactory& factory) {
  if (!interface_registry_)
    return;

  interface_registry_->AddInterface(name, factory);
}

}  // namespace content
