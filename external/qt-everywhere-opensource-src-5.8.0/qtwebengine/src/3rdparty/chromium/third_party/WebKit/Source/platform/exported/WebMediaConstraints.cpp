/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebMediaConstraints.h"

#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#include <math.h>

namespace blink {

namespace {

template <typename T>
void maybeEmitNamedValue(StringBuilder& builder, bool emit, const char* name, T value)
{
    if (!emit)
        return;
    if (builder.length() > 1)
        builder.append(", ");
    builder.append(name);
    builder.append(": ");
    builder.appendNumber(value);
}

void maybeEmitNamedBoolean(StringBuilder& builder, bool emit, const char* name, bool value)
{
    if (!emit)
        return;
    if (builder.length() > 1)
        builder.append(", ");
    builder.append(name);
    builder.append(": ");
    if (value)
        builder.append("true");
    else
        builder.append("false");
}

} // namespace

class WebMediaConstraintsPrivate final : public RefCounted<WebMediaConstraintsPrivate> {
public:
    static PassRefPtr<WebMediaConstraintsPrivate> create();
    static PassRefPtr<WebMediaConstraintsPrivate> create(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced);

    bool isEmpty() const;
    const WebMediaTrackConstraintSet& basic() const;
    const WebVector<WebMediaTrackConstraintSet>& advanced() const;
    const String toString() const;

private:
    WebMediaConstraintsPrivate(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced);

