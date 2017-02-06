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

#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/DynamicsCompressorNode.h"
#include "platform/audio/DynamicsCompressor.h"
#include "wtf/PtrUtil.h"

// Set output to stereo by default.
static const unsigned defaultNumberOfOutputChannels = 2;

namespace blink {

DynamicsCompressorHandler::DynamicsCompressorHandler(
    AudioNode& node,
    float sampleRate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release)
    : AudioHandler(NodeTypeDynamicsCompressor, node, sampleRate)
    , m_threshold(threshold)
    , m_knee(knee)
    , m_ratio(ratio)
    , m_reduction(0)
    , m_attack(attack)
    , m_release(release)
{
    addInput();
    addOutput(defaultNumberOfOutputChannels);
    initialize();
}

PassRefPtr<DynamicsCompressorHandler> DynamicsCompressorHandler::create(
    AudioNode& node,
    float sampleRate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release)
{
    return adoptRef(new DynamicsCompressorHandler(
        node,
        sampleRate,
        threshold,
        knee,
        ratio,
        attack,
        release));
}

DynamicsCompressorHandler::~DynamicsCompressorHandler()
{
    uninitialize();
}

void DynamicsCompressorHandler::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0).bus();
    ASSERT(outputBus);

    float threshold = m_threshold->value();
    float knee = m_knee->value();
    float ratio = m_ratio->value();
    float attack = m_attack->value();
    float release = m_release->value();

    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamThreshold, threshold);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamKnee, knee);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamRatio, ratio);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamAttack, attack);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamRelease, release);

    m_dynamicsCompressor->process(input(0).bus(), outputBus, framesToProcess);

    m_reduction = m_dynamicsCompressor->parameterValue(DynamicsCompressor::ParamReduction);
}

void DynamicsCompressorHandler::initialize()
{
    if (isInitialized())
        return;

    AudioHandler::initialize();
    m_dynamicsCompressor = wrapUnique(new DynamicsCompressor(sampleRate(), defaultNumberOfOutputChannels));
}

void DynamicsCompressorHandler::clearInternalStateWhenDisabled()
{
    m_reduction = 0;
}

double DynamicsCompressorHandler::tailTime() const
{
    return m_dynamicsCompressor->tailTime();
}

double DynamicsCompressorHandler::latencyTime() const
{
    return m_dynamicsCompressor->latencyTime();
}

// ----------------------------------------------------------------

DynamicsCompressorNode::DynamicsCompressorNode(AbstractAudioContext& context)
    : AudioNode(context)
    , m_threshold(AudioParam::create(context, ParamTypeDynamicsCompressorThreshold, -24, -100, 0))
    , m_knee(AudioParam::create(context, ParamTypeDynamicsCompressorKnee, 30, 0, 40))
    , m_ratio(AudioParam::create(context, ParamTypeDynamicsCompressorRatio, 12, 1, 20))
    , m_attack(AudioParam::create(context, ParamTypeDynamicsCompressorAttack, 0.003, 0, 1))
    , m_release(AudioParam::create(context, ParamTypeDynamicsCompressorRelease, 0.250, 0, 1))
{
    setHandler(DynamicsCompressorHandler::create(
        *this,
        context.sampleRate(),
        m_threshold->handler(),
        m_knee->handler(),
        m_ratio->handler(),
        m_attack->handler(),
        m_release->handler()));
}

DynamicsCompressorNode* DynamicsCompressorNode::create(AbstractAudioContext& context, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    return new DynamicsCompressorNode(context);
}

DEFINE_TRACE(DynamicsCompressorNode)
{
    visitor->trace(m_threshold);
    visitor->trace(m_knee);
    visitor->trace(m_ratio);
    visitor->trace(m_attack);
    visitor->trace(m_release);
    AudioNode::trace(visitor);
}

DynamicsCompressorHandler& DynamicsCompressorNode::dynamicsCompressorHandler() const
{
    return static_cast<DynamicsCompressorHandler&>(handler());
}

AudioParam* DynamicsCompressorNode::threshold() const
{
    return m_threshold;
}

AudioParam* DynamicsCompressorNode::knee() const
{
    return m_knee;
}

AudioParam* DynamicsCompressorNode::ratio() const
{
    return m_ratio;
}

float DynamicsCompressorNode::reduction() const
{
    return dynamicsCompressorHandler().reductionValue();
}

AudioParam* DynamicsCompressorNode::attack() const
{
    return m_attack;
}

AudioParam* DynamicsCompressorNode::release() const
{
    return m_release;
}

} // namespace blink

