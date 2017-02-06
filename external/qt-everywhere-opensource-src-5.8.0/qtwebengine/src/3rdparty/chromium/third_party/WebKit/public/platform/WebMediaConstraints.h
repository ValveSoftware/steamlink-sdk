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

#ifndef WebMediaConstraints_h
#define WebMediaConstraints_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

#include <vector>

namespace blink {

class WebMediaConstraintsPrivate;

class BLINK_PLATFORM_EXPORT BaseConstraint {
public:
    explicit BaseConstraint(const char* name);
    virtual ~BaseConstraint();
    virtual bool isEmpty() const = 0;
    virtual bool hasMandatory() const = 0;
    const char* name() const
    {
        return m_name;
    }
    virtual WebString toString() const = 0;

private:
    const char* m_name;
};

class BLINK_PLATFORM_EXPORT LongConstraint : public BaseConstraint {
public:
    explicit LongConstraint(const char* name);

    void setMin(long value)
    {
        m_min = value;
        m_hasMin = true;
    }

    void setMax(long value)
    {
        m_max = value;
        m_hasMax = true;
    }

    void setExact(long value)
    {
        m_exact = value;
        m_hasExact = true;
    }

    void setIdeal(long value)
    {
        m_ideal = value;
        m_hasIdeal = true;
    }

    bool matches(long value) const;
    bool isEmpty() const override;
    bool hasMandatory() const override;
    WebString toString() const override;
    bool hasMin() const { return m_hasMin; }
    long min() const { return m_min; }
    bool hasMax() const { return m_hasMax; }
    long max() const { return m_max; }
    bool hasExact() const { return m_hasExact; }
    long exact() const { return m_exact; }
    bool hasIdeal() const { return m_hasIdeal; }
    long ideal() const { return m_ideal; }

private:
    long m_min;
    long m_max;
    long m_exact;
    long m_ideal;
    unsigned m_hasMin : 1;
    unsigned m_hasMax : 1;
    unsigned m_hasExact : 1;
    unsigned m_hasIdeal : 1;
};

class BLINK_PLATFORM_EXPORT DoubleConstraint : public BaseConstraint {
public:
    // Permit a certain leeway when comparing floats. The offset of 0.00001
    // is chosen based on observed behavior of doubles formatted with
    // rtc::ToString.
    static const double kConstraintEpsilon;

    explicit DoubleConstraint(const char* name);

    void setMin(double value)
    {
        m_min = value;
        m_hasMin = true;
    }

    void setMax(double value)
    {
        m_max = value;
        m_hasMax = true;
    }

    void setExact(double value)
    {
        m_exact = value;
        m_hasExact = true;
    }

    void setIdeal(double value)
    {
        m_ideal = value;
        m_hasIdeal = true;
    }

    bool matches(double value) const;
    bool isEmpty() const override;
    bool hasMandatory() const override;
    WebString toString() const override;
    bool hasMin() const { return m_hasMin; }
    double min() const { return m_min; }
    bool hasMax() const { return m_hasMax; }
    double max() const { return m_max; }
    bool hasExact() const { return m_hasExact; }
    double exact() const { return m_exact; }
    bool hasIdeal() const { return m_hasIdeal; }
    double ideal() const { return m_ideal; }

private:
    double m_min;
    double m_max;
    double m_exact;
    double m_ideal;
    unsigned m_hasMin : 1;
    unsigned m_hasMax : 1;
    unsigned m_hasExact : 1;
    unsigned m_hasIdeal : 1;
};

class BLINK_PLATFORM_EXPORT StringConstraint : public BaseConstraint {
public:
    // String-valued options don't have min or max, but can have multiple
    // values for ideal and exact.
    explicit StringConstraint(const char* name);

    void setExact(const WebString& exact)
    {
        m_exact.assign(&exact, 1);
    }

    void setExact(const WebVector<WebString>& exact)
    {
        m_exact.assign(exact);
    }

    void setIdeal(const WebVector<WebString>& ideal)
    {
        m_ideal.assign(ideal);
    }


    bool matches(WebString value) const;
    bool isEmpty() const override;
    bool hasMandatory() const override;
    WebString toString() const override;
    bool hasExact() const { return !m_exact.isEmpty(); }
    bool hasIdeal() const { return !m_ideal.isEmpty(); }
    const WebVector<WebString>& exact() const;
    const WebVector<WebString>& ideal() const;

private:
    WebVector<WebString> m_exact;
    WebVector<WebString> m_ideal;
};

class BLINK_PLATFORM_EXPORT BooleanConstraint : public BaseConstraint {
public:
    explicit BooleanConstraint(const char* name);

    bool exact() const { return m_exact; }
    bool ideal() const { return m_ideal; }
    void setIdeal(bool value)
    {
        m_ideal = value;
        m_hasIdeal = true;
    }

