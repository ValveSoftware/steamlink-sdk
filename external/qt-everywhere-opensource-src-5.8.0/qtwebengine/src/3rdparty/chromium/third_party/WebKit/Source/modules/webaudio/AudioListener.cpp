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

#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/PannerNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/HRTFDatabaseLoader.h"

namespace blink {

AudioListener::AudioListener(AbstractAudioContext& context)
    : m_positionX(AudioParam::create(context, ParamTypeAudioListenerPositionX, 0.0))
    , m_positionY(AudioParam::create(context, ParamTypeAudioListenerPositionY, 0.0))
    , m_positionZ(AudioParam::create(context, ParamTypeAudioListenerPositionZ, 0.0))
    , m_forwardX(AudioParam::create(context, ParamTypeAudioListenerForwardX, 0.0))
    , m_forwardY(AudioParam::create(context, ParamTypeAudioListenerForwardY, 0.0))
    , m_forwardZ(AudioParam::create(context, ParamTypeAudioListenerForwardZ, -1.0))
    , m_upX(AudioParam::create(context, ParamTypeAudioListenerUpX, 0.0))
    , m_upY(AudioParam::create(context, ParamTypeAudioListenerUpY, 1.0))
    , m_upZ(AudioParam::create(context, ParamTypeAudioListenerUpZ, 0.0))
    , m_dopplerFactor(1)
    , m_speedOfSound(343.3)
    , m_lastUpdateTime(-1)
    , m_isListenerDirty(false)
    , m_positionXValues(AudioUtilities::kRenderQuantumFrames)
    , m_positionYValues(AudioUtilities::kRenderQuantumFrames)
    , m_positionZValues(AudioUtilities::kRenderQuantumFrames)
    , m_forwardXValues(AudioUtilities::kRenderQuantumFrames)
    , m_forwardYValues(AudioUtilities::kRenderQuantumFrames)
    , m_forwardZValues(AudioUtilities::kRenderQuantumFrames)
    , m_upXValues(AudioUtilities::kRenderQuantumFrames)
    , m_upYValues(AudioUtilities::kRenderQuantumFrames)
    , m_upZValues(AudioUtilities::kRenderQuantumFrames)
{
    // Initialize the cached values with the current values.  Thus, we don't need to notify any
    // panners because we haved moved.
    m_lastPosition = position();
    m_lastForward = orientation();
    m_lastUp = upVector();
}

AudioListener::~AudioListener()
{
}

DEFINE_TRACE(AudioListener)
{
    visitor->trace(m_positionX);
    visitor->trace(m_positionY);
    visitor->trace(m_positionZ);

    visitor->trace(m_forwardX);
    visitor->trace(m_forwardY);
    visitor->trace(m_forwardZ);

    visitor->trace(m_upX);
    visitor->trace(m_upY);
    visitor->trace(m_upZ);
}

void AudioListener::addPanner(PannerHandler& panner)
{
    ASSERT(isMainThread());
    m_panners.add(&panner);
}

void AudioListener::removePanner(PannerHandler& panner)
{
    ASSERT(isMainThread());
    ASSERT(m_panners.contains(&panner));
    m_panners.remove(&panner);
}

bool AudioListener::hasSampleAccurateValues() const
{
    return positionX()->handler().hasSampleAccurateValues()
        || positionY()->handler().hasSampleAccurateValues()
        || positionZ()->handler().hasSampleAccurateValues()
        || forwardX()->handler().hasSampleAccurateValues()
        || forwardY()->handler().hasSampleAccurateValues()
        || forwardZ()->handler().hasSampleAccurateValues()
        || upX()->handler().hasSampleAccurateValues()
        || upY()->handler().hasSampleAccurateValues()
        || upZ()->handler().hasSampleAccurateValues();
}

void AudioListener::updateValuesIfNeeded(size_t framesToProcess)
{
    double currentTime = positionX()->handler().destinationHandler().currentTime();
    if (m_lastUpdateTime != currentTime) {
        // Time has changed. Update all of the automation values now.
        m_lastUpdateTime = currentTime;

        bool sizesAreGood = framesToProcess <= m_positionXValues.size()
            && framesToProcess <= m_positionYValues.size()
            && framesToProcess <= m_positionZValues.size()
            && framesToProcess <= m_forwardXValues.size()
            && framesToProcess <= m_forwardYValues.size()
            && framesToProcess <= m_forwardZValues.size()
            && framesToProcess <= m_upXValues.size()
            && framesToProcess <= m_upYValues.size()
            && framesToProcess <= m_upZValues.size();

        DCHECK(sizesAreGood);
        if (!sizesAreGood)
            return;

        positionX()->handler().calculateSampleAccurateValues(m_positionXValues.data(), framesToProcess);
        positionY()->handler().calculateSampleAccurateValues(m_positionYValues.data(), framesToProcess);
        positionZ()->handler().calculateSampleAccurateValues(m_positionZValues.data(), framesToProcess);

        forwardX()->handler().calculateSampleAccurateValues(m_forwardXValues.data(), framesToProcess);
        forwardY()->handler().calculateSampleAccurateValues(m_forwardYValues.data(), framesToProcess);
        forwardZ()->handler().calculateSampleAccurateValues(m_forwardZValues.data(), framesToProcess);

        upX()->handler().calculateSampleAccurateValues(m_upXValues.data(), framesToProcess);
        upY()->handler().calculateSampleAccurateValues(m_upYValues.data(), framesToProcess);
        upZ()->handler().calculateSampleAccurateValues(m_upZValues.data(), framesToProcess);
    }
}

const float* AudioListener::getPositionXValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_positionXValues.data();
}

