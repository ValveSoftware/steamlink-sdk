/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DeviceMotionData_h
#define DeviceMotionData_h

#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {
class WebDeviceMotionData;
}

namespace WebCore {

class DeviceMotionData : public RefCountedWillBeGarbageCollected<DeviceMotionData> {
public:

    class Acceleration : public RefCountedWillBeGarbageCollected<DeviceMotionData::Acceleration> {
    public:
        static PassRefPtrWillBeRawPtr<Acceleration> create(bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z);
        void trace(Visitor*) { }

        bool canProvideX() const { return m_canProvideX; }
        bool canProvideY() const { return m_canProvideY; }
        bool canProvideZ() const { return m_canProvideZ; }

        double x() const { return m_x; }
        double y() const { return m_y; }
        double z() const { return m_z; }

    private:
        Acceleration(bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z);

        double m_x;
        double m_y;
        double m_z;

        bool m_canProvideX;
        bool m_canProvideY;
        bool m_canProvideZ;
    };

    class RotationRate : public RefCountedWillBeGarbageCollected<DeviceMotionData::RotationRate> {
    public:
        static PassRefPtrWillBeRawPtr<RotationRate> create(bool canProvideAlpha, double alpha, bool canProvideBeta,  double beta, bool canProvideGamma, double gamma);
        void trace(Visitor*) { }

        bool canProvideAlpha() const { return m_canProvideAlpha; }
        bool canProvideBeta() const { return m_canProvideBeta; }
        bool canProvideGamma() const { return m_canProvideGamma; }

        double alpha() const { return m_alpha; }
        double beta() const { return m_beta; }
        double gamma() const { return m_gamma; }

    private:
        RotationRate(bool canProvideAlpha, double alpha, bool canProvideBeta,  double beta, bool canProvideGamma, double gamma);

        double m_alpha;
        double m_beta;
        double m_gamma;

        bool m_canProvideAlpha;
        bool m_canProvideBeta;
        bool m_canProvideGamma;
    };

    static PassRefPtrWillBeRawPtr<DeviceMotionData> create();
    static PassRefPtrWillBeRawPtr<DeviceMotionData> create(
        PassRefPtrWillBeRawPtr<Acceleration>,
        PassRefPtrWillBeRawPtr<Acceleration> accelerationIncludingGravity,
        PassRefPtrWillBeRawPtr<RotationRate>,
        bool canProvideInterval,
        double interval);
    static PassRefPtrWillBeRawPtr<DeviceMotionData> create(const blink::WebDeviceMotionData&);
    void trace(Visitor*);

    Acceleration* acceleration() const { return m_acceleration.get(); }
    Acceleration* accelerationIncludingGravity() const { return m_accelerationIncludingGravity.get(); }
    RotationRate* rotationRate() const { return m_rotationRate.get(); }

    bool canProvideInterval() const { return m_canProvideInterval; }
    double interval() const { return m_interval; }

    bool canProvideEventData() const;

private:
    DeviceMotionData();
    DeviceMotionData(PassRefPtrWillBeRawPtr<Acceleration>, PassRefPtrWillBeRawPtr<Acceleration> accelerationIncludingGravity, PassRefPtrWillBeRawPtr<RotationRate>, bool canProvideInterval, double interval);

    RefPtrWillBeMember<Acceleration> m_acceleration;
    RefPtrWillBeMember<Acceleration> m_accelerationIncludingGravity;
    RefPtrWillBeMember<RotationRate> m_rotationRate;
    bool m_canProvideInterval;
    double m_interval;
};

} // namespace WebCore

#endif // DeviceMotionData_h
