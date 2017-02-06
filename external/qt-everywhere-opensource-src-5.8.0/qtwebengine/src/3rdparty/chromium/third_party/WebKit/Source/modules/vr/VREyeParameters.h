// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VREyeParameters_h
#define VREyeParameters_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRFieldOfView.h"
#include "platform/heap/Handle.h"

#include "wtf/Forward.h"

namespace blink {

struct WebVREyeParameters;

class VREyeParameters final : public GarbageCollected<VREyeParameters>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    VREyeParameters();

    DOMFloat32Array* offset() const { return m_offset; }
    VRFieldOfView* fieldOfView() const { return m_fieldOfView; }
    unsigned long renderWidth() const { return m_renderWidth; }
    unsigned long renderHeight() const { return m_renderHeight; }

    void update(const device::blink::VREyeParametersPtr&);

    DECLARE_VIRTUAL_TRACE()

private:
    Member<DOMFloat32Array> m_offset;
    Member<VRFieldOfView> m_fieldOfView;
    unsigned long m_renderWidth;
    unsigned long m_renderHeight;
};

} // namespace blink

#endif // VREyeParameters_h