const float* AudioListener::getPositionYValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_positionYValues.data();
}

const float* AudioListener::getPositionZValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_positionZValues.data();
}

const float* AudioListener::getForwardXValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_forwardXValues.data();
}

const float* AudioListener::getForwardYValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_forwardYValues.data();
}

const float* AudioListener::getForwardZValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_forwardZValues.data();
}

const float* AudioListener::getUpXValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_upXValues.data();
}

const float* AudioListener::getUpYValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_upYValues.data();
}

const float* AudioListener::getUpZValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_upZValues.data();
}

void AudioListener::updateState()
{
    // This must be called from the audio thread in pre or post render phase of
    // the graph processing.  (AudioListener doesn't have access to the context
    // to check for the audio thread.)
    DCHECK(!isMainThread());

    MutexTryLocker tryLocker(m_listenerLock);
    if (tryLocker.locked()) {
        FloatPoint3D currentPosition = position();
        FloatPoint3D currentForward = orientation();
        FloatPoint3D currentUp = upVector();

        m_isListenerDirty = currentPosition != m_lastPosition
            || currentForward != m_lastForward
            || currentUp != m_lastUp;

        if (m_isListenerDirty) {
            m_lastPosition = currentPosition;
            m_lastForward = currentForward;
            m_lastUp = currentUp;
        }
    } else {
        // Main thread must be updating the position, forward, or up vector;
        // just assume the listener is dirty.  At worst, we'll do a little more
        // work than necessary for one rendering quantum.
        m_isListenerDirty = true;
    }
}

void AudioListener::createAndLoadHRTFDatabaseLoader(float sampleRate)
{
    if (!m_hrtfDatabaseLoader)
        m_hrtfDatabaseLoader = HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(sampleRate);
}

bool AudioListener::isHRTFDatabaseLoaded()
{
    return m_hrtfDatabaseLoader->isLoaded();
}

void AudioListener::waitForHRTFDatabaseLoaderThreadCompletion()
{
    if (m_hrtfDatabaseLoader)
        m_hrtfDatabaseLoader->waitForLoaderThreadCompletion();
}

void AudioListener::markPannersAsDirty(unsigned type)
{
    DCHECK(isMainThread());
    for (PannerHandler* panner : m_panners)
        panner->markPannerAsDirty(type);
}

void AudioListener::setPosition(const FloatPoint3D& position)
{
    DCHECK(isMainThread());

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_positionX->setValue(position.x());
    m_positionY->setValue(position.y());
    m_positionZ->setValue(position.z());
    markPannersAsDirty(PannerHandler::AzimuthElevationDirty | PannerHandler::DistanceConeGainDirty);
}

void AudioListener::setOrientation(const FloatPoint3D& orientation)
{
    DCHECK(isMainThread());

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_forwardX->setValue(orientation.x());
    m_forwardY->setValue(orientation.y());
    m_forwardZ->setValue(orientation.z());
    markPannersAsDirty(PannerHandler::AzimuthElevationDirty);
}

void AudioListener::setUpVector(const FloatPoint3D& upVector)
{
    DCHECK(isMainThread());

    // This synchronizes with panner's process().
    MutexLocker listenerLocker(m_listenerLock);
    m_upX->setValue(upVector.x());
    m_upY->setValue(upVector.y());
    m_upZ->setValue(upVector.z());
    markPannersAsDirty(PannerHandler::AzimuthElevationDirty);
}

void AudioListener::setVelocity(float x, float y, float z)
{
    // The velocity is not used internally and cannot be read back by scripts,
    // so it can be ignored entirely.
}

void AudioListener::setDopplerFactor(double dopplerFactor)
{
    m_dopplerFactor = dopplerFactor;
}

void AudioListener::setSpeedOfSound(double speedOfSound)
{
    m_speedOfSound = speedOfSound;
}

} // namespace blink
