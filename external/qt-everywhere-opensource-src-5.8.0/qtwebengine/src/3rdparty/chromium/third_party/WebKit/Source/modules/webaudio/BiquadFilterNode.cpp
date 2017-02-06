/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "modules/webaudio/BiquadFilterNode.h"

#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "platform/Histogram.h"
#include "wtf/PtrUtil.h"

namespace blink {

BiquadFilterNode::BiquadFilterNode(AbstractAudioContext& context)
    : AudioNode(context)
    , m_frequency(AudioParam::create(context, ParamTypeBiquadFilterFrequency, 350.0, 0, context.sampleRate() / 2))
    , m_q(AudioParam::create(context, ParamTypeBiquadFilterQ, 1.0))
    , m_gain(AudioParam::create(context, ParamTypeBiquadFilterGain, 0.0))
    , m_detune(AudioParam::create(context, ParamTypeBiquadFilterDetune, 0.0))
{
    setHandler(AudioBasicProcessorHandler::create(
        AudioHandler::NodeTypeBiquadFilter,
        *this,
        context.sampleRate(),
        wrapUnique(new BiquadProcessor(
            context.sampleRate(),
            1,
            m_frequency->handler(),
            m_q->handler(),
            m_gain->handler(),
            m_detune->handler()))));

    // Explicitly set the filter type so that any histograms get updated with the default value.
    // Otherwise, the histogram won't ever show it.
    setType("lowpass");
}

BiquadFilterNode* BiquadFilterNode::create(AbstractAudioContext& context, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    return new BiquadFilterNode(context);
}

DEFINE_TRACE(BiquadFilterNode)
{
    visitor->trace(m_frequency);
    visitor->trace(m_q);
    visitor->trace(m_gain);
    visitor->trace(m_detune);
    AudioNode::trace(visitor);
}

BiquadProcessor* BiquadFilterNode::getBiquadProcessor() const
{
    return static_cast<BiquadProcessor*>(static_cast<AudioBasicProcessorHandler&>(handler()).processor());
}

String BiquadFilterNode::type() const
{
    switch (const_cast<BiquadFilterNode*>(this)->getBiquadProcessor()->type()) {
    case BiquadProcessor::LowPass:
        return "lowpass";
    case BiquadProcessor::HighPass:
        return "highpass";
    case BiquadProcessor::BandPass:
        return "bandpass";
    case BiquadProcessor::LowShelf:
        return "lowshelf";
    case BiquadProcessor::HighShelf:
        return "highshelf";
    case BiquadProcessor::Peaking:
        return "peaking";
    case BiquadProcessor::Notch:
        return "notch";
    case BiquadProcessor::Allpass:
        return "allpass";
    default:
        ASSERT_NOT_REACHED();
        return "lowpass";
    }
}

void BiquadFilterNode::setType(const String& type)
{
    // For the Q histogram, we need to change the name of the AudioParam for the lowpass and
    // highpass filters so we know to count the Q value when it is set. And explicitly set the value
    // to itself so the histograms know the initial value.

    if (type == "lowpass") {
        setType(BiquadProcessor::LowPass);
        m_q->setParamType(ParamTypeBiquadFilterQLowpass);
        m_q->setValue(m_q->value());
    } else if (type == "highpass") {
        setType(BiquadProcessor::HighPass);
        m_q->setParamType(ParamTypeBiquadFilterQHighpass);
        m_q->setValue(m_q->value());
    } else if (type == "bandpass") {
        setType(BiquadProcessor::BandPass);
    } else if (type == "lowshelf") {
        setType(BiquadProcessor::LowShelf);
    } else if (type == "highshelf") {
        setType(BiquadProcessor::HighShelf);
    } else if (type == "peaking") {
        setType(BiquadProcessor::Peaking);
    } else if (type == "notch") {
        setType(BiquadProcessor::Notch);
    } else if (type == "allpass") {
        setType(BiquadProcessor::Allpass);
    }
}

bool BiquadFilterNode::setType(unsigned type)
{
    if (type > BiquadProcessor::Allpass)
        return false;

    DEFINE_STATIC_LOCAL(EnumerationHistogram, filterTypeHistogram,
        ("WebAudio.BiquadFilter.Type", BiquadProcessor::Allpass + 1));
    filterTypeHistogram.count(type);

    getBiquadProcessor()->setType(static_cast<BiquadProcessor::FilterType>(type));
    return true;
}

void BiquadFilterNode::getFrequencyResponse(const DOMFloat32Array* frequencyHz, DOMFloat32Array* magResponse, DOMFloat32Array* phaseResponse)
{
    ASSERT(frequencyHz && magResponse && phaseResponse);

    int n = std::min(frequencyHz->length(), std::min(magResponse->length(), phaseResponse->length()));
    if (n)
        getBiquadProcessor()->getFrequencyResponse(n, frequencyHz->data(), magResponse->data(), phaseResponse->data());
}

} // namespace blink

