// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRGetDevicesCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/vr/VRDisplayCollection.h"

namespace blink {

VRGetDevicesCallback::VRGetDevicesCallback(ScriptPromiseResolver* resolver, VRDisplayCollection* displays)
    : m_resolver(resolver)
    , m_displays(displays)
{
}

VRGetDevicesCallback::~VRGetDevicesCallback()
{
}

void VRGetDevicesCallback::onSuccess(mojo::WTFArray<device::blink::VRDisplayPtr> displays)
{
    m_resolver->resolve(m_displays->updateDisplays(std::move(displays)));
}

void VRGetDevicesCallback::onError()
{
    m_resolver->reject();
}

} // namespace blink
