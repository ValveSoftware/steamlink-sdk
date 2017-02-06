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

#include "modules/webaudio/AudioParam.h"

#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "platform/FloatConversion.h"
#include "platform/Histogram.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/MathExtras.h"

namespace blink {

const double AudioParamHandler::DefaultSmoothingConstant = 0.05;
const double AudioParamHandler::SnapThreshold = 0.001;

AudioParamHandler::AudioParamHandler(AbstractAudioContext& context, AudioParamType paramType, double defaultValue, float minValue, float maxValue)
    : AudioSummingJunction(context.deferredTaskHandler())
    , m_paramType(paramType)
    , m_intrinsicValue(defaultValue)
    , m_defaultValue(defaultValue)
    , m_minValue(minValue)
    , m_maxValue(maxValue)
{
    // The destination MUST exist because we need the destination handler for the AudioParam.
    RELEASE_ASSERT(context.destination());

    m_destinationHandler = &context.destination()->audioDestinationHandler();
    m_timeline.setSmoothedValue(defaultValue);
}

AudioDestinationHandler& AudioParamHandler::destinationHandler() const
{
    return *m_destinationHandler;
}

void AudioParamHandler::setParamType(AudioParamType paramType)
{
    m_paramType = paramType;
}

String AudioParamHandler::getParamName() const
{
    // The returned string should be the name of the node and the name of the AudioParam for
    // that node.
    switch (m_paramType) {
    case ParamTypeAudioBufferSourcePlaybackRate:
        return "AudioBufferSource.playbackRate";
    case ParamTypeAudioBufferSourceDetune:
        return "AudioBufferSource.detune";
    case ParamTypeBiquadFilterFrequency:
        return "BiquadFilter.frequency";
    case ParamTypeBiquadFilterQ:
    case ParamTypeBiquadFilterQLowpass:
    case ParamTypeBiquadFilterQHighpass:
        // We don't really need separate names for the Q parameter for lowpass and highpass filters.
        // The difference is only for the histograms.
        return "BiquadFilter.Q";
    case ParamTypeBiquadFilterGain:
        return "BiquadFilter.gain";
    case ParamTypeBiquadFilterDetune:
        return "BiquadFilter.detune";
    case ParamTypeDelayDelayTime:
        return "Delay.delayTime";
    case ParamTypeDynamicsCompressorThreshold:
        return "DynamicsCompressor.threshold";
    case ParamTypeDynamicsCompressorKnee:
        return "DynamicsCompressor.knee";
    case ParamTypeDynamicsCompressorRatio:
        return "DynamicsCompressor.ratio";
    case ParamTypeDynamicsCompressorAttack:
        return "DynamicsCompressor.attack";
    case ParamTypeDynamicsCompressorRelease:
        return "DynamicsCompressor.release";
    case ParamTypeGainGain:
        return "Gain.gain";
    case ParamTypeOscillatorFrequency:
        return "Oscillator.frequency";
    case ParamTypeOscillatorDetune:
        return "Oscillator.detune";
    case ParamTypeStereoPannerPan:
        return "StereoPanner.pan";
    case ParamTypePannerPositionX:
        return "Panner.positionX";
    case ParamTypePannerPositionY:
        return "Panner.positionY";
    case ParamTypePannerPositionZ:
        return "Panner.positionZ";
    case ParamTypePannerOrientationX:
        return "Panner.orientationX";
    case ParamTypePannerOrientationY:
        return "Panner.orientationY";
    case ParamTypePannerOrientationZ:
        return "Panner.orientationZ";
    case ParamTypeAudioListenerPositionX:
        return "AudioListener.positionX";
    case ParamTypeAudioListenerPositionY:
        return "AudioListener.positionY";
    case ParamTypeAudioListenerPositionZ:
        return "AudioListener.positionZ";
    case ParamTypeAudioListenerForwardX:
        return "AudioListener.forwardX";
    case ParamTypeAudioListenerForwardY:
        return "AudioListener.forwardY";
    case ParamTypeAudioListenerForwardZ:
        return "AudioListener.forwardZ";
    case ParamTypeAudioListenerUpX:
        return "AudioListener.upX";
    case ParamTypeAudioListenerUpY:
        return "AudioListener.upY";
    case ParamTypeAudioListenerUpZ:
        return "AudioListener.upZ";
    };

    NOTREACHED();
    return "UnknownNode.unknownAudioParam";
}

float AudioParamHandler::value()
{
    // Update value for timeline.
    float v = intrinsicValue();
    if (deferredTaskHandler().isAudioThread()) {
        bool hasValue;
        float timelineValue = m_timeline.valueForContextTime(destinationHandler(), v, hasValue, minValue(), maxValue());

        if (hasValue)
            v = timelineValue;
    }

    setIntrinsicValue(v);
    return v;
}

void AudioParamHandler::setIntrinsicValue(float newValue)
{
    newValue = clampTo(newValue, m_minValue, m_maxValue);
    noBarrierStore(&m_intrinsicValue, newValue);
}

void AudioParamHandler::setValue(float value)
{
    setIntrinsicValue(value);
    updateHistograms(value);
}

float AudioParamHandler::smoothedValue()
{
    return m_timeline.smoothedValue();
}

bool AudioParamHandler::smooth()
{
    // If values have been explicitly scheduled on the timeline, then use the exact value.
    // Smoothing effectively is performed by the timeline.
    bool useTimelineValue = false;
    float value = m_timeline.valueForContextTime(destinationHandler(), intrinsicValue(), useTimelineValue, minValue(), maxValue());


    float smoothedValue = m_timeline.smoothedValue();
    if (smoothedValue == value) {
        // Smoothed value has already approached and snapped to value.
        setIntrinsicValue(value);
        return true;
    }

    if (useTimelineValue) {
        m_timeline.setSmoothedValue(value);
    } else {
        // Dezipper - exponential approach.
        smoothedValue += (value - smoothedValue) * DefaultSmoothingConstant;

        // If we get close enough then snap to actual value.
        // FIXME: the threshold needs to be adjustable depending on range - but
        // this is OK general purpose value.
        if (fabs(smoothedValue - value) < SnapThreshold)
            smoothedValue = value;
        m_timeline.setSmoothedValue(smoothedValue);
    }

    setIntrinsicValue(value);
    return false;
}

float AudioParamHandler::finalValue()
{
    float value = intrinsicValue();
    calculateFinalValues(&value, 1, false);
    return value;
}

void AudioParamHandler::calculateSampleAccurateValues(float* values, unsigned numberOfValues)
{
    bool isSafe = deferredTaskHandler().isAudioThread() && values && numberOfValues;
    ASSERT(isSafe);
    if (!isSafe)
        return;

    calculateFinalValues(values, numberOfValues, true);
}

void AudioParamHandler::calculateFinalValues(float* values, unsigned numberOfValues, bool sampleAccurate)
{
    bool isGood = deferredTaskHandler().isAudioThread() && values && numberOfValues;
    ASSERT(isGood);
    if (!isGood)
        return;

    // The calculated result will be the "intrinsic" value summed with all audio-rate connections.

    if (sampleAccurate) {
        // Calculate sample-accurate (a-rate) intrinsic values.
        calculateTimelineValues(values, numberOfValues);
    } else {
        // Calculate control-rate (k-rate) intrinsic value.
        bool hasValue;
        float value = intrinsicValue();
        float timelineValue = m_timeline.valueForContextTime(destinationHandler(), value, hasValue, minValue(), maxValue());

        if (hasValue)
            value = timelineValue;

        values[0] = value;
        setIntrinsicValue(value);
    }

    // Now sum all of the audio-rate connections together (unity-gain summing junction).
    // Note that connections would normally be mono, but we mix down to mono if necessary.
    RefPtr<AudioBus> summingBus = AudioBus::create(1, numberOfValues, false);
    summingBus->setChannelMemory(0, values, numberOfValues);

    for (unsigned i = 0; i < numberOfRenderingConnections(); ++i) {
        AudioNodeOutput* output = renderingOutput(i);
        ASSERT(output);

        // Render audio from this output.
        AudioBus* connectionBus = output->pull(0, AudioHandler::ProcessingSizeInFrames);

        // Sum, with unity-gain.
        summingBus->sumFrom(*connectionBus);
    }
}

void AudioParamHandler::calculateTimelineValues(float* values, unsigned numberOfValues)
{
    // Calculate values for this render quantum.  Normally numberOfValues will
    // equal to AudioHandler::ProcessingSizeInFrames (the render quantum size).
    double sampleRate = destinationHandler().sampleRate();
    size_t startFrame = destinationHandler().currentSampleFrame();
    size_t endFrame = startFrame + numberOfValues;

    // Note we're running control rate at the sample-rate.
    // Pass in the current value as default value.
    setIntrinsicValue(m_timeline.valuesForFrameRange(startFrame, endFrame, intrinsicValue(), values, numberOfValues, sampleRate, sampleRate, minValue(), maxValue()));
}

void AudioParamHandler::connect(AudioNodeOutput& output)
{
    ASSERT(deferredTaskHandler().isGraphOwner());

    if (m_outputs.contains(&output))
        return;

    output.addParam(*this);
    m_outputs.add(&output);
    changedOutputs();
}

void AudioParamHandler::disconnect(AudioNodeOutput& output)
{
    ASSERT(deferredTaskHandler().isGraphOwner());

    if (m_outputs.contains(&output)) {
        m_outputs.remove(&output);
        changedOutputs();
        output.removeParam(*this);
    }
}

int AudioParamHandler::computeQHistogramValue(float newValue) const
{
    // For the Q value, assume a useful range is [0, 25] and that 0.25 dB resolution is good enough.
    // Then, we can map the floating point Q value (in dB) to an integer just by multipling by 4 and
    // rounding.
    newValue = clampTo(newValue, 0.0, 25.0);
    return static_cast<int>(4 * newValue + 0.5);
}

void AudioParamHandler::updateHistograms(float newValue)
{
    switch (m_paramType) {
    case ParamTypeBiquadFilterQLowpass:
        {
            // The histogram for the Q value for a lowpass biquad filter.
            DEFINE_STATIC_LOCAL(SparseHistogram, lowpassQHistogram, ("WebAudio.BiquadFilter.Q.Lowpass"));

            lowpassQHistogram.sample(computeQHistogramValue(newValue));
        }
        break;
    case ParamTypeBiquadFilterQHighpass:
        {
            // The histogram for the Q value for a highpass biquad filter.
            DEFINE_STATIC_LOCAL(SparseHistogram, highpassQHistogram, ("WebAudio.BiquadFilter.Q.Highpass"));

            highpassQHistogram.sample(computeQHistogramValue(newValue));
        }
        break;
    default:
        // Nothing to do for all other types.
        break;
    }
}

// ----------------------------------------------------------------

AudioParam::AudioParam(AbstractAudioContext& context, AudioParamType paramType, double defaultValue, float minValue, float maxValue)
    : m_handler(AudioParamHandler::create(context, paramType, defaultValue, minValue, maxValue))
    , m_context(context)
{
}

AudioParam* AudioParam::create(AbstractAudioContext& context, AudioParamType paramType, double defaultValue)
{
    // Default nominal range is most negative float to most positive.  This basically means any
    // value is valid, except that floating-point infinities are excluded.
    float limit = std::numeric_limits<float>::max();
    return new AudioParam(context, paramType, defaultValue, -limit, limit);
}

AudioParam* AudioParam::create(AbstractAudioContext& context, AudioParamType paramType, double defaultValue, float minValue, float maxValue)
{
    DCHECK_LE(minValue, maxValue);
    return new AudioParam(context, paramType, defaultValue, minValue, maxValue);
}

DEFINE_TRACE(AudioParam)
{
    visitor->trace(m_context);
}

float AudioParam::value() const
{
    return handler().value();
}

void AudioParam::warnIfOutsideRange(const String& paramMethod, float value)
{
    if (value < minValue() || value > maxValue()) {
        context()->getExecutionContext()->addConsoleMessage(
            ConsoleMessage::create(
                JSMessageSource,
                WarningMessageLevel,
                handler().getParamName()
                + "."
                + paramMethod
                + " "
                + String::number(value)
                + " outside nominal range ["
                + String::number(minValue()) + ", " + String::number(maxValue())
                + "]; value will be clamped."));
    }
}

void AudioParam::setValue(float value)
{
    warnIfOutsideRange("value", value);
    handler().setValue(value);
}

float AudioParam::defaultValue() const
{
    return handler().defaultValue();
}

float AudioParam::minValue() const
{
    return handler().minValue();
}

float AudioParam::maxValue() const
{
    return handler().maxValue();
}

void AudioParam::setParamType(AudioParamType paramType)
{
    handler().setParamType(paramType);
}

AudioParam* AudioParam::setValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    warnIfOutsideRange("setValueAtTime value", value);
    handler().timeline().setValueAtTime(value, time, exceptionState);
    handler().updateHistograms(value);
    return this;
}

