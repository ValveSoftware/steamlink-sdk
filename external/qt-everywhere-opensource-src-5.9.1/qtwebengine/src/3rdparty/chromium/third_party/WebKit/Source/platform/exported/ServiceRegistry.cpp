// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/InterfaceProvider.h"

#include "wtf/StdLibExtras.h"

namespace blink {
namespace {

class EmptyInterfaceProvider : public InterfaceProvider {
  void getInterface(const char* name, mojo::ScopedMessagePipeHandle) override {}
};
}

InterfaceProvider* InterfaceProvider::getEmptyInterfaceProvider() {
  DEFINE_STATIC_LOCAL(EmptyInterfaceProvider, emptyInterfaceProvider, ());
  return &emptyInterfaceProvider;
}
}
