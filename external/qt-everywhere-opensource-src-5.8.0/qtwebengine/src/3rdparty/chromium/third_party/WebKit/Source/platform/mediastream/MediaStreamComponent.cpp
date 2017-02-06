/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/mediastream/MediaStreamComponent.h"

#include "platform/UUID.h"
#include "platform/audio/AudioBus.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "public/platform/WebAudioSourceProvider.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

MediaStreamComponent* MediaStreamComponent::create(MediaStreamSource* source)
{
    return new MediaStreamComponent(createCanonicalUUIDString(), source);
}

MediaStreamComponent* MediaStreamComponent::create(const String& id, MediaStreamSource* source)
{
    return new MediaStreamComponent(id, source);
}

MediaStreamComponent::MediaStreamComponent(const String& id, MediaStreamSource* source)
    : m_source(source)
    , m_id(id)
    , m_enabled(true)
    , m_muted(false)
{
    DCHECK(m_id.length());
    ThreadState::current()->registerPreFinalizer(this);
}

void MediaStreamComponent::dispose()
{
    m_trackData.reset();
}

void MediaStreamComponent::AudioSourceProviderImpl::wrap(WebAudioSourceProvider* provider)
{
    MutexLocker locker(m_provideInputLock);
    m_webAudioSourceProvider = provider;
}

void MediaStreamComponent::getSettings(WebMediaStreamTrack::Settings& settings)
{
    DCHECK(m_trackData);
    m_trackData->getSettings(settings);
}

void MediaStreamComponent::AudioSourceProviderImpl::provideInput(AudioBus* bus, size_t framesToProcess)
{
    DCHECK(bus);
    if (!bus)
        return;

    MutexTryLocker tryLocker(m_provideInputLock);
    if (!tryLocker.locked() || !m_webAudioSourceProvider) {
        bus->zero();
        return;
    }

    // Wrap the AudioBus channel data using WebVector.
    size_t n = bus->numberOfChannels();
    WebVector<float*> webAudioData(n);
    for (size_t i = 0; i < n; ++i)
        webAudioData[i] = bus->channel(i)->mutableData();

    m_webAudioSourceProvider->provideInput(webAudioData, framesToProcess);
}

DEFINE_TRACE(MediaStreamComponent)
{
    visitor->trace(m_source);
}

} // namespace blink
