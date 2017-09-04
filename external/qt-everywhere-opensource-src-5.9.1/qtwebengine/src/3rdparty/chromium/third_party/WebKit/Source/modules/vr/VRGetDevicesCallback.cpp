// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRGetDevicesCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"

namespace blink {

VRGetDevicesCallback::VRGetDevicesCallback(ScriptPromiseResolver* resolver)
    : m_resolver(resolver) {}

VRGetDevicesCallback::~VRGetDevicesCallback() {}

void VRGetDevicesCallback::onSuccess(VRDisplayVector displays) {
  m_resolver->resolve(displays);
}

void VRGetDevicesCallback::onError() {
  m_resolver->reject();
}

}  // namespace blink
