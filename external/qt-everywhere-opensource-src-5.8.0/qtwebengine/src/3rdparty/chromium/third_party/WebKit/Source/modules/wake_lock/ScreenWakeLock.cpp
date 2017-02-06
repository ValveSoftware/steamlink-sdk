// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "core/page/PageVisibilityState.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/ServiceRegistry.h"

namespace blink {

// static
bool ScreenWakeLock::keepAwake(Screen& screen)
{
    ScreenWakeLock* screenWakeLock = fromScreen(screen);
    if (!screenWakeLock)
        return false;

    return screenWakeLock->keepAwake();
}

// static
void ScreenWakeLock::setKeepAwake(Screen& screen, bool keepAwake)
{
    ScreenWakeLock* screenWakeLock = fromScreen(screen);
    if (screenWakeLock)
        screenWakeLock->setKeepAwake(keepAwake);
}

// static
const char* ScreenWakeLock::supplementName()
{
    return "ScreenWakeLock";
}

// static
ScreenWakeLock* ScreenWakeLock::from(LocalFrame* frame)
{
    return static_cast<ScreenWakeLock*>(Supplement<LocalFrame>::from(frame, supplementName()));
}

// static
void ScreenWakeLock::provideTo(LocalFrame& frame, ServiceRegistry* registry)
{
    DCHECK(RuntimeEnabledFeatures::wakeLockEnabled());
    Supplement<LocalFrame>::provideTo(
        frame,
        ScreenWakeLock::supplementName(),
        registry ? new ScreenWakeLock(frame, registry) : nullptr);
}

void ScreenWakeLock::pageVisibilityChanged()
{
    notifyService();
}

void ScreenWakeLock::didCommitLoad(LocalFrame* committedFrame)
{
    // Reset wake lock flag for this frame if it is the one being navigated.
    if (committedFrame == frame())
        setKeepAwake(false);
}

void ScreenWakeLock::willDetachFrameHost()
{
    setKeepAwake(false);
}

DEFINE_TRACE(ScreenWakeLock)
{
    Supplement<LocalFrame>::trace(visitor);
    PageLifecycleObserver::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

ScreenWakeLock::ScreenWakeLock(LocalFrame& frame, ServiceRegistry* registry)
    : PageLifecycleObserver(frame.page())
    , LocalFrameLifecycleObserver(&frame)
    , m_keepAwake(false)
{
    DCHECK(!m_service.is_bound());
    DCHECK(registry);
    registry->connectToRemoteService(mojo::GetProxy(&m_service));
}

bool ScreenWakeLock::keepAwake() const
{
    return m_keepAwake;
}

void ScreenWakeLock::setKeepAwake(bool keepAwake)
{
    m_keepAwake = keepAwake;
    notifyService();
}

// static
ScreenWakeLock* ScreenWakeLock::fromScreen(Screen& screen)
{
    return screen.frame() ? ScreenWakeLock::from(screen.frame()) : nullptr;
}

void ScreenWakeLock::notifyService()
{
    if (!m_service)
        return;

    if (m_keepAwake && frame()->page() && frame()->page()->isPageVisible())
        m_service->RequestWakeLock();
    else
        m_service->CancelWakeLock();
}

} // namespace blink
