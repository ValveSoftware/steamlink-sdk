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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "modules/webaudio/AnalyserNode.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"

namespace WebCore {

AnalyserNode::AnalyserNode(AudioContext* context, float sampleRate)
    : AudioBasicInspectorNode(context, sampleRate, 2)
{
    ScriptWrappable::init(this);
    setNodeType(NodeTypeAnalyser);
    initialize();
}

AnalyserNode::~AnalyserNode()
{
    uninitialize();
}

void AnalyserNode::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0)->bus();

    if (!isInitialized() || !input(0)->isConnected()) {
        outputBus->zero();
        return;
    }

    AudioBus* inputBus = input(0)->bus();

    // Give the analyser the audio which is passing through this AudioNode.
    m_analyser.writeInput(inputBus, framesToProcess);

    // For in-place processing, our override of pullInputs() will just pass the audio data through unchanged if the channel count matches from input to output
    // (resulting in inputBus == outputBus). Otherwise, do an up-mix to stereo.
    if (inputBus != outputBus)
        outputBus->copyFrom(*inputBus);
}

void AnalyserNode::setFftSize(unsigned size, ExceptionState& exceptionState)
{
    if (!m_analyser.setFftSize(size)) {
        exceptionState.throwDOMException(
            IndexSizeError,
            (size < RealtimeAnalyser::MinFFTSize || size > RealtimeAnalyser::MaxFFTSize) ?
                ExceptionMessages::indexOutsideRange("FFT size", size, RealtimeAnalyser::MinFFTSize, ExceptionMessages::InclusiveBound, RealtimeAnalyser::MaxFFTSize, ExceptionMessages::InclusiveBound)
                : ("The value provided (" + String::number(size) + ") is not a power of two."));
    }
}

void AnalyserNode::setMinDecibels(double k, ExceptionState& exceptionState)
{
    if (k < maxDecibels()) {
        m_analyser.setMinDecibels(k);
    } else {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexExceedsMaximumBound("minDecibels", k, maxDecibels()));
    }
}

void AnalyserNode::setMaxDecibels(double k, ExceptionState& exceptionState)
{
    if (k > minDecibels()) {
        m_analyser.setMaxDecibels(k);
    } else {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexExceedsMinimumBound("maxDecibels", k, minDecibels()));
    }
}

void AnalyserNode::setSmoothingTimeConstant(double k, ExceptionState& exceptionState)
{
    if (k >= 0 && k <= 1) {
        m_analyser.setSmoothingTimeConstant(k);
    } else {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange("smoothing value", k, 0.0, ExceptionMessages::InclusiveBound, 1.0, ExceptionMessages::InclusiveBound));
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
