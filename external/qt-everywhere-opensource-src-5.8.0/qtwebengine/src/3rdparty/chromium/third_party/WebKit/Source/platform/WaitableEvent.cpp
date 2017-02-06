// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/WaitableEvent.h"

#include "base/synchronization/waitable_event.h"
#include "wtf/PtrUtil.h"
#include <vector>

namespace blink {

WaitableEvent::WaitableEvent(ResetPolicy policy, InitialState state)
{
    m_impl = wrapUnique(new base::WaitableEvent(
        policy == ResetPolicy::Manual
            ? base::WaitableEvent::ResetPolicy::MANUAL
            : base::WaitableEvent::ResetPolicy::AUTOMATIC,
        state == InitialState::Signaled
            ? base::WaitableEvent::InitialState::SIGNALED
            : base::WaitableEvent::InitialState::NOT_SIGNALED));
}

WaitableEvent::~WaitableEvent() {}

void WaitableEvent::reset()
{
    m_impl->Reset();
}

void WaitableEvent::wait()
{
    m_impl->Wait();
}

void WaitableEvent::signal()
{
    m_impl->Signal();
}

size_t WaitableEvent::waitMultiple(const WTF::Vector<WaitableEvent*>& events)
{
    std::vector<base::WaitableEvent*> baseEvents;
    for (size_t i = 0; i < events.size(); ++i)
        baseEvents.push_back(events[i]->m_impl.get());
    size_t idx = base::WaitableEvent::WaitMany(baseEvents.data(), baseEvents.size());
    DCHECK_LT(idx, events.size());
    return idx;
}

} // namespace blink
