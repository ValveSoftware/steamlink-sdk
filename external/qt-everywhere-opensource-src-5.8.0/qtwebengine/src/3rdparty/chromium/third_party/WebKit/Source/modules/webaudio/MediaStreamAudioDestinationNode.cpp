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

#include "modules/webaudio/MediaStreamAudioDestinationNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "platform/UUID.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "wtf/Locker.h"

namespace blink {

// WebAudioCapturerSource ignores the channel count beyond 8, so we set the
// block here to avoid anything can cause the crash.
static const unsigned long kMaxChannelCount = 8;

MediaStreamAudioDestinationHandler::MediaStreamAudioDestinationHandler(AudioNode& node, size_t numberOfChannels)
    : AudioBasicInspectorHandler(NodeTypeMediaStreamAudioDestination, node, node.context()->sampleRate(), numberOfChannels)
    , m_mixBus(AudioBus::create(numberOfChannels, ProcessingSizeInFrames))
{
    m_source = MediaStreamSource::create("WebAudio-" + createCanonicalUUIDString(), MediaStreamSource::TypeAudio, "MediaStreamAudioDestinationNode", false, MediaStreamSource::ReadyStateLive, true);
    MediaStreamSourceVector audioSources;
    audioSources.append(m_source.get());
    MediaStreamSourceVector videoSources;
    m_stream = MediaStream::create(node.context()->getExecutionContext(), MediaStreamDescriptor::create(audioSources, videoSources));
    MediaStreamCenter::instance().didCreateMediaStreamAndTracks(m_stream->descriptor());

    m_source->setAudioFormat(numberOfChannels, node.context()->sampleRate());

    initialize();
}

PassRefPtr<MediaStreamAudioDestinationHandler> MediaStreamAudioDestinationHandler::create(AudioNode& node, size_t numberOfChannels)
{
    return adoptRef(new MediaStreamAudioDestinationHandler(node, numberOfChannels));
}

MediaStreamAudioDestinationHandler::~MediaStreamAudioDestinationHandler()
{
    uninitialize();
}

void MediaStreamAudioDestinationHandler::process(size_t numberOfFrames)
{
    // Conform the input bus into the internal mix bus, which represents
    // MediaStreamDestination's channel count.

    // Synchronize with possible dynamic changes to the channel count.
    MutexTryLocker tryLocker(m_processLock);

    // If we can get the lock, we can process normally by updating the
    // mix bus to a new channel count, if needed.  If not, just use the
    // old mix bus to do the mixing; we'll update the bus next time
    // around.
    if (tryLocker.locked()) {
      unsigned count = channelCount();
      if (count != m_mixBus->numberOfChannels()) {
        m_mixBus = AudioBus::create(count, ProcessingSizeInFrames);
        // setAudioFormat has an internal lock.  This can cause audio to
        // glitch.  This is outside of our control.
        m_source->setAudioFormat(count, context()->sampleRate());
      }
    }

    m_mixBus->copyFrom(*input(0).bus());

    // consumeAudio has an internal lock (also used by setAudioFormat).
    // This can cause audio to glitch.  This is outside of our control.
    m_source->consumeAudio(m_mixBus.get(), numberOfFrames);
}

void MediaStreamAudioDestinationHandler::setChannelCount(unsigned long channelCount, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    // Currently the maximum channel count supported for this node is 8,
    // which is constrained by m_source (WebAudioCapturereSource). Although
    // it has its own safety check for the excessive channels, throwing an
    // exception here is useful to developers.
    if (channelCount > maxChannelCount()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange<unsigned>("channel count",
                channelCount,
                1,
                ExceptionMessages::InclusiveBound,
                maxChannelCount(),
                ExceptionMessages::InclusiveBound));
        return;
    }

    // Synchronize changes in the channel count with process() which
    // needs to update m_mixBus.
    MutexLocker locker(m_processLock);

    AudioHandler::setChannelCount(channelCount, exceptionState);
}

unsigned long MediaStreamAudioDestinationHandler::maxChannelCount() const
{
    return kMaxChannelCount;
}

// ----------------------------------------------------------------

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(AbstractAudioContext& context, size_t numberOfChannels)
    : AudioBasicInspectorNode(context)
{
    setHandler(MediaStreamAudioDestinationHandler::create(*this, numberOfChannels));
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::create(AbstractAudioContext& context, size_t numberOfChannels, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    return new MediaStreamAudioDestinationNode(context, numberOfChannels);
}

MediaStream* MediaStreamAudioDestinationNode::stream() const
{
    return static_cast<MediaStreamAudioDestinationHandler&>(handler()).stream();
}

} // namespace blink

