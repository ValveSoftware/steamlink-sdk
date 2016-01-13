// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPushManager_h
#define NavigatorPushManager_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class Navigator;
class PushManager;

class NavigatorPushManager FINAL : public NoBaseWillBeGarbageCollectedFinalized<NavigatorPushManager>, public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorPushManager);
public:
    virtual ~NavigatorPushManager();
    static NavigatorPushManager& from(Navigator&);

    static PushManager* push(Navigator&);
    PushManager* pushManager();

    void trace(Visitor*);

private:
    NavigatorPushManager();
    static const char* supplementName();

    PersistentWillBeMember<PushManager> m_pushManager;
};

} // namespace WebCore

#endif // NavigatorPushManager_h
