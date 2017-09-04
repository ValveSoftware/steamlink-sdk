// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControlsMediaEventListener.h"

#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/MediaControls.h"

namespace blink {

MediaControlsMediaEventListener::MediaControlsMediaEventListener(
    MediaControls* mediaControls)
    : EventListener(CPPEventListenerType), m_mediaControls(mediaControls) {
  m_mediaControls->m_mediaElement->addEventListener(
      EventTypeNames::volumechange, this, false);
  m_mediaControls->m_mediaElement->addEventListener(EventTypeNames::focusin,
                                                    this, false);
}

bool MediaControlsMediaEventListener::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsMediaEventListener::handleEvent(
    ExecutionContext* executionContext,
    Event* event) {
  if (event->type() == EventTypeNames::volumechange) {
    m_mediaControls->onVolumeChange();
    return;
  }

  if (event->type() == EventTypeNames::focusin) {
    m_mediaControls->onFocusIn();
    return;
  }

  NOTREACHED();
}

DEFINE_TRACE(MediaControlsMediaEventListener) {
  EventListener::trace(visitor);
  visitor->trace(m_mediaControls);
}

}  // namespace blink