AudioParam* AudioParam::linearRampToValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    warnIfOutsideRange("linearRampToValueAtTime value", value);
    handler().timeline().linearRampToValueAtTime(
        value, time, handler().intrinsicValue(), context()->currentTime(), exceptionState);

    // This is probably the best we can do for the histogram.  We don't want to run the automation
    // to get all the values and use them to update the histogram.
    handler().updateHistograms(value);

    return this;
}

AudioParam* AudioParam::exponentialRampToValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    warnIfOutsideRange("exponentialRampToValue value", value);
    handler().timeline().exponentialRampToValueAtTime(value, time, handler().intrinsicValue(), context()->currentTime(), exceptionState);

    // This is probably the best we can do for the histogram.  We don't want to run the automation
    // to get all the values and use them to update the histogram.
    handler().updateHistograms(value);

    return this;
}

AudioParam* AudioParam::setTargetAtTime(float target, double time, double timeConstant, ExceptionState& exceptionState)
{
    warnIfOutsideRange("setTargetAtTime value", target);
    handler().timeline().setTargetAtTime(target, time, timeConstant, exceptionState);

    // Don't update the histogram here.  It's not clear in normal usage if the parameter value will
    // actually reach |target|.
    return this;
}

AudioParam* AudioParam::setValueCurveAtTime(DOMFloat32Array* curve, double time, double duration, ExceptionState& exceptionState)
{
    float* curveData = curve->data();
    float min = minValue();
    float max = maxValue();

    // First, find any non-finite value in the curve and throw an exception if
    // there are any.
    for (unsigned k = 0; k < curve->length(); ++k) {
        float value = curveData[k];

        if (!std::isfinite(value)) {
            exceptionState.throwDOMException(
                V8TypeError,
                "The provided float value for the curve at element "
                + String::number(k)
                + " is non-finite: "
                + String::number(value));
            return nullptr;
        }
    }

    // Second, find the first value in the curve (if any) that is outside the
    // nominal range.  It's probably not necessary to produce a warning on every
    // value outside the nominal range.
    for (unsigned k = 0; k < curve->length(); ++k) {
        float value = curveData[k];

        if (value < min || value > max) {
            warnIfOutsideRange("setValueCurveAtTime value", value);
            break;
        }
    }

    handler().timeline().setValueCurveAtTime(curve, time, duration, exceptionState);

    // We could update the histogram with every value in the curve, due to interpolation, we'll
    // probably be missing many values.  So we don't update the histogram.  setValueCurveAtTime is
    // probably a fairly rare method anyway.
    return this;
}

AudioParam* AudioParam::cancelScheduledValues(double startTime, ExceptionState& exceptionState)
{
    handler().timeline().cancelScheduledValues(startTime, exceptionState);
    return this;
}

} // namespace blink

