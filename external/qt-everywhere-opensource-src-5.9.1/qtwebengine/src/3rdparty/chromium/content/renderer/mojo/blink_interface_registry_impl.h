// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_
#define CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/InterfaceRegistry.h"

namespace service_manager {
class InterfaceRegistry;
}

namespace content {

class BlinkInterfaceRegistryImpl : public blink::InterfaceRegistry {
 public:
  explicit BlinkInterfaceRegistryImpl(
      base::WeakPtr<service_manager::InterfaceRegistry> interface_registry);
  ~BlinkInterfaceRegistryImpl();

  // blink::InterfaceRegistry override.
  void addInterface(const char* name,
                    const blink::InterfaceFactory& factory) override;

 private:
  const base::WeakPtr<service_manager::InterfaceRegistry> interface_registry_;

  DISALLOW_COPY_AND_ASSIGN(BlinkInterfaceRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_
