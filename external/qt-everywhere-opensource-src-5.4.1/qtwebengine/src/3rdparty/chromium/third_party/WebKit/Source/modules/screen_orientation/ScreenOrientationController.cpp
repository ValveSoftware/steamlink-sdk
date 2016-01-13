// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationController.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "core/page/Page.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformScreen.h"
#include "public/platform/WebScreenOrientationClient.h"

namespace WebCore {

ScreenOrientationController::~ScreenOrientationController()
{
}

void ScreenOrientationController::persistentHostHasBeenDestroyed()
{
    // Unregister lifecycle observation once page is being torn down.
    observeContext(0);
}

void ScreenOrientationController::provideTo(LocalFrame& frame, blink::WebScreenOrientationClient* client)
{
    ScreenOrientationController* controller = new ScreenOrientationController(frame, client);
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), adoptPtrWillBeNoop(controller));
}

ScreenOrientationController& ScreenOrientationController::from(LocalFrame& frame)
{
    return *static_cast<ScreenOrientationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

ScreenOrientationController::ScreenOrientationController(LocalFrame& frame, blink::WebScreenOrientationClient* client)
    : PageLifecycleObserver(frame.page())
    , m_overrideOrientation(blink::WebScreenOrientationUndefined)
    , m_client(client)
    , m_frame(frame)
{
}

const char* ScreenOrientationController::supplementName()
{
    return "ScreenOrientationController";
}

// Compute the screen orientation using the orientation angle and the screen width / height.
blink::WebScreenOrientationType ScreenOrientationController::computeOrientation(FrameView* view)
{
    // Bypass orientation detection in layout tests to get consistent results.
    // FIXME: The screen dimension should be fixed when running the layout tests to avoid such
    // issues.
    if (isRunningLayoutTest())
        return blink::WebScreenOrientationPortraitPrimary;

    FloatRect rect = screenRect(view);
    uint16_t rotation = screenOrientationAngle(view);
    bool isTallDisplay = rotation % 180 ? rect.height() < rect.width() : rect.height() > rect.width();
    switch (rotation) {
    case 0:
        return isTallDisplay ? blink::WebScreenOrientationPortraitPrimary : blink::WebScreenOrientationLandscapePrimary;
    case 90:
        return isTallDisplay ? blink::WebScreenOrientationLandscapePrimary : blink::WebScreenOrientationPortraitSecondary;
    case 180:
        return isTallDisplay ? blink::WebScreenOrientationPortraitSecondary : blink::WebScreenOrientationLandscapeSecondary;
    case 270:
        return isTallDisplay ? blink::WebScreenOrientationLandscapeSecondary : blink::WebScreenOrientationPortraitPrimary;
    default:
        ASSERT_NOT_REACHED();
        return blink::WebScreenOrientationPortraitPrimary;
    }
}

void ScreenOrientationController::pageVisibilityChanged()
{
    if (page() && page()->visibilityState() == PageVisibilityStateVisible) {
        blink::WebScreenOrientationType oldOrientation = m_overrideOrientation;
        m_overrideOrientation = blink::WebScreenOrientationUndefined;
        // FIXME: sendOrientationChangeEvent() currently send an event all the
        // children of the frame, so it should only be called on the frame on
        // top of the tree. We would need the embedder to call
        // sendOrientationChangeEvent on every WebFrame part of a WebView to be
        // able to remove this.
        if (m_frame == m_frame.localFrameRoot() && oldOrientation != orientation())
            m_frame.sendOrientationChangeEvent();
    } else if (m_overrideOrientation == blink::WebScreenOrientationUndefined) {
        // The page is no longer visible, store the last know screen orientation
        // so that we keep returning this orientation until the page becomes
        // visible again.
        m_overrideOrientation = orientation();
    }
}

blink::WebScreenOrientationType ScreenOrientationController::orientation() const
{
    if (m_overrideOrientation != blink::WebScreenOrientationUndefined) {
        // The page is not visible, keep returning the last known screen orientation.
        ASSERT(!page() || page()->visibilityState() != PageVisibilityStateVisible);
        return m_overrideOrientation;
    }

    blink::WebScreenOrientationType orientationType = screenOrientationType(m_frame.view());
    if (orientationType == blink::WebScreenOrientationUndefined) {
        // The embedder could not provide us with an orientation, deduce it ourselves.
        orientationType = computeOrientation(m_frame.view());
    }
    ASSERT(orientationType != blink::WebScreenOrientationUndefined);
    return orientationType;
}

void ScreenOrientationController::lockOrientation(blink::WebScreenOrientationLockType orientation, blink::WebLockOrientationCallback* callback)
{
    if (!m_client) {
        return;
    }

    m_client->lockOrientation(orientation, callback);
}

void ScreenOrientationController::unlockOrientation()
{
    if (!m_client) {
        return;
    }

    m_client->unlockOrientation();
}

} // namespace WebCore
