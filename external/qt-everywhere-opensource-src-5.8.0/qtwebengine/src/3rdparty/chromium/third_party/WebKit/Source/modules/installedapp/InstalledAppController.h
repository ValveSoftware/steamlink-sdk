// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstalledAppController_h
#define InstalledAppController_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "public/platform/modules/installedapp/WebInstalledAppClient.h"

namespace blink {

class WebSecurityOrigin;

class MODULES_EXPORT InstalledAppController final
    : public GarbageCollectedFinalized<InstalledAppController>, public Supplement<LocalFrame>, public LocalFrameLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(InstalledAppController);
    WTF_MAKE_NONCOPYABLE(InstalledAppController);
public:
    virtual ~InstalledAppController();

    void getInstalledApps(const WebSecurityOrigin&, std::unique_ptr<AppInstalledCallbacks>);

    static void provideTo(LocalFrame&, WebInstalledAppClient*);
    static InstalledAppController* from(LocalFrame&);
    static const char* supplementName();

    DECLARE_VIRTUAL_TRACE();

private:
    InstalledAppController(LocalFrame&, WebInstalledAppClient*);

    // Inherited from LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    WebInstalledAppClient* m_client;
};

} // namespace blink

#endif // InstalledAppController_h
