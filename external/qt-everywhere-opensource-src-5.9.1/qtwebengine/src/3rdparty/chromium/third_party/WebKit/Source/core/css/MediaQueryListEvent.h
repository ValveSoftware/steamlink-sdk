// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaQueryListEvent_h
#define MediaQueryListEvent_h

#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryListEventInit.h"
#include "core/events/Event.h"

namespace blink {

class MediaQueryListEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaQueryListEvent* create(MediaQueryList* list) {
    return new MediaQueryListEvent(list);
  }

  static MediaQueryListEvent* create(const String& media, bool matches) {
    return new MediaQueryListEvent(media, matches);
  }

  static MediaQueryListEvent* create(
      const AtomicString& eventType,
      const MediaQueryListEventInit& initializer) {
    return new MediaQueryListEvent(eventType, initializer);
  }

  String media() const {
    return m_mediaQueryList ? m_mediaQueryList->media() : m_media;
  }
  bool matches() const {
    return m_mediaQueryList ? m_mediaQueryList->matches() : m_matches;
  }

  const AtomicString& interfaceName() const override {
    return EventNames::MediaQueryListEvent;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    Event::trace(visitor);
    visitor->trace(m_mediaQueryList);
  }

 private:
  MediaQueryListEvent(const String& media, bool matches)
      : Event(EventTypeNames::change, false, false),
        m_media(media),
        m_matches(matches) {}

  explicit MediaQueryListEvent(MediaQueryList* list)
      : Event(EventTypeNames::change, false, false),
        m_mediaQueryList(list),
        m_matches(false) {}

  MediaQueryListEvent(const AtomicString& eventType,
                      const MediaQueryListEventInit& initializer)
      : Event(eventType, initializer), m_matches(false) {
    if (initializer.hasMedia())
      m_media = initializer.media();
    if (initializer.hasMatches())
      m_matches = initializer.matches();
  }

  // We have m_media/m_matches for JS-created events; we use m_mediaQueryList
  // for events that blink generates.
  Member<MediaQueryList> m_mediaQueryList;
  String m_media;
  bool m_matches;
};

}  // namespace blink

#endif  // MediaQueryListEvent_h
