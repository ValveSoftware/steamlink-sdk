// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplayCollection_h
#define VRDisplayCollection_h

#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplay.h"
#include "platform/heap/Handle.h"

namespace blink {

struct WebVRDevice;

class VRDisplayCollection final : public GarbageCollected<VRDisplayCollection> {
public:
    explicit VRDisplayCollection(NavigatorVR*);

    VRDisplayVector updateDisplays(mojo::WTFArray<device::blink::VRDisplayPtr>);
    VRDisplay* getDisplayForIndex(unsigned index);

    DECLARE_VIRTUAL_TRACE();

private:
    Member<NavigatorVR> m_navigatorVR;
    HeapVector<Member<VRDisplay>> m_displays;
};

} // namespace blink

#endif // VRDisplayCollection_h
