// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "core/page/PageVisibilityState.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/InterfaceProvider.h"

namespace blink {

// static
bool ScreenWakeLock::keepAwake(Screen& screen) {
  ScreenWakeLock* screenWakeLock = fromScreen(screen);
  if (!screenWakeLock)
    return false;

  return screenWakeLock->keepAwake();
}

// static
void ScreenWakeLock::setKeepAwake(Screen& screen, bool keepAwake) {
  ScreenWakeLock* screenWakeLock = fromScreen(screen);
  if (screenWakeLock)
    screenWakeLock->setKeepAwake(keepAwake);
}

// static
const char* ScreenWakeLock::supplementName() {
  return "ScreenWakeLock";
}

// static
ScreenWakeLock* ScreenWakeLock::from(LocalFrame* frame) {
  if (!RuntimeEnabledFeatures::wakeLockEnabled())
    return nullptr;
  ScreenWakeLock* supplement = static_cast<ScreenWakeLock*>(
      Supplement<LocalFrame>::from(frame, supplementName()));
  if (!supplement) {
    supplement = new ScreenWakeLock(*frame);
    Supplement<LocalFrame>::provideTo(*frame, supplementName(), supplement);
  }
  return supplement;
}

void ScreenWakeLock::pageVisibilityChanged() {
  notifyService();
}

void ScreenWakeLock::contextDestroyed() {
  setKeepAwake(false);
}

DEFINE_TRACE(ScreenWakeLock) {
  Supplement<LocalFrame>::trace(visitor);
  PageVisibilityObserver::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

ScreenWakeLock::ScreenWakeLock(LocalFrame& frame)
    : ContextLifecycleObserver(frame.document()),
      PageVisibilityObserver(frame.page()),
      m_keepAwake(false) {
  DCHECK(!m_service.is_bound());
  DCHECK(frame.interfaceProvider());
  frame.interfaceProvider()->getInterface(mojo::GetProxy(&m_service));
}

bool ScreenWakeLock::keepAwake() const {
  return m_keepAwake;
}

void ScreenWakeLock::setKeepAwake(bool keepAwake) {
  m_keepAwake = keepAwake;
  notifyService();
}

// static
ScreenWakeLock* ScreenWakeLock::fromScreen(Screen& screen) {
  return screen.frame() ? ScreenWakeLock::from(screen.frame()) : nullptr;
}

void ScreenWakeLock::notifyService() {
  if (!m_service)
    return;

  if (m_keepAwake && page() && page()->isPageVisible())
    m_service->RequestWakeLock();
  else
    m_service->CancelWakeLock();
}

}  // namespace blink
