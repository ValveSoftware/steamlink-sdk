// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorMediaSession_h
#define NavigatorMediaSession_h

#include "core/frame/Navigator.h"
#include "modules/mediasession/MediaSession.h"
#include "platform/Supplementable.h"

namespace blink {

class Navigator;

// Provides MediaSession as a supplement of Navigator as an attribute.
class NavigatorMediaSession final
    : public GarbageCollected<NavigatorMediaSession>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorMediaSession);

 public:
  static NavigatorMediaSession& from(Navigator&);
  static MediaSession* mediaSession(ScriptState*, Navigator&);

  DECLARE_TRACE();

 private:
  NavigatorMediaSession(Navigator&);
  static const char* supplementName();

  // The MediaSession instance of this Navigator.
  Member<MediaSession> m_session;
};

}  // namespace blink

#endif  // NavigatorMediaSession_h
