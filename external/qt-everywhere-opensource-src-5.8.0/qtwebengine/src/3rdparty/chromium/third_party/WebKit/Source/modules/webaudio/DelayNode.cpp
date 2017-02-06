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

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/DelayNode.h"
#include "modules/webaudio/DelayProcessor.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"

namespace blink {

const double maximumAllowedDelayTime = 180;

DelayNode::DelayNode(AbstractAudioContext& context, double maxDelayTime)
    : AudioNode(context)
    , m_delayTime(AudioParam::create(context, ParamTypeDelayDelayTime, 0.0, 0.0, maxDelayTime))
{
    setHandler(AudioBasicProcessorHandler::create(
        AudioHandler::NodeTypeDelay,
        *this,
        context.sampleRate(),
        wrapUnique(new DelayProcessor(
            context.sampleRate(),
            1,
            m_delayTime->handler(),
            maxDelayTime))));
}

DelayNode* DelayNode::create(AbstractAudioContext& context, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    // The default maximum delay time for the delay node is 1 sec.
    return create(context, 1, exceptionState);
}

DelayNode* DelayNode::create(AbstractAudioContext& context, double maxDelayTime, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    if (maxDelayTime <= 0 || maxDelayTime >= maximumAllowedDelayTime) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange(
                "max delay time",
                maxDelayTime,
                0.0,
                ExceptionMessages::ExclusiveBound,
                maximumAllowedDelayTime,
                ExceptionMessages::ExclusiveBound));
        return nullptr;
    }

    return new DelayNode(context, maxDelayTime);
}

AudioParam* DelayNode::delayTime()
{
    return m_delayTime;
}

DEFINE_TRACE(DelayNode)
{
    visitor->trace(m_delayTime);
    AudioNode::trace(visitor);
}

} // namespace blink

