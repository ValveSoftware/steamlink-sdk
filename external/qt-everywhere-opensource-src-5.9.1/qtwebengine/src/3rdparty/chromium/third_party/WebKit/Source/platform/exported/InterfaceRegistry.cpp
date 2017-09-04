// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/InterfaceRegistry.h"

#include "wtf/StdLibExtras.h"

namespace blink {
namespace {

class EmptyInterfaceRegistry : public InterfaceRegistry {
  void addInterface(const char* name,
                    const InterfaceFactory& factory) override {}
};

}  // namespace

InterfaceRegistry* InterfaceRegistry::getEmptyInterfaceRegistry() {
  DEFINE_STATIC_LOCAL(EmptyInterfaceRegistry, emptyInterfaceRegistry, ());
  return &emptyInterfaceRegistry;
}

}  // namespace blink
