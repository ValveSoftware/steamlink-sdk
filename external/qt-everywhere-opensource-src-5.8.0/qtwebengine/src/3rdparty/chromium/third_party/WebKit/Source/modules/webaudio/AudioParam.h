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

#ifndef AudioParam_h
#define AudioParam_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/AudioParamTimeline.h"
#include "modules/webaudio/AudioSummingJunction.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/text/WTFString.h"
#include <sys/types.h>

namespace blink {

class AudioNodeOutput;

// Each AudioParam gets an identifier here.  This is mostly for instrospection if warnings or
// other messages need to be printed. It's useful to know what the AudioParam represents.  The
// name should include the node type and the name of the AudioParam.
enum AudioParamType {
    ParamTypeAudioBufferSourcePlaybackRate,
    ParamTypeAudioBufferSourceDetune,
    ParamTypeBiquadFilterFrequency,
    ParamTypeBiquadFilterQ,
    ParamTypeBiquadFilterQLowpass,
    ParamTypeBiquadFilterQHighpass,
    ParamTypeBiquadFilterGain,
    ParamTypeBiquadFilterDetune,
    ParamTypeDelayDelayTime,
    ParamTypeDynamicsCompressorThreshold,
    ParamTypeDynamicsCompressorKnee,
    ParamTypeDynamicsCompressorRatio,
    ParamTypeDynamicsCompressorAttack,
    ParamTypeDynamicsCompressorRelease,
    ParamTypeGainGain,
    ParamTypeOscillatorFrequency,
    ParamTypeOscillatorDetune,
    ParamTypeStereoPannerPan,
    ParamTypePannerPositionX,
    ParamTypePannerPositionY,
    ParamTypePannerPositionZ,
    ParamTypePannerOrientationX,
    ParamTypePannerOrientationY,
    ParamTypePannerOrientationZ,
    ParamTypeAudioListenerPositionX,
    ParamTypeAudioListenerPositionY,
    ParamTypeAudioListenerPositionZ,
    ParamTypeAudioListenerForwardX,
    ParamTypeAudioListenerForwardY,
    ParamTypeAudioListenerForwardZ,
    ParamTypeAudioListenerUpX,
    ParamTypeAudioListenerUpY,
    ParamTypeAudioListenerUpZ,
};

// AudioParamHandler is an actual implementation of web-exposed AudioParam
// interface. Each of AudioParam object creates and owns an AudioParamHandler,
// and it is responsible for all of AudioParam tasks. An AudioParamHandler
// object is owned by the originator AudioParam object, and some audio
// processing classes have additional references. An AudioParamHandler can
// outlive the owner AudioParam, and it never dies before the owner AudioParam
// dies.
class AudioParamHandler final : public ThreadSafeRefCounted<AudioParamHandler>, public AudioSummingJunction {
public:
    AudioParamType getParamType() const { return m_paramType; }
    void setParamType(AudioParamType);
    // Return a nice name for the AudioParam.
    String getParamName() const;

    static const double DefaultSmoothingConstant;
    static const double SnapThreshold;

    static PassRefPtr<AudioParamHandler> create(
        AbstractAudioContext& context,
        AudioParamType paramType,
        double defaultValue,
        float minValue,
        float maxValue)
    {
        return adoptRef(new AudioParamHandler(context, paramType, defaultValue, minValue, maxValue));
    }

    // This should be used only in audio rendering thread.
    AudioDestinationHandler& destinationHandler() const;

    // AudioSummingJunction
    void didUpdate() override { }

    AudioParamTimeline& timeline() { return m_timeline; }

    // Intrinsic value.
    float value();
    void setValue(float);

    // Final value for k-rate parameters, otherwise use calculateSampleAccurateValues() for a-rate.
    // Must be called in the audio thread.
    float finalValue();

    float defaultValue() const { return static_cast<float>(m_defaultValue); }
    float minValue() const { return m_minValue; }
    float maxValue() const { return m_maxValue; }

    // Value smoothing:

    // When a new value is set with setValue(), in our internal use of the parameter we don't immediately jump to it.
    // Instead we smoothly approach this value to avoid glitching.
    float smoothedValue();

    // Smoothly exponentially approaches to (de-zippers) the desired value.
    // Returns true if smoothed value has already snapped exactly to value.
    bool smooth();

    void resetSmoothedValue() { m_timeline.setSmoothedValue(intrinsicValue()); }

    bool hasSampleAccurateValues() { return m_timeline.hasValues() || numberOfRenderingConnections(); }

    // Calculates numberOfValues parameter values starting at the context's current time.
    // Must be called in the context's render thread.
    void calculateSampleAccurateValues(float* values, unsigned numberOfValues);

    // Connect an audio-rate signal to control this parameter.
    void connect(AudioNodeOutput&);
    void disconnect(AudioNodeOutput&);

    float intrinsicValue() const { return noBarrierLoad(&m_intrinsicValue); }

    // Update any histograms with the given value.
    void updateHistograms(float newValue);

private:
    AudioParamHandler(AbstractAudioContext&, AudioParamType, double defaultValue, float min, float max);

    void warnIfOutsideRange(float value, float minValue, float maxValue);

    // sampleAccurate corresponds to a-rate (audio rate) vs. k-rate in the Web Audio specification.
    void calculateFinalValues(float* values, unsigned numberOfValues, bool sampleAccurate);
    void calculateTimelineValues(float* values, unsigned numberOfValues);

    int computeQHistogramValue(float) const;

    // The type of AudioParam, indicating what this AudioParam represents and what node it belongs
    // to.  Mostly for informational purposes and doesn't affect implementation.
    AudioParamType m_paramType;

    // Intrinsic value
    float m_intrinsicValue;
    void setIntrinsicValue(float newValue);

    float m_defaultValue;

    // Nominal range for the value
    float m_minValue;
    float m_maxValue;

    AudioParamTimeline m_timeline;

    // The destination node used to get necessary information like the smaple rate and context time.
    RefPtr<AudioDestinationHandler> m_destinationHandler;
};

// AudioParam class represents web-exposed AudioParam interface.
class AudioParam final : public GarbageCollectedFinalized<AudioParam>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static AudioParam* create(AbstractAudioContext&, AudioParamType, double defaultValue);
    static AudioParam* create(AbstractAudioContext&, AudioParamType, double defaultValue, float minValue, float maxValue);

    DECLARE_TRACE();
    // |handler| always returns a valid object.
    AudioParamHandler& handler() const { return *m_handler; }
    // |context| always returns a valid object.
    AbstractAudioContext* context() const { return m_context; }

    AudioParamType getParamType() const { return handler().getParamType(); }
    void setParamType(AudioParamType);
    String getParamName() const;

    float value() const;
    void setValue(float);
    float defaultValue() const;

    float minValue() const;
    float maxValue() const;

    AudioParam* setValueAtTime(float value, double time, ExceptionState&);
    AudioParam* linearRampToValueAtTime(float value, double time, ExceptionState&);
    AudioParam* exponentialRampToValueAtTime(float value, double time, ExceptionState&);
    AudioParam* setTargetAtTime(float target, double time, double timeConstant, ExceptionState&);
    AudioParam* setValueCurveAtTime(DOMFloat32Array* curve, double time, double duration, ExceptionState&);
    AudioParam* cancelScheduledValues(double startTime, ExceptionState&);

private:
    AudioParam(AbstractAudioContext&, AudioParamType, double defaultValue, float min, float max);

    void warnIfOutsideRange(const String& paramMethd, float value);

    RefPtr<AudioParamHandler> m_handler;
    Member<AbstractAudioContext> m_context;
};

} // namespace blink

#endif // AudioParam_h
