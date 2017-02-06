// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRPose_h
#define VRPose_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class VRPose final : public GarbageCollected<VRPose>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static VRPose* create()
    {
        return new VRPose();
    }

    double timeStamp() const { return m_timeStamp; }
    DOMFloat32Array* orientation() const { return m_orientation; }
    DOMFloat32Array* position() const { return m_position; }
    DOMFloat32Array* angularVelocity() const { return m_angularVelocity; }
    DOMFloat32Array* linearVelocity() const { return m_linearVelocity; }
    DOMFloat32Array* angularAcceleration() const { return m_angularAcceleration; }
    DOMFloat32Array* linearAcceleration() const { return m_linearAcceleration; }

    void setPose(const device::blink::VRPosePtr&);

    DECLARE_VIRTUAL_TRACE();

private:
    VRPose();

    double m_timeStamp;
    Member<DOMFloat32Array> m_orientation;
    Member<DOMFloat32Array> m_position;
    Member<DOMFloat32Array> m_angularVelocity;
    Member<DOMFloat32Array> m_linearVelocity;
    Member<DOMFloat32Array> m_angularAcceleration;
    Member<DOMFloat32Array> m_linearAcceleration;
};

} // namespace blink

#endif // VRPose_h