    void setExact(bool value)
    {
        m_exact = value;
        m_hasExact = true;
    }

    bool matches(bool value) const;
    bool isEmpty() const override;
    bool hasMandatory() const override;
    WebString toString() const override;
    bool hasExact() const { return m_hasExact; }
    bool hasIdeal() const { return m_hasIdeal; }

private:
    unsigned m_ideal : 1;
    unsigned m_exact : 1;
    unsigned m_hasIdeal : 1;
    unsigned m_hasExact : 1;
};

struct WebMediaTrackConstraintSet {
public:
    BLINK_PLATFORM_EXPORT WebMediaTrackConstraintSet();

    LongConstraint width;
    LongConstraint height;
    DoubleConstraint aspectRatio;
    DoubleConstraint frameRate;
    StringConstraint facingMode;
    DoubleConstraint volume;
    LongConstraint sampleRate;
    LongConstraint sampleSize;
    BooleanConstraint echoCancellation;
    DoubleConstraint latency;
    LongConstraint channelCount;
    StringConstraint deviceId;
    StringConstraint groupId;
    // Constraints not exposed in Blink at the moment, only through
    // the legacy name interface.
    StringConstraint mediaStreamSource; // tab, screen, desktop, system
    BooleanConstraint renderToAssociatedSink;
    BooleanConstraint hotwordEnabled;
    BooleanConstraint googEchoCancellation;
    BooleanConstraint googExperimentalEchoCancellation;
    BooleanConstraint googAutoGainControl;
    BooleanConstraint googExperimentalAutoGainControl;
    BooleanConstraint googNoiseSuppression;
    BooleanConstraint googHighpassFilter;
    BooleanConstraint googTypingNoiseDetection;
    BooleanConstraint googExperimentalNoiseSuppression;
    BooleanConstraint googBeamforming;
    StringConstraint googArrayGeometry;
    BooleanConstraint googAudioMirroring;
    BooleanConstraint googDAEchoCancellation;
    BooleanConstraint googNoiseReduction;
    LongConstraint offerToReceiveAudio;
    LongConstraint offerToReceiveVideo;
    BooleanConstraint voiceActivityDetection;
    BooleanConstraint iceRestart;
    BooleanConstraint googUseRtpMux;
    BooleanConstraint enableDtlsSrtp;
    BooleanConstraint enableRtpDataChannels;
    BooleanConstraint enableDscp;
    BooleanConstraint enableIPv6;
    BooleanConstraint googEnableVideoSuspendBelowMinBitrate;
    LongConstraint googNumUnsignalledRecvStreams;
    BooleanConstraint googCombinedAudioVideoBwe;
    LongConstraint googScreencastMinBitrate;
    BooleanConstraint googCpuOveruseDetection;
    LongConstraint googCpuUnderuseThreshold;
    LongConstraint googCpuOveruseThreshold;
    LongConstraint googCpuUnderuseEncodeRsdThreshold;
    LongConstraint googCpuOveruseEncodeRsdThreshold;
    BooleanConstraint googCpuOveruseEncodeUsage;
    LongConstraint googHighStartBitrate;
    BooleanConstraint googPayloadPadding;
    LongConstraint googLatencyMs;
    LongConstraint googPowerLineFrequency;

    BLINK_PLATFORM_EXPORT bool isEmpty() const;
    BLINK_PLATFORM_EXPORT bool hasMandatory() const;
    BLINK_PLATFORM_EXPORT bool hasMandatoryOutsideSet(const std::vector<std::string>&, std::string&) const;
    BLINK_PLATFORM_EXPORT WebString toString() const;

private:
    std::vector<const BaseConstraint*> allConstraints() const;
};

class WebMediaConstraints {
public:
    WebMediaConstraints()
    {
    }
    WebMediaConstraints(const WebMediaConstraints& other) { assign(other); }
    ~WebMediaConstraints() { reset(); }

    WebMediaConstraints& operator=(const WebMediaConstraints& other)
    {
        assign(other);
        return *this;
    }

    BLINK_PLATFORM_EXPORT void assign(const WebMediaConstraints&);

    BLINK_PLATFORM_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }
    BLINK_PLATFORM_EXPORT bool isEmpty() const;

    BLINK_PLATFORM_EXPORT void initialize();
    BLINK_PLATFORM_EXPORT void initialize(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced);

    BLINK_PLATFORM_EXPORT const WebMediaTrackConstraintSet& basic() const;
    BLINK_PLATFORM_EXPORT const WebVector<WebMediaTrackConstraintSet>& advanced() const;

    BLINK_PLATFORM_EXPORT const WebString toString() const;

private:
    WebPrivatePtr<WebMediaConstraintsPrivate> m_private;
};

} // namespace blink

#endif // WebMediaConstraints_h
