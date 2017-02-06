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

#ifndef MediaStreamAudioDestinationNode_h
#define MediaStreamAudioDestinationNode_h

#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AudioBasicInspectorNode.h"
#include "platform/audio/AudioBus.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class AbstractAudioContext;

class MediaStreamAudioDestinationHandler final : public AudioBasicInspectorHandler {
public:
    static PassRefPtr<MediaStreamAudioDestinationHandler> create(AudioNode&, size_t numberOfChannels);
    ~MediaStreamAudioDestinationHandler() override;

    MediaStream* stream() { return m_stream.get(); }

    // AudioHandler.
    void process(size_t framesToProcess) override;
    void setChannelCount(unsigned long, ExceptionState&) override;

    unsigned long maxChannelCount() const;

private:
    MediaStreamAudioDestinationHandler(AudioNode&, size_t numberOfChannels);
    // As an audio source, we will never propagate silence.
    bool propagatesSilence() const override { return false; }

    // This Persistent doesn't make a reference cycle.
    Persistent<MediaStream> m_stream;
    Persistent<MediaStreamSource> m_source;

    // This synchronizes dynamic changes to the channel count with
    // process() to manage the mix bus.
    mutable Mutex m_processLock;

    // This internal mix bus is for up/down mixing the input to the actual
    // number of channels in the destination.
    RefPtr<AudioBus> m_mixBus;
};

class MediaStreamAudioDestinationNode final : public AudioBasicInspectorNode {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaStreamAudioDestinationNode* create(AbstractAudioContext&, size_t numberOfChannels, ExceptionState&);
    MediaStream* stream() const;

private:
    MediaStreamAudioDestinationNode(AbstractAudioContext&, size_t numberOfChannels);
};

} // namespace blink

#endif // MediaStreamAudioDestinationNode_h
