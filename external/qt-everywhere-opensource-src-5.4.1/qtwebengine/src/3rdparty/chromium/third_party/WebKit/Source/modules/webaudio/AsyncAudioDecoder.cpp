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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "modules/webaudio/AsyncAudioDecoder.h"

#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioBufferCallback.h"
#include "platform/Task.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioFileReader.h"
#include "public/platform/Platform.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/MainThread.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

AsyncAudioDecoder::AsyncAudioDecoder()
    : m_thread(adoptPtr(blink::Platform::current()->createThread("Audio Decoder")))
{
}

AsyncAudioDecoder::~AsyncAudioDecoder()
{
}

void AsyncAudioDecoder::decodeAsync(ArrayBuffer* audioData, float sampleRate, PassOwnPtr<AudioBufferCallback> successCallback, PassOwnPtr<AudioBufferCallback> errorCallback)
{
    ASSERT(isMainThread());
    ASSERT(audioData);
    if (!audioData)
        return;

    // Add a ref to keep audioData alive until completion of decoding.
    RefPtr<ArrayBuffer> audioDataRef(audioData);

    // The leak references to successCallback and errorCallback are picked up on notifyComplete.
    m_thread->postTask(new Task(WTF::bind(&AsyncAudioDecoder::decode, audioDataRef.release().leakRef(), sampleRate, successCallback.leakPtr(), errorCallback.leakPtr())));
}

void AsyncAudioDecoder::decode(ArrayBuffer* audioData, float sampleRate, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback)
{
    RefPtr<AudioBus> bus = createBusFromInMemoryAudioFile(audioData->data(), audioData->byteLength(), false, sampleRate);

    // Decoding is finished, but we need to do the callbacks on the main thread.
    // The leaked reference to audioBuffer is picked up in notifyComplete.
    callOnMainThread(WTF::bind(&AsyncAudioDecoder::notifyComplete, audioData, successCallback, errorCallback, bus.release().leakRef()));
}

void AsyncAudioDecoder::notifyComplete(ArrayBuffer* audioData, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, AudioBus* audioBus)
{
    // Adopt references, so everything gets correctly dereffed.
    RefPtr<ArrayBuffer> audioDataRef = adoptRef(audioData);
    OwnPtr<AudioBufferCallback> successCallbackPtr = adoptPtr(successCallback);
    OwnPtr<AudioBufferCallback> errorCallbackPtr = adoptPtr(errorCallback);
    RefPtr<AudioBus> audioBusRef = adoptRef(audioBus);

    RefPtrWillBeRawPtr<AudioBuffer> audioBuffer = AudioBuffer::createFromAudioBus(audioBus);
    if (audioBuffer.get() && successCallback)
        successCallback->handleEvent(audioBuffer.get());
    else if (errorCallback)
        errorCallback->handleEvent(audioBuffer.get());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
