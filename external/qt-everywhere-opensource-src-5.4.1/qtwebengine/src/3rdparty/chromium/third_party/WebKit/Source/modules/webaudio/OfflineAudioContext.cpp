/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "modules/webaudio/OfflineAudioContext.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<OfflineAudioContext> OfflineAudioContext::create(ExecutionContext* context, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState& exceptionState)
{
    // FIXME: add support for workers.
    if (!context || !context->isDocument()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "Workers are not supported.");
        return nullptr;
    }

    Document* document = toDocument(context);

    if (!numberOfFrames) {
        exceptionState.throwDOMException(SyntaxError, "number of frames cannot be zero.");
        return nullptr;
    }

    if (numberOfChannels > AudioContext::maxNumberOfChannels()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange<unsigned>(
                "number of channels",
                numberOfChannels,
                0,
                ExceptionMessages::InclusiveBound,
                AudioContext::maxNumberOfChannels(),
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (!isSampleRateRangeGood(sampleRate)) {
        exceptionState.throwDOMException(SyntaxError, "sample rate (" + String::number(sampleRate) + ") must be in the range 44100-96000 Hz.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<OfflineAudioContext> audioContext(adoptRefWillBeThreadSafeRefCountedGarbageCollected(new OfflineAudioContext(document, numberOfChannels, numberOfFrames, sampleRate)));

    if (!audioContext->destination()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "OfflineAudioContext(" + String::number(numberOfChannels)
            + ", " + String::number(numberOfFrames)
            + ", " + String::number(sampleRate)
            + ")");
    }

    audioContext->suspendIfNeeded();
    return audioContext.release();
}

OfflineAudioContext::OfflineAudioContext(Document* document, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : AudioContext(document, numberOfChannels, numberOfFrames, sampleRate)
{
    ScriptWrappable::init(this);
}

OfflineAudioContext::~OfflineAudioContext()
{
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
