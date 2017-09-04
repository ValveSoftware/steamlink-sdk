// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControlsWindowEventListener.h"

#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/shadow/MediaControls.h"

namespace blink {

namespace {

// Helper returning the top DOMWindow as a LocalDOMWindow. Returns nullptr if it
// is not a LocalDOMWindow.
// This does not work with OOPIF.
LocalDOMWindow* getTopLocalDOMWindow(LocalDOMWindow* window) {
  if (!window->top() || !window->top()->isLocalDOMWindow())
    return nullptr;
  return static_cast<LocalDOMWindow*>(window->top());
}

}  // anonymous namespace

MediaControlsWindowEventListener* MediaControlsWindowEventListener::create(
    MediaControls* mediaControls,
    std::unique_ptr<Callback> callback) {
  return new MediaControlsWindowEventListener(mediaControls,
                                              std::move(callback));
}

MediaControlsWindowEventListener::MediaControlsWindowEventListener(
    MediaControls* mediaControls,
    std::unique_ptr<Callback> callback)
    : EventListener(CPPEventListenerType),
      m_mediaControls(mediaControls),
      m_callback(std::move(callback)),
      m_isActive(false) {
  DCHECK(m_callback);
}

bool MediaControlsWindowEventListener::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsWindowEventListener::start() {
  if (m_isActive)
    return;

  if (LocalDOMWindow* window = m_mediaControls->document().domWindow()) {
    window->addEventListener(EventTypeNames::click, this, true);

    if (LocalDOMWindow* outerWindow = getTopLocalDOMWindow(window)) {
      if (window != outerWindow)
        outerWindow->addEventListener(EventTypeNames::click, this, true);
      outerWindow->addEventListener(EventTypeNames::resize, this, true);
    }
  }

  m_mediaControls->panelElement()->addEventListener(EventTypeNames::click, this,
                                                    false);
  m_mediaControls->timelineElement()->addEventListener(EventTypeNames::click,
                                                       this, false);
  m_mediaControls->castButtonElement()->addEventListener(EventTypeNames::click,
                                                         this, false);
  m_mediaControls->volumeSliderElement()->addEventListener(
      EventTypeNames::click, this, false);

  m_isActive = true;
}

void MediaControlsWindowEventListener::stop() {
  if (!m_isActive)
    return;

  if (LocalDOMWindow* window = m_mediaControls->document().domWindow()) {
    window->removeEventListener(EventTypeNames::click, this, true);

    if (LocalDOMWindow* outerWindow = getTopLocalDOMWindow(window)) {
      if (window != outerWindow)
        outerWindow->removeEventListener(EventTypeNames::click, this, true);
      outerWindow->removeEventListener(EventTypeNames::resize, this, true);
    }

    m_isActive = false;
  }

  m_mediaControls->panelElement()->removeEventListener(EventTypeNames::click,
                                                       this, false);
  m_mediaControls->timelineElement()->removeEventListener(EventTypeNames::click,
                                                          this, false);
  m_mediaControls->castButtonElement()->removeEventListener(
      EventTypeNames::click, this, false);
  m_mediaControls->volumeSliderElement()->removeEventListener(
      EventTypeNames::click, this, false);

  m_isActive = false;
}

void MediaControlsWindowEventListener::handleEvent(
    ExecutionContext* executionContext,
    Event* event) {
  DCHECK(event->type() == EventTypeNames::click ||
         event->type() == EventTypeNames::resize);

  if (!m_isActive)
    return;
  (*m_callback.get())();
}

DEFINE_TRACE(MediaControlsWindowEventListener) {
  EventListener::trace(visitor);
  visitor->trace(m_mediaControls);
}

}  // namespace blink