    WebMediaTrackConstraintSet m_basic;
    WebVector<WebMediaTrackConstraintSet> m_advanced;
};

PassRefPtr<WebMediaConstraintsPrivate> WebMediaConstraintsPrivate::create()
{
    WebMediaTrackConstraintSet basic;
    WebVector<WebMediaTrackConstraintSet> advanced;
    return adoptRef(new WebMediaConstraintsPrivate(basic, advanced));
}

PassRefPtr<WebMediaConstraintsPrivate> WebMediaConstraintsPrivate::create(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced)
{
    return adoptRef(new WebMediaConstraintsPrivate(basic, advanced));
}

WebMediaConstraintsPrivate::WebMediaConstraintsPrivate(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced)
    : m_basic(basic)
    , m_advanced(advanced)
{
}

bool WebMediaConstraintsPrivate::isEmpty() const
{
    // TODO(hta): When generating advanced constraints, make sure no empty
    // elements can be added to the m_advanced vector.
    return m_basic.isEmpty() && m_advanced.isEmpty();
}

const WebMediaTrackConstraintSet& WebMediaConstraintsPrivate::basic() const
{
    return m_basic;
}

const WebVector<WebMediaTrackConstraintSet>& WebMediaConstraintsPrivate::advanced() const
{
    return m_advanced;
}

const String WebMediaConstraintsPrivate::toString() const
{
    StringBuilder builder;
    if (!isEmpty()) {
        builder.append('{');
        builder.append(basic().toString());
        if (!advanced().isEmpty()) {
            if (builder.length() > 1)
                builder.append(", ");
            builder.append("advanced: [");
            bool first = true;
            for (const auto& constraintSet : advanced()) {
                if (!first)
                    builder.append(", ");
                builder.append('{');
                builder.append(constraintSet.toString());
                builder.append('}');
                first = false;
            }
            builder.append(']');
        }
        builder.append('}');
    }
    return builder.toString();
}

// *Constraints

BaseConstraint::BaseConstraint(const char* name)
    : m_name(name)
{
}

BaseConstraint::~BaseConstraint()
{
}

LongConstraint::LongConstraint(const char* name)
    : BaseConstraint(name)
    , m_min()
    , m_max()
    , m_exact()
    , m_ideal()
    , m_hasMin(false)
    , m_hasMax(false)
    , m_hasExact(false)
    , m_hasIdeal(false)
{
}

bool LongConstraint::matches(long value) const
{
    if (m_hasMin && value < m_min) {
        return false;
    }
    if (m_hasMax && value > m_max) {
        return false;
    }
    if (m_hasExact && value != m_exact) {
        return false;
    }
    return true;
}

bool LongConstraint::isEmpty() const
{
    return !m_hasMin && !m_hasMax && !m_hasExact && !m_hasIdeal;
}

bool LongConstraint::hasMandatory() const
{
    return m_hasMin || m_hasMax || m_hasExact;
}

WebString LongConstraint::toString() const
{
    StringBuilder builder;
    builder.append('{');
    maybeEmitNamedValue(builder, m_hasMin, "min", m_min);
    maybeEmitNamedValue(builder, m_hasMax, "max", m_max);
    maybeEmitNamedValue(builder, m_hasExact, "exact", m_exact);
    maybeEmitNamedValue(builder, m_hasIdeal, "ideal", m_ideal);
    builder.append('}');
    return builder.toString();
}

const double DoubleConstraint::kConstraintEpsilon = 0.00001;

DoubleConstraint::DoubleConstraint(const char* name)
    : BaseConstraint(name)
    , m_min()
    , m_max()
    , m_exact()
    , m_ideal()
    , m_hasMin(false)
    , m_hasMax(false)
    , m_hasExact(false)
    , m_hasIdeal(false)
{
}

bool DoubleConstraint::matches(double value) const
{
    if (m_hasMin && value < m_min - kConstraintEpsilon) {
        return false;
    }
    if (m_hasMax && value > m_max + kConstraintEpsilon) {
        return false;
    }
    if (m_hasExact && fabs(static_cast<double>(value) - m_exact) > kConstraintEpsilon) {
        return false;
    }
    return true;
}

bool DoubleConstraint::isEmpty() const
{
    return !m_hasMin && !m_hasMax && !m_hasExact && !m_hasIdeal;
}

bool DoubleConstraint::hasMandatory() const
{
    return m_hasMin || m_hasMax || m_hasExact;
}

WebString DoubleConstraint::toString() const
{
    StringBuilder builder;
    builder.append('{');
    maybeEmitNamedValue(builder, m_hasMin, "min", m_min);
    maybeEmitNamedValue(builder, m_hasMax, "max", m_max);
    maybeEmitNamedValue(builder, m_hasExact, "exact", m_exact);
    maybeEmitNamedValue(builder, m_hasIdeal, "ideal", m_ideal);
    builder.append('}');
    return builder.toString();
}

StringConstraint::StringConstraint(const char* name)
    : BaseConstraint(name)
    , m_exact()
    , m_ideal()
{
}

bool StringConstraint::matches(WebString value) const
{
    if (m_exact.isEmpty()) {
        return true;
    }
    for (const auto& choice : m_exact) {
        if (value == choice) {
            return true;
        }
    }
    return false;
}

bool StringConstraint::isEmpty() const
{
    return m_exact.isEmpty() && m_ideal.isEmpty();
}

bool StringConstraint::hasMandatory() const
{
    return !m_exact.isEmpty();
}

const WebVector<WebString>& StringConstraint::exact() const
{
    return m_exact;
}

const WebVector<WebString>& StringConstraint::ideal() const
{
    return m_ideal;
}

WebString StringConstraint::toString() const
{
    StringBuilder builder;
    builder.append('{');
    if (!m_ideal.isEmpty()) {
        builder.append("ideal: [");
        bool first = true;
        for (const auto& iter : m_ideal) {
            if (!first)
                builder.append(", ");
            builder.append('"');
            builder.append(iter);
            builder.append('"');
            first = false;
        }
        builder.append(']');
    }
    if (!m_exact.isEmpty()) {
        if (builder.length() > 1)
            builder.append(", ");
        builder.append("exact: [");
        bool first = true;
        for (const auto& iter : m_exact) {
            if (!first)
                builder.append(", ");
            builder.append('"');
            builder.append(iter);
            builder.append('"');
        }
        builder.append(']');
    }
    builder.append('}');
    return builder.toString();
}

BooleanConstraint::BooleanConstraint(const char* name)
    : BaseConstraint(name)
    , m_ideal(false)
    , m_exact(false)
    , m_hasIdeal(false)
    , m_hasExact(false)
{
}

bool BooleanConstraint::matches(bool value) const
{
    if (m_hasExact && static_cast<bool>(m_exact) != value) {
        return false;
    }
    return true;
}

bool BooleanConstraint::isEmpty() const
{
    return !m_hasIdeal && !m_hasExact;
}

bool BooleanConstraint::hasMandatory() const
{
    return m_hasExact;
}

WebString BooleanConstraint::toString() const
{
    StringBuilder builder;
    builder.append('{');
    maybeEmitNamedBoolean(builder, m_hasExact, "exact", exact());
    maybeEmitNamedBoolean(builder, m_hasIdeal, "ideal", ideal());
    builder.append('}');
    return builder.toString();
}

WebMediaTrackConstraintSet::WebMediaTrackConstraintSet()
    : width("width")
    , height("height")
    , aspectRatio("aspectRatio")
    , frameRate("frameRate")
    , facingMode("facingMode")
    , volume("volume")
    , sampleRate("sampleRate")
    , sampleSize("sampleSize")
    , echoCancellation("echoCancellation")
    , latency("latency")
    , channelCount("channelCount")
    , deviceId("deviceId")
    , groupId("groupId")
    , mediaStreamSource("mediaStreamSource")
    , renderToAssociatedSink("chromeRenderToAssociatedSink")
    , hotwordEnabled("hotwordEnabled")
    , googEchoCancellation("googEchoCancellation")
    , googExperimentalEchoCancellation("googExperimentalEchoCancellation")
    , googAutoGainControl("googAutoGainControl")
    , googExperimentalAutoGainControl("googExperimentalAutoGainControl")
    , googNoiseSuppression("googNoiseSuppression")
    , googHighpassFilter("googHighpassFilter")
    , googTypingNoiseDetection("googTypingNoiseDetection")
    , googExperimentalNoiseSuppression("googExperimentalNoiseSuppression")
    , googBeamforming("googBeamforming")
    , googArrayGeometry("googArrayGeometry")
    , googAudioMirroring("googAudioMirroring")
    , googDAEchoCancellation("googDAEchoCancellation")
    , googNoiseReduction("googNoiseReduction")
    , offerToReceiveAudio("offerToReceiveAudio")
    , offerToReceiveVideo("offerToReceiveVideo")
    , voiceActivityDetection("voiceActivityDetection")
    , iceRestart("iceRestart")
    , googUseRtpMux("googUseRtpMux")
    , enableDtlsSrtp("enableDtlsSrtp")
    , enableRtpDataChannels("enableRtpDataChannels")
    , enableDscp("enableDscp")
    , enableIPv6("enableIPv6")
    , googEnableVideoSuspendBelowMinBitrate("googEnableVideoSuspendBelowMinBitrate")
    , googNumUnsignalledRecvStreams("googNumUnsignalledRecvStreams")
    , googCombinedAudioVideoBwe("googCombinedAudioVideoBwe")
    , googScreencastMinBitrate("googScreencastMinBitrate")
    , googCpuOveruseDetection("googCpuOveruseDetection")
    , googCpuUnderuseThreshold("googCpuUnderuseThreshold")
    , googCpuOveruseThreshold("googCpuOveruseThreshold")
    , googCpuUnderuseEncodeRsdThreshold("googCpuUnderuseEncodeRsdThreshold")
    , googCpuOveruseEncodeRsdThreshold("googCpuOveruseEncodeRsdThreshold")
    , googCpuOveruseEncodeUsage("googCpuOveruseEncodeUsage")
    , googHighStartBitrate("googHighStartBitrate")
    , googPayloadPadding("googPayloadPadding")
    , googLatencyMs("latencyMs")
    , googPowerLineFrequency("googPowerLineFrequency")
{
}

std::vector<const BaseConstraint*> WebMediaTrackConstraintSet::allConstraints() const
{
    const BaseConstraint* temp[] = {
        &width, &height, &aspectRatio, &frameRate, &facingMode, &volume,
        &sampleRate, &sampleSize, &echoCancellation, &latency, &channelCount,
        &deviceId, &groupId, &mediaStreamSource, &renderToAssociatedSink,
        &hotwordEnabled, &googEchoCancellation,
        &googExperimentalEchoCancellation, &googAutoGainControl,
        &googExperimentalAutoGainControl, &googNoiseSuppression,
        &googHighpassFilter, &googTypingNoiseDetection,
        &googExperimentalNoiseSuppression, &googBeamforming,
        &googArrayGeometry, &googAudioMirroring, &googDAEchoCancellation,
        &googNoiseReduction, &offerToReceiveAudio,
        &offerToReceiveVideo, &voiceActivityDetection, &iceRestart,
        &googUseRtpMux, &enableDtlsSrtp, &enableRtpDataChannels,
        &enableDscp, &enableIPv6, &googEnableVideoSuspendBelowMinBitrate,
        &googNumUnsignalledRecvStreams, &googCombinedAudioVideoBwe,
        &googScreencastMinBitrate, &googCpuOveruseDetection,
        &googCpuUnderuseThreshold, &googCpuOveruseThreshold,
        &googCpuUnderuseEncodeRsdThreshold, &googCpuOveruseEncodeRsdThreshold,
        &googCpuOveruseEncodeUsage, &googHighStartBitrate, &googPayloadPadding,
        &googLatencyMs, &googPowerLineFrequency
    };
    const int elementCount = sizeof(temp) / sizeof(temp[0]);
    return std::vector<const BaseConstraint*>(&temp[0], &temp[elementCount]);
}

bool WebMediaTrackConstraintSet::isEmpty() const
{
    for (const auto& constraint : allConstraints()) {
        if (!constraint->isEmpty())
            return false;
    }
    return true;
}

bool WebMediaTrackConstraintSet::hasMandatoryOutsideSet(const std::vector<std::string>& goodNames, std::string& foundName) const
{
    for (const auto& constraint : allConstraints()) {
        if (constraint->hasMandatory()) {
            if (std::find(goodNames.begin(), goodNames.end(), constraint->name())
                == goodNames.end()) {
                foundName = constraint->name();
                return true;
            }
        }
    }
    return false;
}

bool WebMediaTrackConstraintSet::hasMandatory() const
{
    std::string dummyString;
    return hasMandatoryOutsideSet(std::vector<std::string>(), dummyString);
}

WebString WebMediaTrackConstraintSet::toString() const
{
    StringBuilder builder;
    bool first = true;
    for (const auto& constraint : allConstraints()) {
        if (!constraint->isEmpty()) {
            if (!first)
                builder.append(", ");
            builder.append(constraint->name());
            builder.append(": ");
            builder.append(constraint->toString());
            first = false;
        }
    }
    return builder.toString();
}

// WebMediaConstraints

void WebMediaConstraints::assign(const WebMediaConstraints& other)
{
    m_private = other.m_private;
}

void WebMediaConstraints::reset()
{
    m_private.reset();
}

bool WebMediaConstraints::isEmpty() const
{
    return m_private.isNull() || m_private->isEmpty();
}

void WebMediaConstraints::initialize()
{
    ASSERT(isNull());
    m_private = WebMediaConstraintsPrivate::create();
}

void WebMediaConstraints::initialize(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced)
{
    ASSERT(isNull());
    m_private = WebMediaConstraintsPrivate::create(basic, advanced);
}

const WebMediaTrackConstraintSet& WebMediaConstraints::basic() const
{
    ASSERT(!isNull());
    return m_private->basic();
}

const WebVector<WebMediaTrackConstraintSet>& WebMediaConstraints::advanced() const
{
    ASSERT(!isNull());
    return m_private->advanced();
}

const WebString WebMediaConstraints::toString() const
{
    if (isNull())
        return WebString("");
    return m_private->toString();
}

} // namespace blink
