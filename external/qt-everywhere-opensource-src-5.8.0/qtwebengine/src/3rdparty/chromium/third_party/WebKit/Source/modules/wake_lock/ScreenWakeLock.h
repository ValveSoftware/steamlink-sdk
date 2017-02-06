// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenWakeLock_h
#define ScreenWakeLock_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/page/PageLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "public/platform/modules/wake_lock/wake_lock_service.mojom-blink.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;
class Screen;
class ServiceRegistry;

class MODULES_EXPORT ScreenWakeLock final : public GarbageCollectedFinalized<ScreenWakeLock>, public Supplement<LocalFrame>, public PageLifecycleObserver, public LocalFrameLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(ScreenWakeLock);
    WTF_MAKE_NONCOPYABLE(ScreenWakeLock);
public:
    static bool keepAwake(Screen&);
    static void setKeepAwake(Screen&, bool);

    static const char* supplementName();
    static ScreenWakeLock* from(LocalFrame*);
    static void provideTo(LocalFrame&, ServiceRegistry*);

    ~ScreenWakeLock() = default;

    DECLARE_VIRTUAL_TRACE();

private:
    ScreenWakeLock(LocalFrame&, ServiceRegistry*);

    // Inherited from PageLifecycleObserver.
    void pageVisibilityChanged() override;
    void didCommitLoad(LocalFrame*) override;

    // Inherited from LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    bool keepAwake() const;
    void setKeepAwake(bool);

    static ScreenWakeLock* fromScreen(Screen&);
    void notifyService();

    mojom::blink::WakeLockServicePtr m_service;
    bool m_keepAwake;
};

} // namespace blink

#endif // ScreenWakeLock_h
