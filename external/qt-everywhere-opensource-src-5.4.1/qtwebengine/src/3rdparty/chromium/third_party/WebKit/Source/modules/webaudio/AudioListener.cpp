/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "modules/webaudio/AudioListener.h"

#include "modules/webaudio/PannerNode.h"
#include "platform/audio/AudioBus.h"

namespace WebCore {

AudioListener::AudioListener()
    : m_position(0, 0, 0)
    , m_orientation(0, 0, -1)
    , m_upVector(0, 1, 0)
    , m_velocity(0, 0, 0)
    , m_dopplerFactor(1)
    , m_speedOfSound(343.3)
{
    ScriptWrappable::init(this);
}

AudioListener::~AudioListener()
{
    m_panners.clear();
}

void AudioListener::addPanner(PannerNode* panner)
{
    if (!panner)
        return;

    m_panners.append(panner);
}

void AudioListener::removePanner(PannerNode* panner)
{
    for (unsigned i = 0; i < m_panners.size(); ++i) {
        if (panner == m_panners[i]) {
            m_panners.remove(i);
            break;
        }
    }
}

void AudioListener::markPannersAsDirty(unsigned type)
{
    for (unsigned i = 0; i < m_panners.size(); ++i)
        m_panners[i]->markPannerAsDirty(type);
}

void AudioListener::setPosition(const FloatPoint3D &position)
{
    if (m_position == position)
        return;

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_position = position;
    markPannersAsDirty(PannerNode::AzimuthElevationDirty | PannerNode::DistanceConeGainDirty | PannerNode::DopplerRateDirty);
}

void AudioListener::setOrientation(const FloatPoint3D &orientation)
{
    if (m_orientation == orientation)
        return;

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_orientation = orientation;
    markPannersAsDirty(PannerNode::AzimuthElevationDirty);
}

void AudioListener::setUpVector(const FloatPoint3D &upVector)
{
    if (m_upVector == upVector)
        return;

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_upVector = upVector;
    markPannersAsDirty(PannerNode::AzimuthElevationDirty);
}

void AudioListener::setVelocity(const FloatPoint3D &velocity)
{
    if (m_velocity == velocity)
        return;

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_velocity = velocity;
    markPannersAsDirty(PannerNode::DopplerRateDirty);
}

void AudioListener::setDopplerFactor(double dopplerFactor)
{
    if (m_dopplerFactor == dopplerFactor)
        return;

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_dopplerFactor = dopplerFactor;
    markPannersAsDirty(PannerNode::DopplerRateDirty);
}

void AudioListener::setSpeedOfSound(double speedOfSound)
{
    if (m_speedOfSound == speedOfSound)
        return;

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_speedOfSound = speedOfSound;
    markPannersAsDirty(PannerNode::DopplerRateDirty);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
