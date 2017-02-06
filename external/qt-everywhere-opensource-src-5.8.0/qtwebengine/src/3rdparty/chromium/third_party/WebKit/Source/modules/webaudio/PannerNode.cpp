/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webaudio/PannerNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/AudioBufferSourceNode.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "platform/Histogram.h"
#include "platform/audio/HRTFPanner.h"
#include "wtf/MathExtras.h"

namespace blink {

static void fixNANs(double& x)
{
    if (std::isnan(x) || std::isinf(x))
        x = 0.0;
}

PannerHandler::PannerHandler(
    AudioNode& node, float sampleRate,
    AudioParamHandler& positionX,
    AudioParamHandler& positionY,
    AudioParamHandler& positionZ,
    AudioParamHandler& orientationX,
    AudioParamHandler& orientationY,
    AudioParamHandler& orientationZ)
    : AudioHandler(NodeTypePanner, node, sampleRate)
    , m_listener(node.context()->listener())
    , m_distanceModel(DistanceEffect::ModelInverse)
    , m_isAzimuthElevationDirty(true)
    , m_isDistanceConeGainDirty(true)
    , m_lastGain(-1.0)
    , m_cachedAzimuth(0)
    , m_cachedElevation(0)
    , m_cachedDistanceConeGain(1.0f)
    , m_positionX(positionX)
    , m_positionY(positionY)
    , m_positionZ(positionZ)
    , m_orientationX(orientationX)
    , m_orientationY(orientationY)
    , m_orientationZ(orientationZ)
{
    // Load the HRTF database asynchronously so we don't block the Javascript thread while creating the HRTF database.
    // The HRTF panner will return zeroes until the database is loaded.
    listener()->createAndLoadHRTFDatabaseLoader(node.context()->sampleRate());

    addInput();
    addOutput(2);

    // Node-specific default mixing rules.
    m_channelCount = 2;
    m_channelCountMode = ClampedMax;
    m_channelInterpretation = AudioBus::Speakers;

    // Explicitly set the default panning model here so that the histograms
    // include the default value.
    setPanningModel("equalpower");

    initialize();
}

PassRefPtr<PannerHandler> PannerHandler::create(
    AudioNode& node, float sampleRate,
    AudioParamHandler& positionX,
    AudioParamHandler& positionY,
    AudioParamHandler& positionZ,
    AudioParamHandler& orientationX,
    AudioParamHandler& orientationY,
    AudioParamHandler& orientationZ)
{
    return adoptRef(new PannerHandler(
        node,
        sampleRate,
        positionX,
        positionY,
        positionZ,
        orientationX,
        orientationY,
        orientationZ));
}

PannerHandler::~PannerHandler()
{
    uninitialize();
}

void PannerHandler::process(size_t framesToProcess)
{
    AudioBus* destination = output(0).bus();

    if (!isInitialized() || !input(0).isConnected() || !m_panner.get()) {
        destination->zero();
        return;
    }

    AudioBus* source = input(0).bus();
    if (!source) {
        destination->zero();
        return;
    }

    // The audio thread can't block on this lock, so we call tryLock() instead.
    MutexTryLocker tryLocker(m_processLock);
    MutexTryLocker tryListenerLocker(listener()->listenerLock());

    if (tryLocker.locked() && tryListenerLocker.locked()) {
        // HRTFDatabase should be loaded before proceeding when the panning model is HRTF.
        if (m_panningModel == Panner::PanningModelHRTF && !listener()->isHRTFDatabaseLoaded()) {
            if (context()->hasRealtimeConstraint()) {
                // Some AbstractAudioContexts cannot block on the HRTFDatabase loader.
                destination->zero();
                return;
            }

            listener()->waitForHRTFDatabaseLoaderThreadCompletion();
        }

        if (hasSampleAccurateValues() || listener()->hasSampleAccurateValues()) {
            // It's tempting to skip sample-accurate processing if isAzimuthElevationDirty() and
            // isDistanceConeGain() both return false.  But in general we can't because something
            // may scheduled to start in the middle of the rendering quantum.  On the other hand,
            // the audible effect may be small enough that we can afford to do this optimization.
            processSampleAccurateValues(destination, source, framesToProcess);
        } else {
            // Apply the panning effect.
            double azimuth;
            double elevation;

            // Update dirty state in case something has moved; this can happen if the AudioParam for
            // the position or orientation component is set directly.
            updateDirtyState();

            azimuthElevation(&azimuth, &elevation);

            m_panner->pan(azimuth, elevation, source, destination, framesToProcess);

            // Get the distance and cone gain.
            float totalGain = distanceConeGain();

            m_lastGain = totalGain;

            // Apply gain in-place with de-zippering.
            destination->copyWithGainFrom(*destination, &m_lastGain, totalGain);
        }
    } else {
        // Too bad - The tryLock() failed.
        // We must be in the middle of changing the properties of the panner or the listener.
        destination->zero();
    }
}

void PannerHandler::processSampleAccurateValues(AudioBus* destination, const AudioBus* source, size_t framesToProcess)
{
    RELEASE_ASSERT(framesToProcess <= ProcessingSizeInFrames);

    // Get the sample accurate values from all of the AudioParams, including the values from the
    // AudioListener.
    float pannerX[ProcessingSizeInFrames];
    float pannerY[ProcessingSizeInFrames];
    float pannerZ[ProcessingSizeInFrames];

    float orientationX[ProcessingSizeInFrames];
    float orientationY[ProcessingSizeInFrames];
    float orientationZ[ProcessingSizeInFrames];

    m_positionX->calculateSampleAccurateValues(pannerX, framesToProcess);
    m_positionY->calculateSampleAccurateValues(pannerY, framesToProcess);
    m_positionZ->calculateSampleAccurateValues(pannerZ, framesToProcess);
    m_orientationX->calculateSampleAccurateValues(orientationX, framesToProcess);
    m_orientationY->calculateSampleAccurateValues(orientationY, framesToProcess);
    m_orientationZ->calculateSampleAccurateValues(orientationZ, framesToProcess);

    // Get the automation values from the listener.
    const float* listenerX = listener()->getPositionXValues(ProcessingSizeInFrames);
    const float* listenerY = listener()->getPositionYValues(ProcessingSizeInFrames);
    const float* listenerZ = listener()->getPositionZValues(ProcessingSizeInFrames);

    const float* forwardX = listener()->getForwardXValues(ProcessingSizeInFrames);
    const float* forwardY = listener()->getForwardYValues(ProcessingSizeInFrames);
    const float* forwardZ = listener()->getForwardZValues(ProcessingSizeInFrames);

    const float* upX = listener()->getUpXValues(ProcessingSizeInFrames);
    const float* upY = listener()->getUpYValues(ProcessingSizeInFrames);
    const float* upZ = listener()->getUpZValues(ProcessingSizeInFrames);

    // Compute the azimuth, elevation, and total gains for each position.
    double azimuth[ProcessingSizeInFrames];
    double elevation[ProcessingSizeInFrames];
    float totalGain[ProcessingSizeInFrames];

    for (unsigned k = 0; k < framesToProcess; ++k) {
        FloatPoint3D pannerPosition(pannerX[k], pannerY[k], pannerZ[k]);
        FloatPoint3D orientation(orientationX[k], orientationY[k], orientationZ[k]);
        FloatPoint3D listenerPosition(listenerX[k], listenerY[k], listenerZ[k]);
        FloatPoint3D listenerForward(forwardX[k], forwardY[k], forwardZ[k]);
        FloatPoint3D listenerUp(upX[k], upY[k], upZ[k]);

        calculateAzimuthElevation(&azimuth[k], &elevation[k],
            pannerPosition, listenerPosition, listenerForward, listenerUp);

        // Get distance and cone gain
        totalGain[k] = calculateDistanceConeGain(pannerPosition, orientation, listenerPosition);
    }

    m_panner->panWithSampleAccurateValues(azimuth, elevation, source, destination, framesToProcess);
    destination->copyWithSampleAccurateGainValuesFrom(*destination, totalGain, framesToProcess);
}

void PannerHandler::initialize()
{
    if (isInitialized())
        return;

    m_panner = Panner::create(m_panningModel, sampleRate(), listener()->hrtfDatabaseLoader());
    listener()->addPanner(*this);

    // Set the cached values to the current values to start things off.  The panner is already
    // marked as dirty, so this won't matter.
    m_lastPosition = position();
    m_lastOrientation = orientation();

    AudioHandler::initialize();
}

void PannerHandler::uninitialize()
{
    if (!isInitialized())
        return;

    m_panner.reset();
    listener()->removePanner(*this);

    AudioHandler::uninitialize();
}

AudioListener* PannerHandler::listener()
{
    return m_listener;
}

String PannerHandler::panningModel() const
{
    switch (m_panningModel) {
    case Panner::PanningModelEqualPower:
        return "equalpower";
    case Panner::PanningModelHRTF:
        return "HRTF";
    default:
        ASSERT_NOT_REACHED();
        return "equalpower";
    }
}

void PannerHandler::setPanningModel(const String& model)
{
    if (model == "equalpower")
        setPanningModel(Panner::PanningModelEqualPower);
    else if (model == "HRTF")
        setPanningModel(Panner::PanningModelHRTF);
}

bool PannerHandler::setPanningModel(unsigned model)
{
    DEFINE_STATIC_LOCAL(EnumerationHistogram, panningModelHistogram,
        ("WebAudio.PannerNode.PanningModel", 2));
    panningModelHistogram.count(model);

    switch (model) {
    case Panner::PanningModelEqualPower:
    case Panner::PanningModelHRTF:
        if (!m_panner.get() || model != m_panningModel) {
            // This synchronizes with process().
            MutexLocker processLocker(m_processLock);
            m_panner = Panner::create(model, sampleRate(), listener()->hrtfDatabaseLoader());
            m_panningModel = model;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

String PannerHandler::distanceModel() const
{
    switch (const_cast<PannerHandler*>(this)->m_distanceEffect.model()) {
    case DistanceEffect::ModelLinear:
        return "linear";
    case DistanceEffect::ModelInverse:
        return "inverse";
    case DistanceEffect::ModelExponential:
        return "exponential";
    default:
        ASSERT_NOT_REACHED();
        return "inverse";
    }
}

void PannerHandler::setDistanceModel(const String& model)
{
    if (model == "linear")
        setDistanceModel(DistanceEffect::ModelLinear);
    else if (model == "inverse")
        setDistanceModel(DistanceEffect::ModelInverse);
    else if (model == "exponential")
        setDistanceModel(DistanceEffect::ModelExponential);
}

bool PannerHandler::setDistanceModel(unsigned model)
{
    switch (model) {
    case DistanceEffect::ModelLinear:
    case DistanceEffect::ModelInverse:
    case DistanceEffect::ModelExponential:
        if (model != m_distanceModel) {
            // This synchronizes with process().
            MutexLocker processLocker(m_processLock);
            m_distanceEffect.setModel(static_cast<DistanceEffect::ModelType>(model), true);
            m_distanceModel = model;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

void PannerHandler::setRefDistance(double distance)
{
    if (refDistance() == distance)
        return;

    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    m_distanceEffect.setRefDistance(distance);
    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setMaxDistance(double distance)
{
    if (maxDistance() == distance)
        return;

    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    m_distanceEffect.setMaxDistance(distance);
    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setRolloffFactor(double factor)
{
    if (rolloffFactor() == factor)
        return;

    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    m_distanceEffect.setRolloffFactor(factor);
    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setConeInnerAngle(double angle)
{
    if (coneInnerAngle() == angle)
        return;

    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    m_coneEffect.setInnerAngle(angle);
    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setConeOuterAngle(double angle)
{
    if (coneOuterAngle() == angle)
        return;

    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    m_coneEffect.setOuterAngle(angle);
    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setConeOuterGain(double angle)
{
    if (coneOuterGain() == angle)
        return;

    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    m_coneEffect.setOuterGain(angle);
    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setPosition(float x, float y, float z)
{
    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);

    m_positionX->setValue(x);
    m_positionY->setValue(y);
    m_positionZ->setValue(z);

    markPannerAsDirty(PannerHandler::AzimuthElevationDirty | PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::setOrientation(float x, float y, float z)
{
    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);

    m_orientationX->setValue(x);
    m_orientationY->setValue(y);
    m_orientationZ->setValue(z);

    markPannerAsDirty(PannerHandler::DistanceConeGainDirty);
}

void PannerHandler::calculateAzimuthElevation(
    double* outAzimuth,
    double* outElevation,
    const FloatPoint3D& position,
    const FloatPoint3D& listenerPosition,
    const FloatPoint3D& listenerForward,
    const FloatPoint3D& listenerUp)
{
    double azimuth = 0.0;

    // Calculate the source-listener vector
    FloatPoint3D sourceListener = position - listenerPosition;

    // normalize() does nothing if the length of |sourceListener| is zero.
    sourceListener.normalize();

    // Align axes
    FloatPoint3D listenerRight = listenerForward.cross(listenerUp);
    listenerRight.normalize();

    FloatPoint3D listenerForwardNorm = listenerForward;
    listenerForwardNorm.normalize();

    FloatPoint3D up = listenerRight.cross(listenerForwardNorm);

    float upProjection = sourceListener.dot(up);

    FloatPoint3D projectedSource = sourceListener - upProjection * up;

    azimuth = rad2deg(projectedSource.angleBetween(listenerRight));
    fixNANs(azimuth); // avoid illegal values

    // Source  in front or behind the listener
    double frontBack = projectedSource.dot(listenerForwardNorm);
    if (frontBack < 0.0)
        azimuth = 360.0 - azimuth;

    // Make azimuth relative to "front" and not "right" listener vector
    if ((azimuth >= 0.0) && (azimuth <= 270.0))
        azimuth = 90.0 - azimuth;
    else
        azimuth = 450.0 - azimuth;

    // Elevation
    double elevation = 90 - rad2deg(sourceListener.angleBetween(up));
    fixNANs(elevation); // avoid illegal values

    if (elevation > 90.0)
        elevation = 180.0 - elevation;
    else if (elevation < -90.0)
        elevation = -180.0 - elevation;

    if (outAzimuth)
        *outAzimuth = azimuth;
    if (outElevation)
        *outElevation = elevation;
}

float PannerHandler::calculateDistanceConeGain(
    const FloatPoint3D& position,
    const FloatPoint3D& orientation,
    const FloatPoint3D& listenerPosition)
{
    double listenerDistance = position.distanceTo(listenerPosition);
    double distanceGain = m_distanceEffect.gain(listenerDistance);
    double coneGain = m_coneEffect.gain(position, orientation, listenerPosition);

    return float(distanceGain * coneGain);
}

void PannerHandler::azimuthElevation(double* outAzimuth, double* outElevation)
{
    ASSERT(context()->isAudioThread());

    // Calculate new azimuth and elevation if the panner or the listener changed
    // position or orientation in any way.
    if (isAzimuthElevationDirty() || listener()->isListenerDirty()) {
        calculateAzimuthElevation(
            &m_cachedAzimuth,
            &m_cachedElevation,
            position(),
            listener()->position(),
            listener()->orientation(),
            listener()->upVector());
        m_isAzimuthElevationDirty = false;
    }

    *outAzimuth = m_cachedAzimuth;
    *outElevation = m_cachedElevation;
}

float PannerHandler::distanceConeGain()
{
    ASSERT(context()->isAudioThread());

    // Calculate new distance and cone gain if the panner or the listener
    // changed position or orientation in any way.
    if (isDistanceConeGainDirty() || listener()->isListenerDirty()) {
        m_cachedDistanceConeGain = calculateDistanceConeGain(position(), orientation(), listener()->position());
        m_isDistanceConeGainDirty = false;
    }

    return m_cachedDistanceConeGain;
}

void PannerHandler::markPannerAsDirty(unsigned dirty)
{
    if (dirty & PannerHandler::AzimuthElevationDirty)
        m_isAzimuthElevationDirty = true;

    if (dirty & PannerHandler::DistanceConeGainDirty)
        m_isDistanceConeGainDirty = true;
}

void PannerHandler::setChannelCount(unsigned long channelCount, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AbstractAudioContext::AutoLocker locker(context());

    // A PannerNode only supports 1 or 2 channels
    if (channelCount > 0 && channelCount <= 2) {
        if (m_channelCount != channelCount) {
            m_channelCount = channelCount;
            if (m_channelCountMode != Max)
                updateChannelsForInputs();
        }
    } else {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange<unsigned long>(
                "channelCount",
                channelCount,
                1,
                ExceptionMessages::InclusiveBound,
                2,
                ExceptionMessages::InclusiveBound));
    }
}

void PannerHandler::setChannelCountMode(const String& mode, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AbstractAudioContext::AutoLocker locker(context());

    ChannelCountMode oldMode = m_channelCountMode;

    if (mode == "clamped-max") {
        m_newChannelCountMode = ClampedMax;
    } else if (mode == "explicit") {
        m_newChannelCountMode = Explicit;
    } else if (mode == "max") {
        // This is not supported for a PannerNode, which can only handle 1 or 2 channels.
        exceptionState.throwDOMException(
            NotSupportedError,
                "Panner: 'max' is not allowed");
        m_newChannelCountMode = oldMode;
    } else {
        // Do nothing for other invalid values.
        m_newChannelCountMode = oldMode;
    }

    if (m_newChannelCountMode != oldMode)
        context()->deferredTaskHandler().addChangedChannelCountMode(this);
}

bool PannerHandler::hasSampleAccurateValues() const
{
    return m_positionX->hasSampleAccurateValues()
        || m_positionY->hasSampleAccurateValues()
        || m_positionZ->hasSampleAccurateValues()
        || m_orientationX->hasSampleAccurateValues()
        || m_orientationY->hasSampleAccurateValues()
        || m_orientationZ->hasSampleAccurateValues();
}

void PannerHandler::updateDirtyState()
{
    DCHECK(context()->isAudioThread());

    FloatPoint3D currentPosition = position();
    FloatPoint3D currentOrientation = orientation();

    bool hasMoved = currentPosition != m_lastPosition
        || currentOrientation != m_lastOrientation;

    if (hasMoved) {
        m_lastPosition = currentPosition;
        m_lastOrientation = currentOrientation;

        markPannerAsDirty(PannerHandler::AzimuthElevationDirty | PannerHandler::DistanceConeGainDirty);
    }
}
// ----------------------------------------------------------------

PannerNode::PannerNode(AbstractAudioContext& context)
    : AudioNode(context)
    , m_positionX(AudioParam::create(context, ParamTypePannerPositionX, 0.0))
    , m_positionY(AudioParam::create(context, ParamTypePannerPositionY, 0.0))
    , m_positionZ(AudioParam::create(context, ParamTypePannerPositionZ, 0.0))
    , m_orientationX(AudioParam::create(context, ParamTypePannerOrientationX, 1.0))
    , m_orientationY(AudioParam::create(context, ParamTypePannerOrientationY, 0.0))
    , m_orientationZ(AudioParam::create(context, ParamTypePannerOrientationZ, 0.0))
{
    setHandler(PannerHandler::create(
        *this,
        context.sampleRate(),
        m_positionX->handler(),
        m_positionY->handler(),
        m_positionZ->handler(),
        m_orientationX->handler(),
        m_orientationY->handler(),
        m_orientationZ->handler()));
}

PannerNode* PannerNode::create(AbstractAudioContext& context, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    return new PannerNode(context);
}

PannerHandler& PannerNode::pannerHandler() const
{
    return static_cast<PannerHandler&>(handler());
}

String PannerNode::panningModel() const
{
    return pannerHandler().panningModel();
}

void PannerNode::setPanningModel(const String& model)
{
    pannerHandler().setPanningModel(model);
}

void PannerNode::setPosition(float x, float y, float z)
{
    pannerHandler().setPosition(x, y, z);
}

void PannerNode::setOrientation(float x, float y, float z)
{
    pannerHandler().setOrientation(x, y, z);
}

void PannerNode::setVelocity(float x, float y, float z)
{
    // The velocity is not used internally and cannot be read back by scripts,
    // so it can be ignored entirely.
}

String PannerNode::distanceModel() const
{
    return pannerHandler().distanceModel();
}

void PannerNode::setDistanceModel(const String& model)
{
    pannerHandler().setDistanceModel(model);
}

double PannerNode::refDistance() const
{
    return pannerHandler().refDistance();
}

void PannerNode::setRefDistance(double distance)
{
    pannerHandler().setRefDistance(distance);
}

double PannerNode::maxDistance() const
{
    return pannerHandler().maxDistance();
}

void PannerNode::setMaxDistance(double distance)
{
    pannerHandler().setMaxDistance(distance);
}

double PannerNode::rolloffFactor() const
{
    return pannerHandler().rolloffFactor();
}

void PannerNode::setRolloffFactor(double factor)
{
    pannerHandler().setRolloffFactor(factor);
}

double PannerNode::coneInnerAngle() const
{
    return pannerHandler().coneInnerAngle();
}

void PannerNode::setConeInnerAngle(double angle)
{
    pannerHandler().setConeInnerAngle(angle);
}

double PannerNode::coneOuterAngle() const
{
    return pannerHandler().coneOuterAngle();
}

void PannerNode::setConeOuterAngle(double angle)
{
    pannerHandler().setConeOuterAngle(angle);
}

double PannerNode::coneOuterGain() const
{
    return pannerHandler().coneOuterGain();
}

void PannerNode::setConeOuterGain(double gain)
{
    pannerHandler().setConeOuterGain(gain);
}

DEFINE_TRACE(PannerNode)
{
    visitor->trace(m_positionX);
    visitor->trace(m_positionY);
    visitor->trace(m_positionZ);

    visitor->trace(m_orientationX);
    visitor->trace(m_orientationY);
    visitor->trace(m_orientationZ);

    AudioNode::trace(visitor);
}

} // namespace blink
