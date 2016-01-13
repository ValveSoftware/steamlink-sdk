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

#ifndef MediaStreamAudioSourceNode_h
#define MediaStreamAudioSourceNode_h

#if ENABLE(WEB_AUDIO)

#include "platform/audio/AudioSourceProvider.h"
#include "platform/audio/AudioSourceProviderClient.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AudioSourceNode.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Threading.h"

namespace WebCore {

class AudioContext;

class MediaStreamAudioSourceNode FINAL : public AudioSourceNode, public AudioSourceProviderClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaStreamAudioSourceNode);
public:
    static PassRefPtrWillBeRawPtr<MediaStreamAudioSourceNode> create(AudioContext*, MediaStream*, MediaStreamTrack*, PassOwnPtr<AudioSourceProvider>);

    virtual ~MediaStreamAudioSourceNode();

    MediaStream* mediaStream() { return m_mediaStream.get(); }

    // AudioNode
    virtual void process(size_t framesToProcess) OVERRIDE;

    // AudioSourceProviderClient
    virtual void setFormat(size_t numberOfChannels, float sampleRate) OVERRIDE;

    AudioSourceProvider* audioSourceProvider() const { return m_audioSourceProvider.get(); }

    virtual void trace(Visitor*) OVERRIDE;

private:
    MediaStreamAudioSourceNode(AudioContext*, MediaStream*, MediaStreamTrack*, PassOwnPtr<AudioSourceProvider>);

    // As an audio source, we will never propagate silence.
    virtual bool propagatesSilence() const OVERRIDE { return false; }

    RefPtrWillBeMember<MediaStream> m_mediaStream;
    RefPtrWillBeMember<MediaStreamTrack> m_audioTrack;
    OwnPtr<AudioSourceProvider> m_audioSourceProvider;

    Mutex m_processLock;

    unsigned m_sourceNumberOfChannels;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)

#endif // MediaStreamAudioSourceNode_h
