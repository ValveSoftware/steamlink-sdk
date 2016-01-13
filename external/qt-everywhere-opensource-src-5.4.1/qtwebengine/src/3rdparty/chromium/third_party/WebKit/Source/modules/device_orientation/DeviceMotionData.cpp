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

#include "config.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include "public/platform/WebDeviceMotionData.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<DeviceMotionData::Acceleration> DeviceMotionData::Acceleration::create(
    bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z)
{
    return adoptRefWillBeNoop(new DeviceMotionData::Acceleration(canProvideX, x, canProvideY, y, canProvideZ, z));
}

DeviceMotionData::Acceleration::Acceleration(bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z)
    : m_x(x)
    , m_y(y)
    , m_z(z)
    , m_canProvideX(canProvideX)
    , m_canProvideY(canProvideY)
    , m_canProvideZ(canProvideZ)

{
}

PassRefPtrWillBeRawPtr<DeviceMotionData::RotationRate> DeviceMotionData::RotationRate::create(
    bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    return adoptRefWillBeNoop(new DeviceMotionData::RotationRate(canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma));
}

DeviceMotionData::RotationRate::RotationRate(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
    : m_alpha(alpha)
    , m_beta(beta)
    , m_gamma(gamma)
    , m_canProvideAlpha(canProvideAlpha)
    , m_canProvideBeta(canProvideBeta)
    , m_canProvideGamma(canProvideGamma)
{
}

PassRefPtrWillBeRawPtr<DeviceMotionData> DeviceMotionData::create()
{
    return adoptRefWillBeNoop(new DeviceMotionData);
}

PassRefPtrWillBeRawPtr<DeviceMotionData> DeviceMotionData::create(
    PassRefPtrWillBeRawPtr<Acceleration> acceleration,
    PassRefPtrWillBeRawPtr<Acceleration> accelerationIncludingGravity,
    PassRefPtrWillBeRawPtr<RotationRate> rotationRate,
    bool canProvideInterval,
    double interval)
{
    return adoptRefWillBeNoop(new DeviceMotionData(acceleration, accelerationIncludingGravity, rotationRate, canProvideInterval, interval));
}

PassRefPtrWillBeRawPtr<DeviceMotionData> DeviceMotionData::create(const blink::WebDeviceMotionData& data)
{
    return DeviceMotionData::create(
        DeviceMotionData::Acceleration::create(
            data.hasAccelerationX, data.accelerationX,
            data.hasAccelerationY, data.accelerationY,
            data.hasAccelerationZ, data.accelerationZ),
        DeviceMotionData::Acceleration::create(
            data.hasAccelerationIncludingGravityX, data.accelerationIncludingGravityX,
            data.hasAccelerationIncludingGravityY, data.accelerationIncludingGravityY,
            data.hasAccelerationIncludingGravityZ, data.accelerationIncludingGravityZ),
        DeviceMotionData::RotationRate::create(
            data.hasRotationRateAlpha, data.rotationRateAlpha,
            data.hasRotationRateBeta, data.rotationRateBeta,
            data.hasRotationRateGamma, data.rotationRateGamma),
        true, data.interval);
}

DeviceMotionData::DeviceMotionData()
    : m_canProvideInterval(false)
    , m_interval(0)
{
}

DeviceMotionData::DeviceMotionData(
    PassRefPtrWillBeRawPtr<Acceleration> acceleration,
    PassRefPtrWillBeRawPtr<Acceleration> accelerationIncludingGravity,
    PassRefPtrWillBeRawPtr<RotationRate> rotationRate,
    bool canProvideInterval,
    double interval)
    : m_acceleration(acceleration)
    , m_accelerationIncludingGravity(accelerationIncludingGravity)
    , m_rotationRate(rotationRate)
    , m_canProvideInterval(canProvideInterval)
    , m_interval(interval)
{
}

void DeviceMotionData::trace(Visitor* visitor)
{
    visitor->trace(m_acceleration);
    visitor->trace(m_accelerationIncludingGravity);
    visitor->trace(m_rotationRate);
}

bool DeviceMotionData::canProvideEventData() const
{
    const bool hasAcceleration = m_acceleration && (m_acceleration->canProvideX() || m_acceleration->canProvideY() || m_acceleration->canProvideZ());
    const bool hasAccelerationIncludingGravity = m_accelerationIncludingGravity && (m_accelerationIncludingGravity->canProvideX() || m_accelerationIncludingGravity->canProvideY() || m_accelerationIncludingGravity->canProvideZ());
    const bool hasRotationRate = m_rotationRate && (m_rotationRate->canProvideAlpha() || m_rotationRate->canProvideBeta() || m_rotationRate->canProvideGamma());

    return hasAcceleration || hasAccelerationIncludingGravity || hasRotationRate;
}

} // namespace WebCore
