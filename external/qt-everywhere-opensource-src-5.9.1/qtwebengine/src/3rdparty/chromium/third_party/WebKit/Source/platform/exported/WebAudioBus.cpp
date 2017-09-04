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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebAudioBus.h"

#include "wtf/build_config.h"

#include "platform/audio/AudioBus.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class WebAudioBusPrivate : public AudioBus {};

void WebAudioBus::initialize(unsigned numberOfChannels,
                             size_t length,
                             double sampleRate) {
  RefPtr<AudioBus> audioBus = AudioBus::create(numberOfChannels, length);
  audioBus->setSampleRate(sampleRate);

  if (m_private)
    (static_cast<AudioBus*>(m_private))->deref();

  audioBus->ref();
  m_private = static_cast<WebAudioBusPrivate*>(audioBus.get());
}

void WebAudioBus::resizeSmaller(size_t newLength) {
  ASSERT(m_private);
  if (m_private) {
    ASSERT(newLength <= length());
    m_private->resizeSmaller(newLength);
  }
}

void WebAudioBus::reset() {
  if (m_private) {
    (static_cast<AudioBus*>(m_private))->deref();
    m_private = 0;
  }
}

unsigned WebAudioBus::numberOfChannels() const {
  if (!m_private)
    return 0;
  return m_private->numberOfChannels();
}

size_t WebAudioBus::length() const {
  if (!m_private)
    return 0;
  return m_private->length();
}

double WebAudioBus::sampleRate() const {
  if (!m_private)
    return 0;
  return m_private->sampleRate();
}

float* WebAudioBus::channelData(unsigned channelIndex) {
  if (!m_private)
    return 0;
  ASSERT(channelIndex < numberOfChannels());
  return m_private->channel(channelIndex)->mutableData();
}

PassRefPtr<AudioBus> WebAudioBus::release() {
  RefPtr<AudioBus> audioBus(adoptRef(static_cast<AudioBus*>(m_private)));
  m_private = 0;
  return audioBus;
}

}  // namespace blink
