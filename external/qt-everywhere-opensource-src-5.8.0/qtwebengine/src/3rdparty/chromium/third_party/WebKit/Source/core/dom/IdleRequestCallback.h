// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdleRequestCallback_h
#define IdleRequestCallback_h

#include "core/dom/ScriptedIdleTaskController.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"

namespace blink {

class IdleCallbackDeadline;

class IdleRequestCallback : public GarbageCollectedFinalized<IdleRequestCallback> {
public:
    DEFINE_INLINE_VIRTUAL_TRACE() {}
    virtual ~IdleRequestCallback() {}

    virtual void handleEvent(IdleDeadline*) = 0;
};

} // namespace blink

#endif // IdleRequestCallback_h
