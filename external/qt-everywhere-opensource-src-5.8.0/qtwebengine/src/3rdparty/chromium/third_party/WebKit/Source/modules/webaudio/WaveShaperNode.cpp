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

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/WaveShaperNode.h"
#include "wtf/PtrUtil.h"

namespace blink {

WaveShaperNode::WaveShaperNode(AbstractAudioContext& context)
    : AudioNode(context)
{
    setHandler(AudioBasicProcessorHandler::create(AudioHandler::NodeTypeWaveShaper, *this, context.sampleRate(), wrapUnique(new WaveShaperProcessor(context.sampleRate(), 1))));

    handler().initialize();
}

WaveShaperNode* WaveShaperNode::create(AbstractAudioContext& context, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    return new WaveShaperNode(context);
}

WaveShaperProcessor* WaveShaperNode::getWaveShaperProcessor() const
{
    return static_cast<WaveShaperProcessor*>(static_cast<AudioBasicProcessorHandler&>(handler()).processor());
}

void WaveShaperNode::setCurve(DOMFloat32Array* curve, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (curve && curve->length() < 2) {
        exceptionState.throwDOMException(
            InvalidAccessError,
            ExceptionMessages::indexExceedsMinimumBound<unsigned>(
                "curve length",
                curve->length(),
                2));
        return;
    }

    getWaveShaperProcessor()->setCurve(curve);
}

DOMFloat32Array* WaveShaperNode::curve()
{
    return getWaveShaperProcessor()->curve();
}

void WaveShaperNode::setOversample(const String& type)
{
    ASSERT(isMainThread());

    // This is to synchronize with the changes made in
    // AudioBasicProcessorNode::checkNumberOfChannelsForInput() where we can
    // initialize() and uninitialize().
    AbstractAudioContext::AutoLocker contextLocker(context());

    if (type == "none") {
        getWaveShaperProcessor()->setOversample(WaveShaperProcessor::OverSampleNone);
    } else if (type == "2x") {
        getWaveShaperProcessor()->setOversample(WaveShaperProcessor::OverSample2x);
    } else if (type == "4x") {
        getWaveShaperProcessor()->setOversample(WaveShaperProcessor::OverSample4x);
    } else {
        ASSERT_NOT_REACHED();
    }
}

String WaveShaperNode::oversample() const
{
    switch (const_cast<WaveShaperNode*>(this)->getWaveShaperProcessor()->oversample()) {
    case WaveShaperProcessor::OverSampleNone:
        return "none";
    case WaveShaperProcessor::OverSample2x:
        return "2x";
    case WaveShaperProcessor::OverSample4x:
        return "4x";
    default:
        ASSERT_NOT_REACHED();
        return "none";
    }
}

} // namespace blink

