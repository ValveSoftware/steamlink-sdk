// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/frame/PlatformEventController.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

class FrameView;
class ScreenOrientation;
class WebScreenOrientationClient;

class MODULES_EXPORT ScreenOrientationController final
    : public GarbageCollectedFinalized<ScreenOrientationController>
    , public Supplement<LocalFrame>
    , public LocalFrameLifecycleObserver
    , public PlatformEventController {
    USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
    WTF_MAKE_NONCOPYABLE(ScreenOrientationController);
public:
    ~ScreenOrientationController() override;

    void setOrientation(ScreenOrientation*);
    void notifyOrientationChanged();

    void lock(WebScreenOrientationLockType, WebLockOrientationCallback*);
    void unlock();

    static void provideTo(LocalFrame&, WebScreenOrientationClient*);
    static ScreenOrientationController* from(LocalFrame&);
    static const char* supplementName();

    DECLARE_VIRTUAL_TRACE();

private:
    ScreenOrientationController(LocalFrame&, WebScreenOrientationClient*);

    static WebScreenOrientationType computeOrientation(const IntRect&, uint16_t);

    // Inherited from PlatformEventController.
    void didUpdateData() override;
    void registerWithDispatcher() override;
    void unregisterWithDispatcher() override;
    bool hasLastData() override;
    void pageVisibilityChanged() override;

    // Inherited from LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    void notifyDispatcher();

    void updateOrientation();

    void dispatchEventTimerFired(Timer<ScreenOrientationController>*);

    bool isActiveAndVisible() const;

    Member<ScreenOrientation> m_orientation;
    WebScreenOrientationClient* m_client;
    Timer<ScreenOrientationController> m_dispatchEventTimer;
};

} // namespace blink

#endif // ScreenOrientationController_h
