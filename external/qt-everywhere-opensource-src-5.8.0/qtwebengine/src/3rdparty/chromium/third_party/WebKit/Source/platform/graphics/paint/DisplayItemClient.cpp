// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#endif

namespace blink {

DisplayItemClient::CacheGenerationOrInvalidationReason::ValueType DisplayItemClient::CacheGenerationOrInvalidationReason::s_nextGeneration = kFirstValidGeneration;

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS

HashSet<const DisplayItemClient*>* liveDisplayItemClients = nullptr;
HashMap<const void*, HashMap<const DisplayItemClient*, String>>* displayItemClientsShouldKeepAlive = nullptr;

DisplayItemClient::DisplayItemClient()
{
    if (displayItemClientsShouldKeepAlive) {
        for (auto item : *displayItemClientsShouldKeepAlive)
            CHECK(!item.value.contains(this));
    }
    if (!liveDisplayItemClients)
        liveDisplayItemClients = new HashSet<const DisplayItemClient*>();
    liveDisplayItemClients->add(this);
}

DisplayItemClient::~DisplayItemClient()
{
    if (displayItemClientsShouldKeepAlive) {
        for (auto& item : *displayItemClientsShouldKeepAlive) {
            CHECK(!item.value.contains(this))
                << "Short-lived DisplayItemClient: " << item.value.get(this)
                << ". See crbug.com/609218.";
        }
    }
    liveDisplayItemClients->remove(this);
    // In case this object is a subsequence owner.
    endShouldKeepAliveAllClients(this);
}

bool DisplayItemClient::isAlive() const
{
    return liveDisplayItemClients && liveDisplayItemClients->contains(this);
}

void DisplayItemClient::beginShouldKeepAlive(const void* owner) const
{
    CHECK(isAlive());
    if (!displayItemClientsShouldKeepAlive)
        displayItemClientsShouldKeepAlive = new HashMap<const void*, HashMap<const DisplayItemClient*, String>>();
    auto addResult = displayItemClientsShouldKeepAlive->add(owner, HashMap<const DisplayItemClient*, String>()).storedValue->value.add(this, "");
    if (addResult.isNewEntry)
        addResult.storedValue->value = debugName();
}

void DisplayItemClient::endShouldKeepAlive() const
{
    if (displayItemClientsShouldKeepAlive) {
        for (auto& item : *displayItemClientsShouldKeepAlive)
            item.value.remove(this);
    }
}

void DisplayItemClient::endShouldKeepAliveAllClients(const void* owner)
{
    if (displayItemClientsShouldKeepAlive)
        displayItemClientsShouldKeepAlive->remove(owner);
}

void DisplayItemClient::endShouldKeepAliveAllClients()
{
    delete displayItemClientsShouldKeepAlive;
    displayItemClientsShouldKeepAlive = nullptr;
}

#endif // CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS

} // namespace blink
