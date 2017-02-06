// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/SyncEvent.h"

namespace blink {

SyncEvent::SyncEvent()
{
}

SyncEvent::SyncEvent(const AtomicString& type, const String& tag, bool lastChance, WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer)
    , m_tag(tag)
    , m_lastChance(lastChance)
{
}

SyncEvent::SyncEvent(const AtomicString& type, const SyncEventInit& init)
    : ExtendableEvent(type, init)
{
    m_tag = init.tag();
    m_lastChance = init.lastChance();
}

SyncEvent::~SyncEvent()
{
}

const AtomicString& SyncEvent::interfaceName() const
{
    return EventNames::SyncEvent;
}

String SyncEvent::tag()
{
    return m_tag;
}

bool SyncEvent::lastChance()
{
    return m_lastChance;
}

DEFINE_TRACE(SyncEvent)
{
    ExtendableEvent::trace(visitor);
}

} // namespace blink
