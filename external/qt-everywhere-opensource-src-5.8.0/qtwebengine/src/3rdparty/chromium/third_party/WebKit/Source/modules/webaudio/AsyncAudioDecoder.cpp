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

#include "core/dom/DOMArrayBuffer.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/AsyncAudioDecoder.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioBufferCallback.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioFileReader.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"

namespace blink {

AsyncAudioDecoder::AsyncAudioDecoder()
    : m_thread(wrapUnique(Platform::current()->createThread("Audio Decoder")))
{
}

AsyncAudioDecoder::~AsyncAudioDecoder()
{
}

void AsyncAudioDecoder::decodeAsync(DOMArrayBuffer* audioData, float sampleRate, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, ScriptPromiseResolver* resolver, AbstractAudioContext* context)
{
    ASSERT(isMainThread());
    ASSERT(audioData);
    if (!audioData)
        return;

    m_thread->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&AsyncAudioDecoder::decode, wrapCrossThreadPersistent(audioData), sampleRate, wrapCrossThreadPersistent(successCallback), wrapCrossThreadPersistent(errorCallback), wrapCrossThreadPersistent(resolver), wrapCrossThreadPersistent(context)));
}

void AsyncAudioDecoder::decode(DOMArrayBuffer* audioData, float sampleRate, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, ScriptPromiseResolver* resolver, AbstractAudioContext* context)
{
    RefPtr<AudioBus> bus = createBusFromInMemoryAudioFile(audioData->data(), audioData->byteLength(), false, sampleRate);

    // Decoding is finished, but we need to do the callbacks on the main thread.
    // A reference to |*bus| is retained by WTF::Function and will be removed after notifyComplete() is done.
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&AsyncAudioDecoder::notifyComplete, wrapCrossThreadPersistent(audioData), wrapCrossThreadPersistent(successCallback), wrapCrossThreadPersistent(errorCallback), bus.release(), wrapCrossThreadPersistent(resolver), wrapCrossThreadPersistent(context)));
}

void AsyncAudioDecoder::notifyComplete(DOMArrayBuffer*, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, AudioBus* audioBus, ScriptPromiseResolver* resolver, AbstractAudioContext* context)
{
    ASSERT(isMainThread());

    AudioBuffer* audioBuffer = AudioBuffer::createFromAudioBus(audioBus);

    // Let the context finish the notification.
    context->handleDecodeAudioData(audioBuffer, resolver, successCallback, errorCallback);
}

} // namespace blink
