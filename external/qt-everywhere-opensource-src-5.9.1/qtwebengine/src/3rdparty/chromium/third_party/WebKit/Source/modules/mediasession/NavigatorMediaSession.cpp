// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/NavigatorMediaSession.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "modules/mediasession/MediaSession.h"
#include "platform/Supplementable.h"

namespace blink {

NavigatorMediaSession::NavigatorMediaSession(Navigator&) {}

DEFINE_TRACE(NavigatorMediaSession) {
  visitor->trace(m_session);
  Supplement<Navigator>::trace(visitor);
}

const char* NavigatorMediaSession::supplementName() {
  return "NavigatorMediaSession";
}

NavigatorMediaSession& NavigatorMediaSession::from(Navigator& navigator) {
  NavigatorMediaSession* supplement = static_cast<NavigatorMediaSession*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorMediaSession(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

MediaSession* NavigatorMediaSession::mediaSession(ScriptState* scriptState,
                                                  Navigator& navigator) {
  NavigatorMediaSession& self = NavigatorMediaSession::from(navigator);
  if (!self.m_session)
    self.m_session = MediaSession::create(scriptState);
  return self.m_session.get();
}

}  // namespace blink
