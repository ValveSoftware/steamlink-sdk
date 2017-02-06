// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrameLifecycleNotifier.h"

#include "core/frame/LocalFrameLifecycleObserver.h"

namespace blink {

void LocalFrameLifecycleNotifier::notifyWillDetachFrameHost()
{
    TemporaryChange<IterationState> scope(m_iterationState, AllowingNone);
    for (LocalFrameLifecycleObserver* observer : m_observers)
        observer->willDetachFrameHost();
}

} // namespace blink
