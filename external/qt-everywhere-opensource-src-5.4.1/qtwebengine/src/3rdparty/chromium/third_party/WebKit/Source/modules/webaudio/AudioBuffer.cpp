/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "modules/webaudio/AudioBuffer.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioFileReader.h"
#include "modules/webaudio/AudioContext.h"

namespace WebCore {

float AudioBuffer::minAllowedSampleRate()
{
    // crbug.com/344375
    return 3000;
}

float AudioBuffer::maxAllowedSampleRate()
{
    // Windows can support up to this rate.
    return 192000;
}

PassRefPtrWillBeRawPtr<AudioBuffer> AudioBuffer::create(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
{
    if (sampleRate < minAllowedSampleRate() || sampleRate > maxAllowedSampleRate() || numberOfChannels > AudioContext::maxNumberOfChannels() || !numberOfChannels || !numberOfFrames)
        return nullptr;

    RefPtrWillBeRawPtr<AudioBuffer> buffer = adoptRefWillBeNoop(new AudioBuffer(numberOfChannels, numberOfFrames, sampleRate));

    if (!buffer->createdSuccessfully(numberOfChannels))
        return nullptr;
    return buffer;
}

PassRefPtrWillBeRawPtr<AudioBuffer> AudioBuffer::create(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState& exceptionState)
{
    if (!numberOfChannels || numberOfChannels > AudioContext::maxNumberOfChannels()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange(
                "number of channels",
                numberOfChannels,
                1u,
                ExceptionMessages::InclusiveBound,
                AudioContext::maxNumberOfChannels(),
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (sampleRate < AudioBuffer::minAllowedSampleRate() || sampleRate > AudioBuffer::maxAllowedSampleRate()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange(
                "sample rate",
                sampleRate,
                AudioBuffer::minAllowedSampleRate(),
                ExceptionMessages::InclusiveBound,
                AudioBuffer::maxAllowedSampleRate(),
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (!numberOfFrames) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexExceedsMinimumBound(
                "number of frames",
                numberOfFrames,
                static_cast<size_t>(0)));
        return nullptr;
    }

    RefPtrWillBeRawPtr<AudioBuffer> audioBuffer = create(numberOfChannels, numberOfFrames, sampleRate);

    if (!audioBuffer.get()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "createBuffer("
            + String::number(numberOfChannels) + ", "
            + String::number(numberOfFrames) + ", "
            + String::number(sampleRate)
            + ") failed.");
    }

    return audioBuffer;
}

PassRefPtrWillBeRawPtr<AudioBuffer> AudioBuffer::createFromAudioFileData(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    RefPtr<AudioBus> bus = createBusFromInMemoryAudioFile(data, dataSize, mixToMono, sampleRate);
    if (bus.get()) {
        RefPtrWillBeRawPtr<AudioBuffer> buffer = adoptRefWillBeNoop(new AudioBuffer(bus.get()));
        if (buffer->createdSuccessfully(bus->numberOfChannels()))
            return buffer;
    }

    return nullptr;
}

PassRefPtrWillBeRawPtr<AudioBuffer> AudioBuffer::createFromAudioBus(AudioBus* bus)
{
    if (!bus)
        return nullptr;
    RefPtrWillBeRawPtr<AudioBuffer> buffer = adoptRefWillBeNoop(new AudioBuffer(bus));
    if (buffer->createdSuccessfully(bus->numberOfChannels()))
        return buffer;
    return nullptr;
}

bool AudioBuffer::createdSuccessfully(unsigned desiredNumberOfChannels) const
{
    return numberOfChannels() == desiredNumberOfChannels;
}

AudioBuffer::AudioBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : m_sampleRate(sampleRate)
    , m_length(numberOfFrames)
{
    ScriptWrappable::init(this);
    m_channels.reserveCapacity(numberOfChannels);

    for (unsigned i = 0; i < numberOfChannels; ++i) {
        RefPtr<Float32Array> channelDataArray = Float32Array::create(m_length);
        // If the channel data array could not be created, just return. The caller will need to
        // check that the desired number of channels were created.
        if (!channelDataArray) {
            return;
        }

        channelDataArray->setNeuterable(false);
        m_channels.append(channelDataArray);
    }
}

AudioBuffer::AudioBuffer(AudioBus* bus)
    : m_sampleRate(bus->sampleRate())
    , m_length(bus->length())
{
    ScriptWrappable::init(this);
    // Copy audio data from the bus to the Float32Arrays we manage.
    unsigned numberOfChannels = bus->numberOfChannels();
    m_channels.reserveCapacity(numberOfChannels);
    for (unsigned i = 0; i < numberOfChannels; ++i) {
        RefPtr<Float32Array> channelDataArray = Float32Array::create(m_length);
        // If the channel data array could not be created, just return. The caller will need to
        // check that the desired number of channels were created.
        if (!channelDataArray)
            return;

        channelDataArray->setNeuterable(false);
        channelDataArray->setRange(bus->channel(i)->data(), m_length, 0);
        m_channels.append(channelDataArray);
    }
}

PassRefPtr<Float32Array> AudioBuffer::getChannelData(unsigned channelIndex, ExceptionState& exceptionState)
{
    if (channelIndex >= m_channels.size()) {
        exceptionState.throwDOMException(IndexSizeError, "channel index (" + String::number(channelIndex) + ") exceeds number of channels (" + String::number(m_channels.size()) + ")");
        return nullptr;
    }

    Float32Array* channelData = m_channels[channelIndex].get();
    return Float32Array::create(channelData->buffer(), channelData->byteOffset(), channelData->length());
}

Float32Array* AudioBuffer::getChannelData(unsigned channelIndex)
{
    if (channelIndex >= m_channels.size())
        return 0;

    return m_channels[channelIndex].get();
}

void AudioBuffer::zero()
{
    for (unsigned i = 0; i < m_channels.size(); ++i) {
        if (getChannelData(i))
            getChannelData(i)->zeroRange(0, length());
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
